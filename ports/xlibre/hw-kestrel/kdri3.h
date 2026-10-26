/* hw/kestrel/kdri3.h -- DRI3 for Xkestrel (kdri3.c) */
#ifndef KDRI3_H
#define KDRI3_H

#include "scrnintstr.h"

/* After fbScreenInit: DRI3 on the first render node, if there is one */
Bool kDri3Init(ScreenPtr screen);

#endif
