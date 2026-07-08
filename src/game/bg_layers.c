/* bg_layers.c — faithful port of the in-race parallax background layers that sit
 * between the sky/ground fill (fn10EF_0008) and the road (fn10EF_0051):
 *
 *   décor load   fn15ae_05e2  : depack DEC<level>.CPT into slot 0 of the 8-slot
 *                               décor buffer `_DAT_2000_bc73` (DGROUP 0xDE63).
 *   fn18ef_056c               : generate slots 1..7 as cascading 1-px circular
 *                               LEFT shifts of slot 0 (sub-pixel scroll frames).
 *   mountains    fn10EF_0307  : 16-row horizon band at playfield row 72 (screen
 *                               120..135), 4-plane colour, horizontal scroll +
 *                               4-frame sub-pixel animation from cd60 (0xEF50).
 *   clouds       fn10EF_039F  : 5 stacked parallax bands filling the whole 72-row
 *                               sky (screen 48..119), single plane (Map Mask 2 =
 *                               colour bit 1) over the cyan sky -> white.
 *
 * Layer order in run_level (fn0869_0006): fill, fn10EF_039F (clouds),
 * fn10EF_0307 (mountains), then the road.  Confirmed call sites
 * decompiled.c:1723-1724.
 *
 * EGA -> indexed translation: identical convention to bob.c / ilbm.c — the EGA
 * plane written first (Map Mask 8) is colour bit 3, then 4->bit2, 2->bit1,
 * 1->bit0.  A single-plane write (clouds, Map Mask 2) only toggles colour bit 1.
 */
#include "ff_game.h"
#include "gnames.h"
#include "../asset/ff_assets.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* --- décor buffer (the engine's `_DAT_2000_bc73`, 0xA000 bytes = 8 slots) ---- */
#define DECOR_SLOTS 8
#define SLOT_BYTES  0x1400          /* 5120 = 4 planes x 16 rows x 80 bytes      */
#define STRIP_ROWS  16
#define STRIP_PITCH 80              /* bytes per plane row (640 px, scroll wrap)  */
#define PLANE_BYTES (STRIP_ROWS * STRIP_PITCH)   /* 1280 bytes per plane          */

static u8  g_decor[DECOR_SLOTS * SLOT_BYTES];
static int g_decor_ok = 0;

/* Playfield vertical origin (run_level: tDE57 += 0x780 = 48 rows). Mountains are
 * drawn at tDE57 + 0xB40 (= playfield row 72), i.e. screen row 48 + 72 = 120. */
#define PF_Y          48
#define MTN_SCREEN_Y  (PF_Y + 72)   /* 120 */

/* ------------------------------------------------------------------------- */
/* fn18ef_056c — cascade slot0 into slots 1..7, each a 1-px circular LEFT     */
/* shift of the one before (so slot k = slot0 << k px).  Processes 0x1C0=448  */
/* rows of 80 bytes (= 7 slots x 64 plane-rows), writing the slot one ahead.  */
/* Each 80-byte row is a big-endian bit string; the leftmost pixel (byte 0    */
/* MSB) wraps into the rightmost (byte 79 LSB).                               */
/* ------------------------------------------------------------------------- */
static void decor_shift_cascade(void)
{
    u8 *p9  = g_decor;                 /* pcVar9  = slot 0 */
    u8 *p10 = g_decor + SLOT_BYTES;    /* pcVar10 = slot 1 */
    for (int row = 0; row < 0x1C0; ++row) {
        int carry = (p9[0] >> 7) & 1;            /* wrap: MSB of byte 0 */
        for (int i = 0x4F; i >= 0; --i) {
            int newcarry = (p9[i] >> 7) & 1;
            p10[i] = (u8)((p9[i] << 1) | carry);
            carry = newcarry;
        }
        p9  += 0x50;
        p10 += 0x50;
    }
}

/* ------------------------------------------------------------------------- */
/* décor load (fn15ae_05e2 core): depack DEC<'A'+level>.CPT into slot 0, then  */
/* build the 7 shifted slots.  Demo plays level 0 -> DECA.CPT.                 */
/* ------------------------------------------------------------------------- */
void decor_load(const char *cpt_dir, int level)
{
    char path[300];
    snprintf(path, sizeof path, "%s/DEC%c.CPT", cpt_dir, (char)('A' + level));

    int len = 0;
    u8 *buf = ff_depack_file(path, &len);
    if (!buf || len < SLOT_BYTES) {
        if (buf) free(buf);
        g_decor_ok = 0;
        fprintf(stderr, "warning: decor load failed (%s)\n", path);
        return;
    }
    memcpy(g_decor, buf, SLOT_BYTES);   /* slot 0 = depacked strip */
    free(buf);
    decor_shift_cascade();              /* slots 1..7 */
    g_decor_ok = 1;
}

/* ------------------------------------------------------------------------- */
/* fn10EF_0307 — mountains: 16-row horizon band, full 4-plane colour, scrolled */
/* horizontally over the 640-px (80-byte) source strip.                       */
/*   cd60        = ds->wEF50  (mountain scroll accumulator, init 0xA0)         */
/*   slot        = (cd60 >> 4) & 3        (sub-pixel pre-shift frame)          */
/*   scroll_byte = (cd60 & 0x7fff) >> 7   (byte column offset into the strip)  */
/* Each dest screen row r reads source row r circularly from scroll_byte; the  */
/* band is opaque (all four planes written).                                  */
/* ------------------------------------------------------------------------- */
/* strip-slot mask: run_level selects aF689[(EF50>>4) & 7] (EIGHT sub-pixel
 * shift slots), but the round_transition FINALE loop masks & 3 (ghidra
 * FUN_13a8_048a @3990) — game/cutscene.c switches this for its frames. */
int g_mtn_mask = 7;

void draw_mountains(void)
{
    if (!g_decor_ok) return;

    u16 cd60        = Gw_mtn_scroll;
    int slot        = (cd60 >> 4) & g_mtn_mask;
    int scroll_byte = (cd60 & 0x7FFF) >> 7;
    const u8 *src   = g_decor + (long)slot * SLOT_BYTES;

    for (int r = 0; r < STRIP_ROWS; ++r) {
        u8 *row = ff_fb + (long)(MTN_SCREEN_Y + r) * FF_W;
        const u8 *p0 = src + 0 * PLANE_BYTES + r * STRIP_PITCH;
        const u8 *p1 = src + 1 * PLANE_BYTES + r * STRIP_PITCH;
        const u8 *p2 = src + 2 * PLANE_BYTES + r * STRIP_PITCH;
        const u8 *p3 = src + 3 * PLANE_BYTES + r * STRIP_PITCH;
        for (int d = 0; d < 40; ++d) {
            int s = (scroll_byte + d) % STRIP_PITCH;
            u8 b0 = p0[s], b1 = p1[s], b2 = p2[s], b3 = p3[s];
            for (int k = 0; k < 8; ++k) {
                u8 m = (u8)(0x80 >> k);
                int c = 0;
                if (b0 & m) c |= 8;     /* Map Mask 8 -> colour bit 3 */
                if (b1 & m) c |= 4;     /* Map Mask 4 -> colour bit 2 */
                if (b2 & m) c |= 2;     /* Map Mask 2 -> colour bit 1 */
                if (b3 & m) c |= 1;     /* Map Mask 1 -> colour bit 0 */
                row[d * 8 + k] = (u8)c; /* opaque band */
            }
        }
    }
}

/* ========================================================================== */
/* Clouds — fn120d_0bc7 (texture generator) + fn13a8_01c9 (band tables) +      */
/* fn10EF_039F (renderer).                                                     */
/*                                                                             */
/* The cloud texture is generated ONCE from BOB sprite #0x18 (the 64x72 cloud  */
/* tile): fn120d_0bc7 transposes/tiles it into a 128x576, 4-plane buffer        */
/* (`_DAT_2000_d00f`, 0x9000 bytes; plane size 0x2400, 16 bytes/row).  The      */
/* renderer reads the buffer at +0x4800 (plane 2 = the tile's colour-2 puff     */
/* mask) and writes screen plane 1 (Map Mask 2) -> colour 6 (white) over the    */
/* cyan sky (colour 4).  Five contiguous parallax bands fill the 72-row sky.    */
/* ========================================================================== */
#define CTEX_BYTES  0x9000
#define CTEX_PLANE  0x2400          /* one plane (128 px wide x 576 rows / 16B) */

static u8  g_cloud[CTEX_BYTES];
static int g_cloud_ok = 0;

/* fn120d_0bc7 + fn1987_191b (src read) + fn1987_199d (dst write): build the
 * cloud texture from sprite #0x18.  Faithful pixel-for-pixel transliteration:
 * 8 tiles, each src row -> one canvas row, each src pixel written at dst_x and
 * dst_x+64 (wrap 128); dst_x base = tile index, dst_y runs 0..575. */
static void cloud_gen(void)
{
    int sw = 0, sh = 0;
    const u8 *spr = ff_bob_data(GC.bob, 0x18, &sw, &sh);   /* sprite 24 = 64x72 */
    if (!spr || sw <= 0 || sh <= 0) { g_cloud_ok = 0; return; }

    memset(g_cloud, 0, sizeof g_cloud);                    /* fn1987_17ae clear */
    long src_rb    = sw >> 3;                              /* 8 bytes/plane row  */
    long src_plane = src_rb * sh;                          /* 576 = src plane sz */

    int dst_y = 0;
    for (int tile = 0; tile < 8; ++tile) {
        for (int sy = 0; sy < sh; ++sy) {
            int dst_x = tile;
            for (int sx = 0; sx < sw; ++sx) {
                long sb = (long)sy * src_rb + (sx >> 3);
                u8   sm = (u8)(0x80 >> (sx & 7));
                int color = 0;                             /* fn1987_191b */
                if (spr[sb]                & sm) color |= 8;
                if (spr[sb + src_plane]    & sm) color |= 4;
                if (spr[sb + 2*src_plane]  & sm) color |= 2;
                if (spr[sb + 3*src_plane]  & sm) color |= 1;

                int xs[2] = { dst_x, (dst_x + 64) & 127 }; /* fn1987_199d x2 */
                for (int t = 0; t < 2; ++t) {
                    int dx = xs[t];
                    long bo = (long)dst_y * 16 + (dx >> 3);
                    u8   dm = (u8)(0x80 >> (dx & 7));
                    for (int p = 0; p < 4; ++p) {          /* plane p = bit (3-p) */
                        if ((color >> (3 - p)) & 1) g_cloud[p*CTEX_PLANE + bo] |=  dm;
                        else                        g_cloud[p*CTEX_PLANE + bo] &= ~dm;
                    }
                }
                dst_x += 1;
            }
            ++dst_y;
        }
    }
    g_cloud_ok = 1;
}

/* fn10EF_039F — 5 stacked parallax cloud bands over the sky.  Band heights and
 * texture base offsets are the code-segment constants (a04A9 + the 0x180/0x2A0/
 * 0x370/0x410 deltas from fn13a8_01c9). Per band the scroll index selects a
 * 64-entry table slot: offset = col + row*0x480 with col=(e>>3), row=7-(e&7),
 * e = ((accum>>8)&0xFC)>>2.  Each screen row reads 8 bytes (64-px plane-2 mask)
 * at +0x4800, tiled 5x; single-plane (bit 1) replace over the cyan sky. */
void draw_clouds(void)
{
    if (!g_cloud_ok) { cloud_gen(); if (!g_cloud_ok) return; }

    static const int heights[5]  = { 24, 18, 13, 10, 7 };
    static const long band_add[5]= { 0, 0x180, 0x2A0, 0x370, 0x410 };
    const u16 accum[5] = { Gw_cloud_acc0, Gw_cloud_acc1, Gw_cloud_acc2,
                           Gw_cloud_acc3, Gw_cloud_acc4 };

    int screen_y = PF_Y;                       /* clouds start at screen row 48 */
    for (int b = 0; b < 5; ++b) {
        int e   = ((accum[b] >> 8) & 0xFC) >> 2;   /* table entry 0..63 */
        int col = e >> 3;
        int row = 7 - (e & 7);
        long src_base = (long)col + (long)row * 0x480 + band_add[b] + 0x4800;

        for (int j = 0; j < heights[b]; ++j) {
            const u8 *m = g_cloud + src_base + (long)j * 16;
            u8 *fbrow = ff_fb + (long)(screen_y + j) * FF_W;
            for (int px = 0; px < FF_W; ++px) {
                int sp = px & 63;                          /* 64-px period x5 */
                int cb = (m[sp >> 3] >> (7 - (sp & 7))) & 1;
                fbrow[px] = (u8)((fbrow[px] & ~2) | (cb << 1));
            }
        }
        screen_y += heights[b];
    }
}
