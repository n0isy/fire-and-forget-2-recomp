/* ff_font.h — EGA 8x8 text blitter (faithful port of fn1187_035F / fn1187_0232). */
#ifndef FF_FONT_H
#define FF_FONT_H
#include <stdint.h>

/* Load the font block extracted from the EXE code segment (assets/font_ega.bin).
 * Returns 0 on success. */
int  ff_font_load(const char *path);

/* Draw `len` characters of `str` at pixel (x,y) (x must be byte-aligned / mult of
 * 8) using font variant `color` (0..3). Writes RAW EGA indices into ff_fb; the
 * displayed colour comes from the current screen palette. */
void ff_font_draw(const char *str, int len, int x, int y, int color);

/* Same glyph blit (fn1187_0232), but into an arbitrary 320x200 index buffer
 * `dst` instead of ff_fb. Used by the persistent HUD-cockpit overlay, which
 * mirrors the original's VRAM page (incremental gauge draws accumulate). */
void ff_font_draw_buf(uint8_t *dst, const char *str, int len, int x, int y, int color);

#endif
