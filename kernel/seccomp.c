/* kernel/seccomp.c -- system call filters (seccomp.h)
 *
 * Written from the Linux user-space interface (seccomp(2), prctl(2),
 * <linux/seccomp.h>, <linux/filter.h>) so that Firefox's and Chromium's
 * sandboxes, built for it, work unchanged.
 *
 * A filter is a classic BPF program over struct seccomp_data. Programs are
 * checked once when they are installed -- only the instructions seccomp
 * allows, loads inside seccomp_data, jumps that stay inside the program
 * (they can only go forward, so every run ends), a return at the end -- and
 * then interpreted on every system call of the thread. A thread's filters
 * form a chain, newest first; a child shares its parent's chain, so each
 * link is reference-counted and never changes after it is installed. */
#include <kernel/seccomp.h>
#include <kernel/syscall.h>
#include <kernel/process.h>
#include <kernel/task.h>
#include <kernel/arch.h>
#include <kernel/mm.h>
#include <kernel/string.h>
#include <kernel/klog.h>
#include <kernel/uaccess.h>
#include <kernel/posix.h>

/* <linux/seccomp.h> */
#define SECCOMP_SET_MODE_STRICT         0
#define SECCOMP_SET_MODE_FILTER         1
#define SECCOMP_GET_ACTION_AVAIL        2
#define SECCOMP_GET_NOTIF_SIZES         3

#define SECCOMP_FILTER_FLAG_TSYNC       (1u << 0)
#define SECCOMP_FILTER_FLAG_LOG         (1u << 1)
#define SECCOMP_FILTER_FLAG_SPEC_ALLOW  (1u << 2)
#define SECCOMP_FILTER_FLAG_NEW_LISTENER (1u << 3)
#define SECCOMP_FILTER_FLAG_TSYNC_ESRCH (1u << 4)

#define SECCOMP_RET_KILL_PROCESS        0x80000000u
#define SECCOMP_RET_KILL_THREAD         0x00000000u
#define SECCOMP_RET_TRAP                0x00030000u
#define SECCOMP_RET_ERRNO               0x00050000u
#define SECCOMP_RET_USER_NOTIF          0x7fc00000u
#define SECCOMP_RET_TRACE               0x7ff00000u
#define SECCOMP_RET_LOG                 0x7ffc0000u
#define SECCOMP_RET_ALLOW               0x7fff0000u
#define SECCOMP_RET_ACTION_FULL         0xffff0000u
#define SECCOMP_RET_DATA                0x0000ffffu

#define BPF_MAXINSNS        4096        /* per filter */
#define MAX_INSNS_PER_PATH  32768       /* the whole chain, 4 more per filter */
#define BPF_MEMWORDS        16

/* <linux/filter.h>, <linux/bpf_common.h> */
struct sock_filter { uint16_t code; uint8_t jt, jf; uint32_t k; };
struct sock_fprog { uint16_t len; uint16_t pad[3]; uint64_t filter; };

#define BPF_CLASS(c)    ((c) & 0x07)
#define BPF_LD      0x00
#define BPF_LDX     0x01
#define BPF_ST      0x02
#define BPF_STX     0x03
#define BPF_ALU     0x04
#define BPF_JMP     0x05
#define BPF_RET     0x06
#define BPF_MISC    0x07
#define BPF_W       0x00
#define BPF_IMM     0x00
#define BPF_ABS     0x20
#define BPF_MEM     0x60
#define BPF_LEN     0x80
#define BPF_OP(c)   ((c) & 0xf0)
#define BPF_ADD     0x00
#define BPF_SUB     0x10
#define BPF_MUL     0x20
#define BPF_DIV     0x30
#define BPF_OR      0x40
#define BPF_AND     0x50
#define BPF_LSH     0x60
#define BPF_RSH     0x70
#define BPF_NEG     0x80
#define BPF_MOD     0x90
#define BPF_XOR     0xa0
#define BPF_JA      0x00
#define BPF_JEQ     0x10
#define BPF_JGT     0x20
#define BPF_JGE     0x30
#define BPF_JSET    0x40
#define BPF_K       0x00
#define BPF_X       0x08
#define BPF_A       0x10
#define BPF_TAX     0x00
#define BPF_TXA     0x80

struct seccomp_data {
    int32_t  nr;
    uint32_t arch;
    uint64_t instruction_pointer;
    uint64_t args[6];
};
_Static_assert(sizeof(struct seccomp_data) == 64, "seccomp_data layout");

struct seccomp_filter {
    int       refs;
    struct seccomp_filter *prev;    /* the older filters (holds a reference)      */
    uint16_t  len;
    bool      log;                  /* SECCOMP_FILTER_FLAG_LOG                     */
    struct sock_filter insns[];
};

static void filter_get(struct seccomp_filter *f)
{
    if (f) __atomic_add_fetch(&f->refs, 1, __ATOMIC_RELAXED);
}

static void filter_put(struct seccomp_filter *f)
{
    while (f && __atomic_sub_fetch(&f->refs, 1, __ATOMIC_ACQ_REL) == 0) {
        struct seccomp_filter *prev = f->prev;
        kfree(f);
        f = prev;
    }
}

/* ---- checking a program (once, when it is installed) --------------------- */

static bool alu_ok(struct sock_filter *in)
{
    uint16_t op = BPF_OP(in->code), src = in->code & BPF_X;
    if (in->code & ~(0xf0 | BPF_X | 0x07)) return false;
    switch (op) {
    case BPF_NEG: return src == BPF_K;
    case BPF_DIV: case BPF_MOD: return src == BPF_X || in->k != 0;
    case BPF_LSH: case BPF_RSH: return src == BPF_X || in->k < 32;
    case BPF_ADD: case BPF_SUB: case BPF_MUL: case BPF_OR: case BPF_AND: case BPF_XOR: return true;
    default: return false;
    }
}

/* seccomp_check_filter() and bpf_check_classic() together: 0 or -EINVAL.
 * LEN loads become immediates (the length never changes). */
static int check(struct sock_filter *p, unsigned len)
{
    for (unsigned pc = 0; pc < len; pc++) {
        struct sock_filter *in = &p[pc];
        unsigned left = len - pc - 1;               /* instructions after this one */
        switch (BPF_CLASS(in->code)) {
        case BPF_LD:
            if (in->code == (BPF_LD | BPF_W | BPF_ABS)) {
                if (in->k >= sizeof(struct seccomp_data) || (in->k & 3)) return -EINVAL;
            } else if (in->code == (BPF_LD | BPF_W | BPF_LEN)) {
                in->code = BPF_LD | BPF_IMM;
                in->k = sizeof(struct seccomp_data);
            } else if (in->code == (BPF_LD | BPF_MEM)) {
                if (in->k >= BPF_MEMWORDS) return -EINVAL;
            } else if (in->code != (BPF_LD | BPF_IMM)) return -EINVAL;
            break;
        case BPF_LDX:
            if (in->code == (BPF_LDX | BPF_W | BPF_LEN)) {
                in->code = BPF_LDX | BPF_IMM;
                in->k = sizeof(struct seccomp_data);
            } else if (in->code == (BPF_LDX | BPF_MEM)) {
                if (in->k >= BPF_MEMWORDS) return -EINVAL;
            } else if (in->code != (BPF_LDX | BPF_IMM)) return -EINVAL;
            break;
        case BPF_ST: case BPF_STX:
            if ((in->code & ~0x07) || in->k >= BPF_MEMWORDS) return -EINVAL;
            break;
        case BPF_ALU:
            if (!alu_ok(in)) return -EINVAL;
            break;
        case BPF_JMP:
            if (in->code == (BPF_JMP | BPF_JA)) {
                if (in->k >= left) return -EINVAL;
                break;
            }
            if (in->code & ~(0xf0 | BPF_X | 0x07)) return -EINVAL;
            switch (BPF_OP(in->code)) {
            case BPF_JEQ: case BPF_JGT: case BPF_JGE: case BPF_JSET: break;
            default: return -EINVAL;
            }
            if (in->jt >= left || in->jf >= left) return -EINVAL;
            break;
        case BPF_RET:
            if (in->code != (BPF_RET | BPF_K) && in->code != (BPF_RET | BPF_A)) return -EINVAL;
            break;
        case BPF_MISC:
            if (in->code != (BPF_MISC | BPF_TAX) && in->code != (BPF_MISC | BPF_TXA)) return -EINVAL;
            break;
        }
    }
    return BPF_CLASS(p[len - 1].code) == BPF_RET ? 0 : -EINVAL;
}

/* ---- running one ---------------------------------------------------------- */

static uint32_t run(const struct seccomp_filter *flt, const struct seccomp_data *d)
{
    uint32_t A = 0, X = 0, M[BPF_MEMWORDS] = { 0 };
    const struct sock_filter *p = flt->insns;
    for (unsigned pc = 0; pc < flt->len; pc++) {
        const struct sock_filter *in = &p[pc];
        uint32_t k = in->k;
        switch (BPF_CLASS(in->code)) {
        case BPF_LD:
            if (in->code == (BPF_LD | BPF_W | BPF_ABS)) memcpy(&A, (const uint8_t *)d + k, 4);
            else if (in->code == (BPF_LD | BPF_MEM)) A = M[k];
            else A = k;
            break;
        case BPF_LDX:
            X = in->code == (BPF_LDX | BPF_MEM) ? M[k] : k;
            break;
        case BPF_ST:  M[k] = A; break;
        case BPF_STX: M[k] = X; break;
        case BPF_ALU: {
            uint32_t v = (in->code & BPF_X) ? X : k;
            switch (BPF_OP(in->code)) {
            case BPF_ADD: A += v; break;
            case BPF_SUB: A -= v; break;
            case BPF_MUL: A *= v; break;
            case BPF_DIV: if (!v) return 0; A /= v; break;      /* by X = 0: the program returns 0 */
            case BPF_MOD: if (!v) return 0; A %= v; break;
            case BPF_OR:  A |= v; break;
            case BPF_AND: A &= v; break;
            case BPF_LSH: A <<= (v & 31); break;
            case BPF_RSH: A >>= (v & 31); break;
            case BPF_XOR: A ^= v; break;
            case BPF_NEG: A = -A; break;
            }
            break;
        }
        case BPF_JMP: {
            if (in->code == (BPF_JMP | BPF_JA)) { pc += k; break; }
            uint32_t v = (in->code & BPF_X) ? X : k;
            bool c;
            switch (BPF_OP(in->code)) {
            case BPF_JEQ: c = A == v; break;
            case BPF_JGT: c = A > v; break;
            case BPF_JGE: c = A >= v; break;
            default:      c = (A & v) != 0; break;     /* BPF_JSET */
            }
            pc += c ? in->jt : in->jf;
            break;
        }
        case BPF_RET:
            return in->code == (BPF_RET | BPF_A) ? A : k;
        case BPF_MISC:
            if (in->code == (BPF_MISC | BPF_TAX)) X = A; else A = X;
            break;
        }
    }
    return 0;                                       /* not reached: check() wants a return last */
}

/* Every filter runs; the action that comes first in Linux's order (kill
 * process, kill thread, trap, errno, user notification, trace, log, allow)
 * wins, the newest filter's on a tie. */
static uint32_t run_chain(const struct seccomp_filter *f, const struct seccomp_data *d,
                          const struct seccomp_filter **match)
{
    uint32_t ret = SECCOMP_RET_ALLOW;
    for (; f; f = f->prev) {
        uint32_t r = run(f, d);
        if ((int32_t)(r & SECCOMP_RET_ACTION_FULL) < (int32_t)(ret & SECCOMP_RET_ACTION_FULL)) {
            ret = r;
            *match = f;
        }
    }
    return ret;
}

static int logged;                                  /* LOG and FLAG_LOG lines: a few, then quiet */

static void note(const struct tcb *t, const char *what, uint64_t nr, uint32_t data)
{
    if (logged >= 32) return;
    logged++;
    const char *name = syscall_name((int)nr);
    kprintf("seccomp: pid %d (%s): %s syscall %lu (%s), data %u\n", t->pid, t->name, what, nr,
            name ? name : "?", data);
}

bool seccomp_syscall(struct int_frame *f)
{
    struct tcb *t = current_task();
    uint64_t nr = f->rax;
    if (t->seccomp_mode == SECCOMP_MODE_STRICT) {
        if (nr == SYS_read || nr == SYS_write || nr == SYS_exit || nr == SYS_rt_sigreturn) return true;
        kprintf("seccomp: pid %d (%s): syscall %lu in strict mode\n", t->pid, t->name, nr);
        signal_die(SIGKILL, false, f->rip);
    }
    const struct seccomp_filter *flt = __atomic_load_n(&t->seccomp, __ATOMIC_ACQUIRE);
    if (!flt) return true;
    struct seccomp_data d = {
        .nr = (int32_t)nr, .arch = AUDIT_ARCH_X86_64, .instruction_pointer = f->rip,
        .args = { f->rdi, f->rsi, f->rdx, f->r10, f->r8, f->r9 },
    };
    const struct seccomp_filter *m = NULL;
    uint32_t r = run_chain(flt, &d, &m);
    uint32_t data = r & SECCOMP_RET_DATA;
    switch (r & SECCOMP_RET_ACTION_FULL) {
    case SECCOMP_RET_ALLOW:
        return true;
    case SECCOMP_RET_LOG:
        note(t, "logged", nr, data);
        return true;
    case SECCOMP_RET_ERRNO:
        if (m->log) note(t, "errno for", nr, data);
        f->rax = (uint64_t)-(int64_t)(data > 4095 ? 4095 : data);
        return false;
    case SECCOMP_RET_TRAP: {
        /* SIGSYS to the program's handler, which emulates the call: f->rax
         * keeps the number, as Linux's syscall_rollback() leaves it */
        if (m->log) note(t, "SIGSYS for", nr, data);
        struct ksig_info i = { .code = SYS_SECCOMP, .addr = f->rip, .syscall = (int32_t)nr,
                               .arch = AUDIT_ARCH_X86_64, .errnum = (int32_t)data };
        signal_force_info(SIGSYS, &i);
        return false;
    }
    case SECCOMP_RET_TRACE:                         /* no tracer, no listener */
    case SECCOMP_RET_USER_NOTIF:
        f->rax = (uint64_t)-ENOSYS;
        return false;
    case SECCOMP_RET_KILL_THREAD:
        kprintf("seccomp: pid %d (%s): thread killed for syscall %lu\n", t->pid, t->name, nr);
        signal_die(SIGSYS, false, f->rip);
    default:                                        /* KILL_PROCESS, and actions Linux doesn't know */
        kprintf("seccomp: pid %d (%s): process killed for syscall %lu\n", t->pid, t->name, nr);
        signal_die(SIGSYS, true, f->rip);
    }
}

/* ---- inheritance -------------------------------------------------------- */

void seccomp_fork(struct tcb *c, const struct tcb *p)
{
    c->seccomp_mode = p->seccomp_mode;
    c->no_new_privs = p->no_new_privs;
    c->seccomp = p->seccomp;
    filter_get(c->seccomp);
}

void seccomp_release(struct tcb *t)
{
    uint64_t fl = spin_lock_irqsave(&proc_lock);    /* a TSYNC in another thread looks at it */
    struct seccomp_filter *f = t->seccomp;
    t->seccomp = NULL;
    t->seccomp_mode = SECCOMP_MODE_DISABLED;
    spin_unlock_irqrestore(&proc_lock, fl);
    filter_put(f);
}

/* ---- installing ------------------------------------------------------------ */

static bool is_ancestor(const struct seccomp_filter *old, const struct seccomp_filter *chain)
{
    if (!old) return true;
    for (; chain; chain = chain->prev)
        if (chain == old) return true;
    return false;
}

static bool live_sibling(const struct tcb *o, const struct tcb *t)
{
    return o != t && o->state != TASK_UNUSED && o->state != TASK_ZOMBIE && o->user && o->signal == t->signal;
}

static int64_t set_strict(void)
{
    struct tcb *t = current_task();
    uint64_t fl = spin_lock_irqsave(&proc_lock);
    int64_t r = 0;
    if (t->seccomp_mode == SECCOMP_MODE_FILTER) r = -EINVAL;
    else t->seccomp_mode = SECCOMP_MODE_STRICT;
    spin_unlock_irqrestore(&proc_lock, fl);
    return r;
}

static int64_t set_filter(uint64_t flags, uint64_t uprog)
{
    const uint64_t known = SECCOMP_FILTER_FLAG_TSYNC | SECCOMP_FILTER_FLAG_LOG | SECCOMP_FILTER_FLAG_SPEC_ALLOW |
                           SECCOMP_FILTER_FLAG_TSYNC_ESRCH;
    if (flags & ~known) return -EINVAL;             /* NEW_LISTENER included: no user notification */
    struct tcb *t = current_task();
    struct sock_fprog fp;
    if (copy_from_user(&fp, (const void *)uprog, sizeof fp)) return -EFAULT;
    if (fp.len == 0 || fp.len > BPF_MAXINSNS) return -EINVAL;
    /* a filter could fool a set-user-ID program, so Linux wants either
     * no_new_privs or CAP_SYS_ADMIN (here: root) */
    if (!t->no_new_privs && t->uid != 0) return -EACCES;
    struct seccomp_filter *n = kmalloc(sizeof *n + fp.len * sizeof(struct sock_filter));
    if (!n) return -ENOMEM;
    if (copy_from_user(n->insns, (const void *)fp.filter, fp.len * sizeof(struct sock_filter))) {
        kfree(n);
        return -EFAULT;
    }
    int err = check(n->insns, fp.len);
    if (err) { kfree(n); return err; }
    n->refs = 1;
    n->len = fp.len;
    n->log = flags & SECCOMP_FILTER_FLAG_LOG;
    n->prev = NULL;

    uint64_t fl = spin_lock_irqsave(&proc_lock);    /* clone() copies t->seccomp under it */
    int64_t r = 0;
    unsigned total = n->len;
    for (const struct seccomp_filter *w = t->seccomp; w; w = w->prev) total += w->len + 4;
    if (t->seccomp_mode == SECCOMP_MODE_STRICT) r = -EINVAL;
    else if (total > MAX_INSNS_PER_PATH) r = -ENOMEM;
    else if (flags & SECCOMP_FILTER_FLAG_TSYNC) {
        /* every other thread must be able to take the new chain: no strict
         * mode, and nothing it has that the chain would drop */
        for (int i = 0; i < MAX_TASKS && !r; i++) {
            struct tcb *o = task_slot(i);
            if (live_sibling(o, t) &&
                (o->seccomp_mode == SECCOMP_MODE_STRICT || !is_ancestor(o->seccomp, t->seccomp)))
                r = (flags & SECCOMP_FILTER_FLAG_TSYNC_ESRCH) ? -ESRCH : o->pid;
        }
    }
    if (r) {
        spin_unlock_irqrestore(&proc_lock, fl);
        kfree(n);
        return r;
    }
    n->prev = t->seccomp;                           /* t's reference moves to the chain */
    __atomic_store_n(&t->seccomp, n, __ATOMIC_RELEASE);
    t->seccomp_mode = SECCOMP_MODE_FILTER;
    if (flags & SECCOMP_FILTER_FLAG_TSYNC) {
        for (int i = 0; i < MAX_TASKS; i++) {
            struct tcb *o = task_slot(i);
            if (!live_sibling(o, t)) continue;
            struct seccomp_filter *old = o->seccomp;
            filter_get(n);
            __atomic_store_n(&o->seccomp, n, __ATOMIC_RELEASE);
            o->seccomp_mode = SECCOMP_MODE_FILTER;
            if (t->no_new_privs) o->no_new_privs = true;
            filter_put(old);                        /* never the last: n's chain holds it */
        }
    }
    spin_unlock_irqrestore(&proc_lock, fl);
    return 0;
}

int64_t seccomp_prctl_set(uint64_t mode, uint64_t uprog)
{
    if (mode == SECCOMP_MODE_STRICT) return set_strict();
    if (mode == SECCOMP_MODE_FILTER) return set_filter(0, uprog);
    return -EINVAL;
}

int64_t sys_seccomp(uint64_t op, uint64_t flags, uint64_t uargs, uint64_t a4, uint64_t a5, uint64_t a6)
{
    (void)a4; (void)a5; (void)a6;
    switch (op) {
    case SECCOMP_SET_MODE_STRICT:
        if (flags || uargs) return -EINVAL;
        return set_strict();
    case SECCOMP_SET_MODE_FILTER:
        return set_filter(flags, uargs);
    case SECCOMP_GET_ACTION_AVAIL: {
        uint32_t action;
        if (flags) return -EINVAL;
        if (get_user_u32(&action, (const void *)uargs)) return -EFAULT;
        switch (action) {
        case SECCOMP_RET_KILL_PROCESS: case SECCOMP_RET_KILL_THREAD: case SECCOMP_RET_TRAP:
        case SECCOMP_RET_ERRNO: case SECCOMP_RET_TRACE: case SECCOMP_RET_LOG: case SECCOMP_RET_ALLOW:
            return 0;
        default:                                    /* USER_NOTIF: no listeners */
            return -EOPNOTSUPP;
        }
    }
    default:                                        /* GET_NOTIF_SIZES too, as before Linux 5.0 */
        return -EINVAL;
    }
}
