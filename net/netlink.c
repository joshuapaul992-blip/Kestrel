/* net/netlink.c -- NETLINK_ROUTE (rtnetlink), written for Kestrel from the
 * documented message formats (rtnetlink(7), netlink(7))
 *
 * Requests are answered as the send call makes them, by queueing the
 * replies on the sender: a dump is a series of NLM_F_MULTI messages in
 * datagrams of up to NLMSG_GOODSIZE bytes, ended by NLMSG_DONE; anything
 * else gets its reply, then an NLMSG_ERROR (0 for an acknowledgement)
 * when it failed or NLM_F_ACK asked for one.
 *
 * What there is:
 *   RTM_GETLINK, RTM_NEWLINK/SETLINK on an existing link (up and down,
 *     MTU, name)
 *   RTM_GETADDR, RTM_NEWADDR, RTM_DELADDR: one IPv4 address per link
 *   RTM_GETROUTE (a dump of the main table, or one lookup), RTM_NEWROUTE,
 *     RTM_DELROUTE: unicast routes
 *   RTM_GETNEIGH, RTM_DELNEIGH: the ARP cache
 *   RTM_GETRULE: the three standard rules (local, main, default)
 * Other dumps (IPv6, qdiscs, ...) are empty; other changes EOPNOTSUPP.
 * Changes from anywhere (DHCP, ioctls, netlink) are announced to the
 * groups RTNLGRP_LINK, _IPV4_IFADDR and _IPV4_ROUTE. A change of address
 * made from user space stops the kernel's DHCP client on that link. */
#include "msock.h"
#include <kernel/socket.h>
#include <kernel/task.h>
#include <kernel/string.h>
#include <kernel/posix.h>
#include <kernel/mm.h>
#include <kernel/time.h>

#define NLMSG_GOODSIZE  3776
#define NLMSG_HDRLEN    16
#define NLA_ALIGN(n)    (((n) + 3u) & ~3u)

#define NLMSG_NOOP      1
#define NLMSG_ERROR     2
#define NLMSG_DONE      3
#define NLMSG_MIN_TYPE  0x10

#define NLM_F_REQUEST   0x01
#define NLM_F_MULTI     0x02
#define NLM_F_ACK       0x04
#define NLM_F_ROOT      0x100
#define NLM_F_MATCH     0x200
#define NLM_F_DUMP      (NLM_F_ROOT | NLM_F_MATCH)
#define NLM_F_REPLACE   0x100
#define NLM_F_EXCL      0x200
#define NLM_F_CREATE    0x400
#define NLM_F_CAPPED    0x100

enum {
    RTM_NEWLINK = 16, RTM_DELLINK, RTM_GETLINK, RTM_SETLINK,
    RTM_NEWADDR = 20, RTM_DELADDR, RTM_GETADDR,
    RTM_NEWROUTE = 24, RTM_DELROUTE, RTM_GETROUTE,
    RTM_NEWNEIGH = 28, RTM_DELNEIGH, RTM_GETNEIGH,
    RTM_NEWRULE = 32, RTM_DELRULE, RTM_GETRULE,
};

enum { IFLA_ADDRESS = 1, IFLA_BROADCAST, IFLA_IFNAME, IFLA_MTU, IFLA_LINK, IFLA_QDISC, IFLA_STATS,
       IFLA_TXQLEN = 13, IFLA_OPERSTATE = 16, IFLA_LINKMODE = 17, IFLA_STATS64 = 23, IFLA_GROUP = 27,
       IFLA_PROMISCUITY = 30, IFLA_NUM_TX_QUEUES = 31, IFLA_NUM_RX_QUEUES = 32, IFLA_CARRIER = 33,
       IFLA_CARRIER_CHANGES = 35, IFLA_MIN_MTU = 50, IFLA_MAX_MTU = 51 };
enum { IFA_ADDRESS = 1, IFA_LOCAL, IFA_LABEL, IFA_BROADCAST, IFA_CACHEINFO = 6, IFA_FLAGS = 8 };
enum { RTA_DST = 1, RTA_SRC, RTA_IIF, RTA_OIF, RTA_GATEWAY, RTA_PRIORITY, RTA_PREFSRC, RTA_TABLE = 15 };
enum { NDA_DST = 1, NDA_LLADDR };
enum { FRA_PRIORITY = 6, FRA_TABLE = 15 };

#define IFF_LOWER_UP        0x10000
#define IFA_F_PERMANENT     0x80
#define RT_SCOPE_UNIVERSE   0
#define RT_SCOPE_LINK       253
#define RT_SCOPE_HOST       254
#define RT_TABLE_DEFAULT    253
#define RT_TABLE_MAIN       254
#define RT_TABLE_LOCAL      255
#define RTPROT_KERNEL       2
#define RTPROT_BOOT         3
#define RTN_UNICAST         1
#define RTN_LOCAL           2
#define RTM_F_CLONED        0x200
#define NUD_INCOMPLETE      0x01
#define NUD_REACHABLE       0x02
#define ARPHRD_ETHER        1
#define ARPHRD_LOOPBACK     772

#define GRP_LINK        1                   /* RTNLGRP_LINK */
#define GRP_IPV4_IFADDR 5
#define GRP_IPV4_ROUTE  7

struct nlmsghdr { uint32_t len; uint16_t type, flags; uint32_t seq, pid; };
struct rtattr { uint16_t len, type; };
struct ifinfomsg { uint8_t family, pad; uint16_t type; int32_t index; uint32_t flags, change; };
struct ifaddrmsg { uint8_t family, prefixlen, flags, scope; uint32_t index; };
struct rtmsg { uint8_t family, dst_len, src_len, tos, table, protocol, scope, type; uint32_t flags; };
struct ndmsg { uint8_t family, pad1; uint16_t pad2; int32_t ifindex; uint16_t state; uint8_t flags, type; };
struct fib_rule_hdr { uint8_t family, dst_len, src_len, tos, table, res1, res2, action; uint32_t flags; };
struct ifa_cacheinfo { uint32_t prefered, valid, cstamp, tstamp; };

/* ---- building messages ---------------------------------------------------------- */
struct nlm { uint8_t d[1024]; uint32_t len; };      /* one message */

static void *m_start(struct nlm *m, uint16_t type, uint16_t flags, uint32_t seq, uint32_t pid, size_t fixed)
{
    memset(m->d, 0, NLMSG_HDRLEN + fixed);
    struct nlmsghdr *h = (struct nlmsghdr *)m->d;
    h->type = type; h->flags = flags; h->seq = seq; h->pid = pid;
    m->len = NLMSG_HDRLEN + (uint32_t)NLA_ALIGN(fixed);
    return m->d + NLMSG_HDRLEN;
}

static void m_attr(struct nlm *m, uint16_t type, const void *data, size_t len)
{
    if (m->len + 4 + NLA_ALIGN(len) > sizeof m->d) return;
    struct rtattr *a = (struct rtattr *)(m->d + m->len);
    a->len = (uint16_t)(4 + len);
    a->type = type;
    memcpy(a + 1, data, len);
    memset((uint8_t *)(a + 1) + len, 0, NLA_ALIGN(len) - len);
    m->len += 4 + NLA_ALIGN(len);
}
static void m_u32(struct nlm *m, uint16_t type, uint32_t v) { m_attr(m, type, &v, 4); }
static void m_u8(struct nlm *m, uint16_t type, uint8_t v) { m_attr(m, type, &v, 1); }
static void m_str(struct nlm *m, uint16_t type, const char *s) { m_attr(m, type, s, strlen(s) + 1); }
static void m_end(struct nlm *m) { ((struct nlmsghdr *)m->d)->len = m->len; }

/* Replies to one socket: messages gathered into datagrams */
struct nlout {
    struct msock *to;
    uint8_t buf[NLMSG_GOODSIZE];
    uint32_t len;
    bool lost;
};

static const struct sockaddr_nl kernel_addr = { AF_NETLINK, 0, 0, 0 };

static void out_flush(struct nlout *o)
{
    if (!o->len) return;
    if (msock_queue(o->to, o->buf, o->len, &kernel_addr, sizeof kernel_addr) < 0) o->lost = true;
    o->len = 0;
}

static void out_put(struct nlout *o, struct nlm *m)
{
    m_end(m);
    if (o->len + m->len > sizeof o->buf) out_flush(o);
    memcpy(o->buf + o->len, m->d, m->len);
    o->len += m->len;
}

/* ---- parsing requests ------------------------------------------------------------ */
struct attrs { const struct rtattr *a[64]; };

static void parse_attrs(struct attrs *at, const uint8_t *p, size_t len)
{
    memset(at, 0, sizeof *at);
    while (len >= 4) {
        const struct rtattr *a = (const struct rtattr *)p;
        if (a->len < 4 || a->len > len) break;
        uint16_t t = a->type & 0x3fff;                  /* without NLA_F_NESTED / _NET_BYTEORDER */
        if (t < 64) at->a[t] = a;
        size_t step = NLA_ALIGN(a->len);
        if (step >= len) break;
        p += step; len -= step;
    }
}
static size_t a_len(const struct rtattr *a) { return a ? a->len - 4u : 0; }
static const void *a_data(const struct rtattr *a) { return a + 1; }
static bool a_u32(const struct rtattr *a, uint32_t *v)
{
    if (!a || a_len(a) < 4) return false;
    memcpy(v, a_data(a), 4);
    return true;
}

/* ---- links ---------------------------------------------------------------------- */
static bool is_lo(struct netif *n) { return n == netif_loopback(); }

static uint32_t link_flags(struct netif *n)
{
    uint32_t f = n->flags;
    if (is_lo(n)) f |= IFF_LOOPBACK;
    else f |= IFF_BROADCAST | IFF_MULTICAST;
    if ((f & IFF_UP) && (f & IFF_RUNNING)) f |= IFF_LOWER_UP;
    return f;
}

static void fill_link(struct nlm *m, struct netif *n, uint16_t type, uint16_t flags, uint32_t seq, uint32_t pid)
{
    struct ifinfomsg *i = m_start(m, type, flags, seq, pid, sizeof *i);
    i->family = AF_UNSPEC;
    i->type = is_lo(n) ? ARPHRD_LOOPBACK : ARPHRD_ETHER;
    i->index = n->index;
    i->flags = link_flags(n);
    m_str(m, IFLA_IFNAME, n->name);
    m_u32(m, IFLA_TXQLEN, 1000);
    bool up = (n->flags & IFF_UP) && (n->flags & IFF_RUNNING);
    m_u8(m, IFLA_OPERSTATE, is_lo(n) ? 0 : up ? 6 : 2);        /* UNKNOWN, UP, DOWN */
    m_u8(m, IFLA_LINKMODE, 0);
    m_u32(m, IFLA_MTU, n->mtu);
    m_u32(m, IFLA_MIN_MTU, is_lo(n) ? 0 : 68);
    m_u32(m, IFLA_MAX_MTU, is_lo(n) ? 65535 : ETH_MTU);
    m_u32(m, IFLA_GROUP, 0);
    m_u32(m, IFLA_PROMISCUITY, 0);
    m_u32(m, IFLA_NUM_TX_QUEUES, 1);
    m_u32(m, IFLA_NUM_RX_QUEUES, 1);
    m_u8(m, IFLA_CARRIER, up || is_lo(n));
    m_str(m, IFLA_QDISC, is_lo(n) ? "noqueue" : "pfifo_fast");
    m_u32(m, IFLA_CARRIER_CHANGES, 0);
    static const uint8_t bcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, zero[ETH_ALEN];
    m_attr(m, IFLA_ADDRESS, is_lo(n) ? zero : n->mac, ETH_ALEN);
    m_attr(m, IFLA_BROADCAST, is_lo(n) ? zero : bcast, ETH_ALEN);
    /* rtnl_link_stats64, then the 32-bit rtnl_link_stats: the first eight
     * counters are rx/tx packets, bytes, errors and drops */
    uint64_t s64[24];
    memset(s64, 0, sizeof s64);
    s64[0] = n->rx_packets; s64[1] = n->tx_packets; s64[2] = n->rx_bytes; s64[3] = n->tx_bytes;
    s64[4] = n->rx_errors; s64[5] = n->tx_errors; s64[6] = n->rx_dropped; s64[7] = n->tx_dropped;
    m_attr(m, IFLA_STATS64, s64, sizeof s64);
    uint32_t s32[24];
    for (int k = 0; k < 24; k++) s32[k] = (uint32_t)s64[k];
    m_attr(m, IFLA_STATS, s32, sizeof s32);
}

static struct netif *link_of(const struct ifinfomsg *i, const struct attrs *at)
{
    if (i->index > 0) return netif_by_index(i->index);
    const struct rtattr *a = at->a[IFLA_IFNAME];
    if (!a || a_len(a) < 1) return NULL;
    char name[IFNAMSIZ];
    size_t n = a_len(a) < sizeof name - 1 ? a_len(a) : sizeof name - 1;
    memcpy(name, a_data(a), n);
    name[n] = 0;
    return netif_by_name(name);
}

static int set_link(const struct ifinfomsg *i, const struct attrs *at, uint16_t flags)
{
    struct netif *n = link_of(i, at);
    if (!n) return (flags & NLM_F_CREATE) ? -EOPNOTSUPP : -ENODEV;    /* no virtual links to make */
    if ((flags & NLM_F_CREATE) && (flags & NLM_F_EXCL)) return -EEXIST;
    uint32_t mtu;
    if (a_u32(at->a[IFLA_MTU], &mtu) && (mtu < 68 || mtu > (is_lo(n) ? 65535u : (uint32_t)ETH_MTU))) return -EINVAL;
    const struct rtattr *addr = at->a[IFLA_ADDRESS];
    if (addr && (a_len(addr) != ETH_ALEN || memcmp(a_data(addr), n->mac, ETH_ALEN))) return -EOPNOTSUPP;
    const struct rtattr *nm = i->index > 0 ? at->a[IFLA_IFNAME] : NULL;     /* a new name for a link given by index */
    if (nm) {
        char name[IFNAMSIZ];
        size_t k = a_len(nm);
        if (!k || k > IFNAMSIZ) return -EINVAL;
        memcpy(name, a_data(nm), k);
        name[IFNAMSIZ - 1] = 0;
        if (!name[0] || strchr(name, '/') || strchr(name, ' ')) return -EINVAL;
        struct netif *other = netif_by_name(name);
        if (other && other != n) return -EEXIST;
        strlcpy(n->name, name, sizeof n->name);
    }
    if (a_u32(at->a[IFLA_MTU], &mtu)) n->mtu = (uint16_t)mtu;
    if (i->change & IFF_UP) netif_set_up(n, i->flags & IFF_UP);
    else if (!i->change && (i->flags & IFF_UP) && !(n->flags & IFF_UP)) netif_set_up(n, true);
    rtnl_notify_link(n);
    return 0;
}

/* ---- addresses ---------------------------------------------------------------------- */
static void fill_addr(struct nlm *m, struct netif *n, uint32_t ip, uint32_t mask, uint16_t type, uint16_t flags,
                      uint32_t seq, uint32_t pid)
{
    struct ifaddrmsg *a = m_start(m, type, flags, seq, pid, sizeof *a);
    a->family = AF_INET;
    a->prefixlen = (uint8_t)mask_bits(mask);
    a->flags = IFA_F_PERMANENT;
    a->scope = is_lo(n) ? RT_SCOPE_HOST : RT_SCOPE_UNIVERSE;
    a->index = (uint32_t)n->index;
    m_attr(m, IFA_ADDRESS, &ip, 4);
    m_attr(m, IFA_LOCAL, &ip, 4);
    if (!is_lo(n) && mask != 0xffffffffu) { uint32_t b = ip | ~mask; m_attr(m, IFA_BROADCAST, &b, 4); }
    m_str(m, IFA_LABEL, n->name);
    m_u32(m, IFA_FLAGS, IFA_F_PERMANENT);
    struct ifa_cacheinfo ci = { 0xffffffffu, 0xffffffffu, 0, 0 };
    m_attr(m, IFA_CACHEINFO, &ci, sizeof ci);
}

static uint32_t prefix_mask(int len) { return len <= 0 ? 0 : be32(len >= 32 ? 0xffffffffu : ~(0xffffffffu >> len)); }

static int change_addr(bool add, const struct ifaddrmsg *a, const struct attrs *at, uint16_t flags)
{
    if (a->family != AF_INET) return -EAFNOSUPPORT;
    struct netif *n = netif_by_index((int)a->index);
    if (!n) return -ENODEV;
    uint32_t ip = 0;
    const struct rtattr *r = at->a[IFA_LOCAL] ? at->a[IFA_LOCAL] : at->a[IFA_ADDRESS];
    bool given = r && a_len(r) >= 4;
    if (given) memcpy(&ip, a_data(r), 4);
    if (add) {
        if (!given || a->prefixlen > 32) return -EINVAL;
        uint32_t mask = prefix_mask(a->prefixlen);
        if (n->ip == ip && (flags & NLM_F_EXCL)) return -EEXIST;
        if (is_lo(n) && (ip & 0xff) != 127) return -EINVAL;
        dhcp_stop(n);
        netif_set_addr(n, ip, mask);
        return 0;
    }
    if (!n->ip || (given && ip != n->ip) || (a->prefixlen && a->prefixlen != mask_bits(n->mask))) return -EADDRNOTAVAIL;
    dhcp_stop(n);
    netif_set_addr(n, 0, 0);
    return 0;
}

/* ---- routes ---------------------------------------------------------------------------- */
static bool is_subnet_route(uint32_t dst, uint32_t mask, uint32_t gw, struct netif *n)
{
    return !gw && n->ip && n->mask == mask && (n->ip & mask) == dst;
}

static void fill_route(struct nlm *m, uint32_t dst, uint32_t mask, uint32_t gw, struct netif *n, uint16_t type,
                       uint16_t flags, uint32_t seq, uint32_t pid)
{
    struct rtmsg *r = m_start(m, type, flags, seq, pid, sizeof *r);
    bool subnet = is_subnet_route(dst, mask, gw, n);
    r->family = AF_INET;
    r->dst_len = (uint8_t)mask_bits(mask);
    r->table = RT_TABLE_MAIN;
    r->protocol = subnet ? RTPROT_KERNEL : RTPROT_BOOT;
    r->scope = gw ? RT_SCOPE_UNIVERSE : RT_SCOPE_LINK;
    r->type = RTN_UNICAST;
    m_u32(m, RTA_TABLE, RT_TABLE_MAIN);
    if (mask) m_attr(m, RTA_DST, &dst, 4);
    if (gw) m_attr(m, RTA_GATEWAY, &gw, 4);
    if (subnet) m_attr(m, RTA_PREFSRC, &n->ip, 4);
    m_u32(m, RTA_OIF, (uint32_t)n->index);
}

static int change_route(bool add, const struct rtmsg *r, const struct attrs *at, uint16_t flags)
{
    if (r->family != AF_INET) return -EAFNOSUPPORT;
    if (r->dst_len > 32) return -EINVAL;
    if (r->type && r->type != RTN_UNICAST) return -EOPNOTSUPP;      /* blackhole, prohibit, local, ... */
    uint32_t table = r->table;
    a_u32(at->a[RTA_TABLE], &table);
    if (table && table != RT_TABLE_MAIN) return -EOPNOTSUPP;        /* one table */
    uint32_t mask = prefix_mask(r->dst_len), dst = 0, gw = 0, oif = 0;
    if (at->a[RTA_DST] && a_len(at->a[RTA_DST]) >= 4) memcpy(&dst, a_data(at->a[RTA_DST]), 4);
    if (at->a[RTA_GATEWAY] && a_len(at->a[RTA_GATEWAY]) >= 4) memcpy(&gw, a_data(at->a[RTA_GATEWAY]), 4);
    a_u32(at->a[RTA_OIF], &oif);
    dst &= mask;
    bool exists = false;
    for (int i = 0; i < MAX_ROUTES; i++)
        if (routes[i].used && routes[i].dst == dst && routes[i].mask == mask) exists = true;
    if (!add) return route_del(dst, mask) ? -ESRCH : 0;
    if (exists && (flags & NLM_F_EXCL)) return -EEXIST;
    if (!exists && !(flags & NLM_F_CREATE) && (flags & NLM_F_REPLACE)) return -ENOENT;
    struct netif *n = oif ? netif_by_index((int)oif) : NULL;
    if (oif && !n) return -ENODEV;
    if (gw) {
        struct netif *via = route_lookup(gw, NULL);
        if (!via || (n && via != n)) return -ENETUNREACH;          /* the gateway must be on a link */
        n = via;
    }
    if (!n) return -ENODEV;
    return route_add(dst, mask, gw, n) ? -ENOSPC : 0;
}

/* `ip route get`: where a packet to the address goes */
static int get_route(struct nlout *o, const struct nlmsghdr *h, const struct rtmsg *r, const struct attrs *at)
{
    (void)r;
    uint32_t dst;
    if (!at->a[RTA_DST] || a_len(at->a[RTA_DST]) < 4) return -EINVAL;
    memcpy(&dst, a_data(at->a[RTA_DST]), 4);
    struct nlm m;
    struct rtmsg *q = m_start(&m, RTM_NEWROUTE, 0, h->seq, h->pid, sizeof *q);
    q->family = AF_INET;
    q->dst_len = 32;
    q->protocol = 0;
    q->flags = RTM_F_CLONED;
    if (ip_is_local(dst)) {
        q->table = RT_TABLE_LOCAL; q->scope = RT_SCOPE_HOST; q->type = RTN_LOCAL;
        m_u32(&m, RTA_TABLE, RT_TABLE_LOCAL);
        m_attr(&m, RTA_DST, &dst, 4);
        m_u32(&m, RTA_OIF, (uint32_t)netif_loopback()->index);
        m_attr(&m, RTA_PREFSRC, &dst, 4);
    } else {
        uint32_t nexthop;
        struct netif *n = route_lookup(dst, &nexthop);
        if (!n) return -ENETUNREACH;
        q->table = RT_TABLE_MAIN; q->scope = RT_SCOPE_UNIVERSE; q->type = RTN_UNICAST;
        m_u32(&m, RTA_TABLE, RT_TABLE_MAIN);
        m_attr(&m, RTA_DST, &dst, 4);
        m_u32(&m, RTA_OIF, (uint32_t)n->index);
        if (nexthop != dst) m_attr(&m, RTA_GATEWAY, &nexthop, 4);
        if (n->ip) m_attr(&m, RTA_PREFSRC, &n->ip, 4);
    }
    out_put(o, &m);
    return 0;
}

/* ---- neighbours ------------------------------------------------------------------------ */
struct neigh_ctx { struct nlout *o; const struct nlmsghdr *h; };

static void neigh_one(void *ctx, struct netif *n, uint32_t ip, const uint8_t *mac, bool resolved)
{
    struct neigh_ctx *c = ctx;
    struct nlm m;
    struct ndmsg *d = m_start(&m, RTM_NEWNEIGH, NLM_F_MULTI, c->h->seq, c->h->pid, sizeof *d);
    d->family = AF_INET;
    d->ifindex = n->index;
    d->state = resolved ? NUD_REACHABLE : NUD_INCOMPLETE;
    d->type = RTN_UNICAST;
    m_attr(&m, NDA_DST, &ip, 4);
    if (resolved) m_attr(&m, NDA_LLADDR, mac, ETH_ALEN);
    out_put(c->o, &m);
}

/* ---- dispatch -------------------------------------------------------------------------- */
#define DUMPED 1

static void done(struct nlout *o, const struct nlmsghdr *h)
{
    struct nlm m;
    int32_t *v = m_start(&m, NLMSG_DONE, NLM_F_MULTI, h->seq, h->pid, 4);
    *v = 0;
    out_put(o, &m);
}

static int dump(struct nlout *o, const struct nlmsghdr *h, const uint8_t *body, size_t blen)
{
    uint8_t family = blen ? body[0] : AF_UNSPEC;        /* every request starts with the family */
    bool v4 = family == AF_UNSPEC || family == AF_INET;
    struct nlm m;
    switch (h->type) {
    case RTM_GETLINK:
        for (struct netif *n = netif_list(); n; n = n->next) { fill_link(&m, n, RTM_NEWLINK, NLM_F_MULTI, h->seq, h->pid); out_put(o, &m); }
        break;
    case RTM_GETADDR:
        if (v4) for (struct netif *n = netif_list(); n; n = n->next)
            if (n->ip) { fill_addr(&m, n, n->ip, n->mask, RTM_NEWADDR, NLM_F_MULTI, h->seq, h->pid); out_put(o, &m); }
        break;
    case RTM_GETROUTE:
        if (v4) for (int i = 0; i < MAX_ROUTES; i++) {
            struct route *r = &routes[i];
            if (!r->used || is_lo(r->nif)) continue;    /* loopback routes belong to the local table */
            fill_route(&m, r->dst, r->mask, r->gw, r->nif, RTM_NEWROUTE, NLM_F_MULTI, h->seq, h->pid);
            out_put(o, &m);
        }
        break;
    case RTM_GETNEIGH:
        if (v4) { struct neigh_ctx c = { o, h }; arp_each(neigh_one, &c); }
        break;
    case RTM_GETRULE:
        if (v4) {
            static const struct { uint32_t prio; uint8_t table; } rules[] = { { 0, RT_TABLE_LOCAL }, { 32766, RT_TABLE_MAIN },
                                                                              { 32767, RT_TABLE_DEFAULT } };
            for (size_t i = 0; i < sizeof rules / sizeof *rules; i++) {
                struct fib_rule_hdr *f = m_start(&m, RTM_NEWRULE, NLM_F_MULTI, h->seq, h->pid, sizeof *f);
                f->family = AF_INET;
                f->table = rules[i].table;
                f->action = 1;                          /* FR_ACT_TO_TBL */
                m_u32(&m, FRA_TABLE, rules[i].table);
                if (rules[i].prio) m_u32(&m, FRA_PRIORITY, rules[i].prio);
                out_put(o, &m);
            }
        }
        break;
    default:                                            /* qdiscs, classes, IPv6 ...: nothing to list */
        break;
    }
    done(o, h);
    return DUMPED;
}

static int request(struct nlout *o, const struct nlmsghdr *h)
{
    const uint8_t *body = (const uint8_t *)(h + 1);
    size_t blen = h->len - NLMSG_HDRLEN;
    bool get = (h->type & 3) == 2;                      /* RTM_GET* are 18, 22, 26, ... */
    if (get && (h->flags & NLM_F_DUMP) == NLM_F_DUMP) return dump(o, h, body, blen);
    if (!get && current_task()->uid != 0) return -EPERM;
    struct attrs at;
    struct nlm m;
    switch (h->type) {
    case RTM_GETLINK: case RTM_NEWLINK: case RTM_SETLINK: {
        if (blen < sizeof(struct ifinfomsg)) return -EINVAL;
        struct ifinfomsg i;
        memcpy(&i, body, sizeof i);
        parse_attrs(&at, body + sizeof i, blen - sizeof i);
        if (h->type != RTM_GETLINK) return set_link(&i, &at, h->type == RTM_NEWLINK ? h->flags : 0);
        struct netif *n = link_of(&i, &at);
        if (!n) return -ENODEV;
        fill_link(&m, n, RTM_NEWLINK, 0, h->seq, h->pid);
        out_put(o, &m);
        return 0;
    }
    case RTM_NEWADDR: case RTM_DELADDR: {
        if (blen < sizeof(struct ifaddrmsg)) return -EINVAL;
        struct ifaddrmsg a;
        memcpy(&a, body, sizeof a);
        parse_attrs(&at, body + sizeof a, blen - sizeof a);
        return change_addr(h->type == RTM_NEWADDR, &a, &at, h->flags);
    }
    case RTM_NEWROUTE: case RTM_DELROUTE: case RTM_GETROUTE: {
        if (blen < sizeof(struct rtmsg)) return -EINVAL;
        struct rtmsg r;
        memcpy(&r, body, sizeof r);
        parse_attrs(&at, body + sizeof r, blen - sizeof r);
        if (h->type == RTM_GETROUTE) return get_route(o, h, &r, &at);
        return change_route(h->type == RTM_NEWROUTE, &r, &at, h->flags);
    }
    case RTM_DELNEIGH: {
        if (blen < sizeof(struct ndmsg)) return -EINVAL;
        struct ndmsg d;
        memcpy(&d, body, sizeof d);
        parse_attrs(&at, body + sizeof d, blen - sizeof d);
        struct netif *n = netif_by_index(d.ifindex);
        uint32_t ip;
        if (!n) return -ENODEV;
        if (!at.a[NDA_DST] || a_len(at.a[NDA_DST]) < 4) return -EINVAL;
        memcpy(&ip, a_data(at.a[NDA_DST]), 4);
        return arp_forget(n, ip) ? -ENOENT : 0;
    }
    case RTM_DELLINK:
        return -EOPNOTSUPP;
    }
    return -EOPNOTSUPP;
}

static void error_msg(struct nlout *o, const struct nlmsghdr *h, int err)
{
    struct nlm m;
    uint8_t *p = m_start(&m, NLMSG_ERROR, NLM_F_CAPPED, h->seq, h->pid, 4 + NLMSG_HDRLEN);
    int32_t e = err;
    memcpy(p, &e, 4);
    memcpy(p + 4, h, NLMSG_HDRLEN);                     /* the request's header, without its payload */
    out_put(o, &m);
}

/* ---- sockets --------------------------------------------------------------------------- */
static bool portid_used(uint32_t id)
{
    for (struct msock *s = msocks; s; s = s->next) if (s->domain == AF_NETLINK && s->portid == id) return true;
    return false;
}

int netlink_bind(struct msock *s, const struct sockaddr_nl *a)
{
    if (a->nl_pid && s->portid && a->nl_pid != s->portid) return -EINVAL;
    if (!s->portid) {
        uint32_t id = a->nl_pid;
        if (id) {
            if (portid_used(id)) return -EADDRINUSE;
        } else {                                        /* the process' id first, as Linux does */
            static uint32_t next = 0x80000000u;
            id = (uint32_t)current_task()->tgid;
            while (portid_used(id)) id = next++ | 0x80000000u;
        }
        s->portid = id;
    }
    s->groups = a->nl_groups;
    return 0;
}

long netlink_send(struct msock *s, const uint8_t *buf, size_t len, const struct sockaddr_nl *to)
{
    if (!s->portid) {
        int rc = netlink_bind(s, &(struct sockaddr_nl){ AF_NETLINK, 0, 0, s->groups });
        if (rc) return rc;
    }
    if (to->nl_pid) {                                   /* to another socket: as it is */
        for (struct msock *d = msocks; d; d = d->next)
            if (d->domain == AF_NETLINK && d->portid == to->nl_pid) {
                struct sockaddr_nl from = { AF_NETLINK, 0, s->portid, 0 };
                int rc = msock_queue(d, buf, len, &from, sizeof from);
                return rc < 0 ? rc : (long)len;
            }
        return -ECONNREFUSED;
    }
    if (to->nl_groups) return current_task()->uid ? -EPERM : (long)len;   /* user multicast: nobody needs it */
    struct nlout *o = kmalloc(sizeof *o);
    if (!o) return -ENOMEM;
    o->to = s;
    o->len = 0;
    o->lost = false;
    size_t off = 0;
    while (len - off >= NLMSG_HDRLEN) {
        struct nlmsghdr h;
        memcpy(&h, buf + off, sizeof h);
        if (h.len < NLMSG_HDRLEN || h.len > len - off) break;
        const struct nlmsghdr *hp = (const struct nlmsghdr *)(buf + off);
        if ((h.flags & NLM_F_REQUEST) && h.type >= NLMSG_MIN_TYPE) {
            if (!h.pid) h.pid = s->portid;
            /* the request in place (aligned copies are made where it is parsed) */
            uint8_t *req = kmalloc(h.len);
            if (!req) { kfree(o); return -ENOMEM; }
            memcpy(req, hp, h.len);
            ((struct nlmsghdr *)req)->pid = h.pid;
            int rc = request(o, (const struct nlmsghdr *)req);
            if (rc != DUMPED && (rc < 0 || (h.flags & NLM_F_ACK))) error_msg(o, (const struct nlmsghdr *)req, rc < 0 ? rc : 0);
            kfree(req);
        }
        off += NLA_ALIGN(h.len);
    }
    out_flush(o);
    kfree(o);
    return (long)len;
}

void netlink_release(struct msock *s) { (void)s; }

/* ---- announcements ------------------------------------------------------------------- */
static void announce(int group, struct nlm *m)
{
    m_end(m);
    struct sockaddr_nl from = { AF_NETLINK, 0, 0, 1u << (group - 1) };
    for (struct msock *s = msocks; s; s = s->next)
        if (s->domain == AF_NETLINK && (s->groups & (1u << (group - 1))))
            msock_queue(s, m->d, m->len, &from, sizeof from);
}

static bool listeners(int group)
{
    for (struct msock *s = msocks; s; s = s->next)
        if (s->domain == AF_NETLINK && (s->groups & (1u << (group - 1)))) return true;
    return false;
}

void rtnl_notify_link(struct netif *nif)
{
    if (!listeners(GRP_LINK)) return;
    struct nlm m;
    fill_link(&m, nif, RTM_NEWLINK, 0, 0, 0);
    announce(GRP_LINK, &m);
}

void rtnl_notify_addr(struct netif *nif, uint32_t ip, uint32_t mask, bool add)
{
    if (!listeners(GRP_IPV4_IFADDR)) return;
    struct nlm m;
    fill_addr(&m, nif, ip, mask, add ? RTM_NEWADDR : RTM_DELADDR, 0, 0, 0);
    announce(GRP_IPV4_IFADDR, &m);
}

void rtnl_notify_route(uint32_t dst, uint32_t mask, uint32_t gw, struct netif *nif, bool add)
{
    if (!nif || is_lo(nif) || !listeners(GRP_IPV4_ROUTE)) return;
    struct nlm m;
    fill_route(&m, dst, mask, gw, nif, add ? RTM_NEWROUTE : RTM_DELROUTE, 0, 0, 0);
    announce(GRP_IPV4_ROUTE, &m);
}
