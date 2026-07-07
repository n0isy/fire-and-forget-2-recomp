/* behaviors.c — Phase 2b: native ports of the most common enemy-behavior
 * handlers from segment 161c (Ghidra raw/ghidra/decompiled.c, FUN_161c_*).
 *
 * These are STRONG definitions that override the WEAK stubs in
 * game/entity_dispatch.c (same names, same signature void(Entity*,int)).
 * Contract (docs/entities.md): handler(Entity *e, int di), di = on-screen
 * distance (slot z_pos - camera, signed).
 *
 * Field/global mapping notes
 * --------------------------
 *  - Slot offsets map onto the named Entity fields where one exists
 *    (screen_x/di/screen_y/x_accum/vx/vz/vy/z_pos/state_phase/timer/param/
 *     hp/hit_flag/targetable/shape_kind/substate). Handler-scratch offsets
 *    (0x1b, 0x27, ...) are reached with EF() — byte-exact, like the original.
 *  - The original DGROUP globals are byte-exact in `G`. Ghidra's DAT_2000_xxxx
 *    sit at DGROUP offset xxxx+0x21F0, DAT_1de1_xxxx at offset xxxx. We use the
 *    confirmed named G fields where they line up (scroll_speed=c7dc,
 *    scroll_x=bb94, scroll_horizon=bb96, aim_target=ba78, invuln_timer=ba7c)
 *    and GW(off) for the few that have no clean name (comments give the DAT).
 *  - Aiming / player collision use the FAITHFUL DGROUP fields G.scroll_x (bb94,
 *    the crosshair X base: 2*scroll_x) and G.scroll_horizon (bb96, crosshair Y =
 *    0x87-scroll_horizon) — NOT the GameCtx shell GC.player_x/y, which drift off
 *    the DGROUP values outside the drawn player sprite (caught by the type-43
 *    BANZAI spawn-test: screen_y 144 vs 0).
 *  - Enemy shots are spawned by ent_fire_shot (13a8:1a1e, copy-the-firer into a
 *    downward-scanned slot 14..0) and handled by enemy_update_ballistic
 *    (161c:22f6) — both fully ported below.
 */
#include "ff_game.h"
#include <string.h>

/* byte-exact access to a slot offset not covered by a named Entity field */
#define EF(e, off, ty)  (*(ty *)((uint8_t *)(e) + (off)))
/* byte-exact access to a DGROUP global by offset (G is the byte-exact image) */
#define GW(off, ty)     (*(ty *)((uint8_t *)&G + (off)))

/* --- DGROUP globals referenced by the original (with their Ghidra DAT name) --- */
/* DAT_2000_d27a — live enemy count (decremented when a slot despawns) */
#define G_ENEMY_COUNT   GW(0xF46A, i16)
/* DAT_2000_d4c1 — "player hit this frame" flag (set on contact) */
#define G_PLAYER_HIT    GW(0xF6B1, u8)
/* DAT_2000_d4e1 — live enemy-shot count (gates new shots) */
#define G_SHOT_COUNT    GW(0xF6D1, i16)
/* DAT_2000_ba7b — parent-aircraft-alive flag (swooper / aircraft) */
#define G_AIRCRAFT_FLAG GW(0xDC6B, u8)
/* DAT_2000_d4c8 — kill counter; wraps every 0xFF kills to award a bonus */
#define G_KILL_CTR      GW(0xF6B8, i16)
/* DAT_1de1_27d1 / 27d3 — 32-bit BCD-ish score (low/high word) */
#define G_SCORE_LO      GW(0x27D1, u16)
#define G_SCORE_HI      GW(0x27D3, u16)

/* ------------------------------------------------------------------ */
/* helpers                                                            */
/* ------------------------------------------------------------------ */

/* port 120d:02d3 — rng_seed_init: SEED the additive-Fibonacci PRNG. Called once
 * by main (FUN_103c_0009 @ startup). Sets the two cursors (r1=0x17, r2=0x36) and
 * copies the 0x37-word seed table from DAT_1de1_37a7 (@0x37A7, in the blob) into
 * the runtime pool DAT_2000_d559 (@0xF749). CRITICAL: 0xF749 is BEYOND the 0xD7F0
 * dgroup image, so without this the pool is BSS-zero and rng16() always returns 0
 * — which made the MIRADOR (type 25, first RNG user @f534) fire a spurious shot
 * that the reference never fires (the f537 entity-pool divergence). */
void ff_rng_seed(void)
{
    GW(0x37A3, i16) = 0x17;                             /* DAT_1de1_37a3 = 0x17 */
    GW(0x37A5, i16) = 0x36;                             /* DAT_1de1_37a5 = 0x36 */
    for (int i = 0; i < 0x37; ++i)                      /* pool[0..0x36] = seed[] */
        GW(0xF749 + i * 2, u16) = GW(0x37A7 + i * 2, u16);
}

/* port 120d:0319 — additive lagged-Fibonacci PRNG over a 0x37-word DGROUP
 * pool (DAT_2000_d559 @ DGROUP 0xF749), driven by two cursors. Returns the
 * fresh signed 16-bit word. (Pool is seeded by rng_seed_init above.) */
static int rng16(void)
{
    i16 *r1  = &GW(0x37A3, i16);                       /* DAT_1de1_37a3 */
    i16 *r2  = &GW(0x37A5, i16);                       /* DAT_1de1_37a5 */
    i16 *tbl = (i16 *)((uint8_t *)&G + 0xF749);        /* DAT_2000_d559[] */
    if (*r1 < 1) *r1 = 0x36; else *r1 -= 1;
    if (*r2 < 1) *r2 = 0x36; else *r2 -= 1;
    tbl[*r2] = (i16)(tbl[*r2] + tbl[*r1]);
    return tbl[*r2];
}

/* port 15ae:056a — narrow AABB test of the enemy against the player (FAITHFUL):
 *   dx = screen_x - wDD84 (scroll_x), |dx| < 0x1E
 *   dy = screen_y - (0x87 - wDD86),   |dy| < 10        */
static int ent_hit_player(Entity *e)
{
    int dx = (i16)e->screen_x - (i16)GW(0xDD84, u16);
    if (!(dx < 0x1e && dx > -0x1e)) return 0;
    int dy = (i16)e->screen_y - (0x87 - (i16)GW(0xDD86, u16));
    return (dy < 10 && dy > -10);
}

/* ------------------------------------------------------------------ */
/* pickups 161c:047a / 04c7 / 0508 — collect-or-despawn when passing   */
/* the player (di < 1): on the narrow hit, bonus 27D1:27D3 += 10 and a */
/* per-type reward; either way despawn + live count down. (sfx 0x16    */
/* omitted — no audio yet.)                                            */
/* ------------------------------------------------------------------ */
static void pickup_pass(Entity *e, int di, int type)
{
    if (di >= 1) return;
    if (ent_hit_player(e)) {
        u16 lo = GW(0x27D1, u16);                    /* bonus += 10 (32-bit) */
        GW(0x27D1, u16) = (u16)(lo + 10);
        GW(0x27D3, u16) += (u16)(lo > 0xFFF5);
        switch (type) {
        case 3:                                      /* 047a BID KERO: K bar */
            GW(0xDC78, i16) += 8;
            if (GW(0xDC78, i16) > 0x1F) GW(0xDC78, i16) = 0x1F;
            break;
        case 2:                                      /* 04c7 BID FUEL: F window */
            GW(0xDC6C, u16) = 0x3F;
            break;
        case 4:                                      /* 0508 MISSILES: engine  */
            GW(0xE9C8, i16) += 4;
            if (GW(0xE9C8, i16) > 0x12) GW(0xE9C8, i16) = 0x12;
            break;
        }
        /* FUN_1c3a_027b(0x16) — pickup sfx, omitted */
    }
    e->type_state = 0xFF;                            /* despawn either way */
    G_ENEMY_COUNT -= 1;                              /* --wF46A            */
}

void pickup_bonus_a(Entity *e, int di) { pickup_pass(e, di, 3); }  /* 161c:047a */
void pickup_shield(Entity *e, int di)  { pickup_pass(e, di, 2); }  /* 161c:04c7 */
void pickup_speed(Entity *e, int di)   { pickup_pass(e, di, 4); }  /* 161c:0508 */

/* port 161c:2b95 — enemy death: morph the slot into a small explosion
 * (type 5), reset its animation/timer, and bump the kill/score bookkeeping.
 * (Sound and the morph far-pointer are dropped in the native port.) */
static void ent_die(Entity *e)
{
    e->type_state = 5;          /* -> ent_explosion_small */
    e->targetable = 0;
    e->shape_kind = 0;
    e->anim_ptr   = 0x0DE1135CUL; /* far ptr to the explosion morph script 1de1:135c */
    e->param      = 0;
    e->timer      = 10;
    EF(e, 0x1b, i16) = 0;        /* explosion uses absolute sprite indices (base 0) */

    if (--G_KILL_CTR == 0) {    /* wave cleared by kills (d4c8, re-armed by each
                                 * PUT/WAVE): BONUS += 10 + arm the big "BONUS"
                                 * banner d4c2 = 12 parity-frames (ghidra @8465-71;
                                 * the banner draw lives in fn0BA8_1B27's head —
                                 * hud_cockpit_step; FUN_1c3a_0643(1) sfx omitted) */
        u16 lo = G_SCORE_LO;
        G_SCORE_LO = lo + 10;
        if (lo > 0xfff5) G_SCORE_HI += 1;
        GW(0xF6B2, u8) = 0x0C;  /* d4c2 = 0xc — the BONUS banner countdown */
        G_KILL_CTR = 0xff;
    }
    /* accumulate this enemy's point value (scratch @0x29) into the bonus total */
    {
        u16 v = EF(e, 0x29, u16);
        u16 lo = GW(0x27C9, u16);
        GW(0x27C9, u16) = lo + v;
        GW(0x27CB, u16) += (u16)(((i16)v >> 15) + (lo + v < lo));
    }
}

/* pool slot base pointer (raw byte access, offsets as in the ROM) */
#define POOL ((u8 *)&G + 0xE5CC)

/* port 13a8:1a1e — fire an enemy shot (type 58 = enemy_update_ballistic).
 * FAITHFUL: concurrency cap (d4e1+d27a < 5); scan slots 14..0 DOWNWARD for a
 * free one (&DAT_2000_c6a6 = slot 14, stepping -0x33); COPY THE FIRER'S 0x33-byte
 * record into the slot, then override the shot fields (so the shot inherits the
 * firer's position/depth AND its scratch @0x27, which gates the re-aim). z_pos--
 * (32-bit); timer = firer.vz - 10; then vz/vy/vx zeroed. The old forward-scan +
 * proto copy put the shot in slot 1 with a wrong +0x27 and vz=0 (no steer). */
static void ent_fire_shot(Entity *e)
{
    if (G_SHOT_COUNT + G_ENEMY_COUNT >= 5) return;     /* (d4e1+d27a) < 5 */
    int sl = 0x0E;                                      /* scan slots 14..0 down */
    u8 *s = POOL + 0x0E * 0x33;
    while ((i8)s[0] >= 0 && sl != 0) { s -= 0x33; --sl; }
    if ((i8)s[0] >= 0 && sl == 0) return;              /* (-1 < iVar2 guard) no free slot */
    G_SHOT_COUNT += 1;                                 /* d4e1++ */
    memcpy(s, e, 0x33);                                /* copy the firer record */
    s[0]    = 0x3A;                                    /* type = ':' (58) */
    s[0x2E] = 0x00;                                    /* shape_kind = 0 */
    *(u16 *)(s + 0x1B) = 0x0000;                       /* w001B base = 0 */
    *(u16 *)(s + 0x1F) = 0x0DE1;                       /* anim_ptr seg */
    *(u16 *)(s + 0x1D) = 0x12A4;                       /* anim_ptr off (script 12a4) */
    s[0x2D] = 0x00;                                    /* substate = 0 */
    { u16 zl = *(u16 *)(s + 0x15);                     /* z_pos-- (32-bit) */
      *(u16 *)(s + 0x15) = (u16)(zl - 1);
      *(u16 *)(s + 0x17) -= (u16)(zl == 0); }
    *(u16 *)(s + 0x21) = 0x0000;                       /* state_phase = 0 */
    *(i16 *)(s + 0x23) = (i16)(*(i16 *)(s + 0x11) - 10); /* timer = firer.vz - 10 */
    *(u16 *)(s + 0x11) = 0x0000;                       /* vz = 0 */
    *(u16 *)(s + 0x13) = 0x0000;                       /* vy = 0 */
    *(u16 *)(s + 0x0F) = 0x0000;                       /* vx = 0 */
}

/* port 13a8:1ac0 — ballistic_aim: steer the shot toward the player, the step
 * scaled by the time-to-impact iVar1 = di / (speed - vz). If speed==vz the shot
 * despawns (a faithful quirk: it does NOT decrement d4e1). */
static void ballistic_aim(Entity *e)
{
    u8 *p = (u8 *)e;
    i16 vz = *(i16 *)(p + 0x11);
    i16 denom = (i16)((i16)GW(0xE9CC, i16) - vz);      /* c7dc(speed) - vz */
    if (denom == 0) { e->type_state = 0xFF; return; }
    i16 iv = (i16)(*(i16 *)(p + 3) / denom);           /* di / (speed - vz) */
    if (iv == 0) {
        *(i16 *)(p + 0x0F) = 0;
    } else {
        *(i16 *)(p + 0x0F) = (i16)(((i16)GW(0xDD84, i16) - *(i16 *)(p + 1)) / iv);
        *(i16 *)(p + 0x13) = (i16)(((0x87 - (i16)GW(0xDD86, i16)) - *(i16 *)(p + 5)) / iv);
    }
}

/* common tail: enemy slipped past the player (di<0) -> contact-test + despawn */
static void ent_pass_player(Entity *e)
{
    if (ent_hit_player(e)) G_PLAYER_HIT = 1;
    G_ENEMY_COUNT -= 1;
    e->type_state = 0xFF;
}

/* ------------------------------------------------------------------ */
/* player projectiles (161c:0038 / 0205) + explosion (161c:0555)       */
/* ------------------------------------------------------------------ */

/* pool slot base pointer (raw byte access, offsets as in the ROM) */
#define POOL ((u8 *)&G + 0xE5CC)

/* port 161c:0038 — the player's GUN shot (type 0). Flight: two boost
 * steps (timer += 10 -> vz; vx = param*10 then vx <<= 1), lifetime 0x14
 * phases; then an AABB sweep over pool slots 0..14: any enemy (type > 10,
 * not the enemy shot ':'=58) inside its box loses 1 hp; if it survives it
 * gets hit_flag + a secondary explosion copy; the shot itself becomes a
 * type-5 explosion pinned just in front of the target. */
void ent_smart_bomb(Entity *e, int di)
{
    u8 *me = (u8 *)e;
    if ((i16)e->state_phase < 2) {
        e->timer = (u16)(e->timer + 10);
        e->vz = (i16)e->timer;
        if (e->state_phase == 0) e->vx = (i16)((i16)e->param * 10);
        else                     e->vx = (i16)(e->vx << 1);
    }
    if (di < 0xE7) {
        u16 ph = e->state_phase;
        e->state_phase = (u16)(ph + 1);
        if (ph == 0x14) e->type_state = 0xFF;          /* burned out */
        for (int i = 0; i < 0x0F; ++i) {
            u8 *t = POOL + i * 0x33;
            i16 dz = (i16)(*(i16 *)(t + 3) - (i16)di);
            i16 dx = (i16)(*(i16 *)(t + 1) - (i16)e->screen_x);
            i16 dy = (i16)((i16)e->screen_y - *(i16 *)(t + 5));
            if ((i8)t[0] > 10 && t[0] != 0x3A
                && dz >= 0 && dz <= (i16)(e->vz - *(i16 *)(t + 0x11))
                && dx <  (i16)(u8)t[0x31] && dx > -(i16)(u8)t[0x31]
                && dy <  (i16)(u8)t[0x32] && dy >= 0) {
                if (t[0x2F]) t[0x2F] -= 1;             /* target hp-- */
                if (t[0x2F]) {                         /* survived: flash + boom */
                    t[0x30] = 0x01;                    /* hit_flag */
                    for (int k = 0; k < 0x0F; ++k) {
                        u8 *f = POOL + k * 0x33;
                        if ((i8)f[0] >= 0) continue;
                        memcpy(f, me, 0x33);           /* explosion = shot copy */
                        f[0] = 0x05;
                        *(u16 *)(f + 0x1D) = 0x135C;   /* morph script 3 */
                        *(u16 *)(f + 0x1F) = 0x0DE1;
                        *(u16 *)(f + 0x23) = 0x04;     /* timer */
                        *(u16 *)(f + 0x11) = *(u16 *)(t + 0x11);   /* vz = target vz */
                        *(i16 *)(f + 0x05) = (i16)(*(i16 *)(f + 0x05) - 0x14);
                        break;
                    }
                }
                e->type_state = 0x05;                  /* shot -> explosion */
                *(u16 *)(me + 0x1D) = 0x135C;
                *(u16 *)(me + 0x1F) = 0x0DE1;
                e->timer = 0x00;
                u16 tzl = *(u16 *)(t + 0x15);
                *(u16 *)(me + 0x17) = (u16)(*(u16 *)(t + 0x17) - 1 + (tzl != 0));
                *(u16 *)(me + 0x15) = (u16)(tzl - 1);  /* z = target z - 1 */
                e->screen_y = (u16)((i16)e->screen_y - 0x14);
            }
        }
    } else {                                           /* overshot: detonate */
        e->type_state = 0x05;
        *(u16 *)(me + 0x1D) = 0x135C;
        *(u16 *)(me + 0x1F) = 0x0DE1;
        e->timer = 0x00;
        e->screen_y = 0x00;
    }
}

/* port 161c:0205 — the HOMING MISSILE (type 1): accelerate (vz up to 0x40);
 * acquire the nearest targetable slot ahead (window 0xFA, tie-break by the
 * steering side), remember its slot + type; then steer vx=dx>>2, vy=dy>>1
 * until inside +/-0x1D — the hit takes 10 hp; a surviving target gets the
 * missile turned into an explosion COPY OF THE TARGET at its position. Past
 * di 0xE7 the missile detonates harmlessly. */
void ent_homing_missile(Entity *e, int di)
{
    u8 *me = (u8 *)e;
    if (di < 0xE7) {
        if ((i16)e->vz < 0x40) e->vz += 1;
        if ((i16)e->state_phase < 0) {                 /* acquire */
            e->timer = 0xFA;
            e->vx = 0; e->vy = 0;
            for (int i = 0; i < 0x0F; ++i) {
                u8 *t = POOL + i * 0x33;
                if ((i8)t[0] < 0 || t[0x2B] == 0) continue;
                i16 dz = (i16)(*(u16 *)(t + 0x15) - (u16)e->z_pos);
                if (dz < 0 || dz > (i16)e->timer) continue;
                int ok = 1;
                if ((i16)e->timer == dz) {             /* tie: prefer our side */
                    if ((i16)GW(0xDD84, u16) < 0) ok = (*(i16 *)(t + 7) <= 0);
                    else                          ok = (*(i16 *)(t + 7) >= 0);
                }
                if (ok) { e->state_phase = (u16)i; e->timer = (u16)dz; }
            }
            if ((i16)e->state_phase >= 0)              /* remember the type */
                e->timer = (u16)(i16)(i8)POOL[(i16)e->state_phase * 0x33];
        } else {
            u8 *t = POOL + (i16)e->state_phase * 0x33;
            if ((i16)e->timer == (i16)(i8)t[0]) {      /* target still alive */
                if (*(i16 *)(t + 3) < (i16)e->di) {    /* passed it */
                    e->state_phase = 0xFFFF; e->vx = 0; e->vy = 0;
                } else {
                    i16 dx = (i16)(*(i16 *)(t + 7) - (i16)e->x_accum);
                    i16 dy = (i16)(*(i16 *)(t + 5) - (i16)e->screen_y);
                    if ((i16)(e->di + e->vz) < *(i16 *)(t + 3)
                        || dx < -0x1D || dx > 0x1D || dy < -0x1D || dy > 0x1D) {
                        e->vx = (i16)((dx >> 2) ? (dx >> 2) : dx);
                        e->vy = (i16)((dy >> 1) ? (dy >> 1) : dy);
                    } else {                           /* HIT: 10 hp */
                        i8 hp = (i8)t[0x2F];
                        i8 nhp = (i8)(hp - 10);
                        t[0x2F] = (u8)nhp;
                        if (nhp <= 0) {
                            t[0x2F] = 0x00;            /* dies via its handler */
                            e->type_state = 0xFF;
                        } else {
                            memcpy(me, t, 0x33);       /* explosion AT the target */
                            me[0] = 0x05;
                            me[0x2E] = 0x00;
                            *(u16 *)(me + 0x1B) = 0x00;
                            *(u16 *)(me + 0x1D) = 0x135C;
                            *(u16 *)(me + 0x1F) = 0x0DE1;
                            me[0x2D] = 0x00;
                            *(u16 *)(me + 0x23) = 0x14;
                            u16 tzl = *(u16 *)(t + 0x15);
                            *(u16 *)(me + 0x17) = (u16)(*(u16 *)(t + 0x17) - 1 + (tzl != 0));
                            *(u16 *)(me + 0x15) = (u16)(tzl - 1);
                            *(i16 *)(me + 0x03) = (i16)(*(i16 *)(t + 3) - 1);
                        }
                    }
                }
            } else { e->state_phase = 0xFFFF; e->vx = 0; e->vy = 0; }
        }
    } else {                                           /* flew too far */
        e->type_state = 0x05;
        *(u16 *)(me + 0x1D) = 0x135C;
        *(u16 *)(me + 0x1F) = 0x0DE1;
        e->timer = 0x00;
        e->di = 0xF0;
        e->vz = 0x00;
    }
}

/* port 161c:0555 — small EXPLOSION (type 5): pulses the morph script's first
 * threshold @0x135C (0x3C/0x50 alternating with the timer parity), counts the
 * timer down to despawn, decays vz toward 0, halves screen_y when substate is
 * set; outside (di<1 or >0xFA) it despawns — passing the player leaves the
 * cockpit flash tF085 = 0x2E at (wDE67, tDE6B) = the slot's x_accum/screen_y. */
void ent_explosion_small(Entity *e, int di)
{
    u8 *me = (u8 *)e;
    if (di < 1 || di > 0xFA) {
        e->type_state = 0xFF;
        if (me[0x2D]) G_ENEMY_COUNT -= 1;
        if (di < 1) {
            GW(0xF085, u16) = 0x2E;                    /* ce95: overlay sprite */
            GW(0xDE67, u16) = *(u16 *)(me + 0x09);     /* bc77 */
            GW(0xDE6B, u16) = *(u16 *)(me + 0x0B);     /* bc7b */
        }
        return;
    }
    if ((e->timer & 1) == 0) {
        GW(0x135C, i16) = 0x3C;                        /* morph pulse (+sfx 0x13) */
    } else {
        GW(0x135C, i16) = 0x50;
    }
    u16 t = e->timer;
    e->timer = (u16)(t - 1);
    if (t == 0) {
        e->type_state = 0xFF;
        if (me[0x2D]) G_ENEMY_COUNT -= 1;
    }
    if (e->vz != 0) {
        if ((i16)e->vz < 1) e->vz += 1;
        else                e->vz -= 1;
    }
    if (me[0x2D] && (i16)e->screen_y != 0)
        e->screen_y = (u16)((i16)e->screen_y >> 1);
}

/* ------------------------------------------------------------------ */
/* handlers                                                           */
/* ------------------------------------------------------------------ */

/* port 161c:06af/06b4/06b9 — does nothing (inert/decorative types). */
void ent_noop(Entity *e, int di) { (void)e; (void)di; }

/* port 161c:06be — drifting enemy (RIDER, JEEPs, ECLAIR): no weapon; dies,
 * hits the player when it slips past, or despawns at the far edge. */
void ent_basic(Entity *e, int di)
{
    if (e->hp == 0)      { ent_die(e); return; }
    if (di < 0)          { ent_pass_player(e); }
    else if (di > 300)   { G_ENEMY_COUNT -= 1; e->type_state = 0xFF; }
}

/* port 161c:08f5/22ba/1f61 — thin alias that runs ent_basic. */
void ent_basic_alias(Entity *e, int di) { ent_basic(e, di); }

/* port 161c:0715 — basic enemy that randomly shoots while in the near band
 * (SIDE CAR, JEEP 3). ~8% chance/frame when within ~0x80 depth. */
void ent_basic_shooter(Entity *e, int di)
{
    if (e->hp == 0)      { ent_die(e); return; }
    if (di < 0)          { ent_pass_player(e); }
    else if (di < 0x12d) { if (di < 0x80 && rng16() > 30000) ent_fire_shot(e); }
    else                 { G_ENEMY_COUNT -= 1; e->type_state = 0xFF; }
}

/* port 161c:088f — fires on a fixed 16-frame cadence (TANK1/3, CANON1). */
void ent_periodic_shooter(Entity *e, int di)
{
    if (e->hp == 0) { ent_die(e); return; }
    if (di < 0)     { ent_pass_player(e); return; }
    u16 t = e->state_phase;
    e->state_phase = t + 1;
    if (t > 0xf) { e->state_phase = 0; ent_fire_shot(e); }
}

/* port 161c:0909 — fires every 10 frames while in the near band (CANON2,
 * GUARDIAN, POT GUN). Idles (no despawn) when far. */
void ent_shooter_b(Entity *e, int di)
{
    if (e->hp == 0)     { ent_die(e); return; }
    if (di < 0)         { ent_pass_player(e); return; }
    if (di < 0x96) {
        e->state_phase += 1;
        if ((i16)e->state_phase > 9) { e->state_phase = 0; ent_fire_shot(e); }
    }
}

/* port 161c:0976 — tanks that sweep side to side on a sine path and fire in a
 * mid band (CHAR1/2/3). x position follows the DGROUP sine LUT. */
void ent_wobble_shooter(Entity *e, int di)
{
    if (e->hp == 0) { ent_die(e); return; }
    if (di < 0)     { ent_pass_player(e); return; }

    if ((i16)G.aim_target < 0) e->vz += 1;
    else                       e->vz = (i16)G.scroll_speed - 2;

    e->state_phase = (e->state_phase + 7) & 0xff;
    const i16 *sine = (const i16 *)((uint8_t *)&G + 0x2DD5);   /* DAT_1de1_2dd5 */
    e->x_accum = (u16)(sine[e->state_phase] >> 1);

    if (e->hit_flag && di > 0x32 && di < 0x96) {
        e->timer += 1;
        if ((e->timer & 7) == 0) ent_fire_shot(e);
    }
}

/* port 161c:078d — diving militia (MILICE1/2): a 3-phase swoop launched from
 * one side; lives only while the carrier aircraft flag is set, then fires. */
void ent_swooper(Entity *e, int di)
{
    if (G_AIRCRAFT_FLAG == 0) { e->type_state = 0xFF; G_ENEMY_COUNT -= 1; return; }
    if (e->hp == 0)           { ent_die(e); return; }

    switch (e->state_phase) {
    case 0:
        /* launch side by the SIGN OF scroll_x (bb94) — ndisasm @161c:07d9:
         * `cmp word [0xdd84],0` (dd84 = bb94+21f0) -> x=0x40 else -0x40. (Was
         * wrongly G.aim_target/DC68=lives; demo/sweep never saw scroll_x<0 here,
         * the L2 play run @~2460 did — swooper spawned on the wrong side.) */
        e->x_accum  = (u16)(((i16)G.scroll_x < 0) ? 0x40 : -0x40);
        e->screen_x = (u16)((i16)e->x_accum - (i16)G.scroll_x);
        e->vz       = (i16)G.scroll_speed + 2;
        e->state_phase += 1;
        break;
    case 1:
        e->vz = (i16)G.scroll_speed + 2;
        if (di > 0x40) e->state_phase += 1;
        break;
    case 2:
        if      ((i16)G.aim_target < 0) e->vz += 1;
        else if (di < 0x40)             e->vz = (i16)G.scroll_speed + 1;
        else if (di < 0x41)             e->vz = (i16)G.scroll_speed;
        else                            e->vz = (i16)G.scroll_speed - 1;
        if (e->hit_flag) {
            u16 old = e->timer; e->timer = old + 1;
            if (old & 4) ent_fire_shot(e);
        }
        break;
    }
}

/* port 161c:0a33 — JUMPER: hops in from depth, then dive-bombs the player
 * vertically and rises back. Two phases via state_phase. */
void ent_dive_bomber(Entity *e, int di)
{
    if (e->hp == 0) { ent_die(e); return; }
    if (di < 0) {
        if (ent_hit_player(e)) { G_PLAYER_HIT = 1; G_ENEMY_COUNT -= 1; e->type_state = 0xFF; return; }
        e->di = 1; e->z_pos = 1;          /* clamp just in front of the player */
    } else if (di > 300) {
        G_ENEMY_COUNT -= 1; e->type_state = 0xFF; return;
    }

    if (e->state_phase == 0) {
        e->vz = 0;
        if (di < 0x81 || (i16)G.aim_target < 0) {
            if (di < 0x20 || (i16)e->timer < 0) {       /* trigger the dive */
                e->state_phase = 1;
                EF(e, 0x1b, i16) += 10;
                e->vz = (i16)G.scroll_speed + 6;
                e->vy = 0x14;
            } else if ((i16)G.aim_target < 0) {
                e->timer = 0xffff;
            } else {
                e->timer -= 1;
            }
        }
    } else if (e->screen_y == 0) {                       /* peak reached: reset */
        e->vy = 0; e->vz = 0; e->state_phase = 0; e->timer = 100;
        EF(e, 0x1b, i16) -= 10;
    } else {
        e->vz = (i16)G.scroll_speed + 3;
        e->vy -= 2;
    }
}

/* port 161c:0b4f — MIRADOR watchtower: stationary; fires with a rate that
 * tightens the closer the enemy is to the player's lateral line. */
void ent_strafer(Entity *e, int di)
{
    if (e->hp == 0) { ent_die(e); return; }
    if (di < 0)     { ent_pass_player(e); return; }

    if (di < 0xb0) {
        int t = ((i16)e->x_accum < 0)
              ? (i16)G.scroll_x * 2 - (i16)e->x_accum
              : (i16)e->x_accum - (i16)G.scroll_x * 2;
        unsigned mask = (t < 0x46) ? 3u : (t < 0x8c) ? 7u : (t < 0xd2) ? 0xfu : 0xffffu;
        u16 old = e->state_phase; e->state_phase = old + 1;
        if ((mask & old) == 0) ent_fire_shot(e);
    } else if (e->state_phase == 0) {
        e->state_phase = (u16)rng16();
    }
}

/* port 161c:0c1a — WALKER gunship: tough multi-state mech. While weak (hp<4)
 * or during a global window it just advances and rakes shots; otherwise it
 * weaves laterally toward its anchor X and fires aimed bursts, bobbing in Y. */
void ent_gunship(Entity *e, int di)
{
    if (e->hp == 0)      { ent_die(e); return; }
    if (di >= 0x12d)     { G_ENEMY_COUNT -= 1; e->type_state = 0xFF; return; }
    if (di < 0) {
        if (ent_hit_player(e)) { G_PLAYER_HIT = 1; G_ENEMY_COUNT -= 1; e->type_state = 0xFF; return; }
        e->vx = 0; e->timer = 0;
    }

    if ((int8_t)e->hp < 4 || G.invuln_timer == 0) {
        e->vz = (i16)G.scroll_speed + 4;
        e->vy = 0; e->vx = 0;
        EF(e, 0x27, i16) += 1;
        if ((EF(e, 0x27, i16) & 0xf) == 0) ent_fire_shot(e);
    } else {
        if (e->state_phase == 0) {
            e->state_phase = EF(e, 0x1b, i16);
            EF(e, 0x27, i16) = (i16)e->x_accum;          /* remember anchor X */
        }
        int iv = e->timer; e->timer += 1;
        if (iv < 0x65) {
            iv = (EF(e, 0x27, i16) - (i16)e->x_accum) >> 2;
            e->vz = (i16)G.scroll_speed - ((di - 0x10) >> 4);
            if ((e->timer & 0x3f) == 0 && iv + 0x1e < 0x3c) {
                e->vz += 6;
                e->z_pos -= 1;
                ent_fire_shot(e);
                e->z_pos += 1;
                e->vz -= 6;
            }
        } else {
            e->vz -= 1;
            iv = ((i16)G.scroll_x * 2 - (i16)e->x_accum) >> 2;
        }
        e->vx = iv;

        if (e->vy == 0 && (int8_t)e->hp < 7) {
            e->param ^= 1;
            EF(e, 0x1b, i16) = (i16)e->state_phase + (e->param ? 10 : 0);
        }
        if ((i16)e->vy < 0) {
            e->param ^= 1;
            EF(e, 0x1b, i16) = (i16)e->state_phase + (e->param ? 10 : 0);
            e->screen_y = 0;
            e->vy = 8;
        } else {
            e->vy -= 2;
        }
    }
}

/* port 161c:0de6 — MINE AIR: inert floating hazard; only dies or detonates on
 * contact when it slips past the player. */
void ent_mine(Entity *e, int di)
{
    if (e->hp == 0) { ent_die(e); return; }
    if (di < 0)     { ent_pass_player(e); }
}

/* port 161c:0e29 — BOMBE TH crash target: as it passes, if it is over the
 * player crosshair it detonates (hitting the player); otherwise despawns. */
void ent_crash_target(Entity *e, int di)
{
    if (e->hp == 0) { ent_die(e); return; }
    if (di < 0) {
        int dx = (i16)e->x_accum - 2 * (i16)G.scroll_x;         /* x_accum - 2*bb94 (161c:0e29) */
        int dy = (i16)e->screen_y - (0x87 - (i16)G.scroll_horizon); /* screen_y - (0x87-bb96) */
        if (dx < 0x8c && dx > -0x8c && dy < 0x28 && dy > -0x28) {
            G_PLAYER_HIT = 1;
            e->di = 0;
            e->type_state = 5;            /* explode */
            e->shape_kind = 0;
            e->anim_ptr = 0x0DE1135CUL;   /* explosion morph script 1de1:135c */
            EF(e, 0x1b, i16) = 0;
            e->timer = 10;
        } else {
            G_ENEMY_COUNT -= 1; e->type_state = 0xFF;
        }
    }
}

/* port 161c:0ec6 — MOUCHARD aircraft (and the bare type 55/56): a strafing
 * boss-ish flyer with three behavior windows (approach run, attack loop, far
 * cruise). Carries the swooper carrier flag (G_AIRCRAFT_FLAG). */
void ent_aircraft(Entity *e, int di)
{
    if (e->hp == 0) { ent_die(e); return; }

    if (di < 0) {
        if (ent_hit_player(e)) G_PLAYER_HIT = 1;
        e->vz = (i16)G.scroll_speed + 1;
    }
    if ((i16)e->screen_y < 0) e->screen_y = 0;

    if (e->timer == 0) {                                  /* approach run */
        if (di < 0x20) e->vx += 1;
        if (di < 5) {
            e->vz = (i16)G.scroll_speed;
            e->di = 4;
            e->timer = 1;
            e->param = EF(e, 0x1b, i16);
        } else {
            G_AIRCRAFT_FLAG = 0;
            if ((i16)e->screen_y < 0x30) e->vy += 3; else e->vy -= 3;
            e->vz = (i16)G.scroll_speed - 3;
        }
        if ((i16)G.aim_target < 0) e->timer = 2;
    } else if (e->timer == 1) {                           /* attack loop */
        if ((i16)G.aim_target < 0) e->timer = 2;
        else { u16 sp = e->state_phase; e->state_phase = sp + 1; if ((i16)sp > 100) e->timer = 2; }
        if ((i16)e->screen_y < 0x30) e->vy += 3; else e->vy -= 3;
        if ((i16)e->x_accum < 0)     e->vx += 4; else e->vx -= 4;
        e->vz = (i16)G.scroll_speed;
        if (di < 4) e->vz += 1;
        EF(e, 0x1b, i16) = (i16)e->param + ((e->state_phase & 8) == 0 ? 10 : 0);
    } else if (di < 0x12d) {                              /* far cruise */
        e->vz = (i16)G.scroll_speed + 6;
        if ((i16)e->screen_y < 0x30) e->vy += 3; else e->vy -= 3;
        if ((i16)e->x_accum < 0x30) e->vx += 2; else e->vx -= 2;
    } else {
        G_AIRCRAFT_FLAG = 1;
        G_ENEMY_COUNT -= 1;
        e->type_state = 0xFF;
    }
}

/* port 161c:1e40 — BANZAI tracker: continuously snaps itself onto the player
 * crosshair while advancing, so it bears straight down on the player. */
void ent_tracker(Entity *e, int di)
{
    if (e->hp == 0) { ent_die(e); return; }
    if (di < 0)     { ent_pass_player(e); return; }
    e->vz       = (i16)G.scroll_speed - 3;
    e->x_accum  = (u16)((i16)G.scroll_x << 1);         /* bb94 << 1 (161c:1e40) */
    e->screen_y = (u16)(0x87 - (i16)G.scroll_horizon); /* 0x87 - bb96 */
}

/* port 161c:22f6 — enemy ballistic shot (type 58 ':'). Steers toward the player:
 * on the first active frame (state_phase 0) latch vz = timer (= firer.vz-10) and
 * aim; thereafter re-aim every 4th frame (unless the +0x27 gate is set), with a
 * 0x50-frame lifetime cap; when it passes the player (di<1) it nudges one final
 * step (screen_x/y -= vel/(vz/-di)), tests the hit (-> weapon_state=1), then
 * despawns. Disabled (auto-despawn) while bDE6D set or game_mode==4. */
void enemy_update_ballistic(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (GW(0xDE6D, u8) != 0 || (i16)GW(0xF6AF, i16) == 4) {   /* bDE6D / game_mode==4 */
        e->type_state = 0xFF; G_SHOT_COUNT -= 1; return;
    }
    if (di < 1) {                                            /* passed the player */
        i16 t3 = *(i16 *)(p + 3);
        if (t3 != 0) {
            i16 iv = (i16)(*(i16 *)(p + 0x11) / (i16)(-t3));  /* vz / -di */
            if (iv != 0) {
                *(i16 *)(p + 1) = (i16)(*(i16 *)(p + 1) - *(i16 *)(p + 0x0F) / iv);
                *(i16 *)(p + 5) = (i16)(*(i16 *)(p + 5) - *(i16 *)(p + 0x13) / iv);
            }
        }
        if (ent_hit_player(e)) G_PLAYER_HIT = 1;             /* d4c1 = 1 */
        e->type_state = 0xFF; G_SHOT_COUNT -= 1;
    } else if (*(i16 *)(p + 0x21) == 0) {                    /* first active frame */
        *(u16 *)(p + 0x21) = 1;                              /* state_phase = 1 */
        *(u16 *)(p + 0x11) = *(u16 *)(p + 0x23);             /* vz = timer */
        ballistic_aim(e);
    } else {
        i16 ph = *(i16 *)(p + 0x21);
        *(u16 *)(p + 0x21) = (u16)(ph + 1);                 /* state_phase++ */
        if (ph > 0x50) { e->type_state = 0xFF; G_SHOT_COUNT -= 1; }   /* lifetime cap */
        if (*(i16 *)(p + 0x27) == 0) {
            *(u16 *)(p + 0x25) = (u16)(*(u16 *)(p + 0x25) + 1);       /* param++ */
            if ((*(u16 *)(p + 0x25) & 3) == 0) ballistic_aim(e);     /* re-aim /4 */
        }
    }
}

/* port 161c:1abe — ent_splitter (POULE, type 38). ACTIVE: vz = speed - 4 (dives a
 * touch faster than the scroll, so its di drops one extra per frame vs a plain
 * scroll-locked enemy). On death (hp==0): SPLIT — copy this record into the first
 * free slot as an EGG (type 39 '\''), shape base +10, hp 10, live count++, then run
 * the normal death (explosion + score, ent_die). On passing the player (di<0):
 * hit-test + despawn. */
void ent_splitter(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2F] == 0x00) {                              /* hp == 0 -> split */
        u8 *s = POOL; int i = 0;
        for (; (i8)s[0] >= 0 && i < 0x14; ++i) s += 0x33;   /* first free slot 0..19 */
        if (i < 0x14) {
            memcpy(s, p, 0x33);                         /* copy poule -> egg */
            s[0]    = 0x27;                             /* type = '\'' = 39 (EGG) */
            s[0x2B] = 0x00;                             /* targetable = 0 */
            *(i16 *)(s + 0x1B) = (i16)(*(i16 *)(s + 0x1B) + 10); /* shape base += 10 */
            s[0x2F] = 0x0A;                             /* hp = 10 */
            G_ENEMY_COUNT += 1;                         /* d27a++ */
        }
        ent_die(e);                                     /* FUN_161c_2b95 */
    } else if (di < 0) {                                /* passed the player */
        ent_pass_player(e);
    } else {
        e->vz = (i16)((i16)GW(0xE9CC, i16) - 4);        /* vz = speed - 4 */
    }
}

/* port 15ae:05a6 — WIDE AABB vs the player (|dx|<0x1e && |dy|<0x1e). */
static int ent_hit_player_wide(Entity *e)
{
    int dx = (i16)e->screen_x - (i16)GW(0xDD84, u16);
    if (!(dx < 0x1e && dx > -0x1e)) return 0;
    int dy = (i16)e->screen_y - (0x87 - (i16)GW(0xDD86, u16));
    return (dy < 0x1e && dy > -0x1e);
}

/* port 161c:1b69 — ent_bouncing_hazard (EGG, type 39, from a POULE split). While
 * di in [0,0x12d): if screen_y<0 flip vy (bounce up) and apply, else vy -= 2
 * (gravity) — the egg arcs up then falls. On death (hp==0): normal death. On
 * passing the player (di<0): WIDE hit-test -> hit feedback (wF6A9=0x1e, wF203=0xd);
 * despawn. Beyond di 0x12d: despawn. */
void ent_bouncing_hazard(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2F] == 0x00) { ent_die(e); return; }
    if (di < 0) {
        if (ent_hit_player_wide(e)) {                   /* screen collision */
            GW(0xF6A9, u16) = 0x1E;                     /* d4b9 (+ sfx 3, omitted) */
            GW(0xF203, u16) = 0x0D;                     /* d013 */
        }
        G_ENEMY_COUNT -= 1;
        e->type_state = 0xFF;
    } else if (di < 0x12d) {
        if (*(i16 *)(p + 5) < 0) {                      /* screen_y < 0: bounce */
            *(i16 *)(p + 0x13) = (i16)(-*(i16 *)(p + 0x13));            /* vy = -vy */
            *(i16 *)(p + 5) = (i16)(*(i16 *)(p + 5) + *(i16 *)(p + 0x13)); /* screen_y += vy */
        } else {
            *(i16 *)(p + 0x13) = (i16)(*(i16 *)(p + 0x13) - 2);        /* vy -= 2 */
        }
    } else {
        G_ENEMY_COUNT -= 1;
        e->type_state = 0xFF;
    }
}

/* port 161c:123d — ent_interceptor (FLYING H, type 34). A 4-state air attacker
 * (state_phase @0x21, jump table): 0 APPROACH (latch anchor x_accum @0x27, sweep
 * toward the player's altitude, vz=speed-4), 1 TRACK (home vx/vy clamped ±10/±5,
 * count a timer up to 0x5a), 2 DIVE (aim at the player crosshair, vz-=blink), 3
 * RETREAT (decay vx/vy by half, vz+=2). The decompiler's unaff_DI = vx (+0xf) and
 * unaff_SI = vy (+0x13); both are computed per-state and stored at the end. On
 * death (hp==0): normal death. di<1 -> state 3 (or a state-2 hit despawns it);
 * low lives (tDC68<0) or hp<4 forces state 0. */
void ent_interceptor(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2F] == 0x00) { ent_die(e); return; }
    int vx = *(i16 *)(p + 0x0F), vy = *(i16 *)(p + 0x13);   /* SI/DI, overwritten below */

    if (di < 1) {
        if (*(i16 *)(p + 0x21) == 2) {
            if (GW(0xF6AF, u16) == 0) *(i16 *)(p + 5) = 0;      /* game_mode==0: screen_y=0 */
            if (ent_hit_player(e)) {                            /* 15ae:056a */
                G_PLAYER_HIT = 1; G_ENEMY_COUNT -= 1; e->type_state = 0xFF; return;
            }
        }
        *(i16 *)(p + 0x21) = 3;
    }
    if ((i16)GW(0xDC68, u16) < 0 || (i8)p[0x2F] < 4) *(i16 *)(p + 0x21) = 0;

    switch (*(u16 *)(p + 0x21) < 4 ? *(u16 *)(p + 0x21) : 0xFFFF) {
    case 0:                                                     /* APPROACH */
        if (*(i16 *)(p + 0x25) == 0) {
            *(i16 *)(p + 0x25) = 1;
            *(i16 *)(p + 0x27) = *(i16 *)(p + 7);               /* latch anchor = x_accum */
        }
        vx = (i16)(*(i16 *)(p + 0x27) * 2 - *(i16 *)(p + 7));
        vy = (i16)((0x5A - *(i16 *)(p + 5)) >> 3);
        if (di < 10) *(i16 *)(p + 0x21) = 1;
        if ((i16)GW(0xDC68, u16) < 0 || (i8)p[0x2F] < 4) {
            if (di > 300) { G_ENEMY_COUNT -= 1; e->type_state = 0xFF; return; }
            *(i16 *)(p + 0x11) += 1;                            /* vz += 1 */
        } else {
            *(i16 *)(p + 0x11) = (i16)((i16)GW(0xE9CC, i16) - 4);   /* vz = speed - 4 */
        }
        break;
    case 1:                                                     /* TRACK */
        vx = (i16)(*(i16 *)(p + 0x27) - *(i16 *)(p + 7));
        vy = (i16)(0x5A - *(i16 *)(p + 5));
        if (vx >= 0xB) vx = 10; else if (vx < -10) vx = -10;
        if (vy >= 6)   vy = 5;  else if (vy < -5)  vy = -5;
        *(i16 *)(p + 0x11) = (i16)GW(0xE9CC, i16);              /* vz = speed */
        if (di < 8) { if (di < 6) *(i16 *)(p + 0x11) += 1; }
        else        *(i16 *)(p + 0x11) -= 1;
        if (GW(0xF6AF, u16) < 4) {
            i16 t = *(i16 *)(p + 0x23); *(i16 *)(p + 0x23) = (i16)(t + 1);
            if (t > 0x5A) *(i16 *)(p + 0x21) = 2;
        } else *(i16 *)(p + 0x23) = 0;
        break;
    case 2:                                                     /* DIVE */
        vy = (i16)((0x87 - (i16)GW(0xDD86, u16)) - *(i16 *)(p + 5));
        vx = (i16)((i16)GW(0xDD84, u16) * 2 - *(i16 *)(p + 7));
        if (vy > 1 || vy < -1) vy >>= 1;
        if (vx >= 0x1F) vx = 0x1E; else if (vx < -0x1E) vx = -0x1E;
        if (vy >= 0xB)  vy = 10;  else if (vy < -10)   vy = -10;
        *(i16 *)(p + 0x23) = 0;
        *(i16 *)(p + 0x11) -= (i16)GW(0x24FF, u16);            /* vz -= blink parity */
        break;
    case 3:                                                     /* RETREAT */
        vx = (i16)(-((*(i16 *)(p + 0x0F) + 1) >> 1));
        vy = (i16)(-((*(i16 *)(p + 0x13) + 1) >> 1));
        if (*(i16 *)(p + 0x11) < (i16)GW(0xE9CC, i16) + 1) *(i16 *)(p + 0x11) += 2;
        if (di >= -1) *(i16 *)(p + 0x21) = 1;
        break;
    default: break;                                            /* state>=4: skip */
    }
    *(i16 *)(p + 0x0F) = (i16)vx;
    *(i16 *)(p + 0x13) = (i16)vy;
}

/* port 161c:2504 — enemy_update_split4_descend (B CITERN "leader" boss, type 73).
 * ALIVE: rides toward the player; near (di<0x1f) shape_kind = a3a3e[level] (a
 * multi-part boss shape) and vz = speed(+1); mid (di<300) vz = speed-4 clamp>=10
 * (or +1 while the road window wDC6C==0); far (di>=300) despawn. DEATH (hp==0):
 * fire LEADER SHOT DOWN (DE50=7), set anim_step, morph this slot into a large
 * explosion (type 6, overriding ent_die's 5), and BREAK APART into 4 sub-explosion
 * copies at the a3a2e offsets {(80,130)(-80,130)(120,0)(-120,0)}. d600 (@0xF7F0,
 * engine-rumble pitch) tracks vz. */
void enemy_update_split4_descend(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    p[0x2E] = 0;                                             /* shape_kind = 0 */
    if (p[0x2F] == 0x00) {                                  /* boss destroyed */
        GW(0xF7F0, i16) = (i16)((i16)GW(0xE9CC, i16) + 2);  /* d600 = speed + 2 */
        GW(0xDC6E, u8) = 1;                                 /* ba7e anim_step = 1 */
        GW(0xDE50, u8) = 7; GW(0xDE52, u8) = 1; GW(0xDE51, u8) = 0x3C;  /* LEADER SHOT DOWN */
        ent_die(e);                                         /* FUN_161c_2b95 */
        e->type_state = 6;                                  /* large explosion (overrides 5) */
        *(i16 *)(p + 0x23) = (i16)(*(i16 *)(p + 0x23) + 10); /* timer += 10 */
        *(i16 *)(p + 0x11) -= 1;                            /* vz -= 1 */
        const i16 *tbl = (const i16 *)((u8 *)&G + 0x3A2E);  /* 4 {dx,dy} pairs */
        u8 *s = POOL; int slot = 0;
        for (int k = 0; k < 4; ++k) {                       /* 4 forward free slots */
            while ((i8)s[0] >= 0 && slot <= 0x0E) { ++slot; s += 0x33; }
            if (slot < 0x0F) {
                memcpy(s, p, 0x33);                         /* copy boss-as-explosion */
                s[0x2D] = 0;                                /* substate = 0 */
                *(i16 *)(s + 7) = (i16)(*(i16 *)(s + 7) + tbl[k * 2]);     /* x_accum += dx */
                *(i16 *)(s + 5) = (i16)(*(i16 *)(s + 5) + tbl[k * 2 + 1]); /* screen_y += dy */
            }
        }
    } else {
        if (di < 0x1F) {                                    /* near */
            p[0x2E] = GW(0x3A3E + GW(0x27C7, u16), u8);     /* shape_kind = a3a3e[level] */
            *(i16 *)(p + 0x11) = (i16)GW(0xE9CC, i16);      /* vz = speed */
            if (di < 0x19 || GW(0xDC6C, u16) == 0) *(i16 *)(p + 0x11) += 1;
        } else if (di < 300) {                              /* mid */
            if (GW(0xDC6C, u16) == 0) {
                *(i16 *)(p + 0x11) += 1;
            } else {
                i16 v = (i16)((i16)GW(0xE9CC, i16) - 4);
                if (v < 10) v = 10;
                *(i16 *)(p + 0x11) = v;
            }
        } else {                                            /* far: despawn */
            e->type_state = 0xFF; G_ENEMY_COUNT -= 1;
        }
        GW(0xF7F0, i16) = *(i16 *)(p + 0x11);               /* d600 = vz */
    }
}

/* port 161c:2648 — enemy_update_split4_spawner (B/L MINES, convoy segment type 74).
 * Rides in like the descend boss; when near (di<0x1f) it sets shape_kind 3 and, every
 * 8th frame (state_phase&7==0), DROPS A MINE — a copy of the proto @DGROUP 0x224f placed
 * trailing just behind it (di/z_pos = boss - (vz+1), vx = boss.x_accum>>4, x_accum = boss
 * x_accum). Sways on the sine LUT (a2DD5[timer]>>1). Far (di>=0x1f): vz = speed-4 clamp>=10.
 * DEATH (hp==0): LEADER SHOT DOWN (DE50=7) + anim_step, morph to a large explosion (type 6),
 * break apart into 4 sub-explosion copies at the a3a43 {dx,dy} offsets. */
void enemy_update_split4_spawner(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    p[0x2E] = 0;                                              /* shape_kind = 0 */
    if (p[0x2F] == 0x00) {                                    /* boss destroyed */
        GW(0xDC6E, u8) = 1;                                   /* ba7e anim_step = 1 */
        GW(0xDE50, u8) = 7; GW(0xDE52, u8) = 1; GW(0xDE51, u8) = 0x3C;   /* LEADER SHOT DOWN */
        ent_die(e);
        e->type_state = 6;                                   /* large explosion */
        *(i16 *)(p + 0x23) = (i16)(*(i16 *)(p + 0x23) + 10); /* timer += 10 */
        *(i16 *)(p + 0x11) -= 1;                             /* vz -= 1 */
        const i16 *tbl = (const i16 *)((u8 *)&G + 0x3A43);   /* 4 {dx,dy} pairs */
        u8 *s = POOL; int slot = 0;
        for (int k = 0; k < 4; ++k) {
            while ((i8)s[0] >= 0 && slot <= 0x0E) { ++slot; s += 0x33; }
            if (slot < 0x0F) {
                memcpy(s, p, 0x33);                          /* copy boss-as-explosion */
                *(i16 *)(s + 7) = (i16)(*(i16 *)(s + 7) + tbl[k * 2]);       /* x_accum += dx */
                *(i16 *)(s + 5) = (i16)(*(i16 *)(s + 5) + tbl[k * 2 + 1]);   /* screen_y += dy */
            }
        }
    } else {
        if (di < 0x1F) {                                     /* near: ride + drop mines */
            *(i16 *)(p + 0x11) = (i16)GW(0xE9CC, i16);       /* vz = speed */
            if (di < 0x19 || GW(0xDC6C, u16) == 0) *(i16 *)(p + 0x11) += 1;
            p[0x2E] = 3;                                     /* shape_kind = 3 */
            u16 ph = (u16)(*(u16 *)(p + 0x21) + 1);
            *(u16 *)(p + 0x21) = (u16)(ph & 7);
            if ((ph & 7) == 0) {                             /* every 8 frames: drop a mine */
                u8 *s = POOL; int slot = 0;
                while ((i8)s[0] >= 0 && slot <= 0x0E) { ++slot; s += 0x33; }
                if (slot < 0x0F) {
                    memcpy(s, (u8 *)&G + 0x224F, 0x33);      /* mine proto @0x224f */
                    i16 vz = *(i16 *)(p + 0x11);
                    *(i16 *)(s + 0x0F) = (i16)(*(i16 *)(p + 7) >> 4);   /* vx = x_accum>>4 */
                    *(i16 *)(s + 0x11) = vz;                            /* vz */
                    u16 nvz = (u16)(vz + 1);
                    *(u16 *)(s + 7) = *(u16 *)(p + 7);                  /* x_accum = boss x_accum */
                    *(u16 *)(s + 3) = (u16)(*(i16 *)(p + 3) - (i16)nvz);/* di = boss di - (vz+1) */
                    u16 zl = *(u16 *)(p + 0x15);                        /* z_pos = boss z_pos - (vz+1) */
                    *(u16 *)(s + 0x17) = (u16)((*(i16 *)(p + 0x17) - ((i16)nvz >> 15)) - (u16)(zl < nvz));
                    *(u16 *)(s + 0x15) = (u16)(zl - nvz);
                    G_ENEMY_COUNT += 1;
                }
            }
        } else {                                             /* far */
            i16 v = (i16)((i16)GW(0xE9CC, i16) - 4);
            if (v < 10) v = 10;
            *(i16 *)(p + 0x11) = v;
        }
        *(u16 *)(p + 0x23) = (u16)((*(i16 *)(p + 0x23) + 3) & 0xFF);    /* timer = (timer+3)&0xff */
        *(i16 *)(p + 7) = (i16)(((const i16 *)((u8 *)&G + 0x2DD5))[*(i16 *)(p + 0x23)] >> 1);
    }
}

/* port 161c:281f — enemy_update_split4_strafe (B/L TETE, the convoy boss HEAD, type 77).
 * Rides in (near di<0x1f: vz=speed(+1), shape_kind=6 = the head shape; mid di<300: cruise
 * = vz+1 while the road window wDC6C==0 else speed-4 clamp>=10; far: despawn), sways on the
 * sine LUT (a2DD5[timer]>>2), and drives the engine-rumble d600(vz)/d602(x_accum). DEATH:
 * d600=speed+2, LEADER SHOT DOWN + anim_step, large explosion + 4 sub-explosions at a3a53. */
void enemy_update_split4_strafe(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    p[0x2E] = 0;
    if (p[0x2F] == 0x00) {                                   /* boss destroyed */
        GW(0xF7F0, i16) = (i16)((i16)GW(0xE9CC, i16) + 2);   /* d600 = speed+2 */
        GW(0xDC6E, u8) = 1;
        GW(0xDE50, u8) = 7; GW(0xDE52, u8) = 1; GW(0xDE51, u8) = 0x3C;
        ent_die(e);
        e->type_state = 6;
        *(i16 *)(p + 0x23) = (i16)(*(i16 *)(p + 0x23) + 10);
        *(i16 *)(p + 0x11) -= 1;
        const i16 *tbl = (const i16 *)((u8 *)&G + 0x3A53);
        u8 *s = POOL; int slot = 0;
        for (int k = 0; k < 4; ++k) {
            while ((i8)s[0] >= 0 && slot <= 0x0E) { ++slot; s += 0x33; }
            if (slot < 0x0F) {
                memcpy(s, p, 0x33);
                *(i16 *)(s + 7) = (i16)(*(i16 *)(s + 7) + tbl[k * 2]);
                *(i16 *)(s + 5) = (i16)(*(i16 *)(s + 5) + tbl[k * 2 + 1]);
            }
        }
    } else {
        if (di < 0x1F) {                                     /* near */
            *(i16 *)(p + 0x11) = (i16)GW(0xE9CC, i16);
            if (di < 0x19 || GW(0xDC6C, u16) == 0) *(i16 *)(p + 0x11) += 1;
            p[0x2E] = 6;                                     /* shape_kind = 6 (head) */
        } else if (di < 300) {                               /* mid */
            if (GW(0xDC6C, u16) == 0) {
                *(i16 *)(p + 0x11) += 1;
            } else {
                i16 v = (i16)((i16)GW(0xE9CC, i16) - 4);
                if (v < 10) v = 10;
                *(i16 *)(p + 0x11) = v;
            }
        } else {                                             /* far -> despawn */
            e->type_state = 0xFF; G_ENEMY_COUNT -= 1;
        }
        *(u16 *)(p + 0x23) = (u16)((*(i16 *)(p + 0x23) + 3) & 0xFF);
        *(i16 *)(p + 7) = (i16)(((const i16 *)((u8 *)&G + 0x2DD5))[*(i16 *)(p + 0x23)] >> 2);
        GW(0xF7F0, i16) = *(i16 *)(p + 0x11);                /* d600 = vz */
        GW(0xF7F2, i16) = *(i16 *)(p + 7);                   /* d602 = x_accum */
    }
}

/* port 161c:060d — ent_explosion_large (type 6, the boss break-apart burst). Like
 * the small explosion but pulses the morph threshold @0x135C between 0x78/0xa0 (a
 * big burst) by timer parity, counts the timer @0x23 down to despawn, and decays
 * vz toward 0. Passing the player (di<1) or beyond di 0xfa despawns, leaving the
 * cockpit flash tF085=0x2e at (wDE67, tDE6B). */
void ent_explosion_large(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (di < 1 || di > 0xFA) {
        e->type_state = 0xFF;
        if (p[0x2D]) G_ENEMY_COUNT -= 1;
        if (di < 1) {
            GW(0xF085, u16) = 0x2E;                    /* ce95 cockpit flash */
            GW(0xDE67, u16) = *(u16 *)(p + 0x09);      /* bc77 */
            GW(0xDE6B, u16) = *(u16 *)(p + 0x0B);      /* bc7b */
        }
        return;
    }
    if ((*(u16 *)(p + 0x23) & 1) == 0) GW(0x135C, i16) = 0x78;   /* morph pulse (+sfx 0x13) */
    else                               GW(0x135C, i16) = 0xA0;
    u16 t = *(u16 *)(p + 0x23);
    *(u16 *)(p + 0x23) = (u16)(t - 1);
    if (t == 0) {
        e->type_state = 0xFF;
        if (p[0x2D]) G_ENEMY_COUNT -= 1;
    }
    if (*(i16 *)(p + 0x11) != 0) {                     /* vz decay toward 0 */
        if (*(i16 *)(p + 0x11) < 1) *(i16 *)(p + 0x11) += 1;
        else                        *(i16 *)(p + 0x11) -= 1;
    }
}

/* ================================================================== */
/* Phase 2c — remaining enemy handlers (boss / formation / mine-layer  */
/* / pickup / sine). Ported from segment 161c (Ghidra decompiled.c);   */
/* the two DGROUP wave tables are the SINE LUT @0x2DD5 and the COSINE   */
/* LUT @0x2FD5 (256 signed words each). Verified vs the original via    */
/* the spawn-injection harness (--spawntest / cap_spawntest.py).        */
/* ================================================================== */

/* port 161c:2b63 — SILENT morph to a small explosion (unlike ent_die/2b95 it
 * bumps no kill/score counter): type=5, clear targetable/shape, explosion anim,
 * timer=10. (The original stores DS in the anim seg; the port uses its DGROUP
 * morph-seg convention 0x0DE1, exactly as ent_die does.) */
static void ent_morph_explosion(Entity *e)
{
    u8 *p = (u8 *)e;
    p[0]    = 0x05;                                /* -> ent_explosion_small */
    p[0x2b] = 0x00;                                /* targetable = 0 */
    p[0x2e] = 0x00;                                /* shape_kind = 0 */
    *(u16 *)(p + 0x1b) = 0x0000;
    *(u16 *)(p + 0x1f) = 0x0DE1;                   /* anim seg */
    *(u16 *)(p + 0x1d) = 0x135C;                   /* anim off — explosion morph */
    *(u16 *)(p + 0x25) = 0x0000;                   /* param = 0 */
    *(u16 *)(p + 0x23) = 0x000A;                   /* timer = 10 */
}

/* port 161c:0009 — first free pool slot in the range 0..14. Returns the slot
 * base pointer, or NULL when slots 0..14 are all occupied. */
static u8 *free_slot14(void)
{
    u8 *s = POOL; int i = 0;
    while ((i8)s[0] >= 0 && i < 0x0F) { ++i; s += 0x33; }
    return (i < 0x0F) ? s : NULL;
}

/* shared mine-drop (mine_layer 16a3 / mine_dropper 198d / strafer_layer 1f75):
 * copy the mine proto @DGROUP 0x20EA into a free slot, trailing the dropper by
 * (vz) in depth (32-bit z_pos / di subtract), inheriting its screen_y/x_accum. */
static void drop_mine(Entity *e)
{
    u8 *p = (u8 *)e;
    u8 *s = free_slot14();
    if (!s) return;
    memcpy(s, (u8 *)&G + 0x20EA, 0x33);            /* mine proto @0x20ea */
    u16 vz = *(u16 *)(p + 0x11);
    u16 zl = *(u16 *)(p + 0x15);
    *(u16 *)(s + 0x11) = vz;                                                    /* c3ed vz */
    *(u16 *)(s + 0x17) = (u16)((*(i16 *)(p + 0x17) - ((i16)vz >> 15)) - (u16)(zl < vz)); /* z hi */
    *(u16 *)(s + 0x15) = (u16)(zl - vz);                                        /* z lo */
    *(u16 *)(s + 0x03) = (u16)(*(i16 *)(p + 3) - (i16)vz);                      /* di -= vz */
    *(u16 *)(s + 0x05) = *(u16 *)(p + 5);                                       /* screen_y */
    *(u16 *)(s + 0x07) = *(u16 *)(p + 7);                                       /* x_accum */
    G_ENEMY_COUNT += 1;
}

/* port 161c:107b — ent_boss_strafer (LEADER, types 33 & 80). ALIVE (0<=di<=300):
 * vz = d600 (engine rumble pitch); on LEVEL 5 (index 4) it flies a serpentine
 * strafing run driven by both wave LUTs (x_accum = d602 + sin*3/4, screen_y =
 * |cos*3/4| + 0x28) and fires a burst whenever it crosses the player's x-line;
 * on other levels it sweeps on the sine LUT toward the player's altitude and
 * fires on hit_flag. Death (hp==0) -> normal death; di<0 (contact) or di>300 -> despawn. */
void ent_boss_strafer(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2f] == 0x00) { ent_die(e); return; }
    if (di < 0 || di > 300) {
        if (di < 0 && ent_hit_player(e)) G_PLAYER_HIT = 1;
        G_ENEMY_COUNT -= 1; e->type_state = 0xFF;
        return;
    }
    *(u16 *)(p + 0x11) = GW(0xF7F0, u16);                     /* vz = d600 */
    *(u16 *)(p + 0x23) = (u16)((*(i16 *)(p + 0x23) + 5) & 0xff);   /* timer += 5 */
    if (GW(0x27C7, i16) == 4) {                              /* level 5: strafing run */
        if (G_ENEMY_COUNT == 1) *(i16 *)(p + 0x11) += 1;
        *(u16 *)(p + 0x1f) = 0x1de1;                          /* anim seg (firing pose) */
        *(u16 *)(p + 0x1d) = 0x1258;                          /* anim off */
        *(i16 *)(p + 3) -= 1;                                 /* di-- */
        i16 s = ((const i16 *)((u8 *)&G + 0x2DD5))[*(i16 *)(p + 0x23)];        /* sin[timer] */
        *(i16 *)(p + 7) = (i16)(GW(0xF7F2, i16) + (i16)(s - (i16)(s >> 2)));   /* x = d602 + sin*3/4 */
        i16 c = ((const i16 *)((u8 *)&G + 0x2FD5))[*(i16 *)(p + 0x23)];        /* cos[timer] */
        c = (i16)(c - (i16)(c >> 2));
        if (c < 0) c = (i16)(-c);
        *(i16 *)(p + 5) = (i16)(c + 0x28);                    /* screen_y = |cos*3/4| + 0x28 */
        if (di < 0xb4) {
            i16 dx = (i16)(GW(0xDD84, i16) * 2 - *(i16 *)(p + 7));
            if (dx < 0x19 && dx > -0x19) {                    /* over the player: FIRE */
                *(i16 *)(p + 0x11) += 6;
                { u16 zl=*(u16*)(p+0x15); *(u16*)(p+0x15)=(u16)(zl-1); *(u16*)(p+0x17)-=(u16)(zl==0); }
                *(u16 *)(p + 0x27) = 1;
                ent_fire_shot(e);
                { u16 zl=*(u16*)(p+0x15); *(u16*)(p+0x15)=(u16)(zl+1); *(u16*)(p+0x17)+=(u16)(zl>0xfffe); }
                *(i16 *)(p + 0x11) -= 6;
            }
        }
    } else {                                                 /* other levels: sweep + track */
        if (*(i16 *)(p + 0x21) == 0) {
            *(i16 *)(p + 0x21) = 1;
            i16 x = *(i16 *)(p + 7);
            *(i16 *)(p + 0x23) = (i16)((x << 2) + x);         /* timer = x_accum*5 */
        }
        *(i16 *)(p + 7) = (i16)(((const i16 *)((u8 *)&G + 0x2DD5))[*(i16 *)(p + 0x23)] >> 1);
        *(i16 *)(p + 3) += (i16)(((const i16 *)((u8 *)&G + 0x2FD5))[*(i16 *)(p + 0x23)] >> 4);
        i16 targY = (i16)(0x87 - GW(0xDD86, i16));
        if (targY > 100) targY = 100;
        *(i16 *)(p + 0x13) = (i16)((targY - *(i16 *)(p + 5)) >> 1);
        if (p[0x30] != 0) {                                   /* hit this frame: FIRE */
            p[0x30] = 0;
            *(i16 *)(p + 0x11) += 6;
            ent_fire_shot(e);
            *(i16 *)(p + 0x11) -= 6;
        }
    }
}

/* port 161c:1483 — ent_kamikaze (type 35). A 3-state suicide flyer (state @0x21):
 * 0 HOME onto the player crosshair; when locked on it DIVES (di=-1, sets the
 * cockpit-flash overlay ce95/bc77/bc7b that damages the player) into state 1; 1
 * STRIKE animation window (bumps the shape, then wraps around behind the player
 * to state 2); 2 RETREAT (despawns past di 300). aim_target<0 forces retreat.
 * Death (hp==0) while in state 2: drops a SHIELD pickup (type 2) — a quirk of the
 * original places it ALWAYS in pool slot 1 (proven by ndisasm: si=1 then si*0x33). */
void ent_kamikaze(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2f] == 0x00) {                                   /* destroyed */
        if (*(i16 *)(p + 0x21) == 2 && free_slot14()) {      /* state 2: drop a SHIELD */
            u8 *s = POOL + 0x33;                             /* ALWAYS slot 1 (original quirk) */
            memcpy(s, p, 0x33);
            s[0]    = 0x02;                                  /* type 2 -> pickup_shield */
            *(u16 *)(s + 0x1b) = 0x0000;
            *(u16 *)(s + 0x1f) = 0x0DE1;                     /* anim seg */
            *(u16 *)(s + 0x1d) = 0x12c0;                     /* anim off (pickup shape) */
            *(u16 *)(s + 0x05) = 0x0000;                     /* screen_y = 0 */
            *(u16 *)(s + 0x11) = 0x0000;                     /* vz = 0 */
            s[0x2e] = 0x00;                                  /* shape_kind = 0 */
            G_ENEMY_COUNT += 1;
        }
        ent_die(e);
        return;
    }
    if ((i16)GW(0xDC68, i16) < 0) *(u16 *)(p + 0x21) = 2;    /* aim_target<0 -> retreat */
    i16 st = *(i16 *)(p + 0x21);
    if (st == 0) {                                           /* HOME */
        i16 vx = (i16)((GW(0xDD84, i16) * 2 - *(i16 *)(p + 7)) >> 1);
        i16 vy = (i16)(((0xa0 - GW(0xDD86, i16)) - *(i16 *)(p + 5)) >> 1);
        *(i16 *)(p + 0x0f) = vx;
        *(i16 *)(p + 0x13) = vy;
        if (*(i16 *)(p + 3) < 1) {                           /* at the player line */
            if (vx == 0 && vy == 0) {                        /* locked on: DIVE */
                *(i16 *)(p + 0x11) = GW(0xE9CC, i16);
                *(u16 *)(p + 3) = 0xffff;
                GW(0xF085, u16) = *(u16 *)(p + 0x1b);        /* ce95 cockpit flash */
                GW(0xDE67, u16) = (u16)(GW(0xDD84, i16) + 0xa0);   /* bc77 */
                GW(0xDE6B, u16) = (u16)(GW(0xDD86, i16) - 8);      /* bc7b */
                *(u16 *)(p + 0x21) = 1;
            } else {
                *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) + 2);
                *(u16 *)(p + 3) = 0x0000;
            }
        } else {
            *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) - 3);
        }
    } else if (st == 1) {                                   /* STRIKE window */
        i16 t = *(i16 *)(p + 0x23);
        *(i16 *)(p + 0x23) = (i16)(t + 1);
        if (t > 0x1c) {
            if (*(i16 *)(p + 0x23) == 0x1e) {
                *(i16 *)(p + 0x1b) += 10;
                GW(0xDC6C, i16) -= 8;                        /* ba7c invuln_timer -= 8 */
                if ((i16)GW(0xDC6C, i16) < 0) GW(0xDC6C, i16) = 0;
            } else if (*(i16 *)(p + 0x23) > 0x32) {          /* wrap behind the player */
                *(u16 *)(p + 0x21) = 2;
                *(u16 *)(p + 3) = 0x0000;
                *(u16 *)(p + 0x15) = GW(0xEA3C, u16);        /* z_pos = camera */
                *(u16 *)(p + 0x17) = GW(0xEA3E, u16);
                *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) + 2);
                *(i16 *)(p + 5) = (i16)(0xa0 - GW(0xDD86, i16));
                *(i16 *)(p + 7) = (i16)(GW(0xDD84, i16) << 1);
                *(i16 *)(p + 1) = (i16)(*(i16 *)(p + 7) - GW(0xDD84, i16));
            }
        }
        if (*(i16 *)(p + 0x21) < 2) {                        /* still striking */
            *(i16 *)(p + 0x11) = GW(0xE9CC, i16);
            *(u16 *)(p + 3) = 0xffff;
            GW(0xF085, u16) = *(u16 *)(p + 0x1b);
            GW(0xDE67, u16) = (u16)(GW(0xDD84, i16) + 0xa0);
            GW(0xDE6B, u16) = (u16)(GW(0xDD86, i16) - 8);
        }
    } else if (st == 2) {                                   /* RETREAT */
        if (di < 0x12d) *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) + 3);
        else { G_ENEMY_COUNT -= 1; e->type_state = 0xFF; }
    }
}

/* port 161c:16a3 — ent_mine_layer (type 36): advances (vz = speed+1, or vz++ while
 * aim_target<0) and, in the near band (di<0xf0), drops a trailing mine every 8th
 * frame. Beyond di 300 it despawns; hp==0 -> normal death. */
void ent_mine_layer(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2f] == 0x00) { ent_die(e); return; }
    if (di < 0x12d) {
        if ((i16)GW(0xDC68, i16) < 0) *(i16 *)(p + 0x11) += 1;
        else *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) + 1);
        if (di < 0xf0) {
            *(i16 *)(p + 0x21) += 1;
            if ((*(u16 *)(p + 0x21) & 7) == 0) drop_mine(e);
        }
    } else {
        e->type_state = 0xFF; G_ENEMY_COUNT -= 1;
    }
}

/* port 161c:17ab — ent_side_gunner (aircraft escort, types 37 & 47). Lives only
 * while the carrier flag (ba7b) is set. State machine: 0 launch from one side,
 * 1 attack (V-bob between the top and screen_y>100, firing at each turn), 2
 * retreat (vz++, despawn past 300). aim_target<0 forces state 2. hp==0 -> death. */
void ent_side_gunner(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (G_AIRCRAFT_FLAG == 0) { e->type_state = 0xFF; G_ENEMY_COUNT -= 1; return; }
    if (p[0x2f] == 0x00) { ent_die(e); return; }
    if ((i16)GW(0xDC68, i16) < 0) *(u16 *)(p + 0x21) = 2;
    i16 st = *(i16 *)(p + 0x21);
    if (st == 0) {                                          /* launch */
        *(u16 *)(p + 7) = ((i16)GW(0xDD84, i16) < 0) ? 0x40 : 0xffc0;
        *(i16 *)(p + 1) = (i16)(*(i16 *)(p + 7) - GW(0xDD84, i16));
        *(u16 *)(p + 3) = ((i16)GW(0xDD86, i16) < 0x46) ? 0x20 : 0x60;
        *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) + 2);
        *(u16 *)(p + 0x13) = 4;
        *(i16 *)(p + 0x21) += 1;
    } else if (st == 1) {                                   /* attack V-bob */
        if (di < 0x40)      *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) + 1);
        else if (di < 0x41) *(i16 *)(p + 0x11) = GW(0xE9CC, i16);
        else                *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) - 1);
        if (*(i16 *)(p + 0x13) == 4) {
            if (*(i16 *)(p + 5) > 100) { *(u16 *)(p + 0x13) = 0xfffc; ent_fire_shot(e); }
        } else if (*(i16 *)(p + 5) < 0) {
            *(u16 *)(p + 5) = 0; *(u16 *)(p + 0x13) = 4; ent_fire_shot(e);
        }
    } else if (st == 2) {                                   /* retreat */
        *(i16 *)(p + 0x11) += 1;
        if (di > 300) { e->type_state = 0xFF; G_ENEMY_COUNT -= 1; }
    }
}

/* port 161c:18f4 — ent_bomber (type 48): dives from above (vz = speed-7, x tracks
 * 2*scroll_x) on a screen_y ramp keyed to di; fires a single bomb once it enters
 * the near band (di<0x70). hp==0 -> death; di<0 -> contact + despawn. */
void ent_bomber(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2f] == 0x00) { ent_die(e); return; }
    if (di < 0) { ent_pass_player(e); return; }
    *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) - 7);
    *(i16 *)(p + 7) = (i16)(GW(0xDD84, i16) << 1);
    if (di - 0x70 < 0) {
        *(i16 *)(p + 5) = (i16)(0x32 - (di - 0x70));
        if (*(i16 *)(p + 0x21) == 0) { *(u16 *)(p + 0x21) = 1; ent_fire_shot(e); }
    } else {
        *(i16 *)(p + 5) = (i16)(di - 0x3e);
    }
}

/* port 161c:198d — ent_mine_dropper (type 49): like the bomber (vz = speed-5) but
 * drops a mine once instead of firing when it enters the near band (di<0x70). */
void ent_mine_dropper(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2f] == 0x00) { ent_die(e); return; }
    if (di < 0) { ent_pass_player(e); return; }
    *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) - 5);
    *(i16 *)(p + 7) = (i16)(GW(0xDD84, i16) << 1);
    if (di - 0x70 < 0) {
        *(i16 *)(p + 5) = (i16)(0x32 - (di - 0x70));
        if (*(i16 *)(p + 0x21) == 0) { *(u16 *)(p + 0x21) = 1; drop_mine(e); }
    } else {
        *(i16 *)(p + 5) = (i16)(di - 0x3e);
    }
}

/* port 161c:1c14 — ent_descender (type 40): eases down to the player's altitude
 * (vz toward speed, vx toward 2*scroll_x, vy = remaining altitude/2) while
 * di<0xc9; once level or beyond that, it settles (vx=0, gravity vy++, park at
 * screen_y 0). hp==0 -> death; di<0 -> contact + despawn. */
void ent_descender(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2f] == 0x00) { ent_die(e); return; }
    if (di < 0) { ent_pass_player(e); return; }
    int settle = 0;
    if (di < 0xc9) {
        i16 iv = (i16)(((0x87 - GW(0xDD86, i16)) - *(i16 *)(p + 5)) >> 1);
        if (iv < 1) settle = 1;
        else {
            if (*(i16 *)(p + 0x11) < GW(0xE9CC, i16)) *(i16 *)(p + 0x11) += 1;
            else                                      *(i16 *)(p + 0x11) -= 1;
            *(i16 *)(p + 0x0f) = (i16)((GW(0xDD84, i16) * 2 - *(i16 *)(p + 7)) >> 1);
            *(i16 *)(p + 0x13) = iv;
        }
    } else settle = 1;
    if (settle) {
        *(u16 *)(p + 0x0f) = 0;
        *(i16 *)(p + 0x13) += 1;
        if (*(i16 *)(p + 5) < 1) { *(u16 *)(p + 5) = 0; *(u16 *)(p + 0x13) = 0; *(u16 *)(p + 0x11) = 0; }
    }
}

/* port 161c:1ce7 — ent_formation_leader (type 41): publishes its own slot pointer
 * into d604/d606 so followers can trail it, tracks the player laterally (vx =
 * 2*scroll_x - x_accum) and vertically (vy toward 0x91-horizon), and slows as the
 * wave thins (vz-=2 while last enemy). aim_target<0 or last-one on contact despawn. */
void ent_formation_leader(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    GW(0xF7F4, u16) = (u16)((u8 *)p - (u8 *)&G);           /* d604 = leader slot offset */
    GW(0xF7F6, u16) = 0x0000;                              /* d606 (seg; single image) */
    if (p[0x2f] == 0x00) { ent_die(e); return; }
    if (di < 0) {
        if (G_ENEMY_COUNT == 1 || (i16)GW(0xDC68, i16) < 0) {
            G_ENEMY_COUNT -= 1; e->type_state = 0xFF; return;
        }
        *(u16 *)(p + 3) = 0;
        *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) + 1);
    } else if (di < 1) {
        *(i16 *)(p + 0x11) = GW(0xE9CC, i16);
    } else {
        *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) - 2);
    }
    if (G_ENEMY_COUNT == 1) *(i16 *)(p + 0x11) -= 2;
    *(i16 *)(p + 0x0f) = (i16)(GW(0xDD84, i16) * 2 - *(i16 *)(p + 7));
    *(i16 *)(p + 0x13) = (i16)((0x91 - GW(0xDD86, i16)) - *(i16 *)(p + 5));
}

/* port 161c:1d9f — ent_formation_follower (type 42): trails the leader recorded in
 * d604. If the leader has reached the front (its di==0) it snaps to the leader's
 * x_accum and screen_y-10 and races in (vz = speed-8); otherwise it holds at the
 * scroll speed. Last enemy: vz = speed-8. hp==0 -> death; di<0 -> contact + despawn. */
void ent_formation_follower(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2f] == 0x00) { ent_die(e); return; }
    if (di < 0) { ent_pass_player(e); return; }
    if (G_ENEMY_COUNT == 1) {
        *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) - 8);
    } else {
        u8 *lead = (u8 *)&G + GW(0xF7F4, u16);             /* d604 leader slot */
        if (*(i16 *)(lead + 3) == 0) {
            *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) - 8);
            *(u16 *)(p + 7) = *(u16 *)(lead + 7);
            *(i16 *)(p + 5) = (i16)(*(i16 *)(lead + 5) - 10);
        } else {
            *(i16 *)(p + 0x11) = GW(0xE9CC, i16);
        }
    }
}

/* port 161c:1ea6 — pickup_weapon (type 68): a hopping WEAPON crate active while
 * flag ba7a is set; airborne it bobs (vz 0x14/0x1c by game_mode, gravity on vy,
 * decaying hp as its lifetime); collected on contact (wide box) it raises the
 * weapon level (aim_target, cap 4) and shows the pickup banner (cd66=0x14). hp==0
 * -> silent morph. Far (di>300) or flag clear -> despawn. */
void pickup_weapon(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (GW(0xDC6A, u8) == 0 || di > 300) {                 /* ba7a inactive / too far */
        G_ENEMY_COUNT -= 1; e->type_state = 0xFF;
    } else if (p[0x2f] == 0x00) {                          /* shot */
        ent_morph_explosion(e);
        GW(0xDC6A, u8) = 0;
    } else if (di < 0) {                                   /* collected */
        if (ent_hit_player_wide(e) && (i16)GW(0xDC68, i16) < 4) {
            GW(0xDC68, i16) += 1;                          /* weapon level++ */
            GW(0xEF56, u16) = 0x14;                        /* cd66 pickup banner */
        }
        GW(0xDC6A, u8) = 0;
        G_ENEMY_COUNT -= 1; e->type_state = 0xFF;
    } else {                                               /* airborne bob */
        *(u16 *)(p + 0x11) = (GW(0xF6AF, u16) == 0) ? 0x14 : 0x1c;
        if (*(i16 *)(p + 5) < 1) {
            p[0x2f] -= 1;
            *(i16 *)(p + 0x13) = (i16)(-*(i16 *)(p + 0x13));
            *(i16 *)(p + 5) += *(i16 *)(p + 0x13);
        }
        *(i16 *)(p + 0x13) -= 1;
    }
}

/* port 161c:1f75 — ent_strafer_layer (type 50): rides in (vz = speed+1, or speed-1
 * once past di 0x3c with invuln_timer set), swaying on the sine LUT, and drops a
 * mine every 8th frame. Beyond di 300 -> despawn; hp==0 -> death. */
void ent_strafer_layer(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2f] == 0x00) { ent_die(e); return; }
    if (di < 0x12d) {
        if (di < 0x3c || GW(0xDC6C, u16) == 0) {
            *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) + 1);
        } else {
            if (di > 0x3c) *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) - 1);
            *(u16 *)(p + 0x23) = (u16)((*(i16 *)(p + 0x23) + 5) & 0xff);
            *(i16 *)(p + 7) = (i16)(((const i16 *)((u8 *)&G + 0x2DD5))[*(i16 *)(p + 0x23)] >> 1);
            *(i16 *)(p + 0x21) += 1;
            if ((*(u16 *)(p + 0x21) & 7) == 0) drop_mine(e);
        }
    } else {
        e->type_state = 0xFF; G_ENEMY_COUNT -= 1;
    }
}

/* port 161c:20ad — ent_boss_sweeper (type 51). On LEVEL 5 (index 4): vz = speed-3,
 * fires on hit_flag, and rides a TRIANGLE wave of di across screen_y (0..0x80).
 * Other levels: vz = speed-2 and screen_y follows the cosine LUT (>>2 + 0x3c).
 * hp==0 -> death; di<0 -> contact + despawn. (di is treated unsigned for the wave.) */
void ent_boss_sweeper(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2f] == 0x00) { ent_die(e); return; }
    if (di < 0) { ent_pass_player(e); return; }
    if (GW(0x27C7, i16) == 4) {
        *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) - 3);
        if (p[0x30] != 0) { p[0x30] = 0; ent_fire_shot(e); }
        i16 y = (i16)((di & 0x3f) * 2);
        if (di & 0x40) y = (i16)((di & 0x3f) * -2 + 0x80);
        *(i16 *)(p + 5) = y;
    } else {
        *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) - 2);
        *(u16 *)(p + 0x21) = (u16)((*(i16 *)(p + 0x21) + 7) & 0xff);
        *(i16 *)(p + 5) = (i16)((((const i16 *)((u8 *)&G + 0x2FD5))[*(i16 *)(p + 0x21)] >> 2) + 0x3c);
    }
}

/* port 161c:2175 — ent_sine_flyer (type 52): weaves on both wave LUTs (x_accum =
 * sin>>1, screen_y = cos>>2 + 0x3c) at vz = speed-2. hp==0 -> death; di<0 -> despawn. */
void ent_sine_flyer(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2f] == 0x00) { ent_die(e); return; }
    if (di < 0) { ent_pass_player(e); return; }
    *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) - 2);
    *(u16 *)(p + 0x21) = (u16)((*(i16 *)(p + 0x21) + 7) & 0xff);
    i16 ph = *(i16 *)(p + 0x21);
    *(i16 *)(p + 5) = (i16)((((const i16 *)((u8 *)&G + 0x2FD5))[ph] >> 2) + 0x3c);
    *(i16 *)(p + 7) = (i16)(((const i16 *)((u8 *)&G + 0x2DD5))[ph] >> 1);
}

/* port 161c:2203 — ent_sine_gunner (type 53): like the sine flyer but the wobble
 * rides vz (cos>>7) and it fires every 32nd frame in the near band on LEVEL 5. */
void ent_sine_gunner(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2f] == 0x00) { ent_die(e); return; }
    if (di < 0) { ent_pass_player(e); return; }
    *(u16 *)(p + 0x21) = (u16)((*(i16 *)(p + 0x21) + 5) & 0xff);
    i16 ph = *(i16 *)(p + 0x21);
    *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) - 2);
    *(i16 *)(p + 0x11) += (i16)(((const i16 *)((u8 *)&G + 0x2FD5))[ph] >> 7);
    *(i16 *)(p + 7) = (i16)(((const i16 *)((u8 *)&G + 0x2DD5))[ph] >> 1);
    if (GW(0x27C7, i16) == 4 && di < 0x96) {
        u16 t = *(u16 *)(p + 0x23);
        *(u16 *)(p + 0x23) = (u16)(t + 1);
        if ((t & 0x1f) == 0) ent_fire_shot(e);
    }
}

/* port 161c:2408 — enemy_update_enqueue_top (type 62): flies UP (vy--) until it
 * reaches the top of the screen (screen_y<0), where it hatches a new enemy from
 * proto @0x2183 at its own position and then silently morphs to an explosion.
 * di<0 -> despawn (no contact test). */
void enemy_update_enqueue_top(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (di < 0) {
        e->type_state = 0xFF; G_ENEMY_COUNT -= 1;
    } else if (*(i16 *)(p + 5) < 0) {
        *(u16 *)(p + 5) = 0;
        u8 *s = free_slot14();
        if (s) {
            memcpy(s, (u8 *)&G + 0x2183, 0x33);            /* hatch proto @0x2183 */
            *(u16 *)(s + 7)    = *(u16 *)(p + 7);
            *(u16 *)(s + 1)    = *(u16 *)(p + 1);
            *(u16 *)(s + 3)    = *(u16 *)(p + 3);
            *(u16 *)(s + 0x17) = *(u16 *)(p + 0x17);
            *(u16 *)(s + 0x15) = *(u16 *)(p + 0x15);
            G_ENEMY_COUNT += 1;
        }
        ent_morph_explosion(e);
    } else {
        *(i16 *)(p + 0x13) -= 1;
    }
}

/* port 161c:24d8 — enemy_update_offscreen_kill (type 65): inert until it slips past
 * the player (di<0), then contact-test + despawn. */
void enemy_update_offscreen_kill(Entity *e, int di)
{
    if (di < 0) ent_pass_player(e);
}

/* port 161c:29fa — enemy_update_home_player (type 66): closes in (vz = speed-7)
 * while continuously steering toward the player crosshair (vx only inside di 100,
 * vy always). hp==0 -> death; di<0 -> contact + despawn. */
void enemy_update_home_player(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2f] == 0x00) { ent_die(e); return; }
    if (di < 0) { ent_pass_player(e); return; }
    *(u16 *)(p + 0x1b) = 0;
    *(i16 *)(p + 0x11) = (i16)(GW(0xE9CC, i16) - 7);
    i16 vx = (di < 100) ? (i16)((GW(0xDD84, i16) * 2 - *(i16 *)(p + 7)) >> 1) : 0;
    *(i16 *)(p + 0x0f) = vx;
    *(i16 *)(p + 0x13) = (i16)(((0x87 - GW(0xDD86, i16)) - *(i16 *)(p + 5)) >> 1);
}

/* port 161c:2a86 — enemy_update_bounce (type 69): a grounded hopper. state_phase 0
 * settles vz toward 0; otherwise it arcs — on hitting the floor (screen_y<1) it
 * halves vx, reflects vy (=-(vy+4)), steps di/z_pos back one, and re-launches until
 * vy decays. hp==0 -> silent morph; di<0 -> contact + despawn. */
void enemy_update_bounce(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (p[0x2f] == 0x00) { ent_morph_explosion(e); return; }
    if (di < 0) { ent_pass_player(e); return; }
    if (*(i16 *)(p + 0x21) == 0) {
        if (*(i16 *)(p + 0x11) != 0) *(i16 *)(p + 0x11) -= 1;
    } else if (*(i16 *)(p + 5) < 1) {
        *(i16 *)(p + 0x0f) = (i16)(*(i16 *)(p + 0x0f) >> 1);
        if (*(i16 *)(p + 0x0f) == -1) *(u16 *)(p + 0x0f) = 0;
        *(i16 *)(p + 0x13) = (i16)(-(*(i16 *)(p + 0x13) + 4));
        *(i16 *)(p + 3) -= 1;
        { u16 zl = *(u16 *)(p + 0x15); *(u16 *)(p + 0x15) = (u16)(zl - 1); *(u16 *)(p + 0x17) -= (u16)(zl == 0); }
        if (*(i16 *)(p + 0x13) < 1) {
            *(u16 *)(p + 0x21) = 0; *(u16 *)(p + 0x13) = 0; *(u16 *)(p + 5) = 0;
        } else {
            *(i16 *)(p + 5) += *(i16 *)(p + 0x13);
        }
    } else {
        *(i16 *)(p + 0x13) -= 1;
    }
}

/* port 161c:2987 — enemy_update_charge_trigger (type 78): a stationary trigger. While
 * its timer @0x23 is 0 it holds shape 6 in the near band and, after 0x1f frames close,
 * arms the global flag d4d8 and advances (timer->1). In the armed state it mirrors
 * d4d8 into its shape base and stops. (No death / no contact — it is a trigger only.) */
void enemy_update_charge_trigger(Entity *e, int di)
{
    u8 *p = (u8 *)e;
    if (*(i16 *)(p + 0x23) == 0) {
        p[0x2e] = 0;
        *(u16 *)(p + 0x11) = GW(0xE9CC, u16);
        if (di < 0x1f) {
            if (di < 0x19) *(i16 *)(p + 0x11) += 1;
            p[0x2e] = 6;
            i16 ph = *(i16 *)(p + 0x21);
            *(i16 *)(p + 0x21) = (i16)(ph + 1);
            if (ph > 0x1e) { *(i16 *)(p + 0x23) += 1; GW(0xF6C8, u16) = 1; }
        } else {
            *(i16 *)(p + 0x11) -= 2;
        }
    } else if (*(i16 *)(p + 0x23) == 1) {
        *(u16 *)(p + 0x1b) = GW(0xF6C8, u16);
        *(u16 *)(p + 0x11) = 0;
    }
}

/* port 161c:22e2 / 23e0 / 23f4 / 24c4 / 2b4f — thin thunks (types 57/59/60/63/64/79)
 * that just run ent_basic (161c:06be). */
void enemy_ai_default_thunk2(Entity *e, int di) { ent_basic(e, di); }
void enemy_ai_default_thunk3(Entity *e, int di) { ent_basic(e, di); }
void enemy_ai_default_thunk4(Entity *e, int di) { ent_basic(e, di); }
void enemy_ai_default_thunk5(Entity *e, int di) { ent_basic(e, di); }
void enemy_ai_default_thunk6(Entity *e, int di) { ent_basic(e, di); }
