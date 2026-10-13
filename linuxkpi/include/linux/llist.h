/* linux/llist.h -- Kestrel's Linux KPI: lock-free singly linked lists
 *
 * Any number of threads may add at once; taking the whole list
 * (llist_del_all) is the only removal that needs no lock. */
#ifndef _LKPI_LINUX_LLIST_H
#define _LKPI_LINUX_LLIST_H

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/container_of.h>
#include <linux/stddef.h>

struct llist_node { struct llist_node *next; };
struct llist_head { struct llist_node *first; };

#define LLIST_HEAD_INIT(name)           { NULL }
#define LLIST_HEAD(name)                struct llist_head name = LLIST_HEAD_INIT(name)
static inline void init_llist_head(struct llist_head *list) { list->first = NULL; }
static inline void init_llist_node(struct llist_node *node) { node->next = node; }
static inline bool llist_on_list(const struct llist_node *node) { return node->next != node; }
static inline bool llist_empty(const struct llist_head *head) { return READ_ONCE(head->first) == NULL; }
static inline struct llist_node *llist_next(struct llist_node *node) { return node->next; }

/* add new_first..new_last (already linked); true if the list was empty */
static inline bool llist_add_batch(struct llist_node *new_first, struct llist_node *new_last, struct llist_head *head)
{
    struct llist_node *first = READ_ONCE(head->first);
    do {
        new_last->next = first;
    } while (!__atomic_compare_exchange_n(&head->first, &first, new_first, false, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
    return !first;
}
static inline bool llist_add(struct llist_node *new, struct llist_head *head) { return llist_add_batch(new, new, head); }
static inline struct llist_node *llist_del_all(struct llist_head *head)
{
    return __atomic_exchange_n(&head->first, NULL, __ATOMIC_ACQUIRE);
}
static inline struct llist_node *llist_del_first(struct llist_head *head)
{
    struct llist_node *entry = READ_ONCE(head->first), *next;
    do {
        if (!entry) return NULL;
        next = READ_ONCE(entry->next);
    } while (!__atomic_compare_exchange_n(&head->first, &entry, next, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED));
    return entry;
}
static inline struct llist_node *llist_reverse_order(struct llist_node *head)
{
    struct llist_node *new_head = NULL;
    while (head) { struct llist_node *tmp = head; head = head->next; tmp->next = new_head; new_head = tmp; }
    return new_head;
}

#define llist_entry(ptr, type, member)  container_of(ptr, type, member)
#define member_address_is_nonnull(ptr, member) ((uintptr_t)(ptr) + offsetof(typeof(*(ptr)), member) != 0)
#define llist_for_each(pos, node)       for ((pos) = (node); pos; (pos) = (pos)->next)
#define llist_for_each_safe(pos, n, node) \
    for ((pos) = (node); (pos) && ((n) = (pos)->next, true); (pos) = (n))
#define llist_for_each_entry(pos, node, member)                                 \
    for ((pos) = llist_entry((node), typeof(*(pos)), member);                   \
         member_address_is_nonnull(pos, member);                                \
         (pos) = llist_entry((pos)->member.next, typeof(*(pos)), member))
#define llist_for_each_entry_safe(pos, n, node, member)                         \
    for (pos = llist_entry((node), typeof(*pos), member);                       \
         member_address_is_nonnull(pos, member) &&                              \
            (n = llist_entry(pos->member.next, typeof(*n), member), true);      \
         pos = n)

#endif
