/* vm.c — FAITHFUL port of the wave VM fn0BA8_13FD (0BA8:13FD / FUN_13a8_13fd),
 * the level-script dispatcher that spawns the pickup/enemy waves.
 *
 * Replaces the earlier approximated dispatcher: every opcode below is a direct
 * transliteration (cross-checked reko FF2EGA_0BA8.c @1146-1330 and ghidra).
 * One call = one dispatch, exactly one per game frame (run_level's gate
 * `wDC6C != 0 && bDC6E == 0`), plus the arming call fn0BA8_13FD(ds, 1) in the
 * run_level init (vm_arm).
 *
 * EVERY call first steps the PATH VM fn0BA8_175D (game/path_vm.c) — it walks
 * the same bytecode ahead of the wave VM and builds the runtime enemy shapes;
 * the GETCHAR opcode here synchronizes on its wF46C markers.
 *
 * Layout facts (verified against the level-0 script and the QEMU pool):
 *  - PUT operand = word offset of a burst record; count byte at 0xA0+rec,
 *    then COUNT 5-byte spawn records at 0xA0+rec+1 (spawns = count: the
 *    4-pickup record @0xA00 yields the reference's 4 live entities).
 *  - WAVE: [09][rec16][count8][delay8]; one spawn per deadline step.
 *  - wF480 (the enemy/track segment index) cycles: >4 -> 0 at burst start,
 *    += 4 (if < 4) when the burst clears — the road-curve segment jump
 *    (fn0869_154D) uses it, so track curvature is script-synchronized.
 */
#include "ff_game.h"
#include "gnames.h"

/* one wave-VM dispatch; arm != 0 = the fn0BA8_13FD(ds, 1) init call */
static void wave_step(int arm)
{
    if (arm) {
        /* PC = script dir a0094[t27C7] + 0xA0; reset the sync state */
        Gw_wave_pc = (u16)(wave_script_dir[Gw_stage] + 0xA0);
        Gw_wave_side = 0x00;                     /* side flag                */
        Gb_wave_phase = 0x00;                     /* PUT/WAVE phase           */
        Gw_wave_rearm = 0x01;                     /* re-arm flag              */
        Gw_w38A4 = 0x01;
    }

    path_vm_tick(arm, (int)Gw_wave_side);        /* fn0BA8_175D every call   */

    u16 pc = Gw_wave_pc;
    u8 op = wv_b(pc);
    if (op > 0x09) return;

    switch (op) {

    case 0x00:                                 /* CLEAR: rearm the sync    */
        Gw_wave_rearm = 0x01;
        Gw_wave_pc = (u16)(pc + 1);
        Gw_marker_idx = 0x00;                     /* marker read cursor       */
        break;

    case 0x01: {                               /* GETCHAR: wait on marker  */
        i16 di = G_vm_marker(Gw_marker_idx);      /* wF46C[wF7C4]             */
        i16 si = (di < 0) ? (i16)-di : di;
        if ((i16)(u8)wv_b(pc + 1) != si) {       /* wrong shape: resync      */
            path_vm_tick(1, 0);                /* fn0BA8_175D(ds, 1, 0)    */
            Gw_wave_side = 0x01;
            Gw_marker_idx = 0x00;
        } else if (di > 0) {                   /* shape ready: advance     */
            Gw_wave_pc = (u16)(pc + 2);
            Gw_marker_idx = (u16)(Gw_marker_idx + 1);
        } else {                               /* still building: décor    */
            decor_ring_vm_spawn();             /* fn0869_1946              */
            Gw_scale_budget = 0xC0;                 /* faster shape scaling     */
        }
        break;
    }

    case 0x02:                                 /* PUT: burst + wait-clear  */
        if (Gb_wave_phase == 0x00) {
            if (Gw_wave_rearm != 0x00) {
                Gw_wave_side ^= 0x01;            /* flip the build side      */
                Gw_wave_rearm = 0x00;
                Gw_scale_budget = 0x80;
            }
            if ((i16)Gw_track_seg > 0x04) Gw_track_seg = 0x00;
            u16 rec = wv_w(pc + 1);
            int count = (int)(i8)wv_b(0xA0 + rec);
            Gw_kill_ctr = (count < 3) ? 0xFF : (u16)count;
            const u8 *r = wv_ptr(0xA0 + rec + 1);
            for (int n = 0; n < count; ++n) {  /* while (count--) spawn    */
                spawn_enemy_rec(r);
                r += 5;
            }
            Gb_wave_phase = 0x01;
        }
        if (Gw_enemy_count == 0x00) {              /* burst cleared            */
            if ((i16)Gw_track_seg < 0x04) Gw_track_seg = (u16)(Gw_track_seg + 4);
            Gb_wave_phase = 0x00;
            Gw_wave_pc = (u16)(pc + 3);
        }
        break;

    case 0x03:                                 /* WHILE dist > cmp         */
        if ((i16)Gw_objective_hi <= 0 &&
            ((i16)Gw_objective_hi != 0 || (i16)(i8)wv_b(pc + 1) >= (i16)Gw_objective_lo))
            Gw_wave_pc = (u16)(pc + 4);
        else
            Gw_wave_pc = (u16)(wv_w(pc + 2) + 0xA0);
        break;

    case 0x04:                                 /* GOTO                     */
        Gw_wave_pc = (u16)(wv_w(pc + 1) + 0xA0);
        break;

    case 0x05:                                 /* SETDIST                  */
        Gw_objective_hi = 0x00;
        Gw_objective_lo = wv_w(pc + 1);
        Gw_wave_pc = (u16)(pc + 3);
        break;

    case 0x06:                                 /* GOSUB (1-deep)           */
        Gw_wave_ret = (u16)(pc + 3);            /* ptrF7C6 return offset    */
        Gw_wave_pc = (u16)(wv_w(pc + 1) + 0xA0);
        break;

    case 0x07:                                 /* RETURN                   */
        Gw_wave_pc = Gw_wave_ret;
        break;

    case 0x08:                                 /* BEQ dist == 0            */
        if ((Gw_objective_lo | Gw_objective_hi) == 0x00)
            Gw_wave_pc = (u16)(wv_w(pc + 1) + 0xA0);
        else
            Gw_wave_pc = (u16)(pc + 3);
        break;

    case 0x09:                                 /* WAVE: timed drip         */
        switch (Gb_wave_phase) {
        case 0x00:                             /* latch                    */
            if (Gw_wave_rearm != 0x00) {
                Gw_wave_side ^= 0x01;
                Gw_wave_rearm = 0x00;
            }
            Gb_wave_count = wv_b(pc + 3);         /* spawn count              */
            if ((i16)(i8)Gb_wave_count > 0x02) Gw_kill_ctr = (u16)(u8)Gb_wave_count;
            else                             Gw_kill_ctr = 0xFF;
            Gw_wave_deadline_hi = Gw_dist_hi;           /* deadline = accum32       */
            Gw_wave_deadline_lo = Gw_dist_lo;
            Gb_wave_phase = 0x01;
            break;
        case 0x01: {                           /* spawn one when due       */
            i16 dh = (i16)Gw_wave_deadline_hi;
            if (dh <= (i16)Gw_dist_hi &&
                (dh != (i16)Gw_dist_hi || Gw_wave_deadline_lo <= Gw_dist_lo)) {
                u32 next = (((u32)Gw_dist_hi << 16) | Gw_dist_lo) + wv_b(pc + 4);
                Gw_wave_deadline_hi = (u16)(next >> 16);
                Gw_wave_deadline_lo = (u16)next;
                spawn_enemy_rec(wv_ptr(wv_w(pc + 1) + 0xA1));  /* skip count byte */
                Gb_wave_count = (u8)(Gb_wave_count - 1);
                if (Gb_wave_count == 0x00) Gb_wave_phase = (u8)(Gb_wave_phase + 1);
            }
            break;
        }
        case 0x02:                             /* wait until cleared       */
            if (Gw_enemy_count == 0x00) {
                Gb_wave_phase = 0x00;
                Gw_wave_pc = (u16)(pc + 5);
            }
            break;
        }
        break;
    }
}

/* run_level init: fn0BA8_13FD(ds, 1) — arm + one dispatch (SETDIST). */
void vm_arm(void)
{
    wave_step(1);
}

/* per frame: fn0BA8_13FD(ds, 0). */
void vm_step(void)
{
    wave_step(0);
}
