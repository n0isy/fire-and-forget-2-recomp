/* display_init.c — FAITHFUL port of the sprite-directory / display-list init
 * that `main` runs once at startup ("the init of everything"):
 *
 *   fn0A0D_09E4  ff_display_list_init  (0A0D:09E4 / ghidra 120d:09e4)
 *   fn0A0D_0B35  sprite_dir_register (0A0D:0B35, the 2nd-bank append)
 *
 * SOURCE OF TRUTH: transliterated from raw/reko/FF2EGA_0A0D.c AND the EXE
 * disassembly (ndisasm of file offset 0x38B4 / 0x3A05 — the decompilers mangled
 * the pointer-normalisation and the terminator, so the .dis is authoritative).
 *
 * fn0A0D_09E4 does four things (disasm @0x9E4..0xADA):
 *   1. Build 40 display-list far-pointers: ptrDBC8[i] -> DGROUP:0xD7B8 + i*0x1A
 *      (off @0xDBC8+i*4, seg @0xDBCA+i*4 = DGROUP).
 *   2. Link the entries into a chain: entry[i].next(off @+0x10, seg @+0x12) =
 *      ptrDBC8[i+1], for i = 0..38.
 *   3. Terminate: `les bx,[0xdc68]; mov [es:bx+0x12],0; mov [es:bx+0x10],0` —
 *      zero the next-pointer of the entry that the far pointer ptrDC68 addresses.
 *   4. Register the MAIN sprite bank (far ptr ptrDE59) into aDEEE:
 *        tEF52 = *(bank)            (sprite count word @ bank+0)
 *        ptr   = bank + 6           (skip count word + first 4-byte header)
 *        for i in 0..tEF52:
 *          aDEEE[i] = ptr           (off @0xDEEE+i*4, seg @0xDEF0+i*4)
 *          aF087[i] = low byte of sprite i's width  (byte @ ptr-2)
 *          H = *(ptr-4); W = *(ptr-2)
 *          ptr += ((W>>2)*H + 2) << 1 - 4 + 4   ==  (W/2)*H + 4   (next data ptr)
 *      i.e. the SAME record walk as bob.c: 4-byte header {H,W} then (W/2)*H data.
 *      So aDEEE[i] resolves to BOB sprite i for the main bank.
 *
 * fn0A0D_0B35 appends the 2nd bank (far ptr ptrDC7A = NUC): count2 = *(bank2);
 * for si in tEF52 .. tEF52+count2:  aDEEE[si] = 2nd-bank sprite (si-tEF52);
 * same (W/2)*H+4 stride; it does NOT touch tEF52 (the loop bound is tEF52+count2).
 *
 * PORT REPRESENTATION. Sprite pixels live in BobBank C structures (BOB.CPT /
 * NUC.CPT), not inside the DGROUP image, so aDEEE cannot hold real DOS far
 * pointers into the bank. We therefore encode aDEEE as a native directory:
 *   aDEEE[idx].off (0xDEEE+idx*4) = frame index WITHIN the bank
 *   aDEEE[idx].seg (0xDEF0+idx*4) = bank id + 1   (1 = BOB, 2 = NUC, 0 = empty)
 * This preserves the original's structure exactly (aDEEE[idx] identifies sprite
 * idx; main bank 0..count-1, 2nd bank appended at tEF52) and lets ff_dir_*()
 * resolve any registered index to (bank, frame). For idx < tEF52 this is the
 * identity aDEEE[idx] -> BOB frame idx, so it is bit-identical to the previous
 * `idx == BOB frame` shortcut; the difference is that idx >= 236 now correctly
 * reaches the appended NUC bank (needed for the roadside décor near-sprites).
 * tEF52 and aF087[] are still populated with the true values the faithful code
 * reads; the aim_target-overlapping ptrDC68 terminator (step 3) is reproduced
 * only when it targets an in-DGROUP entry (the chunky render never walks the
 * chain, so it is otherwise inert).
 */
#include "ff_game.h"
#include "gmem.h"

/* fn0A0D_09E4 — ff_display_list_init. Call once, after the sprite banks are
 * loaded, before morph_patch()/the first level. */
void ff_display_list_init(void)
{
    /* (1) 40 display-list far-pointers -> DGROUP:0xD7B8 + i*0x1A (stride 0x1A). */
    for (int i = 0; i < 0x28; ++i) {
        GW(0xDBC8 + i * 4) = (u16)(0xD7B8 + i * 0x1A);   /* off  */
        GW(0xDBCA + i * 4) = 0x0000;                     /* seg = DGROUP (port marker) */
    }

    /* (2) chain: entry[i].next(off @+0x10, seg @+0x12) = ptrDBC8[i+1], i=0..38. */
    for (int i = 0; i < 0x27; ++i) {
        u16 eoff = GW(0xDBC8 + i * 4);                   /* entry i offset in DGROUP */
        GW(eoff + 0x10) = GW(0xDBC8 + (i + 1) * 4);      /* next off  */
        GW(eoff + 0x12) = GW(0xDBCA + (i + 1) * 4);      /* next seg  */
    }

    /* (3) terminate: zero the next-ptr of the entry ptrDC68 addresses
     * (disasm: les bx,[0xdc68]; mov [es:bx+0x12],0; mov [es:bx+0x10],0). The port
     * only stores when the far ptr targets a DGROUP entry; the chunky render does
     * not walk the chain, so this is otherwise a no-op. */
    {
        u16 toff = GW(0xDC68);                           /* ptrDC68 off half */
        u16 tseg = GW(0xDC6A);                           /* ptrDC68 seg half */
        if (tseg == 0x0000 && toff != 0x0000 && (u32)toff + 0x14 <= 0xFFFF) {
            GW(toff + 0x10) = 0x0000;
            GW(toff + 0x12) = 0x0000;
        }
    }

    /* (4) register the main bank (ptrDE59 == GC.bob) into aDEEE[0..count). */
    int count = GC.bob ? ff_bob_count(GC.bob) : 0;
    GW(0xEF52) = (u16)count;                             /* tEF52 = sprite count */
    for (int i = 0; i < count; ++i) {
        int w = 0, h = 0;
        ff_bob_dims(GC.bob, i, &w, &h);
        GW(0xDEEE + i * 4) = (u16)i;                     /* aDEEE[i].off = frame i */
        GW(0xDEF0 + i * 4) = 0x0001;                     /* aDEEE[i].seg = bank 0 (+1) */
        GB(0xF087 + i)     = (u8)(w & 0xFF);             /* aF087[i] = width low byte */
    }

    /* game_main (083C:104) then saves the count: tDE69 = tEF52. Reproduce it so
     * run_level_init's `tEF52 = tDE69` restores the count on every level entry. */
    GW(0xDE69) = (u16)count;
}

/* fn0A0D_0B35 — append the 2nd bank (NUC == ptrDC7A) into aDEEE[tEF52 .. ). Does
 * NOT modify tEF52 (matches the disasm: base = [0xef52], bound = [0xef52]+count2).
 * In the original this runs when the 2nd bank is loaded (the landing/takeoff
 * cutscene fn0A0D_0ADB); the demo race never needs it, but registering it keeps
 * the directory complete for the roadside décor (BOB indices 276-299 = NUC). */
static int g_bank2_n;   /* # of 2nd-bank entries actually appended (0 until loaded) */

void sprite_dir_register_bank2(void)
{
    if (!GC.nuc) return;
    int base   = (int)GW(0xEF52);
    int count2 = ff_bob_count(GC.nuc);
    int n = 0;
    for (int j = 0; j < count2; ++j) {
        int idx = base + j;
        if ((u32)(0xDEEE + idx * 4) + 4 > sizeof(struct Globals)) break;   /* safety */
        GW(0xDEEE + idx * 4) = (u16)j;                  /* aDEEE[idx].off = NUC frame j */
        GW(0xDEF0 + idx * 4) = 0x0002;                  /* aDEEE[idx].seg = bank 1 (+1) */
        ++n;
    }
    g_bank2_n = n;
}

/* ---- aDEEE directory accessors (idx -> bank,frame) ---------------------- */
static int dir_lookup(int idx, BobBank **bank, int *frame)
{
    if (idx < 0 || (u32)(0xDEEE + idx * 4) + 4 > sizeof(struct Globals)) return 0;
    u16 seg = GW(0xDEF0 + idx * 4);
    if (seg == 0x0000 || seg == 0x0004) return 0;       /* empty / runtime bank */
    *frame = (int)GW(0xDEEE + idx * 4);
    *bank  = (seg == 0x0003) ? GC.gen                   /* boot-generated décor */
           : (seg == 0x0002) ? GC.nuc                   /* cutscene sheet       */
           : GC.bob;                                    /* main bank            */
    return *bank != NULL;
}

/* is aDEEE[idx] a runtime-composed shape (bank id 4, game/path_vm.c)? */
static int dir_is_rt(int idx, int *slot)
{
    if (idx < 0 || (u32)(0xDEEE + idx * 4) + 4 > sizeof(struct Globals)) return 0;
    if (GW(0xDEF0 + idx * 4) != 0x0004) return 0;
    *slot = (int)GW(0xDEEE + idx * 4);
    return 1;
}

void ff_dir_blit(int idx, int x, int y)
{
    BobBank *b; int fr, slot;
    if (dir_is_rt(idx, &slot)) { rt_shape_blit(slot, x, y); return; }
    if (dir_lookup(idx, &b, &fr)) ff_bob_blit(b, fr, x, y);
}

void ff_dir_dims(int idx, int *w, int *h)
{
    BobBank *b; int fr, slot;
    if (dir_is_rt(idx, &slot)) { rt_shape_dims(slot, w, h); return; }
    if (dir_lookup(idx, &b, &fr)) { ff_bob_dims(b, fr, w, h); return; }
    if (w) *w = 0;
    if (h) *h = 0;
}

/* chunky colour of pixel (x,y) of the registered sprite aDEEE[idx] — used by
 * the runtime shape composer to read its blit sources (any bank). */
int ff_dir_sample(int idx, int x, int y)
{
    BobBank *b; int fr, slot;
    if (dir_is_rt(idx, &slot)) return rt_shape_sample(slot, x, y);
    if (!dir_lookup(idx, &b, &fr)) return 0;
    int w = 0, h = 0;
    const u8 *d = ff_bob_data(b, fr, &w, &h);
    if (!d || x < 0 || x >= w || y < 0 || y >= h) return 0;
    long rb = w >> 3, ps = rb * h;
    u8 m = (u8)(0x80 >> (x & 7));
    long o = (long)y * rb + (x >> 3);
    return ((d[o] & m) ? 8 : 0) | ((d[o + ps] & m) ? 4 : 0)
         | ((d[o + 2 * ps] & m) ? 2 : 0) | ((d[o + 3 * ps] & m) ? 1 : 0);
}

int ff_dir_count(void)
{
    return (int)GW(0xEF52) + g_bank2_n;   /* main count + appended 2nd-bank entries */
}
