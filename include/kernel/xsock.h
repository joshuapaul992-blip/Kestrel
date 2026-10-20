/* include/kernel/xsock.h -- netlink (AF_NETLINK) and packet (AF_PACKET)
 * sockets for system calls (net/msock.c, net/netlink.c, net/packet.c)
 *
 * Both are datagram sockets whose data comes from the kernel: a queue of
 * whole messages, each with the address it came from. The calls mirror the
 * AF_INET ones in inet.h: kernel buffers in, the system call layer copies
 * to and from user memory.
 *
 * Netlink: NETLINK_ROUTE, what `ip`, glibc's and musl's getifaddrs() and
 * if_nameindex() use: links, IPv4 addresses, routes, neighbours (the ARP
 * cache) and the routing rules, read with dumps and changed with requests,
 * plus the multicast groups that announce changes (RTMGRP_LINK,
 * _IPV4_IFADDR, _IPV4_ROUTE, _NEIGH).
 *
 * Packet: SOCK_RAW (whole Ethernet frames) and SOCK_DGRAM (the payload,
 * with the header in the address), for one protocol or ETH_P_ALL, on one
 * interface or all, in both directions, with classic BPF filters
 * (SO_ATTACH_FILTER). Root only. What DHCP clients such as udhcpc use. */
#ifndef KESTREL_XSOCK_H
#define KESTREL_XSOCK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define NETLINK_ROUTE   0
#define SOL_PACKET      263
#define SOL_NETLINK     270
#define SO_ATTACH_FILTER 26
#define SO_DETACH_FILTER 27
#define SO_LOCK_FILTER  44
#define ETH_P_ALL       0x0003

struct sockaddr_nl { uint16_t nl_family, nl_pad; uint32_t nl_pid, nl_groups; };
struct sockaddr_ll {
    uint16_t sll_family, sll_protocol;      /* protocol in network order */
    int32_t  sll_ifindex;
    uint16_t sll_hatype;
    uint8_t  sll_pkttype, sll_halen;
    uint8_t  sll_addr[8];
};

struct bpf_insn { uint16_t code; uint8_t jt, jf; uint32_t k; };
#define BPF_MAXINSNS    4096
/* SO_ATTACH_FILTER from the system call layer: the program, already copied in */
struct kfprog { uint32_t len; const struct bpf_insn *insns; };

struct file;
struct netif;

bool xsock_is(struct file *f);
int  xsock_create(int domain, int type, int protocol, struct file **out);
int  xsock_bind(struct file *f, const void *addr, uint32_t len);
int  xsock_connect(struct file *f, const void *addr, uint32_t len);
long xsock_sendto(struct file *f, const void *buf, size_t len, int flags, const void *to, uint32_t tolen);
/* from: room for 128 bytes; *fromlen gets the address' length */
long xsock_recvfrom(struct file *f, void *buf, size_t len, int flags, void *from, uint32_t *fromlen, int *msg_flags);
int  xsock_shutdown(struct file *f, int how);
int  xsock_name(struct file *f, bool peer, void *out, uint32_t *len);
int  xsock_setsockopt(struct file *f, int level, int opt, const void *val, uint32_t len);
int  xsock_getsockopt(struct file *f, int level, int opt, void *val, uint32_t *len);

/* The stack's hooks (net_lock held) */
extern int msocks_packet;               /* open packet sockets: frames are shown to them */
void packet_tap(struct netif *nif, const uint8_t *frame, size_t len, bool outgoing);
void rtnl_notify_link(struct netif *nif);
void rtnl_notify_addr(struct netif *nif, uint32_t ip, uint32_t mask, bool add);
void rtnl_notify_route(uint32_t dst, uint32_t mask, uint32_t gw, struct netif *nif, bool add);

#endif
