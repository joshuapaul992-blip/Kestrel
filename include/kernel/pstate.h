/* include/kernel/pstate.h -- the processor's clock (arch/x86_64/pstate.c) */
#ifndef KESTREL_PSTATE_H
#define KESTREL_PSTATE_H

#include <stdint.h>

void     pstate_init_cpu(void);         /* each CPU as it starts; the boot CPU first */
void     pstate_tick(void);             /* each local timer tick                    */
uint32_t pstate_mhz(int cpu);           /* its clock while busy, last second; 0: unknown */
const char *pstate_desc(void);          /* how the clock is set (REPORT.TXT)        */

#endif
