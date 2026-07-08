/* path_vm.c — FAITHFUL port of the PATH VM (fn0BA8_175D @0BA8:175D) and the
 * runtime enemy-sprite COMPOSER it drives:
 *
 *   fn0BA8_175D  path VM: walks the SAME level bytecode as the wave VM with its
 *                own cursor (tD7B4). On the script's GETCHAR entries (opcode 1)
 *                it BUILDS the shape whose index is the operand: marker
 *                wF46C[wF7D3] = -idx while building, flipped to +idx when done
 *                (the wave VM's GETCHAR waits on that). Opcode 0 (CLEAR) is a
 *                side-sync: flips bF7DB when it matches the wave side wF7C0,
 *                and swaps the ptrDC7A build-buffer half (tEF52 = tDE69 or
 *                tDE69+40; wF7D9 = 0 or 0x4000).
 *   fn0A0D_0D08  shape composer step (one per frame): opcode stream @a10B4[idx]
 *                (jump table @120d:0d2e): 0 END(-1); 1 NEW W=b2,H=b1 latch base;
 *                8 NEW; 2 BLIT-OR src@(x=b2,y=b3) (fn1187_2894); 3 BLIT-SYM
 *                (fn1187_27FE: writes src AND its mirror side by side = a
 *                symmetric 2W block); 7 BLIT-MIRROR (fn1187_2917); 4 FINALIZE:
 *                build the mip list @0x3837 (idx = 0x3827[k]+base, xform halves
 *                static [8,9,10,3,3,3,3,3]) then step fn0A0D_11F3 per frame;
 *                5/6 one-op combos: copy/mirror pair (src, src+1) + mips.
 *   fn0A0D_11F3  mip walker: for each [srcIdx, xform] pair generate a scaled
 *                copy via the INCREMENTAL scaler into aDEEE[d547], aF087 = real
 *                width, d547++; list ends at a negative srcIdx -> returns the
 *                new tEF52.
 *   fn0A0D_1594  incremental scaler: same decimation programs @0x2521 as the
 *                boot shape generator (shape_gen.c), but spread over frames:
 *                phase 0 = measure destH, phase 1 = measure destW + init +
 *                rows-budget d553 = w2505 / destW (w2505: 0x80 from PUT, 0xC0
 *                during a GETCHAR wait), phase 2 = (d553+1) program tokens per
 *                frame; prog[0] >= 4 = the fn1187_29A5 half-scale done in ONE
 *                frame (dest(x,y) = src(2x,2y)). Returns real destW when done.
 *
 * BUILD-TIME SIDE EFFECT (fn0BA8_175D case 1, state 0): for shape idx > 0x0A
 * the base slot is stored into PROTOTYPE[idx]+0x1B (0x14AB + idx*0x33) — the
 * enemy TYPE's spawn template — so every spawned entity of that type carries
 * its runtime sprite base in w001B, and fn0A0D_053E draws t0002 + w001B.
 *
 * PORT REPRESENTATION: composed sprites live in a mutable CHUNKY bank (RtSpr,
 * directory bank id 4). aDEEE[idx] = {.off = rt slot, .seg = 4}. The original
 * writes planar into the ptrDC7A double buffer; both halves map to rt slots
 * (a fresh slot per registration — the half swap only affects which aDEEE
 * range gets re-registered, which the port reproduces via tEF52).
 */
#include "ff_game.h"
#include "gnames.h"
#include "data/gamedata.h"   /* ffd_composer_dir / ffd_shape_streams /
                              * ffd_decimation_programs / ffd_spawn_offsets */
#include <stdlib.h>
#include <string.h>

/* ---- runtime-composed sprite bank (directory bank id 4) ------------------ *
 * INDEXED BY aDEEE INDEX (slot == aDEEE index). The original composes into the
 * ptrDC7A double-buffer, whose 80 aDEEE slots (tDE69 + 0/40) are OVERWRITTEN each
 * build cycle (EF52 is reset to the base by the path-VM arm / CLEAR side-sync).
 * We mirror that by binding rt[idx] to aDEEE index idx and re-allocating it in
 * place on each build — so repeated builds at the same index reuse the storage
 * instead of leaking (an append-only bank overflowed and hung the composer mid-
 * demo: the MIRADOR shape-25 rebuild stalled the wave VM at ~frame 1581). */
#define RT_MAX 512   /* covers the runtime aDEEE range (~300..380) with headroom */
typedef struct { int w, h; u8 *px; } RtSpr;
static RtSpr rt[RT_MAX];

static void rt_free_all(void)
{
    for (int i = 0; i < RT_MAX; ++i) { free(rt[i].px); rt[i].px = NULL; rt[i].w = rt[i].h = 0; }
}

/* bind rt storage to aDEEE index `idx` (the slot number == idx), overwriting the
 * previous pixel buffer at that index. Returns the slot (== idx) or -1. */
static int rt_bind(int idx, int w, int h)
{
    if (idx < 0 || idx >= RT_MAX || w <= 0 || h <= 0) return -1;
    free(rt[idx].px);
    rt[idx].w = w; rt[idx].h = h;
    rt[idx].px = calloc(1, (size_t)w * h);
    return rt[idx].px ? idx : -1;
}

void rt_shape_dims(int slot, int *w, int *h)
{
    if (slot >= 0 && slot < RT_MAX && rt[slot].px) { if (w) *w = rt[slot].w; if (h) *h = rt[slot].h; return; }
    if (w) *w = 0;
    if (h) *h = 0;
}

void rt_shape_blit(int slot, int x, int y)
{
    if (slot < 0 || slot >= RT_MAX || !rt[slot].px) return;
    RtSpr *s = &rt[slot];
    int qn = (ff_blit_quirk6 && x >= 0 && (x & 7) >= 6) ? 8 - (x & 7) : 0;   /* 1C73 shift-6/7 ROM bug (bob.c) */
    for (int sy = 0; sy < s->h; ++sy) {
        int dy = y + sy;
        if (dy < ff_blit_clip_top || dy >= FF_H) continue;   /* playfield top clip (fn1987_1c73) */
        for (int sx = 0; sx < s->w; ++sx) {
            int dx = x + sx;
            if (dx < 0 || dx >= FF_W) continue;
            u8 c = s->px[(long)sy * s->w + sx];
            if (c) {
                if (sx < qn && !(c & 1)) ff_fb[(long)dy * FF_W + dx] |= c;
                else                     ff_fb[(long)dy * FF_W + dx]  = c;
            }
        }
    }
}

int rt_shape_sample(int slot, int x, int y)
{
    if (slot < 0 || slot >= RT_MAX || !rt[slot].px) return 0;
    if (x < 0 || x >= rt[slot].w || y < 0 || y >= rt[slot].h) return 0;
    return rt[slot].px[(long)y * rt[slot].w + x];
}

/* fn0A0D_116A: register a fresh W x H blank at aDEEE[tEF52], aF087 = W, tEF52++.
 * Returns the rt slot (== the aDEEE index; the "current dest" the blits target). */
static int rt_new_shape(int W, int H)
{
    int idx = (int)Gw_spr_count;
    int slot = rt_bind(idx, W, H);              /* slot == idx */
    if (slot < 0) return -1;
    if (idx < SPRDIR_MAX) {
        g_sprdir[idx].off = (u16)slot;
        g_sprdir[idx].seg = 0x0004;
        g_sprw[idx]       = (u8)(W & 0xFF);
    }
    Gw_spr_count = (u16)(idx + 1);
    return slot;
}

/* ---- blits (fn1187_2894 / 27FE / 2917): src aDEEE[i] into an rt dest ------ */
/* dest(x..) |= src colour bits; x is byte-aligned in the original (x>>3). */
static void blit_or(int dst, int src_idx, int x, int y)
{
    if (dst < 0) return;
    int sw = 0, sh = 0;
    ff_dir_dims(src_idx, &sw, &sh);
    for (int r = 0; r < sh; ++r)
        for (int c = 0; c < sw; ++c) {
            int v = ff_dir_sample(src_idx, c, r);
            int dx = (x & ~7) + c, dy = y + r;
            if (v && dx >= 0 && dx < rt[dst].w && dy >= 0 && dy < rt[dst].h)
                rt[dst].px[(long)dy * rt[dst].w + dx] |= (u8)v;
        }
}

/* fn1187_27FE: symmetric blit — src block at x AND its horizontal mirror
 * adjacent, total 2*srcW wide (verified from the word/bp walk: the mirror of
 * pixel c lands at x + 2*srcW-1-c). */
static void blit_sym(int dst, int src_idx, int x, int y)
{
    if (dst < 0) return;
    int sw = 0, sh = 0;
    ff_dir_dims(src_idx, &sw, &sh);
    int x0 = x & ~7;
    for (int r = 0; r < sh; ++r)
        for (int c = 0; c < sw; ++c) {
            int v = ff_dir_sample(src_idx, c, r);
            if (!v) continue;
            int dy = y + r;
            if (dy < 0 || dy >= rt[dst].h) continue;
            int dx1 = x0 + c, dx2 = x0 + 2 * sw - 1 - c;
            if (dx1 >= 0 && dx1 < rt[dst].w) rt[dst].px[(long)dy * rt[dst].w + dx1] |= (u8)v;
            if (dx2 >= 0 && dx2 < rt[dst].w) rt[dst].px[(long)dy * rt[dst].w + dx2] |= (u8)v;
        }
}

/* fn1187_2917: mirror-only blit — pixel c lands at x + srcW-1-c. */
static void blit_mirror(int dst, int src_idx, int x, int y)
{
    if (dst < 0) return;
    int sw = 0, sh = 0;
    ff_dir_dims(src_idx, &sw, &sh);
    int x0 = x & ~7;
    for (int r = 0; r < sh; ++r)
        for (int c = 0; c < sw; ++c) {
            int v = ff_dir_sample(src_idx, c, r);
            if (!v) continue;
            int dx = x0 + sw - 1 - c, dy = y + r;
            if (dx >= 0 && dx < rt[dst].w && dy >= 0 && dy < rt[dst].h)
                rt[dst].px[(long)dy * rt[dst].w + dx] |= (u8)v;
        }
}

/* ==== fn0A0D_1594 — the incremental scaler ================================= */
static struct {
    int phase;                /* b385E: 0 measure H, 1 measure W + init, 2 emit */
    int unit;                 /* t385C: per-op unit counter                      */
    int src_idx;              /* d545 (aDEEE index of the source)                */
    const u8 *prog, *pp;      /* transform program + cursor (d555)               */
    int sw, sh;               /* source dims                                     */
    int dw_real, dw, dh, pad; /* dest geometry                                   */
    int srow, drow;           /* row cursors                                     */
    int budget;               /* d553 = w2505 / dw_real                          */
    int dst;                  /* rt slot under construction (== dst_idx)         */
    int dst_idx;              /* target aDEEE index (set by mip_tick)            */
} sc;

static void scaler_emit_row(void)
{
    int scol = 0, dcol = sc.pad, u = 0;
    const u8 *q = sc.prog;
    while (scol < sc.sw) {
        u8 op = *q++;
        if (op >= 4) continue;
        if (op == 0) { q = sc.prog; }
        else if (op == 1) {
            int c = *q++;
            for (u = 0; u < c && scol < sc.sw; ++u) {
                int v = ff_dir_sample(sc.src_idx, scol, sc.srow);
                if (sc.dst >= 0 && dcol >= 0 && dcol < rt[sc.dst].w && sc.drow < rt[sc.dst].h)
                    rt[sc.dst].px[(long)sc.drow * rt[sc.dst].w + dcol] = (u8)v;
                ++scol; ++dcol;
            }
        } else if (op == 2) {
            int c = *q++;
            int v = ff_dir_sample(sc.src_idx, scol, sc.srow);
            ++scol;
            for (u = 0; u < c; ++u) {
                if (sc.dst >= 0 && dcol >= 0 && dcol < rt[sc.dst].w && sc.drow < rt[sc.dst].h)
                    rt[sc.dst].px[(long)sc.drow * rt[sc.dst].w + dcol] = (u8)v;
                ++dcol;
            }
        } else scol += *q++;
    }
}

/* one fn0A0D_1594 call; reset != 0 aborts (the b3859 flag). Returns the real
 * dest width when the shape is complete, else 0. */
static int scaler_tick(int reset)
{
    if (reset) { sc.unit = 0; sc.phase = 0; return 0; }

    if (sc.prog[0] >= 4) {                     /* fn1187_29A5 one-shot half-scale */
        sc.dh      = (sc.sh + 1) >> 1;
        sc.dw_real = sc.sw >> 1;
        sc.dw      = (sc.dw_real + 15) & ~15;
        /* fn1987_29a5 keeps the EVEN source columns (0x268a LUT = bits 7,5,3,1)
         * and its (srcW>>4)&1 ODD branch writes them starting at dest byte 0's LOW
         * nibble = dest col (storedW-realW)>>1, i.e. the result is CENTRED in the
         * padded storage exactly like the program path. (Verified byte-for-byte vs
         * a QEMU read of aDEEE[349]: without pad our mip sat 4px too far left.) */
        int pad    = (sc.dw - sc.dw_real) >> 1;
        sc.dst     = rt_bind(sc.dst_idx, sc.dw, sc.dh);
        for (int dy = 0; dy < sc.dh && sc.dst >= 0; ++dy)
            for (int dx = 0; dx < sc.dw_real; ++dx)
                rt[sc.dst].px[(long)dy * sc.dw + pad + dx] =
                    (u8)ff_dir_sample(sc.src_idx, dx * 2, dy * 2);
        sc.phase = 0;
        return sc.dw_real;
    }

    if (sc.phase == 0) {                       /* measure dest H */
        sc.dh = shape_measure(sc.prog, sc.sh);
        sc.phase = 1;
        return 0;
    }
    if (sc.phase == 1) {                       /* measure dest W + init + budget */
        sc.dw_real = shape_measure(sc.prog, sc.sw);
        sc.dw  = (sc.dw_real + 15) & ~15;
        sc.pad = (sc.dw - sc.dw_real) >> 1;
        sc.dst = rt_bind(sc.dst_idx, sc.dw, sc.dh > 0 ? sc.dh : 1);   /* fn1187_1A30 zero-init */
        sc.srow = sc.drow = 0; sc.unit = 0;
        sc.pp = sc.prog;
        sc.phase = 2;
        if (sc.dw_real == 0) { sc.phase = 0; return 0; }   /* original hangs; guard */
        sc.budget = (i16)Gw_scale_budget / sc.dw_real;          /* d553 = w2505/destW */
        return 0;
    }
    /* phase 2: (budget+1) program tokens per call */
    int ret = 0;
    for (int it = 0; it <= sc.budget; ++it) {
        if (sc.srow >= sc.sh) {                            /* complete */
            sc.unit = 0; sc.phase = 0;
            ret = sc.dw_real;
            break;
        }
        u8 op = sc.pp[0];
        if (op >= 4) { sc.pp += 1; continue; }
        if (op == 0) { sc.pp = sc.prog; }
        else if (op == 1) {
            int c = sc.pp[1];
            if (sc.unit < c && sc.srow < sc.sh) {
                scaler_emit_row(); ++sc.srow; ++sc.drow; ++sc.unit;
            } else { sc.pp += 2; sc.unit = 0; }
        } else if (op == 2) {
            int c = sc.pp[1];
            if (sc.unit < c) { scaler_emit_row(); ++sc.drow; ++sc.unit; }
            else { sc.unit = 0; ++sc.srow; sc.pp += 2; }
        } else { sc.srow += sc.pp[1]; sc.pp += 2; }
    }
    return ret;
}

/* ==== fn0A0D_11F3 — the mip-list walker ==================================== */
static struct {
    int phase;        /* b385B: 0 latch, 1 next pair, 2 scaling */
    int k;            /* d54B list cursor — PAIR INDEX into g_miplist
                       * (was the DGROUP offset walking 0x3837 += 4)  */
    int d547;         /* dest sprite-directory slot cursor            */
} mw;

/* one fn0A0D_11F3 call over the g_miplist pair list; reset != 0 rearms.
 * Returns the new tEF52 when the whole list is done, else 0. */
static int mip_tick(int reset)
{
    if (reset) { mw.phase = 0; scaler_tick(1); return 0; }
    if (mw.phase == 0) {
        mw.k = 0;
        mw.d547 = (int)Gw_spr_count;
        mw.phase = 1;
        return 0;
    }
    if (mw.phase == 1) {
        i16 src = g_miplist[mw.k * 2];
        if (src < 0) { mw.phase = 0; return mw.d547; }     /* -1 terminator */
        sc.src_idx = (int)src;
        ff_dir_dims(sc.src_idx, &sc.sw, &sc.sh);
        sc.prog = ffd_decimation_programs[g_miplist[mw.k * 2 + 1]];
        sc.dst_idx = mw.d547;              /* this mip lands at g_sprdir[d547] (== rt slot) */
        sc.phase = 0;
        mw.k += 1;
        mw.phase = 2;
        return 0;
    }
    /* phase 2: one scaler tick */
    int w = scaler_tick(0);
    if (w != 0) {
        int idx = mw.d547;                                  /* register the mip */
        if (idx < SPRDIR_MAX) {
            g_sprdir[idx].off = (u16)sc.dst;
            g_sprdir[idx].seg = 0x0004;
            g_sprw[idx]       = (u8)(w & 0xFF);
        }
        mw.d547 += 1;
        mw.phase = 1;
    }
    return 0;
}

/* ==== fn0A0D_0D08 — the composer step ====================================== */
static int cs_state;   /* iRam21635: per-op multi-frame state         */
static int cs_base;    /* d4FF: tEF52 latched by op 1                 */
static int cs_src;     /* d4FD: source cursor for the combo ops       */
static int cur_dst;    /* the last rt_new_shape slot (the blit dest)  */

static void composer_build_miplist(void)
{
    /* op 4/5/6: the mip list's SRC halves = spawn_offsets[k] + base (the
     * XFORM halves + the -1 terminator stay as seeded from ffd_miplist_init
     * — THE 0x3837 mixed static/dynamic pattern, now explicit in g_miplist) */
    for (int k = 0; k < 8; ++k)
        g_miplist[k * 2] = (i16)(ffd_spawn_offsets[k] + cs_base);
}

static void composer_reset(void)
{
    cs_state = 0;
    mip_tick(1);           /* FUN_120d_11f3(x, 1) via the op-0 path */
}

/* one fn0A0D_0D08 call on the stream at composer-cursor `cur` (the cursor
 * keeps the ORIGINAL DGROUP-offset convention; the bytes come from the
 * const ffd_shape_streams data @0xF3A).
 * Returns the cursor advance, 0 = busy (no advance), -1 = shape done. */
static int composer_step(u16 cur)
{
    const u8 *p = ffd_shape_streams + (cur - 0x0F3A);
    switch (p[0]) {
    case 0:                                     /* END */
        composer_reset();
        return -1;
    case 1:                                     /* NEW + base latch */
        cs_base = (int)Gw_spr_count;
        cur_dst = rt_new_shape(p[2], p[1]);
        return 3;
    case 8:                                     /* NEW (no latch) */
        cur_dst = rt_new_shape(p[2], p[1]);
        return 3;
    case 2:                                     /* BLIT-OR (fn1187_2894) */
        blit_or(cur_dst, (int)p[1], p[2], p[3]);
        return 4;
    case 3:                                     /* BLIT-SYM (fn1187_27FE) */
        blit_sym(cur_dst, (int)p[1], p[2], p[3]);
        return 4;
    case 7:                                     /* BLIT-MIRROR (fn1187_2917) */
        blit_mirror(cur_dst, (int)p[1], p[2], p[3]);
        return 4;
    case 4:                                     /* FINALIZE: mips */
        if (cs_state == 0) { cs_state = 1; composer_build_miplist(); return 0; }
        else {
            int r = mip_tick(0);
            if (r == 0) return 0;
            cs_state = 0;
            Gw_spr_count = (u16)r;
            return 1;
        }
    case 5:                                     /* PAIR: copy src, src+1 + mips */
    case 6: {                                   /* PAIRM: symmetric 2W pair     */
        int mirror = (p[0] == 6);
        if (cs_state == 0) {
            cs_state = 1;
            cs_base = (int)Gw_spr_count;
            cs_src  = (int)p[1];
            int w = 0, h = 0;
            ff_dir_dims(cs_src, &w, &h);
            cur_dst = rt_new_shape(mirror ? w * 2 : w, h);
            if (mirror) blit_sym(cur_dst, cs_src, 0, 0);
            else        blit_or(cur_dst, cs_src, 0, 0);
            cs_src += 1;
            return 0;
        } else if (cs_state == 1) {
            cs_state = 2;
            int w = 0, h = 0;
            ff_dir_dims(cs_src, &w, &h);
            cur_dst = rt_new_shape(mirror ? w * 2 : w, h);
            if (mirror) blit_sym(cur_dst, cs_src, 0, 0);
            else        blit_or(cur_dst, cs_src, 0, 0);
            composer_build_miplist();
            return 0;
        } else {
            int r = mip_tick(0);
            if (r == 0) return 0;
            cs_state = 0;
            Gw_spr_count = (u16)r;
            return -1;                          /* one-op shapes end here */
        }
    }
    default:
        return -1;                              /* unknown op: treat as END */
    }
}

/* ==== fn0BA8_175D — the path VM ============================================ */
void path_vm_tick(int arm, int side)
{
    if (arm) {
        Gw_path_pc = Gw_wave_pc;               /* cursor = the wave-VM PC */
        composer_reset();                      /* fn0A0D_0D08 on a zero byte */
        Gw_buf_half = 0x00;
        Gw_path_marker_idx = 0x00;
        Gb_path_side = 0x00;
        Gb_path_side_latch = 0x00;
        Gb_build_state = 0x00;
        Gw_spr_count = Gw_spr_count_saved;               /* rebase to the runtime area */
        /* aDEEE[tDE69] = ptrDC7A+4 in the original (the build-buffer base);
         * the port registers rt slots on demand instead. */
        Gw_w38A8 = 0x01;
        Gi_shape_marker0 = 0x00;                     /* wF46C[0] = 0 */
        rt_free_all();                         /* fresh runtime bank */
    }

    u16 pc = Gw_path_pc;
    u8 op = wv_b(pc);
    if (op > 0x09) return;

    switch (op) {
    case 0x00:                                 /* side sync (script CLEAR) */
        if ((i16)(i8)Gb_path_side == (i16)side) {
            Gb_path_side ^= 0x01;
            Gw_path_pc = (u16)(pc + 1);
            if (Gb_path_side_latch != 0x00) {
                Gw_spr_count = (u16)(Gw_spr_count_saved + 40);   /* second buffer half */
                Gw_buf_half = 0x4000;
            } else {
                Gw_spr_count = Gw_spr_count_saved;
                Gw_buf_half = 0x00;
            }
            Gb_path_side_latch ^= 0x01;
            Gw_path_marker_idx = 0x00;
            /* aDEEE[tEF52] = bank-half base ptr — on-demand in the port */
        }
        break;

    case 0x01:                                 /* build shape (script GETCHAR) */
        switch (Gb_build_state) {
        case 0x00: {                           /* start: negative marker */
            int idx = wv_b(pc + 1);
            G_vm_marker(Gw_path_marker_idx) = (i16)(-idx);
            Gw_shape_cursor = ffd_composer_dir[idx].off;   /* stream cursor */
            if (idx > 0x0A)                    /* record the base slot in the  */
                *(u16 *)((u8 *)&g_protos[idx] + 0x1B) = Gw_spr_count;  /* TYPE PROTOTYPE +0x1B */
            Gb_build_state = 0x01;
            break;
        }
        case 0x01: {                           /* one composer step per frame */
            int adv = composer_step(Gw_shape_cursor);
            Gw_w38A8 = 0x00;
            if (adv < 0) Gb_build_state = 0x02;
            else         Gw_shape_cursor = (u16)(Gw_shape_cursor + adv);
            break;
        }
        case 0x02:                             /* done: flip marker positive */
            G_vm_marker(Gw_path_marker_idx) = (i16)(-G_vm_marker(Gw_path_marker_idx));
            Gw_path_marker_idx = (u16)(Gw_path_marker_idx + 1);
            G_vm_marker(Gw_path_marker_idx) = 0x00;
            Gw_path_pc = (u16)(pc + 2);
            Gb_build_state = 0x00;
            break;
        }
        break;

    case 0x02: Gw_path_pc = (u16)(pc + 3); break;      /* PUT: skip     */
    case 0x03:                                          /* WHILE         */
        if ((i16)Gw_objective_hi <= 0 &&
            ((i16)Gw_objective_hi != 0 || (i16)(i8)wv_b(pc + 1) >= (i16)Gw_objective_lo))
            Gw_path_pc = (u16)(pc + 4);
        else
            Gw_path_pc = (u16)(wv_w(pc + 2) + 0xA0);
        break;
    case 0x04: Gw_path_pc = (u16)(wv_w(pc + 1) + 0xA0); break;   /* GOTO   */
    case 0x05: Gw_path_pc = (u16)(pc + 3); break;      /* SETDIST: skip */
    case 0x06:                                          /* GOSUB         */
        Gw_path_ret = (u16)(pc + 3);                     /* ptrF7D5 return */
        Gw_path_pc = (u16)(wv_w(pc + 1) + 0xA0);
        break;
    case 0x07: Gw_path_pc = Gw_path_ret; break;          /* RETURN        */
    case 0x08:                                          /* BEQ           */
        if ((Gw_objective_lo | Gw_objective_hi) == 0x00)
            Gw_path_pc = (u16)(wv_w(pc + 1) + 0xA0);
        else
            Gw_path_pc = (u16)(pc + 3);
        break;
    case 0x09: Gw_path_pc = (u16)(pc + 5); break;      /* WAVE: skip    */
    }
}
