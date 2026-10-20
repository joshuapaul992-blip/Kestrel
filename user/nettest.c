/* user/nettest.c -- AF_INET sockets on Kestrel: UDP, TCP, ICMP, interfaces;
 * netlink (rtnetlink, getifaddrs) and packet sockets with BPF filters
 *
 *   nettest                       loopback checks only
 *   nettest HOST:PORT             also: TCP and UDP echo through the network
 *                                 card to a server on HOST (QEMU user
 *                                 networking: 10.0.2.2 is the host), ping
 *                                 the gateway, wait for DHCP
 *   nettest serve PORT            accept one connection, read until EOF and
 *                                 answer with the byte count and checksum
 *                                 (driven from the host through hostfwd)
 * Exit status 0 = every check passed. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <netpacket/packet.h>
#include <netinet/if_ether.h>

static int failures;
#define CHECK(c, what) do { int ok_ = (c); printf("nettest:  %s  %s\n", ok_ ? "ok  " : "FAIL", what); \
                            if (!ok_) { failures++; printf("nettest:        (errno %d: %s)\n", errno, strerror(errno)); } } while (0)

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + t.tv_nsec / 1e9;
}

static struct sockaddr_in sin_of(const char *ip, int port)
{
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons((uint16_t)port) };
    inet_pton(AF_INET, ip, &a.sin_addr);
    return a;
}

static int port_of(int fd)
{
    struct sockaddr_in a;
    socklen_t l = sizeof a;
    if (getsockname(fd, (struct sockaddr *)&a, &l) < 0) return -1;
    return ntohs(a.sin_port);
}

/* A listener on 127.0.0.1, any port; returns the fd, port in *port. */
static int tcp_listener(const char *ip, int *port)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = sin_of(ip, *port);
    if (bind(s, (struct sockaddr *)&a, sizeof a) < 0 || listen(s, 16) < 0) { close(s); return -1; }
    *port = port_of(s);
    return s;
}

static int tcp_connect_to(const char *ip, int port)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = sin_of(ip, port);
    if (connect(s, (struct sockaddr *)&a, sizeof a) < 0) { int e = errno; close(s); errno = e; return -1; }
    return s;
}

static int write_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    while (len) {
        ssize_t n = write(fd, p, len);
        if (n <= 0) { if (n < 0 && errno == EINTR) continue; return -1; }
        p += n; len -= (size_t)n;
    }
    return 0;
}

static ssize_t read_all(int fd, void *buf, size_t len)
{
    char *p = buf;
    size_t got = 0;
    while (got < len) {
        ssize_t n = read(fd, p + got, len - got);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        got += (size_t)n;
    }
    return (ssize_t)got;
}

static uint32_t fnv(const unsigned char *p, size_t n, uint32_t h)
{
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

static void fill(unsigned char *b, size_t n, uint32_t seed)
{
    for (size_t i = 0; i < n; i++) { seed = seed * 1103515245u + 12345u; b[i] = (unsigned char)(seed >> 16); }
}

/* ---- interfaces ----------------------------------------------------------- */
static void test_interfaces(void)
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK(s >= 0, "socket(AF_INET, SOCK_DGRAM)");
    struct ifreq r;
    memset(&r, 0, sizeof r);
    strcpy(r.ifr_name, "lo");
    CHECK(ioctl(s, SIOCGIFADDR, &r) == 0 && ((struct sockaddr_in *)&r.ifr_addr)->sin_addr.s_addr == htonl(INADDR_LOOPBACK),
          "lo is 127.0.0.1 (SIOCGIFADDR)");
    CHECK(ioctl(s, SIOCGIFFLAGS, &r) == 0 && (r.ifr_flags & IFF_UP) && (r.ifr_flags & IFF_LOOPBACK), "lo is up (SIOCGIFFLAGS)");
    CHECK(ioctl(s, SIOCGIFINDEX, &r) == 0 && r.ifr_ifindex == 1, "lo has index 1");
    CHECK(if_nametoindex("lo") == 1, "if_nametoindex(\"lo\")");
    char buf[512];
    struct ifconf ic = { .ifc_len = sizeof buf, .ifc_buf = buf };
    CHECK(ioctl(s, SIOCGIFCONF, &ic) == 0 && ic.ifc_len >= (int)sizeof(struct ifreq), "SIOCGIFCONF lists interfaces");
    memset(&r, 0, sizeof r);
    strcpy(r.ifr_name, "nosuch0");
    CHECK(ioctl(s, SIOCGIFADDR, &r) < 0 && errno == ENODEV, "unknown interface: ENODEV");
    close(s);
    int fd = open("/proc/net/dev", O_RDONLY);
    ssize_t n = fd >= 0 ? read(fd, buf, sizeof buf - 1) : -1;
    if (n > 0) buf[n] = 0;
    CHECK(n > 0 && strstr(buf, "lo:"), "/proc/net/dev lists lo");
    if (fd >= 0) close(fd);
}

/* ---- UDP ------------------------------------------------------------------------ */
static void test_ping(const char *gw);

/* The netlink and BPF ABI (musl ships no kernel headers): just what is used here */
struct sockaddr_nl { unsigned short nl_family, nl_pad; uint32_t nl_pid, nl_groups; };
struct nlmsghdr { uint32_t nlmsg_len; uint16_t nlmsg_type, nlmsg_flags; uint32_t nlmsg_seq, nlmsg_pid; };
struct nlmsgerr { int error; struct nlmsghdr msg; };
struct rtattr { unsigned short rta_len, rta_type; };
struct rtgenmsg { unsigned char rtgen_family; };
struct ifinfomsg { unsigned char ifi_family, ifi_pad; unsigned short ifi_type; int ifi_index; unsigned ifi_flags, ifi_change; };
struct ifaddrmsg { uint8_t ifa_family, ifa_prefixlen, ifa_flags, ifa_scope; uint32_t ifa_index; };
struct rtmsg { unsigned char rtm_family, rtm_dst_len, rtm_src_len, rtm_tos, rtm_table, rtm_protocol, rtm_scope, rtm_type; unsigned rtm_flags; };
struct sock_filter { uint16_t code; uint8_t jt, jf; uint32_t k; };
struct sock_fprog { unsigned short len; struct sock_filter *filter; };
#define NETLINK_ROUTE   0
#define NLM_F_REQUEST   1
#define NLM_F_MULTI     2
#define NLM_F_ACK       4
#define NLM_F_DUMP      0x300
#define NLM_F_CREATE    0x400
#define NLMSG_ERROR     2
#define NLMSG_DONE      3
#define NLMSG_ALIGN(n)  (((n) + 3u) & ~3u)
#define NLMSG_HDRLEN    ((int)NLMSG_ALIGN(sizeof(struct nlmsghdr)))
#define NLMSG_LENGTH(n) ((n) + NLMSG_HDRLEN)
#define NLMSG_DATA(h)   ((void *)((char *)(h) + NLMSG_HDRLEN))
#define NLMSG_OK(h, n)  ((n) >= (int)sizeof(struct nlmsghdr) && (h)->nlmsg_len >= sizeof(struct nlmsghdr) && (h)->nlmsg_len <= (unsigned)(n))
#define NLMSG_NEXT(h, n) ((n) -= NLMSG_ALIGN((h)->nlmsg_len), (struct nlmsghdr *)((char *)(h) + NLMSG_ALIGN((h)->nlmsg_len)))
#define RTA_ALIGN(n)    (((n) + 3u) & ~3u)
#define RTA_LENGTH(n)   (RTA_ALIGN(sizeof(struct rtattr)) + (n))
#define RTA_DATA(a)     ((void *)((char *)(a) + RTA_LENGTH(0)))
#define RTA_OK(a, n)    ((n) >= (int)sizeof(struct rtattr) && (a)->rta_len >= sizeof(struct rtattr) && (a)->rta_len <= (n))
#define RTA_NEXT(a, n)  ((n) -= RTA_ALIGN((a)->rta_len), (struct rtattr *)((char *)(a) + RTA_ALIGN((a)->rta_len)))
#define IFLA_RTA(i)     ((struct rtattr *)((char *)(i) + NLMSG_ALIGN(sizeof(struct ifinfomsg))))
#define IFLA_PAYLOAD(h) ((h)->nlmsg_len - NLMSG_LENGTH(NLMSG_ALIGN(sizeof(struct ifinfomsg))))
#define IFLA_IFNAME     3
#define IFLA_MTU        4
#define RTA_OIF         4
#define RTM_NEWLINK     16
#define RTM_GETLINK     18
#define RTM_SETLINK     19
#define RTM_NEWADDR     20
#define RTM_GETADDR     22
#define RTM_NEWROUTE    24
#define RTMGRP_LINK     1
#define RT_TABLE_MAIN   254
#define RTN_UNICAST     1
#define BPF_LD 0x00
#define BPF_JMP 0x05
#define BPF_RET 0x06
#define BPF_B 0x10
#define BPF_ABS 0x20
#define BPF_JA 0x00
#define BPF_JEQ 0x10
#define BPF_K 0x00
#ifndef SO_ATTACH_FILTER
#define SO_ATTACH_FILTER 26
#define SO_DETACH_FILTER 27
#endif

/* ---- netlink ------------------------------------------------------------------- */
static int nl_open(unsigned groups)
{
    int s = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    struct sockaddr_nl a = { .nl_family = AF_NETLINK, .nl_groups = groups };
    if (s >= 0 && bind(s, (struct sockaddr *)&a, sizeof a) < 0) { close(s); return -1; }
    return s;
}

static int nl_request(int s, int type, int flags, const void *body, size_t blen, unsigned seq)
{
    char buf[512];
    struct nlmsghdr *h = (struct nlmsghdr *)buf;
    memset(buf, 0, sizeof buf);
    h->nlmsg_len = NLMSG_LENGTH(blen);
    h->nlmsg_type = (unsigned short)type;
    h->nlmsg_flags = (unsigned short)(NLM_F_REQUEST | flags);
    h->nlmsg_seq = seq;
    memcpy(NLMSG_DATA(h), body, blen);
    struct sockaddr_nl k = { .nl_family = AF_NETLINK };
    return sendto(s, buf, h->nlmsg_len, 0, (struct sockaddr *)&k, sizeof k) == (ssize_t)h->nlmsg_len ? 0 : -1;
}

/* The answer to request seq: an NLMSG_ERROR's code (0 = ACK), or 1 when a reply of
 * type `want` came; *found counts dump messages naming `name` (links) */
static int nl_answer(int s, unsigned seq, int want, const char *name, int *count, int *found)
{
    static char buf[16384];
    for (;;) {
        struct pollfd pf = { s, POLLIN, 0 };
        if (poll(&pf, 1, 2000) != 1) return -1000;
        ssize_t n = recv(s, buf, sizeof buf, 0);
        if (n < 0) return -1001;
        for (struct nlmsghdr *h = (struct nlmsghdr *)buf; NLMSG_OK(h, (size_t)n); h = NLMSG_NEXT(h, n)) {
            if (h->nlmsg_seq != seq) continue;
            if (h->nlmsg_type == NLMSG_DONE) return 1;
            if (h->nlmsg_type == NLMSG_ERROR) return ((struct nlmsgerr *)NLMSG_DATA(h))->error;
            if (h->nlmsg_type != want) continue;
            if (count) (*count)++;
            if (want == RTM_NEWLINK && name && found) {
                struct ifinfomsg *ifi = NLMSG_DATA(h);
                int len = (int)IFLA_PAYLOAD(h);
                for (struct rtattr *a = IFLA_RTA(ifi); RTA_OK(a, len); a = RTA_NEXT(a, len))
                    if (a->rta_type == IFLA_IFNAME && !strcmp(RTA_DATA(a), name)) (*found)++;
            }
            if (!(h->nlmsg_flags & NLM_F_MULTI)) return 1;
        }
    }
}

static void test_netlink(void)
{
    struct ifaddrs *ifa = NULL;
    int lo4 = 0, lopkt = 0;
    CHECK(getifaddrs(&ifa) == 0, "getifaddrs() (netlink in musl)");
    for (struct ifaddrs *i = ifa; i; i = i->ifa_next) {
        if (strcmp(i->ifa_name, "lo") || !i->ifa_addr) continue;
        if (i->ifa_addr->sa_family == AF_INET && ((struct sockaddr_in *)i->ifa_addr)->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) lo4++;
        if (i->ifa_addr->sa_family == AF_PACKET) lopkt++;
    }
    if (ifa) freeifaddrs(ifa);
    CHECK(lo4 == 1 && lopkt == 1, "getifaddrs: lo with 127.0.0.1 and its link");
    struct if_nameindex *ni = if_nameindex();
    int seen = 0;
    for (struct if_nameindex *p = ni; p && p->if_index; p++) if (p->if_index == 1 && !strcmp(p->if_name, "lo")) seen = 1;
    if (ni) if_freenameindex(ni);
    CHECK(seen, "if_nameindex() lists lo as 1");

    int s = nl_open(0), mon = nl_open(RTMGRP_LINK);
    CHECK(s >= 0 && mon >= 0, "socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE) and bind");
    struct sockaddr_nl me;
    socklen_t ml = sizeof me;
    CHECK(getsockname(s, (struct sockaddr *)&me, &ml) == 0 && me.nl_family == AF_NETLINK && me.nl_pid == (unsigned)getpid(),
          "getsockname: the port id is the pid");
    struct rtgenmsg g = { .rtgen_family = AF_UNSPEC };
    int links = 0, lo = 0;
    nl_request(s, RTM_GETLINK, NLM_F_DUMP, &g, sizeof g, 1);
    CHECK(nl_answer(s, 1, RTM_NEWLINK, "lo", &links, &lo) == 1 && links >= 1 && lo == 1, "RTM_GETLINK dump lists lo, then NLMSG_DONE");
    /* the next datagram's size, without taking it (iproute2 sizes its buffer so) */
    nl_request(s, RTM_GETLINK, NLM_F_DUMP, &g, sizeof g, 2);
    char tiny[4];
    ssize_t full = recv(s, tiny, sizeof tiny, MSG_PEEK | MSG_TRUNC);
    CHECK(full > (ssize_t)sizeof tiny, "recv(MSG_PEEK | MSG_TRUNC): the whole length");
    nl_answer(s, 2, RTM_NEWLINK, NULL, NULL, NULL);
    struct ifinfomsg ifi = { .ifi_family = AF_UNSPEC, .ifi_index = 1 };
    nl_request(s, RTM_GETLINK, 0, &ifi, sizeof ifi, 3);
    int one = 0;
    CHECK(nl_answer(s, 3, RTM_NEWLINK, "lo", &one, &one) == 1 && one == 2, "RTM_GETLINK of index 1: lo");
    ifi.ifi_index = 4242;
    nl_request(s, RTM_GETLINK, 0, &ifi, sizeof ifi, 4);
    CHECK(nl_answer(s, 4, RTM_NEWLINK, NULL, NULL, NULL) == -ENODEV, "an unknown link: NLMSG_ERROR ENODEV");
    int addrs = 0;
    struct ifaddrmsg ia = { .ifa_family = AF_INET };
    nl_request(s, RTM_GETADDR, NLM_F_DUMP, &ia, sizeof ia, 5);
    CHECK(nl_answer(s, 5, RTM_NEWADDR, NULL, &addrs, NULL) == 1 && addrs >= 1, "RTM_GETADDR dump: IPv4 addresses");
    struct { struct ifinfomsg i; struct rtattr a; unsigned mtu; } set = {
        { .ifi_family = AF_UNSPEC, .ifi_index = 1 }, { RTA_LENGTH(4), IFLA_MTU }, 16436 };
    nl_request(s, RTM_SETLINK, NLM_F_ACK, &set, sizeof set, 6);
    CHECK(nl_answer(s, 6, 0, NULL, NULL, NULL) == 0, "RTM_SETLINK with NLM_F_ACK: acknowledged");
    int news = 0;
    CHECK(nl_answer(mon, 0, RTM_NEWLINK, NULL, &news, NULL) == 1 && news == 1, "RTMGRP_LINK listener hears the change");
    struct { struct rtmsg r; struct rtattr a; unsigned oif; } rt = {
        { .rtm_family = AF_INET, .rtm_dst_len = 24, .rtm_table = RT_TABLE_MAIN, .rtm_type = RTN_UNICAST }, { RTA_LENGTH(4), RTA_OIF }, 4242 };
    nl_request(s, RTM_NEWROUTE, NLM_F_CREATE | NLM_F_ACK, &rt, sizeof rt, 7);
    CHECK(nl_answer(s, 7, 0, NULL, NULL, NULL) == -ENODEV, "RTM_NEWROUTE through an unknown link: ENODEV");
    close(s);
    close(mon);

    /* packet sockets and their filters */
    int p = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    CHECK(p >= 0, "socket(AF_PACKET, SOCK_RAW, ETH_P_ALL)");
    struct sock_filter bad[] = { { BPF_JMP | BPF_JA, 0, 0, 5 }, { BPF_RET | BPF_K, 0, 0, 0 } };
    struct sock_fprog fb = { 2, bad };
    CHECK(setsockopt(p, SOL_SOCKET, SO_ATTACH_FILTER, &fb, sizeof fb) < 0 && errno == EINVAL, "a BPF jump out of the program: EINVAL");
    struct sock_filter none[] = { { BPF_RET | BPF_K, 0, 0, 0 } };
    struct sock_fprog fn = { 1, none };
    CHECK(setsockopt(p, SOL_SOCKET, SO_ATTACH_FILTER, &fn, sizeof fn) == 0, "SO_ATTACH_FILTER");
    CHECK(setsockopt(p, SOL_SOCKET, SO_DETACH_FILTER, NULL, 0) == 0, "SO_DETACH_FILTER");
    close(p);
}

/* On eth0, while pinging the gateway: an ICMP-only filter sees the reply,
 * ETH_P_ALL sees our request going out */
static void test_packet_remote(const char *gw)
{
    int ifindex = (int)if_nametoindex("eth0");
    int ip = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP)), all = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    struct sockaddr_ll a = { .sll_family = AF_PACKET, .sll_protocol = htons(ETH_P_IP), .sll_ifindex = ifindex };
    CHECK(ip >= 0 && bind(ip, (struct sockaddr *)&a, sizeof a) == 0, "packet socket bound to eth0, ETH_P_IP");
    struct sock_filter icmp[] = {
        { BPF_LD | BPF_B | BPF_ABS, 0, 0, 9 },          /* the IP protocol */
        { BPF_JMP | BPF_JEQ | BPF_K, 0, 1, IPPROTO_ICMP },
        { BPF_RET | BPF_K, 0, 0, 0xffff },
        { BPF_RET | BPF_K, 0, 0, 0 },
    };
    struct sock_fprog f = { 4, icmp };
    CHECK(setsockopt(ip, SOL_SOCKET, SO_ATTACH_FILTER, &f, sizeof f) == 0, "an ICMP-only BPF filter");
    a.sll_protocol = htons(ETH_P_ALL);
    bind(all, (struct sockaddr *)&a, sizeof a);
    test_ping(gw);
    unsigned char b[2048];
    struct sockaddr_ll from;
    socklen_t fl = sizeof from;
    int reply = 0, others = 0;
    ssize_t n;
    while ((n = recvfrom(ip, b, sizeof b, MSG_DONTWAIT, (struct sockaddr *)&from, &fl)) > 0) {
        if (n >= 28 && b[9] == IPPROTO_ICMP && b[20] == 0 && from.sll_pkttype == PACKET_HOST && from.sll_ifindex == ifindex) reply++;
        else others++;
    }
    CHECK(reply >= 1 && !others, "SOCK_DGRAM: the echo reply, IP header first, and nothing else");
    int out = 0;
    while ((n = recvfrom(all, b, sizeof b, MSG_DONTWAIT, (struct sockaddr *)&from, &fl)) > 0)
        if (n >= 42 && from.sll_pkttype == PACKET_OUTGOING && b[12] == 8 && b[13] == 0 && b[23] == IPPROTO_ICMP && b[34] == 8) out++;
    CHECK(out >= 1, "ETH_P_ALL: our echo request going out, with its Ethernet header");
    close(ip);
    close(all);
}

static void test_udp(void)
{
    int a = socket(AF_INET, SOCK_DGRAM, 0), b = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in la = sin_of("127.0.0.1", 0);
    CHECK(bind(a, (struct sockaddr *)&la, sizeof la) == 0, "UDP bind 127.0.0.1:0");
    int pa = port_of(a);
    CHECK(pa >= 32768 && pa <= 60999, "UDP: ephemeral port from the Linux range");
    struct sockaddr_in to = sin_of("127.0.0.1", pa);
    CHECK(sendto(b, "hello", 5, 0, (struct sockaddr *)&to, sizeof to) == 5, "UDP sendto (autobinds)");
    struct pollfd pf = { a, POLLIN, 0 };
    CHECK(poll(&pf, 1, 2000) == 1 && (pf.revents & POLLIN), "UDP poll: POLLIN");
    int avail = -1;
    CHECK(ioctl(a, FIONREAD, &avail) == 0 && avail == 5, "UDP FIONREAD: next datagram size");
    char buf[64];
    struct sockaddr_in from;
    socklen_t fl = sizeof from;
    ssize_t n = recvfrom(a, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
    CHECK(n == 5 && !memcmp(buf, "hello", 5), "UDP recvfrom: the data");
    CHECK(fl == sizeof from && from.sin_port == htons((uint16_t)port_of(b)) && from.sin_addr.s_addr == htonl(INADDR_LOOPBACK),
          "UDP recvfrom: the sender's address");
    /* truncation */
    sendto(b, "0123456789", 10, 0, (struct sockaddr *)&to, sizeof to);
    struct iovec iov = { buf, 4 };
    struct msghdr m = { .msg_iov = &iov, .msg_iovlen = 1 };
    n = recvmsg(a, &m, 0);
    CHECK(n == 4 && (m.msg_flags & MSG_TRUNC), "UDP recvmsg: short buffer sets MSG_TRUNC");
    /* boundaries kept */
    sendto(b, "one", 3, 0, (struct sockaddr *)&to, sizeof to);
    sendto(b, "three", 5, 0, (struct sockaddr *)&to, sizeof to);
    n = recv(a, buf, sizeof buf, 0);
    ssize_t n2 = recv(a, buf + 10, sizeof buf - 10, 0);
    CHECK(n == 3 && n2 == 5, "UDP keeps datagram boundaries");
    /* nonblocking */
    CHECK(recv(a, buf, sizeof buf, MSG_DONTWAIT) < 0 && errno == EAGAIN, "UDP MSG_DONTWAIT on empty: EAGAIN");
    /* a big datagram: fragmented on lo (MTU 16436) and reassembled */
    static unsigned char big[40000], big2[40000];
    fill(big, sizeof big, 7);
    CHECK(sendto(b, big, sizeof big, 0, (struct sockaddr *)&to, sizeof to) == (ssize_t)sizeof big, "UDP send 40000 bytes");
    n = recv(a, big2, sizeof big2, 0);
    CHECK(n == (ssize_t)sizeof big && !memcmp(big, big2, sizeof big), "UDP 40000-byte datagram reassembled intact");
    /* connected UDP and ICMP port unreachable */
    int c = socket(AF_INET, SOCK_DGRAM, 0);
    int dead = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in dl = sin_of("127.0.0.1", 0);
    bind(dead, (struct sockaddr *)&dl, sizeof dl);
    int dport = port_of(dead);
    close(dead);
    struct sockaddr_in dto = sin_of("127.0.0.1", dport);
    CHECK(connect(c, (struct sockaddr *)&dto, sizeof dto) == 0, "UDP connect");
    send(c, "x", 1, 0);
    struct timeval tv = { 2, 0 };
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    CHECK(recv(c, buf, sizeof buf, 0) < 0 && errno == ECONNREFUSED, "UDP to a closed port: ECONNREFUSED");
    /* SO_RCVTIMEO */
    tv.tv_sec = 0; tv.tv_usec = 200000;
    setsockopt(a, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    double t0 = now();
    int r = (int)recv(a, buf, sizeof buf, 0);
    double dt = now() - t0;
    CHECK(r < 0 && errno == EAGAIN && dt > 0.15 && dt < 1.0, "SO_RCVTIMEO: EAGAIN after 200 ms");
    /* broadcast needs SO_BROADCAST */
    struct sockaddr_in bc = sin_of("255.255.255.255", 9);
    CHECK(sendto(b, "x", 1, 0, (struct sockaddr *)&bc, sizeof bc) < 0 && errno == EACCES, "UDP broadcast without SO_BROADCAST: EACCES");
    /* EADDRINUSE */
    int d = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in same = sin_of("127.0.0.1", pa);
    CHECK(bind(d, (struct sockaddr *)&same, sizeof same) < 0 && errno == EADDRINUSE, "UDP bind to a used port: EADDRINUSE");
    close(a); close(b); close(c); close(d);
}

/* ---- TCP --------------------------------------------------------------------------- */
#define BULK (8u << 20)

static void test_tcp_basic(void)
{
    int port = 0;
    int l = tcp_listener("127.0.0.1", &port);
    CHECK(l >= 0 && port > 0, "TCP listen on 127.0.0.1:0");
    int on = 0;
    socklen_t ol = sizeof on;
    CHECK(getsockopt(l, SOL_SOCKET, SO_ACCEPTCONN, &on, &ol) == 0 && on == 1, "SO_ACCEPTCONN on a listener");
    fcntl(l, F_SETFL, O_NONBLOCK);
    CHECK(accept(l, NULL, NULL) < 0 && errno == EAGAIN, "non-blocking accept with nothing queued: EAGAIN");
    fcntl(l, F_SETFL, 0);
    int c = tcp_connect_to("127.0.0.1", port);
    CHECK(c >= 0, "TCP connect to the listener");
    struct sockaddr_in pa;
    socklen_t pl = sizeof pa;
    int s = accept(l, (struct sockaddr *)&pa, &pl);
    CHECK(s >= 0 && pa.sin_port == htons((uint16_t)port_of(c)), "accept: the peer is our client");
    struct sockaddr_in cp;
    socklen_t cl = sizeof cp;
    CHECK(getpeername(c, (struct sockaddr *)&cp, &cl) == 0 && ntohs(cp.sin_port) == port, "getpeername on the client");
    CHECK(write(c, "ping", 4) == 4, "TCP write");
    char buf[64];
    CHECK(read(s, buf, sizeof buf) == 4 && !memcmp(buf, "ping", 4), "TCP read: the bytes");
    CHECK(write(s, "pong!", 5) == 5 && read(c, buf, sizeof buf) == 5, "TCP: the other way");
    /* half close */
    CHECK(shutdown(c, SHUT_WR) == 0, "shutdown(SHUT_WR)");
    CHECK(read(s, buf, sizeof buf) == 0, "the peer reads EOF");
    CHECK(write(s, "after", 5) == 5 && read(c, buf, sizeof buf) == 5, "the peer can still send after our FIN");
    struct pollfd pf = { s, POLLIN | POLLRDHUP, 0 };
    CHECK(poll(&pf, 1, 0) == 1 && (pf.revents & POLLRDHUP), "poll: POLLRDHUP after the peer's FIN");
    close(s);
    CHECK(read(c, buf, sizeof buf) == 0, "EOF after the peer closes");
    close(c);
    /* refused */
    int dead_port = 0;
    int d = tcp_listener("127.0.0.1", &dead_port);
    close(d);
    CHECK(tcp_connect_to("127.0.0.1", dead_port) < 0 && errno == ECONNREFUSED, "connect to a closed port: ECONNREFUSED");
    /* port in use */
    int l2 = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = sin_of("127.0.0.1", port);
    CHECK(bind(l2, (struct sockaddr *)&a, sizeof a) < 0 && errno == EADDRINUSE, "bind to a listening port: EADDRINUSE");
    close(l2);
    /* non-blocking connect */
    int nb = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    a = sin_of("127.0.0.1", port);
    int rc = connect(nb, (struct sockaddr *)&a, sizeof a);
    CHECK(rc == 0 || errno == EINPROGRESS, "non-blocking connect: EINPROGRESS (or done)");
    pf = (struct pollfd){ nb, POLLOUT, 0 };
    CHECK(poll(&pf, 1, 2000) == 1 && (pf.revents & POLLOUT), "poll: POLLOUT once connected");
    int err = -1;
    socklen_t el = sizeof err;
    CHECK(getsockopt(nb, SOL_SOCKET, SO_ERROR, &err, &el) == 0 && err == 0, "SO_ERROR is 0");
    s = accept(l, NULL, NULL);
    /* unread data on close: RST */
    write(nb, "unread", 6);
    usleep(50000);
    close(s);
    usleep(50000);
    CHECK(read(nb, buf, sizeof buf) < 0 && errno == ECONNRESET, "close with unread data resets: ECONNRESET");
    close(nb);
    /* TCP_NODELAY round trip */
    c = tcp_connect_to("127.0.0.1", port);
    s = accept(l, NULL, NULL);
    int one = 1;
    CHECK(setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one) == 0, "TCP_NODELAY");
    double t0 = now();
    for (int i = 0; i < 200; i++) { write(c, "q", 1); read(s, buf, 1); write(s, "r", 1); read(c, buf, 1); }
    double dt = now() - t0;
    printf("nettest:        200 one-byte round trips in %.1f ms\n", dt * 1000);
    CHECK(dt < 2.0, "small request/response is not stalled by delayed ACKs");
    /* epoll */
    int ep = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = s };
    epoll_ctl(ep, EPOLL_CTL_ADD, s, &ev);
    write(c, "e", 1);
    struct epoll_event out;
    CHECK(epoll_wait(ep, &out, 1, 2000) == 1 && out.data.fd == s, "epoll sees TCP data");
    close(ep); close(c); close(s);
    /* /proc/net/tcp shows the listener (state 0A) */
    int fd = open("/proc/net/tcp", O_RDONLY);
    char pbuf[4096];
    ssize_t n = fd >= 0 ? read(fd, pbuf, sizeof pbuf - 1) : -1;
    if (n > 0) pbuf[n] = 0;
    char want[32];
    snprintf(want, sizeof want, ":%04X 00000000:0000 0A", port);
    CHECK(n > 0 && strstr(pbuf, want), "/proc/net/tcp lists the listener");
    if (fd >= 0) close(fd);
    close(l);
}

/* Bulk transfer through an echo: the child sends BULK bytes and reads
 * them back while the parent echoes. Proves flow control both ways. */
static double bulk_echo(const char *ip, int port, int listener)
{
    static unsigned char out[BULK], in[BULK];
    fill(out, BULK, 12345);
    pid_t pid = -1;
    if (listener >= 0) {
        pid = fork();
        if (pid == 0) {
            int s = accept(listener, NULL, NULL);
            static unsigned char eb[65536];
            ssize_t n;
            while ((n = read(s, eb, sizeof eb)) > 0) if (write_all(s, eb, (size_t)n) < 0) break;
            close(s);
            _exit(0);
        }
    }
    int c = tcp_connect_to(ip, port);
    if (c < 0) { if (pid > 0) { kill(pid, SIGKILL); waitpid(pid, NULL, 0); } return -1; }
    double t0 = now();
    pid_t w = fork();
    if (w == 0) { _exit(write_all(c, out, BULK) == 0 ? 0 : 1); }
    ssize_t got = read_all(c, in, BULK);
    int st = 0;
    waitpid(w, &st, 0);
    double dt = now() - t0;
    close(c);
    if (pid > 0) waitpid(pid, NULL, 0);
    if (got != (ssize_t)BULK || memcmp(in, out, BULK) || !WIFEXITED(st) || WEXITSTATUS(st)) return -1;
    return dt;
}

static void test_tcp_bulk(void)
{
    int port = 0;
    int l = tcp_listener("127.0.0.1", &port);
    double dt = bulk_echo("127.0.0.1", port, l);
    CHECK(dt > 0, "TCP loopback: 8 MiB echoed back intact");
    if (dt > 0) printf("nettest:        loopback echo: %.1f MiB/s each way\n", 8.0 / dt);
    close(l);
}

/* ---- through the network card ------------------------------------------------------ */
static int wait_for_dhcp(char *addr, size_t cap)
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct ifreq r;
    for (int i = 0; i < 150; i++) {
        memset(&r, 0, sizeof r);
        strcpy(r.ifr_name, "eth0");
        if (ioctl(s, SIOCGIFADDR, &r) == 0) {
            inet_ntop(AF_INET, &((struct sockaddr_in *)&r.ifr_addr)->sin_addr, addr, (socklen_t)cap);
            close(s);
            return 0;
        }
        usleep(100000);
    }
    close(s);
    return -1;
}

static uint16_t icmp_csum(const void *d, size_t n)
{
    const uint16_t *w = d;
    uint32_t s = 0;
    for (; n > 1; n -= 2) s += *w++;
    if (n) s += *(const uint8_t *)w;
    while (s >> 16) s = (s & 0xffff) + (s >> 16);
    return (uint16_t)~s;
}

static void test_ping(const char *gw)
{
    int s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    CHECK(s >= 0, "raw ICMP socket");
    struct sockaddr_in to = sin_of(gw, 0);
    unsigned char pkt[64];
    memset(pkt, 0, sizeof pkt);
    struct icmphdr *ic = (struct icmphdr *)pkt;
    ic->type = ICMP_ECHO;
    ic->un.echo.id = htons(0x4b53);
    ic->un.echo.sequence = htons(1);
    memcpy(pkt + 8, "kestrel ping", 12);
    ic->checksum = icmp_csum(pkt, sizeof pkt);
    double t0 = now();
    CHECK(sendto(s, pkt, sizeof pkt, 0, (struct sockaddr *)&to, sizeof to) == (ssize_t)sizeof pkt, "send an ICMP echo request");
    struct timeval tv = { 3, 0 };
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    unsigned char in[256];
    int ok = 0;
    for (int i = 0; i < 5 && !ok; i++) {
        ssize_t n = recv(s, in, sizeof in, 0);
        if (n < 28) break;
        int hl = (in[0] & 15) * 4;
        struct icmphdr *r = (struct icmphdr *)(in + hl);
        ok = r->type == ICMP_ECHOREPLY && r->un.echo.id == htons(0x4b53) && !memcmp(in + hl + 8, "kestrel ping", 12);
    }
    CHECK(ok, "echo reply from the gateway");
    if (ok) printf("nettest:        ping %s: %.2f ms\n", gw, (now() - t0) * 1000);
    close(s);
}

static void test_remote(const char *hostport)
{
    char host[64];
    snprintf(host, sizeof host, "%s", hostport);
    char *colon = strchr(host, ':');
    int port = colon ? atoi(colon + 1) : 7;
    if (colon) *colon = 0;
    char addr[32] = "";
    CHECK(wait_for_dhcp(addr, sizeof addr) == 0, "eth0 has an address (DHCP)");
    printf("nettest:        eth0 is %s\n", addr);
    char rbuf[512];
    int fd = open("/proc/net/route", O_RDONLY);
    ssize_t n = fd >= 0 ? read(fd, rbuf, sizeof rbuf - 1) : -1;
    if (n > 0) rbuf[n] = 0;
    int dflt = 0;                                       /* a line "ethN<TAB>00000000<TAB>..." */
    for (char *l = n > 0 ? rbuf : NULL; l && *l; l = strchr(l, '\n') ? strchr(l, '\n') + 1 : NULL)
        if (!strncmp(l, "eth", 3) && strstr(l, "\t00000000\t") == strchr(l, '\t')) dflt = 1;
    CHECK(dflt, "/proc/net/route has a default route");
    if (fd >= 0) close(fd);
    test_ping(host);
    if (!getuid()) test_packet_remote(host);
    double dt = bulk_echo(host, port, -1);
    CHECK(dt > 0, "TCP through the network card: 8 MiB echoed by the host intact");
    if (dt > 0) printf("nettest:        network echo: %.1f MiB/s each way\n", 8.0 / dt);
    /* UDP echo on the same port number */
    int u = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in to = sin_of(host, port);
    struct timeval tv = { 2, 0 };
    setsockopt(u, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    int good = 0;
    for (int i = 0; i < 20; i++) {
        char msg[32], back[64];
        int len = snprintf(msg, sizeof msg, "datagram %d", i);
        sendto(u, msg, (size_t)len, 0, (struct sockaddr *)&to, sizeof to);
        ssize_t r = recv(u, back, sizeof back, 0);
        if (r == len && !memcmp(back, msg, (size_t)len)) good++;
    }
    CHECK(good >= 18, "UDP through the network card: datagrams echoed by the host");
    printf("nettest:        %d of 20 datagrams came back\n", good);
    close(u);
    /* Name servers: /etc/resolv.conf links to /proc/net/pnp, filled by DHCP */
    fd = open("/etc/resolv.conf", O_RDONLY);
    n = fd >= 0 ? read(fd, rbuf, sizeof rbuf - 1) : -1;
    if (n > 0) rbuf[n] = 0;
    CHECK(n > 0 && strstr(rbuf, "nameserver "), "/etc/resolv.conf names the DHCP name server");
    if (fd >= 0) close(fd);
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM }, *res = NULL;
    int g = getaddrinfo("example.com", "80", &hints, &res);
    if (!g) {
        char ip[32];
        inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ip, sizeof ip);
        printf("nettest:        example.com is %s (DNS works)\n", ip);
        freeaddrinfo(res);
    } else printf("nettest:        example.com did not resolve: %s (the host may have no DNS)\n", gai_strerror(g));
}

static int serve(int port)
{
    int l = tcp_listener("0.0.0.0", &port);
    if (l < 0) { printf("nettest: serve: cannot listen on %d\n", port); return 1; }
    printf("nettest: serving on port %d\n", port);
    fflush(stdout);
    int s = accept(l, NULL, NULL);
    static unsigned char b[65536];
    uint64_t total = 0;
    uint32_t h = 2166136261u;
    ssize_t n;
    while ((n = read(s, b, sizeof b)) > 0) { h = fnv(b, (size_t)n, h); total += (uint64_t)n; }
    char ans[64];
    int len = snprintf(ans, sizeof ans, "%llu %08x\n", (unsigned long long)total, h);
    write_all(s, ans, (size_t)len);
    close(s);
    close(l);
    printf("nettest: served %llu bytes, fnv %08x\n", (unsigned long long)total, h);
    return 0;
}

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc == 3 && !strcmp(argv[1], "serve")) return serve(atoi(argv[2]));
    test_interfaces();
    test_netlink();
    test_udp();
    test_tcp_basic();
    test_tcp_bulk();
    if (argc > 1) test_remote(argv[1]);
    printf(failures ? "nettest: FAILED (%d)\n" : "nettest: all checks passed\n", failures);
    return failures ? 1 : 0;
}
