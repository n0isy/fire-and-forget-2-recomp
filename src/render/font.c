/* font.c — faithful port of the EGA text blitter fn1187_035F / fn1187_0232
 * (Ghidra FUN_1987_035f @ 1987:035F, FUN_1987_0232 @ 1987:0232).
 *
 * The original draws an 8x8x4-plane glyph string straight into EGA video memory:
 *   - puVar = page + (x>>3) + y*0x28          (x is byte-aligned, 8 px / char)
 *   - colour selects a font variant baked into the code segment:
 *       iRam0001041b = 0x1D; if(c!=0){c==1?0x5DD:c==2?0x6BD:0xC3D}
 *   - glyph = cs:[base + char*0x20]; 4 planes via Sequencer Map Mask (0x3C4/0x3C5)
 *     written 8,4,2,1 -> planes 3,2,1,0, eight rows each (stride 0x28).
 *   - the write is opaque (each plane store overwrites), so background pixels in
 *     the 8x8 cell become index 0.
 * fn1187_0232 additionally copies the drawn span to the other EGA page (double
 * buffer); we render to a single framebuffer, so that step is omitted.
 *
 * Here the framebuffer (ff_fb) holds RAW EGA plane indices; the displayed colour
 * comes from the current screen palette (the ATC the last image load programmed).
 * The glyph's colour is therefore baked into the variant's plane data exactly as
 * in the original. The font block is extracted from the EXE code segment into
 * assets/font_ega.bin (CS offset 0; file offset = CS + 0xA670). */
#include <stdio.h>
#include <stdlib.h>
#include "ff_font.h"
#include "../platform/platform.h"

static uint8_t *g_font;      /* CS font block (font_ega.bin) */
static long     g_font_len;

int ff_font_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END); g_font_len = ftell(f); fseek(f, 0, SEEK_SET);
    g_font = (uint8_t *)malloc((size_t)g_font_len);
    if (!g_font) { fclose(f); return 1; }
    if (fread(g_font, 1, (size_t)g_font_len, f) != (size_t)g_font_len) {
        free(g_font); g_font = NULL; fclose(f); return 1;
    }
    fclose(f);
    return 0;
}

/* faithful colour -> font-variant base (the decompiled switch) */
static unsigned font_base(int color)
{
    unsigned base = 0x1D;
    if (color != 0) base = (color == 1) ? 0x5DD : (color == 2) ? 0x6BD : 0xC3D;
    return base;
}

/* Draw `len` chars of `str` at (x,y), x byte-aligned, into the index buffer
 * `dst` (320x200). color picks the variant. Writes each 8x8 cell OPAQUELY
 * (background pixels become index 0), exactly like the original VRAM write. */
void ff_font_draw_buf(uint8_t *dst, const char *str, int len, int x, int y, int color)
{
    if (!g_font || !dst) return;
    unsigned base = font_base(color);
    int bx = x >> 3;                                   /* byte column */
    for (int i = 0; i < len; ++i) {
        unsigned ch = (unsigned char)str[i];
        long goff = (long)base + (long)ch * 0x20;
        if (goff + 0x20 > g_font_len) { ++bx; continue; }
        const uint8_t *g = g_font + goff;
        for (int row = 0; row < 8; ++row) {
            int py = y + row;
            if (py < 0 || py >= FF_H) continue;
            for (int col = 0; col < 8; ++col) {
                int v = 0;                              /* Map Mask 8,4,2,1 -> plane 3,2,1,0 */
                for (int b = 0; b < 4; ++b)
                    if (g[b * 8 + row] & (0x80 >> col)) v |= (1 << (3 - b));
                int px = bx * 8 + col;
                if (px < 0 || px >= FF_W) continue;
                dst[(long)py * FF_W + px] = (uint8_t)v;      /* opaque (overwrite) */
            }
        }
        ++bx;                                            /* advance 8 px */
    }
}

/* Draw `len` chars of `str` at (x,y), x byte-aligned, into ff_fb. */
void ff_font_draw(const char *str, int len, int x, int y, int color)
{
    ff_font_draw_buf(ff_fb, str, len, x, y, color);
}
