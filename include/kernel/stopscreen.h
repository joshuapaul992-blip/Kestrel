/* include/kernel/stopscreen.h -- the kernel panic screen (kernel/stopscreen.c) */
#ifndef KESTREL_STOPSCREEN_H
#define KESTREL_STOPSCREEN_H

/* Draws the red stop screen with `msg` on its STOP line, at 800x600 where
 * the display can switch, else scaled to the whole screen. Interrupts off,
 * the other CPUs stopped. */
void stop_screen(const char *msg);

#endif
