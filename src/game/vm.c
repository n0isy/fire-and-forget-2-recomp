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
#include "gmem.h"

/* one wave-VM dispatch; arm != 0 = the fn0BA8_13FD(ds, 1) init call */
static void wave_step(int arm)
{
    if (arm) {
        /* PC = script dir a0094[t27C7] + 0xA0; reset the sync state */
        GW(0xF6BA) = (u16)(GW(0x0094 + GW(0x27C7) * 2) + 0xA0);
        GW(0xF7C0) = 0x00;                     /* side flag                */
        GB(0x38A6) = 0x00;                     /* PUT/WAVE phase           */
        GW(0xF7C2) = 0x01;                     /* re-arm flag              */
        GW(0x38A4) = 0x01;
    }

    path_vm_tick(arm, (int)GW(0xF7C0));        /* fn0BA8_175D every call   */

    u16 pc = GW(0xF6BA);
    u8 op = GB(pc);
    if (op > 0x09) return;

    switch (op) {

    case 0x00:                                 /* CLEAR: rearm the sync    */
        GW(0xF7C2) = 0x01;
        GW(0xF6BA) = (u16)(pc + 1);
        GW(0xF7C4) = 0x00;                     /* marker read cursor       */
        break;

    case 0x01: {                               /* GETCHAR: wait on marker  */
        i16 di = GI(0xF46C + GW(0xF7C4) * 2);  /* wF46C[wF7C4]             */
        i16 si = (di < 0) ? (i16)-di : di;
        if ((i16)(u8)GB(pc + 1) != si) {       /* wrong shape: resync      */
            path_vm_tick(1, 0);                /* fn0BA8_175D(ds, 1, 0)    */
            GW(0xF7C0) = 0x01;
            GW(0xF7C4) = 0x00;
        } else if (di > 0) {                   /* shape ready: advance     */
            GW(0xF6BA) = (u16)(pc + 2);
            GW(0xF7C4) = (u16)(GW(0xF7C4) + 1);
        } else {                               /* still building: décor    */
            decor_ring_vm_spawn();             /* fn0869_1946              */
            GW(0x2505) = 0xC0;                 /* faster shape scaling     */
        }
        break;
    }

    case 0x02:                                 /* PUT: burst + wait-clear  */
        if (GB(0x38A6) == 0x00) {
            if (GW(0xF7C2) != 0x00) {
                GW(0xF7C0) ^= 0x01;            /* flip the build side      */
                GW(0xF7C2) = 0x00;
                GW(0x2505) = 0x80;
            }
            if ((i16)GW(0xF480) > 0x04) GW(0xF480) = 0x00;
            u16 rec = GW(pc + 1);
            int count = (int)(i8)GB(0xA0 + rec);
            GW(0xF6B8) = (count < 3) ? 0xFF : (u16)count;
            const u8 *r = GPTR(0xA0 + rec + 1);
            for (int n = 0; n < count; ++n) {  /* while (count--) spawn    */
                spawn_enemy_rec(r);
                r += 5;
            }
            GB(0x38A6) = 0x01;
        }
        if (GW(0xF46A) == 0x00) {              /* burst cleared            */
            if ((i16)GW(0xF480) < 0x04) GW(0xF480) = (u16)(GW(0xF480) + 4);
            GB(0x38A6) = 0x00;
            GW(0xF6BA) = (u16)(pc + 3);
        }
        break;

    case 0x03:                                 /* WHILE dist > cmp         */
        if ((i16)GW(0xF6C6) <= 0 &&
            ((i16)GW(0xF6C6) != 0 || (i16)(i8)GB(pc + 1) >= (i16)GW(0xF6C4)))
            GW(0xF6BA) = (u16)(pc + 4);
        else
            GW(0xF6BA) = (u16)(GW(pc + 2) + 0xA0);
        break;

    case 0x04:                                 /* GOTO                     */
        GW(0xF6BA) = (u16)(GW(pc + 1) + 0xA0);
        break;

    case 0x05:                                 /* SETDIST                  */
        GW(0xF6C6) = 0x00;
        GW(0xF6C4) = GW(pc + 1);
        GW(0xF6BA) = (u16)(pc + 3);
        break;

    case 0x06:                                 /* GOSUB (1-deep)           */
        GW(0xF7C6) = (u16)(pc + 3);            /* ptrF7C6 return offset    */
        GW(0xF6BA) = (u16)(GW(pc + 1) + 0xA0);
        break;

    case 0x07:                                 /* RETURN                   */
        GW(0xF6BA) = GW(0xF7C6);
        break;

    case 0x08:                                 /* BEQ dist == 0            */
        if ((GW(0xF6C4) | GW(0xF6C6)) == 0x00)
            GW(0xF6BA) = (u16)(GW(pc + 1) + 0xA0);
        else
            GW(0xF6BA) = (u16)(pc + 3);
        break;

    case 0x09:                                 /* WAVE: timed drip         */
        switch (GB(0x38A6)) {
        case 0x00:                             /* latch                    */
            if (GW(0xF7C2) != 0x00) {
                GW(0xF7C0) ^= 0x01;
                GW(0xF7C2) = 0x00;
            }
            GB(0xF7CA) = GB(pc + 3);           /* spawn count              */
            if ((i16)(i8)GB(0xF7CA) > 0x02) GW(0xF6B8) = (u16)(u8)GB(0xF7CA);
            else                             GW(0xF6B8) = 0xFF;
            GW(0xF7CD) = GW(0xEA3E);           /* deadline = accum32       */
            GW(0xF7CB) = GW(0xEA3C);
            GB(0x38A6) = 0x01;
            break;
        case 0x01: {                           /* spawn one when due       */
            i16 dh = (i16)GW(0xF7CD);
            if (dh <= (i16)GW(0xEA3E) &&
                (dh != (i16)GW(0xEA3E) || GW(0xF7CB) <= GW(0xEA3C))) {
                u32 next = (((u32)GW(0xEA3E) << 16) | GW(0xEA3C)) + GB(pc + 4);
                GW(0xF7CD) = (u16)(next >> 16);
                GW(0xF7CB) = (u16)next;
                spawn_enemy_rec(GPTR(GW(pc + 1) + 0xA1));  /* skip count byte */
                GB(0xF7CA) = (u8)(GB(0xF7CA) - 1);
                if (GB(0xF7CA) == 0x00) GB(0x38A6) = (u8)(GB(0x38A6) + 1);
            }
            break;
        }
        case 0x02:                             /* wait until cleared       */
            if (GW(0xF46A) == 0x00) {
                GB(0x38A6) = 0x00;
                GW(0xF6BA) = (u16)(pc + 5);
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
