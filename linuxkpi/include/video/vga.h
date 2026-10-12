/* video/vga.h -- Kestrel's Linux KPI: VGA registers and the legacy frame buffer window */
#ifndef _LKPI_VIDEO_VGA_H
#define _LKPI_VIDEO_VGA_H

#define VGA_FB_PHYS_BASE        0xA0000         /* legacy frame buffer window */
#define VGA_FB_PHYS_SIZE        65536
#define VGA_ATT_W               0x3C0           /* attribute controller, index and data */
#define VGA_ATT_R               0x3C1
#define VGA_MIS_W               0x3C2           /* miscellaneous output, write */
#define VGA_MIS_R               0x3CC
#define VGA_MIS_COLOR           0x01            /* I/O at 0x3Dx instead of 0x3Bx */
#define VGA_IS1_RC              0x3DA           /* input status 1 (colour); reading it resets the attribute flip-flop */
#define VGA_SEQ_I               0x3C4
#define VGA_SEQ_D               0x3C5
#define VGA_GFX_I               0x3CE
#define VGA_GFX_D               0x3CF
#define VGA_CRT_IC              0x3D4
#define VGA_CRT_DC              0x3D5

#endif
