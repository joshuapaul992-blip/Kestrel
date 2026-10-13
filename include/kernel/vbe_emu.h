/* include/kernel/vbe_emu.h -- VBE calls into the video BIOS, run in an x86
 * emulator (third_party/x86emu) instead of in real mode: the CPU stays in
 * long mode, with its interrupts, page tables and the other CPUs as they
 * are, and the BIOS reaches the hardware through port I/O and the low
 * megabyte of memory, as it would in real mode. Used by the panic screen. */
#ifndef KESTREL_VBE_EMU_H
#define KESTREL_VBE_EMU_H

#include <stdint.h>
#include <stdbool.h>

struct vbe_mode {
    uint16_t number;
    uint16_t width, height;
    uint32_t pitch;
    uint8_t  bpp;
    uint8_t  r_pos, r_size, g_pos, g_size, b_pos, b_size;
    uint32_t phys;                      /* linear frame buffer */
};

/* Copy page 0 (the real-mode interrupt vectors and the BIOS data area)
 * before it becomes the kernel's NULL guard page. */
void vbe_emu_save_low(void);

/* Set a linear frame buffer mode of w x h, 32 bpp or else 24, through
 * VBE 4F00/4F01/4F02. False if there is no video BIOS to call (a UEFI
 * boot), no such mode, or the BIOS did not finish within a few seconds. */
bool vbe_emu_set_mode(uint16_t w, uint16_t h, struct vbe_mode *out);

#endif
