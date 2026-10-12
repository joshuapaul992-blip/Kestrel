/* linuxkpi/src/sort.c -- sorting arrays and lists (include/linux/sort.h, list_sort.h)
 *
 * sort() is a heapsort: no recursion, no allocation, O(n log n); not stable,
 * as in Linux. list_sort() is a bottom-up merge sort, which is stable. */
#include <linux/sort.h>
#include <linux/list_sort.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/export.h>

static void sort_swap(void *a, void *b, size_t size, swap_func_t swap)
{
    if (swap) { swap(a, b, (int)size); return; }
    while (size--) {
        char t = *(char *)a;
        *(char *)a++ = *(char *)b;
        *(char *)b++ = t;
    }
}

struct sort_cmp { cmp_func_t cmp; cmp_r_func_t cmp_r; const void *priv; };

static int sort_do_cmp(const struct sort_cmp *c, const void *a, const void *b)
{
    return c->cmp_r ? c->cmp_r(a, b, c->priv) : c->cmp(a, b);
}

static void sort_common(void *base, size_t num, size_t size, const struct sort_cmp *c, swap_func_t swap)
{
    char *b = base;
    size_t start, end, root, child;

    if (num < 2) return;
    /* heapify: sift down every parent, last first */
    for (start = num / 2; start-- > 0;) {
        for (root = start; (child = 2 * root + 1) < num; root = child) {
            if (child + 1 < num && sort_do_cmp(c, b + child * size, b + (child + 1) * size) < 0) child++;
            if (sort_do_cmp(c, b + root * size, b + child * size) >= 0) break;
            sort_swap(b + root * size, b + child * size, size, swap);
        }
    }
    /* move the largest to the end, shrink the heap, sift the new root down */
    for (end = num - 1; end > 0; end--) {
        sort_swap(b, b + end * size, size, swap);
        for (root = 0; (child = 2 * root + 1) < end; root = child) {
            if (child + 1 < end && sort_do_cmp(c, b + child * size, b + (child + 1) * size) < 0) child++;
            if (sort_do_cmp(c, b + root * size, b + child * size) >= 0) break;
            sort_swap(b + root * size, b + child * size, size, swap);
        }
    }
}

void sort(void *base, size_t num, size_t size, cmp_func_t cmp, swap_func_t swap)
{
    struct sort_cmp c = { .cmp = cmp };
    sort_common(base, num, size, &c, swap);
}
EXPORT_SYMBOL(sort);

void sort_r(void *base, size_t num, size_t size, cmp_r_func_t cmp, swap_func_t swap, const void *priv)
{
    struct sort_cmp c = { .cmp_r = cmp, .priv = priv };
    sort_common(base, num, size, &c, swap);
}
EXPORT_SYMBOL(sort_r);

/* merge two NULL-terminated singly linked runs (through ->next), a before b on ties */
static struct list_head *list_merge(void *priv, list_cmp_func_t cmp, struct list_head *a, struct list_head *b)
{
    struct list_head *head = NULL, **tail = &head;

    while (a && b) {
        if (cmp(priv, a, b) <= 0) { *tail = a; a = a->next; }
        else { *tail = b; b = b->next; }
        tail = &(*tail)->next;
    }
    *tail = a ? a : b;
    return head;
}

void list_sort(void *priv, struct list_head *head, list_cmp_func_t cmp)
{
    struct list_head *runs[64] = { NULL };     /* runs[i]: a sorted run of 2^i entries, or NULL */
    struct list_head *list, *result = NULL, *prev;
    int i, max = 0;

    if (list_empty(head) || head->next == head->prev) return;
    head->prev->next = NULL;
    list = head->next;
    while (list) {
        struct list_head *cur = list;
        list = list->next;
        cur->next = NULL;
        for (i = 0; i < 64 && runs[i]; i++) {
            cur = list_merge(priv, cmp, runs[i], cur);
            runs[i] = NULL;
        }
        if (i == 64) i = 63;
        runs[i] = cur;
        if (i > max) max = i;
    }
    for (i = 0; i <= max; i++)
        if (runs[i]) result = result ? list_merge(priv, cmp, runs[i], result) : runs[i];
    /* rebuild the prev links and close the circle */
    prev = head;
    for (list = result; list; list = list->next) { list->prev = prev; prev->next = list; prev = list; }
    prev->next = head;
    head->prev = prev;
}
EXPORT_SYMBOL(list_sort);
