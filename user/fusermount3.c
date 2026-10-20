/* user/fusermount3.c -- mount and unmount FUSE file systems for libfuse
 *
 *   fusermount3 [-o OPTIONS] [--] MOUNTPOINT     (with _FUSE_COMMFD set)
 *   fusermount3 -u [-q] [-z] [--] MOUNTPOINT
 *
 * libfuse runs this when it cannot call mount(2) itself: it passes one end
 * of a socket pair in the environment (_FUSE_COMMFD), and gets the opened
 * /dev/fuse back over it (SCM_RIGHTS). Kestrel's sessions run as root, so
 * there is no setuid policy to apply: the options go to mount(2) as given.
 * With auto_unmount a child stays behind and unmounts once the file system
 * process closes its end of the socket. Kestrel's own program, written for
 * this protocol; also installed as fusermount. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *prog = "fusermount3";
static int quiet;

static void die(const char *what, int err)
{
    if (!quiet) fprintf(stderr, "%s: %s%s%s\n", prog, what, err ? ": " : "", err ? strerror(err) : "");
    exit(1);
}

static void usage(void)
{
    fprintf(stderr, "usage: %s [-o options] [--] mountpoint\n"
                    "       %s -u [-q] [-z] [--] mountpoint\n", prog, prog);
    exit(1);
}

/* append ",s" (or "s") to buf */
static void add(char *buf, size_t cap, const char *s, size_t n)
{
    size_t l = strlen(buf);
    if (l + 1 + n + 1 > cap) die("options too long", 0);
    if (l) buf[l++] = ',';
    memcpy(buf + l, s, n);
    buf[l + n] = 0;
}

static int send_fd(int sock, int fd)
{
    char byte = 0;
    struct iovec iov = { &byte, 1 };
    union { struct cmsghdr h; char buf[CMSG_SPACE(sizeof(int))]; } u;
    memset(&u, 0, sizeof u);
    struct msghdr msg = { .msg_iov = &iov, .msg_iovlen = 1, .msg_control = u.buf, .msg_controllen = sizeof u.buf };
    struct cmsghdr *c = CMSG_FIRSTHDR(&msg);
    c->cmsg_level = SOL_SOCKET;
    c->cmsg_type = SCM_RIGHTS;
    c->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(c), &fd, sizeof fd);
    ssize_t r;
    do r = sendmsg(sock, &msg, 0); while (r < 0 && errno == EINTR);
    return r == 1 ? 0 : -1;
}

int main(int argc, char **argv)
{
    if (argv[0] && strrchr(argv[0], '/')) prog = strrchr(argv[0], '/') + 1;
    else if (argv[0]) prog = argv[0];
    int unmount = 0, lazy = 0;
    const char *opts = "";
    int i = 1;
    for (; i < argc && argv[i][0] == '-'; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--")) { i++; break; }
        if (!strcmp(a, "-o")) { if (++i >= argc) usage(); opts = argv[i]; continue; }
        if (!strncmp(a, "-o", 2)) { opts = a + 2; continue; }
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) usage();
        if (!strcmp(a, "-V") || !strcmp(a, "--version")) { printf("%s version: 3.16.2 (Kestrel)\n", prog); return 0; }
        for (const char *p = a + 1; *p; p++) {
            if (*p == 'u') unmount = 1;
            else if (*p == 'q') quiet = 1;
            else if (*p == 'z') lazy = 1;
            else usage();
        }
    }
    if (i != argc - 1) usage();
    char mnt[PATH_MAX];
    if (!realpath(argv[i], mnt)) {
        if (!unmount || strlen(argv[i]) >= sizeof mnt) die(argv[i], errno);
        strcpy(mnt, argv[i]);                           /* a mount whose daemon is gone */
    }

    if (unmount) {
        if (umount2(mnt, lazy ? MNT_DETACH : 0) < 0) die(mnt, errno);
        return 0;
    }

    const char *cfd = getenv("_FUSE_COMMFD");
    if (!cfd) die("old style mounting not supported (_FUSE_COMMFD is not set)", 0);
    int sock = atoi(cfd);

    /* split the options: mount flags, the kernel's own, and ours */
    unsigned long flags = MS_NOSUID | MS_NODEV;
    char kopts[1024] = "", fsname[256] = "", subtype[128] = "";
    int auto_unmount = 0;
    char *o = strdup(opts);
    if (!o) die("out of memory", 0);
    for (char *save = NULL, *t = strtok_r(o, ",", &save); t; t = strtok_r(NULL, ",", &save)) {
        if (!strcmp(t, "ro")) flags |= MS_RDONLY;
        else if (!strcmp(t, "rw")) flags &= ~MS_RDONLY;
        else if (!strcmp(t, "suid")) flags &= ~MS_NOSUID;
        else if (!strcmp(t, "nosuid")) flags |= MS_NOSUID;
        else if (!strcmp(t, "dev")) flags &= ~MS_NODEV;
        else if (!strcmp(t, "nodev")) flags |= MS_NODEV;
        else if (!strcmp(t, "exec")) flags &= ~MS_NOEXEC;
        else if (!strcmp(t, "noexec")) flags |= MS_NOEXEC;
        else if (!strcmp(t, "sync")) flags |= MS_SYNCHRONOUS;
        else if (!strcmp(t, "async")) flags &= ~MS_SYNCHRONOUS;
        else if (!strcmp(t, "dirsync")) flags |= MS_DIRSYNC;
        else if (!strcmp(t, "noatime")) flags |= MS_NOATIME;
        else if (!strcmp(t, "nodiratime")) flags |= MS_NODIRATIME;
        else if (!strcmp(t, "relatime") || !strcmp(t, "atime") || !strcmp(t, "user") || !strcmp(t, "nouser")) ;
        else if (!strcmp(t, "auto_unmount")) auto_unmount = 1;
        else if (!strncmp(t, "fsname=", 7)) snprintf(fsname, sizeof fsname, "%s", t + 7);
        else if (!strncmp(t, "subtype=", 8)) snprintf(subtype, sizeof subtype, "%s", t + 8);
        else if (!strcmp(t, "default_permissions") || !strcmp(t, "allow_other") || !strcmp(t, "allow_root") ||
                 !strncmp(t, "max_read=", 9) || !strncmp(t, "blksize=", 8))
            add(kopts, sizeof kopts, t, strlen(t));
        else if (!strcmp(t, "nonempty") || !strcmp(t, "blkdev")) ;
        else if (!quiet) fprintf(stderr, "%s: ignoring option %s\n", prog, t);
    }
    free(o);

    struct stat st;
    if (stat(mnt, &st) < 0) die(mnt, errno);
    int fd = open("/dev/fuse", O_RDWR | O_CLOEXEC);
    if (fd < 0) die("/dev/fuse", errno);
    char data[1200];
    snprintf(data, sizeof data, "fd=%d,rootmode=%o,user_id=%u,group_id=%u%s%s",
             fd, st.st_mode & S_IFMT, (unsigned)getuid(), (unsigned)getgid(), kopts[0] ? "," : "", kopts);
    char type[160];
    snprintf(type, sizeof type, subtype[0] ? "fuse.%s" : "fuse", subtype);
    const char *source = fsname[0] ? fsname : subtype[0] ? subtype : "/dev/fuse";
    if (mount(source, mnt, type, flags, data) < 0) die("mount failed", errno);

    if (send_fd(sock, fd) < 0) {
        int e = errno;
        umount2(mnt, MNT_DETACH);
        die("sending the device to the file system", e);
    }
    close(fd);
    if (!auto_unmount) return 0;

    /* stay until the file system process is gone, then unmount */
    if (fork() > 0) return 0;
    setsid();
    char b;
    for (;;) {
        ssize_t r = recv(sock, &b, 1, 0);
        if (r > 0 || (r < 0 && errno == EINTR)) continue;
        break;
    }
    umount2(mnt, MNT_DETACH);
    return 0;
}
