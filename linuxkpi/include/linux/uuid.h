/* linux/uuid.h -- Kestrel's Linux KPI: 128-bit GUIDs and UUIDs */
#ifndef _LKPI_LINUX_UUID_H
#define _LKPI_LINUX_UUID_H

#include <linux/types.h>
#include <linux/string.h>
#include <linux/random.h>

#define UUID_SIZE       16

typedef struct { __u8 b[UUID_SIZE]; } guid_t;
typedef struct { __u8 b[UUID_SIZE]; } uuid_t;

static inline bool guid_equal(const guid_t *a, const guid_t *b) { return !memcmp(a, b, sizeof *a); }
static inline void guid_copy(guid_t *dst, const guid_t *src) { memcpy(dst, src, sizeof *dst); }
static inline void import_guid(guid_t *dst, const __u8 *src) { memcpy(dst, src, sizeof *dst); }
static inline void import_uuid(uuid_t *dst, const __u8 *src) { memcpy(dst, src, sizeof *dst); }
static inline void export_guid(__u8 *dst, const guid_t *src) { memcpy(dst, src, sizeof *src); }
static inline bool guid_is_null(const guid_t *g)
{
    for (int i = 0; i < UUID_SIZE; i++) if (g->b[i]) return false;
    return true;
}
/* a random (version 4) GUID */
static inline void guid_gen(guid_t *g)
{
    get_random_bytes(g, sizeof *g);
    g->b[7] = (g->b[7] & 0x0f) | 0x40;
    g->b[8] = (g->b[8] & 0x3f) | 0x80;
}
static inline bool uuid_equal(const uuid_t *a, const uuid_t *b) { return !memcmp(a, b, sizeof *a); }
static inline void uuid_copy(uuid_t *dst, const uuid_t *src) { memcpy(dst, src, sizeof *dst); }
static inline bool uuid_is_null(const uuid_t *u)
{
    for (int i = 0; i < UUID_SIZE; i++) if (u->b[i]) return false;
    return true;
}

#endif
