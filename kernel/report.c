/* kernel/report.c -- REPORT.TXT, VBIOS.ROM and RESULTS.TAR on the stick's
 * report volume
 *
 * FAT handling is deliberately minimal and read-only for the metadata: the
 * boot sector (BPB) and root directory are read, the FAT is only followed to
 * find which sectors belong to a file, and nothing but those data sectors is
 * ever written. FAT16 and FAT32 are understood (Microsoft FAT specification,
 * sections 3-6). */
#include <kernel/report.h>
#include <kernel/block.h>
#include <kernel/klog.h>
#include <kernel/mm.h>
#include <kernel/string.h>
#include <kernel/arch.h>
#include <kernel/bootinfo.h>
#include <kernel/posix.h>
#include <kernel/time.h>

#define REPORT_LABEL    "KESTREL RPT"
#define MAX_RUNS        32              /* extents per file (we create 1) */
#define MAX_SECTIONS    8
#define IO_CHUNK        (64 * 1024)

struct run { uint64_t lba; uint32_t count; };       /* sectors, partition relative */

struct fat_file {
    bool       found;
    uint32_t   size;                    /* bytes, from the directory entry */
    int        nruns;
    struct run run[MAX_RUNS];
};

struct section { const char *title; size_t (*fn)(char *, size_t); };

static struct blkdev  *vol;             /* partition (or whole disk) holding the volume */
static struct fat_file f_report, f_vbios, f_results;
static struct section sections[MAX_SECTIONS];
static int            nsections;
static const void    *blob;
static size_t         blob_len;
static const char    *blob_what;
static char           where[96] = "no report volume found";

void report_add_section(const char *title, size_t (*fn)(char *, size_t))
{
    if (nsections < MAX_SECTIONS) sections[nsections++] = (struct section){ title, fn };
}

void report_set_blob(const void *data, size_t len, const char *what)
{
    blob = data; blob_len = len; blob_what = what;
}

const char *report_where(void) { return where; }

/* ======================================================================== */
/*  FAT lookup                                                                 */
/* ======================================================================== */
static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static inline uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

struct fat_vol {
    struct blkdev *dev;
    int      bits;                      /* 16 or 32 */
    uint32_t spc;                       /* sectors per cluster */
    uint64_t fat_lba, root_lba, data_lba;
    uint32_t root_secs;                 /* FAT16 fixed root directory */
    uint32_t root_cluster;              /* FAT32 */
    uint32_t clusters;
    uint8_t *sec;                       /* one-sector scratch (DMA safe) */
    uint64_t cached_fat_lba;
};

static uint64_t cluster_lba(const struct fat_vol *v, uint32_t c) { return v->data_lba + (uint64_t)(c - 2) * v->spc; }

/* FAT entry of cluster c, or 0xFFFFFFFF on a read error. */
static uint32_t fat_next(struct fat_vol *v, uint32_t c)
{
    uint32_t off = c * (v->bits / 8);
    uint64_t lba = v->fat_lba + off / 512;
    if (lba != v->cached_fat_lba) {
        if (blk_read(v->dev, lba, 1, v->sec) < 0) return 0xFFFFFFFFu;
        v->cached_fat_lba = lba;
    }
    uint32_t e = v->bits == 16 ? rd16(v->sec + off % 512) : rd32(v->sec + off % 512) & 0x0FFFFFFFu;
    if (v->bits == 16 && e >= 0xFFF8) e = 0x0FFFFFFF;                 /* end of chain, FAT32 style */
    return e;
}

static bool is_eoc(uint32_t e) { return e >= 0x0FFFFFF8u; }

/* Cluster chain -> sector runs, at most `max_bytes` worth. */
static bool chain_to_runs(struct fat_vol *v, uint32_t first, uint32_t max_bytes, struct fat_file *f)
{
    uint32_t need = (max_bytes + v->spc * 512 - 1) / (v->spc * 512), c = first;
    f->nruns = 0;
    for (uint32_t i = 0; i < need; i++) {
        if (c < 2 || c >= v->clusters + 2) return false;               /* free, reserved or bad */
        uint64_t lba = cluster_lba(v, c);
        struct run *r = f->nruns ? &f->run[f->nruns - 1] : NULL;
        if (r && r->lba + r->count == lba) r->count += v->spc;
        else {
            if (f->nruns == MAX_RUNS) return false;                    /* too fragmented */
            f->run[f->nruns++] = (struct run){ lba, v->spc };
        }
        if (i + 1 < need) {
            uint32_t n = fat_next(v, c);
            if (n == 0xFFFFFFFFu || is_eoc(n)) return false;           /* chain shorter than the size */
            c = n;
        }
    }
    return true;
}

/* Look for the 8.3 names in one directory sector. */
static void scan_dir_sector(struct fat_vol *v, const uint8_t *s, bool *end)
{
    for (int i = 0; i < 512 && !*end; i += 32) {
        const uint8_t *d = s + i;
        if (d[0] == 0x00) { *end = true; return; }
        if (d[0] == 0xE5 || (d[11] & 0x0F) == 0x0F || (d[11] & 0x18)) continue;   /* deleted, LFN, dir, label */
        struct fat_file *f = !memcmp(d, "REPORT  TXT", 11) ? &f_report : !memcmp(d, "VBIOS   ROM", 11) ? &f_vbios :
                             !memcmp(d, "RESULTS TAR", 11) ? &f_results : NULL;
        if (!f || f->found) continue;
        uint32_t first = rd16(d + 26) | (v->bits == 32 ? (uint32_t)rd16(d + 20) << 16 : 0);
        f->size = rd32(d + 28);
        f->found = f->size && chain_to_runs(v, first, f->size, f);
        if (!f->found) kprintf("report: %.11s on %s is empty or fragmented, not used\n", (const char *)d, v->dev->name);
    }
}

static bool try_volume(struct blkdev *b, uint8_t *sec)
{
    if (b->block_size != 512 || blk_read(b, 0, 1, sec) < 0) return false;
    if (sec[510] != 0x55 || sec[511] != 0xAA || (sec[0] != 0xEB && sec[0] != 0xE9)) return false;
    if (rd16(sec + 11) != 512) return false;
    uint32_t spc = sec[13], reserved = rd16(sec + 14), nfats = sec[16], root_entries = rd16(sec + 17);
    if (!spc || (spc & (spc - 1)) || !reserved || !nfats || nfats > 2) return false;
    uint32_t total = rd16(sec + 19) ? rd16(sec + 19) : rd32(sec + 32);
    uint32_t fatsz = rd16(sec + 22) ? rd16(sec + 22) : rd32(sec + 36);
    uint32_t root_secs = (root_entries * 32 + 511) / 512;
    uint64_t meta = (uint64_t)reserved + (uint64_t)nfats * fatsz + root_secs;
    if (!total || !fatsz || total > b->blocks || meta >= total) return false;
    uint32_t clusters = (uint32_t)((total - meta) / spc);
    if (clusters < 4085) return false;                                 /* FAT12: never ours */
    int bits = clusters < 65525 ? 16 : 32;
    const uint8_t *label = sec + (bits == 16 ? 43 : 71);
    if (memcmp(label, REPORT_LABEL, 11) != 0) return false;           /* not the Kestrel report volume */

    struct fat_vol v = {
        .dev = b, .bits = bits, .spc = spc, .clusters = clusters,
        .fat_lba = reserved, .root_lba = reserved + (uint64_t)nfats * fatsz, .root_secs = root_secs,
        .data_lba = meta, .root_cluster = bits == 32 ? rd32(sec + 44) : 0, .cached_fat_lba = ~0ull,
    };
    uint8_t *s = kmalloc(512 * 2);
    if (!s) return false;
    v.sec = s + 512;
    memset(&f_report, 0, sizeof f_report);
    memset(&f_vbios, 0, sizeof f_vbios);
    memset(&f_results, 0, sizeof f_results);
    bool end = false;
    if (bits == 16) {
        for (uint32_t i = 0; i < root_secs && !end; i++)
            if (blk_read(b, v.root_lba + i, 1, s) == 0) scan_dir_sector(&v, s, &end);
    } else {
        uint32_t c = v.root_cluster;
        for (int guard = 0; !end && c >= 2 && !is_eoc(c) && guard < 64; guard++, c = fat_next(&v, c))
            for (uint32_t i = 0; i < spc && !end; i++)
                if (blk_read(b, cluster_lba(&v, c) + i, 1, s) == 0) scan_dir_sector(&v, s, &end);
    }
    kfree(s);
    if (!f_report.found && !f_vbios.found) return false;
    snprintf(where, sizeof where, "%s (FAT%d \"" REPORT_LABEL "\"): %s%s%s%s", b->name, bits,
             f_report.found ? "REPORT.TXT" : "", f_report.found && f_vbios.found ? ", " : "",
             f_vbios.found ? "VBIOS.ROM" : "", f_results.found ? ", RESULTS.TAR" : "");
    return true;
}

bool report_locate(void)
{
    uint8_t *sec = kmalloc(512);
    if (!sec) return false;
    vol = NULL;
    for (int pass = 0; pass < 2 && !vol; pass++)        /* USB sticks first, then any disk */
        for (int i = 0; i < blk_count() && !vol; i++) {
            struct blkdev *b = blk_get(i), *disk = blk_disk_of(b);
            if (b->removed || (pass == 0) != (disk->kind == BLK_DISK_USB)) continue;
            if (try_volume(b, sec)) vol = b;
        }
    kfree(sec);
    if (vol) kprintf("report: report volume on %s\n", where);
    return vol != NULL;
}

/* ======================================================================== */
/*  writing                                                                    */
/* ======================================================================== */

/* Overwrite file `f` with data[0..len), padded with `pad` to the file size
 * (data beyond the file size is cut). */
static int write_file(const struct fat_file *f, const uint8_t *data, size_t len, uint8_t pad)
{
    uint8_t *buf = kmalloc(IO_CHUNK);
    if (!buf) return -ENOMEM;
    size_t pos = 0, total = ((size_t)f->size + 511) & ~(size_t)511;
    int rc = 0;
    for (int r = 0; r < f->nruns && pos < total && rc == 0; r++) {
        uint64_t lba = f->run[r].lba;
        size_t left = (size_t)f->run[r].count * 512;
        while (left && pos < total && rc == 0) {
            size_t n = left < IO_CHUNK ? left : IO_CHUNK;
            if (n > total - pos) n = total - pos;
            for (size_t i = 0; i < n; i++) buf[i] = pos + i < len ? data[pos + i] : pad;
            rc = blk_write(vol, lba, (uint32_t)(n / 512), buf);
            lba += n / 512; left -= n; pos += n;
        }
    }
    kfree(buf);
    return rc;
}

static size_t header(char *buf, size_t cap, const char *reason)
{
    uint64_t ms = uptime_ms();
    return (size_t)snprintf(buf, cap,
        "Kestrel diagnostic report (%s)\n"
        "==========================================================================\n"
        "kernel     %s %s (%s), built " __DATE__ " " __TIME__ "\n"
        "uptime     %lu.%03lu s\n"
        "firmware   %s, loader %s\n"
        "cmdline    %s\n"
        "cpu        %s / %s\n",
        reason, KESTREL_NAME, KESTREL_VERSION, KESTREL_MACHINE, ms / 1000, ms % 1000,
        g_boot.uefi ? "UEFI" : "BIOS", g_boot.loader, g_boot.cmdline[0] ? g_boot.cmdline : "(empty)",
        g_boot.cpu_vendor, g_boot.cpu_brand);
}

int report_save(const char *reason)
{
    if (!vol && !report_locate()) return -ENOENT;
    if (!f_report.found) return -ENOENT;

    size_t cap = f_report.size;
    size_t frames = (cap + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t pa = pmm_alloc_contig(frames);
    if (!pa) return -ENOMEM;
    char *buf = (char *)(uintptr_t)pa;
    size_t n = header(buf, cap, reason);
    for (int i = 0; i < nsections && n < cap; i++) {
        n += (size_t)snprintf(buf + n, cap - n, "\n== %s ==\n", sections[i].title);
        if (n < cap) n += sections[i].fn(buf + n, cap - n);
    }
    if (n < cap) n += (size_t)snprintf(buf + n, cap - n, "\n== kernel log (%lu bytes) ==\n", (uint64_t)klog_size());
    if (n < cap) n += klog_read(0, buf + n, cap - n);
    if (n < cap) n += (size_t)snprintf(buf + n, cap - n, "\n== end of report ==\n");
    if (n > cap) n = cap;

    int rc = write_file(&f_report, (const uint8_t *)buf, n, ' ');
    pmm_free_contig(pa, frames);
    if (rc == 0 && f_vbios.found && blob)
        rc = write_file(&f_vbios, blob, blob_len, 0xFF);
    if (rc == 0) rc = blk_flush(vol);
    if (rc == 0)
        kprintf("report: wrote REPORT.TXT (%lu bytes)%s%s to %s\n", (uint64_t)n,
                f_vbios.found && blob ? " and VBIOS.ROM: " : "", f_vbios.found && blob ? blob_what : "", vol->name);
    else
        kprintf("report: writing to %s failed: %s\n", vol->name, blk_strerror(rc));
    return rc;
}

/* ---- /dev/report: text from programs ---------------------------------------- */
#define USER_TEXT_MAX   (32 * 1024)
static char   user_text[USER_TEXT_MAX];
static size_t user_len;
static bool   user_section;

static size_t user_section_text(char *buf, size_t cap)
{
    size_t n = user_len < cap ? user_len : cap;
    memcpy(buf, user_text, n);
    return n;
}

long report_user_write(const void *buf, size_t len)
{
    if (!user_section) {
        report_add_section("written by programs (/dev/report)", user_section_text);
        user_section = true;
    }
    size_t k = len < USER_TEXT_MAX - user_len ? len : USER_TEXT_MAX - user_len;
    memcpy(user_text + user_len, buf, k);
    user_len += k;
    int rc = report_save("written through /dev/report");
    return rc ? rc : (long)len;
}

/* ---- /dev/results: a tar archive of test results ------------------------------ */
/* Sector `sec` of file f: its LBA and how many sectors follow it in the same
 * run, or false past the end. */
static bool file_sector(const struct fat_file *f, uint64_t sec, uint64_t *lba, uint32_t *left)
{
    for (int r = 0; r < f->nruns; r++) {
        if (sec < f->run[r].count) {
            *lba = f->run[r].lba + sec;
            *left = (uint32_t)(f->run[r].count - sec);
            return true;
        }
        sec -= f->run[r].count;
    }
    return false;
}

static uint64_t results_end;            /* the furthest byte written since the last close */

long report_results_write(const void *buf, size_t len, uint64_t off)
{
    if (!vol && !report_locate()) return -ENODEV;
    if (!f_results.found) return -ENODEV;
    if (off >= f_results.size) return len ? -ENOSPC : 0;
    if (len > f_results.size - off) len = (size_t)(f_results.size - off);
    uint8_t *bounce = kmalloc(IO_CHUNK);
    if (!bounce) return -ENOMEM;
    const uint8_t *src = buf;
    uint64_t pos = off, end = off + len;
    int rc = 0;
    while (pos < end && rc == 0) {
        uint64_t sec = pos / 512, lba;
        uint32_t left;
        if (!file_sector(&f_results, sec, &lba, &left)) { rc = -ENOSPC; break; }
        uint32_t nsec = (uint32_t)((end - sec * 512 + 511) / 512);
        if (nsec > IO_CHUNK / 512) nsec = IO_CHUNK / 512;
        if (nsec > left) nsec = left;
        uint64_t first = sec * 512, last = first + (uint64_t)nsec * 512;
        size_t skip = (size_t)(pos - first), n = (size_t)((end < last ? end : last) - pos);
        if (skip || skip + n < (size_t)nsec * 512)          /* a partial sector: keep what is there */
            rc = blk_read(vol, lba, nsec, bounce);
        if (rc == 0) {
            memcpy(bounce + skip, src + (pos - off), n);
            rc = blk_write(vol, lba, nsec, bounce);
        }
        pos += n;
    }
    kfree(bounce);
    if (rc) {
        kprintf("report: writing RESULTS.TAR on %s failed: %s\n", vol->name, blk_strerror(rc));
        return rc;
    }
    if (end > results_end) results_end = end;
    return (long)len;
}

/* Last close: two zero blocks after the furthest write end the archive there
 * even if the writer stopped early (tar writes its own), then a flush. */
void report_results_close(void)
{
    if (!vol || !f_results.found) return;
    static const uint8_t zero[1024];
    uint64_t at = (results_end + 511) & ~511ull;
    if (results_end && at < f_results.size) report_results_write(zero, sizeof zero, at);
    if (results_end) {
        blk_flush(vol);
        kprintf("report: RESULTS.TAR: %lu bytes written to %s\n", results_end, vol->name);
    }
    results_end = 0;
}
