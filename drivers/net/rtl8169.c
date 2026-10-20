/* drivers/net/rtl8169.c -- Realtek RTL8169/8110 and RTL8168/8111 gigabit,
 *                          RTL8101/8102/8103/8105/8106/8107/8402 Fast Ethernet
 *
 * The descriptor-based Realtek family that Linux drives with r8169: by far
 * the most common wired Ethernet on PC motherboards and laptops ("Realtek
 * PCIe GBE Family Controller", "Realtek PCIe FE Family Controller").
 * PCI 10ec:8161/8167/8168/8169/8136 and boards built on the 8169 by
 * D-Link, Linksys, US Robotics, Corega and others. The 2.5 and 5 gigabit
 * RTL8125/8126 have a different register layout and are not claimed.
 *
 * The chip is identified from the version bits in TxConfig (the XID, as
 * Linux reads it); the version decides the few settings that differ
 * between generations: the receive configuration word, the transmit FIFO
 * mode, how the PHY is reached (PHYAR before the 8168G, the GPHY OCP
 * window from it on), and the PLL and RXDV gate that a previous driver
 * may have left off. Linux additionally loads per-version PHY and
 * PCIe-PHY tuning tables; this driver does not, and relies on what the
 * firmware and the chip's defaults set up.
 *
 * The PHY is told what to advertise (10/100 half and full, 1000 full on
 * the gigabit parts, pause) before autonegotiation restarts: firmware and
 * other systems going to sleep with Wake-on-LAN often leave it advertising
 * 10 Mbit/s only, which the chip then keeps. Energy Efficient Ethernet is
 * not advertised (8168G on), and PCIe power saving (ASPM L0s/L1 on the
 * card's link, the chip's own ASPM and clock request) is turned off, as
 * Linux does: both are known to make these chips drop frames. The
 * negotiated speed and duplex are logged when the link comes up, and
 * IFF_RUNNING follows it. /proc/net/nic shows the link, the PHY registers,
 * the power-saving state and the chip's own counters.
 *
 * One receive and one transmit ring of 256 sixteen-byte descriptors with a
 * 2 KiB buffer each, 64-bit bus addresses: at a gigabit, 256 frames are
 * 3 ms of traffic, more than a whole TCP receive window, so a burst does
 * not overrun the ring before the network thread empties it. Received
 * frames and link changes interrupt by MSI; the handler masks the chip's
 * interrupts and wakes the network thread, whose poll unmasks them once
 * the ring is empty. Without MSI the thread polls the OWN bit of the next
 * receive descriptor from the system tick (net.h), as before.
 *
 * QEMU has no model of these chips, so this driver has only been checked
 * against the documentation and Linux's driver, not run. */
#include <kernel/net.h>
#include <kernel/klog.h>
#include <kernel/cpu.h>
#include <kernel/time.h>
#include <kernel/msi.h>
#include "nic.h"

enum {
    IDR0 = 0x00, MAR0 = 0x08, TNPDS = 0x20, CHIPCMD = 0x37, TXPOLL = 0x38, INTRMASK = 0x3C,
    INTRSTATUS = 0x3E, TXCONFIG = 0x40, RXCONFIG = 0x44, CFG9346 = 0x50, CONFIG1 = 0x52, PHYAR = 0x60,
    PHYSTATUS = 0x6C, PMCH = 0x6F, GPHY_OCP = 0xB8, RXMAXSIZE = 0xDA, CPLUSCMD = 0xE0,
    INTRMITIGATE = 0xE2, RDSAR = 0xE4, MAXTXPKT = 0xEC, MISC = 0xF0,
    COUNTERS = 0x10, CONFIG2 = 0x53, CONFIG5 = 0x56,
};
#define CFG2_CLKREQ_EN  0x80
#define CFG5_ASPM_EN    0x01
#define COUNTER_DUMP    0x08
#define CMD_RST     0x10
#define CMD_RE      0x08
#define CMD_TE      0x04
#define TXPOLL_NPQ  0x40
#define CFG_UNLOCK  0xC0
#define CFG_LOCK    0x00
#define PS_FULL     0x01                /* PHYSTATUS */
#define LINK_STATUS 0x02
#define PS_10       0x04
#define PS_100      0x08
#define PS_1000     0x10
#define PS_RXFLOW   0x20
#define PS_TXFLOW   0x40
/* IntrMask / IntrStatus */
#define I_RXOK      0x0001
#define I_RXERR     0x0002
#define I_RDU       0x0010              /* receive descriptors unavailable: the ring was full */
#define I_LINKCHG   0x0020
#define I_FIFOOVER  0x0040
#define I_SYSERR    0x8000
#define I_EVENTS    (I_RXOK | I_RXERR | I_RDU | I_LINKCHG | I_FIFOOVER | I_SYSERR)
/* MII registers */
#define MII_BMCR    0x00
#define MII_BMSR    0x01
#define MII_ANAR    0x04
#define MII_LPA     0x05
#define MII_GBCR    0x09
#define MII_GBSR    0x0a
#define BMCR_ANENABLE   0x1000
#define BMCR_ANRESTART  0x0200
#define ANAR_CSMA       0x0001
#define ANAR_10HALF     0x0020
#define ANAR_10FULL     0x0040
#define ANAR_100HALF    0x0080
#define ANAR_100FULL    0x0100
#define ANAR_PAUSE      0x0400
#define ANAR_ASYM_PAUSE 0x0800
#define GBCR_1000HALF   0x0100
#define GBCR_1000FULL   0x0200
#define RX_DMA_BURST    (7u << 8)
#define RX_FIFO_THRESH  (7u << 13)
#define RX_EARLY_OFF    (1u << 11)
#define RX_MULTI_EN     (1u << 14)
#define RX128_INT_EN    (1u << 15)
#define ACCEPT_BCAST    0x08
#define ACCEPT_MCAST    0x04
#define ACCEPT_MYPHYS   0x02
#define TX_DMA_BURST    (7u << 8)
#define TX_IFG          (3u << 24)
#define TXCFG_AUTO_FIFO (1u << 7)
#define CP_PCIMULRW     (1u << 3)
#define CP_RXCHKSUM     (1u << 5)
#define CP_RXVLAN       (1u << 6)
#define RXDV_GATED_EN   (1u << 19)

#define D_OWN   (1u << 31)
#define D_EOR   (1u << 30)
#define D_FS    (1u << 29)
#define D_LS    (1u << 28)
#define RX_RES  (1u << 21)

#define NDESC    256
#define BUF_SIZE 2048

struct desc { uint32_t opts1, opts2; uint64_t addr; } __attribute__((packed));

struct r8169 {
    struct netif nif;
    volatile uint8_t *regs;
    int ver;                            /* Linux's RTL_GIGA_MAC_VER_xx, 0 = unknown */
    bool fast_only;                     /* a 10/100 part (RTL810x) */
    volatile struct desc *rx, *tx;
    uint8_t *rxbuf, *txbuf;
    uint32_t rx_next, tx_next, tx_clean;
    int msi;                            /* the vector, or -1: polled from the tick */
    uint16_t events;                    /* IntrStatus bits the interrupt saw, for poll */
    uint8_t link;                       /* PHYSTATUS when the link last changed */
    struct pci_device *pci;
    uint16_t aspm_found;                /* ASPM control bits (card, upstream port << 8) firmware had set */
    volatile uint8_t *tally;            /* 64 bytes the chip dumps its counters into */
};

/* The chip's tally counters, as it writes them (little-endian) */
struct tally {
    uint64_t tx_packets, rx_packets, tx_errors;
    uint32_t rx_errors;
    uint16_t rx_missed, align_errors;
    uint32_t tx_one_collision, tx_multi_collision;
    uint64_t rx_unicast, rx_broadcast;
    uint32_t rx_multicast;
    uint16_t tx_aborted, tx_underrun;
} __attribute__((packed));

static uint8_t r8(struct r8169 *t, uint32_t o) { return *(volatile uint8_t *)(t->regs + o); }
static uint16_t r16(struct r8169 *t, uint32_t o) { return *(volatile uint16_t *)(t->regs + o); }
static uint32_t r32(struct r8169 *t, uint32_t o) { return *(volatile uint32_t *)(t->regs + o); }
static void w8(struct r8169 *t, uint32_t o, uint8_t v) { *(volatile uint8_t *)(t->regs + o) = v; }
static void w16(struct r8169 *t, uint32_t o, uint16_t v) { *(volatile uint16_t *)(t->regs + o) = v; }
static void w32(struct r8169 *t, uint32_t o, uint32_t v) { *(volatile uint32_t *)(t->regs + o) = v; }

static const struct { uint16_t vendor, device; } ids[] = {
    { 0x10ec, 0x8161 }, { 0x10ec, 0x8167 }, { 0x10ec, 0x8168 }, { 0x10ec, 0x8169 }, { 0x10ec, 0x8136 },
    { 0x1186, 0x4300 }, { 0x1186, 0x4302 }, { 0x1259, 0xc107 }, { 0x16ec, 0x0116 },
    { 0x1737, 0x1032 }, { 0x10ff, 0x8168 }, { 0x1734, 0x1159 },
};

/* TxConfig version bits -> Linux's mac_version, first match wins (r8169_main.c). */
static const struct { uint16_t mask, val; uint8_t ver; } xids[] = {
    { 0x7cf, 0x54b, 53 }, { 0x7cf, 0x54a, 52 },                                         /* RTL8117 */
    { 0x7cf, 0x502, 51 }, { 0x7cf, 0x501, 50 }, { 0x7cf, 0x500, 49 },                   /* 8168EP */
    { 0x7cf, 0x541, 46 }, { 0x7cf, 0x540, 45 },                                         /* 8168H */
    { 0x7cf, 0x5c8, 44 }, { 0x7cf, 0x509, 42 }, { 0x7cf, 0x4c1, 41 }, { 0x7cf, 0x4c0, 40 },   /* 8168G */
    { 0x7c8, 0x488, 38 }, { 0x7cf, 0x481, 36 }, { 0x7cf, 0x480, 35 },                   /* 8168F, 8411 */
    { 0x7c8, 0x2c8, 34 }, { 0x7cf, 0x2c1, 32 }, { 0x7c8, 0x2c0, 33 },                   /* 8168E */
    { 0x7cf, 0x281, 25 }, { 0x7c8, 0x280, 26 }, { 0x7c8, 0x288, 27 },                   /* 8168D, 8168DP */
    { 0x7cf, 0x3c9, 23 }, { 0x7cf, 0x3c8, 18 }, { 0x7c8, 0x3c8, 24 }, { 0x7cf, 0x3c0, 19 },   /* 8168C, 8168CP */
    { 0x7cf, 0x3c2, 20 }, { 0x7cf, 0x3c3, 21 }, { 0x7c8, 0x3c0, 22 },
    { 0x7c8, 0x380, 17 }, { 0x7c8, 0x300, 11 },                                         /* 8168B */
    { 0x7c8, 0x448, 39 }, { 0x7c8, 0x440, 37 }, { 0x7cf, 0x409, 29 }, { 0x7c8, 0x408, 30 },   /* 8106E, 8402, 8105E */
    { 0x7cf, 0x349,  8 }, { 0x7cf, 0x249,  8 }, { 0x7cf, 0x348,  7 }, { 0x7cf, 0x248,  7 },   /* 8102E, 8103E */
    { 0x7cf, 0x340, 13 }, { 0x7cf, 0x240, 14 }, { 0x7cf, 0x343, 10 }, { 0x7cf, 0x342, 16 },   /* 8101E */
    { 0x7c8, 0x348,  9 }, { 0x7c8, 0x248,  9 }, { 0x7c8, 0x340, 16 },
    { 0xfc8, 0x388, 13 }, { 0xfc8, 0x308, 13 },
    { 0xfc8, 0x980,  6 }, { 0xfc8, 0x180,  5 }, { 0xfc8, 0x100,  4 }, { 0xfc8, 0x040,  3 }, { 0xfc8, 0x008,  2 },   /* 8169, 8110 */
};

static const char *chip_name(int v)
{
    if (v >= 52) return "RTL8117";
    if (v >= 49) return "RTL8168EP/8111EP";
    if (v >= 45) return "RTL8168H/8111H";
    if (v >= 40) return "RTL8168G/8111G";
    if (v == 39) return "RTL8106E";
    if (v == 38) return "RTL8411";
    if (v == 37) return "RTL8402";
    if (v >= 35) return "RTL8168F/8111F";
    if (v >= 32 && v <= 34) return "RTL8168E/8111E";
    if (v == 29 || v == 30) return "RTL8105E";
    if (v >= 25 && v <= 28) return "RTL8168D/8111D";
    if (v >= 18 && v <= 24) return "RTL8168C/8111C";
    if (v == 11 || v == 17) return "RTL8168B/8111B";
    if (v >= 7 && v <= 9) return "RTL8102E/8103E";
    if (v >= 10 && v <= 16) return "RTL8101E";
    if (v >= 2 && v <= 6) return "RTL8169/8110";
    return "RTL8168 family (unknown version)";
}

/* ---- PHY: MII registers through PHYAR, or the GPHY OCP window (8168G on) ---------- */
static uint16_t phy_read(struct r8169 *t, int reg)
{
    if (t->ver >= 40) {
        uint32_t ocp = 0xa400u + 2u * (uint32_t)reg;
        w32(t, GPHY_OCP, ocp << 15);
        for (int i = 0; i < 100; i++) {                         /* the flag goes up when the data is there */
            udelay(25);
            uint32_t v = r32(t, GPHY_OCP);
            if (v & 0x80000000u) return (uint16_t)v;
        }
    } else {
        w32(t, PHYAR, (uint32_t)(reg & 0x1f) << 16);
        for (int i = 0; i < 100; i++) {
            udelay(25);
            uint32_t v = r32(t, PHYAR);
            if (v & 0x80000000u) { udelay(20); return (uint16_t)v; }
        }
    }
    return 0xffff;
}

static void phy_write(struct r8169 *t, int reg, uint16_t val)
{
    if (t->ver >= 40) {
        uint32_t ocp = 0xa400u + 2u * (uint32_t)reg;            /* standard registers, page 0x0a40 */
        w32(t, GPHY_OCP, 0x80000000u | ocp << 15 | val);
        for (int i = 0; i < 100 && (r32(t, GPHY_OCP) & 0x80000000u); i++) udelay(25);
    } else {
        w32(t, PHYAR, 0x80000000u | (uint32_t)(reg & 0x1f) << 16 | val);
        for (int i = 0; i < 100 && (r32(t, PHYAR) & 0x80000000u); i++) udelay(25);
        udelay(20);
    }
}

/* A driver that shut the chip down (Linux, or the firmware's PXE) may have
 * gated the receive path and powered the PLL and the PHY down. */
static void power_up(struct r8169 *t)
{
    int v = t->ver;
    if ((v >= 25 && v <= 28) || v == 37) w8(t, PMCH, r8(t, PMCH) | 0x80);
    if (v >= 40) {
        w8(t, PMCH, r8(t, PMCH) | 0xc0);
        w32(t, MISC, r32(t, MISC) & ~RXDV_GATED_EN);
    }
    if (v < 40) phy_write(t, 0x1f, 0);                          /* page 0 */
    /* Advertise everything the part can do; firmware and Wake-on-LAN
     * often leave 10 Mbit/s only. No 1000 half: the MACs don't do it. */
    uint16_t anar = phy_read(t, MII_ANAR);
    if (anar == 0xffff) anar = 0;
    anar = (uint16_t)((anar & ~0x0fe0u) | ANAR_CSMA | ANAR_10HALF | ANAR_10FULL | ANAR_100HALF | ANAR_100FULL |
                      ANAR_PAUSE | ANAR_ASYM_PAUSE);
    phy_write(t, MII_ANAR, anar);
    if (!t->fast_only) {
        uint16_t gbcr = phy_read(t, MII_GBCR);
        if (gbcr == 0xffff) gbcr = 0;
        phy_write(t, MII_GBCR, (uint16_t)((gbcr & ~(GBCR_1000HALF | GBCR_1000FULL)) | GBCR_1000FULL));
    }
    if (t->ver >= 40) {                                         /* no EEE: the EEE advertisement, OCP 0xa5d0 (page 0x0a5d, register 16) */
        w32(t, GPHY_OCP, 0x80000000u | 0xa5d0u << 15 | 0);
        for (int i = 0; i < 100 && (r32(t, GPHY_OCP) & 0x80000000u); i++) udelay(25);
    }
    phy_write(t, MII_BMCR, BMCR_ANENABLE | BMCR_ANRESTART);     /* and not powered down or isolated */
}

/* ---- PCIe power saving ------------------------------------------------------------ */
static uint8_t pcie_cap(struct pci_device *d)
{
    if (!(pci_read16(d->bus, d->device, d->function, 0x06) & 0x10)) return 0;   /* no capability list */
    uint8_t p = pci_read8(d->bus, d->device, d->function, 0x34) & 0xfc;
    for (int i = 0; p && i < 48; i++) {
        if (pci_read8(d->bus, d->device, d->function, p) == 0x10) return p;
        p = pci_read8(d->bus, d->device, d->function, (uint8_t)(p + 1)) & 0xfc;
    }
    return 0;
}

/* Clear ASPM control (L0s, L1) in a function's PCIe Link Control; the bits that were set */
static uint8_t aspm_off(struct pci_device *d)
{
    uint8_t c = d ? pcie_cap(d) : 0;
    if (!c) return 0;
    uint8_t off = (uint8_t)(c + 0x10);
    uint16_t lc = pci_read16(d->bus, d->device, d->function, off);
    if (lc & 3) pci_write16(d->bus, d->device, d->function, off, (uint16_t)(lc & ~3u));
    return lc & 3;
}

/* Linux turns L1 off on the link (its comment: random device stops and
 * hangs), and the chip's ASPM and clock request with it. Both ends of the
 * link are cleared: L1 is entered only when both allow it, but the port
 * may still go to L0s on its own side. */
static void power_saving_off(struct r8169 *t)
{
    t->aspm_found = (uint16_t)(aspm_off(t->pci) | aspm_off(t->pci->parent) << 8);
    if (t->ver >= 32) {
        w8(t, CFG9346, CFG_UNLOCK);
        w8(t, CONFIG2, r8(t, CONFIG2) & (uint8_t)~CFG2_CLKREQ_EN);
        w8(t, CONFIG5, r8(t, CONFIG5) & (uint8_t)~CFG5_ASPM_EN);
        w8(t, CFG9346, CFG_LOCK);
    }
}

/* 10/100 parts (Linux's RTL8101/8102/8103/8105/8106/8402 versions): no 1000BASE-T */
static bool is_fast_only(int v)
{
    return (v >= 7 && v <= 10) || (v >= 13 && v <= 16) || v == 29 || v == 30 || v == 37 || v == 39;
}

static bool reset(struct r8169 *t)
{
    w8(t, CHIPCMD, CMD_RST);
    for (int i = 0; i < 1000; i++) {
        if (!(r8(t, CHIPCMD) & CMD_RST)) return true;
        udelay(100);
    }
    return false;
}

/* ---- netif operations --------------------------------------------------------- */
static void tx_reclaim(struct r8169 *t)
{
    while (t->tx_clean != t->tx_next && !(t->tx[t->tx_clean].opts1 & D_OWN))
        t->tx_clean = (t->tx_clean + 1) % NDESC;
}

static int r8169_xmit(struct netif *nif, const void *frame, size_t len)
{
    struct r8169 *t = nif->priv;
    if (len > BUF_SIZE) return -1;
    uint32_t next = (t->tx_next + 1) % NDESC;
    if (next == t->tx_clean) tx_reclaim(t);
    if (next == t->tx_clean) return -1;                         /* ring full */
    uint32_t i = t->tx_next;
    uint8_t *b = t->txbuf + (size_t)i * BUF_SIZE;
    memcpy(b, frame, len);
    if (len < 60) { memset(b + len, 0, 60 - len); len = 60; }   /* some versions send short frames wrong */
    t->tx[i].opts2 = 0;
    nic_fence();
    t->tx[i].opts1 = D_OWN | D_FS | D_LS | (i == NDESC - 1 ? D_EOR : 0) | (uint32_t)len;
    t->tx_next = next;
    nic_fence();
    w8(t, TXPOLL, TXPOLL_NPQ);
    return 0;
}

static void link_change(struct r8169 *t)
{
    uint8_t ps = r8(t, PHYSTATUS);
    if ((ps ^ t->link) & LINK_STATUS) {
        if (ps & LINK_STATUS)
            kprintf("r8169: %s: link up, %s Mbit/s, %s duplex%s\n", t->nif.name,
                    ps & PS_1000 ? "1000" : ps & PS_100 ? "100" : ps & PS_10 ? "10" : "?",
                    ps & PS_FULL ? "full" : "half", ps & (PS_RXFLOW | PS_TXFLOW) ? ", flow control" : "");
        else kprintf("r8169: %s: link down\n", t->nif.name);
        netif_carrier(&t->nif, ps & LINK_STATUS);
    }
    t->link = ps;
}

static void r8169_poll(struct netif *nif)
{
    struct r8169 *t = nif->priv;
    uint16_t ev = __atomic_exchange_n(&t->events, 0, __ATOMIC_ACQ_REL);
    if (t->msi < 0) {                                           /* nobody acknowledges for us */
        ev = r16(t, INTRSTATUS);
        if (ev) w16(t, INTRSTATUS, ev);
    }
    if (ev & (I_RDU | I_FIFOOVER)) nif->rx_dropped++;            /* the ring or the FIFO overran: frames lost, the chip doesn't say how many */
    if (ev & I_SYSERR) kprintf("r8169: %s: PCI system error\n", nif->name);
    if ((ev & I_LINKCHG) || ((r8(t, PHYSTATUS) ^ t->link) & LINK_STATUS)) link_change(t);

    int n = 0;
    uint32_t s;
    while (!((s = t->rx[t->rx_next].opts1) & D_OWN) && n < NDESC) {
        nic_fence();
        uint32_t i = t->rx_next;
        uint32_t len = s & 0x3fff;
        if ((s & (D_FS | D_LS)) == (D_FS | D_LS) && !(s & RX_RES) && len > 4 + 14)
            net_rx_frame(nif, t->rxbuf + (size_t)i * BUF_SIZE, len - 4);    /* without the CRC */
        else nif->rx_errors++;
        t->rx[i].opts2 = 0;
        nic_fence();
        t->rx[i].opts1 = D_OWN | (i == NDESC - 1 ? D_EOR : 0) | BUF_SIZE;   /* back to the chip */
        t->rx_next = (i + 1) % NDESC;
        n++;
    }
    tx_reclaim(t);
    /* Caught up: interrupts back on. A frame that came meanwhile raises
     * one at once (its status bit is already set), and netd looks again
     * anyway while pending() says so. */
    if (t->msi >= 0 && (t->rx[t->rx_next].opts1 & D_OWN)) w16(t, INTRMASK, I_EVENTS);
}

/* MSI: hand the work to the network thread */
static void r8169_irq(void *arg)
{
    struct r8169 *t = arg;
    uint16_t st = r16(t, INTRSTATUS);
    if (!st || st == 0xffff) return;
    w16(t, INTRMASK, 0);
    w16(t, INTRSTATUS, st);
    __atomic_or_fetch(&t->events, st, __ATOMIC_ACQ_REL);
    net_kick();
}

static bool r8169_pending(struct netif *nif)
{
    struct r8169 *t = nif->priv;
    return !(t->rx[t->rx_next].opts1 & D_OWN);
}

static bool r8169_link(struct netif *nif)
{
    struct r8169 *t = nif->priv;
    return (r8(t, PHYSTATUS) & LINK_STATUS) != 0;
}

/* /proc/net/nic */
static size_t r8169_info(struct netif *nif, char *buf, size_t cap)
{
    struct r8169 *t = nif->priv;
    uint8_t ps = r8(t, PHYSTATUS);
    size_t n = 0;
#define OUT(...) do { if (n < cap) n += (size_t)snprintf(buf + n, cap - n, __VA_ARGS__); } while (0)
    OUT("  chip:      Realtek %s, version %d, %s, %d descriptors, %s\n", chip_name(t->ver), t->ver,
        t->fast_only ? "10/100" : "10/100/1000", NDESC, t->msi >= 0 ? "MSI" : "polled");
    if (ps & LINK_STATUS)
        OUT("  link:      up, %s Mbit/s, %s duplex, flow control rx %s tx %s\n",
            ps & PS_1000 ? "1000" : ps & PS_100 ? "100" : ps & PS_10 ? "10" : "?", ps & PS_FULL ? "full" : "half",
            ps & PS_RXFLOW ? "on" : "off", ps & PS_TXFLOW ? "on" : "off");
    else OUT("  link:      down\n");
    OUT("  phy:       BMCR %04x BMSR %04x ANAR %04x LPA %04x GBCR %04x GBSR %04x (PHYSTATUS %02x)\n",
        phy_read(t, MII_BMCR), phy_read(t, MII_BMSR), phy_read(t, MII_ANAR), phy_read(t, MII_LPA),
        t->fast_only ? 0 : phy_read(t, MII_GBCR), t->fast_only ? 0 : phy_read(t, MII_GBSR), ps);
    OUT("  aspm:      firmware had card %s, upstream port %s; now off. Config2 %02x Config5 %02x\n",
        (t->aspm_found & 3) == 3 ? "L0s+L1" : (t->aspm_found & 3) == 2 ? "L1" : (t->aspm_found & 3) == 1 ? "L0s" : "none",
        (t->aspm_found >> 8 & 3) == 3 ? "L0s+L1" : (t->aspm_found >> 8 & 3) == 2 ? "L1" : (t->aspm_found >> 8 & 3) == 1 ? "L0s" : "none",
        r8(t, CONFIG2), r8(t, CONFIG5));
    OUT("  rings:     rx next %u, tx next %u, tx clean %u; IntrMask %04x IntrStatus %04x\n",
        t->rx_next, t->tx_next, t->tx_clean, r16(t, INTRMASK), r16(t, INTRSTATUS));
    if (t->tally) {
        uint64_t a = nic_bus(t->tally);
        w32(t, COUNTERS + 4, (uint32_t)(a >> 32));
        w32(t, COUNTERS, (uint32_t)a);
        w32(t, COUNTERS, (uint32_t)a | COUNTER_DUMP);
        int i;
        for (i = 0; i < 1000 && (r32(t, COUNTERS) & COUNTER_DUMP); i++) udelay(10);
        if (i < 1000) {
            nic_fence();
            struct tally c;
            memcpy(&c, (const void *)t->tally, sizeof c);
            OUT("  counters:  rx %lu (unicast %lu broadcast %lu multicast %u), rx errors %u, missed %u, alignment %u\n",
                c.rx_packets, c.rx_unicast, c.rx_broadcast, c.rx_multicast, c.rx_errors, c.rx_missed, c.align_errors);
            OUT("             tx %lu, tx errors %lu, collisions %u single %u multiple, aborted %u, underrun %u\n",
                c.tx_packets, c.tx_errors, c.tx_one_collision, c.tx_multi_collision, c.tx_aborted, c.tx_underrun);
        } else OUT("  counters:  the chip did not dump them\n");
    }
#undef OUT
    return n < cap ? n : cap;
}

static bool r8169_init(struct pci_device *d)
{
    struct r8169 *t = kzalloc(sizeof *t);
    if (!t) return false;
    pci_enable(d, false, true, true);
    t->regs = nic_mmio(d, 0, NULL);                             /* BAR1 on the 8169, BAR2 on PCIe parts: the first memory BAR */
    if (!t->regs) { kprintf("r8169: %02x:%02x.%u: no register window\n", d->bus, d->device, d->function); kfree(t); return false; }

    uint32_t xid = r32(t, TXCONFIG) >> 20 & 0xfcf;
    if ((xid & 0x7cf) == 0x609 || (xid & 0x7cf) == 0x641 || (xid & 0x7cf) == 0x688 || (xid & 0x7cf) == 0x649 ||
        (xid & 0x7cf) == 0x64a) {
        kprintf("r8169: %02x:%02x.%u: RTL8125/8126 (2.5/5 GbE) is not supported\n", d->bus, d->device, d->function);
        kfree(t);
        return false;
    }
    for (size_t i = 0; i < sizeof xids / sizeof *xids; i++)
        if ((xid & xids[i].mask) == xids[i].val) { t->ver = xids[i].ver; break; }
    t->fast_only = is_fast_only(t->ver) || d->device_id == 0x8136;
    t->msi = -1;
    t->pci = d;

    w16(t, INTRMASK, 0);
    w16(t, INTRSTATUS, 0xffff);
    if (t->ver >= 40) w32(t, MISC, r32(t, MISC) | RXDV_GATED_EN);   /* quiesce the receive path before the reset */
    if (!reset(t)) { kprintf("r8169: %s does not come out of reset\n", chip_name(t->ver)); kfree(t); return false; }
    power_saving_off(t);
    power_up(t);

    struct netif *n = &t->nif;
    for (int i = 0; i < 6; i++) n->mac[i] = r8(t, IDR0 + (uint32_t)i);
    if (!(n->mac[0] | n->mac[1] | n->mac[2] | n->mac[3] | n->mac[4] | n->mac[5]) || (n->mac[0] & 1)) {
        uint64_t r = rdtsc();                                   /* no address programmed: a random local one */
        n->mac[0] = 0x02; for (int i = 1; i < 6; i++) n->mac[i] = (uint8_t)(r >> (8 * i));
        w8(t, CFG9346, CFG_UNLOCK);
        w32(t, IDR0 + 4, (uint32_t)n->mac[4] | (uint32_t)n->mac[5] << 8);
        w32(t, IDR0, (uint32_t)n->mac[0] | (uint32_t)n->mac[1] << 8 | (uint32_t)n->mac[2] << 16 | (uint32_t)n->mac[3] << 24);
        w8(t, CFG9346, CFG_LOCK);
    }

    /* Linux lets only mac_version 18 and later use 64-bit addresses: the
     * older 8169/8110 parts get memory below 4 GiB */
    bool dma32 = t->ver < 18;
    t->rx = nic_dma(NDESC * sizeof(struct desc), dma32);
    t->tx = nic_dma(NDESC * sizeof(struct desc), dma32);
    t->rxbuf = nic_dma((size_t)NDESC * BUF_SIZE, dma32);
    t->txbuf = nic_dma((size_t)NDESC * BUF_SIZE, dma32);
    if (!t->rx || !t->tx || !t->rxbuf || !t->txbuf) { kprintf("r8169: out of memory\n"); return false; }
    t->tally = nic_dma(sizeof(struct tally), dma32);
    for (uint32_t i = 0; i < NDESC; i++) {
        t->rx[i].addr = nic_bus(t->rxbuf + (size_t)i * BUF_SIZE);
        t->rx[i].opts1 = D_OWN | (i == NDESC - 1 ? D_EOR : 0) | BUF_SIZE;
        t->tx[i].addr = nic_bus(t->txbuf + (size_t)i * BUF_SIZE);
        t->tx[i].opts1 = i == NDESC - 1 ? D_EOR : 0;
    }

    /* The order of rtl_hw_start() in Linux: configuration registers unlocked,
     * C+ mode, ring addresses, then transmitter and receiver on, then their
     * configuration words. */
    w8(t, CFG9346, CFG_UNLOCK);
    uint16_t cp = r16(t, CPLUSCMD) & ~(CP_RXVLAN | CP_RXCHKSUM);
    if (t->ver && t->ver <= 6) cp |= CP_PCIMULRW;
    w16(t, CPLUSCMD, cp);
    w8(t, MAXTXPKT, 0x3f);                                      /* 8169: no early transmit; 8168: largest packet */
    w16(t, INTRMITIGATE, 0);
    w16(t, RXMAXSIZE, BUF_SIZE);
    w32(t, TNPDS + 4, (uint32_t)(nic_bus(t->tx) >> 32));
    w32(t, TNPDS, (uint32_t)nic_bus(t->tx));
    w32(t, RDSAR + 4, (uint32_t)(nic_bus(t->rx) >> 32));
    w32(t, RDSAR, (uint32_t)nic_bus(t->rx));
    w8(t, CFG9346, CFG_LOCK);
    (void)r8(t, CHIPCMD);                                       /* flush the posted writes */
    w8(t, CHIPCMD, CMD_TE | CMD_RE);

    int v = t->ver;
    uint32_t rxcfg;
    if ((v >= 2 && v <= 6) || (v >= 10 && v <= 17)) rxcfg = RX_FIFO_THRESH | RX_DMA_BURST;
    else if ((v >= 18 && v <= 24) || (v >= 34 && v <= 36) || v == 38) rxcfg = RX128_INT_EN | RX_MULTI_EN | RX_DMA_BURST;
    else if (v >= 40) rxcfg = RX128_INT_EN | RX_MULTI_EN | RX_DMA_BURST | RX_EARLY_OFF;
    else rxcfg = RX128_INT_EN | RX_DMA_BURST;
    w32(t, RXCONFIG, rxcfg);
    w32(t, TXCONFIG, TX_DMA_BURST | TX_IFG | (v >= 34 && v != 39 ? TXCFG_AUTO_FIFO : 0));
    w32(t, MAR0 + 4, 0xffffffff);                               /* every multicast group; the stack filters */
    w32(t, MAR0, 0xffffffff);
    w32(t, RXCONFIG, rxcfg | ACCEPT_BCAST | ACCEPT_MCAST | ACCEPT_MYPHYS);

    n->driver = "r8169";
    n->mtu = ETH_MTU;
    n->xmit = r8169_xmit;
    n->poll = r8169_poll;
    n->pending = r8169_pending;
    n->link_up = r8169_link;
    n->info = r8169_info;
    n->priv = t;
    t->link = r8(t, PHYSTATUS) & ~LINK_STATUS;                  /* the first poll reports the link */
    netif_register(n);
    if (!v || v > 6) t->msi = msi_enable(d, r8169_irq, t, "r8169");   /* Linux keeps the PCI 8169s on INTx: their MSI is unreliable */
    w16(t, INTRSTATUS, 0xffff);
    if (t->msi >= 0) w16(t, INTRMASK, I_EVENTS);
    kprintf("r8169: %s: Realtek %s (XID %03x, version %d) at %02x:%02x.%u, %s, %d descriptors, %s\n", n->name, chip_name(v),
            xid, v, d->bus, d->device, d->function, t->fast_only ? "10/100" : "10/100/1000", NDESC,
            t->msi >= 0 ? "MSI" : "polled");
    if (t->aspm_found) kprintf("r8169: %s: PCIe ASPM was on (card %x, upstream port %x), turned off\n", n->name,
                               t->aspm_found & 3, t->aspm_found >> 8);
    return true;
}

void rtl8169_probe(void)
{
    for (size_t i = 0; i < pci_device_count(); i++) {
        struct pci_device *d = pci_device_at(i);
        for (size_t k = 0; k < sizeof ids / sizeof *ids; k++)
            if (d->vendor_id == ids[k].vendor && d->device_id == ids[k].device) { r8169_init(d); break; }
    }
}
