/* third_party/x86emu/shim: the few C library names x86emu uses, from the kernel */
#pragma once
#include <stdarg.h>
#include <kernel/klog.h>
#include <kernel/string.h>
/* to the serial port (kernel/vbe_emu.c): the emulator runs in a panic */
void printk(const char *fmt, ...);
#define printf printk
