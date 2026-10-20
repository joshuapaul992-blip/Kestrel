/* user/nopietest.c -- a program linked at fixed low addresses (ET_EXEC, not
 * position independent): the kernel gives it the low window (uvm.h).
 *
 * Its code, data, bss, TLS and brk heap all sit below 512 GiB, where the
 * kernel's identity map is in every other process. Each check hands the
 * kernel a pointer into that memory, or makes it fault there, and prints
 * one "nopie: ..." line; the last is "nopie: done, N failed".
 * Built with -static -no-pie (Makefile). */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#ifndef FUTEX_WAIT_PRIVATE
#define FUTEX_WAIT_PRIVATE 128
#define FUTEX_WAKE_PRIVATE 129
#endif

static int failed;
static void check(int ok, const char *what)
{
    printf("nopie: %s: %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) failed++;
}

static char bss_buf[65536];
static const char rodata_msg[] = "nopie: write() from .rodata\n";
static __thread int tls_value = 42;
static volatile sig_atomic_t got_signal;
static volatile uintptr_t handler_sp;
static char altstack[16384];
static int futex_word;
static volatile int counter;

static void on_usr1(int sig)
{
    int here;
    handler_sp = (uintptr_t)&here;
    got_signal = sig;
}

static void *thread_main(void *arg)
{
    (void)arg;
    tls_value = 7;                                      /* this thread's copy */
    for (int i = 0; i < 1000; i++) __atomic_add_fetch(&counter, 1, __ATOMIC_RELAXED);
    __atomic_store_n(&futex_word, 1, __ATOMIC_RELEASE);
    syscall(SYS_futex, &futex_word, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
    return NULL;
}

static void on_segv(int sig, siginfo_t *si, void *uc)
{
    (void)sig; (void)uc;
    printf("nopie: ok: SIGSEGV at %p\n", si->si_addr);
    fflush(stdout);
    _exit(failed ? 1 : 0);
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--exec")) {
        printf("nopie: ok: execve() into a low program again (main at %p)\n", (void *)main);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--segv")) {
        struct sigaction sa = { 0 };
        sa.sa_sigaction = on_segv;
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &sa, NULL);
        *(volatile int *)0x20000 = 1;                   /* in the window, nothing mapped */
        printf("nopie: FAIL: no SIGSEGV\n");
        return 1;
    }

    uintptr_t m = (uintptr_t)main;
    printf("nopie: main at %#lx, bss at %p, brk at %p\n", (unsigned long)m, (void *)bss_buf, sbrk(0));
    check(m < 0x8000000000ul, "main is in the low window");

    /* the kernel reads and writes the window */
    check(write(1, rodata_msg, sizeof rodata_msg - 1) == (ssize_t)(sizeof rodata_msg - 1), "write() from .rodata");
    int fd = open("/proc/self/maps", O_RDONLY);
    ssize_t n = fd >= 0 ? read(fd, bss_buf, sizeof bss_buf - 1) : -1;
    if (fd >= 0) close(fd);
    check(n > 0 && strstr(bss_buf, "nopietest") != NULL, "read() of /proc/self/maps into .bss");
    if (n > 0) {
        bss_buf[n] = 0;
        char *nl = strchr(bss_buf, '\n');
        if (nl) *nl = 0;
        printf("nopie: first mapping: %s\n", bss_buf);
    }
    struct timespec ts;
    check(clock_gettime(CLOCK_MONOTONIC, &ts) == 0 && ts.tv_sec >= 0, "clock_gettime() into the stack (vDSO)");

    /* brk heap right after the image (musl's sbrk() only reads it) */
    uintptr_t b0 = (uintptr_t)syscall(SYS_brk, 0);
    char *p = (char *)b0;
    check(b0 < 0x8000000000ul && (uintptr_t)syscall(SYS_brk, b0 + (1 << 20)) == b0 + (1 << 20), "brk() up 1 MiB in the window");
    memset(p, 0x5a, 1 << 20);
    check(p[12345] == 0x5a && (uintptr_t)syscall(SYS_brk, b0) == b0, "brk heap written and given back");
    char *h = malloc(3 << 20);
    check(h != NULL, "malloc(3 MiB)");
    if (h) { memset(h, 1, 3 << 20); free(h); }

    /* MAP_FIXED low, and a pointer from there through the kernel */
    char *low = mmap((void *)0x40000000, 1 << 16, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    check(low == (void *)0x40000000, "mmap(MAP_FIXED_NOREPLACE) at 1 GiB");
    if (low != MAP_FAILED) {
        int pfd[2];
        check(pipe(pfd) == 0 && write(pfd[1], "window", 7) == 7 && read(pfd[0], low + 4000, 7) == 7 &&
              !strcmp(low + 4000, "window"), "pipe read() into it, across a page boundary");
        close(pfd[0]); close(pfd[1]);
        check(munmap(low, 1 << 16) == 0, "munmap() of it");
    }
    void *high = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    check(high != MAP_FAILED && (uintptr_t)high >= 0x400000000000ul, "plain mmap() still goes high");

    /* signals: a handler in the window, on an alternate stack in .bss */
    stack_t ss = { .ss_sp = altstack, .ss_size = sizeof altstack };
    sigaltstack(&ss, NULL);
    struct sigaction sa = { 0 };
    sa.sa_handler = on_usr1;
    sa.sa_flags = SA_ONSTACK;
    sigaction(SIGUSR1, &sa, NULL);
    raise(SIGUSR1);
    check(got_signal == SIGUSR1 && handler_sp >= (uintptr_t)altstack && handler_sp < (uintptr_t)altstack + sizeof altstack,
          "SIGUSR1 handled on a .bss alternate stack");

    /* threads: TLS, and a futex in .bss */
    pthread_t th;
    check(pthread_create(&th, NULL, thread_main, NULL) == 0, "pthread_create()");
    while (!__atomic_load_n(&futex_word, __ATOMIC_ACQUIRE))
        syscall(SYS_futex, &futex_word, FUTEX_WAIT_PRIVATE, 0, NULL, NULL, 0);
    pthread_join(th, NULL);
    check(counter == 1000 && tls_value == 42, "futex on .bss, and TLS kept per thread");

    /* fork: the child has its own window */
    bss_buf[0] = 'P';
    pid_t pid = fork();
    if (pid == 0) {
        bss_buf[0] = 'C';
        _exit(bss_buf[0] == 'C' ? 0 : 1);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    check(pid > 0 && WIFEXITED(st) && WEXITSTATUS(st) == 0 && bss_buf[0] == 'P', "fork(): copy-on-write in the window");

    /* a fault on an unmapped window address is the process's SIGSEGV */
    pid = fork();
    if (pid == 0) { execl(argv[0], argv[0], "--segv", (char *)NULL); _exit(127); }
    waitpid(pid, &st, 0);
    check(WIFEXITED(st) && WEXITSTATUS(st) == 0, "SIGSEGV at an unmapped low address");

    /* execve: low program to low program */
    pid = fork();
    if (pid == 0) { execl(argv[0], argv[0], "--exec", (char *)NULL); _exit(127); }
    waitpid(pid, &st, 0);
    check(WIFEXITED(st) && WEXITSTATUS(st) == 0, "execve() of a low program from a low program");

    /* and to an ordinary (PIE) program */
    pid = fork();
    if (pid == 0) { execl("/bin/busybox", "true", (char *)NULL); _exit(127); }
    waitpid(pid, &st, 0);
    check(WIFEXITED(st) && WEXITSTATUS(st) == 0, "execve() of a PIE program from a low program");

    printf("nopie: done, %d failed\n", failed);
    return failed != 0;
}
