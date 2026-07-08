/* hud_cockpit.c — faithful port of the in-race HUD cockpit gauges (fn0BA8_1B27
 * @0BA8:1B27, cross-checked FUN_13a8_1b27). This is the fuel / aim / weapon-icon
 * gauge cluster drawn on the top cockpit panel every frame — the dominant
 * remaining pixel diff after the road, car, pickups and panel text.
 *
 * SOURCE OF TRUTH (transliterated, every constant/offset/table kept):
 *   fn0BA8_1B27   raw/reko/FF2EGA_0BA8.c   (the gauge state machine)
 *   fn1187_0232   raw/reko/FF2EGA_1187.c   (the glyph blitter == ff_font_draw)
 * The gauges are drawn as 8x8 FONT GLYPHS whose char codes come from STATIC
 * DGROUP tables:
 *   0x38B1/0x38B2/0x38B3  weapon/ammo icon column (x=0x108, y=0x10/0x18/0x20)
 *   0x38C2                aim-up marker           (x=(bF7ED<<3)+0x110, y=0x20)
 *   0x38D6                aim-down marker
 *   0x38CE                fuel column glyph       (x=0x110, y=0x10/0x18/0x20)
 * indexed by the live gauge levels (bF462 weapon, bF7ED aim, w38AF fuel).
 *
 * PERSISTENT-VRAM MODEL. The original draws ONE glyph per frame into the
 * persistent EGA page (the panel background is painted once at level start and
 * never re-blitted); the gauge columns are the ACCUMULATION of those per-frame
 * writes. This port rebuilds ff_fb from scratch each frame, so we keep a
 * persistent overlay `g_hud` that mirrors that VRAM page: the ported state
 * machine writes its one glyph/frame into g_hud (via ff_font_draw_buf), and
 * hud_cockpit_draw() composites g_hud over the freshly-painted panel each frame.
 * g_hud is cleared to the transparent sentinel 0xFF only on level init (the
 * fn0BA8_1B27 wArg04!=0 branch), exactly like the VRAM page is cleared once.
 *
 * ALSO ported in this file: the F/K bar gauges (fn0BA8_1B27 @1642-1668 via
 * fn10EF_04C7 / hud_bar_seg — ptrDF9A/DF9E/DFA2 = aDEEE[43/44/45], proven by
 * write-watchpoint) and the STAGE COMPLETED banner (@5185, hud_stage_banner).
 */
#include "ff_game.h"
#include "gnames.h"
#include "data/gamedata.h"   /* ffd_txt_bonus/stage/completed named strings */
#include "../render/ff_font.h"
#include <string.h>

/* Persistent HUD overlay = the original's VRAM page for the gauge columns.
 * 0xFF = never-written (transparent); any 0..15 = an opaque glyph pixel. */
static u8  g_hud[FF_W * FF_H];

/* The original's fn0BA8_1B27 wArg04 (= wFFFFFFDA) is nonzero ONLY on the first
 * frame of the level (cleared @974 after the first iteration). On that frame the
 * function does the INIT branch and RETURNS — it does NOT advance the gauge state
 * machine that frame. hud_cockpit_reset() runs the init in run_level's prologue,
 * so this flag makes the first hud_cockpit_step() an init-only no-op (matching the
 * reference: the first gauge UPDATE is frame 1, not frame 0). Without it we run one
 * extra orb/fuel-fill step and the whole startup animation drifts ~2 frames ahead. */
static int s_hud_first;

/* draw one gauge glyph into the overlay. `off` keeps the ORIGINAL glyph-table
 * offset convention (base 0x38B1) — the char codes come from the named
 * ffd_hud_glyph_tables data, no longer from G. */
static void hud_glyph(u16 off, int x, int y, int color)
{
    ff_font_draw_buf(g_hud, (const char *)&ffd_hud_glyph_tables[off - 0x38B1],
                     1, x, y, color);
}

/* fn10EF_04C7 (@10EF:04C7): draw ONE 8-px-wide, 1-row gauge-bar segment at
 * (x,y) into VRAM from a far-ptr's first 4 bytes, one per EGA plane (mask
 * 8,4,2,1 -> colour bit 3,2,1,0). The bar far-ptrs `ptrDF9A/DF9E/DFA2` are
 * `aDEEE[43/44/45]` (proven via a write-watchpoint -> the main sprite-directory
 * registration loop in fn0A0D_09E4, called from main); aDEEE[idx]==BOB frame idx
 * for the main bank, so the segment sprites are BOB frames 43(erase)/44(K)/45(F).
 * The original draws one segment row per frame into persistent VRAM (the bar is
 * the accumulation); we write into the persistent overlay `g_hud`. */
static void hud_bar_seg(int x, int y, int frame)
{
    if (!GC.bob) return;
    int w = 0, h = 0;
    const u8 *d = ff_bob_data(GC.bob, frame, &w, &h);
    if (!d || w <= 0 || h <= 0) return;
    if (y < 0 || y >= FF_H) return;
    /* fn10EF_04C7: lodsb then `inc si` -> si advances 2 bytes/plane, so the four
     * planes are d[0],d[2],d[4],d[6] (first byte of each plane; plane stride 2 =
     * 16-px-wide sprite). map mask 8/4/2/1 read in that order -> colour bit 3/2/1/0. */
    for (int k = 0; k < 8; ++k) {           /* one 8-px cell */
        u8 m = (u8)(0x80 >> k);
        int c = 0;
        if (d[0] & m) c |= 8;
        if (d[2] & m) c |= 4;
        if (d[4] & m) c |= 2;
        if (d[6] & m) c |= 1;
        int px = x + k;
        if (px >= 0 && px < FF_W) g_hud[(long)y * FF_W + px] = (u8)c;   /* opaque */
    }
}

/* fn0BA8_1B27 wArg04!=0 branch (lines 1495-1510): one-time HUD reset on level
 * entry. Clears the gauge overlay (the VRAM page clear) and zeroes the gauge
 * levels + the score/high redraw caches. */
void hud_cockpit_reset(void)
{
    memset(g_hud, 0xFF, sizeof g_hud);      /* transparent everywhere */
    Gb_bonus_banner = 0x00;                       /* bF6B2 */
    Gb_orbs_shown = 0x00;                       /* bF7ED aim current = 0 */
    Gw_fuel_shown = 0x00;                       /* w38AF fuel level = 0 */
    Gw_kbar_shown = 0x00;                       /* w38AD icon anim = 0 */
    Gw_fbar_shown = 0x00;                       /* w38AB icon anim = 0 */
    Gw_wF7E7 = 0xFFFF;                      /* wF7E7 */
    Gw_wF7E5 = 0xFFFF;                      /* wF7E5 */
    Gw_high_cache_hi = 0xFFFF;                      /* wF7E3 high-score cache */
    Gw_high_cache_lo = 0xFFFF;                      /* tF7E1 */
    Gw_bonus_cache_hi = 0x00;                        /* wF7EB */
    Gw_bonus_cache_lo = 0x00;                        /* tF7E9 */
    Gb_stage_digit = (u8)(Gw_stage + 49);       /* bF7EE = t27C7 + 49 */
    Gb_bF7EF = 0x00;                        /* bF7EF */
    s_hud_first = 1;                          /* wArg04!=0: first step is init-only */
}

static void hud_text_row(const char *s, int x, int y);   /* fwd (banner rows) */
static int s_bonus_banner;      /* d4c2 banner pending for THIS frame's render */

/* fn18dc_004f + fn0A0D_034C: format the low `nd` decimals of v and write the
 * glyphs into the PERSISTENT overlay at char cell (xc,yc) — the HUD numbers,
 * like the gauges, are incremental VRAM draws that persist across frames. */
void hud_number_draw(u32 v, int nd, int xc, int yc)
{
    char d8[9];
    u32 t = v % 100000000u;
    for (int i = 7; i >= 0; --i) { d8[i] = (char)('0' + (t % 10)); t /= 10; }
    if (nd > 8) nd = 8;
    ff_font_draw_buf(g_hud, d8 + (8 - nd), nd, xc * 8, yc * 8, 1);
}

/* fn0BA8_1B27 wArg04==0 branch: the NUMBER draws (head) + gauge cluster
 * (lines 1544-1641). Advances the state machines by exactly one glyph/frame
 * and writes into the persistent overlay. */
void hud_cockpit_step(void)
{
    if (s_hud_first) {                                 /* fn0BA8_1B27 wArg04!=0 branch:
                                                        * frame 0 does init only, no update */
        s_hud_first = 0;
        return;
    }

    /* NUMBER DRAWS (fn0BA8_1B27 head, ghidra FUN_13a8_1b27): PARITY-GATED,
     * INCREMENTAL, PERSISTENT — the 0232-family blit writes both pages, and
     * the panel art has "0..0" cells baked in, so undrawn cells read as zeros:
     *   24FF==0 frames: BONUS digits only when the DGROUP cache d5f9/d5fb
     *     (0xF7E9/0xF7EB) differs (else the d4c2/bF6B2 "BONUS" banner counts
     *     down); HIGH digits only when d5f1/d5f3 (0xF7E1/0xF7E3) differs.
     *   24FF!=0 frames: SCORE digits every such frame (the pre-add value).
     * So EVEN race frames show the score drawn one frame earlier — every
     * historical pixel sample (capext every-10th ⇔ ODD original frames)
     * missed this; the unfiltered per-frame test exposed it immediately. */
    if (Gw_blink == 0x00) {
        if (Gw_bonus_cache_hi == Gw_bonus_hi && Gw_bonus_cache_lo == Gw_bonus_lo) {
            if (Gb_bonus_banner != 0x00) {                  /* d4c2: big BONUS banner */
                Gb_bonus_banner -= 1;
                s_bonus_banner = 1;   /* drawn in hud_stage_banner (render phase):
                                       * the original paints it OVER the already-
                                       * drawn playfield of THIS frame's page, so
                                       * it flickers at page rate — the port must
                                       * draw it after render_world's repaint */
            }
        } else {
            Gw_bonus_cache_hi = Gw_bonus_hi; Gw_bonus_cache_lo = Gw_bonus_lo;
            hud_number_draw(((u32)Gw_bonus_hi << 16) | Gw_bonus_lo, 3, 0x1C, 4);
        }
        if (Gw_high_cache_hi != Gw_high_hi || Gw_high_cache_lo != Gw_high_lo) {
            hud_number_draw(((u32)Gw_high_hi << 16) | Gw_high_lo, 7, 1, 2);
            Gw_high_cache_hi = Gw_high_hi; Gw_high_cache_lo = Gw_high_lo;
        }
    } else {
        hud_number_draw(((u32)Gw_score_hi << 16) | Gw_score_lo, 7, 1, 4);
    }

    if (Gb_hud_phase == 0x00) {                          /* weapon/ammo icon anim */
        if (Gw_blink != 0x00) {                      /* blink parity */
            int ax_367 = (i8)Gb_weapon_anim;               /* bF462 (signed) */
            if (ax_367 != 0) {
                int next = (i8)Gb_weapon_anim + 1;
                if (ffd_hud_glyph_tables[0x38B4 - 0x38B1 + ax_367] == 0x00) {  /* table terminator */
                    Gb_hud_phase = 0x01;
                    next = 0x00;
                }
                Gb_weapon_anim = (u8)next;
                hud_glyph((u16)(0x38B1 + ax_367), 0x108, 0x10, 0x01);
                hud_glyph((u16)(0x38B2 + ax_367), 0x108, 0x18, 0x01);
                hud_glyph((u16)(0x38B3 + ax_367), 0x108, 0x20, 0x01);
            }
        }
    } else if ((i16)(i8)Gb_orbs_shown < (i16)Gw_lives) {  /* fewer orbs shown than lives: add one */
        Gb_orbs_shown = (u8)((i8)Gb_orbs_shown + 1);
        /* FILLED lives-orb glyph, char code @0x38D5 (`&w38AF + 19` words); x =
         * (bF7ED<<3)+0x110 -> the 4 orbs land at x=0x118/0x120/0x128/0x130, y=0x20. */
        hud_glyph(0x38D5, ((i16)(i8)Gb_orbs_shown << 3) + 0x110, 0x20, 0x01);
    } else if ((i16)(i8)Gb_orbs_shown > (i16)Gw_lives) {  /* more orbs shown than lives: erase one */
        if ((i16)Gw_lives >= 0)                        /* ERASE glyph, char code @0x38D6 */
            hud_glyph(0x38D6, ((i16)(i8)Gb_orbs_shown << 3) + 0x110, 0x20, 0x01);
        Gb_orbs_shown = (u8)((i8)Gb_orbs_shown - 1);
    } else {                                            /* orbs == lives: fuel gauge */
        i16 ax_158 = (i16)Gw_fuel_shown;
        if (ax_158 >= (i16)Gw_missile_fuel) {
            if ((i16)Gw_fuel_shown > (i16)Gw_missile_fuel) {
                Gw_fuel_shown = (u16)((i16)Gw_fuel_shown - 1);
                i16 fuel = (i16)Gw_fuel_shown, wLoc, si;
                if (fuel > 11)      { wLoc = fuel - 12; si = 0x10; }
                else if (fuel > 5)  { wLoc = fuel - 6;  si = 0x18; }
                else                { wLoc = fuel;      si = 0x20; }
                hud_glyph((u16)(0x38CE + wLoc), 0x110, si, 0x01);
                Gb_weapon_anim = (fuel == 0) ? 22 : 0x0D;
                Gb_hud_phase = 0x00;
            }
        } else {
            Gb_weapon_anim = (ax_158 == 0) ? 0x01 : 0x07;
            Gb_hud_phase = 0x00;
            Gw_fuel_shown = (u16)((i16)Gw_fuel_shown + 1);
            i16 fuel = (i16)Gw_fuel_shown, wLoc, si;
            if (fuel > 0x0C)      { wLoc = fuel - 12; si = 0x10; }
            else if (fuel > 0x06) { wLoc = fuel - 6;  si = 0x18; }
            else                  { wLoc = fuel;      si = 0x20; }
            hud_glyph((u16)(0x38CE + wLoc), 0x110, si, 0x01);
        }
    }

    /* F/K bar gauges (fn0BA8_1B27 @1642-1668). Blink parity picks which bar this
     * frame: w24FF!=0 -> LEFT "F" bar @x=0x50 ramps `w38AB`->`wDC6C>>1`;
     * w24FF==0 -> RIGHT "K" bar @x=0xC8 ramps `w38AD`->`wDC78`. Each draws ONE
     * 8x1 segment row/frame at y=0x27-level: fill sprite when growing (F=frame45,
     * K=frame44), erase sprite (frame43) when shrinking. Persistent overlay. */
    if (Gw_blink != 0x00) {                             /* F bar, x=0x50 */
        i16 tgt = (i16)((i16)Gw_fuel_window >> 1);
        if ((i16)Gw_fbar_shown > tgt) {
            hud_bar_seg(0x50, 0x27 - (i16)Gw_fbar_shown, 43);   /* ptrDF9A erase */
            Gw_fbar_shown = (u16)((i16)Gw_fbar_shown - 1);
        } else if ((i16)Gw_fbar_shown < tgt) {
            Gw_fbar_shown = (u16)((i16)Gw_fbar_shown + 1);
            hud_bar_seg(0x50, 0x27 - (i16)Gw_fbar_shown, 45);   /* ptrDFA2 F fill */
        }
    } else {                                              /* K bar, x=0xC8 */
        i16 tgt = (i16)Gw_kero;
        if ((i16)Gw_kbar_shown > tgt) {
            hud_bar_seg(0xC8, 0x27 - (i16)Gw_kbar_shown, 43);   /* ptrDF9A erase */
            Gw_kbar_shown = (u16)((i16)Gw_kbar_shown - 1);
        } else if ((i16)Gw_kbar_shown < tgt) {
            Gw_kbar_shown = (u16)((i16)Gw_kbar_shown + 1);
            hud_bar_seg(0xC8, 0x27 - (i16)Gw_kbar_shown, 44);   /* ptrDF9E K fill */
        }
    }
}

/* Composite the persistent gauge overlay over the freshly-painted panel. */
void hud_cockpit_draw(void)
{
    for (long i = 0; i < FF_W * FF_H; ++i)
        if (g_hud[i] != 0xFF) ff_fb[i] = g_hud[i];
}

/* fn13a8_000b draw_text_row — the BIG banner text: each char -> BOB sprite
 * (digits '0'-'9' -> 80-89, letters 'A'-'Z' -> 90-115), 0x10 px apart, drawn
 * centre-x / bottom-y with the +PF_Y=48 playfield offset (the enqueue path
 * FUN_120d_07cf -> fn1187_1C73). Space (0x20) skips; '#' box (0x23) omitted. */
static void hud_text_row(const char *s, int x, int y)
{
    if (!GC.bob) return;
    for (; (u8)*s; ++s, x += 0x10) {
        int c = (u8)*s;
        if (c == 0x20 || c == 0x23) continue;
        int spr = c + 0x20;
        if (spr > 0x60) spr = c + 0x19;
        int w = 0, h = 0;
        ff_bob_dims(GC.bob, spr, &w, &h);
        if (w > 0 && h > 0) ff_bob_blit(GC.bob, spr, x - (w >> 1), y - h + 48);
    }
}

/* fn0BA8_1B27 @5185-5205: while anim_step (bDC6E) is set — the B-CITERN boss /
 * stage-clear flag — draw "STAGE <n> COMPLETED" over the playfield. bF7EE holds
 * the ASCII stage digit (t27C7 + 49, set in hud_cockpit_reset). */
void hud_stage_banner(void)
{
    /* the d4c2 "BONUS" banner (fn0BA8_1B27 head): drawn over the playfield on
     * the parity-0 pages only — visible every other frame in the original. */
    if (s_bonus_banner) {
        s_bonus_banner = 0;
        hud_text_row((const char *)ffd_txt_bonus, 0x80, 0x1E);
    }
    if (Gb_stage_clear == 0) return;
    hud_text_row((const char *)ffd_txt_stage, 0x80, 0x1E);   /* STAGE        */
    hud_text_row((const char *)Gp_stage_digit, 0xA0, 0x2E);   /* stage number */
    hud_text_row((const char *)ffd_txt_completed, 0x60, 0x3E);   /* COMPLETED    */
}
