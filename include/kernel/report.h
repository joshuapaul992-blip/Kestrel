/* include/kernel/report.h -- diagnostic report written to the boot stick
 *
 * Machines without a serial port still need a way to hand a log to a
 * developer. tools/mkusbimg.py puts a small FAT volume labelled
 * "KESTREL RPT" on the stick with two preallocated files; this module finds
 * that volume on any block device, follows the files' cluster chains and
 * overwrites their contents in place:
 *
 *   REPORT.TXT   header (version, firmware, command line, CPU), every
 *                registered section (e.g. the NVIDIA report, text written
 *                to /dev/report) and the complete kernel log
 *   VBIOS.ROM    a registered binary blob (the graphics card's video BIOS)
 *   RESULTS.TAR  what programs write to /dev/results: a tar archive of
 *                test results (kestrel-selftest)
 *
 * File sizes, the FAT and the directory are never changed, so the volume
 * stays valid for every OS. Only a volume with exactly that label is
 * touched. */
#ifndef KESTREL_REPORT_H
#define KESTREL_REPORT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* A section of REPORT.TXT: fn fills buf (at most cap bytes) and returns the
 * length. Sections appear in registration order. */
void report_add_section(const char *title, size_t (*fn)(char *buf, size_t cap));
/* Contents of VBIOS.ROM (the pointer must stay valid). */
void report_set_blob(const void *data, size_t len, const char *what);

/* Find the report volume (boot: after USB storage has settled). */
bool report_locate(void);
/* Write REPORT.TXT (and VBIOS.ROM if a blob is set). Returns 0 or -errno. */
int  report_save(const char *reason);
/* One line for the shell: where the report goes, or why it cannot. */
const char *report_where(void);
/* /dev/report: text written there by programs (the benchmark's results)
 * is kept, at most 32 KiB in all, as a section of REPORT.TXT, which is
 * rewritten on every write. Returns len or -errno. */
long report_user_write(const void *buf, size_t len);
/* /dev/results: bytes written at `off` go to RESULTS.TAR at that offset, so
 * `tar -cf /dev/results DIR` puts an archive of test results on the stick.
 * Returns the bytes written or -errno (-ENOSPC past the file's end).
 * report_results_close() ends the archive and flushes (last close). */
long report_results_write(const void *buf, size_t len, uint64_t off);
void report_results_close(void);

#endif
