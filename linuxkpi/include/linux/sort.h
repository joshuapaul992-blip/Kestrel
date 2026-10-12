/* linux/sort.h -- Kestrel's Linux KPI: sorting arrays (linuxkpi/src/sort.c) */
#ifndef _LKPI_LINUX_SORT_H
#define _LKPI_LINUX_SORT_H

#include <linux/types.h>

typedef void (*swap_func_t)(void *a, void *b, int size);
typedef int (*cmp_func_t)(const void *a, const void *b);
typedef int (*cmp_r_func_t)(const void *a, const void *b, const void *priv);
void sort(void *base, size_t num, size_t size, cmp_func_t cmp, swap_func_t swap);
void sort_r(void *base, size_t num, size_t size, cmp_r_func_t cmp, swap_func_t swap, const void *priv);

#endif
