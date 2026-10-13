/* include/kernel/kestrel_font.h -- the terminals' bitmap fonts
 *
 * 256 glyphs indexed by byte value (Windows-1252 layout). Glyph c's rows are
 * rows[c * height + r], top to bottom; bit 15 of a row is the leftmost
 * pixel. Width and height are the terminal's cell, at most 16 pixels wide.
 *
 * Each monitor's terminal takes the font for its resolution (kfont_for):
 * Kestrel 10 below 1024x768, Kestrel 12 at 1024x768 and above. Both are
 * generated from the glyph sheets in third_party/kestrel_font/ by
 * tools/sheet2font.py (`make fonts`). */
#ifndef KESTREL_FONT_ARRAY_H
#define KESTREL_FONT_ARRAY_H

#include <stdint.h>

struct kfont {
    int width, height;
    const uint16_t *rows;
};

extern const struct kfont kfont_10;     /* drivers/kfont_10.c */
extern const struct kfont kfont_12;     /* drivers/kfont_12.c */
extern const struct kfont kfont_15;     /* drivers/kfont_15.c: the boot manager, proportional */

static inline const struct kfont *kfont_for(int width, int height)
{
    return width < 1024 || height < 768 ? &kfont_10 : &kfont_12;
}

static inline const uint16_t *kfont_glyph(const struct kfont *f, unsigned char c)
{
    return f->rows + (unsigned)c * (unsigned)f->height;
}

#endif
