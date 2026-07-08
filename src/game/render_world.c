/* render_world.c -- faithful transliteration of Fire & Forget II's in-race
 * background renderer (sky + perspective road + roadside scenery hook).
 *
 * Ported, line-for-line where the decompiled source permits, from:
 *   fn0869_158D (0869:158D)  -- build perspective LUTs E4CC/DC84 from base @0x2CD5
 *   fn0869_15D6 (0869:15D6)  -- "world_road_perspective": fill the coord table @0x2C51
 *                               (a2C51 entry 0 + a2C53 entries 1..15) for 16 depth
 *                               levels from the perspective LUT @0xE4CC and player x.
 *   fn10EF_0008 (10EF:0008)  -- background fill (sky band + ground band).
 *   fn10EF_0051 (10EF:0051)  -- road polygon rasteriser: walks the coord table and
 *                               draws each depth band as a trapezoid of horizontal
 *                               spans, with per-band centre dashes / side rumble.
 *   helpers 10EF:00F7/017E (edge interpolation), 10EF:01C0 (span fill),
 *           10EF:0222/0252/0291 (centre dash / side stripes).
 *
 * All DGROUP accesses go through gmem.h G-accessors (GW/GB/GI/GPTR), keeping every
 * constant, offset and table index identical to the decompiled binary.
 *
 * EGA -> indexed translation
 * --------------------------
 * The original writes 4 EGA bit-planes to A000:xxxx through the Sequencer map-mask
 * and the Graphics-Controller Set/Reset + Bit-Mask registers.  Our framebuffer
 * ff_fb is INDEXED (one byte = colour 0..15).  Every planar fill in these routines
 * is a *solid* Set/Reset write (all four planes via map-mask 0x0F, enable-set/reset
 * 0x0F), so the colour written equals the Set/Reset register value.  We therefore
 * translate each fill into a direct index write of that Set/Reset colour at the
 * identical pixel x,y, exactly as bob.c combines planes into an index for sprites:
 *      sky    = Set/Reset 0x04 (fn10EF_0008, GC 0x0400)
 *      ground = Set/Reset 0x02 (fn10EF_0008, GC 0x0200)
 *      road   = Set/Reset 0x0E (fn10EF_0051, GC 0x0E00)  -> colour 14
 *      dash   = Set/Reset 0x06 (fn10EF_0222, GC 0x0600)  -> colour 6
 *      stripe = Set/Reset 0x0A (fn10EF_0252, GC 0x0A00)  -> colour 10
 * The per-scanline span coordinates and the depth/Y math are kept identical; the
 * GC Bit-Mask edge read-modify-write (left/right partial bytes) collapses to an
 * inclusive integer span [left,right] at the same pixels.
 *
 * Level track curvature
 * ---------------------
 * fn0869_15D6 reads the road curve profile through the far pointer ds->tF6CD
 * (set up by fn0869_158D from ds->ptr33D2).  That pointer addresses the level's
 * track segment in a SEPARATE segment (0de1), outside the DGROUP image — the
 * real curve bytes live in the EXE and are extracted to assets/track.bin.  The
 * track module below loads them and walks tF6CD (fn0869_154D) so the road bends
 * exactly like the original instead of staying straight.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "../ff2.h"     /* platform (ff_fb, FF_W, FF_H), globals, types */
#include "gnames.h"     /* semantic names of the scalar state fields */
#include "ff_game.h"    /* g_persp_scale/g_persp_halfw runtime LUT blocks */
#include "data/gamedata.h"   /* ffd_track_* / ffd_persp_ramp */

/* parallax background layers (game/bg_layers.c), drawn between fill and road */
void draw_clouds(void);
void draw_mountains(void);

/* ---- road track curve data (the level's curve profile) ------------------- *
 * fn0869_15D6 reads the per-band curve through ds->tF6CD = ptr33D2[0]. The
 * curve bytes are the ffd_track_* data (7 contiguous 0x7F-terminated segments
 * + the FarPtr[12] segment table + the 7-byte tail), assembled at boot into
 * the flat g_track image so the ORIGINAL offset walk (tF6CD one byte per
 * fn0869_154D step, curve[tF6CD+0..14] per band, segment jumps through the
 * table) runs unchanged — only the fetch is redirected out of G. */
#define TRACK_BASE 0x31DD                /* the region's original DGROUP home */
#define TRACK_LEN  (0x3409 - 0x31DD)     /* segments + seg-ptr table + tail   */
static u8 g_track[TRACK_LEN];
static u8  trk_b(int off) { return g_track[off - TRACK_BASE]; }
static u16 trk_w(int off) { return (u16)(trk_b(off) | ((u16)trk_b(off + 1) << 8)); }
static u32 trk_d(int off) { return (u32)trk_w(off) | ((u32)trk_w(off + 2) << 16); }

/* assemble the flat track image from the named data (boot; game_main.c) */
void track_seed(void)
{
    static const struct { u16 off, len; const void *src; } ts[9] = {
        { 0x31DD, 0x21, ffd_track_31dd }, { 0x31FE, 0x30, ffd_track_31fe },
        { 0x322E, 0x4E, ffd_track_322e }, { 0x327C, 0x6C, ffd_track_327c },
        { 0x32E8, 0x30, ffd_track_32e8 }, { 0x3318, 0x4E, ffd_track_3318 },
        { 0x3366, 0x6C, ffd_track_3366 }, { 0x33D2, 0x30, ffd_track_seg_ptrs },
        { 0x3402, 0x07, ffd_track_tbl_tail },
    };
    for (int i = 0; i < 9; ++i)
        memcpy(g_track + (ts[i].off - TRACK_BASE), ts[i].src, ts[i].len);
}

static int g_tF6CD = 0x31DD;              /* the curve pointer (original offsets) */
static int g_prev_phase = -1;

void track_load(const char *path) { (void)path; }   /* curve is data now */
void track_reset(void)
{
    g_tF6CD = (int)trk_w(0x33D2);
    Gw_track_ptr = (u16)g_tF6CD;            /* keep the tF6CD field live:
                                           * the camera reads the curve through it */
    g_prev_phase = -1;
}

/* fn0869_154D — advance the curve pointer one step; on the 0x7F segment-end
 * marker (byte at tF6CD+0x0F) jump to ptr33D2[wF480], wrapping on a null entry. */
void track_advance(void)
{
    ++g_tF6CD;
    if (trk_b(g_tF6CD + 0x0F) == 0x7F) {
        u16 wF480 = Gw_track_seg;
        if (trk_d(0x33D2 + wF480 * 4) == 0) wF480 = 0;         /* null entry -> wrap */
        g_tF6CD = (int)trk_w(0x33D2 + wF480 * 4);              /* ptr33D2[wF480].off */
        Gw_track_seg = (u16)(wF480 + 1);
    }
    Gw_track_ptr = (u16)g_tF6CD;            /* mirror into the tF6CD field */
}

/* signed curve byte for band offset c (0..14) at the current pointer */
static int track_curve(int c) { return (i8)trk_b(g_tF6CD + c); }

/* current road-curve byte (tF6CD.u1)->b0000 — used by the car-lean column. */
int track_curve_now(void) { return (i8)trk_b(g_tF6CD); }
/* current track pointer (diagnostics: compare vs the QEMU tF6CD trace). */
int track_pos(void) { return g_tF6CD; }

/* fn1069_0006 frame gate: advance the curve pointer when (c84c & 0x30) changes
 * (local_4a != uVar5).  c84c = tEA3C (0xEA3C).  Called once per frame.
 * local_4a is an UNINITIALIZED stack local, so the very first comparison sees
 * garbage != phase and ADVANCES once on the first frame — measured on the QEMU
 * reference (tF6CD 0x31DD -> 0x31DE between frames 1 and 2). Without this the
 * whole track lags one advance and the second segment jump lands with wF480=5
 * (straight) instead of 4 (the 0x31FE curve segment) — the road never bends. */
void track_step(void)
{
    int phase = Gw_dist_lo & 0x30;
    if (g_prev_phase < 0 || g_prev_phase != phase) {
        track_advance();
        g_prev_phase = phase;
    }
}

/* ---- DGROUP offsets (verbatim from the decompiled source) ---------------- */
#define OFF_BASE    0x2CD5   /* 11477: base perspective table, 256 bytes            */
/* the derived perspective LUTs (were the G byte regions DC84/E4CC) */
u8 g_persp_scale[0x100];   /* E4CC */
u8 g_persp_halfw[0x100];   /* DC84 */
/* THE ROAD COORD TABLE (de-DGROUP'd): was a2C51 @0x2C51 — 16 depth levels x 4
 * columns at a 0x20-byte column stride. RoadCoords lays the columns out with
 * the SAME stride (16 i16 each; center[] carries the a2CB1 alias + the 2-entry
 * trailing spill), so one flat index reproduces ENT(k,c) = 0x2C51+2k+0x20c. */
RoadCoords g_road;

/* Set/Reset colours, as programmed by the EGA draw routines (see header). */
enum { COL_GROUND = 0x02, COL_SKY = 0x04, COL_DASH = 0x06, COL_ROAD = 0x0E, COL_STRIPE = 0x0A };

/* Playfield vertical origin: run_level (fn0869_0006) does ds->tDE57 += 0x0780
 * (1920 bytes = 48 rows), so the whole playfield (sky/mountains/road, 0x98=152
 * rows) is drawn starting at screen row 48, leaving rows 0..47 for the HUD panel.
 * Our coord table / fill math stays in playfield-local Y (0..151); we add this
 * offset only when writing the linear framebuffer. */
#define PF_Y 48

/* coord-table CELL (an i16 lvalue) — k = depth level, c = column:
 *   col0 = left x, col1 = right x, col2 = screen Y, col3 = centre accumulator
 * (= the a2CB1 the entity projector interpolates). g_road's column-major
 * layout matches the original 0x20-byte column stride exactly. */
#define ENT(k, c) (((i16 *)&g_road)[(c) * 0x10 + (k)])

/* ------------------------------------------------------------------------- */
/* fn0869_158D (table-build part): derive the half-width and scale LUTs from  */
/* the base perspective table.  (The tF6CD/ptr33D2 curve-pointer setup is the */
/* level-track hook described in the header and is intentionally omitted.)    */
/* ------------------------------------------------------------------------- */
static void setup_persp_luts(void)
{
    for (int si_24 = 0x00; si_24 < 0x0100; ++si_24) {
        g_persp_halfw[si_24] = (u8)((u16)ffd_persp_ramp[si_24] * 100 >> 0x07);
        g_persp_scale[si_24] = (u8)(ffd_persp_ramp[si_24] >> 0x01);
    }
}

/* ------------------------------------------------------------------------- */
/* fn0869_15D6  "world_road_perspective": fill the coord table for 16 depth levels. */
/* ------------------------------------------------------------------------- */
static void world_road_perspective(void)
{
    /* es_bx_19 = ds->tF6CD;  -- curve far ptr -> level track (absent); curve = 0 */
    int di_27 = 0x10 - (Gw_dist_lo & 0x0F);                 /* Eq_5091 di_27 */

    if ((i16)Gw_road_center > 0x0136)      Gw_road_center = 0x0136;
    else if ((i16)Gw_road_center < 0x0A)   Gw_road_center = 0x0A;

    /* a2C51 (entry 0): the nearest / screen-edge quad */
    ENT(0, 0) = (i16)(Gw_road_center + (u16)~0xCD);            /* w0000 */
    ENT(0, 1) = (i16)(Gw_road_center + 0xCE);                  /* w0020 */
    ENT(0, 2) = 0x98;                                          /* w0040 (Y = 152) */
    i16 si_48 = (i16)Gw_road_center;                          /* ci16 si_48 */
    ENT(0, 3) = (i16)(si_48 + (i16)~0x9F);                     /* w0060 */

    i16 curve0 = (i16)track_curve(0);                         /* (es_bx_19.u1)->b0000 */
    i32 ax_65 = (i32)di_27 * curve0;                          /* Eq_5147 */
    u16 ax_56 = (u16)g_persp_scale[di_27];                     /* Eq_5153 ax_56.u1 */
    /* si_122 is SIGNED (= wF1FD - 0xA0 + curve): negative when the car steers
     * right of centre. The 16-bit Borland multiply ax_56*si_122 is a signed
     * 16-bit product (low word), then an ARITHMETIC >>7. (Treating si_122 as
     * unsigned here is wrong and only hides while wF1FD==0xA0, i.e. si_122==0.) */
    i16 si_122 = (i16)(si_48 + (i16)~0x9F + (i16)(ax_65 >> 4));
    i16 ax_76 = (i16)(((i16)((u16)ax_56 * (u16)si_122) >> 7) + 0xA0);
    u16 ax_80 = (u16)g_persp_halfw[di_27];                     /* Eq_5170 ax_80.u1 */

    /* a2C53 (entry 1) */
    ENT(1, 0) = (i16)(ax_76 - (i16)ax_80);                    /* w0000 (left)  */
    ENT(1, 1) = (i16)((i16)ax_80 + ax_76);                    /* w0020 (right) */
    ENT(1, 2) = (i16)((ax_56 >> 1) + 0x55);                   /* w0040 (Y)     */
    ENT(1, 3) = si_122;                                       /* w0060         */

    int wLoc08_182 = (i16)(ax_65 >> 4);
    int di_107 = di_27 + 16;                                  /* Eq_5196 di_107 */

    for (int wLoc0A_190 = 0x02; wLoc0A_190 < 0x10; ++wLoc0A_190) {
        i16 curve = (i16)track_curve(wLoc0A_190 - 1);         /* *ptrLoc06_193 (tF6CD+k) */
        int v27_118 = wLoc08_182 + curve;
        u16 ax_111 = (u16)(g_persp_scale[di_107] >> 0x01);
        si_122 = (i16)(si_122 + v27_118);
        i16 ax_129 = (i16)(((i16)((u16)ax_111 * (u16)si_122) >> 6) + 0xA0);
        u16 ax_133 = (u16)g_persp_halfw[di_107];

        ENT(wLoc0A_190, 0) = (i16)(ax_129 - (i16)ax_133);      /* w0000 left  */
        ENT(wLoc0A_190, 1) = (i16)((i16)ax_133 + ax_129);      /* w0020 right */
        ENT(wLoc0A_190, 2) = (i16)(ax_111 + 0x55);             /* w0040 Y     */
        ENT(wLoc0A_190, 3) = si_122;                           /* w0060       */

        wLoc08_182 = v27_118;
        di_107 += 16;
    }

    /* trailing spill written by the loop's final pointer (faithful, unused below) */
    g_road.center[16] = si_122;              /* ptrLoc0E_200->w0060 */
    g_road.center[17] = si_122;              /* ptrLoc0E_200->w0062 */
}

/* ------------------------------------------------------------------------- */
/* fn10EF_0008: background fill -- sky band then ground band.                 */
/* Page base ds->tDE57 is the visible page (offset 0) for our linear buffer.  */
/* 40 bytes per scanline (320/8); each EGA byte = 8 pixels of the SR colour.  */
/* ------------------------------------------------------------------------- */
static void background_fill(void)
{
    memset(ff_fb, 0x00, (size_t)FF_W * FF_H);

    /* region 1: 0x5A0 words = 2880 bytes = 72 rows from page base, GC 0x0400 -> col 4 */
    for (int y = 0; y < 72; ++y)
        memset(ff_fb + (long)(y + PF_Y) * FF_W, COL_SKY, FF_W);

    /* di_50 = di_40 + 640  -> skip 16 rows (playfield rows 72..87 = the mountain
     * band, filled by the parallax/mountain layer fn10EF_039F) */
    /* region 2: 0x500 words = 2560 bytes = 64 rows, GC 0x0200 -> col 2              */
    for (int y = 88; y < 152; ++y)
        memset(ff_fb + (long)(y + PF_Y) * FF_W, COL_GROUND, FF_W);
}

/* inclusive horizontal span [x0,x1] at row y, clipped to the screen.
 * (fn10EF_01C0 / fn10EF_0291 fill the byte range left..right inclusive.) */
static void hspan(int y, int x0, int x1, u8 col)
{
    int sy = y + PF_Y;                            /* playfield-local Y -> screen Y */
    if (sy < 0 || sy >= FF_H) return;
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0)      x0 = 0;       /* ax_17 = 0  when ax < 0  (fn10EF_017E) */
    if (x1 > 0x13F)  x1 = 0x13F;   /* ax_17 = 0x13F clamp                   */
    if (x0 >= FF_W || x1 < 0) return;
    u8 *row = ff_fb + (long)sy * FF_W;
    for (int x = x0; x <= x1; ++x) row[x] = col;
}

/* ------------------------------------------------------------------------- */
/* fn10EF_0051 + helpers: rasterise one depth band as a trapezoid of spans.   */
/* Edges are interpolated linearly over y exactly as fn10EF_00F7 walks them.  */
/*   yk, lxk, rxk  : near edge  (Y[k],   left/right x)                        */
/*   yk1,lxk1,rxk1 : far  edge  (Y[k+1], left/right x), with yk1 < yk         */
/*   stripe: 0 none, 1 centre dash (fn10EF_0222), 2 side rumble (fn10EF_0252) */
/* ------------------------------------------------------------------------- */
/* fn10EF_00F7 + fn10EF_017E: Bresenham edge walk. For each playfield row bx in
 * [bx(far), dx(near)) it stores the edge x at edgex[bx].  Faithful four-case
 * transliteration; (ax = far x, cx = near x, dx = near y, bx = far y), dx > bx.
 * The original clamps x to [0,0x13F] for the byte/bit address; we store the raw
 * x and let hspan() clamp identically (same filled span). */
static void edge_walk(int *edgex, int ax, int cx, int dx, int bx)
{
    int di_18 = dx - bx;                       /* row count (neary - fary) */
    int bp_28 = cx - ax;                       /* near x - far x           */
    if (bp_28 < 0) {
        int bp_91 = -bp_28;
        int bx_93 = di_18 - bp_91;
        if (bx_93 >= 0) {                       /* steep: one store per row */
            int dx_129 = bx_93, bx_130 = bx;
            while (dx > bx_130) {
                edgex[bx_130] = ax;
                ++bx_130;
                dx_129 -= bp_91;
                if (dx_129 < 0) { dx_129 += di_18; --ax; }
            }
        } else {                               /* shallow */
            int bx_100 = bx, dx_103 = -bx_93;
            while (cx != ax) {
                --ax;
                dx_103 -= di_18;
                if (dx_103 < 0) { dx_103 += bp_91; edgex[bx_100] = ax; ++bx_100; }
            }
        }
    } else {
        int bx_32 = di_18 - bp_28;
        if (bx_32 < 0) {                        /* shallow */
            int bx_63 = bx, dx_66 = -bx_32;
            while (ax != cx) {
                ++ax;
                dx_66 -= di_18;
                if (dx_66 < 0) { dx_66 += bp_28; edgex[bx_63] = ax; ++bx_63; }
            }
        } else {                               /* steep */
            int dx_36 = bx_32, bx_37 = bx;
            while (dx > bx_37) {
                edgex[bx_37] = ax;
                ++bx_37;
                dx_36 -= bp_28;
                if (dx_36 < 0) { dx_36 += di_18; ++ax; }
            }
        }
    }
}

static void draw_band(int yk, int lxk, int rxk,
                      int yk1, int lxk1, int rxk1, int stripe)
{
    if (yk1 == yk) return;
    static int leftx[256], rightx[256];
    /* fn10EF_0051 calls fn10EF_00F7(buf, ax=x[k+1], cx=x[k], dx=Y[k], bx=Y[k+1]) */
    edge_walk(leftx,  lxk1, lxk, yk, yk1);
    edge_walk(rightx, rxk1, rxk, yk, yk1);

    int y0 = yk1, y1 = yk;                      /* rows [far, near) */
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y < y1; ++y) {
        if (y < 0 || y >= 256) continue;
        int lx = leftx[y], rx = rightx[y];

        hspan(y, lx, rx, COL_ROAD);               /* fn10EF_01C0 road span, col 14 */

        if (stripe == 1) {                        /* fn10EF_0222 centre dash, col 6 */
            int w = rx - lx;                      /* ax_22 = w01F4 - w0000          */
            int cx = (w >> 1) + lx;               /* ax_29                          */
            hspan(y, cx, cx + (w >> 5), COL_DASH);
        } else if (stripe == 2) {                 /* fn10EF_0252 side rumble, col 10 */
            int w = rx - lx;
            hspan(y, lx + 1, (w >> 5) + (lx + 1), COL_STRIPE);
            hspan(y, rx - 1 - (w >> 5), rx - 1,   COL_STRIPE);
        }
    }
}

static void draw_road(void)
{
    /* wArg04 = (ss->*bp).wFFFFFFB6 = ds->tEA3C & 0x30 (animation phase) */
    int wArg04 = (int)(Gw_dist_lo & 0x30);
    int bp_165 = (wArg04 >> 0x04) & 0x03;

    /* di_129 walks the depth levels 0..14; reads the next entry at k+1. */
    for (int k = 0; k < 15; ++k) {
        i16 cx_58 = ENT(k, 0);                /* left x  [k]   */
        i16 dx_59 = ENT(k, 2);                /* Y       [k]   */
        i16 ax_60 = ENT(k + 1, 0);            /* left x  [k+1] */
        i16 bx_61 = ENT(k + 1, 2);            /* Y       [k+1] */

        if (bx_61 != dx_59) {
            i16 rx0 = ENT(k, 1);              /* right x [k]   (ds->*(di+32))   */
            i16 rx1 = ENT(k + 1, 1);          /* right x [k+1] (ds->*(di+0x22)) */

            int stripe = 0;
            if (k < 13) {                     /* di_129 < 11371 (= 0x2C51+2k < 0x2C6B) */
                ++bp_165;
                if (bp_165 & 0x01)
                    stripe = ((bp_165 & 0x02) == 0) ? 1 : 2;
            }

            draw_band(dx_59, cx_58, rx0, bx_61, ax_60, rx1, stripe);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Public entry point.                                                        */
/* ------------------------------------------------------------------------- */
void render_world(void)
{
    /* wF1FD (player lateral road position) drives the pseudo-3D road perspective:
     * it is 0xA0 (centred) only at rest; camera_update sets it to 0xA0 - wDD84
     * every frame, so when the car steers (wDD84 != 0) the whole road swings.
     * Do NOT hardcode it here — world_road_perspective clamps it to [0x0A,0x136]
     * exactly as fn0869_15D6 does. (At frame 0, wDD84==0 so wF1FD stays 0xA0.) */

    setup_persp_luts();    /* fn0869_158D: build E4CC / DC84 LUTs    */
    world_road_perspective();    /* fn0869_15D6: fill coord table @0x2C51  */
    background_fill();     /* fn10EF_0008: sky + ground              */
    draw_clouds();         /* fn10EF_039F: 5 sky bands (white clouds) */
    draw_mountains();      /* fn10EF_0307: horizon band (DECA strip)  */
    draw_road();           /* fn10EF_0051: perspective road          */
}
