/* net/msock.h -- netlink and packet sockets, shared by net/msock.c,
 * net/netlink.c and net/packet.c (the system call side is xsock.h) */
#ifndef KESTREL_NET_MSOCK_H
#define KESTREL_NET_MSOCK_H

#include <kernel/net.h>
#include <kernel/vfs.h>
#include <kernel/inet.h>
#include <kernel/xsock.h>

/* One queued message and the address it came from */
struct mmsg {
    struct mmsg *next;
    uint32_t len;
    uint8_t  addrlen;
    uint8_t  addr[23];
    uint8_t  data[];
};

struct msock {
    struct vnode vn;                    /* vn.data == this */
    int      domain, type, proto;
    struct mmsg *rq, *rq_tail;
    size_t   rq_bytes;
    uint32_t rcvbuf, sndbuf;
    uint32_t rcvtimeo;                  /* ms, 0 = forever */
    bool     shut_rd;
    int      err;                       /* pending error (SO_ERROR), positive errno */
    uint64_t drops;
    /* netlink */
    uint32_t portid;                    /* 0 until bound (autobound on first send) */
    uint32_t groups;                    /* multicast groups 1-32, as a bit mask */
    uint32_t dst_portid, dst_groups;    /* connect() */
    /* packet */
    int      ifindex;                   /* 0 = every interface */
    uint16_t pproto;                    /* network order; 0 = nothing until bound */
    struct bpf_insn *filter;
    uint32_t nfilter;
    bool     filter_locked;
    struct msock *next;                 /* all netlink and packet sockets */
};

static inline struct msock *msk(struct file *f) { return f->vn->data; }

extern struct msock *msocks;

/* Queue a message on s (net_lock held). -ENOBUFS, counted, when it is full. */
int  msock_queue(struct msock *s, const void *data, size_t len, const void *addr, size_t addrlen);

/* net/netlink.c */
int  netlink_bind(struct msock *s, const struct sockaddr_nl *a);
long netlink_send(struct msock *s, const uint8_t *buf, size_t len, const struct sockaddr_nl *to);
void netlink_release(struct msock *s);

/* net/packet.c */
int  packet_bind(struct msock *s, const struct sockaddr_ll *a, uint32_t len);
long packet_send(struct msock *s, const uint8_t *buf, size_t len, const struct sockaddr_ll *to, uint32_t tolen);
int  packet_attach_filter(struct msock *s, const struct kfprog *p);

#endif
