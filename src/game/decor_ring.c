/* decor_ring.c — FAITHFUL port of the roadside décor ring (rocks/bushes at the
 * road edges):
 *
 *   fn0869_1946 (0869:1946)  décor spawn: copy a 10-byte prototype from the
 *                            static table @0x2727 (16 kinds, cycled by w379F)
 *                            into the aDDEC ring (10 entries @0xDDEC, stride
 *                            0x0A), one spawn per 16-unit scroll step.
 *   fn0869_17B9 (0869:17B9)  décor draw: walk the ring, compute the depth row
 *                            si_73 from the scroll phase, scale from E4CC,
 *                            morph-pick a sprite (scripts 5/6/7 @0x13c4/13F8/
 *                            142C — thresholds patched by morph_patch), and
 *                            enqueue at the road edge via fn0A0D_082A into the
 *                            same depth-sorted display list the entities use.
 *
 * SOURCE OF TRUTH: raw/reko/FF2EGA_0869.c lines 1307-1378 (17B9) and 1380-1410
 * (1946); the run_level call sites @705-711 (spawn accumulator wFFFFFFF2 and
 * the 17B9 call before the background fill).
 *
 * Ring entry fields (stride 0x0A): +0 w0000 = kind (>0x0D -> full morph scale,
 * else half), +2 w0002 = side/offset (>0 left of road, <=0 right; |w0002| is a
 * SHIFT count applied to the scale), +4 w0004 = spawn scroll-phase (low nibble
 * of accum>>4), +6 ptr0006 = far ptr to the morph script (seg 0de1 = DGROUP).
 *
 * Sprites: the scripts pick BOB 68-79 (far) and 276-299 (near, in the 2nd bank
 * NUC appended at aDEEE[tEF52..) by fn0A0D_0B35). During the attract DEMO the
 * original never loads NUC (the round_transition NUC scene does), so aDEEE 276+
 * is unregistered; the port's dl_flush skips unregistered directory entries.
 *
 * fn0800_038F(tEA3C, 4) is the 32-bit right shift of the scroll accumulator
 * (EA3E:EA3C) — the low word of (accum32 >> 4) is the spawn/draw scroll phase.
 */
#include "ff_game.h"
#include "gmem.h"
#include <string.h>

/* run_level stack local wFFFFFFF2 — the spawn accumulator (+= tE9CC/frame).
 * The original NEVER initializes it (no writer besides the += in the loop): it
 * starts as menu-stack residue. Measured on the QEMU reference at run_level
 * entry (ss:bp-0x0E after the prologue): 0x2540 — large, so the `> 0x40` fire
 * condition holds every frame for the first ~150 frames and the spawn rate is
 * limited only by the wF6D8 one-per-scroll-phase gate + the wF468 density gate
 * (reference: ring fills at frames 29-31, phases 2/3/4, right after wF468
 * turns 3). Deterministic per boot on the reference machine. */
static i16 s_spawn_accum;

void decor_reset(void)
{
    s_spawn_accum = 0x2540;
}

/* stage-entry override: wFFFFFFF2 is an UNINIT stack local whose residue differs
 * per entry path (menu 0x2540 measured; start_level path measured separately). */
void decor_set_accum(int v)
{
    s_spawn_accum = (i16)v;
}

/* fn0869_1946 — spawn one décor entry into the aDDEC ring. */
static void decor_spawn(void)
{
    if (!((i16)GW(0xDC7E) < (i16)GW(0xF468) && GW(0xDC6C) != 0x00))
        return;                                        /* density gate + race on */

    i16 si_23 = (i16)(GW(0xEF4A) + 1);                 /* next write cursor */
    if ((i16)GW(0xEF4A) > 0x08) si_23 = 0x00;
    if (si_23 == (i16)GW(0xDE5D)) return;              /* ring full */

    u16 ax_40 = (u16)((((u32)GW(0xEA3E) << 16) | GW(0xEA3C)) >> 4);
    if (ax_40 == GW(0xF6D8)) return;                   /* one spawn / 16-unit step */

    GW(0xDC7E) = (u16)(GW(0xDC7E) + 1);
    GW(0xF6D8) = ax_40;
    i16 slot = (i16)GW(0xEF4A);                        /* ax_54 = old cursor */
    GW(0xEF4A) = (u16)si_23;
    /* fn0800_03AD(0x0A, w379F*0x0A + 0x2727, slot*0x0A + 0xDDEC) */
    memcpy(GPTR(0xDDEC + slot * 0x0A), GPTR(0x2727 + (i16)GW(0x379F) * 0x0A), 0x0A);
    GW(0x379F) = (u16)((GW(0x379F) + 1) & 0x0F);       /* cycle the 16 kinds */
    GW(0xDDEC + slot * 0x0A + 0x04) = (u16)(ax_40 & 0x0F);   /* w0004 = phase */
}

/* wave-VM GETCHAR path (fn0BA8_13FD case 1, marker still 0): the original
 * calls fn0869_1946 directly each tick while waiting on a shape build. */
void decor_ring_vm_spawn(void)
{
    decor_spawn();
}

/* run_level @705-709: the per-frame spawn accumulator (stack local wFFFFFFF2). */
void decor_spawn_step(void)
{
    s_spawn_accum = (i16)(s_spawn_accum + (i16)GW(0xE9CC));
    if (s_spawn_accum > 0x40) {
        s_spawn_accum -= 0x40;
        decor_spawn();                                 /* fn0869_1946 */
    }
}

/* fn0869_17B9 — walk the ring and enqueue each active entry into the shared
 * depth-sorted display list (fn0A0D_082A == dl_enqueue_sorted). Called from
 * render_entities() before the entity-pool walk (the original enqueues décor
 * before the background fill / entity update; the sort key interleaves them). */
void decor_ring_enqueue(void)
{
    if ((i16)GW(0xDE5D) == (i16)GW(0xEF4A)) return;    /* ring empty */

    i16 dx_23 = (i16)(0x0F - (GW(0xEA3C) & 0x0F));
    u16 ax_30 = (u16)((((u32)GW(0xEA3E) << 16) | GW(0xEA3C)) >> 4);
    i16 wLoc08 = ((i16)GW(0xE9CC) > 0x0E) ? 0x02 : 0x01;   /* recycle bound */

    i16 di = (i16)GW(0xDE5D);
    do {
        u16 eoff = (u16)(0xDDEC + di * 0x0A);
        if (GW(eoff) == 0x00) {                        /* zero kind ends the walk */
            di = (i16)GW(0xEF4A);
        } else {
            i16 si_73 = (i16)(0x0F - ((ax_30 - GW(eoff + 0x04)) & 0x0F));
            u16 ax_87 = (u16)GB(0xE4CC + ((si_73 << 4) + dx_23));  /* scale */
            u16 p     = GW(eoff + 0x06);               /* morph script (DGROUP off) */
            i16 wLoc14 = ((i16)GW(eoff) > 0x0D) ? (i16)ax_87
                                                : (i16)(ax_87 >> 1);
            while (GI(p) > wLoc14) p += 4;             /* morph walk */
            i16 spr = GI(p + 2);                       /* t0002 sprite index */
            if (spr < 0) spr = GI(p - 2);              /* tFFFE = prev sprite */

            i16 w0002 = GI(eoff + 0x02);
            if (w0002 > 0x00) {                        /* left of the road */
                dl_enqueue_sorted(
                    (i16)(GI(0x2C51 + 2 * si_73) - (i16)(ax_87 << GB(eoff + 0x02))),
                    GI(0x2C51 + 2 * (si_73 + 0x20)),
                    (int)spr, (i16)(si_73 << 4));
            } else {                                   /* right of the road */
                dl_enqueue_sorted(
                    (i16)(GI(0x2C51 + 2 * (si_73 + 0x10)) + (i16)(ax_87 << -w0002)),
                    GI(0x2C51 + 2 * (si_73 + 0x20)),
                    (int)spr, (i16)(si_73 << 4));
            }

            if (si_73 < wLoc08) {                      /* reached the near edge */
                GW(eoff) = 0x00;                       /* recycle the entry */
                GW(0xDC7E) = (u16)(GW(0xDC7E) - 1);
                if ((i16)GW(0xDE5D) < 0x09) GW(0xDE5D) = (u16)(GW(0xDE5D) + 1);
                else                        GW(0xDE5D) = 0x00;
            }
            if (di == 0x09) di = 0x00;
            else            di = (i16)(di + 1);
        }
    } while (di != (i16)GW(0xEF4A));
}
