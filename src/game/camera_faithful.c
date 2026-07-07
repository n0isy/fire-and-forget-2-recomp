/* camera_faithful.c — FAITHFUL line-by-line port of the camera/scroll/speed
 * update from run_level (fn0869_0006).
 *
 * Provides a STRONG `void camera_update(void)` that overrides the approximated
 * one in vm.c / the weak fallback in game_main.c.
 *
 * SOURCE OF TRUTH (transliterated, every constant/offset kept):
 *   - run_level  fn0869_0006   raw/reko/FF2EGA_0869.c
 *   - IR ground truth          raw/reko/FF2EGA_0869.dis     (decisive on splits)
 *   - cross-check              raw/ghidra/decompiled.c  FUN_1069_0006
 *
 * This file ports ONLY the per-frame scroll math that lives inside run_level's
 * main loop  `while ((ss->*bp).wFFFFFFCA != 0) { ... }`  — i.e. exactly one
 * camera frame per call:
 *
 *   speed  : di_1210 = clamp(di_1195,0,0xFF);  tE9CC = aF482[di_1210]   (0xF482)
 *   accum  : tEA3C(32) += tE9CC                                         (0xEA3C:EA3E)
 *   object : tF6C4(32) -= (u16)(tE9CC - si_1303)                        (0xF6C4:F6C6)
 *   scrollX: wDD84 += clip(tE9CC * steer), clamp +/-0x96; wF1FD = 0xA0-wDD84
 *   sine   : steer += (tE9CC * sine[tEA3C & 0xFF]) >> 6   when |wDD84|>0x70
 *   worldX : wEF50 += tE9CC * (curve>>2), wrap mod 0x2800               (0xEF50)
 *   parallax col accumulators wEA40..wEA48 += that delta * {0x20..0x40}
 *
 * Run-level LOCALS that persist across frames (they are stack variables that
 * survive the loop, NOT DGROUP fields) are kept here as file statics, exactly
 * like the original keeps them in the stack frame across loop iterations:
 *   di_1195  -> throttle / "gear" index into the speed table aF482
 *   local_52 -> steering lean accumulator      (Reko bp-0x50 / tFFFFFFB0)
 *   local_2e -> roll accumulator               (Reko bp-0x2E / D4, special state)
 *
 * INPUT: the original reads the joystick/keys for STEERING from w24BF (left) and
 * w24C1 (right).  Per the port contract those two reads are taken from
 * GC.input[FF_K_LEFT] / GC.input[FF_K_RIGHT]; every other state (throttle
 * w24BB/w24BD, mode, timers, ...) is driven from the real game state in G.
 */
#include "ff_game.h"
#include "gmem.h"

/* ---- run_level stack locals that persist across loop iterations ---------- */
/* di_1195 (throttle / gear index) is now the shared global g_throttle so that
 * run_level owns it (declared in ff_game.h). Defined here; run_level_init and
 * this file's level guard both zero it on level entry. */
int g_throttle;         /* throttle / gear index (speed-table index, di_1195)  */
#define di_1195 g_throttle
/* steering lean tFFFFFFB0 (range -3..3) — EXPOSED so the car-banking column in
 * run_level (wFFFFFFD0 -= si_2063 + tFFFFFFB0) can read it. */
int g_steer_lean;
#define local_52 g_steer_lean
/* wFFFFFFE6 — the throttle "decelerating" flag (1 = braking/anim, 0 = accel/idle),
 * read by the car-lean accumulator in run_level. */
int g_decel_e6;
/* local_6 — exhaust-flame animation phase (0..3), read by draw_player_vehicle. */
u8  g_exhaust_phase;
/* roll accumulator (Reko local_2e / D4) — EXPOSED as g_roll so run_level's
 * game_mode switch#1 (takeoff/land) can drive it and the horizon block below can
 * read/decay it. Zero in ground mode (mode 0), so the ground phase is untouched. */
int g_roll;
#define local_2e g_roll
/* local_24 — the wheel-dust sprite id, set to 0x3a/0x3b/0x3c by the throttle /
 * sine-sway / horizon-clamp code below (and by switch#2 case 0 in run_level), and
 * drawn at (bb94,0x98) by draw_player_vehicle. Reset to 0 at the top of each frame
 * (the original inits it once + resets after the draw). */
int g_dust;
/* cVar13 — steering-direction flag (0 none, 1 left, 2 right) set in the steering
 * code; read by switch#2's flight case (mode 1) to pick the banking column. */
int g_steer_flag;
static int cam_level = -1;  /* level guard: re-zero the locals on level start  */

/* Resolve the road-curve byte addressed by the far pointer tF6CD (0xF6CD:F6CF).
 * In the original this points into the level's road/curve data, which lives in a
 * SEPARATELY-loaded segment.  This port maps only DGROUP (G), so the far
 * pointer's offset is resolved inside the loaded image; the test points tF6CD at
 * an in-DGROUP curve byte.  With no level loaded the read yields 0 == straight
 * road, which is the genuine result of (curve>>2)*speed, not an invented value. */
static i8 cam_road_curve(void)
{
    u16 off = GW(0xF6CD);                 /* tF6CD — offset half of the far ptr */
    return (i8)GB(off);                   /* signed road-curvature byte         */
}

/* ===========================================================================
 * camera_update — one camera frame of run_level (STRONG override).
 * ======================================================================== */
void camera_update(void)
{
    /* run_level zeroes di_1195 (line 0x39) at level entry; the steering/roll
     * accumulators live in a fresh stack frame.  Re-zero on level change. */
    if (cam_level != GC.level) {
        di_1195 = 0;
        local_52 = 0;
        local_2e = 0;
        g_decel_e6 = 0;
        g_exhaust_phase = 0;
        cam_level = GC.level;
    }

    g_dust = 0;                          /* local_24 = 0 at frame start (@1032/1779) */

    /* STEERING input — faithful: w24BF (0x24BF) = steer left, w24C1 (0x24C1) =
     * steer right.  Driven by the demo replay (fn120d_0251) or, in interactive
     * play, mirrored from GC.input into those flags by the input layer. */
    int steer_left  = (GW(0x24BF) != 0);
    int steer_right = (GW(0x24C1) != 0);

    /* THROTTLE input — driven from the real game state (w24BB/w24BD). */
    u16 w24BB = GW(0x24BB);               /* accelerate */
    u16 w24BD = GW(0x24BD);               /* brake      */

    u16 game_mode = GW(0xF6AF);           /* wF6AF */

    /* ---- throttle / steering evolution -----------------------------------
     * Faithful to run_level's `if (wF6AF != 0) {special} else {driving}`.   */
    if (game_mode != 0) {
        /* --- special states (jump / launch / decel / crash) --- */
        if (game_mode == 1) {                          /* roll D4 (local_2e) */
            if (w24BB != 0 && local_2e < 0x40)         ++local_2e;
            else if (w24BD != 0 && local_2e > ~0x3F)   --local_2e;   /* > -0x40 */
            else if (local_2e == ~0x00)                local_2e = 0; /* == -1   */
            else                                        local_2e >>= 1;
        }
        if (game_mode != 4) {                          /* steering lean (@1399-1430) */
            if (steer_left) {
                if (local_52 < 3) ++local_52;
                g_steer_flag = (g_steer_flag != 2);    /* cVar13 = cVar13 != 2 */
            } else if (steer_right) {
                if (local_52 > ~0x02) --local_52;      /* > -3 */
                g_steer_flag = (g_steer_flag == 1) ? 0 : 2;
            } else {
                g_steer_flag = 0;
                if (local_52 > 0)      --local_52;
                else if (local_52 < 0) ++local_52;
            }
        }
    } else {
        /* --- normal driving (wF6AF == 0): throttle --- */
        if (GW(0xDC6C) == 0) {                          /* invuln_timer == 0  */
            if ((i16)GW(0xDC68) < 0) {                  /* aim_target < 0     */
                if (di_1195 != 0) di_1195 -= GW(0x2845);
            } else {
                GB(0xF6B1) = 1;                         /* weapon_state       */
                GW(0xF6A9) = 0;                         /* mode_timer         */
            }
        } else if (GB(0xDC6E) != 0) {                   /* anim_step != 0     */
            g_decel_e6 = 1;                              /* wFFFFFFE6 = 1      */
            di_1195 -= GW(0x2845);
        } else if (w24BB != 0) {                        /* accelerate         */
            g_decel_e6 = 0;                              /* wFFFFFFE6 = 0      */
            if (di_1195 < 0xC0) {
                if (di_1195 < 0x20)                      /* @1326: kick-off dust */
                    g_dust = 0x3B - (i16)GW(0x24FF);     /* 0x3a/0x3b */
                di_1195 += GW(0x2845);
            }
        } else if (w24BD != 0) {                        /* brake              */
            if (di_1195 != 0) { g_decel_e6 = 1; di_1195 -= GW(0x2845); }
            else g_decel_e6 = 0;
        } else {
            g_decel_e6 = 0;                              /* idle / no input    */
        }

        /* --- normal driving: steering (gated by previous-frame speed) --- */
        if (GW(0xE9CC) != 0) {                          /* tE9CC != 0         */
            if ((i16)GW(0xDD84) > 0x70 || (i16)GW(0xDD84) < ~0x6F) { /* |sx|>0x70 */
                /* sine sway: steer += (speed * sine[accum & 0xFF]) >> 6 */
                i16 prod = (i16)((u16)GW(0xE9CC)
                                 * (u16)(i16)SINE(GW(0xEA3C) & 0xFF));
                local_52 += (i16)(prod >> 6);
                if (local_52 > 3)        local_52 = 3;
                else if (local_52 < ~0x02) local_52 = ~0x02;          /* -3 */
                g_dust = 0x3B - (i16)GW(0x24FF);          /* @1350: sharp-sway dust */
                if (di_1195 > 100 || GW(0x24FF) != 0) di_1195 -= GW(0x2845);
            }
            if (steer_left) {                           /* w24BF (@1372) */
                if (local_52 < 3) ++local_52;
                g_steer_flag = 1;
            } else if (steer_right) {                   /* w24C1 (@1365) */
                if (local_52 > ~0x02) --local_52;       /* > -3 */
                g_steer_flag = 2;
            } else {                                    /* idle (@1357) */
                g_steer_flag = 0;
                if (local_52 == ~0x00) local_52 = 0;    /* == -1 */
                else                    local_52 >>= 1;
            }
        }
    }

    /* ---- horizon / pitch (bb96 = DD86) — fn1069_0006 @1432-1450 ----------
     * bb96 += roll (local_2e); then the flight ceiling / bounce / ground clamp.
     * In ground mode local_2e is 0 and bb96 stays 0x87, so this is inert; it only
     * bites in the takeoff/flight/landing modes (2/1/3) where the car climbs. */
    GW(0xDD86) = (u16)((i16)GW(0xDD86) + local_2e);
    if ((i16)GW(0xDD86) < 0x88) {
        if ((i16)GW(0xDD86) < 8 && game_mode == 1) {   /* flight ceiling */
            GW(0xDD86) = 8;
            local_2e = 0;
        }
    } else if (game_mode == 1) {                        /* flight: bounce off floor */
        GW(0xDD86) = (u16)(0x87 - (((i16)GW(0xDD86) - 0x87) >> 2));
        g_dust = 0x3C;                                   /* @1441 */
        local_2e = -3 - local_2e / 3;
        /* FUN_1c3a_027b(0x11) bump sfx — omitted */
    } else {                                            /* ground/takeoff/land clamp */
        local_2e = -(local_2e / 3);
        GW(0xDD86) = 0x87;
        g_dust = 0x3B;                                   /* @1448 (landing thump dust) */
        /* FUN_1c3a_027b(0x0e) sfx — omitted */
    }

    /* ---- speed: clamp throttle to [0,0xFF] IN PLACE (fn1069_0006 @1451-1459:
     * iVar8 is clamped and stored back, so takeoff's over-ramp saturates at 0xFF),
     * then look up aF482 (0xF482). A no-op in ground mode (throttle <= 0xC0). ---- */
    if (di_1195 > 0xFF)     di_1195 = 0xFF;
    else if (di_1195 < 0)   di_1195 = 0;
    GW(0xE9CC) = SPEED(di_1195);            /* tE9CC = aF482[iVar8]           */

    /* ---- scroll_accum (32-bit, EA3C:EA3E) += speed ----------------------- */
    u32 acc = ((u32)GW(0xEA3E) << 16) | GW(0xEA3C);
    acc += GW(0xE9CC);
    GW(0xEA3C) = (u16)acc;
    GW(0xEA3E) = (u16)(acc >> 16);

    /* ---- exhaust-flame animation phase (fn1069_0006 @492-497, local_6) ----
     * Drives the vertical flicker of the twin exhaust puffs (BOB 0x41) the car
     * draw enqueues (a378F[phase] Y offset). iVar8 = the throttle (g_throttle). */
    if (g_throttle < 0x20) g_exhaust_phase = (u8)((g_exhaust_phase + GW(0xE9CC)) & 3);
    else                   g_exhaust_phase ^= 2;

    /* ---- si_1303 (objective subtrahend bias) selected by mode ------------ */
    u16 si_1303;
    if (game_mode == 1)      si_1303 = 0x18;
    else if (game_mode == 4) si_1303 = 0x08;
    else                     si_1303 = 0x12;

    /* ---- objective / remaining distance (32-bit, F6C4:F6C6) -------------- */
    if (GB(0xF688) == 0) {
        u32 obj = ((u32)GW(0xF6C6) << 16) | GW(0xF6C4);
        /* fn1069_0006 ~1486: uVar5 = tE9CC - iVar9 (SIGNED 16b); 32-bit
         * (d4d6:d4d4) -= sign_extend(uVar5). At speed<iVar9 this ADDS, so the
         * objective counter ramps up early (tE9CC small) then counts down. */
        obj -= (u32)(i32)(i16)(GW(0xE9CC) - si_1303);   /* sign-extend the 16b */
        GW(0xF6C4) = (u16)obj;
        GW(0xF6C6) = (u16)(obj >> 16);
        if ((i16)GW(0xF6C6) < 0) {                       /* high word < 0 -> done */
            GW(0xF6C6) = 0;
            GW(0xF6C4) = 0;
            GB(0xF688) = 1;
        }
    } else {
        GW(0xF6C6) = 0;
        GW(0xF6C4) = 0;
    }

    /* fn1069_0006 @1500-1509 — the TIME LIMIT (was NEVER ported; the unfiltered
     * idle-run pixel test caught it at frame 3110): the objective counter RISES
     * while the player is slower than the pace (speed < iVar9: obj -= negative),
     * and once the 32-bit value exceeds 0x10000 (`d4d6 > 0 && (d4d6 > 1 ||
     * d4d4 != 0)`) the original arms panel msg 4 OUT-OF-TIME and — while the
     * fuel window is open — sets bF6B1 = 2 EVERY FRAME: the HARD player hit
     * (player_hit_step zeroes the lives on the crash), i.e. running out of
     * time destroys the vehicle and ends the game (original game over at
     * frame 3161 of the standing-car run vs the port's fuel-only 6441). */
    if ((i16)GW(0xF6C6) > 0 && ((i16)GW(0xF6C6) > 1 || GW(0xF6C4) != 0)) {
        if (GB(0xDE50) != 0x04) {                        /* msg 4 OUT OF TIME */
            GB(0xDE52) = 0x01;
            GB(0xDE50) = 0x04;
            GB(0xDE51) = 0x05;
        }
        if (GW(0xDC6C) != 0)
            GB(0xF6B1) = 0x02;                           /* d4c1 = 2: hard hit */
    }

    /* ---- scroll_x (DD84): steer * speed, clipped; crosshair; clamp ------- */
    i16 local_48 = (i16)((u16)GW(0xE9CC) * (u16)(i16)local_52);  /* tE9CC*steer */
    if (local_48 < 0 || local_48 > 4) local_48 = (i16)(local_48 >> 2);
    else if (local_48 != 0)           local_48 = 1;

    i16 sx = (i16)GW(0xDD84) + local_48;
    GW(0xF1FD) = (u16)(0xA0 - sx);          /* wF1FD crosshair (UNclamped sx) */
    if (sx > 0x96)        sx = 0x96;
    else if (sx < ~0x95) sx = ~0x95;        /* < -0x96 -> -0x96 */
    GW(0xDD84) = (u16)sx;

    /* ---- world_x (EF50) += speed*(curve>>2); wrap mod 0x2800 + parallax -- */
    i16 cv = (i16)((i8)cam_road_curve());
    cv = (i16)(cv >> 2);                                 /* arithmetic >>2     */
    i16 ae = (i16)((u16)GW(0xE9CC) * (u16)cv);           /* ax_1427 low word   */

    i16 wx = (i16)((i16)GW(0xEF50) + ae);
    if (wx < 0)            wx = (i16)(wx + 0x2800);
    else if (wx >= 0x2800) wx = (i16)(wx - 0x2800);
    GW(0xEF50) = (u16)wx;

    /* parallax background column accumulators (same delta, different rates).
     * (run_level then selects a column-graphics pointer via aF689[(EF50>>4)&7];
     *  that select needs the level's far pointer table, so it is omitted here.) */
    GW(0xEA48) = (u16)(GW(0xEA48) + (u16)(ae << 6));     /* *0x40 */
    GW(0xEA46) = (u16)(GW(0xEA46) + (u16)(ae * 0x35));
    GW(0xEA44) = (u16)(GW(0xEA44) + (u16)(ae * 0x33));
    GW(0xEA42) = (u16)(GW(0xEA42) + (u16)(ae * 0x2A));
    GW(0xEA40) = (u16)(GW(0xEA40) + (u16)(ae << 5));     /* *0x20 */

    /* ---- reflect the visible horizontal scroll for the renderer ----------
     * The on-screen turn/roll offset is wDD84; no wave term is added (the
     * original computes none here). */
    GC.scroll_x = (i16)GW(0xDD84);
}
