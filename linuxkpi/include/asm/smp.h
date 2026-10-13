/* asm/smp.h -- Kestrel's Linux KPI: x86 SMP helpers */
#ifndef _LKPI_ASM_SMP_H
#define _LKPI_ASM_SMP_H
#include <asm/cpufeature.h>

int wbinvd_on_all_cpus(void);          /* 0 */

#endif
