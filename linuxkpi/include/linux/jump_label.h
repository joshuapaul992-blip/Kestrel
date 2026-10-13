/* linux/jump_label.h -- Kestrel's Linux KPI: static keys
 *
 * Linux patches the branch into the code; here a key is an atomic count
 * that is tested at run time. */
#ifndef _LKPI_LINUX_JUMP_LABEL_H
#define _LKPI_LINUX_JUMP_LABEL_H

#include <linux/atomic.h>
#include <linux/compiler.h>

struct static_key { atomic_t enabled; };
struct static_key_true { struct static_key key; };
struct static_key_false { struct static_key key; };
#define STATIC_KEY_INIT_TRUE            { .enabled = ATOMIC_INIT(1) }
#define STATIC_KEY_INIT_FALSE           { .enabled = ATOMIC_INIT(0) }
#define DEFINE_STATIC_KEY_TRUE(name)    struct static_key_true name = { STATIC_KEY_INIT_TRUE }
#define DEFINE_STATIC_KEY_FALSE(name)   struct static_key_false name = { STATIC_KEY_INIT_FALSE }
#define DECLARE_STATIC_KEY_TRUE(name)   extern struct static_key_true name
#define DECLARE_STATIC_KEY_FALSE(name)  extern struct static_key_false name
#define static_key_enabled(k)           (atomic_read(&(k)->key.enabled) > 0)
#define static_branch_likely(k)         likely(static_key_enabled(k))
#define static_branch_unlikely(k)       unlikely(static_key_enabled(k))
#define static_branch_enable(k)         atomic_set(&(k)->key.enabled, 1)
#define static_branch_disable(k)        atomic_set(&(k)->key.enabled, 0)
#define static_branch_inc(k)            atomic_inc(&(k)->key.enabled)
#define static_branch_dec(k)            atomic_dec(&(k)->key.enabled)

#endif
