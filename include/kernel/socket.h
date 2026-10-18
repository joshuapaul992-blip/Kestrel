/* include/kernel/socket.h -- AF_UNIX stream sockets (fs/unixsock.c)
 *
 * What X11 needs locally: SOCK_STREAM sockets named either by a path in the
 * file system ("/tmp/.X11-unix/X0", a VSOCK node) or in Linux's abstract
 * namespace (sun_path[0] == 0, "@/tmp/.X11-unix/X0", which xtrans tries
 * first). connect() completes at once by queueing a server-side socket on
 * the listener; accept() hands it out. Each direction has a 64 KiB buffer,
 * filled straight from the sender's memory and drained straight into the
 * receiver's (one copy each way, see struct sock_io); blocking,
 * O_NONBLOCK / MSG_DONTWAIT, EOF and poll() follow POSIX/Linux.
 * Peer credentials (SO_PEERCRED) are the creating process' pid/uid/gid.
 * SOCK_SEQPACKET (socket, socketpair) and SOCK_DGRAM (socketpair) keep
 * message boundaries; any type passes descriptors (SCM_RIGHTS).
 * Not yet: unconnected datagram sockets (sendto/recvfrom with addresses). */
#ifndef KESTREL_SOCKET_H
#define KESTREL_SOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define AF_UNIX         1
#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_SEQPACKET  5
#define SOCK_NONBLOCK   04000
#define SOCK_CLOEXEC    02000000
#define SOL_SOCKET      1
#define SO_TYPE         3
#define SO_ERROR        4
#define SO_SNDBUF       7
#define SO_RCVBUF       8
#define SO_PEERCRED     17
#define MSG_PEEK        0x2
#define MSG_CTRUNC      0x8
#define MSG_TRUNC       0x20
#define MSG_WAITALL     0x100
#define MSG_CMSG_CLOEXEC 0x40000000
#define SCM_RIGHTS      1
#define SCM_CREDENTIALS 2
#define SCM_MAX_FD      253
#define MSG_DONTWAIT    0x40
#define MSG_NOSIGNAL    0x4000
#define SHUT_RD         0
#define SHUT_WR         1
#define SHUT_RDWR       2
#define UNIX_PATH_MAX   108

struct sockaddr_un { uint16_t sun_family; char sun_path[UNIX_PATH_MAX]; };
struct ucred { int32_t pid; uint32_t uid, gid; };

struct file;

/* User memory (iovecs) */
struct iovec { uint64_t base, len; };

/* Where socket data comes from or goes to: the user's iovecs (v != NULL,
 * already checked with ubuf_ok), or one kernel buffer. A cursor: each copy
 * continues where the last one stopped. */
struct sock_io {
    const struct iovec *v;
    uint64_t n, i, off;                 /* iovecs, the current one, offset in it */
    uint8_t *kbuf;                      /* v == NULL: kernel memory at kbuf + off */
};
#define SOCK_IO_USER(vec, cnt)  ((struct sock_io){ .v = (vec), .n = (cnt) })
#define SOCK_IO_KERNEL(buf)     ((struct sock_io){ .kbuf = (uint8_t *)(buf) })

/* Copy len bytes between k and the cursor: out = from k to io. Called
 * without locks held, as user memory may fault. -EFAULT on a bad pointer. */
int   sock_io_copy(struct sock_io *io, void *k, size_t len, bool out);

bool  usock_is(struct file *f);
int   usock_create(int domain, int type, int protocol, struct file **out);
int   usock_pair(int type, struct file **a, struct file **b);
int   usock_bind(struct file *f, const struct sockaddr_un *a, uint32_t len);
int   usock_listen(struct file *f, int backlog);
int   usock_connect(struct file *f, const struct sockaddr_un *a, uint32_t len);
int   usock_accept(struct file *f, int flags, struct file **out, struct sockaddr_un *peer, uint32_t *len);
long  usock_send(struct file *f, const void *buf, size_t len, int flags);
long  usock_recv(struct file *f, void *buf, size_t len, int flags);
/* With descriptors (SCM_RIGHTS): see fs/unixsock.c */
long  usock_sendmsg(struct file *f, struct sock_io *io, size_t len, int flags, struct file **files, int nfiles);
long  usock_recvmsg(struct file *f, struct sock_io *io, size_t len, int flags, struct file **files, int *nfiles,
                    int maxfiles, int *msg_flags);
int   usock_shutdown(struct file *f, int how);
int   usock_name(struct file *f, bool peer, struct sockaddr_un *a, uint32_t *len);
int   usock_getsockopt(struct file *f, int level, int opt, void *val, uint32_t *len);

#endif
