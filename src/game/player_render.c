/* player_render.c — the faithful RENDER PATH of one race frame:
 * render_frame() (the fn1069_0006 draw-then-flip @978 composition: world,
 * HUD panel, entities, the player vehicle with all its overlays, HUD numbers,
 * typewriter, gauges, banner) + draw_player_vehicle() (the fn1069_0006
 * @1748-1791 pass-2 block: rotor, car/exhaust/dust or flash silhouette or the
 * fn1069_13c0 crash particle burst, and the tF085 cockpit-flash overlay).
 * (The shell-era player_update() approximation was dead code — the faithful
 * input/physics chain is keyboard.c + camera_faithful.c + run_level.c — and
 * was removed.) */
#include "ff_game.h"
#include "gmem.h"
#include "../render/ff_font.h"
#include <string.h>

void draw_player_vehicle(void);   /* faithful run_level vehicle draw (below) */

/* =========================================================================== */
/* STRONG: render path  (bg -> sorted entities -> player)                      */
/* =========================================================================== */
void render_frame(void)
{
    /* (1) background — faithful sky + perspective road + scenery (render_world) -- */
    render_world();

    /* (1b) HUD panel — run_level blits BOB frame 25 (320x48) via
     * fn1187_2ABC(ds, ds->ptrDF52, 0x30, 0x140, 0,0); ptrDF52 = aDEEE[25],
     * the registered BOB frame 25 (only 320x48 frame), at the top (0,0). */
    if (GC.bob) ff_bob_blit(GC.bob, 25, 0, 0);

    /* (2) entities (PASS 1) — faithful sprite pipeline: project each pool slot
     * (fn0869_1726), pick the morph sprite (fn0A0D_053E), enqueue depth-sorted,
     * and blit far->near (game/sprite_dl.c). Drawn behind the player car. */
    render_entities();

    if (GC.bob) {
        /* (3) player vehicle — faithful port of the run_level draw (fn1069_0006):
         *   sx       = bb94 (lateral, @0xDD84) + 0xA0            (centred screen X)
         *   local_32 = sx >> 5, clamped [0,9]                    (banking column)
         *   local_30 = 4 (normal driving state)                 (sprite row)
         *   id       = a3687[local_30*0x20 + local_32*2]         (steering table)
         *   draw via display list (fn120d_07cf + fn1187_1c73):
         *     screen X = sx - (W>>1)
         *     screen Y = ((H>>1) + bb96) - H + PF_Y   (bb96 @0xDD86, +48 playfield) */
        draw_player_vehicle();
    }

    /* (5) green objective-panel typewriter (fn13a8_00a1): redraw the revealed text
     * + distance counter over the HUD bg. Typing STATE advances per game iteration
     * (run_level_frame); this only repaints what has been revealed so far. */
    panel_text_draw();

    /* (6) HUD cockpit gauges (fn0BA8_1B27): composite the persistent fuel/aim/
     * weapon gauge overlay (mirrors the original VRAM page) over the panel. */
    hud_cockpit_draw();

    /* (7) STAGE COMPLETED banner (fn0BA8_1B27 @5185): big text over the playfield
     * while anim_step (bDC6E) is set — i.e. after the B-CITERN boss is killed. */
    hud_stage_banner();
}

/* fn1069_13c0 update_particles — the crash PARTICLE BURST (drawn instead of the
 * car while the crash counter bDE6D runs). 8 ring slots @DGROUP 0x35CA (stride
 * 10: {vx, x, vy, y, v0}); sprite = GW(0x2507 + (bDE6D>>1)*2) — the 13-word
 * table 0x2507..0x251F = explosion sprites 46..49 then the generated mips
 * 244..251, so the burst shrinks as the counter runs down. First crash frame
 * (bDE6D == 0x18) inits each slot: x = bb94 (already +0xA0 in the original's
 * draw window = screen-centred), vy = v0, y = bb96 + 10. Per frame: enqueue
 * (x, y, spr) via FUN_120d_07cf (centre-x / bottom-y / +PF_Y / 1C73 clip),
 * then x += vx; y -= vy; vy -= 1 (gravity); y > 0x96 -> vy = -vy, y -= vy
 * (ground bounce). (The b409f sfx call FUN_1c3a_027b(0x13) is omitted.) */
static void crash_particles_draw(void)
{
    int spr = (i16)GW(0x2507 + (g_crash_phase >> 1) * 2);
    if (g_crash_phase == 0x18) {
        for (int i = 0; i < 8; ++i) {
            u16 p = (u16)(0x35CA + i * 10);
            GI(p + 2) = (i16)(GI(0xDD84) + 0xA0);   /* x  = bb94 (+0xA0 window) */
            GI(p + 4) = GI(p + 8);                  /* vy = v0                  */
            GI(p + 6) = (i16)(GI(0xDD86) + 10);     /* y  = bb96 + 10           */
        }
    }
    int w = 0, h = 0;
    ff_dir_dims(spr, &w, &h);
    for (int i = 0; i < 8; ++i) {
        u16 p = (u16)(0x35CA + i * 10);
        if (w > 0 && h > 0) {
            int top = GI(p + 6) - h;
            if (top < 0) top += 1;                  /* 1187:1CFA top-clip quirk */
            ff_dir_blit(spr, GI(p + 2) - (w >> 1), top + 48);
        }
        GI(p + 2) = (i16)(GI(p + 2) + GI(p + 0));   /* x += vx  */
        GI(p + 6) = (i16)(GI(p + 6) - GI(p + 4));   /* y -= vy  */
        GI(p + 4) = (i16)(GI(p + 4) - 1);           /* vy -= 1  */
        if (GI(p + 6) > 0x96) {                     /* ground bounce */
            GI(p + 4) = (i16)(-GI(p + 4));
            GI(p + 6) = (i16)(GI(p + 6) - GI(p + 4));
        }
    }
}

/* Faithful player-vehicle draw (extracted so run_level state drives it). */
void draw_player_vehicle(void)
{
    if (!GC.bob) return;
    int bb94 = GI(0xDD84);
    int bb96 = GI(0xDD86);
    int sx = bb94 + 0xA0;
    /* banking column wFFFFFFD0 = (DD84+0xA0)>>5 - road-lean - steering-lean, computed
     * faithfully each frame by car_column_step() (fn1069_0006 @619-653). */
    int local_32 = g_car_col;
    int local_30 = g_car_row;                           /* switch#2: 4 ground, 0..3 flight */
    int id = GI(0x3687 + local_30 * 0x20 + local_32 * 2);
    int w = 0, h = 0;
    ff_bob_dims(GC.bob, id, &w, &h);
    if (w <= 0 || h <= 0) return;
    int iVar9 = (h >> 1) + bb96;                        /* main-car enqueue Y (fn0A0D_07CF) */

    /* flight rotor/shadow sprite local_34 (0x15/0x16/0x17) at (bb94, 0x9b) — drawn
     * BEFORE the car (fn1069_0006 @1748-1750), so it sits behind it. */
    if (g_rotor != 0) {
        int rw = 0, rh = 0;
        ff_bob_dims(GC.bob, g_rotor, &rw, &rh);
        if (rw > 0 && rh > 0) ff_bob_blit(GC.bob, g_rotor, sx - (rw >> 1), 0x9B - rh + 48);
    }

    int scrX = sx - (w >> 1);
    /* fn1187_1C73 top-clip quirk (disasm 1187:1CFA `inc ax`): a sprite crossing
     * the playfield top draws one row lower (bottom = y INCLUSIVE). The car can
     * cross only at the flight ceiling (iVar9 = h/2 + bb96, bb96 min 8). */
    int cartop = iVar9 - h;
    if (cartop < 0) cartop += 1;
    int scrY = cartop + 48;                             /* +48 = PF_Y playfield base */
    ff_blit_clip_top = 48;                              /* playfield window (1C73)   */
    ff_blit_quirk6   = 1;                               /* shift-6 leading-byte ROM bug */

    /* fn1069_0006 @1751/@1783: while the crash counter bDE6D runs, the car is
     * REPLACED by the fn1069_13c0 particle burst; and in game_mode 4 (crash /
     * post-burst throttle drain until respawn) neither the car nor the dust
     * draws — the `if (bc7d==0) { if (d4bf!=4) {...} } else {particles}` gates. */
    if (g_crash_phase != 0) {
        crash_particles_draw();                         /* FUN_1069_13c0 */
    } else if (GW(0xF6AF) != 0x04) {
        /* fn1069_0006 @1756-1776: the (wF6A9==0 || w24FF==0) gate chooses the car draw.
         *   PASS (normal): FUN_120d_07cf(bb94, iVar9, id) — the multicolour car sprite,
         *     followed by the twin exhaust / wheel-dust puffs (a373B offsets).
         *   FAIL (damage FLASH — wF6A9!=0 && w24FF!=0): FUN_120d_08d7(bb94, iVar9, id, d013)
         *     — the SAME sprite drawn through the low-level blit's flash path, which sets
         *     EGA Map Mask = d013-1 so every opaque pixel becomes ONE colour index = a
         *     solid red silhouette (d013=0xd hit -> 12, =10 respawn -> 9). NO exhaust in
         *     the flash branch (the else has only the 08d7 call). */
        if (g_flash_f6a9 == 0 || GW(0x24FF) == 0) {   /* d4b9 read at the @1756 draw
                                                        * point (pre 20-frame dec) */
            ff_bob_blit(GC.bob, id, scrX, scrY);        /* FUN_120d_07cf: normal car */

            /* twin exhaust / wheel-dust puffs — fn1069_0006 @758-772. a373B[frame*4]=
             * {dxL,dyL,dxR,dyR}: id<7 -> two BOB 0x41 (Y-wobbled by a378F[phase]);
             * id>=7 && w24FF -> BOB 0x43/0x42 wheel-DUST. Centre-x/bottom-y +PF_Y. */
            const i8 *t = (const i8 *)GPTR(0x373B + id * 4);
            if (id < 7) {
                int ey = iVar9 + (i8)GB(0x378F + g_exhaust_phase);
                ff_bob_blit(GC.bob, 0x41, sx + t[0] - 8, ey + t[1] - 1 + 48);   /* left  */
                ff_bob_blit(GC.bob, 0x41, sx + t[2] - 8, ey + t[3] - 1 + 48);   /* right */
            } else if (GW(0x24FF) != 0) {
                int wl = 0, hl = 0, wr = 0, hr = 0;
                ff_bob_dims(GC.bob, 0x43, &wl, &hl);
                ff_bob_dims(GC.bob, 0x42, &wr, &hr);
                ff_bob_blit(GC.bob, 0x43, sx + t[0] - (wl >> 1), iVar9 + t[1] - hl + 48); /* left dust  */
                ff_bob_blit(GC.bob, 0x42, sx + t[2] - (wr >> 1), iVar9 + t[3] - hr + 48); /* right dust */
            }
        } else {
            int col = (i16)GW(0xF203) - 1;              /* d013 - 1 (Map Mask colour) */
            if (col > 0) ff_bob_blit_recolor(GC.bob, id, scrX, scrY, col);   /* FUN_120d_08d7 flash */
        }

        /* wheel-dust cloud local_24 (fn1069_0006 @1777-1779): sprite g_dust (0x3a/0x3b/
         * 0x3c, set by the throttle/sine/horizon/corner code) drawn at (bb94, 0x98) —
         * INSIDE the bc7d==0/d4bf!=4 gates like the car. */
        if (g_dust != 0) {
            int dw = 0, dh = 0;
            ff_bob_dims(GC.bob, g_dust, &dw, &dh);
            if (dw > 0 && dh > 0) ff_bob_blit(GC.bob, g_dust, sx - (dw >> 1), 0x98 - dh + 48);
        }
    }

    /* fn1069_0006 @1789-1791: the cockpit FLASH overlay tF085 (ce95) — armed by a
     * passing explosion (ent_explosion_*), the kamikaze strike or an egg hit at
     * (wDE67, tDE6B); enqueued once via FUN_120d_07cf and CLEARED. (The port set
     * the field in behaviors.c but never drew/cleared it — a real pixel gap.) */
    if (GW(0xF085) != 0) {
        int fw = 0, fh = 0;
        ff_dir_dims((int)GW(0xF085), &fw, &fh);
        if (fw > 0 && fh > 0) {
            int ftop = (i16)GW(0xDE6B) - fh;
            if (ftop < 0) ftop += 1;                    /* 1187:1CFA top-clip quirk */
            ff_dir_blit((int)GW(0xF085), (i16)GW(0xDE67) - (fw >> 1), ftop + 48);
        }
        GW(0xF085) = 0;
    }
    ff_blit_clip_top = 0;                               /* end of the playfield draws */
    ff_blit_quirk6   = 0;
}
