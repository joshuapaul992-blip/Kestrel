/* =============================================================================
 *  seccomp.h -- system call filters (kernel/seccomp.c)
 *
 *  Linux's seccomp, which Firefox's and Chromium's sandboxes are built on:
 *  a thread hands the kernel classic BPF programs that see each system call
 *  it makes (its number, the architecture, the instruction pointer and the
 *  six arguments) and say what happens to it -- allow it, fail it with an
 *  errno, turn it into a SIGSYS the program's own handler takes, or kill
 *  the thread or the process. Filters stack (every one runs; the strictest
 *  answer wins), are inherited by fork and clone, survive execve and can
 *  never be taken away. seccomp(2) and prctl(PR_SET_SECCOMP) install them;
 *  SECCOMP_SET_MODE_STRICT leaves only read, write, exit and rt_sigreturn.
 *
 *  Not here: SECCOMP_RET_USER_NOTIF listeners and SECCOMP_RET_TRACE (no
 *  ptrace); a filter answering either fails the call with ENOSYS, as Linux
 *  does when no listener or tracer is attached.
 * ============================================================================= */
#ifndef KESTREL_SECCOMP_H
#define KESTREL_SECCOMP_H

#include <stdint.h>
#include <stdbool.h>

struct tcb;
struct int_frame;

#define SECCOMP_MODE_DISABLED   0
#define SECCOMP_MODE_STRICT     1
#define SECCOMP_MODE_FILTER     2

#define AUDIT_ARCH_X86_64       0xC000003Eu
#define SYS_SECCOMP             1           /* si_code of a SECCOMP_RET_TRAP SIGSYS */

struct seccomp_filter;                      /* a reference-counted, immutable chain */

/* Run the current thread's filters on the system call in f (f->rax = its
 * number). True: it goes ahead. False: it does not, and f->rax holds what
 * the caller sees (an errno, or the number itself under a SIGSYS); a kill
 * does not return. */
bool seccomp_syscall(struct int_frame *f);

/* fork/clone: the child gets the parent's mode, filters and no_new_privs
 * (proc_lock held). reap: the dead thread's reference goes. */
void seccomp_fork(struct tcb *child, const struct tcb *parent);
void seccomp_release(struct tcb *t);

int64_t sys_seccomp(uint64_t op, uint64_t flags, uint64_t uargs, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t seccomp_prctl_set(uint64_t mode, uint64_t uprog);     /* PR_SET_SECCOMP */

#endif
