/* linux/list_sort.h -- Kestrel's Linux KPI: sorting lists (linuxkpi/src/sort.c): stable, a merge sort */
#ifndef _LKPI_LINUX_LIST_SORT_H
#define _LKPI_LINUX_LIST_SORT_H

#include <linux/types.h>

struct list_head;
typedef int (*list_cmp_func_t)(void *priv, const struct list_head *a, const struct list_head *b);
void list_sort(void *priv, struct list_head *head, list_cmp_func_t cmp);

#endif
