/* user/sandboxtest.c -- seccomp filters on Kestrel (musl)
 *
 * What Firefox's and Chromium's sandboxes ask of the kernel: no_new_privs,
 * seccomp(2) and prctl(PR_SET_SECCOMP), filters that fail calls with an
 * errno or turn them into a SIGSYS whose handler emulates them (si_syscall,
 * si_arch, si_call_addr, the registers in the ucontext), filters on
 * arguments and on calls the kernel doesn't have, stacking, inheritance by
 * fork, threads and execve, SECCOMP_FILTER_FLAG_TSYNC, the kill actions
 * and strict mode. Filters can't be taken away, so each test runs in a
 * child of its own. Exit status 0 = every check passed. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <ucontext.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>

static int failures;
#define CHECK(c, what) do { int ok_ = (c); printf("  %s  %s\n", ok_ ? "ok  " : "FAIL", what); if (!ok_) failures++; } while (0)

/* <linux/filter.h>, <linux/seccomp.h> (musl doesn't ship them) */
struct sock_filter { uint16_t code; uint8_t jt, jf; uint32_t k; };
struct sock_fprog { unsigned short len; struct sock_filter *filter; };
struct seccomp_data { int nr; uint32_t arch; uint64_t instruction_pointer; uint64_t args[6]; };
#define BPF_STMT(c, k)          { (uint16_t)(c), 0, 0, (k) }
#define BPF_JUMP(c, k, jt, jf)  { (uint16_t)(c), (jt), (jf), (k) }
#define LD_ABS      0x20        /* BPF_LD | BPF_W | BPF_ABS */
#define JEQ_K       0x15        /* BPF_JMP | BPF_JEQ | BPF_K */
#define JA          0x05
#define RET_K       0x06
#define ALU_AND_K   0x54
#define ALU_DIV_X   0x3c
#define LDX_IMM     0x01
#define SECCOMP_SET_MODE_STRICT     0
#define SECCOMP_SET_MODE_FILTER     1
#define SECCOMP_GET_ACTION_AVAIL    2
#define SECCOMP_FILTER_FLAG_TSYNC   1
#define SECCOMP_FILTER_FLAG_NEW_LISTENER 8
#define RET_KILL_PROCESS  0x80000000u
#define RET_KILL_THREAD   0x00000000u
#define RET_TRAP          0x00030000u
#define RET_ERRNO         0x00050000u
#define RET_USER_NOTIF    0x7fc00000u
#define RET_LOG           0x7ffc0000u
#define RET_ALLOW         0x7fff0000u
#define AUDIT_ARCH_X86_64 0xC000003Eu
#define NR      offsetof(struct seccomp_data, nr)
#define ARCH    offsetof(struct seccomp_data, arch)
#define ARG0    offsetof(struct seccomp_data, args[0])

static long sc(long op, long flags, void *args) { return syscall(SYS_seccomp, op, flags, args); }

static long install(struct sock_filter *f, unsigned short n, unsigned flags)
{
    struct sock_fprog p = { n, f };
    return sc(SECCOMP_SET_MODE_FILTER, flags, &p);
}

/* one call number -> one action, the rest allowed */
static long deny(long nr, uint32_t action, unsigned flags)
{
    struct sock_filter f[] = {
        BPF_STMT(LD_ABS, NR),
        BPF_JUMP(JEQ_K, (uint32_t)nr, 0, 1),
        BPF_STMT(RET_K, action),
        BPF_STMT(RET_K, RET_ALLOW),
    };
    return install(f, 4, flags);
}

/* Run fn in a child; its exit status (or 128 + the signal that ended it). */
static int in_child(int (*fn)(void))
{
    fflush(stdout);
    pid_t p = fork();
    if (p == 0) { int r = fn(); fflush(stdout); _exit(r); }
    int st = 0;
    if (waitpid(p, &st, 0) != p) return -1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
}

static int status_seccomp(void)
{
    FILE *f = fopen("/proc/self/status", "r");
    char line[128];
    int v = -1;
    if (!f) return -1;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "Seccomp: %d", &v) == 1) break;
    fclose(f);
    return v;
}

/* ---- errno, arguments, unknown calls, stacking ---------------------------- */
static int t_errno(void)
{
    int bad = 0;
    struct sock_filter f[] = {
        BPF_STMT(LD_ABS, ARCH),
        BPF_JUMP(JEQ_K, AUDIT_ARCH_X86_64, 1, 0),
        BPF_STMT(RET_K, RET_KILL_PROCESS),
        BPF_STMT(LD_ABS, NR),
        BPF_JUMP(JEQ_K, SYS_getppid, 0, 1),
        BPF_STMT(RET_K, RET_ERRNO | 1234),
        BPF_JUMP(JEQ_K, 999, 0, 1),                     /* a call Kestrel doesn't have */
        BPF_STMT(RET_K, RET_ERRNO | 77),
        BPF_JUMP(JEQ_K, SYS_getpgid, 0, 3),
        BPF_STMT(LD_ABS, ARG0),
        BPF_JUMP(JEQ_K, 12345, 0, 1),
        BPF_STMT(RET_K, RET_ERRNO | 55),
        BPF_STMT(RET_K, RET_ALLOW),
    };
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) return 90;
    if (install(f, sizeof f / sizeof f[0], 0)) return 91;
    errno = 0;
    if (!(syscall(SYS_getppid) == -1 && errno == 1234)) { printf("    getppid: errno %d\n", errno); bad |= 1; }
    errno = 0;
    if (!(syscall(999) == -1 && errno == 77)) { printf("    999: errno %d\n", errno); bad |= 2; }
    errno = 0;
    if (!(getpgid(12345) == -1 && errno == 55)) bad |= 4;
    if (getpgid(0) < 0) bad |= 8;                      /* other arguments go through */
    if (getpid() <= 0 || write(1, "", 0) != 0) bad |= 16;
    if (prctl(PR_GET_SECCOMP, 0, 0, 0, 0) != 2) bad |= 32;
    if (status_seccomp() != 2) bad |= 64;
    /* a newer filter with the same action wins */
    if (deny(SYS_getppid, RET_ERRNO | 9, 0)) bad |= 128;
    errno = 0;
    if (!(syscall(SYS_getppid) == -1 && errno == 9)) bad |= 128;
    /* errno is capped at 4095 */
    if (deny(SYS_getsid, RET_ERRNO | 0xffff, 0)) bad |= 256;
    errno = 0;
    if (!(getsid(0) == -1 && errno == 4095)) bad |= 256;
    if (bad) printf("    failed checks: %#x\n", bad);
    return bad ? 1 : 0;
}

/* ---- SIGSYS: the handler sees the call and emulates it ------------------ */
static volatile int trap_ok, trap_calls;
static void on_sys(int sig, siginfo_t *si, void *ucv)
{
    ucontext_t *uc = ucv;
    greg_t *g = uc->uc_mcontext.gregs;
    struct { void *call_addr; int syscall; unsigned arch; } *s = (void *)((char *)si + 16);
    trap_calls++;
    trap_ok = sig == SIGSYS && si->si_code == 1 /* SYS_SECCOMP */ && si->si_errno == 42 &&
              s->syscall == SYS_getuid && s->arch == AUDIT_ARCH_X86_64 &&
              (greg_t)s->call_addr == g[REG_RIP] && g[REG_RAX] == SYS_getuid;
    g[REG_RAX] = 777;                                   /* what getuid() returns */
}

static int t_trap(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_sys;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSYS, &sa, NULL);
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) return 90;
    if (deny(SYS_getuid, RET_TRAP | 42, 0)) return 91;
    long r = syscall(SYS_getuid);
    if (r != 777 || trap_calls != 1 || !trap_ok) {
        printf("    getuid() = %ld, %d SIGSYS, handler saw %s\n", r, trap_calls, trap_ok ? "the right call" : "a wrong call");
        return 1;
    }
    /* blocked: delivered anyway, with the default action (the process dies) */
    sigset_t m;
    sigemptyset(&m);
    sigaddset(&m, SIGSYS);
    sigprocmask(SIG_BLOCK, &m, NULL);
    syscall(SYS_getuid);
    return 2;
}

/* ---- inheritance: fork, execve ----------------------------------------- */
static char self_path[256];
static int t_inherit(void)
{
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) return 90;
    if (deny(SYS_getppid, RET_ERRNO | 1234, 0)) return 91;
    fflush(stdout);
    pid_t p = fork();
    if (p == 0) {
        errno = 0;
        int forked = syscall(SYS_getppid) == -1 && errno == 1234 && prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) == 1;
        if (!forked) _exit(1);
        execl(self_path, self_path, "exec-check", (char *)NULL);
        _exit(2);
    }
    int st;
    waitpid(p, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 3;
}

static int exec_check(void)
{
    errno = 0;
    if (!(syscall(SYS_getppid) == -1 && errno == 1234)) return 4;
    if (prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) != 1) return 5;
    if (prctl(PR_GET_SECCOMP, 0, 0, 0, 0) != 2) return 6;
    return 0;
}

/* ---- threads: a filter is per thread; TSYNC gives it to all ------------- */
static volatile int thr_result;
static void *thr_own(void *a)
{
    (void)a;
    if (deny(SYS_getsid, RET_ERRNO | 33, 0)) { thr_result = 1; return NULL; }
    errno = 0;
    thr_result = getsid(0) == -1 && errno == 33 ? 0 : 2;
    return NULL;
}
static void *thr_tsync(void *a)
{
    (void)a;
    thr_result = (int)deny(SYS_getpgrp, RET_ERRNO | 44, SECCOMP_FILTER_FLAG_TSYNC);
    return NULL;
}
static volatile int spin_stop;
static void *thr_spin(void *a) { (void)a; while (!spin_stop) syscall(SYS_sched_yield); errno = 0; syscall(SYS_getpgrp); return (void *)(long)errno; }

static int t_threads(void)
{
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) return 90;
    pthread_t a, b, c;
    pthread_create(&a, NULL, thr_own, NULL);
    pthread_join(a, NULL);
    if (thr_result) return 1;
    if (getsid(0) < 0) return 2;                       /* the other thread's filter isn't ours */
    pthread_create(&c, NULL, thr_spin, NULL);          /* already running when TSYNC comes */
    pthread_create(&b, NULL, thr_tsync, NULL);
    pthread_join(b, NULL);
    if (thr_result) { printf("    TSYNC returned %d\n", thr_result); return 3; }
    errno = 0;
    if (!(syscall(SYS_getpgrp) == -1 && errno == 44)) return 4;
    spin_stop = 1;
    void *e;
    pthread_join(c, &e);
    if ((long)e != 44) return 5;
    return 0;
}

/* ---- kills ---------------------------------------------------------------- */
static int t_kill_process(void)
{
    prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    if (deny(SYS_getppid, RET_KILL_PROCESS, 0)) return 91;
    getppid();
    return 0;
}

static void *thr_killed(void *a) { (void)a; deny(SYS_getppid, RET_KILL_THREAD, 0); getppid(); return (void *)1; }
static int t_kill_thread(void)
{
    prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    pthread_t t;
    pthread_create(&t, NULL, thr_killed, NULL);
    for (int i = 0; i < 200; i++) usleep(5000);        /* the thread dies; the process doesn't */
    return getppid() > 0 ? 0 : 1;
}

static int t_strict_ok(void)
{
    if (prctl(PR_SET_SECCOMP, 1, 0, 0, 0)) return 90;
    if (write(1, "", 0) != 0) return 91;
    syscall(SYS_exit, 3);                               /* exit, not exit_group */
    return 92;
}

static int t_strict_kill(void)
{
    if (sc(SECCOMP_SET_MODE_STRICT, 0, NULL)) return 90;
    getppid();
    return 0;
}

/* ---- refusals -------------------------------------------------------------- */
static int t_refusals(void)
{
    int bad = 0;
    struct sock_filter jump_out[] = { BPF_JUMP(JEQ_K, 1, 5, 0), BPF_STMT(RET_K, RET_ALLOW) };
    struct sock_filter no_ret[] = { BPF_STMT(LD_ABS, NR) };
    struct sock_filter off_end[] = { BPF_STMT(LD_ABS, 64), BPF_STMT(RET_K, RET_ALLOW) };
    struct sock_filter unaligned[] = { BPF_STMT(LD_ABS, 2), BPF_STMT(RET_K, RET_ALLOW) };
    struct sock_filter div0[] = { BPF_STMT(0x34 /* DIV K */, 0), BPF_STMT(RET_K, RET_ALLOW) };
    /* dividing by X = 0 at run time: the program returns 0, KILL_THREAD -- not here,
     * this one only checks it is accepted */
    struct sock_filter divx[] = { BPF_STMT(LDX_IMM, 1), BPF_STMT(ALU_DIV_X, 0), BPF_STMT(RET_K, RET_ALLOW) };
    errno = 0;
    if (!(sc(SECCOMP_SET_MODE_FILTER, 0, NULL) == -1 && errno == EFAULT)) bad |= 1;   /* Firefox's probe */
    if (prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) != 0) bad |= 2;
    errno = 0;
    if (!(prctl(PR_SET_NO_NEW_PRIVS, 0, 0, 0, 0) == -1 && errno == EINVAL)) bad |= 2;
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) || prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0) != 1) bad |= 2;
    errno = 0;
    if (!(install(jump_out, 0, 0) == -1 && errno == EINVAL)) bad |= 4;
    errno = 0;
    if (!(install(jump_out, 2, 0) == -1 && errno == EINVAL)) bad |= 4;
    errno = 0;
    if (!(install(no_ret, 1, 0) == -1 && errno == EINVAL)) bad |= 8;
    errno = 0;
    if (!(install(off_end, 2, 0) == -1 && errno == EINVAL)) bad |= 16;
    errno = 0;
    if (!(install(unaligned, 2, 0) == -1 && errno == EINVAL)) bad |= 16;
    errno = 0;
    if (!(install(div0, 2, 0) == -1 && errno == EINVAL)) bad |= 32;
    uint32_t act = RET_LOG;
    if (sc(SECCOMP_GET_ACTION_AVAIL, 0, &act)) bad |= 256;
    /* user notification: Kestrel has none (Linux does); either way, the
     * listener flag works exactly when the action is offered */
    act = RET_USER_NOTIF;
    errno = 0;
    long notif = sc(SECCOMP_GET_ACTION_AVAIL, 0, &act);
    if (notif && errno != EOPNOTSUPP) bad |= 256;
    errno = 0;
    long l = install(divx, 3, SECCOMP_FILTER_FLAG_NEW_LISTENER);
    if (notif ? !(l == -1 && errno == EINVAL) : l < 0) bad |= 64;
    if (install(divx, 3, 0)) bad |= 128;
    errno = 0;
    if (!(sc(SECCOMP_SET_MODE_STRICT, 0, NULL) == -1 && errno == EINVAL)) bad |= 512;  /* filters already */
    if (bad) printf("    failed checks: %#x\n", bad);
    return bad ? 1 : 0;
}

static int t_eacces(void)
{
    struct sock_filter allow[] = { BPF_STMT(RET_K, RET_ALLOW) };
    if (setuid(1000)) return 90;                        /* not root, no no_new_privs */
    errno = 0;
    if (!(install(allow, 1, 0) == -1 && errno == EACCES)) return 1;
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) || install(allow, 1, 0)) return 2;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "exec-check")) return exec_check();
    ssize_t n = readlink("/proc/self/exe", self_path, sizeof self_path - 1);
    if (n <= 0) strcpy(self_path, argv[0]); else self_path[n] = 0;
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("sandboxtest: seccomp\n");
    CHECK(prctl(PR_GET_SECCOMP, 0, 0, 0, 0) == 0, "no filter to begin with (PR_GET_SECCOMP 0)");
    CHECK(in_child(t_refusals) == 0, "refusals: NULL program, bad programs, NEW_LISTENER; no_new_privs");
    CHECK(in_child(t_eacces) == 0, "EACCES without root or no_new_privs");
    CHECK(in_child(t_errno) == 0, "RET_ERRNO on numbers, arguments, unknown calls; stacking; /proc status");
    int tr = in_child(t_trap);
    CHECK(tr == 128 + SIGSYS, "RET_TRAP: SIGSYS handler emulates the call; blocked SIGSYS kills");
    if (tr != 128 + SIGSYS) printf("    status %d\n", tr);
    CHECK(in_child(t_inherit) == 0, "inherited by fork and kept across execve");
    CHECK(in_child(t_threads) == 0, "per-thread filters; TSYNC reaches running threads");
    CHECK(in_child(t_kill_process) == 128 + SIGSYS, "RET_KILL_PROCESS ends the process with SIGSYS");
    CHECK(in_child(t_kill_thread) == 0, "RET_KILL_THREAD ends only that thread");
    CHECK(in_child(t_strict_ok) == 3, "strict mode: read, write, exit allowed");
    CHECK(in_child(t_strict_kill) == 128 + SIGKILL, "strict mode: anything else is SIGKILL");
    printf("sandboxtest: %s (%d failed)\n", failures ? "FAILED" : "passed", failures);
    return failures ? 1 : 0;
}
