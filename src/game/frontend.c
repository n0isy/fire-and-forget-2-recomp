/* frontend.c — FAITHFUL port of the front-end boot sequence (task #22):
 *
 *   main (083C:0009)      boot screens: TITUS.CPT (shown while the assets load)
 *                         then PRES.CPT, each via fn0BA8_0AC5 + fn0BA8_1325.
 *   fn0DAE_0001 (0DAE:0001 / FUN_15ae_0001)  title screen draw:
 *                         TITRE.CPT + 3 DGROUP text overlays + credits digit +
 *                         the © sprite via the display list (fn0A0D_07CF idx 0x38
 *                         at centre-x 8, bottom-y 200 -> fn1187_1C73).
 *   fn0DAE_00D5 (FUN_15ae_00d5)  show_screen_timed: screen + t24F1=5 or input.
 *   fn0DAE_0118 (FUN_15ae_0118)  the menu / player-select loop:
 *                         post-demo screens DROGUE/TITUS/PRES, INSERT COINS,
 *                         FIRE inserts credits (tEA3A 1..3), PRESS BUTTON TO
 *                         PLAY, timeout (t24F1==0) -> bDC6F=1 (attract DEMO).
 *
 * SOURCE OF TRUTH: raw/reko/FF2EGA_0DAE.c + raw/ghidra/decompiled.c
 * (FUN_15ae_0001/00d5/0118, FUN_103c_0009) + ndisasm of the timer ISR.
 *
 * TIMER MODEL (t24F1) — derived from the EXE, not guessed. The `dec word[24F1]`
 * lives only in the timer ISR (found by byte-scan FF 0E F1 24 -> 1c3a:06EE and
 * 1c3a:08DF). The INT8 hook (1c3a:0399/03E1) programs the PIT to reload 0x3333
 * (AdLib branch, 1193182/13107 = 91.03 Hz, dividers /5 /18) or 0x4000 (speaker
 * branch, 1193182/16384 = 72.83 Hz, dividers /4 /18). BOTH branches give the
 * same t24F1 rate: 91.03/90 = 72.83/72 = 1.011 Hz. The port integrates that
 * rate per platform frame (FF_FPS=70, ff_wait_frame) with an exact rational
 * accumulator, and decrements t24F1 as a SIGNED word (the original can go
 * negative; fn0DAE_02D0 tests t24F1 < 0).
 *
 * FRAME ADAPTATION. Every original busy-wait ("while t24F1 != 0 { poll }")
 * becomes a state advanced once per platform frame. All draws go to ff_fb,
 * which persists between frames exactly like the original VRAM page (the menu
 * never repaints its background).
 *
 * SPIRAL WIPES fn0BA8_1315/1325/132F — PORTED (see the wipe block below): every
 * front-end screen is revealed by the expanding cell spiral (1325), the
 * to-the-race transitions contract to black (1315), and the race's first frame
 * is revealed by the spiral (fn0869_0006 @1928-1936 local_28 path).
 *
 * The boot screens TITUS/PRES have NO wait in main — they stay up while the
 * 1990 hardware loads/depacks the assets. Our load is instant, so the port
 * holds each for FE_BOOT_HOLD frames (~2 s). This constant is presentation
 * pacing (hardware load time), not game logic; the screens' PIXELS are the
 * faithful part.
 */
#include "ff_game.h"
#include "gnames.h"
#include "data/gamedata.h"   /* ffd_txt_* named game strings */
#include "../render/ff_font.h"
#include <stdio.h>
#include <string.h>

/* ---- timer-ISR model ------------------------------------------------------
 * The platform now runs at the faithful 18.2 Hz (ff_wait_frame: one presented
 * frame = one bBCBB tick of the timer ISR), so the ISR chain reduces to:
 * t24F1 is decremented once every 0x12 (18) frames — identical to the in-race
 * model in run_level.c. */
static int fe_isr18;

static void fe_timer_frame(void)
{
    if (++fe_isr18 >= 0x12) {
        fe_isr18 = 0;
        Gi_tick_timer -= 1;                       /* dec word [24F1] — signed, no clamp */
    }
}

/* ---- state machine --------------------------------------------------------- */
typedef enum {
    FE_BOOT_TITUS,      /* main: TITUS.CPT during asset load          */
    FE_BOOT_PRES,       /* main: PRES.CPT before the menu loop        */
    FE_CUTSCENE,        /* fn13a8_048a: round_transition (NUC scene)  */
    FE_CONTINUE,        /* fn0DAE_02D0: CONTINUE? countdown (tEA3A>0) */
    FE_GAMEOVER,        /* fn13a8_0e13: GAMEOVER.CPT (post-race)      */
    FE_HIGHSCORE,       /* fn13a8_0e13: HIGHSCOR.CPT + score table    */
    FE_DEMO_SCREENS,    /* fn0DAE_0118 prologue: DROGUE/TITUS/PRES    */
    FE_MENU,            /* fn0DAE_0118 main loop                      */
    FE_MENU_RELEASE,    /* inner `while (w24B7)` fire-release wait    */
    FE_MENU_EXIT_WAIT,  /* tEA3A==0 path: t24F1=1 busy-wait           */
    FE_RACE             /* handed over to run_level                   */
} FeState;

#define FE_BOOT_HOLD 36           /* ~2 s @18.2 fps per boot screen (load-time model) */

static FeState fe_state = FE_BOOT_TITUS;
static int fe_hold;               /* frame counter for the boot-screen holds     */
static int fe_sub;                /* FE_DEMO_SCREENS sub-index 0..2              */
static int fe_di;                 /* menu iVar3 state (0/1/2)                    */
static int fe_entered;            /* one-shot entry flag for the current state   */
static int fe_race_started;       /* race frame 0 consumed + reveal wipe done    */
static int fe_start_done;         /* race already set up (CONTINUE path)         */
static char fe_assets[300];

/* ---- fn13a8_132F SPIRAL WIPE (fn0BA8_1315 = clear + contract-to-black,
 * fn0BA8_1325 = expand-reveal) — the screen-transition effect around every
 * front-end screen and the race's first frame. FUN_1987_278a copies ONE CELL
 * (2 bytes = 16 px wide x 10 rows; grid 20x20 over the 320x200 page) from the
 * freshly-drawn back page onto the visible page; 132f walks a square spiral
 * around cell (10,10) — 10 rings (sizes 1,3..19 outward for dir 0; 19,17..1
 * inward from the border for dir 1), ONE RING PER FRAME SYNC (FUN_1987_0136),
 * so a wipe lasts 10 presented frames while the caller's logic is blocked
 * (the ISR keeps ticking t24F1 — fe_timer_frame runs during wipe frames).
 * Port model: wp_new = the composed NEW screen; ff_fb starts as the OLD
 * visible screen (wp_old snapshot taken in fe_show_cpt); wipe_ring() copies
 * one ring of cells per frame. dir 1 models FUN_1987_01ac (page clear): the
 * "new" content is black. */
static u8  wp_old[FF_W * FF_H];
static u8  wp_new[FF_W * FF_H];
static int wp_active, wp_dir, wp_c, wp_x, wp_y;

/* Two-stage transition state (param_3=1 screens): the original fn13a8_0AC5
 * wipes the OLD screen to BLACK (fn13a8_1315) on the OLD palette, THEN programs
 * the NEW palette (fn1987_2A6F) while the screen is black, THEN the caller
 * reveals the NEW screen (fn13a8_1325) on the NEW palette — so the new palette
 * NEVER paints the old image. fe_stage: 0 idle/single-stage, 1 = to-black
 * running (old palette live), 2 = reveal running (new palette live). At the
 * stage-1 -> stage-2 boundary the screen is fully black, so the palette swap
 * (fe_new_pal) is invisible. */
static u8  fe_next[FF_W * FF_H];              /* the composed NEW screen (stage 2 source) */
static u8  fe_old_pal[FF_NCOL][3], fe_new_pal[FF_NCOL][3];
static int fe_stage;

static void wipe_cell(int cx, int cy)
{
    if (cx < 0 || cx >= 20 || cy < 0 || cy >= 20) return;
    for (int r = 0; r < 10; ++r)
        memcpy(ff_fb + (long)(cy * 10 + r) * FF_W + cx * 16,
               wp_new + (long)(cy * 10 + r) * FF_W + cx * 16, 16);
}

/* one ring of fn13a8_132F (the loop body between FUN_1987_0136 waits) */
static void wipe_ring(void)
{
    int s = wp_dir ? -1 : 1;
    wp_x -= s; wp_y -= s;
    for (int i = 0; i < wp_c; ++i) { wp_y += 1; wipe_cell(wp_x, wp_y); }
    for (int i = 0; i < wp_c; ++i) { wp_x += 1; wipe_cell(wp_x, wp_y); }
    for (int i = 0; i < wp_c; ++i) { wp_y -= 1; wipe_cell(wp_x, wp_y); }
    for (int i = 0; i < wp_c; ++i) { wp_x -= 1; wipe_cell(wp_x, wp_y); }
    wp_c += wp_dir ? -2 : 2;
    if (wp_c == (wp_dir ? -1 : 0x15)) wp_active = 0;
}

/* fn13a8_0AC5 param_3==0 path (boot TITUS/PRES + the race first-frame reveal):
 * the loader does `bc67 ^= 0x2000; 01ac(); bc67 ^= 0x2000` — and 01ac clears the
 * page bc67 addresses, which (bc67 was just toggled onto it) is the DISPLAYED
 * page. So the OLD screen is INSTANTLY cleared to BLACK (not a spiral 1315),
 * THEN the new palette is programmed (2a6f, screen already black), THEN the
 * caller's 1325 reveals the NEW screen over the black. The reveal base is
 * therefore BLACK — NOT the previous screen. Revealing over wp_old showed e.g.
 * TITUS under the PRES palette during the PRES reveal (the user's boot flash);
 * revealing over black (like the instant 01ac clear) removes it. */
static void fe_wipe_expand(void)
{
    memcpy(wp_new, ff_fb, sizeof wp_new);
    memset(ff_fb, 0, (size_t)FF_W * FF_H);          /* 01ac: instant clear the display to black */
    ff_set_palette((const uint8_t (*)[3])GC.pal);   /* 2a6f on the black screen, before 1325 */
    wp_active = 1; wp_dir = 0; wp_c = 1; wp_x = 10; wp_y = 10;
    fe_stage = 0;
}

/* fn13a8_0AC5 param_3=1 path: the FULL faithful transition. Stage 1 = fn13a8_1315
 * contract the OLD screen to black on the OLD palette; at the black midpoint set
 * the NEW palette (fn1987_2A6F, invisible); stage 2 = fn13a8_1325 reveal the NEW
 * screen on the NEW palette. This is what fixes the "half old image, new palette"
 * flash — the palette only changes once the old image is fully black. */
static void fe_wipe_transition(void)
{
    memcpy(fe_next, ff_fb, sizeof fe_next);         /* the composed NEW screen (stage 2) */
    memcpy(ff_fb, wp_old, sizeof wp_old);           /* restore the OLD visible screen */
    memcpy(GC.pal, fe_old_pal, sizeof fe_old_pal);  /* keep GC.pal == the live DAC (old) */
    ff_set_palette((const uint8_t (*)[3])fe_old_pal); /* to-black runs on the OLD palette */
    memset(wp_new, 0, sizeof wp_new);               /* stage 1 target = black */
    wp_active = 1; wp_dir = 1; wp_c = 0x13; wp_x = -1; wp_y = -1;
    fe_stage = 1;
}

/* fn0BA8_1315: FUN_1987_01ac page clear + contracting wipe = fade to black
 * (standalone, keeps the CURRENT palette — the next screen loads afterward). */
static void fe_wipe_black(void)
{
    memset(wp_new, 0, sizeof wp_new);
    wp_active = 1; wp_dir = 1; wp_c = 0x13; wp_x = -1; wp_y = -1;
    fe_stage = 0;
}

/* one wipe frame + the stage-1 -> stage-2 midpoint (palette swap on the black
 * screen, then arm the reveal of the composed NEW screen). Shared by
 * frontend_frame and frontend_wipe_frame. */
static void wipe_advance(void)
{
    wipe_ring();
    if (!wp_active && fe_stage == 1) {              /* to-black finished: swap palette + reveal */
        memcpy(GC.pal, fe_new_pal, sizeof fe_new_pal); /* GC.pal == the live DAC (new) */
        ff_set_palette((const uint8_t (*)[3])fe_new_pal);   /* 2a6f on the black screen */
        memcpy(wp_new, fe_next, sizeof wp_new);     /* stage 2 source = the NEW screen */
        wp_active = 1; wp_dir = 0; wp_c = 1; wp_x = 10; wp_y = 10;
        fe_stage = 2;
    } else if (!wp_active) {
        fe_stage = 0;
    }
}

/* fn13a8_0e13 name-entry locals (see FE_GAMEOVER / FE_HIGHSCORE):
 *   ne_slot = iVar6 (0..5 = the new table row, 6 = did not qualify)
 *   ne_pos  = local_8 (0..2 = letter cursor, 3 = done/entry disabled)
 *   ne_let  = local_5 (letter index 0..25 -> 'A'+idx)
 *   ne_rep  = local_4 (key-repeat cooldown), ne_rel = local_3 (reload 9 then 1)
 *   ne_ctr  = the *tF205 free-running frame counter (-> bBCBC, 1 tick/frame)   */
static int ne_slot = 6, ne_pos = 3, ne_let, ne_rep, ne_rel;
static u8  ne_ctr;

/* load a front-end CPT (VGA palette path, w24F9==4). Snapshots the OLD visible
 * screen + its (currently-live) palette, then loads the new screen + its palette
 * into ff_fb/GC.pal — but does NOT push the new palette to the live DAC. The
 * transition (fe_wipe_expand / fe_wipe_transition) decides WHEN the DAC flips,
 * exactly like fn13a8_0AC5 defers fn1987_2A6F to after the (optional) to-black.
 * (Headless screenshots read GC.pal, which is the new palette after the load.) */
static int fe_show_cpt(const char *name)
{
    char p[360];
    memcpy(wp_old, ff_fb, sizeof wp_old);       /* the previously VISIBLE screen */
    memcpy(fe_old_pal, GC.pal, sizeof fe_old_pal); /* the currently-live (OLD) palette */
    snprintf(p, sizeof p, "%s/cpt/%s", fe_assets, name);
    if (ff_load_ilbm_cpt_vga(p, ff_fb, GC.pal) != 0) {
        fprintf(stderr, "frontend: load %s failed\n", p);
        return 1;
    }
    memcpy(fe_new_pal, GC.pal, sizeof fe_new_pal); /* the NEW palette (applied by the wipe) */
    return 0;
}

/* credits digit: fn1187_0232(&('0'+tEA3A), 1, 0x128, 0xC0, 3) */
static void fe_draw_credits_digit(void)
{
    char d = (char)('0' + (int)Gw_credits);
    ff_font_draw(&d, 0x01, 0x0128, 0xC0, 0x03);
}

/* fn13a8_000b draw_text_row (full-screen variant, NO playfield +PF_Y offset —
 * unlike the in-race hud_stage_banner): each char -> a BOB sprite (digits '0'-'9'
 * -> 80-89, letters 'A'-'Z' -> 90-115), 0x10 px apart, centre-x / bottom-y. Space
 * (0x20) and the '#' name-entry cursor (0x23) are skipped in the display path. */
static void fe_text_row(const char *s, int x, int y)
{
    if (!GC.bob) return;
    for (; (u8)*s; ++s, x += 0x10) {
        int c = (u8)*s;
        if (c == 0x20 || c == 0x23) continue;
        int spr = c + 0x20;
        if (spr > 0x60) spr = c + 0x19;
        int w = 0, h = 0;
        ff_bob_dims(GC.bob, spr, &w, &h);
        if (w > 0 && h > 0) ff_bob_blit(GC.bob, spr, x - (w >> 1), y - h);
    }
}

/* fn13a8_0e13 display path (@4478-4496): HIGHSCOR.CPT + the 6-row score table.
 * Each row is the 14-char table string @0x27d9+i*0x0e ("N XXX SCORE"), with the
 * 7-digit score re-formatted from the numeric table (282d/282f) into name+6 by
 * fn18dc_004f, drawn via draw_text_row at (0x60, 0x6b + i*0x10). (Attract/display
 * only — the interactive name-entry loop is not part of the demo cycle.) */
static void fe_highscore_draw(void)
{
    fe_show_cpt("HIGHSCOR.CPT");
    int y = 0x6b;
    for (int i = 0; i < 6; ++i) {
        char *name = g_high.name[i];
        u32 sc = ((u32)g_high.score[i].hi << 16) | g_high.score[i].lo;
        for (int d = 6; d >= 0; --d) { name[6 + d] = (char)('0' + (sc % 10)); sc /= 10; }
        fe_text_row(name, 0x60, y);
        y += 0x10;
    }
}

/* fn0DAE_0001 — the title screen (TITRE.CPT + overlays + © sprite). */
void frontend_title_screen(void)
{
    fe_show_cpt("TITRE.CPT");                                  /* fn0BA8_0AC5 */
    ff_font_draw((char *)ffd_txt_deathconvoy, 0x10, 0x60, 100,  0x02); /* THE DEATH CONVOY */
    ff_font_draw((char *)ffd_txt_titus1990, 0x0A, 0x10, 0xC0, 0x03); /* TITUS 1990       */
    ff_font_draw((char *)ffd_txt_credits, 0x0A, 0xE0, 0xC0, 0x03); /* CREDITS 00       */
    fe_draw_credits_digit();                                    /* '0'+tEA3A @0x128 */
    /* fn0A0D_07CF(ds, x=8, y=200, idx=0x38) + fn1187_1C73: the © glyph is the
     * display-list sprite aDEEE[0x38] (BOB frame 56, 16x11) drawn with centre-x
     * at 8 and BOTTOM y at 200 — no playfield offset on the full-screen title. */
    {
        int w = 0, h = 0;
        ff_dir_dims(0x38, &w, &h);
        if (w > 0 && h > 0) ff_dir_blit(0x38, 8 - (w >> 1), 200 - h);
    }
    /* fn0BA8_1325 wipe — transition effect, not ported (settled pixels equal) */
}

/* menu entry (fn0DAE_0118 after the prologue): title + INSERT COINS + arm */
static void fe_menu_enter(void)
{
    frontend_title_screen();                                   /* fn0DAE_0001 */
    ff_font_draw((char *)ffd_txt_insertcoins, 0x0C, 0x70, 0xB0, 0x03); /* INSERT COINS */
    Gw_stage = 0x00;                                          /* level = 0    */
    Gi_tick_timer = 0x0F;                                          /* 15-tick timeout */
    fe_di = 0;
}

/* menu exit tail (fn0DAE_0118 after the loop): erase line, set demo flag,
 * auto-credit; then hand over to the race. */
static void fe_menu_exit(void)
{
    ff_font_draw((char *)ffd_txt_spaces20, 0x14, 0x50, 0xB0, 0x03); /* 20 spaces    */
    Gb_demo_flag = (Gi_tick_timer == 0x00) ? 0x01 : 0x00;            /* timeout -> DEMO */
    fe_wipe_black();                    /* fn0DAE_0118 @5396: fn0BA8_1315       */
    if (Gw_credits == 0x00) {
        Gw_credits = 0x01;                                      /* auto-credit  */
        fe_draw_credits_digit();
        Gi_tick_timer = 0x01;
        fe_state = FE_MENU_EXIT_WAIT;                           /* while(t24F1); */
        return;
    }
    fe_state = FE_RACE;
}

/* race hand-over: fn0DAE_05E2 setup_level_background + the post-menu stores.
 * game_start_level -> run_level_init builds all runtime tables + the VGA race
 * palette (and zeroes the scores / sets tDC68=4 exactly as the menu tail does:
 * ba78=4, 27CB/27C9/27D7/27D5=0). */
static void fe_start_race(void)
{
    Gw_best_hi = 0x00;                                          /* session best */
    Gw_best_lo = 0x00;
    Gb_demo_abort = 0x00;
    Gw_lives = 0x04;                                          /* lives = 4 (fn0DAE_0118 @179) */
    Gw_score_lo = 0x00; Gw_score_hi = 0x00;                       /* score = 0 (fn0DAE_0118 @180-181) */
    game_start_level(0);                                        /* fn0DAE_05E2 + run_level init */
    ff_set_palette((const uint8_t (*)[3])GC.pal);               /* race VGA palette */
    /* fn0BA8_1315 wipe-in — transition effect, not ported */
}

/* debug/testing: which front-end state owns the frame (the FeState ordinal;
 * 3 = FE_CONTINUE, 4 = FE_GAMEOVER, 5 = FE_HIGHSCORE, 7 = FE_MENU). */
int frontend_phase(void) { return (int)fe_state; }

void frontend_reset(const char *asset_dir)
{
    snprintf(fe_assets, sizeof fe_assets, "%s", asset_dir ? asset_dir : "assets");
    fe_state = FE_BOOT_TITUS;
    fe_entered = 0;
    fe_hold = 0;
    fe_isr18 = 0;
    fe_race_started = 0;
    fe_start_done = 0;
    wp_active = 0;
    fe_stage = 0;
    memset(wp_old, 0, sizeof wp_old);   /* cold boot: wipe in from black */
    memset(fe_old_pal, 0, sizeof fe_old_pal);   /* cold boot: from black */
    Gw_credits = 0x00;                 /* no credits at cold boot */
    Gb_demo_flag = 0x00;
    Gb_demo_abort = 0x00;
}

/* One frame of the front-end. Returns 1 while the frontend owns the frame,
 * 0 once the race (run_level_frame) should run instead. */
int frontend_frame(void)
{
    if (fe_state == FE_RACE && fe_race_started) return 0;

    fe_timer_frame();                  /* the ISR keeps ticking during wipes  */
    if (wp_active) { wipe_advance(); return 1; }   /* fn13a8_132F: one ring/frame + stage swap */
    /* the front-end polls input exactly like the original screens (they all call
     * fn0A0D_0002): keystate -> w24B7 (fire, insert credit / skip) and w24B9
     * (aux/start, begin normal play). In headless boot both stay 0 -> the menu
     * times out into the attract demo, unchanged. */
    ff_poll_controls();
    int fire  = (Gw_btn_fire != 0x00);
    int start = (Gw_btn_start != 0x00);

    switch (fe_state) {

    case FE_BOOT_TITUS:                /* main: TITUS.CPT + asset load */
        if (!fe_entered) { fe_show_cpt("TITUS.CPT"); fe_wipe_expand(); fe_entered = 1; break; }
        if (++fe_hold >= FE_BOOT_HOLD) {
            fe_state = FE_BOOT_PRES; fe_entered = 0; fe_hold = 0;
        }
        break;

    case FE_BOOT_PRES:                 /* main: PRES.CPT before the loop */
        if (!fe_entered) { fe_show_cpt("PRES.CPT"); fe_wipe_expand(); fe_entered = 1; break; }
        if (++fe_hold >= FE_BOOT_HOLD) {
            /* main enters the while(true){ menu; race } loop */
            fe_state = FE_MENU; fe_entered = 0; fe_hold = 0;
        }
        break;

    case FE_CUTSCENE:                  /* fn13a8_048a round_transition (NUC scene) */
        if (!round_transition_frame()) {
            fe_entered = 0;
            fe_wipe_black();           /* fn13a8_048a tail @4251: fn0BA8_1315 */
            /* game_main @961-964: tEA3A!=0 -> the CONTINUE screen (fn0DAE_02D0);
             * ==0 -> the highscore path (fn13a8_0e13). */
            fe_state = (Gw_credits != 0) ? FE_CONTINUE : FE_GAMEOVER;
        }
        break;

    case FE_CONTINUE: {                /* fn0DAE_02D0: CONTINUE? countdown         */
        if (!fe_entered) {
            frontend_title_screen();                            /* fn0DAE_0001     */
            ff_font_draw((char *)ffd_txt_continue, 0x0C, 0x70, 0xB0, 0x03); /* CONTINUE//08 */
            Gi_tick_timer = 0x09;         /* 9-tick (~9 s) countdown; the `while(==9)`
                                        * tick-sync spin is timing-only, dropped   */
            fe_entered = 1;
            fe_wipe_transition();      /* fn0DAE_0001 tail @5300 (param_3=1): to-black + 1325 */
            break;
        }
        int done = 0;
        if (fire) done = 1;                                     /* w24B7: CONTINUE */
        if (Gi_tick_timer < 0) { Gw_credits = 0x00; done = 1; }    /* expired         */
        {                              /* live countdown digit: '0'+t24F1 @ (200,0xB0) */
            char d = (char)('0' + Gi_tick_timer);
            ff_font_draw(&d, 0x01, 200, 0xB0, 0x03);
        }
        if (done) {
            fe_entered = 0;
            if (Gw_credits != 0) {
                /* CONTINUED: session-best 27D5/27D7 (+ HIGH 27CD/27CF) update
                 * (fn0DAE_02D0 tail, 32-bit compares), then game_main @967-969
                 * resets + loop back into run_level on the SAME stage. */
                if ((i16)Gw_best_hi <= (i16)Gw_score_hi &&
                    ((i16)Gw_best_hi < (i16)Gw_score_hi || Gw_best_lo < Gw_score_lo)) {
                    Gw_best_hi = Gw_score_hi; Gw_best_lo = Gw_score_lo;
                    if ((i16)Gw_high_hi <= (i16)Gw_score_hi &&
                        ((i16)Gw_high_hi < (i16)Gw_score_hi || Gw_high_lo < Gw_score_lo)) {
                        Gw_high_hi = Gw_score_hi; Gw_high_lo = Gw_score_lo;
                    }
                }
                Gw_score_lo = 0x00; Gw_score_hi = 0x00;           /* @967-968 score=0 */
                Gw_lives = 0x04;                              /* @969 lives=4     */
                game_start_level((int)Gw_stage);              /* re-enter the SAME stage */
                /* the to-black runs on the CONTINUE palette (fn0DAE_02D0 @5443);
                 * the race palette (in GC.pal now) is pushed to the DAC on the
                 * black screen at the FE_RACE first-frame reveal — not here. */
                fe_wipe_black();       /* fn0DAE_02D0 tail @5443: fn0BA8_1315      */
                fe_start_done = 1;     /* race set up here — skip fe_start_race    */
                fe_state = FE_RACE;
                break;
            }
            fe_state = FE_GAMEOVER;                             /* no continue      */
        }
        break;
    }

    case FE_GAMEOVER:                  /* fn13a8_0e13 @4437: GAMEOVER.CPT, t24F1=6 */
        if (!fe_entered) {
            if (Gb_demo_abort != 0x00) {  /* 0e13 gate: demo aborted -> skip the screens */
                Gw_score_lo = 0x00; Gw_score_hi = 0x00; Gw_lives = 0x04;
                frontend_reenter();
                break;
            }
            fe_show_cpt("GAMEOVER.CPT");
            Gi_tick_timer = 0x06;
            Gw_stage = 0x00;                          /* stage back to 0          */
            {                                           /* fn0DAE_05E2: DECA reload */
                char cp[360];
                snprintf(cp, sizeof cp, "%s/cpt", fe_assets);
                decor_load(cp, 0);
            }
            Gb_exit_flag = 0x00;
            /* restore the session best into the score (32-bit compare @0e13) */
            if ((i16)Gw_score_hi <= (i16)Gw_best_hi &&
                ((i16)Gw_score_hi < (i16)Gw_best_hi || Gw_score_lo < Gw_best_lo)) {
                Gw_score_hi = Gw_best_hi; Gw_score_lo = Gw_best_lo;
            }
            /* find the table slot: first entry with score <= ours */
            {
                int i;
                for (i = 0; i < 6; ++i) {
                    u16 thi = g_high.score[i].hi, tlo = g_high.score[i].lo;
                    if ((i16)thi <= (i16)Gw_score_hi &&
                        ((i16)thi < (i16)Gw_score_hi || tlo <= Gw_score_lo)) break;
                }
                ne_slot = i;
                if (i < 6) {                            /* shift down + insert      */
                    for (int k = 5; k > i; --k) {
                        g_high.score[k] = g_high.score[k - 1];
                        for (int c = 2; c < 5; ++c)     /* the 3 name chars         */
                            g_high.name[k][c] = g_high.name[k - 1][c];
                    }
                    g_high.score[i].hi = Gw_score_hi;
                    g_high.score[i].lo = Gw_score_lo;
                    for (int c = 2; c < 5; ++c) g_high.name[i][c] = ' ';
                }
            }
            fe_entered = 1;
            fe_wipe_transition();      /* fn13a8_0e13 @4438 (param_3=1): to-black + 1325 */
            break;
        }
        if (fire) Gi_tick_timer = 0x00;
        if (Gi_tick_timer <= 0x00) { fe_entered = 0; fe_state = FE_HIGHSCORE; }
        break;

    case FE_HIGHSCORE: {               /* fn13a8_0e13 @4478: HIGHSCOR.CPT + table, t24F1=0x1e */
        char *nm = g_high.name[ne_slot < 6 ? ne_slot : 5];  /* the new entry's row
                                                             * (slot 6 = "did not
                                                             * qualify": row unused,
                                                             * entry disabled) */
        int boxy = ne_slot * 0x10;                          /* iVar4               */
        if (!fe_entered) {
            fe_highscore_draw();
            Gi_tick_timer = 0x1E;
            if (ne_slot < 6) {          /* highlight box behind the new row (fill_rects) */
                for (int y = boxy + 0x5E; y <= boxy + 0x6C; ++y)
                    for (int x = 0x76; x <= 0xA9; ++x) ff_fb[(long)y * FF_W + x] = 0;
                for (int y = boxy + 0x5F; y <= boxy + 0x6B; ++y)
                    for (int x = 0x77; x <= 0xA8; ++x) ff_fb[(long)y * FF_W + x] = 2;
                fe_text_row(nm, 0x60, boxy + 0x6B);         /* redraw the row over it */
                ne_pos = 0;
            } else {
                ne_pos = 3;             /* no entry: display-only hold              */
            }
            ne_let = 0; ne_rep = 1; ne_rel = 9; ne_ctr = 0;
            fe_entered = 1;
            fe_wipe_transition();      /* fn13a8_0e13 @4497 (param_3=1): to-black + 1325 */
            break;
        }
        /* fn13a8_0e13 name-entry loop body (one iteration per frame; *tF205 -> ne_ctr).
         * Keys: UP (w24BB) prev letter, DOWN (w24BD) next, LEFT (w24C1) backspace,
         * FIRE (w24B7) or RIGHT (w24BF) accept; timeout t24F1 (0x1E ticks).        */
        int exit_hs = 0;
        if (Gw_btn_left != 0 && ne_pos != 0 && ne_rep == 0) {   /* LEFT: backspace   */
            nm[2 + ne_pos] = '#';
            ne_pos -= 1;
            ne_rep = ne_rel; ne_rel = 1; ne_ctr = 0;
        }
        if (ne_pos < 3) {
            if (Gw_btn_accel == 0) {
                if (Gw_btn_brake == 0) {
                    if (Gw_btn_left == 0) { ne_rep = 0; ne_rel = 9; }
                } else if (ne_rep == 0) {                      /* DOWN: next letter */
                    ne_let = (ne_let == 25) ? 0 : ne_let + 1;
                    ne_rep = ne_rel; ne_rel = 1; ne_ctr = 0;
                }
            } else if (ne_rep == 0) {                          /* UP: prev letter   */
                ne_let = (ne_let == 0) ? 25 : ne_let - 1;
                ne_rep = ne_rel; ne_rel = 1; ne_ctr = 0;
            }
            if (ne_rep != 0) {          /* cursor box (fn0A0D_098C entry, colour 2) */
                int x0 = ne_pos * 0x10 + 0x78, x1 = ne_pos * 0x10 + 0x87;
                for (int y = boxy + 0x60; y <= boxy + 0x6A; ++y)
                    for (int x = x0; x <= x1; ++x) ff_fb[(long)y * FF_W + x] = 2;
            }
        }
        {   /* FIRE/RIGHT: accept — the original spins `while (fire||right) poll;`
             * after acting (a blocking release wait), so a held key acts ONCE.
             * Frame model: act on the RISING edge only. */
            static int ne_acc_prev;
            int acc = (fire || Gw_btn_right != 0);
            if (acc && !ne_acc_prev) {
                ne_rep = 1;
                if (ne_pos == 3) exit_hs = 1;
                else { nm[2 + ne_pos] = (char)('A' + ne_let); ne_pos += 1; }
            }
            ne_acc_prev = acc;
        }
        if (ne_pos < 3) {
            if (ne_rep == 0) {
                nm[2 + ne_pos] = (char)((ne_ctr & 8) ? 'A' + ne_let : '#');  /* blink */
            } else {
                Gi_tick_timer = 0x1E;      /* activity resets the timeout               */
                nm[2 + ne_pos] = (char)('A' + ne_let);
            }
            if ((i8)ne_rep < (i8)ne_ctr) ne_rep = 0;           /* repeat delay over  */
        }
        if (ne_slot < 6) fe_text_row(nm, 0x60, boxy + 0x6B);   /* redraw the row     */
        if (Gi_tick_timer < 0 || Gb_exit_flag != 0) {               /* timeout            */
            exit_hs = 1;
            if (ne_pos < 3) nm[2 + ne_pos] = ' ';
        }
        ne_ctr += 1;                                           /* the bBCBC tick     */
        if (exit_hs) {
            fe_entered = 0;
            if (ne_slot < 6) {          /* strip '#', SAVE the HIGH file (fn0A0D_0483) */
                for (int c = 2; c < 5; ++c)
                    if (nm[c] == '#') nm[c] = ' ';
                ff_save_high(fe_assets);
            }
            if ((i16)Gw_high_hi <= (i16)Gw_score_hi &&          /* HIGH display update */
                ((i16)Gw_high_hi < (i16)Gw_score_hi || Gw_high_lo < Gw_score_lo)) {
                Gw_high_hi = Gw_score_hi; Gw_high_lo = Gw_score_lo;
            }
            ne_slot = 6; ne_pos = 3;
            /* game_main @967-969, AFTER the highscore path: score = 0, lives = 4. */
            Gw_score_lo = 0x00; Gw_score_hi = 0x00;
            Gw_lives = 0x04;
            frontend_reenter();
        }
        break;
    }

    case FE_DEMO_SCREENS: {            /* fn0DAE_0118 prologue (bDC6F && !bF6B3) */
        static const char *names[3] = { "DROGUE.CPT", "TITUS.CPT", "PRES.CPT" };
        if (!fe_entered) {
            fe_show_cpt(names[fe_sub]);
            Gi_tick_timer = 0x05;         /* fn0DAE_00D5: 5 ticks or input */
            fe_entered = 1;
            fe_wipe_transition();      /* fn0DAE_00D5 @5313 (param_3=1): to-black + 1325 */
            break;
        }
        if (fire) Gi_tick_timer = 0x00;   /* w24B7 || t24B9 -> skip */
        if (Gi_tick_timer == 0x00) {
            fe_entered = 0;
            if (++fe_sub >= 3) { fe_state = FE_MENU; }
        }
        break;
    }

    case FE_MENU:                      /* fn0DAE_0118 main loop */
        if (!fe_entered) { fe_menu_enter(); fe_entered = 1; fe_wipe_transition(); break; }

        if (fe_di == 0) {              /* iVar3 state machine */
            if (Gw_credits != 0x00) fe_di = 1;
        } else if (fe_di == 1) {
            ff_font_draw((char *)ffd_txt_pressbutton, 0x14, 0x50, 0xB0, 0x03); /* PRESS BUTTON TO PLAY */
            fe_di = 2;
        }

        if (fire) {                    /* w24B7: insert credit / start */
            Gi_tick_timer = 0x0A;
            if (Gw_credits < 0x03) {
                Gw_credits = (u16)(Gw_credits + 1);
                fe_draw_credits_digit();
                fe_state = FE_MENU_RELEASE;      /* while (w24B7) poll; */
            } else {
                fe_entered = 0;
                fe_menu_exit();                   /* si = 1 -> exit    */
            }
            break;
        }
        /* fn0DAE_0118 @146: aux/start (t24B9) OR timeout -> exit the menu loop.
         * fe_menu_exit sets bDC6F = (t24F1==0), so a real START keeps bDC6F=0
         * (a human plays level 1); only a timeout arms the attract DEMO. */
        if (start || Gi_tick_timer == 0x00) {
            fe_entered = 0;
            fe_menu_exit();
        }
        break;

    case FE_MENU_RELEASE:              /* wait for fire release, then resume */
        if (!fire) fe_state = FE_MENU;
        break;

    case FE_MENU_EXIT_WAIT:            /* t24F1=1; while (t24F1 != 0) ; */
        if (Gi_tick_timer <= 0x00) fe_state = FE_RACE;
        break;

    case FE_RACE:
        break;                         /* handover handled below */
    }

    if (fe_state == FE_RACE && !wp_active && !fe_race_started) {
        /* HANDOVER + the race FIRST-FRAME REVEAL (fn0869_0006 @1928-1936: on
         * the first frame — local_28, the same first-frame flag as the HUD
         * init — the page flip is replaced by fn0BA8_1325, so race frame 0 is
         * revealed by the expanding spiral while the race logic waits the 10
         * ring syncs; the ISR keeps ticking, which the calibrated s_isr18
         * entry phase already absorbs). */
        if (!fe_start_done) fe_start_race();
        fe_start_done = 0;
        memcpy(wp_old, ff_fb, sizeof wp_old);   /* the to-black wipe result */
        run_level_frame();                      /* race frame 0             */
        fe_wipe_expand();
        fe_race_started = 1;
        return 1;
    }
    return 1;
}

/* Re-enter the front-end after a finished game/demo cycle (main's outer loop).
 * With bDC6F set (attract timeout) and bF6B3 clear, fn0DAE_0118 first shows the
 * three DROGUE/TITUS/PRES screens, then re-arms the attract demo. */
void frontend_reenter(void)
{
    fe_entered = 0;
    fe_hold = 0;
    fe_sub = 0;
    fe_race_started = 0;
    fe_start_done = 0;
    fe_state = (Gb_demo_flag != 0x00 && Gb_demo_abort == 0x00) ? FE_DEMO_SCREENS
                                                          : FE_MENU;
}

/* fn0DAE_03AA start_level — the STAGE-CLEAR interlude + next-stage setup, run from
 * game_main's `bDC6E!=0` branch after run_level returns (the boss was killed, so
 * anim_step is set and — unlike the attract demo — NOT cleared). Ported from
 * FUN_15ae_03aa (ghidra @5450): t24F1=0x14; t27C7 += bDC6E (advance the stage);
 * when t27C7 reaches 5 all stages are cleared -> wrap to 0 + CONG.CPT (the
 * congratulations screen); otherwise load ETAPE.CPT and draw the two-line interlude
 * "YOU GOT ME" @(0x50,0x23) + "BUT I AM NOT DEAD" @(0x20,0xba) (fn13a8_000b, the
 * full-screen draw_text_row = fe_text_row). Then setup_level_background (fn0DAE_05E2:
 * reload the new stage's DEC<level>.CPT décor). run_level_init (rebuild the per-stage
 * tables + the GAMEPLAY palette, reset the race — SCORE/LIVES carry) is DEFERRED to
 * the CALLER, run AFTER the modal hold, so the ETAPE palette stays active for the
 * interlude and the gameplay palette is only built when the stage begins (mirrors the
 * original: run_level re-enters after start_level's hold loop). The modal hold
 * (`while (t24F1>=0 && !fire)` + wipe fn0BA8_1325/1315) is presentation only. Returns 1
 * to continue to the next stage, 0 when the game is complete (stages wrapped to 0). */
int frontend_stage_advance(void)
{
    Gi_tick_timer = 0x14;                                    /* t24F1 = 20 (interlude hold) */
    Gw_stage = (u16)(Gw_stage + Gb_stage_clear);          /* t27C7 += anim_step -> stage++ */
    int cont = 1;
    if (Gw_stage == 0x05) {                             /* all 5 stages cleared */
        Gw_stage = 0x00;
        fe_show_cpt("CONG.CPT");                          /* fn13a8_0ac5 CONG.CPT */
        cont = 0;
    } else {
        fe_show_cpt("ETAPE.CPT");                         /* fn13a8_0ac5 ETAPE.CPT (sets the ETAPE palette) */
        fe_text_row((char *)ffd_txt_yougotme, 0x50, 0x23);    /* "YOU GOT ME"        */
        fe_text_row((char *)ffd_txt_notdead, 0x20, 0xBA);    /* "BUT I AM NOT DEAD" */
        /* fn0DAE_05E2 setup_level_background: reload the new stage's roadside décor
         * (DEC<'A'+level>.CPT). NOTE: run_level_init (which rebuilds the GAMEPLAY
         * palette + resets the race) is DEFERRED to the caller, run AFTER the
         * interlude hold — exactly as the original re-enters run_level after
         * start_level's hold loop. Keeping it out here leaves the ETAPE palette
         * active for the interlude screen (the palette fix). */
        char cp[360];
        snprintf(cp, sizeof cp, "%s/cpt", fe_assets);
        decor_load(cp, (int)Gw_stage);
    }
    fe_wipe_transition();   /* fn0DAE_03AA @5466: fn13a8_0AC5 (param_3=1) to-black on
                             * the last-race palette -> ETAPE palette -> 1325 reveal;
                             * the @5474 stage-out 1315 + the next race's first-frame
                             * reveal stay CUT in the game_app interlude path. */
    return cont;
}

/* animate a pending spiral wipe outside frontend_frame (the game_app interlude
 * hold): applies one ring into ff_fb (+ the stage-1 -> stage-2 palette swap),
 * returns 1 while the wipe owns the frame. */
int frontend_wipe_frame(void)
{
    if (!wp_active) return 0;
    wipe_advance();
    return 1;
}

/* game_main death branch (@958-970), run when run_level_frame() signals the race
 * ended (fn1069_0006 returned with DC6E==0). Ports the resets: credit/players
 * tEA3A--, score zeroed (already done in run_level's exit epilogue), lives tDC68=4;
 * then re-enters the front end so the attract cycle repeats (menu → demo). The
 * post-race GAME OVER + HIGH SCORE screens (fn13a8_048a/fn13a8_0e13) are the
 * follow-up visual layer; the LOOP itself is faithful here. */
void frontend_post_race(void)
{
    if ((i16)Gw_credits != 0) Gw_credits = (u16)(Gw_credits - 1);  /* c84a-- (@959) */
    /* game_main death branch order (@960-969): FUN_13a8_048a (round_transition —
     * the NUC cutscene, PORTED in game/cutscene.c; skipped when bF6B3 set) ->
     * FUN_13a8_0e13 (GAME OVER -> HIGH SCORE) -> score=0/lives=4 -> the menu.
     * (The tEA3A!=0 CONTINUE screen fn0DAE_02D0 is not in the attract path.) */
    round_transition_start();
    fe_entered = 0;
    fe_race_started = 0;
    fe_start_done = 0;
    fe_state = FE_CUTSCENE;
}
