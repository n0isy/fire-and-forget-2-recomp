/* entity_faithful.c - FAITHFUL line-by-line port of the entity engine.
 *
 * Replaces the approximated game/entity.c. Provides STRONG definitions of:
 *   - void entities_update_all(void)      <- entities_update  fn0DAE_0446 @15ae:0446
 *   - int  spawn_enemy_rec(const u8 *rec) <- spawn_enemy      fn0BA8_038B @13a8:038b
 *
 * Source of truth (transliterated, every constant kept):
 *   raw/reko/FF2EGA_0DAE.c   fn0DAE_0446   (entities_update)
 *   raw/reko/FF2EGA_0BA8.c   fn0BA8_038B   (spawn_enemy)
 * cross-checked against raw/ghidra/decompiled.c FUN_15ae_0446 / FUN_13a8_038b.
 *
 * DGROUP access via gmem.h (GW/GB/GI/GD/GPTR). Ghidra DAT_2000_xxxx live at
 * DGROUP offset xxxx+0x21F0 (pool 0xc3dc -> 0xE5CC); DAT_1de1_xxxx at offset xxxx.
 * Confirmed offsets used here:
 *   pool       DAT_2000_c3dc -> 0xE5CC  (entity_pool(); 20 slots * 0x33)
 *   camera     DAT_2000_c84c/c84e -> 0xEA3C/0xEA3E  (scroll_accum, 32-bit, tEA3C)
 *   scroll arg DAT_2000_bb94 -> 0xDD84  (wDD84, wArg04/param_1 of the original)
 *   persp LUT  DAT_1de1_2cb1 -> 0x2CB1  (a2CB1: i16 scale over 16 depth levels)
 *   templates  type*0x33 + 0x1490       (a1490: slot prototypes, stride 0x33)
 *   live count DAT_2000_d27a -> 0xF46A  (wF46A: incremented by spawn)
 *   wave gate  DAT_2000_ba7c/ba7e -> 0xDC6C/0xDC6E (wDC6C invuln_timer / bDC6E)
 *
 * Behavior dispatch: the original calls the far-pointer table a3A63[type](slot,di);
 * natively this is ff_behavior_for(type) -> f(slot, di). The per-slot lifecycle
 * (hp==0 death, di<0 contact, di>300 despawn, else AI) lives INSIDE each handler
 * (behaviors.c), exactly as in the ROM - it is NOT centralized here.
 */
#include "ff_game.h"
#include "gmem.h"
#include <string.h>

/* ====================================================================== */
/* spawn_enemy  -  fn0BA8_038B @13a8:038b                                  */
/* ---------------------------------------------------------------------- */
/* Takes a 5-byte WAVE record [type, depth, y, x, vz]. Finds a free slot,
 * memcpy's the type's 0x33-byte prototype, then overlays the record fields
 * EXACTLY as the original. Returns the byte advance (5), mirroring the
 * original's `return pcVar5 + 5` (pointer to the next record).               */
int spawn_enemy_rec(const unsigned char *rec)
{
    /* di_16 = scan index; local_6 = pool base (&DAT_2000_c3dc @ 0xE5CC). */
    int  di_16 = 0;
    i8  *local_6 = (i8 *)entity_pool();

    /* find first free slot (b0000 < 0) or run off the end of the 20 slots. */
    while (1) {
        if (*local_6 < 0 || di_16 >= 0x14)
            break;
        ++di_16;
        local_6 += 0x33;
    }

    if (di_16 < 0x14) {
        Entity *e = (Entity *)local_6;
        int     type = (int)(i8)rec[0];          /* (char)*param_1 */

        /* memcpy(slot, &template[type*0x33 + 0x1490], 0x33). NOTE: slot[0]
         * keeps the prototype's type byte - NOT rec[0] - so e.g. type 32
         * (MOUCHARD) re-types itself to 56 on spawn. */
        memcpy(local_6,
               GPTR(DG_ENEMY_PROTOS + type * ENEMY_PROTO_STRIDE),
               ENTITY_SLOT_SIZE);

        /* z_pos(32) = camera(32) + (u8)rec[1]   (low @0x15, high @0x17). */
        {
            u32 cam = ((u32)GW(0xEA3E) << 16) | (u32)GW(0xEA3C);
            e->z_pos = cam + (u32)(u8)rec[1];
        }

        /* screen_y(+0x05) = (signed char)rec[2]. */
        e->screen_y = (u16)(i16)(i8)rec[2];

        /* x_accum(+0x07) = (signed char)rec[3] * 0x19 >> 4. */
        e->x_accum = (u16)(i16)(((int)(i8)rec[3] * 0x19) >> 4);

        /* vz(+0x11) = (signed char)rec[4]; then vz += rec[4] >> 1. */
        {
            int cVar3 = (int)(i8)rec[4];
            int iVar6  = cVar3 >> 1;             /* arithmetic shift */
            e->vz = (i16)cVar3;
            e->vz = (i16)(e->vz + iVar6);

            e->substate = 0x01;                  /* slot[0x2D] = 1 */
            GI(0xF46A) += 1;                     /* ++wF46A (live count) */

            /* if rec[1] == 0 (z_pos lands exactly on the camera): nudge once
             * more and back the depth off by one velocity step. */
            if ((u16)(e->z_pos >> 16) == GW(0xEA3E) &&
                (u16)e->z_pos        == GW(0xEA3C)) {
                e->vz = (i16)(e->vz + iVar6);
                e->z_pos -= (u32)(i32)(i16)e->vz;   /* 32-bit subtract */
            }
        }
    }

    return 0x05;                                  /* return pcVar5 + 5 */
}

/* ====================================================================== */
/* entities_update  -  fn0DAE_0446 @15ae:0446                              */
/* ---------------------------------------------------------------------- */
/* One pass over the 20-slot pool: integrate depth, project screen_x/screen_y
 * (perspective via a2CB1 when proj_mode != 0, else screen-anchored), then
 * dispatch the per-type handler a3A63[type](slot, di).                       */
void entities_update_all(void)
{
    /* prologue: original re-ticks the current wave script here -
     *   if (wDC6C != 0 && bDC6E == 0) fn0BA8_13FD(0);
     * fn0BA8_13FD is the wave-script VM (ported in game/vm.c; run_level_frame
     * ticks it with this exact gate); not re-entered here to avoid
     * double-advancing it. The exact
     * gate condition is reproduced for fidelity. */
    if (GW(0xDC6C) != 0 && GB(0xDC6E) == 0) {
        /* fn0BA8_13FD(ds, 0);  -- wave VM, see game/vm.c */
    }

    /* wArg04 / param_1 = wDD84 (the horizontal projection / scroll argument);
     * camera depth low word = _DAT_2000_c84c (scroll_accum @0xEA3C).
     * a2CB1: i16 perspective scale over 16 depth levels @ DGROUP 0x2CB1. */
    const int  wArg04 = (int)GI(0xDD84);
    const i16 *a2CB1  = (const i16 *)GPTR(DG_PERSP_TBL);

    i8 *local_6 = (i8 *)entity_pool();            /* &DAT_2000_c3dc */

    for (int local_e = 0; local_e < 0x14; ++local_e, local_6 += 0x33) {
        Entity *e = (Entity *)local_6;

        /* free slot (b0000 < 0) -> skip. g_slot_drawn models the original's
         * INTERLEAVED AI+projection: a slot that is free when the loop reaches
         * it is NOT projected/enqueued this frame — even if a LATER slot's AI
         * spawns a copy into it during this same pass (e.g. ent_smart_bomb's
         * secondary-explosion copy into a lower free slot). The port renders
         * after the whole update, so without the mask it drew such copies one
         * frame early (a phantom overlay with a stale 0x135C snapshot — the
         * f2000 boss-fight pixel diff). */
        g_slot_drawn[local_e] = 0;
        if (*local_6 < 0)
            continue;
        g_slot_drawn[local_e] = 1;

        /* --- depth integration: z_pos(32) += (sign-extended) vz(16). --- */
        e->z_pos += (u32)(i32)(i16)e->vz;

        /* --- on-screen distance: di = (i16)lowword(z_pos) - camera_low. --- */
        int iVar8 = (int)(i16)((u16)e->z_pos - GW(0xEA3C));
        e->di = (i16)iVar8;

        /* --- horizontal projection -> screen_x (w0001). --- */
        if (e->proj_mode == 0) {
            /* screen-anchored (b002C == 0): linear. */
            e->x_accum  = (u16)((i16)e->x_accum + e->vx);   /* w0007 += w000F */
            e->screen_x = (u16)((i16)e->x_accum - wArg04);  /* w0001 = w0007 - wArg04 */
        } else {
            /* perspective (b002C != 0): interpolate a2CB1 over 16 depth levels. */
            int iVar7, local_c;
            if (iVar8 < 0) {
                iVar7   = 0;
                local_c = 0;
            } else {
                local_c = (int)(GW(0xEA3C) & 0x0F) + iVar8;  /* (camera&0xf)+di */
                iVar7   = local_c >> 4;
                if (iVar7 > 0x0F)
                    iVar7 = 0x0F;
            }
            int iVar4 = (int)a2CB1[iVar7];                   /* s0 = persp[idx]   */
            iVar7     = (int)a2CB1[iVar7 + 1];               /* s1 = persp[idx+1] */
            e->x_accum  = (u16)((i16)e->x_accum + e->vx);    /* w0007 += w000F */
            e->screen_x = (u16)((i16)e->x_accum +
                                (((iVar7 - iVar4) * (local_c & 0x0F)) >> 4) + iVar4);
        }

        /* --- vertical: screen_y += vy  (w0005 += w0013). --- */
        e->screen_y = (u16)((i16)e->screen_y + e->vy);

        /* --- dispatch a3A63[type](slot, di); type = current slot[0]. --- */
        {
            behavior_fn f = ff_behavior_for((int)(i8)e->type_state);
            if (f)
                f(e, iVar8);
        }

        /* SNAPSHOT the shared explosion morph-threshold (wDAT 0x135C) as it stands
         * AFTER this slot's AI. The original fn0DAE_0446 projects each slot right
         * after its AI (fn0869_1726 reads 0x135C), so slot i's explosion sprite
         * uses the 0x135C its own ent_explosion_* just wrote (0x3c/0x50 or
         * 0x78/0xa0 by timer parity) — or the stale value from an earlier slot if
         * this slot's AI didn't touch it (e.g. a just-spawned burst). Our render
         * runs after ALL AIs, so it would otherwise see only the LAST value; the
         * per-slot snapshot lets render_entity reproduce the interleaved value. */
        g_slot_thresh[local_e] = GW(0x135C);

        /* original: if the slot is still active (b0000 >= 0) it enqueues the
         * shadow + sprite via fn0869_1726 / enqueue_enemy_sprite. Rendering is
         * handled separately by render_frame(), so nothing is enqueued here. */
    }
}
