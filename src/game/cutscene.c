/* cutscene.c — FAITHFUL port of round_transition fn0BA8_048A (@0BA8:048A /
 * ghidra FUN_13a8_048a), called from game_main's DEATH branch (@959: tEA3A--,
 * then this, then the CONTINUE screen / highscore). Two scenes in one function:
 *
 *  DEATH PATH (t27C7 != 4, incl. every attract-demo cycle since bF6B3 == 0):
 *    free the entity pool (fn0869_19E7 — the aDDEC décor ring is NOT cleared,
 *    so leftover roadside rocks keep drawing), load NUC.CPT (fn0A0D_0ADB) and
 *    register it at aDEEE[tEF52..] (fn0A0D_0B35), arm panel msg 10 (the X/O
 *    tic-tac-toe grid @0x3509 — the enemy "wins" the game), then the VEHICLE-
 *    RISE loop: 11 steps drawing NUC frames tEF52+0..+10 at centre (0xA0,0x57)
 *    over the persistent last race frame — per step: sky fill rows 0..0x47
 *    (fn1187_1A95 colour 4), clouds, the vehicle, mountains OVER it, then the
 *    bottom-16-row merge (fn1187_18E4/191B/1807/186D: rewrite vehicle pixels
 *    over the mountain band ONLY where the band left sky colour 4 — vehicle
 *    behind the peaks), then entities + décor ring. Each step is held until
 *    the ISR counter *tF205 (-> DGROUP 0xBCBC, ticking once per 4 PIT ticks
 *    = once per GAME FRAME, ndisasm @0xD874-D88E) reaches 3 => 3 frames/step.
 *    Exit: t24F3=0xF8, 15-tick hold on the final image.
 *
 *  FINALE (t27C7 == 4 — the player DIED ON STAGE 5, or the fire+aux+no-credit
 *  cheat at entry): the BAD ENDING — the convoy launches the nuclear rocket.
 *    t27C7=5, arm the wave VM (script dir[5]), speed 0, then a full mini game
 *    loop (scroll/curve/clouds/mountains &3 strip/road/entities/décor) with
 *    the level-5 wave script; when the charge trigger (type 78) arms wF6C8
 *    (d4d8): load NUC, then the ROCKET rises from y=0x57 by 2/frame (frame
 *    tEF52+12, flame tEF52+16 on y&2), advancing frames at the 0BA8:06D6
 *    thresholds [0x41,0x3B,0x2B,0x1D,0x17,0x01] (actions @06E2: e++/c++, the
 *    y==1 one also zeroes the tick counter); off-screen (y<=0) a 20-tick
 *    pause, then the nuclear burst (sprite 0x2A, depth 200) grows at -y until
 *    -y > 0x4F. Then t27C7-- and the vehicle-rise plays too.
 *
 * SOURCE OF TRUTH: raw/ghidra/decompiled.c FUN_13a8_048a (3886-4209) cross-
 * checked vs raw/reko/FF2EGA_0BA8.c fn0BA8_048A; the 06D6/06E2 tables and the
 * action bodies read from the EXE (file 0x5F56/0x5F62/0x5F6E, ndisasm); the
 * tF205 target 0xBCBC proven by the ptr store @ghidra 11309 + the ISR disasm.
 *
 * SINGLE-FRAMEBUFFER ADAPTATION: the original draws over the persistent VRAM
 * pages (road rows + HUD survive from the race, alternating between the last
 * TWO race frames as it flips). ff_fb persists across run_level's return, so
 * the port draws over the LAST race frame (page parity collapses to one).
 * render_world() clears the whole fb, so the finale saves/restores the HUD
 * rows 0..PF_Y-1 around it (the original never repaints the HUD here).
 */
#include "ff_game.h"
#include "gmem.h"
#include <string.h>

#define PF_Y 48

typedef enum { RT_OFF, RT_FINALE, RT_RISE, RT_EXIT } RtState;
static RtState rt_state = RT_OFF;
static int rt_ctr;                /* *tF205 -> the bBCBC tick ctr (1 tick = 1 frame) */
static int rt_spr, rt_spr_end;    /* rise-loop sprite cursor (local_58)              */
/* finale locals (register/stack vars of fn0BA8_048A) */
static int fl_rearm;              /* local_3 — wave VM re-armed once pool cleared    */
static int fl_done;               /* local_4 — finale finished                       */
static int fl_rocket;             /* local_5 — NUC loaded / rocket phase             */
static int fl_y;                  /* DI (init 0x57) — rocket bottom-y                */
static int fl_e, fl_c;            /* local_e rocket frame / local_c flame frame      */
static int fl_phase;              /* local_10 — track-advance gate (tEA3C & 0x30)    */

/* 0BA8:06D6 — rocket-rise event thresholds (y after the -2 step) and their
 * actions @06E2 (EXE 0x5F56/0x5F62 + ndisasm of the bodies @0x5F6E):
 *   y==0x41 e++ | 0x3B c++ | 0x2B e++ | 0x1D c++ | 0x17 e++ | 0x01 ctr=0,e++  */
static const i16 rk_thr[6] = { 0x01, 0x17, 0x1D, 0x2B, 0x3B, 0x41 };
static const u8  rk_act[6] = { 2, 0, 1, 0, 1, 0 };   /* 0=e++, 1=c++, 2=ctr0+e++ */

/* fn0869_19E7 — reset_effect_pools: slots 15..19 get the type-0 prototype
 * (@0x1490) copied in, then ALL 20 slots are marked free (+0 = 0xFF, +0x2E = 0).
 * NOTE: it does NOT clear the aDDEC décor ring — leftover rocks keep drawing. */
static void pool_reset_19e7(void)
{
    for (int si = 0x0F; si < 0x14; ++si)
        memcpy(GPTR(0xE5CC + si * 0x33), GPTR(0x1490), 0x33);
    for (int si = 0; si < 0x14; ++si) {
        GB(0xE5CC + si * 0x33 + 0x00) = 0xFF;
        GB(0xE5CC + si * 0x33 + 0x2E) = 0x00;
    }
}

/* prep the vehicle-rise loop (the common tail after the finale / pool reset) */
static void rt_begin_rise(void)
{
    if (!fl_rocket)                              /* fn0A0D_0ADB: load + register NUC  */
        sprite_dir_register_bank2();             /* (GC.nuc loaded at game_init)      */
    GB(0xDE52) = 0x01;                           /* arm the typewriter                */
    GB(0xDE50) = 0x0A;                           /* msg 10 = the X/O tic-tac-toe grid */
    rt_spr     = (i16)GW(0xEF52);                /* NUC frames tEF52+0 .. tEF52+10    */
    rt_spr_end = rt_spr + 0x0B;
    rt_ctr     = 0;
    rt_state   = RT_RISE;
}

/* fn0BA8_048A entry (up to the t27C7 dispatch). Call ONCE from the game_main
 * death branch, right after tEA3A--. */
void round_transition_start(void)
{
    if (GB(0xF6B3) != 0x00) { rt_state = RT_OFF; return; }   /* demo aborted: skip */

    GB(0x24F3) = 0x98;                           /* playfield height (t24F3.u0)  */
    /* tDE57 += 0x780 — VRAM page base bookkeeping, not modelled (chunky fb)   */
    GW(0xEF52) = GW(0xDE69);                     /* tEF52 = tDE69                */
    ff_poll_controls();                          /* fn0A0D_0002                  */
    if (GW(0x24B7) != 0 && GW(0x24B9) != 0 && GW(0xEA3A) == 0)
        GW(0x27C7) = 0x04;                       /* fire+aux, no credits: finale */

    fl_rearm = fl_done = fl_rocket = 0;
    fl_y = 0x57;
    fl_phase = -1;

    if (GW(0x27C7) == 0x04) {                    /* THE FINALE (bad ending)      */
        GW(0x27C7) = 0x05;                       /* wave script dir[5]           */
        vm_arm();                                /* fn0BA8_13FD(1)               */
        if ((i16)GW(0xF480) > 0x04) GW(0xF480) = 0x00;
        GW(0xF6C8) = 0x00;                       /* d4d8 charge trigger          */
        GW(0xE9CC) = 0x00;                       /* speed 0                      */
        rt_state = RT_FINALE;
        return;
    }
    pool_reset_19e7();                           /* fn0869_19E7                  */
    rt_begin_rise();
}

/* ---- the rocket / explosion display-list block (finale, local_5 set) ------ *
 * Runs between the entity enqueue and the display-list draw (fn0A0D_082A),    *
 * installed as the sprite_dl extra-enqueue hook so it lands inside the same   *
 * depth-sorted flush the original uses.                                       */
static void rocket_enqueue(void)
{
    if (rt_state != RT_FINALE || !fl_rocket) return;
    if (fl_y > 0) {                              /* the rise                     */
        dl_enqueue_sorted(0xA0, (i16)fl_y, fl_e, 0x01);
        if (fl_y & 0x02)                         /* flame at (0xA1, y+1), front  */
            dl_enqueue_sorted(0xA0 + 1, (i16)(fl_y + 1), fl_c, 0x00);
        fl_y -= 2;
        for (int k = 0; k < 6; ++k)              /* 0BA8:06D6 event table        */
            if (fl_y == rk_thr[k]) {
                if (rk_act[k] == 1) fl_c += 1;
                else { if (rk_act[k] == 2) rt_ctr = 0; fl_e += 1; }
                break;
            }
    } else if (rt_ctr > 0x14) {                  /* 20-tick pause, then the burst */
        rt_ctr = 0x23;                           /* *tF205 = '#' (keeps the gate) */
        dl_enqueue_sorted(0xA0, (i16)(-fl_y), 0x2A, 200);
        fl_y -= 2;
        if (fl_y < -0x4F) fl_done = 1;
    }
}

/* one frame of the FINALE loop (fn0BA8_048A LAB_13a8_084b body) */
static void finale_frame(void)
{
    if (GW(0xF46A) == 0 && !fl_rearm) {          /* pool cleared: re-arm script 5 */
        fl_rearm = 1;
        vm_arm();
    }
    if (GW(0xF6C8) != 0) {                       /* charge armed: brake to 0      */
        if (GW(0xE9CC) != 0) GW(0xE9CC) -= 1;
    } else if ((i16)GW(0xE9CC) < 3) {
        GW(0xE9CC) += 1;                         /* cruise up to speed 3          */
    }
    /* fn0BA8_203A displist reset — handled inside render_entities()            */
    {
        u32 acc = ((u32)GW(0xEA3E) << 16) | GW(0xEA3C);
        acc += GW(0xE9CC);
        GW(0xEA3C) = (u16)acc; GW(0xEA3E) = (u16)(acc >> 16);
    }
    if ((i16)GW(0xDD84) > 0)      GW(0xDD84) -= 1;   /* steer decays to centre  */
    else if ((i16)GW(0xDD84) < 0) GW(0xDD84) += 1;
    if ((i16)GW(0xF1FD) > 0xA0)      GW(0xF1FD) -= 1;
    else if ((i16)GW(0xF1FD) < 0xA0) GW(0xF1FD) += 1;
    {
        /* iVar6 = (curve * speed) >> 2 — NB shift AFTER the multiply here      */
        i16 iv = (i16)(((i16)((i8)track_curve_now()) * (i16)GW(0xE9CC)) >> 2);
        i16 wx = (i16)((i16)GW(0xEF50) + iv);
        if (wx < 0)            wx = (i16)(wx + 0x2800);
        else if (wx >= 0x2800) wx = (i16)(wx - 0x2800);
        GW(0xEF50) = (u16)wx;
        /* aF689[(wEF50>>4) & 3] strip select — the finale masks &3 (ghidra     *
         * @3990), unlike run_level's &7; draw_mountains honours g_mtn_mask.    */
        GW(0xEA48) = (u16)(GW(0xEA48) + (u16)(iv << 6));
        GW(0xEA46) = (u16)(GW(0xEA46) + (u16)(iv * 0x35));
        GW(0xEA44) = (u16)(GW(0xEA44) + (u16)(iv * 0x33));
        GW(0xEA42) = (u16)(GW(0xEA42) + (u16)(iv * 0x2A));
        GW(0xEA40) = (u16)(GW(0xEA40) + (u16)(iv << 5));
    }
    {
        int phase = GW(0xEA3C) & 0x30;
        if (fl_phase != phase) { track_advance(); fl_phase = phase; }
    }
    /* fn0869_15D6 build + fn10EF_0008/0051/039F/0307 draw = render_world();    *
     * the HUD rows persist in the original (never repainted here), so shield   *
     * them from render_world's full clear.                                     */
    {
        static u8 hud[PF_Y * FF_W];
        memcpy(hud, ff_fb, sizeof hud);
        extern int g_mtn_mask;
        g_mtn_mask = 3;
        render_world();
        g_mtn_mask = 7;
        memcpy(ff_fb, hud, sizeof hud);
    }
    if (fl_rearm) vm_step();                     /* fn0BA8_13FD(0)               */
    entities_update_all();                       /* fn0DAE_0446                  */
    g_dl_extra = rocket_enqueue;                 /* fn0A0D_082A rocket block     */
    render_entities();                           /* décor + pool + rocket, 1C73  */
    g_dl_extra = 0;
    /* flip + fn1187_0136 = the caller presents this frame                      */
    if (GW(0xE9CC) != 0) GW(0x24FF) ^= 0x01;     /* blink parity                 */
    if (GW(0xF6C8) != 0 && !fl_rocket) {         /* charge fired: load the NUC   */
        fl_rocket = 1;
        sprite_dir_register_bank2();             /* fn0A0D_0ADB                  */
        GW(0xF6C8) = (u16)(GW(0xEF52) + 19);
        fl_e = (i16)GW(0xEF52) + 0x0C;           /* rocket frame                 */
        fl_c = (i16)GW(0xEF52) + 0x10;           /* flame frame                  */
    }
    rt_ctr += 1;                                 /* the bBCBC tick               */
}

/* one DRAW step of the vehicle-rise loop (the fn0BA8_048A local_58 loop body) */
static void rise_step(void)
{
    /* *tF205 = 0 (done by the caller), then fn0BA8_00A1: ONE typewriter tick   */
    panel_text_advance();
    /* fn1187_1A95(0,0,0x13F,0x47,4): sky fill, playfield rows 0..0x47          */
    for (int y = 0; y <= 0x47; ++y)
        memset(ff_fb + (long)(y + PF_Y) * FF_W, 0x04, FF_W);
    draw_clouds();                               /* fn10EF_039F                  */
    /* fn0A0D_07CF(0xA0, 0x57, spr) + the truncated display-list draw: the      *
     * vehicle frame alone, centre-x 0xA0, bottom-y 0x57.                       */
    int w = 0, h = 0;
    ff_dir_dims(rt_spr, &w, &h);
    if (w > 0 && h > 0) {
        ff_blit_clip_top = PF_Y;                 /* fn1187_1C73 playfield clip   */
        ff_blit_quirk6   = 1;                    /* shift-6 leading-byte ROM bug */
        int top = 0x57 - h;                      /* + the 1187:1CFA top-clip +1  */
        if (top < 0) top += 1;                   /* (inert: NUC h <= 84 < 0x57)  */
        ff_dir_blit(rt_spr, 0xA0 - (w >> 1), top + PF_Y);
        ff_blit_clip_top = 0;
        ff_blit_quirk6   = 0;
    }
    draw_mountains();                            /* fn10EF_0307 (over the car)   */
    /* the bottom-16-row merge (fn1187_18E4/191B/1807/186D): rewrite vehicle    *
     * pixels over the mountain band rows 0x48..0x57 where the band left sky    *
     * colour 4 — so the peaks occlude the vehicle, the sky gaps don't.         */
    if (w > 0 && h >= 0x10) {
        for (int r = 0; r < 0x10; ++r) {
            long row = (long)(0x48 + r + PF_Y) * FF_W;
            for (int c = 0; c < w; ++c) {
                int col = ff_dir_sample(rt_spr, c, h - 0x10 + r);
                int sx = 0xA0 - (w >> 1) + c;
                if (col && sx >= 0 && sx < FF_W && ff_fb[row + sx] == 0x04)
                    ff_fb[row + sx] = (u8)col;
            }
        }
    }
    /* fn0BA8_203A + fn0DAE_0446 + fn0869_17B9 + fn1187_1C73: entities + the    *
     * leftover décor ring (empty pool in the death path; finale leftovers stay) */
    entities_update_all();
    render_entities();
    /* the typewriter glyphs persist in VRAM; redraw the revealed set (pixel-   *
     * equivalent to the original's incremental one-glyph draw)                 */
    panel_text_draw();
}

/* One presented frame of the cutscene. Returns 1 while it owns the frame,
 * 0 when round_transition has finished (or was skipped: bF6B3). */
int round_transition_frame(void)
{
    switch (rt_state) {
    case RT_OFF:
        return 0;
    case RT_FINALE:
        finale_frame();
        if (fl_done) {                           /* rocket burst finished        */
            GW(0x27C7) -= 1;                     /* t27C7 back to 4              */
            rt_begin_rise();
        }
        return 1;
    case RT_RISE:
        if (rt_ctr == 0) rise_step();            /* the step body (1 frame)      */
        rt_ctr += 1;                             /* bBCBC tick per frame         */
        if (rt_ctr >= 3) {                       /* while (*tF205 < 3) hold      */
            rt_spr += 1;
            if (rt_spr >= rt_spr_end) {          /* loop done: the exit tail     */
                GB(0x24F3) = 0xF8;               /* t24F3 restored               */
                /* tDE57 -= 0x780 — page bookkeeping, not modelled              */
                rt_ctr = 0;
                rt_state = RT_EXIT;
            } else {
                rt_ctr = 0;
            }
        }
        return 1;
    case RT_EXIT:                                /* *tF205=0; while (< 0x0F)     */
        rt_ctr += 1;
        if (rt_ctr >= 0x0F) { rt_state = RT_OFF; return 0; }
        return 1;
    }
    return 0;
}
