/* net/packet.c -- packet sockets (AF_PACKET) and classic BPF filters,
 * written for Kestrel from packet(7) and the BPF instruction set
 *
 * Every frame a card receives, and every frame the stack sends, is shown
 * to the packet sockets (packet_tap) before anything else looks at it. A
 * socket takes the frames of its protocol (or all of them, ETH_P_ALL) on
 * its interface (or any), runs its filter over them and queues what the
 * filter keeps: the whole frame for SOCK_RAW, the payload for SOCK_DGRAM,
 * with a struct sockaddr_ll naming the interface, the protocol, the
 * sender's MAC and whether the frame was for us, broadcast, multicast,
 * someone else's or our own going out. Sending puts the frame straight on
 * the card. Loopback has no frames and is not tapped. */
#include "msock.h"
#include <kernel/socket.h>
#include <kernel/task.h>
#include <kernel/string.h>
#include <kernel/posix.h>
#include <kernel/mm.h>

int msocks_packet;

#define PACKET_HOST      0
#define PACKET_BROADCAST 1
#define PACKET_MULTICAST 2
#define PACKET_OTHERHOST 3
#define PACKET_OUTGOING  4

/* ---- classic BPF ------------------------------------------------------------------ */
#define BPF_CLASS(c)    ((c) & 0x07)
#define BPF_LD          0x00
#define BPF_LDX         0x01
#define BPF_ST          0x02
#define BPF_STX         0x03
#define BPF_ALU         0x04
#define BPF_JMP         0x05
#define BPF_RET         0x06
#define BPF_MISC        0x07
#define BPF_SIZE(c)     ((c) & 0x18)
#define BPF_W           0x00
#define BPF_H           0x08
#define BPF_B           0x10
#define BPF_MODE(c)     ((c) & 0xe0)
#define BPF_IMM         0x00
#define BPF_ABS         0x20
#define BPF_IND         0x40
#define BPF_MEM         0x60
#define BPF_LEN         0x80
#define BPF_MSH         0xa0
#define BPF_OP(c)       ((c) & 0xf0)
#define BPF_SRC(c)      ((c) & 0x08)
#define BPF_X           0x08
#define BPF_RVAL(c)     ((c) & 0x18)
#define BPF_A           0x10
#define BPF_MEMWORDS    16
#define SKF_AD_OFF      (-0x1000)

struct bpf_ctx { uint16_t proto; uint8_t pkttype; int ifindex; };      /* proto: host order */

/* Linux's ancillary loads (SKF_AD_*) that make sense here */
static bool bpf_ancillary(int32_t k, const struct bpf_ctx *c, uint32_t len, uint32_t *a)
{
    switch (k - SKF_AD_OFF) {
    case 0:  *a = c->proto; return true;                /* SKF_AD_PROTOCOL, host order */
    case 4:  *a = c->pkttype; return true;              /* SKF_AD_PKTTYPE */
    case 8:  *a = (uint32_t)c->ifindex; return true;    /* SKF_AD_IFINDEX */
    case 40: *a = len; return true;                     /* SKF_AD_PAY_OFFSET: no headers parsed */
    case 44: case 48: case 52: *a = 0; return true;     /* VLAN_TAG, _PRESENT, CPU... */
    case 56: *a = 0; return true;                       /* SKF_AD_RANDOM: deterministic is fine */
    }
    return false;
}

static bool bpf_load(const uint8_t *p, uint32_t len, uint32_t off, int size, uint32_t *out)
{
    uint32_t n = size == BPF_W ? 4 : size == BPF_H ? 2 : 1;
    if (off > len || len - off < n) return false;
    const uint8_t *q = p + off;
    *out = n == 4 ? (uint32_t)q[0] << 24 | (uint32_t)q[1] << 16 | (uint32_t)q[2] << 8 | q[3]
         : n == 2 ? (uint32_t)q[0] << 8 | q[1] : q[0];
    return true;
}

/* The filter's verdict: how many bytes to keep (0: drop the packet) */
static uint32_t bpf_run(const struct bpf_insn *f, uint32_t n, const uint8_t *p, uint32_t len, const struct bpf_ctx *c)
{
    uint32_t A = 0, X = 0, M[BPF_MEMWORDS] = { 0 };
    for (uint32_t pc = 0; pc < n; pc++) {
        const struct bpf_insn *i = &f[pc];
        uint32_t k = i->k, v;
        switch (BPF_CLASS(i->code)) {
        case BPF_LD:
            switch (BPF_MODE(i->code)) {
            case BPF_IMM: A = k; break;
            case BPF_LEN: A = len; break;
            case BPF_MEM: A = M[k % BPF_MEMWORDS]; break;
            case BPF_ABS:
                if ((int32_t)k < 0) { if (!bpf_ancillary((int32_t)k, c, len, &A)) return 0; break; }
                if (!bpf_load(p, len, k, BPF_SIZE(i->code), &A)) return 0;
                break;
            case BPF_IND:
                if (!bpf_load(p, len, X + k, BPF_SIZE(i->code), &A)) return 0;
                break;
            default: return 0;
            }
            break;
        case BPF_LDX:
            switch (BPF_MODE(i->code)) {
            case BPF_IMM: X = k; break;
            case BPF_LEN: X = len; break;
            case BPF_MEM: X = M[k % BPF_MEMWORDS]; break;
            case BPF_MSH: if (!bpf_load(p, len, k, BPF_B, &v)) return 0; X = (v & 0xf) * 4; break;
            default: return 0;
            }
            break;
        case BPF_ST:  M[k % BPF_MEMWORDS] = A; break;
        case BPF_STX: M[k % BPF_MEMWORDS] = X; break;
        case BPF_ALU:
            v = BPF_SRC(i->code) == BPF_X ? X : k;
            switch (BPF_OP(i->code)) {
            case 0x00: A += v; break;
            case 0x10: A -= v; break;
            case 0x20: A *= v; break;
            case 0x30: if (!v) return 0; A /= v; break;
            case 0x40: A |= v; break;
            case 0x50: A &= v; break;
            case 0x60: A = v >= 32 ? 0 : A << v; break;
            case 0x70: A = v >= 32 ? 0 : A >> v; break;
            case 0x80: A = (uint32_t)-(int32_t)A; break;
            case 0x90: if (!v) return 0; A %= v; break;
            case 0xa0: A ^= v; break;
            default: return 0;
            }
            break;
        case BPF_JMP:
            v = BPF_SRC(i->code) == BPF_X ? X : k;
            switch (BPF_OP(i->code)) {
            case 0x00: pc += k; break;                  /* JA */
            case 0x10: pc += A == v ? i->jt : i->jf; break;
            case 0x20: pc += A > v ? i->jt : i->jf; break;
            case 0x30: pc += A >= v ? i->jt : i->jf; break;
            case 0x40: pc += (A & v) ? i->jt : i->jf; break;
            default: return 0;
            }
            break;
        case BPF_RET:
            return BPF_RVAL(i->code) == BPF_A ? A : k;
        case BPF_MISC:
            if (i->code & 0x80) A = X; else X = A;      /* TXA, TAX */
            break;
        }
    }
    return 0;                                           /* fell off the end */
}

/* What the kernel accepts: every jump lands inside, the last instruction returns */
static bool bpf_valid(const struct bpf_insn *f, uint32_t n)
{
    if (!n || n > BPF_MAXINSNS) return false;
    for (uint32_t pc = 0; pc < n; pc++) {
        const struct bpf_insn *i = &f[pc];
        if (BPF_CLASS(i->code) == BPF_JMP) {
            if (BPF_OP(i->code) == 0x00) { if (i->k >= n - pc - 1) return false; }
            else if (BPF_OP(i->code) > 0x40 || i->jt >= n - pc - 1 || i->jf >= n - pc - 1) return false;
        }
        if ((BPF_CLASS(i->code) == BPF_ST || BPF_CLASS(i->code) == BPF_STX ||
             ((BPF_CLASS(i->code) == BPF_LD || BPF_CLASS(i->code) == BPF_LDX) && BPF_MODE(i->code) == BPF_MEM)) &&
            i->k >= BPF_MEMWORDS) return false;
        if (BPF_CLASS(i->code) == BPF_ALU && BPF_SRC(i->code) != BPF_X && i->k == 0 &&
            (BPF_OP(i->code) == 0x30 || BPF_OP(i->code) == 0x90)) return false;
    }
    return BPF_CLASS(f[n - 1].code) == BPF_RET;
}

int packet_attach_filter(struct msock *s, const struct kfprog *p)
{
    if (!bpf_valid(p->insns, p->len)) return -EINVAL;
    struct bpf_insn *f = kmalloc(p->len * sizeof *f);
    if (!f) return -ENOMEM;
    memcpy(f, p->insns, p->len * sizeof *f);
    kfree(s->filter);
    s->filter = f;
    s->nfilter = p->len;
    return 0;
}

/* ---- receiving ----------------------------------------------------------------------- */
void packet_tap(struct netif *nif, const uint8_t *frame, size_t len, bool outgoing)
{
    if (nif == netif_loopback() || len < ETH_HLEN) return;
    uint16_t type = (uint16_t)(frame[12] | frame[13] << 8);            /* network order */
    static const uint8_t bcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    uint8_t pkttype = outgoing ? PACKET_OUTGOING : !memcmp(frame, nif->mac, ETH_ALEN) ? PACKET_HOST
                    : !memcmp(frame, bcast, ETH_ALEN) ? PACKET_BROADCAST : (frame[0] & 1) ? PACKET_MULTICAST
                    : PACKET_OTHERHOST;
    for (struct msock *s = msocks; s; s = s->next) {
        if (s->domain != AF_PACKET || !s->pproto) continue;
        if (s->ifindex && s->ifindex != nif->index) continue;
        if (s->pproto != be16(ETH_P_ALL) && s->pproto != type) continue;
        if (outgoing && s->pproto != be16(ETH_P_ALL)) continue;    /* Linux shows our own frames to ETH_P_ALL only */
        const uint8_t *data = frame;
        uint32_t dlen = (uint32_t)len;
        if (s->type == SOCK_DGRAM) { data += ETH_HLEN; dlen -= ETH_HLEN; }
        if (s->filter) {
            struct bpf_ctx c = { be16(type), pkttype, nif->index };
            uint32_t keep = bpf_run(s->filter, s->nfilter, data, dlen, &c);
            if (!keep) continue;
            if (keep < dlen) dlen = keep;
        }
        struct sockaddr_ll a;
        memset(&a, 0, sizeof a);
        a.sll_family = AF_PACKET;
        a.sll_protocol = type;
        a.sll_ifindex = nif->index;
        a.sll_hatype = 1;                               /* ARPHRD_ETHER */
        a.sll_pkttype = pkttype;
        a.sll_halen = ETH_ALEN;
        memcpy(a.sll_addr, frame + 6, ETH_ALEN);        /* the sender */
        msock_queue(s, data, dlen, &a, sizeof a);
    }
}

/* ---- the socket calls --------------------------------------------------------------- */
int packet_bind(struct msock *s, const struct sockaddr_ll *a, uint32_t len)
{
    (void)len;
    if (a->sll_family != AF_PACKET) return -EINVAL;
    if (a->sll_ifindex && !netif_by_index(a->sll_ifindex)) return -ENODEV;
    if (a->sll_ifindex == netif_loopback()->index) return -EOPNOTSUPP;     /* loopback has no frames */
    s->ifindex = a->sll_ifindex;
    if (a->sll_protocol) s->pproto = a->sll_protocol;
    return 0;
}

long packet_send(struct msock *s, const uint8_t *buf, size_t len, const struct sockaddr_ll *to, uint32_t tolen)
{
    int ifindex = s->ifindex;
    if (to) {
        if (tolen < 12 || to->sll_family != AF_PACKET) return -EINVAL;
        if (to->sll_ifindex) ifindex = to->sll_ifindex;
    }
    struct netif *n = ifindex ? netif_by_index(ifindex) : NULL;
    if (!n) return ifindex ? -ENXIO : -EDESTADDRREQ;
    if (n == netif_loopback()) return -EOPNOTSUPP;
    if (!(n->flags & IFF_UP)) return -ENETDOWN;
    uint8_t frame[ETH_FRAME_MAX];
    size_t flen;
    if (s->type == SOCK_RAW) {
        if (len < ETH_HLEN) return -EINVAL;
        if (len > ETH_FRAME_MAX) return -EMSGSIZE;
        memcpy(frame, buf, len);
        flen = len;
    } else {
        if (!to || to->sll_halen < ETH_ALEN) return -EINVAL;   /* SOCK_DGRAM: the destination is in the address */
        if (len > n->mtu) return -EMSGSIZE;
        uint16_t proto = to->sll_protocol ? to->sll_protocol : s->pproto;
        memcpy(frame, to->sll_addr, ETH_ALEN);
        memcpy(frame + 6, n->mac, ETH_ALEN);
        memcpy(frame + 12, &proto, 2);
        memcpy(frame + ETH_HLEN, buf, len);
        flen = ETH_HLEN + len;
    }
    if (flen < 60) { memset(frame + flen, 0, 60 - flen); flen = 60; }
    return netif_xmit(n, frame, flen) < 0 ? -ENOBUFS : (long)len;
}
