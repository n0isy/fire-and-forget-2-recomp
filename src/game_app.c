/* game_app.c — entry point for the playable level build (native + WASM + headless). */
#include "game/ff_game.h"
#include "game/gnames.h"
#include "game/data/gamedata.h"   /* ffd_txt_* named game strings */
#include "render/ff_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static int g_quit;
static int g_interlude;   /* frames left to hold a stage-clear interlude screen */

/* The QEMU reference capture is the ATTRACT demo: the menu timeout leaves
 * bDC6F=1 (demo flag) and tEA3A=1 (auto-credit) before run_level. The headless
 * modes (shot/trace/ents/road) start the race directly, so they must begin
 * from the same pre-race state (run_level_init already ran in game_init;
 * these flags are read per frame by panel_objective_triggers / run_level). */
static void attract_state(void) {
    Gb_demo_flag = 0x01;
    Gw_credits = 0x01;
    Gw_lives = 0x04;                  /* lives = 4 (menu/game-start, carries stages) */
    Gw_score_lo = 0x00; Gw_score_hi = 0x00;  /* score = 0 at game start (carries stages) */
}

/* One presented frame: the boot front-end (TITUS -> PRES -> title/menu) owns
 * the frame until the menu hands over to the race (fn0DAE_0118 -> fn0869_0006),
 * exactly like main's `while(true){ menu; do race... }` loop. */
static void frame(void) {
    if (!ff_poll()) {
        g_quit = 1;
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
        /* web: the game has quit (Esc) — hand off to the page. The shell's
         * ffOnQuit() reloads the page, restarting the game from cold boot
         * (wasm/data come from cache, so the restart is near-instant). The
         * native build keeps the plain quit; the game code is untouched. */
        emscripten_run_script("if (window.ffOnQuit) setTimeout(window.ffOnQuit, 0);");
#endif
        return;
    }
    for (int k = 0; k < FF_K_COUNT; k++) GC.input[k] = ff_key(k);
    /* interlude hold: after a STAGE CLEAR, show the "YOU GOT ME" / ETAPE.CPT screen
     * frontend_stage_advance() left in ff_fb for a beat (skippable with FIRE),
     * before the next stage's run_level frames overwrite it. */
    if (g_interlude > 0) {
        if (frontend_wipe_frame()) { ff_present(); return; }   /* ETAPE spiral reveal */
        if (GC.input[FF_K_FIRE]) g_interlude = 0; else --g_interlude;
        if (g_interlude == 0) {                 /* interlude over -> begin the stage */
            run_level_init();                   /* gameplay palette + fresh stage state */
            g_exhaust_phase = 1;                /* local_6 stack residue (start_level path) */
            decor_set_accum(0);                 /* wFFFFFFF2 residue (start_level path) */
            ff_set_palette((const uint8_t(*)[3])GC.pal);
        }
        ff_present();
        return;
    }
    /* main's outer loop (fn103c_0009): frontend (menu/boot) OR the race. When
     * run_level_frame signals the race ended (fn1069_0006 returned), branch like
     * game_main: bDC6E!=0 (boss killed / stage clear) -> frontend_stage_advance()
     * (fn0DAE_03AA start_level, advance to the next stage); else the death path ->
     * frontend_post_race() (attract loop / GAME OVER). */
    if (!frontend_frame()) {
        if (!run_level_frame()) {
            if (Gb_stage_clear != 0 && frontend_stage_advance())
                g_interlude = 40;                 /* hold the stage-clear interlude */
            else
                frontend_post_race();
        }
    }
    ff_present();
}

int main(int argc, char **argv) {
    const char *dir = getenv("FF2_ASSETS");
    char assets[256];
    snprintf(assets, sizeof assets, "%s", dir ? dir : "assets");

    /* attract loop: drive the full frame() logic (frontend + race + post-race)
     * headlessly and confirm the demo LOOPS (boot → race 2041 → post-race → menu →
     * race → ...). Logs each phase transition + race end; stops after 2 races. */
    if (argc >= 2 && strcmp(argv[1], "--loop") == 0) {
        game_init(assets);
        frontend_reset(assets);
        int races = 0, prev = -1;
        for (int i = 0; i < 20000 && races < 2; i++) {
            int fe = frontend_frame();
            if (fe != prev) { printf("  @%-6d phase=%s\n", i, fe ? "FRONTEND" : "RACE"); prev = fe; }
            if (!fe) {
                if (!run_level_frame()) {
                    printf("  @%-6d RACE %d ENDED (DE50=%d DC6E=%d EA3A=%d 27C7=%d) -> post-race\n",
                           i, races + 1, Gb_panel_msg, Gb_stage_clear, Gw_credits, Gw_stage);
                    frontend_post_race();
                    races++;
                }
            }
        }
        printf("saw %d race(s) in the attract loop\n", races);
        return 0;
    }

    /* 3x attract-loop phase check: drive the full frame() logic through 3 demo
     * cycles; for each race emit race-relative STATE rows ("D,<race>,<rf>,...") so
     * each cycle's demo can be diffed vs the reference capext (proving the loop
     * re-runs the identical demo IN PHASE, with no state leak between cycles), plus
     * the race-start frame of each cycle (the steady-state period). */
    if (argc >= 2 && strcmp(argv[1], "--loop3") == 0) {
        game_init(assets);
        frontend_reset(assets);
        printf("D,race,rf,EA3C,E9CC,F6C4,DD84,DD86,DE50,DE51,DE52,DE53,F461,F6EC,F682,F6AF,27C9,27CB,EF50,F688,24F1,F468,DC7E,EF4A,DE5D,379F,F6D8,F46A,DC6C\n");
        int race = 0, rf = 0, prevfe = 1;
        for (int i = 0; i < 14000 && race < 3; i++) {
            int fe = frontend_frame();
            if (fe != prevfe) { if (!fe) printf("S,%d,%d\n", race, i); prevfe = fe; }
            if (!fe) {
                if (run_level_frame()) {
                    printf("D,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                        race, rf, Gw_dist_lo, Gw_speed, Gd_objective, Gw_car_x, Gw_horizon,
                        Gb_panel_msg, Gb_panel_delay, Gb_panel_state, Gw_panel_cursor, Gb_rle_count, Gb_input_mask,
                        Gw_rle_ptr, Gw_game_mode, Gw_score_lo, Gw_score_hi, Gw_mtn_scroll, Gb_obj_done,
                        Gw_tick_timer, Gw_decor_density, Gw_decor_count, Gw_ring_head, Gw_ring_tail,
                        Gw_decor_kind_cur, Gw_decor_latch, Gw_enemy_count, Gw_fuel_window);
                    rf++;
                } else {
                    frontend_post_race();
                    race++; rf = 0;
                }
            }
        }
        return 0;
    }

    /* spawn-injection enemy test: drive NORMAL level-1 play from the tape and, at a
     * fixed frame, spawn ONE enemy of a chosen type (spawn_enemy_rec, deterministic
     * record [type,0xC8,0,0,0x10]) — then dump that slot's per-frame AI state. The
     * QEMU harness (qemu/cap_spawntest.py) replicates the exact same pool-slot write at
     * the same frame in the ORIGINAL, so the two slot traces can be bit-compared to
     * validate a ported enemy handler that the attract demo never exercises.
     * `--spawntest <tape> <type> <spawn_frame> <N>` */
    if (argc >= 6 && strcmp(argv[1], "--spawntest") == 0) {
        FILE *tf = fopen(argv[2], "rb");
        if (!tf) { fprintf(stderr, "cannot open tape %s\n", argv[2]); return 1; }
        fseek(tf, 0, SEEK_END); long tlen = ftell(tf); fseek(tf, 0, SEEK_SET);
        u8 *tape = malloc(tlen ? (size_t)tlen : 1);
        if (fread(tape, 1, (size_t)tlen, tf) != (size_t)tlen) { fclose(tf); return 1; }
        fclose(tf);
        int type = atoi(argv[3]), sf = atoi(argv[4]), n = atoi(argv[5]);
        int hpov = (argc >= 7) ? atoi(argv[6]) : -1;  /* optional hp override (death-branch test) */
        game_init(assets);
        frontend_reset(assets);
        Gb_demo_flag = 0x00; Gw_credits = 0x01; Gw_lives = 0x04;
        Gw_score_lo = 0x00; Gw_score_hi = 0x00;
        /* L3-L5 content test (FF2_STAGE env): inject t27C7 = stage and re-run
         * the run_level prologue (re-arms the wave VM with script dir[stage] +
         * rebuilds the palette) — mirrors the QEMU-side injection at the
         * run_level ENTRY (cap_spawntest.py STAGE env). Décor stays DECA in
         * BOTH engines (the reload lives in the menu/start_level path). */
        { const char *e = getenv("FF2_STAGE");
          if (e) { Gw_stage = (u16)atoi(e); run_level_init(); } }
        demo_input_set_tape(tape, (int)tlen);
        int slot = -1;
        /* per frame: a 'G' globals line (DE50 = the LEADER-SHOT-DOWN panel msg, DC6E anim_step),
         * then an 'E' line for EVERY active pool slot (type/di/scrX/scrY/vz/hp/shape) — so a boss
         * DEATH + its 4-way break-apart sub-explosions are all captured. Compare aligns [i]==[i+1]. */
        printf("G,frame,DE50,DC6E,F46A,27C9,EA3C,DC6C,F6AF,DC68\n");
        for (int f = 0; f < n; f++) {
            if (!run_level_frame()) break;
            if (f == sf) {                          /* inject the test enemy */
                for (int k = 0; k < 20; k++)
                    if ((i8)*(ENT_BASE + k * 0x33) < 0) { slot = k; break; }
                if (type >= 0) {
                    unsigned char rec[5] = { (unsigned char)type, 0xC8, 0x00, 0x00, 0x10 };
                    spawn_enemy_rec(rec);
                    if (hpov >= 0 && slot >= 0) *(ENT_BASE + slot*0x33 + 0x2f) = (u8)hpov;  /* hp override */
                }
                fprintf(stderr, "spawned type %d in slot %d @frame %d hp=%d\n", type, slot, f,
                        slot >= 0 ? *(ENT_BASE + slot*0x33 + 0x2f) : -1);
            }
            if (f >= sf) {                          /* dump globals + the FULL active pool every frame */
                printf("G,%d,%d,%d,%d,%u,%u,%d,%d,%d\n", f, Gb_panel_msg, Gb_stage_clear, (i16)Gw_enemy_count,
                       Gw_score_lo, Gw_dist_lo, (i16)Gw_fuel_window, Gw_game_mode, (i16)Gw_lives);
                /* hidden-field debug line (FF2_XDUMP=lo:hi): the crash-decision
                 * inputs — flash timer F6A9, hit flag F6B1, crash ctr DE6D,
                 * steering DD84, flash colour F203 + slot-0 x_accum (+7). */
                { static int xlo = -2, xhi = -2;
                  if (xlo == -2) { const char *e = getenv("FF2_XDUMP");
                                   if (!e || sscanf(e, "%d:%d", &xlo, &xhi) != 2) { xlo = xhi = -1; } }
                  if (f >= xlo && f <= xhi)
                      printf("X,%d,%d,%d,%d,%d,%d,%d\n", f, (i16)Gw_flash_timer, Gb_player_hit,
                             Gb_crash_ctr, (i16)Gw_car_x, (i16)Gw_flash_colour,
                             (i16)*(u16 *)(ENT_BASE + 7)); }
                for (int k = 0; k < 20; k++) {
                    u8 *s = ENT_BASE + k*0x33;
                    if ((i8)s[0] < 0) continue;
                    printf("E,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", f, k, (i8)s[0],
                        (i16)*(u16*)(s+3), (i16)*(u16*)(s+1), (i16)*(u16*)(s+5),
                        (i16)*(u16*)(s+0x11), s[0x2f], s[0x2e]);
                }
                if (0) slot = slot;   /* (slot tracked only for the spawn msg) */
            }
        }
        return 0;
    }

    /* external-tape replay: drive the game from a per-frame key-mask file
     * (replay_l1.tape = the level-1 recording decoded from the RLE @0x2849) instead
     * of the embedded RLE. `--replay <tape> <N> [play]`: run N frames dumping the
     * --trace2 state. Default = attract (bDC6F=1, matches the demo bit-for-bit);
     * `play` = NORMAL play (bDC6F=0, score kept). In play mode it now CONTINUES PAST
     * level 1 through the STAGE branch (frontend_stage_advance = fn0DAE_03AA) into
     * stage 2, consuming the tape (the interlude consumes no tape frames, so the
     * frame index stays continuous). Optional 5th arg = a PPM to dump the stage-clear
     * interlude screen ("YOU GOT ME" / ETAPE.CPT). */
    if (argc >= 4 && strcmp(argv[1], "--replay") == 0) {
        FILE *tf = fopen(argv[2], "rb");
        if (!tf) { fprintf(stderr, "cannot open tape %s\n", argv[2]); return 1; }
        fseek(tf, 0, SEEK_END); long tlen = ftell(tf); fseek(tf, 0, SEEK_SET);
        u8 *tape = malloc(tlen ? (size_t)tlen : 1);
        if (fread(tape, 1, (size_t)tlen, tf) != (size_t)tlen) { fclose(tf); return 1; }
        fclose(tf);
        int n = atoi(argv[3]);
        int play = (argc >= 5 && strcmp(argv[4], "play") == 0);
        const char *ilude = (argc >= 6) ? argv[5] : NULL;   /* interlude PPM dump */
        const char *endshot = (argc >= 7) ? argv[6] : NULL; /* final gameplay-frame PPM dump */
        game_init(assets);
        frontend_reset(assets);                               /* set the asset dir (fe_show_cpt/decor) */
        if (play) { Gb_demo_flag = 0x00; Gw_credits = 0x01;     /* NORMAL level-1 play */
                    Gw_lives = 0x04; Gw_score_lo = 0x00; Gw_score_hi = 0x00; }  /* lives/score game-start */
        else      attract_state();                            /* attract (== the demo) */
        demo_input_set_tape(tape, (int)tlen);
        /* per-frame PPM dumps for PIXEL verification: FRAMEPPM_DIR + FRAMEPPM_LIST="500,2040,..." */
        int ppf[128], ppn = 0; const char *ppdir = getenv("FRAMEPPM_DIR");
        { const char *e = getenv("FRAMEPPM_LIST");
          if (e && ppdir) { char b[1024]; snprintf(b, sizeof b, "%s", e); char *p = b;
            while (*p && ppn < 128) { while (*p == ',' || *p == ' ') p++; if (!*p) break;
              ppf[ppn++] = atoi(p); while (*p && *p != ',') p++; } } }
        /* full pool dump (E,frame,slot,type,di,scrX,scrY,vz,hp,shape) over POOLDUMP="lo:hi" */
        int fdlo = -1, fdhi = -1; { const char *e = getenv("POOLDUMP"); if (e) sscanf(e, "%d:%d", &fdlo, &fdhi); }
        /* EVERY-frame PPM dumps over a range: FRAMEPPM_ALL="lo:hi" (+FRAMEPPM_DIR) —
         * the unfiltered end-to-end pixel tests (idle fuel-out run / 1.5-level tape) */
        int palo = -1, pahi = -1; { const char *e = getenv("FRAMEPPM_ALL"); if (e) sscanf(e, "%d:%d", &palo, &pahi); }
        printf("frame,EA3C,E9CC,F6C4,DD84,DD86,DE50,DE51,DE52,DE53,F461,F6EC,F682,F6AF,27C9,27CB,EF50,F688,24F1,F468,DC7E,EF4A,DE5D,379F,F6D8,F46A,DC6C,DC6E,27C7\n");
        int f = 0;
        while (f < n) {
            if (run_level_frame()) {
                printf("%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", f,
                    Gw_dist_lo, Gw_speed, Gd_objective, Gw_car_x, Gw_horizon,
                    Gb_panel_msg, Gb_panel_delay, Gb_panel_state, Gw_panel_cursor, Gb_rle_count, Gb_input_mask,
                    Gw_rle_ptr, Gw_game_mode, Gw_score_lo, Gw_score_hi, Gw_mtn_scroll, Gb_obj_done,
                    Gw_tick_timer, Gw_decor_density, Gw_decor_count, Gw_ring_head, Gw_ring_tail,
                    Gw_decor_kind_cur, Gw_decor_latch, Gw_enemy_count, Gw_fuel_window, Gb_stage_clear, Gw_stage);
                if (ppdir) for (int i = 0; i < ppn; i++) if (ppf[i] == f) {
                    char pth[300]; snprintf(pth, sizeof pth, "%s/port_f%d.ppm", ppdir, f);
                    ff_screenshot_ppm(pth, (const uint8_t(*)[3])GC.pal); }
                if (ppdir && palo >= 0 && f >= palo && f <= pahi) {
                    char pth[300]; snprintf(pth, sizeof pth, "%s/port_f%d.ppm", ppdir, f);
                    ff_screenshot_ppm(pth, (const uint8_t(*)[3])GC.pal); }
                if (fdlo >= 0 && f >= fdlo && f <= fdhi)
                    for (int k = 0; k < 20; k++) { u8 *s = ENT_BASE + k*0x33;
                        if ((i8)s[0] < 0) continue;
                        printf("E,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", f, k, (i8)s[0],
                            (i16)*(u16*)(s+3), (i16)*(u16*)(s+1), (i16)*(u16*)(s+5),
                            (i16)*(u16*)(s+0x11), s[0x2f], s[0x2e]); }
                f++;
            } else if (play && Gb_stage_clear != 0) {          /* boss killed -> STAGE branch */
                u32 sc = ((u32)Gw_score_hi << 16) | Gw_score_lo;
                int cont = frontend_stage_advance();       /* fn0DAE_03AA (interlude, ETAPE palette) */
                fprintf(stderr, "STAGE CLEAR @tape %d -> stage %u (score carries %u, lives %d)%s\n",
                        f, Gw_stage, sc, (i16)Gw_lives, cont ? "" : " [GAME COMPLETE]");
                if (ilude) { ff_screenshot_ppm(ilude, (const uint8_t(*)[3])GC.pal); ilude = NULL; }
                if (!cont) break;
                run_level_init();                          /* re-enter run_level: gameplay palette + fresh stage */
                g_exhaust_phase = 1;                       /* local_6 stack residue on the start_level path (measured) */
                /* wFFFFFFF2 (décor spawn accum) stack residue on the start_level
                 * path: pixel-swept vs qemu/pix2 — any value in the {0,0x40,..,
                 * 0x400} class is behaviourally identical (the F468 density gate
                 * masks the early phase); 0 = the simplest in-class constant.
                 * (The menu path's measured residue 0x2540 stays in decor_reset.) */
                decor_set_accum(0);
                { const char *e = getenv("FF2_STAGE_ACCUM");
                  if (e) decor_set_accum(atoi(e)); }
            } else break;                                  /* death / demo end */
        }
        if (endshot) ff_screenshot_ppm(endshot, (const uint8_t(*)[3])GC.pal);  /* last gameplay frame */
        return 0;
    }

    /* keyboard-module test: drive the game with SCRIPTED GC.input (no SDL) to prove
     * a human can start and play level 1 through the ported keyboard path
     * (ff_keystate_update -> ff_poll_controls -> input flags). Phase 1 navigates the
     * MENU (pulse FIRE to insert a credit, then press START); phase 2 drives the
     * race (accelerate, then steer left, then steer right) and reports the car
     * state so the response is visible. */
    if (argc >= 2 && strcmp(argv[1], "--kbdtest") == 0) {
        game_init(assets);
        frontend_reset(assets);
        int rlf = 0;
        for (int i = 0; i < 4000 && rlf < 120; i++) {
            memset(GC.input, 0, sizeof GC.input);
            if (rlf == 0) {                       /* MENU: insert a credit, then start */
                if (i >= 90  && i < 94)  GC.input[FF_K_FIRE]  = 1;   /* pulse FIRE  */
                if (i >= 140 && i < 145) GC.input[FF_K_START] = 1;   /* press START */
            } else if (rlf < 40) {                /* RACE: accelerate */
                GC.input[FF_K_UP] = 1;
            } else if (rlf < 75) {                /* steer LEFT while accelerating */
                GC.input[FF_K_UP] = 1; GC.input[FF_K_LEFT] = 1;
            } else {                              /* steer RIGHT while accelerating */
                GC.input[FF_K_UP] = 1; GC.input[FF_K_RIGHT] = 1;
            }
            if (!frontend_frame()) {
                run_level_frame();
                if (rlf == 0)
                    printf("RACE STARTED @boot %d: bDC6F=%d (0=human play) EA3A=%d lives=%d 27C7=%d\n",
                           i, Gb_demo_flag, Gw_credits, Gw_lives, Gw_stage);
                if (rlf == 20 || rlf == 60 || rlf == 100)
                    printf("  rf=%-3d input=%s DD84(carX)=%-4d speed=%-3d F6AF=%d\n", rlf,
                           rlf < 40 ? "UP" : rlf < 75 ? "UP+LEFT" : "UP+RIGHT",
                           (i16)Gw_car_x, Gw_speed, Gw_game_mode);
                rlf++;
            }
        }
        printf("%s\n", rlf > 0 ? "OK: human-driven level 1 ran" : "FAIL: race never started");
        return 0;
    }

    /* front-end flow test (CONTINUE + name entry): scripted GC.input drives the
     * whole death chain headlessly — menu (2 credits + START), race 1 (UP for
     * score, then a forced quick death: lives=0 + fuel=1), NUC cutscene,
     * CONTINUE screen (FIRE -> re-enter the race), race 2 death, cutscene,
     * GAME OVER -> HIGH SCORE with the interactive NAME ENTRY (DOWN/FIRE
     * letters), then verify the table row + the saved HIGH file. */
    if (argc >= 2 && strcmp(argv[1], "--fetest") == 0) {
        game_init(assets);
        frontend_reset(assets);
        /* FE_TAPE env: drive the races from a per-frame mask tape (re-armed at
         * each race start) instead of scripted GC.input — makes the race inputs
         * byte-identical to the QEMU FE-screens capture (cap_fescreens.py), so
         * the CONTINUE / GAMEOVER / name-entry screens carry the SAME score. */
        u8 *fet = NULL; long fetlen = 0;
        { const char *e = getenv("FE_TAPE");
          if (e) { FILE *tf = fopen(e, "rb");
                   if (tf) { fseek(tf, 0, SEEK_END); fetlen = ftell(tf); fseek(tf, 0, SEEK_SET);
                             fet = malloc(fetlen ? (size_t)fetlen : 1);
                             if (fread(fet, 1, (size_t)fetlen, tf) != (size_t)fetlen) fetlen = 0;
                             fclose(tf); demo_input_set_tape(fet, (int)fetlen); } } }
        int rf = 0, race = 0, cont_fire = 0, ne_script = 0, go_frames = 0, prevph = -1;
        for (int i = 0; i < 30000; i++) {
            memset(GC.input, 0, sizeof GC.input);
            int ph = frontend_phase();
            if (ph != prevph) { printf("  @%-6d FE phase=%d\n", i, ph); prevph = ph; }
            if (ph == 7) {                              /* FE_MENU: 2 credits + START */
                if ((i >= 90 && i < 93) || (i >= 110 && i < 113)) GC.input[FF_K_FIRE] = 1;
                if (i >= 150 && i < 154) GC.input[FF_K_START] = 1;
            } else if (ph == 3) {                       /* FE_CONTINUE */
                if (++cont_fire == 40)                  /* settled: the 2-stage transition
                                                         * (10 to-black + 10 reveal) + margin */
                    ff_screenshot_ppm("diag/fe_continue.ppm", (const uint8_t(*)[3])GC.pal);
                if (cont_fire > 45 && race == 1) GC.input[FF_K_FIRE] = 1;     /* continue once */
            } else if (ph == 4) {                       /* FE_GAMEOVER */
                if (++go_frames == 45)                  /* post-wipe settled screen (the
                                                         * cutscene-tail to-black + the
                                                         * gameover 2-stage transition
                                                         * ~30 phase-4 frames) */
                    ff_screenshot_ppm("diag/fe_gameover.ppm", (const uint8_t(*)[3])GC.pal);
            } else if (ph == 5) {                       /* FE_HIGHSCORE: name entry */
                ++ne_script;
                if (ne_script == 28)                    /* post-wipe, pre-first-key settled */
                    ff_screenshot_ppm("diag/fe_nameentry.ppm", (const uint8_t(*)[3])GC.pal);
                if (ne_script == 30 || ne_script == 36) GC.input[FF_K_DOWN] = 1;  /* -> 'C' */
                if (ne_script == 50 || ne_script == 65 || ne_script == 80)
                    GC.input[FF_K_FIRE] = 1;            /* accept C, C, C */
                if (ne_script == 100) GC.input[FF_K_FIRE] = 1;   /* pos==3 -> exit */
            }
            if (!frontend_frame()) {
                if (rf == 0) rf = 1;                    /* race frame 0 ran inside the
                                                         * frontend handover (spiral reveal) */
                if (!fet && rf < 2600) GC.input[FF_K_UP] = 1;   /* build a qualifying score */
                if (rf == 2650) { Gw_lives = 0; Gw_fuel_window = 1; }  /* force quick death */
                if (run_level_frame()) {
                    rf++;
                } else {
                    race++;
                    printf("  @%-6d RACE %d ENDED rf=%d score=%u EA3A=%d\n",
                           i, race, rf, Gw_score_lo, Gw_credits);
                    rf = 0;
                    if (fet) demo_input_set_tape(fet, (int)fetlen);   /* re-arm per race */
                    if (Gb_stage_clear != 0) { frontend_stage_advance(); run_level_init(); g_exhaust_phase = 1; decor_set_accum(0); }
                    else frontend_post_race();
                }
            }
            if (race >= 2 && frontend_phase() == 7) break;   /* back at the menu: done */
        }
        printf("HIGH TABLE after name entry:\n");
        for (int k = 0; k < 6; k++) {
            u32 sc = ((u32)g_high.score[k].hi << 16) | g_high.score[k].lo;
            printf("  %d: name='%.5s' score=%u\n", k, g_high.name[k], sc);
        }
        char hp[320]; snprintf(hp, sizeof hp, "%s/HIGH", assets);
        FILE *hf = fopen(hp, "rb");
        if (hf) { u8 buf[5]; if (fread(buf, 1, 5, hf) == 5)
                  printf("HIGH file row0: '%.5s'\n", (char *)buf); fclose(hf); }
        return 0;
    }

    /* round_transition cutscene test: run the attract race to its end (2041
     * frames), then drive the fn13a8_048a cutscene frame by frame, dumping a
     * PPM per frame + a state line — for pixel/state comparison vs the QEMU
     * capture of the original's cutscene (qemu/cap_cutscene.py). */
    if (argc >= 3 && strcmp(argv[1], "--cutscene") == 0) {
        game_init(assets);
        frontend_reset(assets);
        attract_state();
        int n = 0;
        while (run_level_frame()) { if (++n > 20000) break; }
        printf("race ended after %d frames (DE50=%d DC6E=%d)\n", n, Gb_panel_msg, Gb_stage_clear);
        char p[400];
        snprintf(p, sizeof p, "%s/port_race_end.ppm", argv[2]);
        ff_screenshot_ppm(p, (const uint8_t(*)[3])GC.pal);
        if ((i16)Gw_credits != 0) Gw_credits = (u16)(Gw_credits - 1);   /* game_main @959 */
        int finale = (argc >= 4 && strcmp(argv[3], "finale") == 0);
        if (finale) Gw_stage = 0x04;   /* inject stage-5 death (same override in QEMU) */
        round_transition_start();
        int f = 0;
        int every = finale ? 10 : 1;     /* finale: PPM every 10th + full state lines */
        while (round_transition_frame()) {
            if (f % every == 0) {
                snprintf(p, sizeof p, "%s/port_cs_%03d.ppm", argv[2], f);
                ff_screenshot_ppm(p, (const uint8_t(*)[3])GC.pal);
            }
            printf("C,%d,%d,%d,%d,%u,%u,%u,%u,%u,%d,%u\n", f, Gb_panel_msg, Gb_panel_state, Gw_panel_cursor,
                   Gw_spr_count, Gw_stage, Gw_dist_lo, Gw_speed, Gw_mtn_scroll,
                   (i16)Gw_enemy_count, Gw_charge_flag);
            if (finale)
                for (int k = 0; k < 20; k++) {
                    u8 *s = ENT_BASE + k*0x33;
                    if ((i8)s[0] < 0) continue;
                    printf("E,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", f, k, (i8)s[0],
                        (i16)*(u16*)(s+3), (i16)*(u16*)(s+1), (i16)*(u16*)(s+5),
                        (i16)*(u16*)(s+0x11), s[0x2f], s[0x2e]);
                }
            if (++f > 4000) break;
        }
        printf("cutscene ran %d frames -> %s/port_cs_*.ppm\n", f, argv[2]);
        return 0;
    }

    /* race length: run run_level_frame() until it signals the race is over
     * (local_38→0, the fn1069_0006 `while(wFFFFFFCA!=0)` exit). Prints the frame
     * count — must match the QEMU capture (run_level returns at frame 2041). */
    if (argc >= 2 && strcmp(argv[1], "--racelen") == 0) {
        game_init(assets);
        attract_state();
        int n = 0;
        while (run_level_frame()) { if (++n > 20000) break; }
        printf("race ended after %d frames (DE50=%d DC6E=%d 27C7=%d EA3A=%d)\n",
               n, Gb_panel_msg, Gb_stage_clear, Gw_stage, Gw_credits);
        return 0;
    }

    /* state trace: run N frames, print the same CSV columns the QEMU capture
     * records (qemu/cap_frames.py) so sim vs original can be diffed numerically. */
    if (argc >= 3 && strcmp(argv[1], "--trace") == 0) {
        game_init(assets);
        attract_state();
        int n = atoi(argv[2]);
        printf("frame,EA3C,E9CC,F6C4,DD84,DD86,DE50,DE52,DE53,F461,F6EC,F682,F6AF,27C9,27CB,EF50,F688\n");
        for (int i = 0; i < n; i++) {
            run_level_frame();
            printf("%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", i,
                Gw_dist_lo, Gw_speed, Gd_objective, Gw_car_x, Gw_horizon,
                Gb_panel_msg, Gb_panel_state, Gw_panel_cursor, Gb_rle_count, Gb_input_mask,
                Gw_rle_ptr, Gw_game_mode, Gw_score_lo, Gw_score_hi, Gw_mtn_scroll, Gb_obj_done);
        }
        return 0;
    }

    /* full-state trace: the same 26 columns qemu/cap_long.py records, plus the
     * décor ring and entity pool per frame — diff vs qemu/cap300/. */
    if (argc >= 3 && strcmp(argv[1], "--trace2") == 0) {
        game_init(assets);
        attract_state();
        int n = atoi(argv[2]);
        printf("frame,EA3C,E9CC,F6C4,DD84,DD86,DE50,DE51,DE52,DE53,F461,F6EC,F682,F6AF,27C9,27CB,EF50,F688,24F1,F468,DC7E,EF4A,DE5D,379F,F6D8,F46A,DC6C\n");
        for (int i = 0; i < n; i++) {
            run_level_frame();
            printf("%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", i,
                Gw_dist_lo, Gw_speed, Gd_objective, Gw_car_x, Gw_horizon,
                Gb_panel_msg, Gb_panel_delay, Gb_panel_state, Gw_panel_cursor, Gb_rle_count, Gb_input_mask,
                Gw_rle_ptr, Gw_game_mode, Gw_score_lo, Gw_score_hi, Gw_mtn_scroll, Gb_obj_done,
                Gw_tick_timer, Gw_decor_density, Gw_decor_count, Gw_ring_head, Gw_ring_tail,
                Gw_decor_kind_cur, Gw_decor_latch, Gw_enemy_count, Gw_fuel_window);
            for (int k = 0; k < 10; k++) {
                DecorProto *e = &g_decor_ring[k];
                if (e->kind)
                    printf("R,%d,%d,%d,%d,%d,%#x\n", i, k, e->kind, e->side,
                           (u16)e->phase, e->morph.off);
            }
            for (int k = 0; k < 20; k++) {
                u8 *s = ENT_BASE + k*0x33;
                if (s[0] != 0xFF)
                    printf("E,%d,%d,%d,%d,%d,%d,%d,%d\n", i, k, s[0],
                        (i16)*(u16*)(s+1), (i16)*(u16*)(s+3),
                        (i16)*(u16*)(s+5), s[0x2E], s[0x2F]);
            }
        }
        return 0;
    }

    /* entity-pool dump: run N frames, print active slots (compare vs QEMU). */
    if (argc >= 3 && strcmp(argv[1], "--ents") == 0) {
        game_init(assets);
        attract_state();
        int n = atoi(argv[2]);
        for (int i = 0; i < n; i++) run_level_frame();
        printf("=== entity pool g_pool[20] after %d frames ===\n", n);
        for (int k = 0; k < 20; k++) {
            u8 *s = ENT_BASE + k*0x33;
            if (s[0] == 0xFF) continue;
            /* replicate the fn0A0D_053E morph walk to show the picked sprite */
            i16 di = (i16)*(u16*)(s+3);
            u16 anim = *(u16*)(s+0x1D);
            i16 base1B = (i16)*(u16*)(s+0x1B);
            int picked = -1, pw=0, ph=0;
            if (di >= 0 && di < 0x100 && anim) {
                u16 ax = (u16)(g_persp_scale[di] >> 1);
                MorphEnt *m = morph_at(anim);
                i16 t19 = (i16)((ax * (u16)m[0].thresh) >> 6);
                int j = 0;
                while (j < 13 && t19 < m[j].thresh) ++j;
                i16 sp = m[j].sprite; if (sp < 0) sp = m[j-1].sprite;
                if (base1B) sp = (i16)(sp + base1B);
                picked = sp; ff_dir_dims(picked, &pw, &ph);
            }
            printf(" slot%2d type=%3d scrX=%4d di=%4d scrY=%4d shape=%d proj=%d anim=%#06x base=%d -> spr=%d %dx%d hp=%d\n",
                k, s[0], (i16)*(u16*)(s+1), (i16)*(u16*)(s+3),
                (i16)*(u16*)(s+5), s[0x2E], s[0x2C],
                anim, base1B, picked, pw, ph, s[0x2F]);
        }
        printf("live count wF46A=%d  tEF52=%d\n", Gw_enemy_count, Gw_spr_count);
        printf("HUD: bF462(weapon)=%d bF6D3=%d w38AF(fuel)=%d bF7ED(orbs)=%d wDC68(lives)=%d "
               "wE9C8=%d w38AB(F)=%d w38AD(K)=%d wDC6C=%d wDC78=%d w24FF=%d "
               "glyph[0x38B1+n]=%d/%d/%d term=%d\n",
               (i8)Gb_weapon_anim, Gb_hud_phase, (i16)Gw_fuel_shown, (i8)Gb_orbs_shown, (i16)Gw_lives,
               (i16)Gw_missile_fuel, (i16)Gw_fbar_shown, (i16)Gw_kbar_shown, (i16)Gw_fuel_window, (i16)Gw_kero,
               Gw_blink, ffd_hud_glyph_tables[0+(i8)Gb_weapon_anim],
               ffd_hud_glyph_tables[1+(i8)Gb_weapon_anim],
               ffd_hud_glyph_tables[2+(i8)Gb_weapon_anim],
               ffd_hud_glyph_tables[3+(i8)Gb_weapon_anim]);
        printf("=== sprite dir g_sprdir[298..350] (idx: off/seg  dims) ===\n");
        for (int idx = 298; idx <= 350; idx++) {
            u16 off = g_sprdir[idx].off, seg = g_sprdir[idx].seg;
            if (seg == 0) continue;
            int w=0,h=0; ff_dir_dims(idx, &w, &h);
            printf("  aDEEE[%d] off=%d seg=%d  %dx%d\n", idx, off, seg, w, h);
        }
        printf("=== decor ring aDDEC (spawn F468=%d DC7E=%d EF4A=%d DE5D=%d 379F=%d F6D8=%#x EA3C=%#x) ===\n",
               (i16)Gw_decor_density, (i16)Gw_decor_count, (i16)Gw_ring_head, (i16)Gw_ring_tail,
               (i16)Gw_decor_kind_cur, Gw_decor_latch, Gw_dist_lo);
        for (int k = 0; k < 10; k++) {
            DecorProto *e = &g_decor_ring[k];
            if (e->kind == 0) continue;
            printf(" ring%d kind=%d side=%d phase=%d script=%#x\n",
                   k, e->kind, e->side, e->phase, e->morph.off);
        }
        return 0;
    }

    /* dump the live 16-colour gameplay palette (VGA path) as "i r g b" lines. */
    if (argc >= 2 && strcmp(argv[1], "--palette") == 0) {
        game_init(assets);
        attract_state();
        run_level_frame();
        for (int i = 0; i < 16; i++)
            printf("%d %d %d %d\n", i, GC.pal[i][0], GC.pal[i][1], GC.pal[i][2]);
        return 0;
    }

    /* dump a composed runtime shape: run N frames, blit aDEEE[idx] onto a blank
     * screen -> PPM (to eyeball the composer output vs the reference sprite). */
    if (argc >= 5 && strcmp(argv[1], "--shape") == 0) {
        game_init(assets);
        attract_state();
        int n = atoi(argv[2]), idx = atoi(argv[3]);
        for (int i = 0; i < n; i++) run_level_frame();
        memset(ff_fb, 0, sizeof ff_fb);
        int w=0,h=0; ff_dir_dims(idx, &w, &h);
        ff_dir_blit(idx, 8, 8);
        ff_screenshot_ppm(argv[4], (const uint8_t(*)[3])GC.pal);
        printf("aDEEE[%d] = %dx%d -> %s\n", idx, w, h, argv[4]);
        return 0;
    }

    /* road coord-table dump: run N frames, print a2C51 (16 entries x 4 cols). */
    if (argc >= 3 && strcmp(argv[1], "--road") == 0) {
        game_init(assets);
        attract_state();
        int n = atoi(argv[2]);
        for (int i = 0; i < n; i++) run_level_frame();
        printf("k  leftx rightx  Y   accum   (a2C51 after %d frames; wF1FD=%u DD84=%d tF6CD=%#x wF480=%d curve=%d)\n",
               n, Gw_road_center, (i16)Gw_car_x, track_pos(), (i16)Gw_track_seg, track_curve_now());
        for (int k = 0; k < 16; k++)
            printf("%2d %5d %6d %4d %6d\n", k,
                g_road.left[k], g_road.right[k],
                g_road.y[k], g_road.center[k]);
        return 0;
    }

    /* headless: load a front-end CPT screen with the VGA palette -> PPM (title,
     * presentation, etc). No game_init needed — just decode + palette + dump. */
    if (argc >= 4 && strcmp(argv[1], "--screen") == 0) {
        char cp[320];
        snprintf(cp, sizeof cp, "%s/cpt/%s", assets, argv[2]);
        if (ff_load_ilbm_cpt_vga(cp, ff_fb, GC.pal) != 0) {
            fprintf(stderr, "load %s failed\n", cp);
            return 1;
        }
        ff_screenshot_ppm(argv[3], (const uint8_t(*)[3])GC.pal);
        printf("screen %s -> %s\n", argv[2], argv[3]);
        return 0;
    }

    /* headless: render the TITLE screen (fn0DAE_0001 + the menu's INSERT COINS
     * draw) -> PPM. Uses the shared frontend title draw (game/frontend.c), which
     * needs the sprite directory for the © glyph (aDEEE[0x38] = BOB 56). */
    if (argc >= 3 && strcmp(argv[1], "--title") == 0) {
        char cp[320];
        ff_apply_data_tables();            /* strings/tables from source data */
        snprintf(cp, sizeof cp, "%s/font_ega.bin", assets);
        ff_font_load(cp);
        snprintf(cp, sizeof cp, "%s/cpt/BOB.CPT", assets);
        GC.bob = ff_bob_load(cp);
        ff_display_list_init();                 /* fn0A0D_09E4: aDEEE directory */
        frontend_reset(assets);
        frontend_title_screen();                /* fn0DAE_0001 */
        ff_font_draw((char *)ffd_txt_insertcoins, 0x0C, 0x70, 0xB0, 3); /* INSERT COINS (menu draw) */
        ff_screenshot_ppm(argv[2], (const uint8_t(*)[3])GC.pal);
        printf("title -> %s\n", argv[2]);
        return 0;
    }

    /* headless: run the full BOOT sequence (frontend + race) for N frames, then
     * screenshot — for bitwise comparison of the boot screens vs the QEMU boot. */
    if (argc >= 4 && strcmp(argv[1], "--boot") == 0) {
        game_init(assets);
        frontend_reset(assets);
        int n = atoi(argv[2]);
        for (int i = 0; i < n; i++)
            if (!frontend_frame()) { if (!run_level_frame()) frontend_post_race(); }
        ff_screenshot_ppm(argv[3], (const uint8_t(*)[3])GC.pal);
        printf("boot frame %d -> %s\n", n, argv[3]);
        return 0;
    }

    /* headless: run N frames then screenshot (no window) */
    if (argc >= 4 && strcmp(argv[1], "--shot") == 0) {
        game_init(assets);
        attract_state();
        int n = atoi(argv[2]);
        for (int i = 0; i < n; i++) run_level_frame();
        ff_screenshot_ppm(argv[3], (const uint8_t(*)[3])GC.pal);
        printf("ran %d frames -> %s\n", n, argv[3]);
        return 0;
    }

    if (ff_init("Fire & Forget II — recomp (level)", 3) != 0) return 1;
    game_init(assets);
    frontend_reset(assets);            /* cold boot: TITUS -> PRES -> menu -> race */
    ff_set_palette((const uint8_t(*)[3])GC.pal);
#ifdef __EMSCRIPTEN__
    /* 18 fps ~= the faithful 18.2 Hz frame rate (one frame per bBCBB tick). */
    emscripten_set_main_loop(frame, 18, 1);
#else
    while (!g_quit) { frame(); ff_wait_frame(); }
    ff_shutdown();
#endif
    return 0;
}
