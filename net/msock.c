/* net/msock.c -- netlink and packet sockets: the file side (see xsock.h)
 *
 * A socket is an anonymous VSOCK vnode inside a struct msock, like the
 * AF_INET ones. What it receives is a queue of whole messages, each with
 * its sender's address, bounded by SO_RCVBUF; a message that does not fit
 * is dropped and counted, as on Linux. netlink.c and packet.c fill the
 * queues and handle what is sent; everything runs under net_lock(). */
#include "msock.h"
#include "sock.h"
#include <kernel/socket.h>
#include <kernel/task.h>
#include <kernel/process.h>
#include <kernel/string.h>
#include <kernel/posix.h>
#include <kernel/mm.h>
#include <kernel/time.h>

#define MSOCK_RCVBUF    212992

struct msock *msocks;
static const struct vnode_ops msock_ops;

bool xsock_is(struct file *f) { return f && f->vn && f->vn->ops == &msock_ops; }

int msock_queue(struct msock *s, const void *data, size_t len, const void *addr, size_t addrlen)
{
    if (s->shut_rd) return 0;
    if (s->rq && s->rq_bytes + len > s->rcvbuf) {        /* one message always fits an empty queue */
        s->drops++;
        if (s->domain == AF_NETLINK) s->err = ENOBUFS;  /* the listener has lost track: tell it */
        wakeup(s);
        return -ENOBUFS;
    }
    struct mmsg *m = kmalloc(sizeof *m + len);
    if (!m) { s->drops++; return -ENOBUFS; }
    m->next = NULL;
    m->len = (uint32_t)len;
    m->addrlen = (uint8_t)(addrlen < sizeof m->addr ? addrlen : sizeof m->addr);
    memcpy(m->addr, addr, m->addrlen);
    memcpy(m->data, data, len);
    if (s->rq_tail) s->rq_tail->next = m; else s->rq = m;
    s->rq_tail = m;
    s->rq_bytes += len;
    wakeup(s);
    return 0;
}

static void msock_flush(struct msock *s)
{
    while (s->rq) { struct mmsg *m = s->rq; s->rq = m->next; kfree(m); }
    s->rq_tail = NULL;
    s->rq_bytes = 0;
}

int xsock_create(int domain, int type, int protocol, struct file **out)
{
    int flags = type & (SOCK_NONBLOCK | SOCK_CLOEXEC);
    type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (domain == AF_NETLINK) {
        if (type != SOCK_RAW && type != SOCK_DGRAM) return -ESOCKTNOSUPPORT;
        if (protocol != NETLINK_ROUTE) return -EPROTONOSUPPORT;
    } else if (domain == AF_PACKET) {
        if (type != SOCK_RAW && type != SOCK_DGRAM) return -ESOCKTNOSUPPORT;
        if (current_task()->uid != 0) return -EPERM;
    } else return -EAFNOSUPPORT;
    struct msock *s = kzalloc(sizeof *s);
    if (!s) return -ENOMEM;
    s->domain = domain;
    s->type = type;
    s->proto = protocol;
    s->rcvbuf = s->sndbuf = MSOCK_RCVBUF;
    if (domain == AF_PACKET) s->pproto = (uint16_t)protocol;   /* already in network order */
    strlcpy(s->vn.name, "socket", sizeof s->vn.name);
    s->vn.type = VSOCK;
    s->vn.mode = S_IFSOCK | 0777;
    s->vn.ops = &msock_ops;
    s->vn.data = s;
    struct file *file = vfs_file_new(&s->vn, O_RDWR | ((flags & SOCK_NONBLOCK) ? O_NONBLOCK : 0));
    if (!file) { kfree(s); return -ENOMEM; }
    uint64_t fl = net_lock();
    s->next = msocks;
    msocks = s;
    if (domain == AF_PACKET) msocks_packet++;
    net_unlock(fl);
    *out = file;
    return 0;
}

int xsock_bind(struct file *f, const void *addr, uint32_t len)
{
    struct msock *s = msk(f);
    uint64_t fl = net_lock();
    int rc;
    if (s->domain == AF_NETLINK) {
        struct sockaddr_nl a;
        if (len < sizeof a) rc = -EINVAL;
        else { memcpy(&a, addr, sizeof a); rc = a.nl_family != AF_NETLINK ? -EINVAL : netlink_bind(s, &a); }
    } else {
        struct sockaddr_ll a;
        memset(&a, 0, sizeof a);
        if (len < 12) rc = -EINVAL;
        else { memcpy(&a, addr, len < sizeof a ? len : sizeof a); rc = packet_bind(s, &a, len); }
    }
    net_unlock(fl);
    return rc;
}

int xsock_connect(struct file *f, const void *addr, uint32_t len)
{
    struct msock *s = msk(f);
    if (s->domain != AF_NETLINK) return -EOPNOTSUPP;
    struct sockaddr_nl a;
    if (len < sizeof(uint16_t)) return -EINVAL;
    memset(&a, 0, sizeof a);
    memcpy(&a, addr, len < sizeof a ? len : sizeof a);
    uint64_t fl = net_lock();
    int rc = 0;
    if (a.nl_family == AF_UNSPEC) s->dst_portid = s->dst_groups = 0;   /* disconnect */
    else if (a.nl_family != AF_NETLINK || len < sizeof a) rc = -EINVAL;
    else {
        if (!s->portid) rc = netlink_bind(s, &(struct sockaddr_nl){ AF_NETLINK, 0, 0, 0 });
        if (!rc) { s->dst_portid = a.nl_pid; s->dst_groups = a.nl_groups; }
    }
    net_unlock(fl);
    return rc;
}

long xsock_sendto(struct file *f, const void *buf, size_t len, int flags, const void *to, uint32_t tolen)
{
    (void)flags;
    struct msock *s = msk(f);
    uint64_t fl = net_lock();
    long rc;
    if (s->domain == AF_NETLINK) {
        struct sockaddr_nl a = { AF_NETLINK, 0, s->dst_portid, s->dst_groups };
        if (to && tolen) {
            if (tolen < sizeof a) { net_unlock(fl); return -EINVAL; }
            memcpy(&a, to, sizeof a);
            if (a.nl_family != AF_NETLINK) { net_unlock(fl); return -EINVAL; }
        }
        rc = netlink_send(s, buf, len, &a);
    } else {
        struct sockaddr_ll a;
        memset(&a, 0, sizeof a);
        if (to && tolen) memcpy(&a, to, tolen < sizeof a ? tolen : sizeof a);
        rc = packet_send(s, buf, len, to && tolen ? &a : NULL, to ? tolen : 0);
    }
    net_unlock(fl);
    return rc;
}

long xsock_recvfrom(struct file *f, void *buf, size_t len, int flags, void *from, uint32_t *fromlen, int *msg_flags)
{
    struct msock *s = msk(f);
    uint64_t fl = net_lock();
    uint64_t deadline = sock_deadline(s->rcvtimeo);
    while (!s->rq) {
        if (s->err) { int e = s->err; s->err = 0; net_unlock(fl); return -e; }
        if (s->shut_rd) { net_unlock(fl); return 0; }
        if ((f->flags & O_NONBLOCK) || (flags & MSG_DONTWAIT)) { net_unlock(fl); return -EAGAIN; }
        int rc = sock_wait(s, deadline);
        if (rc) { net_unlock(fl); return rc; }
    }
    struct mmsg *m = s->rq;
    size_t n = m->len < len ? m->len : len;
    if (buf && n) memcpy(buf, m->data, n);
    if (from) memcpy(from, m->addr, m->addrlen);
    if (fromlen) *fromlen = m->addrlen;
    long ret = (long)n;
    if (m->len > len) {                                 /* the rest of the message is lost */
        if (msg_flags) *msg_flags |= MSG_TRUNC;
        if (flags & MSG_TRUNC) ret = (long)m->len;
    }
    if (!(flags & MSG_PEEK)) {
        s->rq = m->next;
        if (!s->rq) s->rq_tail = NULL;
        s->rq_bytes -= m->len;
        kfree(m);
    }
    net_unlock(fl);
    return ret;
}

int xsock_shutdown(struct file *f, int how)
{
    struct msock *s = msk(f);
    uint64_t fl = net_lock();
    if (how == SHUT_RD || how == SHUT_RDWR) { s->shut_rd = true; msock_flush(s); wakeup(s); }
    net_unlock(fl);
    return 0;
}

int xsock_name(struct file *f, bool peer, void *out, uint32_t *len)
{
    struct msock *s = msk(f);
    uint64_t fl = net_lock();
    if (s->domain == AF_NETLINK) {
        struct sockaddr_nl a = { AF_NETLINK, 0, peer ? s->dst_portid : s->portid, peer ? s->dst_groups : s->groups };
        memcpy(out, &a, sizeof a);
        *len = sizeof a;
    } else {
        if (peer) { net_unlock(fl); return -EOPNOTSUPP; }
        struct sockaddr_ll a;
        memset(&a, 0, sizeof a);
        a.sll_family = AF_PACKET;
        a.sll_protocol = s->pproto;
        a.sll_ifindex = s->ifindex;
        struct netif *n = s->ifindex ? netif_by_index(s->ifindex) : NULL;
        if (n) {
            a.sll_hatype = n == netif_loopback() ? 772 : 1;
            a.sll_halen = ETH_ALEN;
            memcpy(a.sll_addr, n->mac, ETH_ALEN);
        }
        memcpy(out, &a, sizeof a);
        *len = sizeof a;
    }
    net_unlock(fl);
    return 0;
}

static int opt_int(const void *val, uint32_t len, int *out)
{
    if (len < 4) { if (len == 1) { *out = *(const uint8_t *)val; return 0; } return -EINVAL; }
    memcpy(out, val, 4);
    return 0;
}

int xsock_setsockopt(struct file *f, int level, int opt, const void *val, uint32_t len)
{
    struct msock *s = msk(f);
    int v = 0;
    if (level == SOL_SOCKET) {
        if (opt == SO_RCVTIMEO || opt == SO_SNDTIMEO) {
            if (len < 16) return -EINVAL;
            int64_t tv[2];
            memcpy(tv, val, 16);
            if (tv[0] < 0 || tv[1] < 0 || tv[1] >= 1000000) return -EDOM;
            uint64_t ms = (uint64_t)tv[0] * 1000 + (uint64_t)(tv[1] + 999) / 1000;
            if (opt == SO_RCVTIMEO) s->rcvtimeo = ms > 0xffffffffu ? 0 : (uint32_t)ms;
            return 0;
        }
        if (opt == SO_ATTACH_FILTER) {
            if (s->domain != AF_PACKET) return -EINVAL;   /* netlink: nothing to filter for anyone yet */
            if (len != sizeof(struct kfprog)) return -EINVAL;
            uint64_t fl = net_lock();
            int rc = s->filter_locked ? -EPERM : packet_attach_filter(s, val);
            net_unlock(fl);
            return rc;
        }
        if (opt == SO_DETACH_FILTER) {
            uint64_t fl = net_lock();
            int rc = s->filter_locked ? -EPERM : !s->filter ? -ENOENT : 0;
            if (!rc) { kfree(s->filter); s->filter = NULL; s->nfilter = 0; }
            net_unlock(fl);
            return rc;
        }
        if (opt == SO_BINDTODEVICE) return 0;
        int rc = opt_int(val, len, &v);
        if (rc) return rc;
        switch (opt) {
        case SO_RCVBUF: case SO_RCVBUFFORCE:
            if (v < 256) v = 256;
            s->rcvbuf = v > (8 << 20) ? (8u << 20) : (uint32_t)v * 2;   /* Linux doubles it */
            return 0;
        case SO_SNDBUF: case SO_SNDBUFFORCE:
            if (v < 256) v = 256;
            s->sndbuf = v > (8 << 20) ? (8u << 20) : (uint32_t)v * 2;
            return 0;
        case SO_LOCK_FILTER: s->filter_locked = v != 0; return 0;
        case SO_REUSEADDR: case SO_BROADCAST: case SO_TIMESTAMP: case SO_DEBUG: case SO_PASSCRED:
            return 0;
        }
        return -ENOPROTOOPT;
    }
    if (level == SOL_NETLINK && s->domain == AF_NETLINK) {
        int rc = opt_int(val, len, &v);
        if (rc) return rc;
        switch (opt) {
        case 1: case 2: {                               /* NETLINK_ADD_MEMBERSHIP, _DROP_MEMBERSHIP */
            if (v < 1) return -EINVAL;
            if (v > 32) return 0;                       /* groups no one announces on */
            uint64_t fl = net_lock();
            if (!s->portid) netlink_bind(s, &(struct sockaddr_nl){ AF_NETLINK, 0, 0, s->groups });
            if (opt == 1) s->groups |= 1u << (v - 1); else s->groups &= ~(1u << (v - 1));
            net_unlock(fl);
            return 0;
        }
        case 3: case 5: case 8: case 10: case 11: case 12:  /* PKTINFO, NO_ENOBUFS, LISTEN_ALL_NSID,
                                                             * CAP_ACK, EXT_ACK, GET_STRICT_CHK */
            return 0;
        }
        return -ENOPROTOOPT;
    }
    if (level == SOL_PACKET && s->domain == AF_PACKET) {
        switch (opt) {
        case 1: case 2:                                 /* PACKET_ADD_MEMBERSHIP, _DROP_MEMBERSHIP: the cards take every multicast */
        case 8: case 10: case 11: case 15:              /* AUXDATA, VERSION, RESERVE, TIMESTAMP */
            return 0;
        }
        return -ENOPROTOOPT;
    }
    return -ENOPROTOOPT;
}

int xsock_getsockopt(struct file *f, int level, int opt, void *val, uint32_t *len)
{
    struct msock *s = msk(f);
    int v;
    if (level == SOL_SOCKET) {
        switch (opt) {
        case SO_TYPE:     v = s->type; break;
        case SO_PROTOCOL: v = s->proto; break;
        case SO_DOMAIN:   v = s->domain; break;
        case SO_RCVBUF:   v = (int)s->rcvbuf; break;
        case SO_SNDBUF:   v = (int)s->sndbuf; break;
        case SO_ERROR: { uint64_t fl = net_lock(); v = s->err; s->err = 0; net_unlock(fl); break; }
        case SO_ACCEPTCONN: v = 0; break;
        default: return -ENOPROTOOPT;
        }
    } else if (level == SOL_NETLINK && s->domain == AF_NETLINK) {
        v = 0;
    } else if (level == SOL_PACKET && s->domain == AF_PACKET) {
        if (opt != 6) return -ENOPROTOOPT;              /* PACKET_STATISTICS: received, dropped */
        uint64_t fl = net_lock();
        uint32_t st[2] = { 0, (uint32_t)s->drops };
        s->drops = 0;
        net_unlock(fl);
        uint32_t n = *len < sizeof st ? *len : (uint32_t)sizeof st;
        memcpy(val, st, n);
        *len = n;
        return 0;
    } else return -ENOPROTOOPT;
    uint32_t n = *len < 4 ? *len : 4;
    memcpy(val, &v, n);
    *len = n;
    return 0;
}

/* ---- file operations ------------------------------------------------------------- */
static ssize_t msock_fread(struct file *f, void *buf, size_t len) { return xsock_recvfrom(f, buf, len, 0, NULL, NULL, NULL); }
static ssize_t msock_fwrite(struct file *f, const void *buf, size_t len) { return xsock_sendto(f, buf, len, 0, NULL, 0); }

static int msock_poll(struct file *f, int events)
{
    struct msock *s = msk(f);
    uint64_t fl = net_lock();
    poll_wait(s);
    int r = POLLOUT | POLLWRNORM;
    if (s->rq || s->shut_rd) r |= POLLIN | POLLRDNORM;
    if (s->err) r |= POLLERR;
    net_unlock(fl);
    return r & (events | POLLERR | POLLHUP | POLLNVAL);
}

#define FIONREAD 0x541B
static int msock_ioctl(struct vnode *vn, unsigned long req, void *arg)
{
    struct msock *s = vn->data;
    if (req == FIONREAD) {
        uint64_t fl = net_lock();
        *(int *)arg = s->rq ? (int)s->rq->len : 0;
        net_unlock(fl);
        return 0;
    }
    if (net_ioctl_is(req)) return net_ioctl(req, arg);
    return -ENOTTY;
}

static void msock_release(struct file *f)
{
    struct msock *s = msk(f);
    uint64_t fl = net_lock();
    for (struct msock **pp = &msocks; *pp; pp = &(*pp)->next)
        if (*pp == s) { *pp = s->next; break; }
    if (s->domain == AF_NETLINK) netlink_release(s);
    else msocks_packet--;
    msock_flush(s);
    wakeup(s);
    net_unlock(fl);
    kfree(s->filter);
    kfree(s);
}

static const struct vnode_ops msock_ops = {
    .fread = msock_fread, .fwrite = msock_fwrite, .poll = msock_poll, .release = msock_release, .ioctl = msock_ioctl,
    .nolock = true,                         /* net_lock(), net/core.c */
};
