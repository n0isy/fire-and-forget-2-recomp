/* run_level.c — FAITHFUL port of the keystone race-frame routine run_level
 * (fn0869_0006 @ 0869:0006; cross-checked FUN_1069_0006 in raw/ghidra/decompiled.c).
 *
 * The original is one function: an init prologue that zeroes level state and
 * BUILDS the derived runtime tables, followed by a per-frame loop whose body
 * does input -> throttle/speed -> camera/scroll -> road build+draw -> entity
 * update -> sprite/HUD draw, gated each iteration by a VBL wait.
 *
 * This port splits it as the platform requires:
 *   run_level_init()  — the prologue (lines 0869:0006..0119): zero state, build
 *                       every runtime LUT/table into G, set the playfield.
 *   run_level_frame() — ONE iteration of the loop body, MINUS the VBL spin
 *                       (`while (ds->t24F1 == 3) ;`), driving the already-ported
 *                       subsystems in the original order.
 *
 * SOURCE OF TRUTH (transliterated, every constant/offset/index kept):
 *   run_level         raw/reko/FF2EGA_0869.c   fn0869_0006
 *   pool-free + LUTs  raw/reko/FF2EGA_0869.c   fn0869_19E7, fn0869_158D, fn0869_1480
 *   .dis ground truth raw/reko/FF2EGA_0869.dis (decisive on the speed-table base)
 *
 * Table builders, found by tracing the init's calls:
 *   fn0869_158D  builds the perspective LUTs E4CC (0xE4CC) and DC84 (0xDC84) from
 *                the base ramp a2CD5 (0x2CD5).
 *   fn0869_1480  builds the SPEED TABLE aF482 (0xF482) in place — Reko mislabels
 *                its base pointer `aFFFFF462 + 32`, but FF2EGA_0869.dis:1365 shows
 *                `ptrLoc08_164 = SEQ(ds, 0xF482)`, i.e. &aF482[0] — then the ramp
 *                LUT wDE6E (0xDE6E) and the shade table aF209 (0xF209)=wDE6E[aF482].
 *   fn0869_19E7  frees the 20-slot entity pool (a2 "aE5CC" loop: 20 slots @0xE5CC
 *                stride 0x33, type byte = 0xFF free, shape byte = 0).
 *
 * a2CB1 (0x2CB1, entity-perspective centre over 16 depth levels, read by the
 * entity projector fn0DAE_0446) has NO writer anywhere in the available
 * decompilation (confirmed: only reads in FF2EGA_0DAE.c / ghidra) and is zero in
 * the extracted DGROUP image — it is runtime/BSS data whose initialiser is
 * outside the decompiled segments.  It is reconstructed here as the genuine
 * straight-road centreline (0xA0) that fn0869_15D6 yields from the centred player
 * x (wF1FD = 0xA0 set in this init), so projected enemies sit ON the road instead
 * of at screen-x 0.  (See run_level_init / build_entity_persp.)
 *
 * INTEGRATION CONTRACT: run_level_frame calls the EXISTING ported subsystems —
 *   camera_update()  (camera_faithful.c)  throttle ramp + aF482 lookup + scroll
 *   render_world()   (render_world.c)     158D LUTs + 15D6 road perspective + draw
 *   vm_step()        (vm.c)               level script — spawns waves
 *   entities_update_all() (entity_faithful.c) project (a2CB1) + per-type AI
 *   render_frame()   (player_render.c)    render path: bg + sorted sprites + player
 * — it does NOT reimplement them. The throttle ramp + speed-table lookup live in
 * camera_update(); run_level owns the throttle via the shared global g_throttle.
 *
 * All DGROUP access via gmem.h G-accessors (GB/GW/GI/GPTR).
 */
#include "ff_game.h"
#include "gnames.h"
#include "data/gamedata.h"   /* ffd_demo_tape / ffd_persp_ramp / ffd_pal_* */
#include <string.h>

/* bFFFFFFFE — run_level stack-local "READY-FOR-TAKE-OFF armed once" latch (the
 * msg-2 trigger fires on the rising edge of di_1195>=0xC0). Reset on level entry. */
static int s_takeoff_latch = 1;
/* tFFFFFFC4 — previous t24F1, gates the bDE51 countdown (fn1069_0006 @879). */
static u16 s_prev_24f1;
/* In-race timer-ISR model (derived from the EXE, see decor task notes):
 * one game frame = one bBCBB tick (the frame sync fn1187_0136 waits for bBCBB
 * to change; the PIT ISR advances it), and the ISR decrements t24F1 once every
 * 0x12 (18) bBCBB wraps (ndisasm 1c3a:06D4-06EE / 08C5-08DF). run_level's
 * entry spin `t24F1 = 3; while (t24F1 == 3);` (@119) exits right after a
 * decrement, so the race starts with t24F1 == 2 at tick phase 0. */
static int s_isr18;       /* bBCBB divider phase (0..17)                   */
static int s_c2;          /* wFFFFFFC2 — frames since the last t24F1 change */
static int s_c0;          /* wFFFFFFC0 — the 20-frame cycle counter (@983)  */
static int s_local38;     /* wFFFFFFCA — run_level loop-exit / end-of-level tally (init 2) */
/* player-car banking column state (fn1069_0006 @619-653). */
int g_car_col;            /* wFFFFFFD0 / local_32 — banking column [0..9]   */
int g_car_row;            /* local_30 — car sprite row (switch#2)           */
int g_rotor;              /* local_34 — flight rotor/shadow sprite (switch#2)*/
static int s_lean_acc;    /* wFFFFFFEC — road-lean accumulator   */
static int s_lean_dir;    /* wFFFFFFF0 — lean drift direction    */
static int s_road_lean;   /* wFFFFFFEE (local_14) — si_2063 saved for the gun aim */
/* player weapon state (fn1069_0006 fire block @~520-608) */
static int s_fire_cd;     /* wFFFFFFD6 — gun cooldown            */
static int s_speed_prev;  /* local_36 — last SPEED drawn (@1888; uninit stack local:
                             garbage != speed at frame 0, so frame 0 always draws) */
static int s_muzzle;      /* local_22 — muzzle side, init 0x14, flips per shot */

/* ========================================================================== */
/* Table builders (ported verbatim from the init's calls)                     */
/* ========================================================================== */

/* the runtime LUT blocks (declared in ff_game.h) — built below + render_world */
u16 g_speed_tbl[0x100];   /* aF482 */
u16 g_ramp_tbl[0x3F];     /* wDE6E */
u16 g_shade_tbl[300];     /* aF209 */

/* fn0869_158D (table part) — perspective LUTs from the base ramp a2CD5.
 *   g_persp_halfw[i] (DC84) = (u8)(a2CD5[i] * 100 >> 7)   (half-width LUT)
 *   g_persp_scale[i] (E4CC) = (u8)(a2CD5[i] >> 1)         (scale LUT)
 * (render_world.c rebuilds these per frame too; built here so the LUTs exist
 * immediately after init, exactly as fn0869_158D runs inside the prologue.) */
static void build_persp_luts(void)
{
    for (int si_24 = 0x00; si_24 < 0x0100; ++si_24) {
        g_persp_halfw[si_24] = (u8)((u16)ffd_persp_ramp[si_24] * 100 >> 0x07);
        g_persp_scale[si_24] = (u8)(ffd_persp_ramp[si_24] >> 0x01);
    }
}

/* fn0869_1480 — build the speed table aF482, the ramp LUT wDE6E and the shade
 * table aF209.  ptrLoc08_164 starts at &aF482[0] (0xF482, see .dis:1365). */
static void build_speed_shade_tables(void)
{
    u16 *p = g_speed_tbl;                    /* ptrLoc08_164 = &aF482[0] */

    p[0] = 0x00;                             /* aF482[0] = 0 (idle = STOPPED) */
    int wLoc04_163 = 0x00;
    ++p;                                     /* loop1 writes from index 1 (Reko's
                                              * (alias) IR hid this pre-increment;
                                              * verified vs runtime: aF482[0]=0) */

    int di_27 = 0x01;                        /* first run: aF482[1..7] = 1 */
    while (di_27 < 0x08) { *p = 0x01; ++di_27; ++p; }

    for (int si_43 = 0x01; si_43 < 0x20; ++si_43) {   /* groups of 8 = 1..31 */
        int v17_50 = ++wLoc04_163;
        for (int di_52 = 0x00; di_52 < 0x08; ++di_52) { *p = (u16)v17_50; ++p; }
    }
    p[-1] = 0x20;                            /* ptrLoc08_164->wFFFE = 0x20 */

    /* ramp LUT wDE6E:  ramp[0]=0;  ramp[s] = (s*0x100>>8)+1 = s+1, s=1..0x3E */
    g_ramp_tbl[0] = 0x00;
    for (int si_102 = 0x01; si_102 < 0x3F; ++si_102)
        g_ramp_tbl[si_102] = (u16)((si_102 * 0x0100 >> 0x08) + 1);

    /* shade table aF209[i] = wDE6E[aF482[i]]  for i = 0..0xFF */
    for (int si_104 = 0x00; si_104 < 0x0100; ++si_104)
        g_shade_tbl[si_104] = g_ramp_tbl[g_speed_tbl[si_104]];

    /* tail: aF209[0x100..299] = wDEAE, wDEAE+1, ... */
    u16 di_124 = Gw_wDEAE;
    for (int si_125 = 0x0100; si_125 < 300; ++si_125) {
        g_shade_tbl[si_125] = di_124;
        ++di_124;
    }
}

/* a2CB1 (0x2CB1, entity-perspective interpolation table) has NO writer anywhere
 * in the decompile — it is BSS, i.e. ZERO at runtime. (Confirmed by exhaustive
 * search; the earlier 0xA0 "reconstruction" was a fitting hack to compensate for
 * not porting fn0869_1726.) With a2CB1==0 the entity x-projection in fn0DAE_0446
 * yields w0001 = x_accum, and the real perspective scale + 0xA0 screen centering
 * is applied in fn0869_1726 (object_shadow_project) at render time. So we leave
 * a2CB1 zero and do the centering in the render pass. */

/* fn0869_19E7 (pool-free part) — mark all 20 entity-pool slots free.
 * Reko: aE5CC[0].b0000[si]=0xFF, aE5CC[0].b002E[si]=0  for si=0..0x13.
 * The pool is at 0xE5CC, stride 0x33; +0 = type byte (0xFF = free), +0x2E =
 * shape byte. */
static void free_entity_pool(void)
{
    for (int si_42 = 0x00; si_42 < 0x14; ++si_42) {
        *(ENT_BASE + si_42 * 0x33 + 0x00) = 0xFF;
        *(ENT_BASE + si_42 * 0x33 + 0x2E) = 0x00;
    }
}

/* ========================================================================== */
/* run_level_init — fn0869_0006 prologue (0869:0006 .. 0119)                  */
/* ========================================================================== */
void run_level_init(void)
{
    Gb_leader_sfx = 0x00;                        /* ds->bF6DC */
    Gb_bF6DB = 0x00;                        /* ds->bF6DB */
    Gb_obj_sfx_latch = 0x00;                        /* ds->bF6DA */

    free_entity_pool();                       /* fn0869_19E7 (pool-free part) */

    Gw_decor_density = 0x00;                        /* ds->wF468 */
    Gw_decor_count = 0x00;                        /* ds->wDC7E */
    Gb_ring_head = 0x00;                        /* ds->tEF4A.u0 */
    Gb_ring_tail = 0x00;                        /* ds->tDE5D.u0 */

    /* the décor ring (was aDDEC): kind/side zeroed per entry. */
    for (int si_60 = 0x00; si_60 < 0x0A; ++si_60) {
        g_decor_ring[si_60].kind = 0x00;           /* aDDEC[].w0000[si].w0000 */
        g_decor_ring[si_60].side = 0x00;           /* aDDEC[].w0002[si]       */
    }

    Gw_shot_count = 0x00;                        /* ds->wF6D1 */
    Gw_spr_count = Gw_spr_count_saved;                  /* ds->tEF52 = ds->tDE69 */
    Gw_enemy_count = 0x00;                        /* ds->wF46A (live-enemy count) */
    Gb_obj_done = 0x00;                        /* ds->bF688 (objective done)   */
    Gb_kill_ctr = 0xFF;                        /* ds->tF6B8.u0 */
    Gw_bonus_hi = 0x00;                        /* ds->w27D3 */
    Gb_bonus_lo = 0x00;                        /* ds->t27D1.u0 */
    Gb_cflash_spr = 0x00;                        /* ds->tF085.u0 */
    /* fn0BA8_13FD(ds, 1): arm the level script — handled by vm_step() which
     * (re)initialises its PC on the level transition (vm.c).                 */

    build_speed_shade_tables();               /* fn0869_1480 (1st call): aF482, wDE6E, aF209 */

    Gw_playfield_w = 0x0140;                      /* ds->w24F5  playfield width  (0x140) */
    Gb_playfield_h = 0x98;                        /* ds->t24F3.u0 playfield height (0x98) */
    Gw_w24F7 = 0x00;                        /* ds->w24F7 */
    Gw_car_x = 0x00;                        /* ds->wDD84  (scroll/turn x)  */
    Gw_horizon = 0x87;                        /* ds->wDD86  (horizon/pitch)  */
    Gw_w24E7 = 0x00;                        /* ds->w24E7 */
    Gw_w24E9 = 0x00;                        /* ds->w24E9 */
    Gb_panel_msg = 0x00;                        /* ds->bDE50 */
    Gb_panel_state = 0x01;                        /* ds->bDE52 */
    Gb_panel_delay = 0x00;                        /* ds->bDE51 */
    Gw_game_mode = 0x00;                        /* ds->wF6AF  (game mode = drive) */
    Gw_dist_hi = 0x00;                        /* ds->wEA3E  (scroll accum hi)  */
    Gw_dist_lo = 0x00;                        /* ds->tEA3C.u0 (scroll accum lo) */
    Gw_road_center = 0xA0;                        /* ds->wF1FD  (player/crosshair x) */
    Gw_wE4CA = 0x00;                        /* ds->wE4CA */
    Gw_speed = 0x00;                        /* ds->tE9CC.u0 (scroll speed)  */
    Gw_fuel_window = 0x3F;                        /* ds->wDC6C  (road/invuln timer) */

    g_throttle = 0x00;                        /* di_1195 = 0 (throttle local)  */

    if (Gw_kero < 0x08) Gw_kero = 0x08; /* if (ds->wDC78 < 8) = 8 */
    if (Gw_missile_fuel < 0x04) Gw_missile_fuel = 0x04; /* if (ds->wE9C8 < 4) = 4 */

    Gw_flash_timer = 0x00;                        /* ds->wF6A9 */
    Gb_stage_clear = 0x00;                        /* ds->bDC6E  (anim step)       */
    Gb_player_hit = 0x00;                        /* ds->bF6B1 */
    Gb_crash_ctr = 0x00;                        /* ds->bDE6D */

    /* tDC68 = PLAYER LIVES/vehicles: NOT reset here — the original run_level
     * prologue does NOT touch it. It is set to 4 by the GAME START (menu
     * fn0DAE_0118 @179 / attract_state), decremented per lost vehicle @0869:238,
     * and CARRIES ACROSS STAGES (start_level fn0DAE_03AA does not reset it). So a
     * per-stage reset here would wrongly refill lives on the stage advance. */

    memset(Gp_hud_weapon_vars, 0x00, 6);            /* zero 6 bytes @0xF462 (ghidra) */

    Gb_hud_phase = 0x01;                        /* ds->bF6D3 */

    /* fn0869_158D: perspective LUTs + curve-pointer/scroll-phase reset. */
    Gw_wE9CE = 0x00;                        /* ds->wE9CE */
    Gw_track_seg = 0x00;                        /* ds->wF480 */
    build_persp_luts();                       /* E4CC / DC84 from a2CD5 */

    /* fn1187_01C8 / the w24F9==4 title-screen branch / fn1187_2A6F / 2ABC /
     * fn0BA8_203A are HUD/score/title-sprite setup not driven by the five ported
     * subsystems; intentionally omitted from this slice.                      */

    Gb_b40A7 = 0x00;                        /* ds->b40A7 */
    Gb_exit_flag = 0x00;                        /* ds->b40A2 */
    Gb_carrier_flag = 0x01;                        /* ds->bDC6A */
    Gb_pickup_banner = 0x00;                        /* ds->bEF56 */
    Gb_rle_count = 0x00;                        /* ds->bF461 */

    Gw_throttle_step = 0x08;                         /* ds->w2845 (throttle increment) */

    build_speed_shade_tables();               /* fn0869_1480 (2nd call, idempotent) */

    Gw_w24E9 = 0x00;                        /* ds->w24E9 */
    Gw_w24E7 = 0x00;                        /* ds->w24E7 */

    /* ds->t24F1 = 3; while (ds->t24F1 == 3) ;  — the VBL spin: the platform now
     * drives one frame per run_level_frame() call, so it is dropped here.      */

    /* a2CB1 (entity-perspective table) is left ZERO — it has no writer in the
     * engine; the perspective scale + centering is applied in fn0869_1726. */

    /* NOTE: the mountain-scroll accumulator wEF50 (cd60) and the cloud band
     * accumulators c850..c858 (0xEA40..) are init ONCE at cold boot (fn13a8_01c9,
     * see game_init) — NOT here. The run_level prologue must NOT reset them: they
     * PERSIST across demo cycles (the mountain/clouds keep scrolling from where the
     * previous race left off). Resetting them per-race desynced cycle 2 from the
     * original (verified vs qemu/caprace2: race 2 starts with EF50 = the race-1-end
     * value, not 0xA0). */

    /* Demo input replay state (fn1069_0006 init lines 1133-1135): */
    Gb_rle_count = 0x00;                          /* d271 run-length counter = 0 */
    Gw_rle_ptr = 0x2849;                        /* d492 stream ptr -> 0x2849   */
    Gw_track_seg = 0x01;                          /* wF480: fn0869_158D ++ -> 1  */
    track_reset();                              /* fn0869_158D: tF6CD = slot 0 */

    /* HUD score state: the original run_level prologue (fn0869_0006 @34-35 /
     * ghidra @10-11) zeroes ONLY the BONUS (27d1:27d3). The SCORE (27c9:27cb) is
     * NOT reset here — it is zeroed at GAME START (menu fn0DAE_0118 @180-181 /
     * attract_state) and CARRIES ACROSS STAGES (start_level does not touch it), so
     * stage 2 continues the stage-1 score. HIGH (27cd:27cf) also persists (loaded
     * from the HIGH file by ff_load_high at boot). */
    Gw_bonus_lo = 0x00; Gw_bonus_hi = 0x00;       /* bonus = 0 (per-stage) */

    /* Objective panel (fn1069_0006 line ~1071): arm message 0 "BEFORE VISUAL
     * CONTACT" (bc60=0, bc62=1). The distance counter d4d4 (@0xF6C4) is set by the
     * level-script op5 SETCNT=10000; shown as (d4d4>>4) at the panel top. */
    Gd_objective = 10000;                          /* d4d4 = SETCNT 0x2710 */
    s_takeoff_latch = 1;                          /* bFFFFFFFE re-armed on level entry */
    /* `t24F1 = 3; while (t24F1 == 3);` (@119) — the entry spin ends right after
     * an ISR decrement: t24F1 == 2. Three ENVIRONMENT-DERIVED initials, all
     * measured deterministically on the QEMU reference (system_reset + per-frame
     * gdb reads, qemu/cap300/state.csv):
     *  - tFFFFFFC4 (prev-t24F1) is an UNINITIALIZED stack local, != 2 at entry,
     *    so the @879 per-tick gate FIRES ON THE FIRST FRAME with wFFFFFFC2 == 0:
     *    wF468 = (0-12)>>1 = -6 (reference shows 65530 from its frame 1).
     *  - the tick phase: decs land so the gate then sees c2=8 (wF468 = -2) and
     *    c2=18 (wF468 = 3, spawns open) — first dec at the end of our frame 7
     *    -> s_isr18 = 10. Reference: F468 = 0 / -6 / -2 / 3 at frames 0/1/9/27
     *    (ours shifted -1 by the capture alignment sim[i] == orig[i+1]).
     *  - the décor spawn accumulator wFFFFFFF2 is also uninitialized: measured
     *    0x2540 at entry (menu stack residue) — see decor_reset(). */
    Gi_tick_timer = 0x02;
    s_isr18 = 9;                                  /* decs at the ends of our frames 8/26/44 */
    s_c2 = 0;
    s_c0 = 0;
    s_local38 = 2;                                /* wFFFFFFCA = 2 (@1141) */
    s_prev_24f1 = 0xFFFF;                         /* tFFFFFFC4 uninitialized (!= 2) */
    decor_reset();                                /* wFFFFFFF2 = 0x2540 (measured leftover) */
    s_lean_acc = 0; s_lean_dir = 0; g_car_col = 5;/* car-lean state (wFFFFFFEC/F0/D0) */
    s_road_lean = 0;                              /* local_14 = 0 (prologue @53)      */
    s_fire_cd = 0;                                /* wFFFFFFD6                        */
    s_speed_prev = -1;                            /* local_36 uninit (!= any speed)   */
    g_exhaust_phase = 0;                          /* local_6 — uninit stack residue at
                                                   * run_level entry; MEASURED 0 on the
                                                   * menu path (all L1 pixel runs), 1 on
                                                   * the start_level stage path (the
                                                   * stage-2 exhaust rows, qemu/pix2) —
                                                   * the stage branch overrides to 1 */
    s_muzzle = 0x14;                              /* local_22 = 0x14 (prologue @48)   */
    panel_text_reset();
    hud_cockpit_reset();                          /* fn0BA8_1B27 init: clear gauge VRAM */

    /* Gameplay palette — faithful port of fn1069_0006 prologue (decompiled.c
     * 1102-1121). The reference machine is VGA (ds->w24F9 == 4), so the VGA branch
     * runs: the 16-colour palette is 6-bit RGB triples at DGROUP 0x363E
     * (= s_PRES_CPT_1de1_3634 + 10), with COLOUR 2 (bytes @0x3644) patched from
     * 0x3671 (level t27C7 < 2) or 0x366E. fn1987_2A6F programs these straight into
     * the VGA DAC (6-bit). The displayed RGB (and GC.pal) is the DAC 6->8 bit
     * expansion (v<<2)|(v>>4), exactly as the VGA DAC outputs. (The EGA branch —
     * ATC table @0x3674 — is NOT used on the VGA reference.) */
    {
        const u8 *pal   = (const u8 *)ffd_pal_race;
        const u8 *patch = (Gi_stage < 2) ? ffd_pal_patch_lo : ffd_pal_patch_hi;
        for (int i = 0; i < 16; i++)
            for (int ch = 0; ch < 3; ch++) {
                u8 v = (i == 2) ? patch[ch] : pal[i * 3 + ch];
                GC.pal[i][ch] = (u8)(v << 2);   /* VGA DAC 6->8 bit (QEMU: v<<2) */
            }
    }

    /* fn0BA8_13FD(ds, 1): arm the wave VM + path VM and run one dispatch (the
     * script's SETDIST 0x2710 prologue), exactly as the original init does. */
    vm_arm();
}

/* ========================================================================== */
/* Demo input replay — faithful port of fn120d_0251 (120d:0251).              */
/* Reads the RLE [count,mask] stream at DGROUP 0x2849 (d492, set in init) and  */
/* drives the control flags the camera/physics read each frame:               */
/*   d271 (0xF461) run-length; d4fc (0xF6EC) current mask; d492 (0xF682) ptr.  */
/*   mask bits -> 0x24B7 fire, 0x24B9 ?, 0x24BF L, 0x24C1 R, 0x24BB accel,     */
/*               0x24BD brake, 0xF6F4 d4c4, 0xF6C2 d4d2.                       */
/* ========================================================================== */
/* External per-frame input TAPE (1 byte = the same control mask the RLE yields).
 * When set (demo_input_set_tape), demo_input_step takes the frame's mask straight
 * from the tape instead of decompressing the embedded RLE @0x2849 — so the exact
 * recorded level-1 keypresses can drive the port (and, injected identically, the
 * original) in either attract or NORMAL play. The tape IS the decoded RLE, so
 * tape-driven == RLE-driven bit-for-bit; its value is decoupling the input from
 * the attract path (normal play, past the demo cutoff, cross-engine). */
static const u8 *s_tape;
static int s_tape_len, s_tape_idx;
void demo_input_set_tape(const u8 *tape, int len) { s_tape = tape; s_tape_len = len; s_tape_idx = 0; }

/* RLE stream byte: d492 keeps the ORIGINAL offset convention (base 0x2849, so
 * the F682 trace column is unchanged) but the bytes come from the named
 * ffd_demo_tape data, not from G. Beyond the 217 recorded pairs the original
 * read zero bytes (nothing follows the region) — reproduced by the guard. */
static u8 rle_byte(u16 off)
{
    u16 rel = (u16)(off - 0x2849);
    return (rel < sizeof(RlePair) * 217) ? ((const u8 *)ffd_demo_tape)[rel] : 0x00;
}

void demo_input_step(void)
{
    if (s_tape) {                          /* external tape drives the input */
        Gb_input_mask = (s_tape_idx < s_tape_len) ? s_tape[s_tape_idx] : 0x00;
        ++s_tape_idx;
    } else if (Gb_rle_count == 0) {          /* d271 == 0 -> read next RLE pair */
        u16 p = Gw_rle_ptr;                /* d492 (offset, base 0x2849)  */
        Gb_rle_count = rle_byte(p);          /* d271 = stream[0] (count)    */
        Gb_input_mask = rle_byte(p + 1);      /* d4fc = stream[1] (mask)     */
        Gw_rle_ptr = (u16)(p + 2);         /* d492 += 2                   */
    } else {
        Gb_rle_count = (u8)(Gb_rle_count - 1);
    }
    u8 m = Gb_input_mask;                      /* d4fc */
    Gw_btn_fire = (u16)(m & 0x01);           /* 202c7 fire        */
    Gw_btn_start = (u16)(m & 0x02);           /* 202c9             */
    Gw_btn_right = (u16)(m & 0x04);           /* 202cf steer left  */
    Gw_btn_left = (u16)(m & 0x08);           /* 202d1 steer right */
    Gw_btn_accel = (u16)(m & 0x10);           /* 202cb accelerate  */
    Gw_btn_brake = (u16)(m & 0x20);           /* 202cd brake       */
    Gw_chord_edge = (u16)(m & 0x40);           /* d4c4              */
    Gw_btn_takeoff = (u16)(m & 0x80);           /* d4d2              */
    /* poll_controls fn0A0D_0002 @129: the MISSILE button comes straight from
     * the demo mask: wF6B4 = bF6EC & 0x40. */
    Gw_btn_missile = (u16)(m & 0x40);
}

/* ========================================================================== */
/* Objective-panel message state machine — faithful port of fn1069_0006.       */
/* ========================================================================== */
/* fn1069_0006 game-mode SWITCH #1 (@1212-1296, jump table @0x3ae indexed by
 * wF6AF): the takeoff/flight/landing/crash state machine. Runs after the input
 * read, BEFORE the throttle/steering/horizon math (game_mode_step is called
 * before camera_update). It drives the throttle g_throttle (iVar8) and the roll
 * g_roll (local_2e) for the flight modes and performs the mode transitions:
 *   0 ground   -> 2 when throttle>=0xC0 && wDC78 && F6C2 (else arm msg 2)
 *   2 takeoff  -> 1 when horizon<0x80 && throttle>0xFF  (ramps throttle +8/frame)
 *   1 flight   -> 3 when F6C2 || wDC78==0 || bDC6E      (arms msg 1 LANDING)
 *   3 landing  -> 0 when horizon==0x87 && roll==0 && throttle<0xC1 (throttle->0xC0)
 *   4 crash    -> 3 (respawn) or msg 11 (game over) once throttle drains
 * The attract demo runs 0->2->1->3->0 twice (frames ~1322 and ~1871). Verified
 * vs the QEMU capext: speed ramp 24->32 and horizon climb match frame-for-frame. */
static void game_mode_step(void)
{
    u16 mode = Gw_game_mode;
    if (mode > 0x04) return;
    switch (mode) {
    case 0x00:                                          /* 0x10a48 — GROUND */
        if (g_throttle >= 0xC0 && Gw_kero != 0) {   /* di_1195>=0xC0 && wDC78 */
            if (Gw_btn_takeoff != 0) {
                Gw_game_mode = 0x02;                      /* -> takeoff (case 2)    */
            } else if (s_takeoff_latch != 0) {
                s_takeoff_latch = 0;
                Gb_panel_msg = 0x02;     /* msg 2 = READY FOR TAKE OFF */
                Gb_panel_delay = 0x03;     /* re-arm delay               */
                Gb_panel_state = 0x01;     /* arm typewriter             */
            }
        } else {
            s_takeoff_latch = 0x01;
        }
        break;
    case 0x01:                                          /* 0x10a86 — FLIGHT */
        if (Gw_btn_takeoff != 0 || Gw_kero == 0 || Gb_stage_clear != 0) {
            Gw_game_mode = 0x03;                           /* -> landing */
            Gb_panel_msg = 0x01; Gb_panel_delay = 0x02; Gb_panel_state = 0x01;   /* msg 1 LANDING */
        }
        break;
    case 0x02:                                          /* 0x10ab3 — TAKEOFF */
        if (g_throttle < 0x100) g_throttle += (i16)Gw_throttle_step;   /* throttle += 8 */
        g_roll -= (i16)Gw_blink;                       /* roll -= blink parity (0/1) */
        if ((i16)Gw_horizon < 0x80 && g_throttle > 0xFF) Gw_game_mode = 0x01;  /* -> flight */
        break;
    case 0x03:                                          /* 0x10ada — LANDING */
        if (g_throttle > 0xC0) {
            g_throttle -= (i16)Gw_throttle_step;
            if (g_throttle < 0xC0) g_throttle = 0xC0;
        }
        if ((i16)Gw_horizon == 0x87 && g_roll == 0 && g_throttle < 0xC1)
            Gw_game_mode = 0x00;                           /* -> ground */
        else
            g_roll += 2;
        break;
    case 0x04:                                          /* 0x10b10 — CRASH */
        if ((i16)Gw_car_x != 0) Gw_car_x = (u16)((i16)Gw_car_x >> 1);
        if (g_throttle != 0) g_throttle -= 2;
        if ((i16)Gw_horizon < 0x87) g_roll += 2;
        if (Gb_crash_ctr == 0x00 && g_throttle < 1) {
            g_throttle = 0;
            if ((i16)Gw_lives < 0) {                   /* out of lives -> game over */
                Gb_panel_msg = 0x0B; Gb_panel_delay = 0x1E; Gb_panel_state = 0x01;   /* msg 11 */
            } else {
                if (Gb_stage_clear == 0x00) Gw_lives -= 1; /* lose one vehicle */
                if ((i16)Gw_lives >= 0) {              /* respawn */
                    Gw_fuel_window = 0x3F;
                    if (Gw_kero < 8) Gw_kero = 8;
                    /* reko 0869:244-245: the respawn arms the DAMAGE-FLASH /
                     * INVULN window — wF6A9 = 8 flash-ticks (decremented once
                     * per 20-frame cycle = ~160 frames) with the respawn flash
                     * colour wF203 = 10. While wF6A9 != 0 a player hit is only
                     * the cockpit flash, NOT a crash (fn1069_0006 @1191), and
                     * the car draws as the d013-1 silhouette on blink parity.
                     * (Was wrongly dropped as "sound/state" — caught by the L3
                     * gas run: the BOMBE TH detonation inside the post-respawn
                     * window crashed the port but only flashed the original.) */
                    Gw_flash_timer = 0x08;
                    Gw_flash_colour = 0x0A;
                    Gw_game_mode = 0x03;                   /* -> landing (fly back in) */
                    Gw_horizon = (u16)(-0x14);           /* horizon = -0x14 */
                    g_roll = 4;
                    Gb_panel_state = 0x01; Gb_panel_msg = 0x00; Gb_panel_delay = 0x05;   /* msg 0 */
                }
            }
        }
        break;
    }
}

/* fn1069_0006 per-frame objective triggers (lines 921-958): when the panel is
 * idle (bDE51==0 && bDE52==0) it switches the message based on game state. Runs
 * after the HUD/score, before the typewriter advance. In the attract demo no
 * spurious switch occurs (bDC6F==0, t24F1&4==0), so msg stays as armed. */
static void panel_objective_triggers(void)
{
    if (Gb_panel_delay == 0x00 && Gb_panel_state == 0x00) {
        if (Gb_demo_flag != 0x00 && (Gw_tick_timer & 0x04) != 0x00) {
            if (Gb_panel_msg != 0x03) { Gb_panel_state = 0x01; Gb_panel_msg = 0x03; }  /* DEMO MODE */
        } else if ((Gw_objective_lo | Gw_objective_hi) != 0x00) {   /* objective distance left */
            if (Gb_panel_msg != 0x00) { Gb_panel_state = 0x01; Gb_panel_msg = 0x00; }  /* BEFORE VISUAL */
        } else {
            if (Gb_obj_sfx_latch == 0x00 && Gw_flash_timer == 0x00 && (i16)Gw_lives >= 0)
                Gb_obj_sfx_latch = 0x01;     /* fn143A_05B7 objective sound — skipped */
            if (Gb_panel_msg != 0x06) { Gb_panel_state = 0x01; Gb_panel_msg = 0x06; }  /* LEADER SPOTTED */
        }
    }
}

/* ========================================================================== */
/* Player weapons — faithful port of the fn1069_0006 fire block (@~520-608).   */
/* Runs after fn0869_15D6 (road build) and before fn0DAE_0446 (entities).      */
/* ========================================================================== */
static void player_fire_step(void)
{
    /* GUN (w24B7): 4-frame cadence (cooldown 3), free slot scan 15..19,
     * template = prototype[0] @0x1490 (type 0 smart bomb). */
    if (Gw_btn_fire == 0x00) {
        s_fire_cd = 0x00;
    } else if (s_fire_cd != 0x00 || Gw_fuel_window == 0x00) {
        --s_fire_cd;
    } else {
        s_fire_cd = 0x03;
        int sl = 0x0F;
        u8 *p = ENT_BASE + 0x0F * 0x33;
        while ((i8)p[0] >= 0 && sl < 0x14) { p += 0x33; ++sl; }
        if (sl < 0x14) {
            memcpy(p, Gp_proto_gun, 0x33);
            *(u16 *)(p + 0x23) = Gw_speed;            /* timer = speed */
            if (Gw_game_mode == 0x00) {
                i16 aim = (i16)((s_road_lean + g_steer_lean) * 2);
                if (aim > 0x06) aim = 0x06;
                else if (aim < -0x06) aim = -0x06;
                aim = (i16)(aim + ((i16)Gw_car_x >> 6));
                if ((i16)Gw_car_x < 0) aim += 1;      /* round toward 0 */
                *(i16 *)(p + 0x25) = aim;
                *(i16 *)(p + 0x05) = (i16)(0x87 - (i16)Gw_horizon);
                *(i16 *)(p + 0x07) = (i16)((i16)Gw_car_x * 2 + g_steer_lean * 8 + s_muzzle);
            } else {
                *(i16 *)(p + 0x25) = 0x00;
                *(i16 *)(p + 0x05) = (i16)(0x95 - (i16)Gw_horizon);
                *(i16 *)(p + 0x07) = (i16)((i16)Gw_car_x * 2 + s_muzzle);
            }
            s_muzzle = -s_muzzle;                       /* twin barrels alternate */
            u32 z = (((u32)Gw_dist_hi << 16) | Gw_dist_lo) + 3;
            *(u16 *)(p + 0x15) = (u16)z;
            *(u16 *)(p + 0x17) = (u16)(z >> 16);
            /* fn143A_027B(0) gun sfx — omitted */
        }
    }

    /* MISSILE (wF6B4 = demo-mask bit 0x40): needs fuel (wE9C8) + the race
     * window (wDC6C); free slot scan 14..1 DOWNWARD; template = prototype[1]
     * @0x14C3 (type 1 homing missile); consumes one fuel unit. */
    if (Gw_btn_missile != 0x00 && Gw_missile_fuel != 0x00 && Gw_fuel_window != 0x00) {
        int sl = 0x0E;
        u8 *p = ENT_BASE + 0x0E * 0x33;
        while ((i8)p[0] >= 0 && sl != 0) { p -= 0x33; --sl; }
        if (sl != 0) {
            Gw_missile_fuel -= 1;
            memcpy(p, Gp_proto_missile, 0x33);
            *(i16 *)(p + 0x07) = (i16)((i16)Gw_car_x * 2);
            *(i16 *)(p + 0x05) = (i16)(0x95 - (i16)Gw_horizon);
            *(i16 *)(p + 0x11) = (i16)Gw_speed;       /* vz = speed */
            *(i16 *)(p + 0x03) = (i16)(3 - (i16)Gw_speed);
            u32 z = (((u32)Gw_dist_hi << 16) | Gw_dist_lo) + 3 - Gw_speed;
            *(u16 *)(p + 0x15) = (u16)z;
            *(u16 *)(p + 0x17) = (u16)(z >> 16);
            /* fn143A_027B(0x14) missile sfx — omitted */
        }
    }
}

/* ========================================================================== */
/* Player-car banking column — faithful port of fn1069_0006 @619-653 (mode 0). */
/* ========================================================================== */
/* fn1069_0006 @619-653: compute wFFFFFFD0 (car banking column). Base is
 * (wDD84+0xA0)>>5 (line 612-613); then -= si_2063 (road-curve lean) + tFFFFFFB0
 * (steering lean). The road-lean accumulator wFFFFFFEC drifts while braking and
 * decays otherwise. Only game_mode 0 (the demo) is ported here. */
static void car_column_step(void)
{
    int col = ((i16)Gw_car_x + 0xA0) >> 5;       /* wDD84 += 0xA0; wFFFFFFD0 = >>5 */
    g_car_row = 4;                                 /* local_30 default (ground row) */
    g_rotor = 0;                                   /* local_34 default (no rotor)   */
    if (Gw_game_mode == 0x00) {                      /* switch#2 case 0x11244 (ground) */
        i16 curve = (i16)track_curve_now();        /* (tF6CD.u1)->b0000 */
        int si_2063 = (((i16)Gw_speed * (i16)(curve >> 3)) >> 4) + s_lean_acc;
        s_road_lean = si_2063;                     /* local_14 (=si_2063) — read by the gun aim */
        col -= si_2063 + g_steer_lean;             /* -= si_2063 + tFFFFFFB0 */
        if (g_decel_e6 != 0) {
            if (s_lean_dir == 0) s_lean_dir = (col > 4) ? -1 : 1;
            s_lean_acc += s_lean_dir;
        } else if (s_lean_acc != 0 && Gw_speed != 0) {
            s_lean_dir = 0;
            if (s_lean_acc == -1) s_lean_acc = 0; else s_lean_acc >>= 1;
        }
        /* @1654-1656: wheel-dust when cornering hard. The `local_24=0x3b-24ff`
         * assignment is UNCONDITIONAL on 24ff (24ff only gates the omitted sfx). */
        if (Gw_speed != 0 && (si_2063 > 3 || si_2063 < -3))
            g_dust = 0x3B - (i16)Gw_blink;
        /* (engine-screech sound at @643-648 omitted) */
        g_car_row = 4;                             /* local_30 = 4 */
        if (col < 0) col = 0; else if (col > 9) col = 9;   /* @1659: clamp (case 0 only) */
    } else if (Gw_game_mode == 0x01) {              /* switch#2 case 0x11303 (FLIGHT) */
        if (g_steer_flag == 1) { if (col < 8) col = 0x0B; }
        else if (g_steer_flag == 2 && col > 1) col = 10;
        i16 bb96 = (i16)Gw_horizon;                /* car sprite row + rotor by altitude */
        if (bb96 < 0x2B)      { g_car_row = 0; g_rotor = 0x17; }
        else if (bb96 < 0x62) { g_car_row = 1; g_rotor = 0x16; }
        else                  { g_car_row = 2; g_rotor = 0x15; }
        if (g_roll > 0 && g_car_row != 0) --g_car_row;   /* bank by roll */
        if (g_roll < 0 && g_car_row != 2) ++g_car_row;
    } else if (Gw_game_mode == 0x02 || Gw_game_mode == 0x03) {
        /* switch#2 case 0x11377 — TAKEOFF *AND* LANDING. PROVEN BY THE JUMP
         * TABLE @1069:0baa (EXE file 0x203A): [0xbb4, 0xc73, 0xce7, 0xce7,
         * 0xd20] — modes 2 AND 3 both target 0x11377 (the winged row-3 car +
         * altitude rotor), while 0x113b0 (`local_34 = 0`, which ghidra's
         * decompile renders as "case 0x113b0") is MODE 4's case (rotor off
         * during the crash). Treating 0x113b0 as the landing case drew the
         * ground car (row 4, no rotor) during the respawn fly-in — caught by
         * the fuel-crash pixel capture (qemu/capcrash). */
        g_car_row = 3;
        i16 bb96 = (i16)Gw_horizon;
        if (bb96 < 0x2B)      g_rotor = 0x17;
        else if (bb96 < 0x62) g_rotor = 0x16;
        else { if (bb96 > 0x7D) { col = 4; g_car_row = 4; } g_rotor = 0x15; }
    } else if (Gw_game_mode == 0x04) {              /* switch#2 case 0x113b0 (CRASH) */
        g_rotor = 0;
    }
    g_car_col = col;
}

/* ========================================================================== */
/* Player-hit processing — faithful port of fn1069_0006 @1189-1211.            */
/* ========================================================================== */
/* When an enemy set bF6B1 (G_PLAYER_HIT, d4c1) this frame (via ent_pass_player /
 * the ballistic-shot hit), process it: if NOT already crashing (wF6AF!=4) and the
 * player has lives (tDC68>=0), then if the damage-flash timer wF6A9==0 the player
 * CRASHES — set bDE6D=0x18 (a 24-frame crash counter the car-draw block below turns
 * into game_mode=4 + wDC6C=0 + a particle burst); a hard hit (bF6B1==2) zeroes the
 * lives. If wF6A9!=0 (already flashing/invuln) it's just the cockpit flash tF085=0x2e.
 * Then clear bF6B1. Sounds (fn1c3a_05b7/0643/027b) omitted. THE PORT WAS MISSING THIS
 * (the attract demo never gets the player hit, so the demo stayed bit-exact) — found
 * by the spawn-injection bitwise harness (a near boss hitting the player).
 * ORDER (proven by ndisasm of FF2EGA.EXE): in run_level (fn1069_0006 @file_off 0x1496)
 * the crash-flag store `mov byte[0xde6d],0x18` is at 0x17de and the entity-update far call
 * `call 15ae:0446` (which SETS bF6B1 via ent_pass_player) is at 0x2010 — 0x17de < 0x2010,
 * so this block runs BEFORE the entity update and processes the bF6B1 entities set on the
 * PREVIOUS frame (a 1-frame delay). Hence player_hit_step() runs BEFORE entities_update_all(). */
static void player_hit_step(void)
{
    if (Gb_player_hit != 0x00) {                              /* bF6B1 (d4c1) set */
        if (Gw_game_mode != 0x04 && (i16)Gw_lives >= 0) {  /* not crash + has lives */
            if (Gw_flash_timer == 0x00) {                      /* wF6A9 flash timer == 0 -> CRASH */
                Gb_crash_ctr = 0x18;                         /* bc7d = 0x18 crash counter */
                s_lean_acc = 0;                            /* wFFFFFFEC = 0 (reko 0869 @161; the car-lean
                                                            * accumulator is RESET by the crash — without it
                                                            * a brake-built lean froze at speed 0 across the
                                                            * respawn: the wrong banked car after the stage-2
                                                            * fuel-out crash, unfiltered test 2 f3347+) */
                s_lean_dir = 0;                            /* wFFFFFFF0 = 0 (reko 0869 @162) */
                if (Gb_player_hit == 0x02 && (i16)Gw_lives >= 0) Gw_lives = 0;  /* hard hit -> lives 0 */
            } else {                                       /* already flashing: cockpit flash */
                Gw_cflash_spr = 0x2E;                         /* ce95 */
                Gw_cflash_x = (u16)((i16)Gw_car_x + 0xA0);  /* bc77 */
                Gw_cflash_y = (u16)((i16)Gw_horizon + 0x18);  /* bc7b */
            }
        }
        Gb_player_hit = 0x00;                                  /* clear the hit flag */
    }
}

/* fn1069_0006 @1751-1788 car-draw crash branch: while the crash counter bDE6D != 0,
 * the player is exploding — force game_mode=4 (crash), wDC6C=0, the particle burst
 * (fn1069_13c0, drawn in draw_player_vehicle) and count bDE6D down. game_mode_step
 * case 4 then respawns or ends the game once the throttle drains.
 * g_crash_phase = the PRE-decrement bDE6D (the original reads it inside the draw
 * @1786 BEFORE the @1787 decrement; the port's present runs after this step). */
int g_crash_phase;
int g_flash_f6a9;
static void player_crash_step(void)
{
    g_crash_phase = 0;
    if (Gb_crash_ctr != 0x00) {
        Gw_game_mode = 0x04;                                 /* d4bf game_mode = 4 (crash) */
        Gw_fuel_window = 0x00;                                 /* ba7c wDC6C = 0 */
        g_crash_phase = Gb_crash_ctr;                        /* for the fn1069_13c0 draw */
        Gb_crash_ctr -= 1;                                   /* bc7d-- */
    }
    /* SNAPSHOT wF6A9 for the car draw: the original's flash gate reads d4b9 at
     * the car draw @1756, BEFORE the 20-frame block decrements it @1796 — the
     * port draws at the flip position (after the block), so on the dec-to-zero
     * frame it must use the PRE-dec value or the flash ends one odd frame early
     * (unfiltered test 2: orig flashed f3481, port stopped at f3479). */
    g_flash_f6a9 = (i16)Gw_flash_timer;
}

/* ========================================================================== */
/* run_level_frame — ONE iteration of the loop body (VBL spin removed)        */
/* ========================================================================== */
/* Original in-loop order (FF2EGA_0869.c): throttle/steering + scroll math
 * (-> camera_update), fn0869_15D6 road perspective (-> render_world), fn0DAE_0446
 * entity update which re-ticks the wave VM fn0BA8_13FD (-> vm_step +
 * entities_update_all), then the sprite/HUD draw (fn10EF_* / fn0A0D_07CF, the
 * render path).  render_world() performs the per-frame perspective build (15D6)
 * before entities project; render_frame() then composites the final image
 * (it re-invokes render_world() for the background, exactly the original's
 * build-then-draw split).                                                     */
/* One iteration of the inner game loop (fn1069_0006 @1148..1962): input, camera/
 * scroll/counter, curve, road build, render, typewriter, score — exactly once, as
 * the code does. The capture/display rate question is handled in the reference
 * capture, NOT by doubling logic here. */
int run_level_frame(void)
{
    /* fn1069_0006 loop head: `while ((ss->*bp).wFFFFFFCA != 0) { ...frame... }`.
     * When the end-of-level tally has drained local_38 to 0, the original returns
     * (the race is over → game_main runs the post-race path). Signal that to the
     * caller here (0 = race ended, no frame run this call; 1 = a frame ran). */
    if (s_local38 == 0) {
        /* fn1069_0006 exit epilogue (@1150-1169): engine sound off (skipped), then
         * wDC6C=0, and — CRUCIALLY, in DEMO/attract mode (bDC6F set, ba7f) — zero the
         * score (27c9/27cb) and CLEAR anim_step DC6E. That forces game_main's
         * `if (bDC6E==0)` DEATH branch → the attract demo loops back to the MENU; it
         * does NOT advance to stage 2 (verified vs QEMU: DC6E=0 at the return @2041). */
        Gw_fuel_window = 0x00;                  /* ba7c invuln_timer = 0 */
        if (Gb_demo_flag != 0x00) {           /* bDC6F demo flag */
            Gw_score_lo = 0x00; Gw_score_hi = 0x00;   /* score = 0 */
            Gb_stage_clear = 0x00;              /* anim_step = 0 → death branch */
        }
        return 0;
    }

    /* fn1069_0006 @128-147 input dispatch: poll_controls ALWAYS runs first
     * (keyboard key-state -> input flags); then in DEMO mode (bDC6F) the RLE
     * replay OVERRIDES the flags while the player is idle, and any real fire/start
     * keypress ABORTS the demo (wFFFFFFCA=0 -> back to the menu). An external tape
     * (deterministic replay) always overrides. With bDC6F==0 the keyboard flags
     * stand — a human (or scripted GC.input) drives the level. */
    ff_poll_controls();          /* fn0A0D_0002: keystate -> control flags (0x24B7..) */
    if (s_tape) {
        demo_input_step();    /* external tape overrides (deterministic replay) */
    } else if (Gb_demo_flag != 0x00) {              /* attract DEMO */
        if (Gw_btn_fire == 0x00 && Gw_btn_start == 0x00)
            demo_input_step();                    /* fn0A0D_0251 RLE replay */
        else {                                    /* keypress aborts the demo */
            Gb_demo_abort = 0x01;                    /* bF6B3 (@145) */
            s_local38 = 0;                        /* wFFFFFFCA = 0 -> end race loop */
        }
    }
    player_hit_step();        /* 1069:0006 @1189: process the PREV frame's bF6B1 hit -> crash
                               * (bDE6D)/flash — BEFORE entities (@1624) set it, per the code order */
    game_mode_step();         /* 1069:0006 @176-265: wF6AF state machine + msg arm  */
    camera_update();          /* throttle ramp + aF482 speed + scroll/curve/counter */
    track_step();             /* 1069:0006 gate: fn0869_154D advance on (c84c&0x30) */
    decor_spawn_step();       /* 1069:0006 @705-709: décor accum + fn0869_1946 spawn */

    render_world();           /* 0869:158D LUTs + 0869:15D6 road perspective build */
    player_fire_step();       /* 1069:0006 @~520-608: gun + missile spawn blocks   */

    /* fn0DAE_0446 (entity update) runs AFTER the road build in the original
     * frame — its perspective projection reads a2CB1 (= column 3 of the road
     * coord table a2C51+0x60, the centre-line accumulator si_122 the build
     * just wrote; the "no writer" claim was wrong — the writer is fn0869_15D6
     * itself). It re-ticks the wave VM fn0BA8_13FD with the gate
     * (wDC6C != 0 && bDC6E == 0), then projects every pool slot. */
    if (Gw_fuel_window != 0 && Gb_stage_clear == 0) vm_step();   /* fn0BA8_13FD wave VM */
    entities_update_all();    /* fn0DAE_0446 @1624: project + AI per pool slot (SETS bF6B1) */

    car_column_step();        /* 1069:0006 @619-653 (after fn0DAE_0446): banking col;
                               * the gun aim reads the PREVIOUS frame's road lean */
    player_crash_step();      /* 1069:0006 @1751-1788 (car-draw, BEFORE the @1794 20-frame block):
                               * bDE6D crash -> game_mode=4 + wDC6C=0 + bDE6D-- */

    /* 1069:0006 @808-848: the 20-frame cycle block (wFFFFFFC0 == 0x14): wF6A9
     * mode-timer countdown; in flight (wF6AF==1) drain wDC78, else drain the
     * fuel window wDC6C — when it reaches 0: weapon_state=1 and arm the panel
     * msg 5 OUT-OF-FUEL (bDE50=5, bDE51=5). (Sound calls omitted.) */
    if (s_c0 == 0x14) {
        s_c0 = 0;
        if (Gw_flash_timer != 0) {
            Gw_flash_timer -= 1;
            /* ghidra @1796-1804: the damage-flash timer expiring on the
             * ENEMY-HIT colour (wF203==0x0D — set by the egg hit / ballistic
             * hit, reachable since the respawn-invuln fix): with the objective
             * counter at 0 and bF6DA (the objective-done latch) still clear,
             * arm bF6DC — read only by the LEADER-SPOTTED branch's sfx queue
             * (FUN_1c3a_0643(4) @1898), so it is state-complete for the sound
             * port. (else-branch FUN_1c3a_05b7(4,0) = sound, omitted.) */
            if (Gw_flash_timer == 0 && Gw_flash_colour == 0x0D) {
                if (Gw_objective_lo == 0 && Gw_objective_hi == 0 && Gb_obj_sfx_latch == 0)
                    Gb_leader_sfx = 0x01;
            }
        }
        if (Gw_game_mode == 0x01) {
            if (Gw_kero != 0) Gw_kero -= 1;
        } else if (Gw_fuel_window == 0) {                  /* @1810: fuel out -> game-over tally */
            if ((i16)Gw_lives < 0 && s_local38 != 0) --s_local38;
        } else {
            Gw_fuel_window -= 1;
            if (Gw_fuel_window == 0) {
                if (Gw_flash_timer != 0) Gw_flash_timer = 0;   /* + sfx, omitted */
                Gb_player_hit = 0x01;
                if (Gb_panel_msg != 0x05) {
                    Gb_panel_state = 0x01;
                    Gb_panel_msg = 0x05;                 /* OUT OF FUEL */
                    Gb_panel_delay = 0x05;
                }
            }
        }
        /* @1830: boss/level-complete tally — anim_step(bDC6E) set (e.g. by the
         * B-CITERN boss death) drives local_38 down each 20-frame tick. */
        if (Gb_stage_clear != 0 && s_local38 != 0) --s_local38;
    }

    /* @1834-1855: END-OF-LEVEL BONUS TALLY (per frame, outside the 20-frame gate).
     * Once local_38 < 2 and anim_step is set, drain the bonus counter (27D1:27D3)
     * into the score (27C9:27CB) by half each frame, holding local_38 at 1 until
     * the bonus is exhausted (then the next @1830 tick takes local_38 to 0 = demo
     * end). In the ground phase local_38 stays 2 so this is inert. (The @1836-1839
     * bDC6E==0 branch — the 0x36/0x37 overlay pair — is DEAD CODE, see the b40A5
     * note below; the FUN_1c3a_05b7(4,1) tally sound is omitted with the rest of
     * the audio.)
     * VERIFIED (zero-input, out-of-fuel): DC6C(fuel) drains to 0 @f1260 -> DE50=5
     * OUT OF FUEL + bF6B1 -> player CRASH (game_mode 4) -> DC68 lives 4->3 -> respawn
     * (game_mode 3->0, DC6C refuelled 0x3F). BIT-EXACT vs the ORIGINAL for 1299
     * frames INCLUDING the DC68 life decrement (qemu/fuelcap, compare_spawn2 0/0). */
    if (s_local38 < 2) {
        /* (the bDC6E==0 branch draws the 0x36/0x37 overlay pair only when b40A5
         * is set — b40A5 has NO writer anywhere in the decompile and is 0 in the
         * blob, so that draw is dead code; nothing to port there.) */
        if (Gb_stage_clear != 0 && (Gw_bonus_lo != 0 || Gw_bonus_hi != 0)) {
            u16 uVar6 = (u16)((Gw_bonus_lo >> 1) | (((Gw_bonus_hi & 1) != 0) << 15));
            if (uVar6 == 0) uVar6 = 1;
            int borrow = (Gw_bonus_lo < uVar6);
            Gw_bonus_lo = (u16)(Gw_bonus_lo - uVar6);
            Gw_bonus_hi = (u16)(Gw_bonus_hi - ((i16)uVar6 >> 15) - borrow);
            int carry = ((u32)Gw_score_lo + uVar6) > 0xFFFF;
            Gw_score_lo = (u16)(Gw_score_lo + uVar6);
            Gw_score_hi = (u16)(Gw_score_hi + ((i16)uVar6 >> 15) + carry);
            s_local38 = 1;
        }
        Gw_flash_timer = 0;      /* fn1069_0006 @1854: flash timer held 0 during the tally */
    }

    /* 1069:0006 @879-897: the once-per-t24F1-tick block, gated on the timer
     * changing (t24F1 decs every 18 game frames, see the ISR model above):
     *   - bDE51 panel-message re-arm countdown;
     *   - the adaptive décor density: wF468 = (framesPerTick - 12) >> 1, clamped
     *     <= 9 (wFFFFFFC2 counts rendered frames between ticks; = 18 on the
     *     reference machine -> wF468 = 3); wF6B6 records the frame count.
     * (The tFFFFFFE8/wFFFFFFEA delay-loop @899-908 is a frame LIMITER for
     * too-fast machines — pure busy-wait, no game state, omitted.) */
    if (Gw_tick_timer != s_prev_24f1) {
        if (Gb_panel_delay != 0x00) Gb_panel_delay -= 1;
        s_prev_24f1 = Gw_tick_timer;
        i16 ax = (i16)((s_c2 - 12) >> 1);       /* (wFFFFFFC2 + ~0x0B) >> 1 */
        Gw_decor_density = (u16)ax;
        if (ax > 0x09) Gw_decor_density = 0x09;
        Gw_frames_per_tick = (u16)s_c2;
        s_c2 = 0;
    }

    /* 1069:0006 @912: fn0BA8_1B27 — the HUD gauge state machine writes THIS frame's
     * fuel/aim/weapon/bar glyphs into the persistent overlay. In the original this
     * draws into VRAM BEFORE the page flip (@978), so it must run before the present. */
    hud_cockpit_step();         /* 0BA8:1B27 @912: numbers (parity-gated) + gauges */

    /* fn1069_0006 @1888-1891: the SPEED digits — drawn (persistent overlay) only
     * when the value CHANGES; local_36 is an uninitialized stack local, so the
     * first frame always draws. Runs right after the 1B27 call, before the flip. */
    if (s_speed_prev != (i16)Gw_speed) {
        s_speed_prev = (i16)Gw_speed;
        hud_number_draw(Gw_speed, 2, 0x1C, 2);
    }

    panel_objective_triggers(); /* 1069:0006 @921-958: idle-panel message switch (@921) */
    panel_text_advance();       /* fn13a8_00a1 @960: typing state advance (1 ch/iter)    */

    render_frame();             /* 1069:0006 @978 flip: PRESENT — composites the frame
                                 * AFTER the HUD gauge (@912) and typewriter (@960) have
                                 * updated VRAM, exactly as the original draws-then-flips.
                                 * (Presenting here instead of before hud_cockpit_step
                                 * fixes the 1-frame HUD/typewriter lag.) The score
                                 * digits it draws read t27C9 BEFORE the @914 add below,
                                 * matching the original's @912 pre-add score display. */

    /* score accumulation (fn1069_0006 @914), gated by tDC68>=0. Deferred to AFTER the
     * flip: the display above read the pre-add value (like the original @912 draw), and
     * the end-of-frame state is identical since the score changes exactly once/frame. */
    if ((i16)Gw_lives >= 0) {
        u32 sc = ((u32)Gw_score_hi << 16) | Gw_score_lo;
        sc += (u32)((Gw_speed + 3) >> 2);
        Gw_score_lo = (u16)sc; Gw_score_hi = (u16)(sc >> 16);
    }

    /* 1069:0006 @982: blink parity toggles once per frame (after the flip). */
    Gw_blink ^= 0x01;

    /* @983-984: ++wFFFFFFC0 (20-frame cycle) and ++wFFFFFFC2 (frames since the
     * last t24F1 tick); then the timer-ISR tick — the dec lands in this frame's
     * page-flip WAIT (after the @879 gate), so the gate observes it NEXT frame
     * (the reference shows 24F1 changing one row before wF468). */
    ++s_c0;
    ++s_c2;
    if (++s_isr18 >= 0x12) {
        s_isr18 = 0;
        Gi_tick_timer -= 1;                        /* dec word [24F1] — signed */
    }
    return 1;                                    /* a race frame ran this call */
}
