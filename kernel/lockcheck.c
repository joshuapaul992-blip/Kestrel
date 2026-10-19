/* kernel/lockcheck.c -- the lock-order checker (see lockcheck.h)
 *
 * Classes are numbered from 1 (1 is the big kernel lock). The pairs seen
 * are a bit matrix, pair[a] having bit b once a lock of class b was taken
 * with one of class a held; a new pair a -> b closes a cycle if b already
 * reaches a, which a search over the matrix finds. Each CPU keeps the
 * spinlocks it holds (they are held with interrupts off, so they never
 * move to another CPU), each task the sleeping locks it holds.
 *
 * The checker's own state has a plain spinlock of its own, which it does
 * not check; while a CPU is inside the checker (reporting through kprintf,
 * which takes the log's lock) the hooks on that CPU do nothing. */
#include <kernel/lockcheck.h>

#ifdef LOCKCHECK
#include <kernel/smp.h>
#include <kernel/task.h>
#include <kernel/cpu.h>
#include <kernel/arch.h>
#include <kernel/klog.h>
#include <kernel/string.h>

#define LC_MAX          1024            /* classes                              */
#define LC_KEYS         2048            /* the key table: twice LC_MAX          */
#define LC_EDGES        8192            /* where each pair was first seen       */
#define LC_SPIN_DEPTH   16              /* spinlocks one CPU holds at once      */
#define LC_SITES        512             /* places already reported              */
#define CLS_BKL         1               /* the big kernel lock's class */

struct lc_class {
    uint64_t    key;                    /* the name's hash, or the first taker  */
    const char *name;                   /* lockcheck_name(), or NULL            */
    uint64_t    site;                   /* where a lock of it was first taken   */
};

struct lc_edge { uint16_t from, to; uint64_t held_site, site; };
struct lc_held { void *lock; uint16_t cls; uint64_t site; };

static struct lc_class classes[LC_MAX];
static int nclasses = 2;                /* 0 unused, 1 the big lock */
static uint16_t keys[LC_KEYS];          /* open addressing: class numbers by key */
static uint64_t pair[LC_MAX][LC_MAX / 64];
static struct lc_edge edges[LC_EDGES];
static int npairs;
static uint64_t reported_sites[LC_SITES];
static uint64_t self_up[LC_MAX / 64], self_down[LC_MAX / 64];   /* nested at a higher, a lower address */
static uint64_t self_reported[LC_MAX / 64];
static int nreports;

static struct {
    struct lc_held h[LC_SPIN_DEPTH];
    int n;
    bool inside;
    uint64_t bkl_site;                  /* where this CPU last took the big lock */
} percpu_lc[MAX_CPUS];

static volatile uint32_t lc_lock;       /* the checker's own state */

static void lc_take(void) { while (__atomic_exchange_n(&lc_lock, 1, __ATOMIC_ACQUIRE)) cpu_relax(); }
static void lc_give(void) { __atomic_store_n(&lc_lock, 0, __ATOMIC_RELEASE); }

/* This CPU's state, or NULL before its per-CPU block is set up. */
static int cpu_id(void)
{
    if (!percpu_is_kernel_gs(rdmsr(MSR_GS_BASE))) return -1;
    return this_cpu()->id;
}

static void site_str(uint64_t site, char *out, size_t cap)
{
    uint64_t off;
    const char *n = site ? ksym(site, &off) : NULL;
    if (n) snprintf(out, cap, "%s+0x%lx", n, off);
    else snprintf(out, cap, "%lx", site);
}

static void class_str(int c, char *out, size_t cap)
{
    if (c == CLS_BKL) { snprintf(out, cap, "the big kernel lock"); return; }
    if (classes[c].name) { snprintf(out, cap, "\"%s\"", classes[c].name); return; }
    uint64_t off;
    const char *n = ksym(classes[c].site, &off);
    snprintf(out, cap, "lock first taken in %s", n ? n : "?");
}

static uint64_t name_key(const char *s)
{
    uint64_t h = 1469598103934665603ull;
    while (*s) h = (h ^ (uint8_t)*s++) * 1099511628211ull;
    return h | (1ull << 63);            /* never a kernel address */
}

/* The class for key, made if new (checker lock held); 0 when full. */
static int class_for(uint64_t key, const char *name, uint64_t site)
{
    uint32_t i = (uint32_t)(key ^ (key >> 29)) % LC_KEYS;
    for (int probe = 0; probe < LC_KEYS; probe++, i = (i + 1) % LC_KEYS) {
        int c = keys[i];
        if (!c) break;
        if (classes[c].key == key) return c;
    }
    if (nclasses >= LC_MAX) return 0;
    int c = nclasses++;
    classes[c] = (struct lc_class){ key, name, site };
    keys[i] = (uint16_t)c;
    return c;
}

static int tag_class(const uint32_t *tag)
{
    uint32_t v = __atomic_load_n(tag, __ATOMIC_RELAXED);
    if ((v >> 16) != LC_MAGIC) return 0;
    int c = (int)(v & 0xffff);
    return c > 0 && c < LC_MAX ? c : 0;
}

void lockcheck_set_name(uint32_t *tag, const char *name)
{
    uint64_t f = irq_save();
    lc_take();
    int c = class_for(name_key(name), name, 0);
    lc_give();
    __atomic_store_n(tag, c ? (LC_MAGIC << 16) | (uint32_t)c : 0, __ATOMIC_RELAXED);
    irq_restore(f);
}

/* Report each place once (checker lock held): true the first time. */
static bool first_report_at(uint64_t site)
{
    uint32_t i = (uint32_t)(site ^ (site >> 17)) % LC_SITES;
    for (int probe = 0; probe < LC_SITES; probe++, i = (i + 1) % LC_SITES) {
        if (reported_sites[i] == site) return false;
        if (!reported_sites[i]) { reported_sites[i] = site; return true; }
    }
    return false;                       /* full: quiet from now on */
}

static struct lc_edge *edge_find(int from, int to)
{
    uint32_t i = (uint32_t)(from * 31 + to) % LC_EDGES;
    for (int probe = 0; probe < LC_EDGES; probe++, i = (i + 1) % LC_EDGES) {
        if (!edges[i].from) return &edges[i];
        if (edges[i].from == from && edges[i].to == to) return &edges[i];
    }
    return NULL;
}

/* A path from `from` to `to` over the pairs seen (checker lock held):
 * path[0] = from ... path[len-1] = to, or 0 if there is none. */
static int find_path(int from, int to, uint16_t *path)
{
    static uint16_t prev[LC_MAX], queue[LC_MAX];
    static uint64_t seen[LC_MAX / 64];
    memset(seen, 0, sizeof seen);
    int head = 0, tail = 0;
    queue[tail++] = (uint16_t)from;
    seen[from / 64] |= 1ull << (from % 64);
    while (head < tail) {
        int a = queue[head++];
        if (a == to) {
            int len = 0;
            for (int x = to; ; x = prev[x]) { path[len++] = (uint16_t)x; if (x == from) break; }
            for (int i = 0; i < len / 2; i++) { uint16_t t = path[i]; path[i] = path[len - 1 - i]; path[len - 1 - i] = t; }
            return len;
        }
        for (int w = 0; w < LC_MAX / 64; w++) {
            uint64_t bits = pair[a][w] & ~seen[w];
            while (bits) {
                int b = w * 64 + __builtin_ctzll(bits);
                bits &= bits - 1;
                seen[w] |= 1ull << (b % 64);
                prev[b] = (uint16_t)a;
                queue[tail++] = (uint16_t)b;
            }
        }
    }
    return 0;
}

/* What to print after letting go of the checker lock. */
struct lc_report {
    int kind;                           /* 0 none, 1 cycle, 2 sleep under spin, 3 nested class, 4 twice */
    int from, to;
    uint64_t held_site, site;
    uint16_t path[32];
    int len;
};

/* A lock of class c taken with another of the same class held: safe if
 * the code always takes the two in one order, which the checker can see
 * when that order is by address (futex buckets, lowest first). Reported
 * when both orders have been seen, or the same lock is taken twice. */
static void same_class(int c, const void *held, const void *lock, uint64_t held_site, uint64_t site,
                       struct lc_report *r)
{
    uint64_t bit = 1ull << (c % 64);
    if (self_reported[c / 64] & bit) return;
    if (lock > held) self_up[c / 64] |= bit;
    else if (lock < held) self_down[c / 64] |= bit;
    if (lock != held && (!(self_up[c / 64] & bit) || !(self_down[c / 64] & bit))) return;
    self_reported[c / 64] |= bit;
    if (!r->kind) *r = (struct lc_report){ .kind = lock == held ? 4 : 3, .from = c, .to = c, .held_site = held_site, .site = site };
}

/* A held lock of class `from`, taken at held_site, then one of class `to`
 * at site (checker lock held). */
static void add_pair(int from, int to, uint64_t held_site, uint64_t site, struct lc_report *r)
{
    if (from == to) return;             /* see same_class */
    if (pair[from][to / 64] & (1ull << (to % 64))) return;
    struct lc_edge *e = edge_find(from, to);
    if (e) *e = (struct lc_edge){ (uint16_t)from, (uint16_t)to, held_site, site };
    if (!r->kind) {
        uint16_t path[32];
        int len = find_path(to, from, path);
        if (len && len <= 32) {
            *r = (struct lc_report){ .kind = 1, .from = from, .to = to, .held_site = held_site, .site = site, .len = len };
            memcpy(r->path, path, sizeof path);
        }
    }
    pair[from][to / 64] |= 1ull << (to % 64);
    npairs++;
}

static bool selftest;                   /* lockcheck_selftest() running: count, do not print */

static void print_report(struct lc_report *r)
{
    if (selftest) { __atomic_add_fetch(&nreports, 1, __ATOMIC_RELAXED); return; }
    char a[96], b[96], s1[96], s2[96];
    class_str(r->from, a, sizeof a);
    class_str(r->to, b, sizeof b);
    site_str(r->held_site, s1, sizeof s1);
    site_str(r->site, s2, sizeof s2);
    __atomic_add_fetch(&nreports, 1, __ATOMIC_RELAXED);
    if (r->kind == 2) {
        kprintf("lockcheck: %s taken at %s with a spinlock held (%s, taken at %s): it may sleep\n", b, s2, a, s1);
        return;
    }
    if (r->kind == 3) {
        kprintf("lockcheck: two locks of one class taken in both orders: %s taken at %s, another at %s\n", a, s1, s2);
        return;
    }
    if (r->kind == 4) {
        kprintf("lockcheck: %s taken at %s, and again at %s: it waits for itself\n", a, s1, s2);
        return;
    }
    kprintf("lockcheck: lock order cycle: %s (taken at %s), then %s at %s; but elsewhere:\n", a, s1, b, s2);
    for (int i = 0; i + 1 < r->len; i++) {
        lc_take();
        struct lc_edge *e = edge_find(r->path[i], r->path[i + 1]);
        struct lc_edge copy = e && e->from ? *e : (struct lc_edge){ 0 };
        lc_give();
        class_str(r->path[i], a, sizeof a);
        class_str(r->path[i + 1], b, sizeof b);
        site_str(copy.held_site, s1, sizeof s1);
        site_str(copy.site, s2, sizeof s2);
        kprintf("lockcheck:   %s (taken at %s), then %s at %s\n", a, s1, b, s2);
    }
}

static bool sleeping(enum lc_kind k) { return k == LC_MUTEX || k == LC_RWLOCK; }

void lockcheck_acquire(void *lock, uint32_t *tag, enum lc_kind kind, bool try, uint64_t site)
{
    if (!site) site = (uint64_t)__builtin_return_address(0);
    uint64_t f = irq_save();
    int cpu = cpu_id();
    if (cpu < 0 || percpu_lc[cpu].inside) { irq_restore(f); return; }
    percpu_lc[cpu].inside = true;
    struct percpu *pc = this_cpu();
    struct tcb *t = pc->current;
    struct lc_report r = { 0 };

    lc_take();
    int c = kind == LC_BKL ? CLS_BKL : tag_class(tag);
    if (!c) {
        uint64_t off;
        c = class_for(ksym(site, &off) ? site - off : site, NULL, site);
        if (c && tag) __atomic_store_n(tag, (LC_MAGIC << 16) | (uint32_t)c, __ATOMIC_RELAXED);
    }
    if (c == CLS_BKL) {
        if (!classes[CLS_BKL].site) classes[CLS_BKL].site = site;
        percpu_lc[cpu].bkl_site = site;
    }
    if (c && !try) {
        /* held spinlocks, then this */
        for (int i = 0; i < percpu_lc[cpu].n; i++) {
            struct lc_held *h = &percpu_lc[cpu].h[i];
            if (sleeping(kind)) {
                if (!r.kind && first_report_at(site))
                    r = (struct lc_report){ .kind = 2, .from = h->cls, .to = c, .held_site = h->site, .site = site };
            }
            if (h->cls == c) same_class(c, h->lock, lock, h->site, site, &r);
            else add_pair(h->cls, c, h->site, site, &r);
        }
        /* the big lock, then a spinlock */
        if (kind == LC_SPIN && pc->bkl_depth) add_pair(CLS_BKL, c, percpu_lc[cpu].bkl_site, site, &r);
        /* held sleeping locks, then this (but never with the big lock) */
        if (t && kind != LC_BKL)
            for (int i = 0; i < t->lc_n; i++) {
                struct lc_held_task *h = &t->lc_held[i];
                if (h->cls == c) same_class(c, h->lock, lock, h->site, site, &r);
                else add_pair(h->cls, c, h->site, site, &r);
            }
    }
    lc_give();

    if (c && kind == LC_SPIN) {
        if (percpu_lc[cpu].n < LC_SPIN_DEPTH)
            percpu_lc[cpu].h[percpu_lc[cpu].n++] = (struct lc_held){ lock, (uint16_t)c, site };
    } else if (c && sleeping(kind) && t) {
        if (t->lc_n < LC_TASK_DEPTH) t->lc_held[t->lc_n++] = (struct lc_held_task){ lock, (uint16_t)c, site };
    }
    if (r.kind) print_report(&r);
    percpu_lc[cpu].inside = false;
    irq_restore(f);
}

void lockcheck_release(void *lock, enum lc_kind kind)
{
    uint64_t f = irq_save();
    int cpu = cpu_id();
    if (cpu < 0) { irq_restore(f); return; }
    if (kind == LC_SPIN) {
        int n = percpu_lc[cpu].n;
        for (int i = n - 1; i >= 0; i--) {
            if (percpu_lc[cpu].h[i].lock != lock) continue;
            memmove(&percpu_lc[cpu].h[i], &percpu_lc[cpu].h[i + 1], (size_t)(n - 1 - i) * sizeof percpu_lc[cpu].h[0]);
            percpu_lc[cpu].n = n - 1;
            break;
        }
    } else {
        struct tcb *t = this_cpu()->current;
        for (int i = t ? t->lc_n - 1 : -1; i >= 0; i--) {
            if (t->lc_held[i].lock != lock) continue;
            memmove(&t->lc_held[i], &t->lc_held[i + 1], (size_t)(t->lc_n - 1 - i) * sizeof t->lc_held[0]);
            t->lc_n--;
            break;
        }
    }
    irq_restore(f);
}

void lockcheck_schedule(uint64_t site)
{
    if (!site) site = (uint64_t)__builtin_return_address(0);
    uint64_t f = irq_save();
    int cpu = cpu_id();
    if (cpu < 0 || percpu_lc[cpu].inside || !percpu_lc[cpu].n) { irq_restore(f); return; }
    percpu_lc[cpu].inside = true;
    struct lc_held h = percpu_lc[cpu].h[percpu_lc[cpu].n - 1];
    lc_take();
    bool first = first_report_at(site);
    lc_give();
    if (first) {
        char a[96], s1[96], s2[96];
        class_str(h.cls, a, sizeof a);
        site_str(h.site, s1, sizeof s1);
        site_str(site, s2, sizeof s2);
        __atomic_add_fetch(&nreports, 1, __ATOMIC_RELAXED);
        kprintf("lockcheck: schedule() from %s with a spinlock held: %s, taken at %s\n", s2, a, s1);
    }
    percpu_lc[cpu].inside = false;
    irq_restore(f);
}

void lockcheck_task_exit(void)
{
    uint64_t f = irq_save();
    int cpu = cpu_id();
    struct tcb *t = cpu >= 0 ? this_cpu()->current : NULL;
    if (t && t->lc_n && !percpu_lc[cpu].inside) {
        percpu_lc[cpu].inside = true;
        char a[96], s1[96];
        class_str(t->lc_held[0].cls, a, sizeof a);
        site_str(t->lc_held[0].site, s1, sizeof s1);
        __atomic_add_fetch(&nreports, 1, __ATOMIC_RELAXED);
        kprintf("lockcheck: pid %d exits holding %d sleeping lock(s): %s, taken at %s\n", t->pid, t->lc_n, a, s1);
        t->lc_n = 0;
        percpu_lc[cpu].inside = false;
    }
    irq_restore(f);
}

void lockcheck_selftest(void)
{
    static struct spinlock a, b;
    lockcheck_name(&a, "self-test A");
    lockcheck_name(&b, "self-test B");
    int before = nreports;
    selftest = true;
    uint64_t f = spin_lock_irqsave(&a), g = spin_lock_irqsave(&b);
    spin_unlock_irqrestore(&b, g);
    spin_unlock_irqrestore(&a, f);
    f = spin_lock_irqsave(&b);                  /* the other order: a cycle */
    g = spin_lock_irqsave(&a);
    spin_unlock_irqrestore(&a, g);
    spin_unlock_irqrestore(&b, f);
    selftest = false;
    bool ok = nreports == before + 1;
    nreports = before;
    kprintf("lockcheck: on; self-test %s\n", ok ? "passed" : "FAILED (no report for a cycle)");
}

size_t lockcheck_gen_stat(char *buf, size_t cap)
{
    return (size_t)snprintf(buf, cap, "lockcheck classes=%d pairs=%d reports=%d\n", nclasses - 1, npairs, nreports);
}
#endif
