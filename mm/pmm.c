/* mm/pmm.c -- bitmap physical frame allocator fed by the Multiboot2 memory map.
 * Its own spinlock (interrupts off while held) guards the bitmap, so frames
 * can be taken and freed without the big kernel lock: page faults resolved
 * outside it (vm.c) do both. Zeroing happens after the lock is dropped. */
#include <kernel/mm.h>
#include <kernel/multiboot2.h>
#include <kernel/string.h>
#include <kernel/klog.h>
#include <kernel/cpu.h>
#include <kernel/spinlock.h>

/* Frames are tracked up to PMM_MAX_PHYS. The bitmap and the reference counts
 * are sized from the memory map at boot and placed in usable RAM below 4 GiB
 * (the boot identity map). RAM above 4 GiB is identity-mapped here, with
 * 2 MiB pages, before any of it is handed out; only whole 2 MiB pages of it
 * are used, so no mapping reaches past the end of a usable range. */
static uint8_t  *bitmap;                     /* 1 = used */
static int32_t  *frame_refs;
static uint64_t nframes;                     /* entries in both */
static uint64_t total_frames, free_frames, highest;
static uint64_t free_low;                    /* of free_frames, those below 4 GiB */
static uint64_t hint_low, hint_high;         /* where single-frame searches start */
static uint64_t low_page;                    /* usable RAM below 640 KiB (AP start-up) */
static struct spinlock lock;                 /* bitmap, counters, hints */

#define LOW_LIMIT   (BOOT_MAP_END / PAGE_SIZE)  /* frames below 4 GiB: DMA32 */
#define FIRST_FRAME (0x100000 / PAGE_SIZE)

uint64_t pmm_low_page(void) { return low_page; }

static inline void set_used(uint64_t f) { bitmap[f >> 3] |=  (uint8_t)(1u << (f & 7)); }
static inline void set_free(uint64_t f) { bitmap[f >> 3] &= (uint8_t)~(1u << (f & 7)); }
static inline bool is_used(uint64_t f)  { return bitmap[f >> 3] & (1u << (f & 7)); }

/* n frames from f on (all on one side of 4 GiB) became free (n > 0) or used */
static inline void count(uint64_t f, int64_t n)
{
    free_frames += (uint64_t)n;
    if (f < BOOT_MAP_END / PAGE_SIZE) free_low += (uint64_t)n;
}

static void reserve_range(uint64_t start, uint64_t end)
{
    for (uint64_t f = start / PAGE_SIZE; f < (end + PAGE_SIZE - 1) / PAGE_SIZE && f < nframes; f++)
        if (!is_used(f)) { set_used(f); count(f, -1); }
}

typedef void (*range_fn)(uint64_t start, uint64_t end, void *arg);

/* each usable range of the memory map, page-aligned inwards */
static void each_usable(uintptr_t mbi, range_fn fn, void *arg)
{
    for (struct mb2_tag *t = mb2_first_tag(mbi); t->type != MB2_TAG_END; t = mb2_next_tag(t)) {
        if (t->type != MB2_TAG_MMAP) continue;
        struct mb2_tag_mmap *mm = (struct mb2_tag_mmap *)t;
        for (uint8_t *p = (uint8_t *)mm->entries; p < (uint8_t *)t + t->size; p += mm->entry_size) {
            struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)p;
            if (e->type != 1) continue;
            uint64_t s = (e->addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            uint64_t end = (e->addr + e->len) & ~(PAGE_SIZE - 1);
            if (end > PMM_MAX_PHYS) end = PMM_MAX_PHYS;
            if (s < end) fn(s, end, arg);
        }
    }
}

/* the part of a range above 4 GiB the allocator may use: whole 2 MiB pages,
 * which is what gets mapped */
static bool high_part(uint64_t *s, uint64_t *end)
{
    if (*s < BOOT_MAP_END) *s = BOOT_MAP_END;
    *s = (*s + 0x1FFFFF) & ~0x1FFFFFull;
    *end &= ~0x1FFFFFull;
    return *s < *end;
}

static void find_highest(uint64_t s, uint64_t end, void *arg)
{
    uint64_t *h = arg, hs = s, he = end;
    if (end > BOOT_MAP_END && high_part(&hs, &he) && he > *h) *h = he;
    if (end > BOOT_MAP_END) end = BOOT_MAP_END;
    if (s < end && end > *h) *h = end;
}

/* where the tables go: low usable RAM clear of the first 1 MiB, the kernel,
 * the boot information and the modules */
struct place { uintptr_t mbi; uint64_t need, at; };

static bool overlaps(uint64_t a, uint64_t n, uint64_t s, uint64_t e) { return a < e && s < a + n; }

static uint64_t clash_end(const struct place *pl, uint64_t a)
{
    uint64_t kend = ((uint64_t)_kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (overlaps(a, pl->need, (uint64_t)_kernel_start, kend)) return kend;
    uint64_t ms = pl->mbi & ~(PAGE_SIZE - 1), me = pl->mbi + ((struct mb2_info *)pl->mbi)->total_size;
    if (overlaps(a, pl->need, ms, me)) return me;
    for (struct mb2_tag *t = mb2_first_tag(pl->mbi); t->type != MB2_TAG_END; t = mb2_next_tag(t))
        if (t->type == MB2_TAG_MODULE) {
            struct mb2_tag_module *m = (struct mb2_tag_module *)t;
            if (overlaps(a, pl->need, m->mod_start, m->mod_end)) return m->mod_end;
        }
    return 0;
}

static void find_place(uint64_t s, uint64_t end, void *arg)
{
    struct place *pl = arg;
    if (pl->at) return;
    if (end > BOOT_MAP_END) end = BOOT_MAP_END;
    for (uint64_t a = s < 0x100000 ? 0x100000 : s; a + pl->need <= end; ) {
        uint64_t c = clash_end(pl, a);
        if (!c) { pl->at = a; return; }
        a = (c + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    }
}

static void add_low(uint64_t s, uint64_t end, void *arg)
{
    (void)arg;
    /* real-mode start-up code for other CPUs: prefer 0x8000, else the
     * lowest usable page above the BIOS data area */
    for (uint64_t a = s < 0x1000 ? 0x1000 : s; a + PAGE_SIZE <= end && a < 0x9F000; a += PAGE_SIZE) {
        if (a == 0x8000) low_page = a;
        else if (!low_page) low_page = a;
    }
    if (end > BOOT_MAP_END) end = BOOT_MAP_END;
    for (uint64_t a = s; a < end; a += PAGE_SIZE) {
        uint64_t f = a / PAGE_SIZE;
        if (is_used(f)) { set_free(f); count(f, 1); total_frames++; }
    }
}

static void map_high(uint64_t s, uint64_t end, void *arg)
{
    bool *ok = arg;
    if (end <= BOOT_MAP_END || !high_part(&s, &end)) return;
    if (!vmm_identity_map(s, end - s, VMM_WRITE)) { *ok = false; return; }
    for (uint64_t a = s; a < end; a += PAGE_SIZE) {
        uint64_t f = a / PAGE_SIZE;
        if (f < nframes && is_used(f)) { set_free(f); count(f, 1); total_frames++; }
    }
}

void pmm_init(uintptr_t mbi)
{
    for (struct mb2_tag *t = mb2_first_tag(mbi); t->type != MB2_TAG_END; t = mb2_next_tag(t)) {
        if (t->type != MB2_TAG_MMAP) continue;
        struct mb2_tag_mmap *mm = (struct mb2_tag_mmap *)t;
        for (uint8_t *p = (uint8_t *)mm->entries; p < (uint8_t *)t + t->size; p += mm->entry_size) {
            struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)p;
            static const char *types[] = { "?", "usable", "reserved", "ACPI reclaim", "ACPI NVS", "bad" };
            kprintf("  mem %016lx-%016lx %s\n", e->addr, e->addr + e->len - 1,
                    types[e->type <= 5 ? e->type : 0]);
        }
    }

    /* 1. size the tables from the highest frame in use, place them low */
    uint64_t top = 0;
    each_usable(mbi, find_highest, &top);
    nframes = (top / PAGE_SIZE + 63) & ~63ull;          /* whole 64-bit words of bitmap */
    struct place pl = { mbi, nframes / 8 + nframes * sizeof(int32_t), 0 };
    pl.need = (pl.need + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    each_usable(mbi, find_place, &pl);
    if (!pl.at) panic("pmm: no room below 4 GiB for the frame tables (%lu KiB)", pl.need >> 10);
    bitmap = (uint8_t *)pl.at;
    frame_refs = (int32_t *)(pl.at + nframes / 8);
    memset(bitmap, 0xFF, nframes / 8);
    memset(frame_refs, 0, nframes * sizeof(int32_t));

    /* 2. RAM below 4 GiB, less what is in use already */
    each_usable(mbi, add_low, NULL);
    reserve_range(0, 0x100000);
    reserve_range((uint64_t)_kernel_start, (uint64_t)_kernel_end);
    reserve_range(mbi, mbi + ((struct mb2_info *)mbi)->total_size);
    reserve_range(pl.at, pl.at + pl.need);
    for (struct mb2_tag *t = mb2_first_tag(mbi); t->type != MB2_TAG_END; t = mb2_next_tag(t))
        if (t->type == MB2_TAG_MODULE) {                /* e.g. the ISO's boot volume */
            struct mb2_tag_module *m = (struct mb2_tag_module *)t;
            reserve_range(m->mod_start, m->mod_end);
        }
    hint_low = FIRST_FRAME;
    hint_high = LOW_LIMIT;
    highest = top < BOOT_MAP_END ? top : BOOT_MAP_END;

    /* 3. RAM above 4 GiB: mapped first (its page tables come from low RAM),
     * then handed to the allocator */
    bool mapped = true;
    each_usable(mbi, map_high, &mapped);
    if (!mapped) kprintf("pmm: out of memory for page tables, some RAM above 4 GiB left out\n");
    highest = top;

    kprintf("pmm: %lu MiB usable, %lu MiB free, %lu MiB above 4 GiB, kernel %p-%p\n",
            total_frames * PAGE_SIZE >> 20, free_frames * PAGE_SIZE >> 20,
            top > BOOT_MAP_END ? (top - BOOT_MAP_END) >> 20 : 0, _kernel_start, _kernel_end);
}

/* A run of n free frames in [lo, hi) that starts at a multiple of `align`
 * frames (a power of two), searched from *hint and wrapping round to lo.
 * Whole words and bytes of the bitmap in use are skipped at once. */
static uint64_t find_run(size_t n, size_t align, uint64_t lo, uint64_t hi, uint64_t *hint)
{
    uint64_t from = *hint >= lo && *hint < hi ? *hint : lo;
    for (int pass = 0; pass < 2; pass++) {
        uint64_t run = 0, start = pass ? lo : from, stop = pass ? from + n : hi;
        if (stop > hi) stop = hi;
        for (uint64_t f = start; f < stop; f++) {
            if (!run && (f & (align - 1))) { f |= align - 1; continue; }   /* to the next start allowed */
            if (!(f & 63) && ((const uint64_t *)bitmap)[f >> 6] == ~0ull) { run = 0; f += 63; continue; }
            if (!(f & 7) && bitmap[f >> 3] == 0xFF) { run = 0; f += 7; continue; }
            if (is_used(f)) { run = 0; continue; }
            if (++run == n) {
                uint64_t first = f + 1 - n;
                for (uint64_t i = first; i <= f; i++) set_used(i);
                count(first, -(int64_t)n);
                if (n == 1) *hint = f + 1;
                return first;
            }
        }
    }
    return 0;
}

/* High memory first, so RAM below 4 GiB stays for devices that need it. */
static uint64_t alloc_run(size_t n, size_t align, bool dma32)
{
    uint64_t top = highest / PAGE_SIZE, low_top = top < LOW_LIMIT ? top : LOW_LIMIT, first = 0;
    uint64_t flags = spin_lock_irqsave(&lock);
    /* a side without n free frames is not searched: once RAM above 4 GiB
     * is used up, each allocation would scan all of it in vain */
    if (!dma32 && top > LOW_LIMIT && free_frames - free_low >= n) first = find_run(n, align, LOW_LIMIT, top, &hint_high);
    if (!first && free_low >= n) first = find_run(n, align, FIRST_FRAME, low_top, &hint_low);
    spin_unlock_irqrestore(&lock, flags);
    if (!first) return 0;
    memset((void *)(first * PAGE_SIZE), 0, n * PAGE_SIZE);
    return first * PAGE_SIZE;
}

uint64_t pmm_alloc_contig(size_t n) { return alloc_run(n, 1, false); }
uint64_t pmm_alloc(void) { return alloc_run(1, 1, false); }

/* Below 4 GiB, for devices that take 32-bit bus addresses (RTL8139). */
uint64_t pmm_alloc_dma32(size_t n) { return alloc_run(n, 1, true); }

/* 2^order frames starting at a multiple of their size, as Linux's buddy
 * allocator hands them out. */
uint64_t pmm_alloc_order(unsigned order, bool dma32) { return alloc_run((size_t)1 << order, (size_t)1 << order, dma32); }

void pmm_free_contig(uint64_t phys, size_t n)
{
    uint64_t flags = spin_lock_irqsave(&lock);
    for (size_t i = 0; i < n; i++) {
        uint64_t f = phys / PAGE_SIZE + i;
        if (f < nframes && is_used(f)) { set_free(f); count(f, 1); }
    }
    uint64_t f = phys / PAGE_SIZE;
    if (f < LOW_LIMIT) { if (f < hint_low) hint_low = f; }
    else if (f < hint_high) hint_high = f;
    spin_unlock_irqrestore(&lock, flags);
}

void pmm_free(uint64_t phys) { pmm_free_contig(phys, 1); }

/* ---- reference counts of frames mapped into user space --------------------
 * A user page can be mapped by several address spaces (fork, copy-on-write)
 * and held by a memory object (shared mappings, files). Each holder counts;
 * the last page_unref() frees the frame. Locked instructions: holders on
 * different CPUs may drop their references at the same time. */
void page_ref(uint64_t phys)
{
    uint64_t f = phys / PAGE_SIZE;
    if (f < nframes) __atomic_add_fetch(&frame_refs[f], 1, __ATOMIC_ACQ_REL);
}

void page_unref(uint64_t phys)
{
    uint64_t f = phys / PAGE_SIZE;
    if (f >= nframes) return;
    int32_t n = __atomic_sub_fetch(&frame_refs[f], 1, __ATOMIC_ACQ_REL);
    if (n == 0) pmm_free(phys & ~(PAGE_SIZE - 1));
    else if (n < 0) panic("page_unref: frame %lx has no references", phys);
}

int page_refs(uint64_t phys)
{
    uint64_t f = phys / PAGE_SIZE;
    return f < nframes ? __atomic_load_n(&frame_refs[f], __ATOMIC_ACQUIRE) : 0;
}

/* A zeroed frame with one reference, for user memory. */
uint64_t page_alloc(void)
{
    uint64_t p = pmm_alloc();
    if (p) __atomic_store_n(&frame_refs[p / PAGE_SIZE], 1, __ATOMIC_RELEASE);
    return p;
}

uint64_t pmm_total_bytes(void)   { return total_frames * PAGE_SIZE; }
uint64_t pmm_free_bytes(void)    { return free_frames * PAGE_SIZE; }
uint64_t pmm_highest_usable(void){ return highest; }
