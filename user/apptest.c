/* user/apptest.c -- what AppImages and newer glibc programs ask the kernel
 * for: Linux's statfs magic numbers, the prctl options Bionic and glibc
 * use, the newer system calls (preadv2, copy_file_range, fchmodat2,
 * execveat, rt_sigtimedwait, sendmmsg, epoll_pwait2, sched_getattr),
 * mount(2) and umount2(2), and a FUSE file system served from this
 * program itself.
 *
 *   apptest            run the checks (needs a writable /tmp)
 * Exit status 0 = every check passed. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <dirent.h>

#ifndef SYS_fchmodat2
#define SYS_fchmodat2 452
#endif
#ifndef SYS_epoll_pwait2
#define SYS_epoll_pwait2 441
#endif

static int failures;
#define CHECK(c, what) do { int ok_ = (c); printf("  %s  %s\n", ok_ ? "ok  " : "FAIL", what); if (!ok_) failures++; } while (0)

static long fstype(const char *path)
{
    struct statfs s;
    return statfs(path, &s) == 0 ? (long)s.f_type : -1;
}

static void check_statfs(void)
{
    printf("statfs:\n");
    CHECK(fstype("/proc") == 0x9fa0, "/proc is proc (PROC_SUPER_MAGIC)");
    CHECK(fstype("/proc/self/status") == 0x9fa0, "/proc/self/status too");
    CHECK(fstype("/sys") == 0x62656572, "/sys is sysfs");
    CHECK(fstype("/dev") == 0x01021994, "/dev is devtmpfs (TMPFS_MAGIC)");
    CHECK(fstype("/dev/shm") == 0x01021994, "/dev/shm is tmpfs");
    char line[512];
    int proc = 0, sys = 0;
    FILE *f = fopen("/proc/self/mounts", "r");
    while (f && fgets(line, sizeof line, f)) {
        if (!strncmp(line, "proc /proc proc ", 16)) proc = 1;
        if (!strncmp(line, "sysfs /sys sysfs ", 17)) sys = 1;
    }
    if (f) fclose(f);
    CHECK(proc && sys, "/proc/self/mounts names proc and sysfs as Linux does");
    int fields = 0, ok = 1;
    f = fopen("/proc/self/mountinfo", "r");
    while (f && fgets(line, sizeof line, f)) {
        int id, parent; unsigned ma, mi; char root[256], mp[256];
        if (sscanf(line, "%d %d %u:%u %255s %255s", &id, &parent, &ma, &mi, root, mp) == 6) fields++;
        else ok = 0;
        if (!strstr(line, " - ")) ok = 0;
    }
    if (f) fclose(f);
    CHECK(ok && fields >= 3, "/proc/self/mountinfo lines parse");
}

static void check_prctl(void)
{
    printf("prctl:\n");
    void *p = mmap(NULL, 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(prctl(0x53564d41, 0, (unsigned long)p, 8192, (unsigned long)"bionic heap") == 0, "PR_SET_VMA_ANON_NAME");
    CHECK(prctl(0x53564d41, 0, (unsigned long)p, 8192, (unsigned long)"bad[name]") < 0 && errno == EINVAL,
          "PR_SET_VMA_ANON_NAME refuses [ and ]");
    CHECK(prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0) == 0 && prctl(PR_GET_THP_DISABLE, 0, 0, 0, 0) == 1,
          "PR_SET_THP_DISABLE, then PR_GET_THP_DISABLE says 1");
    prctl(PR_SET_THP_DISABLE, 0, 0, 0, 0);
    int sig = -1;
    CHECK(prctl(PR_GET_PDEATHSIG, &sig, 0, 0, 0) == 0 && sig == 0, "PR_GET_PDEATHSIG");
    CHECK(prctl(PR_GET_TIMERSLACK, 0, 0, 0, 0) > 0, "PR_GET_TIMERSLACK");
    CHECK(prctl(PR_GET_SECCOMP, 0, 0, 0, 0) == 0, "PR_GET_SECCOMP: none");
    munmap(p, 8192);
}

static void check_io(void)
{
    printf("file I/O:\n");
    const char *a = "/tmp/apptest-a", *b = "/tmp/apptest-b";
    int fd = open(a, O_RDWR | O_CREAT | O_TRUNC, 0644);
    char x[] = "hello ", y[] = "world";
    struct iovec w[2] = { { x, 6 }, { y, 5 } };
    CHECK(pwritev(fd, w, 2, 100) == 11, "pwritev at offset 100");
    char r1[6] = { 0 }, r2[6] = { 0 };
    struct iovec r[2] = { { r1, 6 }, { r2, 5 } };
    CHECK(preadv(fd, r, 2, 100) == 11 && !memcmp(r1, "hello ", 6) && !memcmp(r2, "world", 5), "preadv reads it back");
    CHECK(lseek(fd, 0, SEEK_CUR) == 0, "neither moved the file position");
    memset(r1, 0, sizeof r1);
    lseek(fd, 100, SEEK_SET);
    CHECK(syscall(SYS_preadv2, fd, r, 1, -1L, 0L, 0) == 6 && lseek(fd, 0, SEEK_CUR) == 106,
          "preadv2 at offset -1 reads at the position and moves it");
    int out = open(b, O_RDWR | O_CREAT | O_TRUNC, 0644);
    loff_t in_off = 100;
    CHECK(syscall(SYS_copy_file_range, fd, &in_off, out, NULL, 11L, 0) == 11 && in_off == 111, "copy_file_range");
    char got[12] = { 0 };
    CHECK(pread(out, got, 11, 0) == 11 && !strcmp(got, "hello world"), "the copy holds the bytes");
    close(out);
    close(fd);
    unlink(b);
    CHECK(symlink(a, b) == 0, "symlink");
    errno = 0;
    CHECK(syscall(SYS_fchmodat2, AT_FDCWD, b, 0600, AT_SYMLINK_NOFOLLOW) < 0 && errno == EOPNOTSUPP,
          "fchmodat2 refuses a symlink's own mode");
    struct stat st;
    CHECK(syscall(SYS_fchmodat2, AT_FDCWD, b, 0600, 0) == 0 && stat(a, &st) == 0 && (st.st_mode & 0777) == 0600,
          "fchmodat2 through the link");
    unlink(b);
    unlink(a);
}

static void check_exec(const char *self)
{
    printf("execveat:\n");
    int dir = open("/bin", O_RDONLY | O_DIRECTORY);
    pid_t p = fork();
    if (!p) {
        char *av[] = { "true", NULL }, *ev[] = { NULL };
        syscall(SYS_execveat, dir, "true", av, ev, 0);
        _exit(42);
    }
    int st = 0;
    waitpid(p, &st, 0);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0, "execveat(/bin, \"true\")");
    int fd = open(self, O_RDONLY);
    p = fork();
    if (!p) {
        char *av[] = { "apptest", "--exit7", NULL }, *ev[] = { NULL };
        syscall(SYS_execveat, fd, "", av, ev, AT_EMPTY_PATH);
        _exit(42);
    }
    waitpid(p, &st, 0);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 7, "execveat(fd, \"\", AT_EMPTY_PATH): fexecve");
    close(fd);
    close(dir);
}

static void check_signals(void)
{
    printf("rt_sigtimedwait:\n");
    sigset_t s;
    sigemptyset(&s);
    sigaddset(&s, SIGUSR1);
    sigprocmask(SIG_BLOCK, &s, NULL);
    struct timespec zero = { 0, 0 }, ms50 = { 0, 50000000 };
    siginfo_t si;
    errno = 0;
    CHECK(sigtimedwait(&s, &si, &zero) < 0 && errno == EAGAIN, "nothing pending: EAGAIN at once");
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int r = sigtimedwait(&s, &si, &ms50);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
    CHECK(r < 0 && errno == EAGAIN && ms >= 45, "a 50 ms timeout waits");
    kill(getpid(), SIGUSR1);
    r = sigtimedwait(&s, &si, &zero);
    CHECK(r == SIGUSR1 && si.si_signo == SIGUSR1 && si.si_pid == getpid() && si.si_code == SI_USER,
          "a pending SIGUSR1, with its siginfo");
    pid_t p = fork();
    if (!p) { usleep(30000); kill(getppid(), SIGUSR1); _exit(0); }
    r = sigwaitinfo(&s, &si);
    CHECK(r == SIGUSR1 && si.si_pid == p, "sigwaitinfo waits for a child's kill");
    waitpid(p, NULL, 0);
    sigprocmask(SIG_UNBLOCK, &s, NULL);
}

static void check_net(void)
{
    printf("sendmmsg, recvmmsg, epoll_pwait2:\n");
    int sv[2];
    socketpair(AF_UNIX, SOCK_DGRAM, 0, sv);
    char a[] = "one", b[] = "two!";
    struct iovec ia = { a, 3 }, ib = { b, 4 };
    struct mmsghdr m[2];
    memset(m, 0, sizeof m);
    m[0].msg_hdr.msg_iov = &ia; m[0].msg_hdr.msg_iovlen = 1;
    m[1].msg_hdr.msg_iov = &ib; m[1].msg_hdr.msg_iovlen = 1;
    CHECK(sendmmsg(sv[0], m, 2, 0) == 2 && m[0].msg_len == 3 && m[1].msg_len == 4, "sendmmsg: two datagrams");
    char ra[8] = { 0 }, rb[8] = { 0 };
    struct iovec ja = { ra, 8 }, jb = { rb, 8 };
    struct mmsghdr n[3];
    char rc[8];
    struct iovec jc = { rc, 8 };
    memset(n, 0, sizeof n);
    n[0].msg_hdr.msg_iov = &ja; n[0].msg_hdr.msg_iovlen = 1;
    n[1].msg_hdr.msg_iov = &jb; n[1].msg_hdr.msg_iovlen = 1;
    n[2].msg_hdr.msg_iov = &jc; n[2].msg_hdr.msg_iovlen = 1;
    CHECK(recvmmsg(sv[1], n, 3, MSG_WAITFORONE, NULL) == 2 && n[0].msg_len == 3 && n[1].msg_len == 4 &&
          !strcmp(ra, "one") && !strcmp(rb, "two!"), "recvmmsg with MSG_WAITFORONE: both, then stops");
    int ep = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = sv[1] }, out;
    epoll_ctl(ep, EPOLL_CTL_ADD, sv[1], &ev);
    struct timespec t = { 0, 20000000 };
    CHECK(syscall(SYS_epoll_pwait2, ep, &out, 1, &t, NULL, 8) == 0, "epoll_pwait2: nothing within 20 ms");
    write(sv[0], "x", 1);
    CHECK(syscall(SYS_epoll_pwait2, ep, &out, 1, &t, NULL, 8) == 1 && out.data.fd == sv[1], "epoll_pwait2: ready");
    close(ep); close(sv[0]); close(sv[1]);
}

static void check_sched(void)
{
    printf("sched_getattr:\n");
    struct { uint32_t size, policy; uint64_t flags; int32_t nice; uint32_t prio; uint64_t rt, dl, per; uint32_t umin, umax; } a;
    memset(&a, 0xff, sizeof a);
    CHECK(syscall(SYS_sched_getattr, 0, &a, sizeof a, 0) == 0 && a.size == sizeof a && a.policy == 0 && a.prio == 0,
          "SCHED_OTHER, priority 0");
    a.policy = 0; a.flags = 0; a.nice = 0; a.size = 48;
    CHECK(syscall(SYS_sched_setattr, 0, &a, 0) == 0, "sched_setattr: SCHED_OTHER");
    a.policy = 1;
    CHECK(syscall(SYS_sched_setattr, 0, &a, 0) < 0 && errno == EPERM, "SCHED_FIFO is refused");
}

/* ---- mount(2), umount2(2) and FUSE ----------------------------------------
 * A FUSE file system served from this program, as a libfuse program would
 * serve it, without libfuse:
 *   /hello        "hello from fuse\n"
 *   /link         -> hello
 *   /dir/big      300000 bytes of a pattern
 *   /dir/prog     this program's own bytes (so it can run from the mount)
 * READDIRPLUS is offered or not by the INIT answer (plus). */
struct fin  { uint32_t len, opcode; uint64_t unique, nodeid; uint32_t uid, gid, pid; uint16_t extlen, pad; };
struct fout { uint32_t len; int32_t error; uint64_t unique; };
struct fattr { uint64_t ino, size, blocks, atime, mtime, ctime; uint32_t atimensec, mtimensec, ctimensec, mode, nlink, uid, gid, rdev, blksize, flags; };
struct fentry { uint64_t nodeid, generation, entry_valid, attr_valid; uint32_t ev_nsec, av_nsec; struct fattr attr; };
struct fdirent { uint64_t ino, off; uint32_t namelen, type; };

enum { N_ROOT = 1, N_HELLO, N_LINK, N_DIR, N_BIG, N_PROG };
static const char *self_path;
static size_t prog_size;

static uint8_t bigpat(uint64_t off) { return (uint8_t)(off * 7 + (off >> 9)); }

static void node_attr(uint64_t n, struct fattr *a)
{
    memset(a, 0, sizeof *a);
    a->ino = n + 100;
    a->nlink = 1;
    a->mtime = 1700000000;
    switch (n) {
    case N_ROOT: case N_DIR: a->mode = S_IFDIR | 0755; a->nlink = 2; break;
    case N_HELLO: a->mode = S_IFREG | 0644; a->size = 16; break;
    case N_LINK: a->mode = S_IFLNK | 0777; a->size = 5; break;
    case N_BIG: a->mode = S_IFREG | 0644; a->size = 300000; break;
    case N_PROG: a->mode = S_IFREG | 0755; a->size = prog_size; break;
    }
    a->blocks = (a->size + 511) / 512;
}

static uint64_t child_of(uint64_t dir, const char *name)
{
    if (dir == N_ROOT) {
        if (!strcmp(name, "hello")) return N_HELLO;
        if (!strcmp(name, "link")) return N_LINK;
        if (!strcmp(name, "dir")) return N_DIR;
    }
    if (dir == N_DIR) {
        if (!strcmp(name, "big")) return N_BIG;
        if (!strcmp(name, "prog")) return N_PROG;
    }
    return 0;
}

static int reply(int fd, uint64_t unique, int err, const void *data, size_t len)
{
    struct fout h = { (uint32_t)(sizeof h + len), err, unique };
    struct iovec v[2] = { { &h, sizeof h }, { (void *)data, len } };
    return writev(fd, v, len ? 2 : 1) == (ssize_t)h.len ? 0 : -1;
}

/* Serve until the mount goes (read: ENODEV). Exit status: requests served. */
static int serve(int fd, int plus)
{
    static uint8_t in[256 * 1024], out[64 * 1024];
    int served = 0;
    int prog = open(self_path, O_RDONLY);
    for (;;) {
        ssize_t n = read(fd, in, sizeof in);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return errno == ENODEV ? (served > 255 ? 255 : served) : 254;
        struct fin *h = (struct fin *)in;
        void *arg = in + sizeof *h;
        served++;
        switch (h->opcode) {
        case 26: {                                      /* INIT */
            uint32_t o[16] = { 7, 31, 32768, plus ? (1u << 13) : 0 };
            o[5] = 65536;                               /* max_write */
            reply(fd, h->unique, 0, o, 64);
            break;
        }
        case 1: {                                       /* LOOKUP */
            uint64_t c = child_of(h->nodeid, (char *)arg);
            if (!c) { reply(fd, h->unique, -ENOENT, NULL, 0); break; }
            struct fentry e = { .nodeid = c, .entry_valid = 1, .attr_valid = 1 };
            node_attr(c, &e.attr);
            reply(fd, h->unique, 0, &e, sizeof e);
            break;
        }
        case 3: {                                       /* GETATTR */
            struct { uint64_t valid; uint32_t nsec, dummy; struct fattr a; } o = { 1, 0, 0, { 0 } };
            node_attr(h->nodeid, &o.a);
            reply(fd, h->unique, 0, &o, sizeof o);
            break;
        }
        case 5: reply(fd, h->unique, 0, "hello", 5); break;     /* READLINK */
        case 14: case 27: {                             /* OPEN, OPENDIR */
            uint64_t o[2] = { h->nodeid * 10, 0 };
            reply(fd, h->unique, 0, o, 16);
            break;
        }
        case 18: case 29: reply(fd, h->unique, 0, NULL, 0); break;     /* RELEASE, RELEASEDIR */
        case 15: {                                      /* READ */
            uint64_t *ri = arg;
            uint64_t off = ri[1];
            uint32_t size = ((uint32_t *)arg)[4];
            struct fattr a;
            node_attr(h->nodeid, &a);
            if (size > sizeof out) size = sizeof out;
            size_t len = off >= a.size ? 0 : (a.size - off < size ? a.size - off : size);
            if (h->nodeid == N_HELLO) memcpy(out, "hello from fuse\n" + off, len);
            else if (h->nodeid == N_BIG) for (size_t i = 0; i < len; i++) out[i] = bigpat(off + i);
            else if (h->nodeid == N_PROG) len = pread(prog, out, len, (off_t)off) > 0 ? len : 0;
            reply(fd, h->unique, 0, out, len);
            break;
        }
        case 28: case 44: {                             /* READDIR, READDIRPLUS */
            uint64_t off = ((uint64_t *)arg)[1];
            static const char *const rootn[] = { ".", "..", "hello", "link", "dir" }, *const dirn[] = { ".", "..", "big", "prog" };
            const char *const *names = h->nodeid == N_ROOT ? rootn : dirn;
            int count = h->nodeid == N_ROOT ? 5 : 4;
            size_t pos = 0;
            for (int i = (int)off; i < count; i++) {
                size_t nl = strlen(names[i]), rec = (sizeof(struct fdirent) + nl + 7) & ~(size_t)7;
                uint64_t c = i < 2 ? (i ? h->nodeid : h->nodeid) : child_of(h->nodeid, names[i]);
                if (h->opcode == 44) {
                    struct fentry *e = (struct fentry *)(out + pos);
                    memset(e, 0, sizeof *e);
                    if (i >= 2) { e->nodeid = c; node_attr(c, &e->attr); }
                    pos += sizeof *e;
                }
                struct fdirent *d = (struct fdirent *)(out + pos);
                struct fattr a;
                node_attr(c, &a);
                d->ino = a.ino; d->off = (uint64_t)i + 1; d->namelen = (uint32_t)nl; d->type = (a.mode >> 12) & 15;
                memcpy(d + 1, names[i], nl);
                memset((char *)(d + 1) + nl, 0, rec - sizeof *d - nl);
                pos += rec;
            }
            reply(fd, h->unique, 0, out, pos);
            break;
        }
        case 17: {                                      /* STATFS */
            uint64_t o[10] = { 1000, 500, 500, 10, 5, 0 };
            ((uint32_t *)o)[10] = 4096; ((uint32_t *)o)[11] = 255; ((uint32_t *)o)[12] = 4096;
            reply(fd, h->unique, 0, o, 80);
            break;
        }
        default: reply(fd, h->unique, -ENOSYS, NULL, 0); break;
        }
    }
}

static int fuse_round(int plus)
{
    const char *mp = "/tmp/apptest-fuse";
    mkdir(mp, 0755);
    int fd = open("/dev/fuse", O_RDWR);
    if (fd < 0) { CHECK(0, "open /dev/fuse"); return -1; }
    char opts[96];
    snprintf(opts, sizeof opts, "fd=%d,rootmode=40000,user_id=0,group_id=0,default_permissions", fd);
    CHECK(mount("apptest", mp, "fuse.apptest", MS_NOSUID | MS_NODEV, opts) == 0, plus ? "mount fuse.apptest (READDIRPLUS)" : "mount fuse.apptest");
    pid_t srv = fork();
    if (!srv) _exit(serve(fd, plus));
    close(fd);
    char buf[64] = { 0 };
    int f = open("/tmp/apptest-fuse/hello", O_RDONLY);
    CHECK(f >= 0 && read(f, buf, sizeof buf) == 16 && !strcmp(buf, "hello from fuse\n"), "read /hello");
    struct stat st;
    CHECK(fstat(f, &st) == 0 && st.st_ino == N_HELLO + 100 && st.st_size == 16 && (st.st_mode & 0777) == 0644, "its attributes");
    CHECK(umount2(mp, 0) < 0 && errno == EBUSY, "umount2 of a mount with an open file: EBUSY");
    close(f);
    memset(buf, 0, sizeof buf);
    CHECK(readlink("/tmp/apptest-fuse/link", buf, sizeof buf) == 5 && !strcmp(buf, "hello"), "readlink /link");
    DIR *d = opendir("/tmp/apptest-fuse");
    int seen = 0;
    for (struct dirent *e; d && (e = readdir(d));)
        seen += !strcmp(e->d_name, "hello") + !strcmp(e->d_name, "link") * 2 + (!strcmp(e->d_name, "dir") && e->d_type == DT_DIR) * 4;
    if (d) closedir(d);
    CHECK(seen == 7, "the root lists hello, link and dir");
    int ok = 1;
    f = open("/tmp/apptest-fuse/dir/big", O_RDONLY);
    static uint8_t big[300000];
    ok = f >= 0 && read(f, big, sizeof big) == (ssize_t)sizeof big;
    for (size_t i = 0; ok && i < sizeof big; i++) if (big[i] != bigpat(i)) ok = 0;
    CHECK(ok, "read 300000 bytes of /dir/big");
    uint8_t *m = f >= 0 ? mmap(NULL, sizeof big, PROT_READ, MAP_PRIVATE, f, 0) : MAP_FAILED;
    ok = m != MAP_FAILED;
    for (size_t i = 0; ok && i < sizeof big; i += 4093) if (m[i] != bigpat(i)) ok = 0;
    CHECK(ok, "the same through mmap");
    if (m != MAP_FAILED) munmap(m, sizeof big);
    if (f >= 0) close(f);
    struct statfs sf;
    CHECK(statfs(mp, &sf) == 0 && sf.f_type == 0x65735546 && sf.f_blocks == 1000 && sf.f_bsize == 4096, "statfs: FUSE_SUPER_MAGIC and the program's numbers");
    CHECK(open("/tmp/apptest-fuse/new", O_WRONLY | O_CREAT, 0644) < 0 && errno == EROFS, "creating a file: EROFS");
    char line[512];
    int listed = 0;
    FILE *mf = fopen("/proc/self/mounts", "r");
    while (mf && fgets(line, sizeof line, mf))
        if (!strcmp(line, "apptest /tmp/apptest-fuse fuse.apptest ro,nosuid,nodev,relatime 0 0\n")) listed = 1;
    if (mf) fclose(mf);
    CHECK(listed, "/proc/self/mounts lists it");
    pid_t p = fork();
    if (!p) { execl("/tmp/apptest-fuse/dir/prog", "apptest", "--exit7", (char *)NULL); _exit(42); }
    int st2 = 0;
    waitpid(p, &st2, 0);
    CHECK(WIFEXITED(st2) && WEXITSTATUS(st2) == 7, "run a program from the mount");
    CHECK(umount2(mp, 0) == 0, "umount2");
    CHECK(access("/tmp/apptest-fuse/hello", F_OK) < 0, "the files are gone");
    waitpid(srv, &st2, 0);
    CHECK(WIFEXITED(st2) && WEXITSTATUS(st2) > 5 && WEXITSTATUS(st2) < 254, "the server saw ENODEV and ended");
    rmdir(mp);
    return 0;
}

static void check_mount(const char *self)
{
    printf("mount, umount2:\n");
    self_path = self;
    struct stat st;
    prog_size = stat(self, &st) == 0 ? (size_t)st.st_size : 0;
    const char *t = "/tmp/apptest-mnt";
    mkdir(t, 0755);
    CHECK(mount("scratch", t, "tmpfs", 0, "mode=0700") == 0 && stat(t, &st) == 0 && (st.st_mode & 0777) == 0700,
          "mount tmpfs with mode=0700");
    CHECK(fstype(t) == 0x01021994, "it is tmpfs");
    int f = open("/tmp/apptest-mnt/x", O_CREAT | O_WRONLY, 0644);
    CHECK(f >= 0 && write(f, "x", 1) == 1, "a file in it");
    CHECK(mount(NULL, t, NULL, MS_REMOUNT | MS_RDONLY, NULL) == 0 &&
          open("/tmp/apptest-mnt/y", O_CREAT | O_WRONLY, 0644) < 0 && errno == EROFS, "remounted read-only: EROFS");
    mount(NULL, t, NULL, MS_REMOUNT, NULL);
    CHECK(umount2(t, 0) < 0 && errno == EBUSY, "busy while the file is open");
    close(f);
    mkdir("/tmp/apptest-mv", 0755);
    CHECK(mount(t, "/tmp/apptest-mv", NULL, MS_MOVE, NULL) == 0 && access("/tmp/apptest-mv/x", F_OK) == 0 &&
          access("/tmp/apptest-mnt/x", F_OK) < 0, "MS_MOVE to another directory");
    CHECK(umount2("/tmp/apptest-mv", 0) == 0 && access("/tmp/apptest-mv/x", F_OK) < 0, "umount2: its file is gone");
    CHECK(mount("proc", t, "proc", 0, NULL) == 0 && fstype(t) == 0x9fa0 && access("/tmp/apptest-mnt/self/status", R_OK) == 0,
          "mount proc again");
    CHECK(umount2(t, MNT_DETACH) == 0 && access("/tmp/apptest-mnt/self", F_OK) < 0, "umount2 MNT_DETACH");
    CHECK(access("/proc/self/status", R_OK) == 0, "/proc is still there");
    CHECK(mount("/etc", t, NULL, MS_BIND, NULL) == 0 && access("/tmp/apptest-mnt/passwd", R_OK) == 0, "bind /etc");
    CHECK(umount2(t, 0) == 0, "and unmount it");
    CHECK(umount2(t, 0) < 0 && errno == EINVAL, "umount2 of a plain directory: EINVAL");
    CHECK(mount("x", t, "nosuchfs", 0, NULL) < 0 && errno == ENODEV, "an unknown type: ENODEV");
    rmdir(t);
    rmdir("/tmp/apptest-mv");
    printf("FUSE:\n");
    fuse_round(0);
    fuse_round(1);
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--exit7")) return 7;
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("apptest: pid %d\n", getpid());
    check_statfs();
    check_prctl();
    check_io();
    check_exec(argv[0][0] == '/' ? argv[0] : "/bin/apptest");
    check_signals();
    check_net();
    check_sched();
    check_mount(argv[0][0] == '/' ? argv[0] : "/bin/apptest");
    if (failures) printf("apptest: %d check(s) FAILED\n", failures);
    else printf("apptest: all checks passed\n");
    return failures != 0;
}
