/* include/kernel/sysvshm.h -- System V shared memory (kernel/sysvshm.c) */
#ifndef KESTREL_SYSVSHM_H
#define KESTREL_SYSVSHM_H

#include <stdint.h>
#include <stddef.h>

/* The system calls, with Linux's arguments and results (-errno) */
int64_t sysv_shmget(int32_t key, uint64_t size, int flags);
int64_t sysv_shmat(int id, uint64_t addr, int flags);
int64_t sysv_shmdt(uint64_t addr);
int64_t sysv_shmctl(int id, int cmd, uint64_t ubuf);
/* /proc/sysvipc/shm */
size_t  sysv_shm_proc(char *buf, size_t cap);

#endif
