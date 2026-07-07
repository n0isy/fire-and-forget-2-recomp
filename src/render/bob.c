/* bob.c — BOB sprite bank (Titus "Blitter OBject") for the FF2 port.
 *
 * Implements BobBank + ff_bob_load/free/count/dims/blit from ff_assets.h.
 *
 * Format (docs/bob_format.md, cross-checked with tools/bob_extract.py):
 *   file:
 *     +0   u16   sprite_count                 (= 236 for BOB)
 *     +2   sprites back-to-back, no alignment
 *   sprite record (little-endian):
 *     +0   u16   height H (rows)              (= [ptr-4] in the engine)
 *     +2   u16   width  W (pixels, multiple of 16)(= [ptr-2] in the engine)
 *     +4   planar data, length = (W/2)*H bytes:
 *          4 EGA planes in a row (plane0 first), plane size = H*(W/8).
 *          a plane row has W/8 bytes, the byte's MSB = leftmost pixel.
 *     color(x,y) = p0 | p1<<1 | p2<<2 | p3<<3;  color 0 = transparent.
 *   stride to next sprite = 4 + (W/2)*H bytes.
 *
 * stdio/stdlib/string only.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../asset/ff_assets.h"      /* declarations + ff_depack_file */
#include "../platform/platform.h"    /* FF_W, FF_H, ff_fb */

struct BobBank {
    uint8_t *buf;        /* depacked bank (owns the memory) */
    int      len;        /* buffer length */
    int      count;      /* number of sprites */
    long    *data_off;   /* [count] offset to pixel data (after the dim words) */
    int     *w;          /* [count] width */
    int     *h;          /* [count] height */
};

static unsigned rd_u16(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

/* Does the string end with the given suffix (case-insensitive). */
static int ends_with_ci(const char *s, const char *suf)
{
    size_t ls = strlen(s), lf = strlen(suf);
    if (lf > ls) return 0;
    const char *a = s + (ls - lf);
    for (size_t i = 0; i < lf; i++) {
        int ca = a[i], cb = suf[i];
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
    }
    return 1;
}

/* Parse the depacked bank buffer into a BobBank (takes ownership of buf). */
static BobBank *bob_parse(uint8_t *buf, int len)
{
    if (!buf || len < 2) { free(buf); return NULL; }

    int count = (int)rd_u16(buf);
    if (count <= 0) { free(buf); return NULL; }

    BobBank *b = (BobBank *)calloc(1, sizeof(*b));
    if (!b) { free(buf); return NULL; }
    b->data_off = (long *)malloc((size_t)count * sizeof(long));
    b->w        = (int  *)malloc((size_t)count * sizeof(int));
    b->h        = (int  *)malloc((size_t)count * sizeof(int));
    if (!b->data_off || !b->w || !b->h) {
        free(b->data_off); free(b->w); free(b->h); free(b); free(buf);
        return NULL;
    }

    long p = 2;
    for (int i = 0; i < count; i++) {
        if (p + 4 > len) {                       /* header does not fit */
            free(b->data_off); free(b->w); free(b->h); free(b); free(buf);
            return NULL;
        }
        int h = (int)rd_u16(buf + p);
        int w = (int)rd_u16(buf + p + 2);
        long doff = p + 4;
        long dlen = (long)(w >> 1) * h;          /* (W/2)*H */
        if (doff + dlen > len) {                 /* pixels do not fit */
            free(b->data_off); free(b->w); free(b->h); free(b); free(buf);
            return NULL;
        }
        b->data_off[i] = doff;
        b->w[i] = w;
        b->h[i] = h;
        p = doff + dlen;
    }

    b->buf   = buf;
    b->len   = len;
    b->count = count;
    return b;
}

/* Parse an in-memory bank image (same layout as a depacked BOB.CPT: u16 count
 * then sprite records). Takes ownership of buf (freed on error/ff_bob_free).
 * Used by the boot shape generator (fn0A0D_133B), which BUILDS a bank. */
BobBank *ff_bob_parse_mem(uint8_t *buf, int len)
{
    return bob_parse(buf, len);
}

BobBank *ff_bob_load(const char *path)
{
    if (!path) return NULL;

    uint8_t *buf = NULL;
    int len = 0;

    if (ends_with_ci(path, ".bin")) {
        /* already-depacked bank — read it as-is */
        FILE *f = fopen(path, "rb");
        if (!f) return NULL;
        if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
        long fl = ftell(f);
        if (fl < 0) { fclose(f); return NULL; }
        if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
        buf = (uint8_t *)malloc((size_t)fl ? (size_t)fl : 1);
        if (!buf) { fclose(f); return NULL; }
        if (fread(buf, 1, (size_t)fl, f) != (size_t)fl) {
            free(buf); fclose(f); return NULL;
        }
        fclose(f);
        len = (int)fl;
    } else {
        /* PP20 .CPT — depack it */
        buf = ff_depack_file(path, &len);
        if (!buf) return NULL;
    }

    return bob_parse(buf, len);   /* takes ownership of buf, frees it on error */
}

void ff_bob_free(BobBank *b)
{
    if (!b) return;
    free(b->buf);
    free(b->data_off);
    free(b->w);
    free(b->h);
    free(b);
}

int ff_bob_count(const BobBank *b)
{
    return b ? b->count : 0;
}

void ff_bob_dims(const BobBank *b, int id, int *w, int *h)
{
    if (!b || id < 0 || id >= b->count) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    if (w) *w = b->w[id];
    if (h) *h = b->h[id];
}

/* Raw planar pixel data for sprite id (4 EGA planes back-to-back, plane size
 * = H*(W/8), first plane = colour bit 3). Returns NULL if id is out of range. */
const uint8_t *ff_bob_data(const BobBank *b, int id, int *w, int *h)
{
    if (!b || id < 0 || id >= b->count) {
        if (w) *w = 0;
        if (h) *h = 0;
        return NULL;
    }
    if (w) *w = b->w[id];
    if (h) *h = b->h[id];
    return b->buf + b->data_off[id];
}

/* Top clip row for sprite blits — the display-list blit fn1987_1c73 clips the
 * sprite to the PLAYFIELD window (negative playfield rows skip source rows,
 * ghidra FUN_1987_1c73 @2446-2455; bottom rows beyond t24F3 dropped @2456-62).
 * sprite_dl.c dl_flush() sets this to the playfield origin (48) around the
 * display-list draw so rising sprites (the finale rocket) never overwrite the
 * HUD rows; 0 (whole screen) everywhere else. */
int ff_blit_clip_top = 0;

/* fn1187_1C73 SHIFT-6 LEADING-BYTE QUIRK — a GENUINE ROM BUG, proven by ndisasm
 * of the 8 per-alignment shift-buffer builders (jump table @1187:1E20). Each
 * builder writes the shifted DATA + the inverse-opacity MASK; the mask is
 * initialized by the first plane (MOV) and accumulated over planes 2-4 with
 * AND... in SEVEN of the eight routines. The shift-6 routine alone (@1187:216B)
 * uses MOV for the LEADING partial byte in the accumulation block too
 * (@21F3 `mov [es:bp+di+0x2],ah` vs shift-5's @212B / shift-7's @22B3
 * `and [es:bp+di+0x2],ah`), so its final leading-byte mask = ~(last plane's
 * data) = ~(colour-bit-0 plane) instead of ~(p3|p2|p1|p0). TWO of the eight
 * routines carry the bug: shift 6 (@216B, leading 2 px) AND shift 7 (@2231,
 * leading 1 px); shift 5 (@209D) accumulates correctly (`and` @212B) and
 * shifts 1-4 fold the leading pixels into the AND-accumulated first WORD.
 * Effect: for a display-list sprite whose LEFT edge lands at screen
 * x%8 ∈ {6,7}, the (8 - x%8) leading pixel columns are cleared only where
 * the texel has BIT 0 — an opaque EVEN-coloured texel there is OR-merged
 * with the background (out = bg | texel). Found by the unfiltered pixel
 * tests: the crash particles showed bg|8 outline pixels in the original
 * (shift 6: f1267/f2547/f3117; shift 7: f3307/f3308). Applied only under
 * ff_blit_quirk6 (the 1C73-modeled draw windows) and only for
 * unclipped-left sprites (x >= 0; the left-clip path takes a different
 * leading route in the ROM). */
int ff_blit_quirk6 = 0;

/* Draw sprite id into ff_fb, top-left corner (x,y).
 * Color 0 is transparent; clipped to the FF_W x FF_H screen. */
void ff_bob_blit(const BobBank *b, int id, int x, int y)
{
    if (!b || id < 0 || id >= b->count) return;

    int w = b->w[id];
    int h = b->h[id];
    if (w <= 0 || h <= 0) return;

    const uint8_t *data = b->buf + b->data_off[id];
    long row_bytes  = w >> 3;            /* W/8 bytes per plane row */
    long plane_size = row_bytes * h;     /* H*(W/8) bytes per plane */
    /* shift-6/7 leading-byte ROM quirk (see ff_blit_quirk6 above): the sprite's
     * leading (8 - x%8) columns OR even-coloured texels over the background. */
    int qn = (ff_blit_quirk6 && x >= 0 && (x & 7) >= 6) ? 8 - (x & 7) : 0;

    for (int sy = 0; sy < h; sy++) {
        int dy = y + sy;
        if (dy < ff_blit_clip_top || dy >= FF_H) continue;   /* playfield top clip (fn1987_1c73) */
        long row_base = (long)sy * row_bytes;
        uint8_t *fb_row = ff_fb + (long)dy * FF_W;

        for (int sx = 0; sx < w; sx++) {
            int dx = x + sx;
            if (dx < 0 || dx >= FF_W) continue;

            long byte_idx = row_base + (sx >> 3);
            uint8_t mask  = (uint8_t)(0x80 >> (sx & 7));

            /* EGA plane order follows the engine's Map Mask convention (8,4,2,1
             * = planes 3,2,1,0; first stored plane -> colour bit 3), same as the
             * text blitter fn1187_0232. */
            int c = 0;
            if (data[byte_idx]                  & mask) c |= 8;
            if (data[byte_idx + plane_size]     & mask) c |= 4;
            if (data[byte_idx + 2 * plane_size] & mask) c |= 2;
            if (data[byte_idx + 3 * plane_size] & mask) c |= 1;

            if (c) {                              /* color 0 — transparent */
                if (sx < qn && !(c & 1)) fb_row[dx] |= (uint8_t)c;  /* leading-byte MOV-mask bug */
                else                     fb_row[dx]  = (uint8_t)c;
            }
        }
    }
}

/* Draw sprite id as a SOLID single-colour silhouette (every non-transparent
 * pixel -> `color`), top-left corner (x,y), clipped. Faithful port of the flash
 * path in the low-level blit fn1987_1c73 (@1987:24a1-24f6): after copying the
 * sprite's opacity mask, its OR pass sets EGA SEQ Map Mask = (t0016 - 1) instead
 * of looping the 4 planes, so the whole opaque area is written as the single
 * colour index (t0016 - 1). run_level (fn1069_0006 @1774) passes t0016 = d013
 * (=0xd hit / 10 respawn), so the silhouette colour = d013 - 1 (0xC red / 9). */
void ff_bob_blit_recolor(const BobBank *b, int id, int x, int y, int color)
{
    if (!b || id < 0 || id >= b->count) return;

    int w = b->w[id];
    int h = b->h[id];
    if (w <= 0 || h <= 0) return;

    const uint8_t *data = b->buf + b->data_off[id];
    long row_bytes  = w >> 3;
    long plane_size = row_bytes * h;

    for (int sy = 0; sy < h; sy++) {
        int dy = y + sy;
        if (dy < ff_blit_clip_top || dy >= FF_H) continue;   /* playfield top clip (fn1987_1c73) */
        long row_base = (long)sy * row_bytes;
        uint8_t *fb_row = ff_fb + (long)dy * FF_W;

        for (int sx = 0; sx < w; sx++) {
            int dx = x + sx;
            if (dx < 0 || dx >= FF_W) continue;

            long byte_idx = row_base + (sx >> 3);
            uint8_t mask  = (uint8_t)(0x80 >> (sx & 7));

            int opaque = (data[byte_idx]                  & mask)
                       | (data[byte_idx + plane_size]     & mask)
                       | (data[byte_idx + 2 * plane_size] & mask)
                       | (data[byte_idx + 3 * plane_size] & mask);
            if (opaque) fb_row[dx] = (uint8_t)color;   /* mask -> solid colour */
        }
    }
}
