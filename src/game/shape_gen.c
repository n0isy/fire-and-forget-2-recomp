/* shape_gen.c — FAITHFUL port of the boot-time SHAPE GENERATOR that builds the
 * 64 roadside-décor sprites (aDEEE[236..299], tEF52 236 -> 300):
 *
 *   fn0A0D_133B (FUN_120d_133b)  register_shape_list: walk the pair list @0x2613
 *                                [src sprite, transform idx] until src < 0; for
 *                                each pair build a decimated copy of the source
 *                                BOB sprite into the tDC74 bank buffer, register
 *                                it at aDEEE[tEF52+i] and store the real width in
 *                                aF087; finally tEF52 = end (300).
 *   fn0A0D_13FC (FUN_120d_13fc)  build one transformed sprite (row pass).
 *   fn0A0D_17C8 (FUN_120d_17c8)  measure: src count -> dest count for a program.
 *   fn0A0D_186E (FUN_120d_186e)  emit one dest row (column pass; sample via
 *                                fn1987_191b, write via fn1987_199d).
 *   fn1187_29A5 (FUN_1987_29a5)  the `04` special program: 2:1 half-scale via
 *                                the bit LUT built by fn1187_263C — keeps the
 *                                EVEN pixels/rows: dest(x,y) = src(2x, 2y).
 *
 * TRANSFORM PROGRAMS: 8 entries of 0x16 bytes @0x2521. A program is a byte
 * stream of ops applied identically to ROWS and COLUMNS:
 *   00        restart the program from the beginning
 *   01 n      copy n units (src+1, dest+1 each; bounded by the src count)
 *   02 n      replicate one src unit n times (src+1, dest+n)
 *   03 n      skip n src units
 *   >= 04 (as the FIRST byte) selects the fn1187_29A5 half-scale path
 * (bytes >= 4 elsewhere are skipped, exactly like the original dispatch).
 *
 * DEST GEOMETRY (program path): destH = measure(prog, srcH); real destW =
 * measure(prog, srcW); stored W = (realW+15)&~15; the pixels are CENTERED with
 * pad = (storedW - realW) >> 1 (fn0A0D_13FC +0x1C). Half-scale path: destH =
 * (srcH+1)>>1, realW = srcW>>1, stored W aligned, NO pad (left-aligned).
 *
 * VERIFIED vs QEMU at the first run_level entry (deterministic system_reset +
 * hbreak): the original's generated headers for aDEEE[276..279] are (H=19,W=32),
 * (15,32), (14,32), (12,16) and the patched script5 thresholds are
 * [25,23,21,20,19,15,14,12,8,7,5,4] — the measure() arithmetic reproduces the
 * 19/15 chain from src sprite 69 (H=23) exactly.
 *
 * PORT REPRESENTATION: the generated records are written as a standard bank
 * image (u16 count + {H,W,planar} records — identical to the tDC74 layout) and
 * parsed into GC.gen; aDEEE[idx] = {.off = frame, .seg = 3} (bank id 3).
 */
#include "ff_game.h"
#include "gmem.h"
#include <stdlib.h>
#include <string.h>

#define GEN_MAX 96

/* ---- fn0A0D_17C8: measure how many dest units a program yields from n src --
 * (shared with the incremental scaler in path_vm.c) */
int shape_measure(const u8 *prog, int n)
{
    const u8 *p = prog;
    int i = 0, out = 0;
    while (i < n) {
        u8 op = *p++;
        if (op >= 4) continue;                 /* non-opcode byte: skipped */
        if (op == 0) { p = prog; }
        else if (op == 1) {
            int c = *p++;
            for (int k = 0; k < c && i < n; ++k) { ++i; ++out; }
        } else if (op == 2) {
            int c = *p++;
            ++i; out += c;
        } else {                               /* op == 3 */
            i += *p++;
        }
    }
    return out;
}

/* ---- one generated sprite ------------------------------------------------- */
typedef struct { int h, w, real_w; u8 *px; } GenSpr;   /* px = chunky W*H */

/* fn0A0D_186E: emit one dest row (column pass over the same program) */
static void emit_row(GenSpr *g, const u8 *prog, int pad,
                     const u8 *sd, int sw, int sh, int sy, int dy)
{
    long rb = sw >> 3, ps = rb * sh;           /* planar row bytes / plane size */
    const u8 *row = sd + (long)sy * rb;
    int scol = 0, dcol = pad;
    const u8 *q = prog;
    while (scol < sw) {
        u8 op = *q++;
        if (op >= 4) continue;
        if (op == 0) { q = prog; }
        else if (op == 1) {
            int c = *q++;
            for (int k = 0; k < c && scol < sw; ++k) {
                u8 m = (u8)(0x80 >> (scol & 7));
                long b = scol >> 3;
                int col = ((row[b] & m) ? 8 : 0) | ((row[b + ps] & m) ? 4 : 0)
                        | ((row[b + 2 * ps] & m) ? 2 : 0) | ((row[b + 3 * ps] & m) ? 1 : 0);
                if (dcol >= 0 && dcol < g->w && dy >= 0 && dy < g->h)
                    g->px[(long)dy * g->w + dcol] = (u8)col;
                ++scol; ++dcol;
            }
        } else if (op == 2) {                  /* sample once, write n times */
            int c = *q++;
            u8 m = (u8)(0x80 >> (scol & 7));
            long b = scol >> 3;
            int col = ((row[b] & m) ? 8 : 0) | ((row[b + ps] & m) ? 4 : 0)
                    | ((row[b + 2 * ps] & m) ? 2 : 0) | ((row[b + 3 * ps] & m) ? 1 : 0);
            ++scol;
            for (int k = 0; k < c; ++k) {
                if (dcol >= 0 && dcol < g->w && dy >= 0 && dy < g->h)
                    g->px[(long)dy * g->w + dcol] = (u8)col;
                ++dcol;
            }
        } else {
            scol += *q++;
        }
    }
}

/* fn0A0D_13FC: build one transformed sprite from BOB source `src` + program */
static int shape_build(GenSpr *g, int src, const u8 *prog)
{
    int sw = 0, sh = 0;
    const u8 *sd = ff_bob_data(GC.bob, src, &sw, &sh);
    if (!sd || sw <= 0 || sh <= 0) return -1;

    if (prog[0] >= 4) {                        /* fn1187_29A5 half-scale path */
        g->h      = (sh + 1) >> 1;
        g->real_w = sw >> 1;
        g->w      = (g->real_w + 15) & ~15;
        int hpad  = (g->w - g->real_w) >> 1;   /* fn1987_29a5 ODD branch CENTRES the
                                                * even-column halved pixels (starts at
                                                * dest byte-0 low nibble = col pad) */
        g->px     = calloc(1, (size_t)g->w * g->h);
        if (!g->px) return -1;
        long rb = sw >> 3, ps = rb * (long)sh;
        for (int dy = 0; dy < g->h; ++dy) {    /* keeps even rows/pixels */
            const u8 *row = sd + (long)(dy * 2) * rb;
            for (int dx = 0; dx < g->real_w; ++dx) {
                int sx = dx * 2;
                u8 m = (u8)(0x80 >> (sx & 7));
                long b = sx >> 3;
                int col = ((row[b] & m) ? 8 : 0) | ((row[b + ps] & m) ? 4 : 0)
                        | ((row[b + 2 * ps] & m) ? 2 : 0) | ((row[b + 3 * ps] & m) ? 1 : 0);
                g->px[(long)dy * g->w + hpad + dx] = (u8)col;   /* centred like fn1987_29a5 */
            }
        }
        return 0;
    }

    g->h      = shape_measure(prog, sh);
    g->real_w = shape_measure(prog, sw);
    g->w      = (g->real_w + 15) & ~15;
    int pad   = (g->w - g->real_w) >> 1;       /* fn0A0D_13FC +0x1C centering */
    if (g->h <= 0 || g->w <= 0) return -1;
    g->px = calloc(1, (size_t)g->w * g->h);    /* fn1187_1A30 zero-fill */
    if (!g->px) return -1;

    int srow = 0, drow = 0;                    /* row pass */
    const u8 *p = prog;
    while (srow < sh) {
        u8 op = *p++;
        if (op >= 4) continue;
        if (op == 0) { p = prog; }
        else if (op == 1) {
            int c = *p++;
            for (int k = 0; k < c && srow < sh; ++k) {
                emit_row(g, prog, pad, sd, sw, sh, srow, drow);
                ++srow; ++drow;
            }
        } else if (op == 2) {                  /* one src row emitted n times */
            int c = *p++;
            for (int k = 0; k < c; ++k) {
                emit_row(g, prog, pad, sd, sw, sh, srow, drow);
                ++drow;
            }
            ++srow;
        } else {
            srow += *p++;
        }
    }
    return 0;
}

/* ---- fn0A0D_133B: build all shapes, pack them into a bank, register -------- */
void shape_gen_build(void)
{
    if (!GC.bob) return;

    GenSpr gs[GEN_MAX];
    int n = 0;
    long data_bytes = 0;
    for (int i = 0; i < GEN_MAX; ++i) {
        i16 src = GI(0x2613 + i * 4);
        if (src < 0) break;                    /* list terminator */
        i16 t = GI(0x2613 + i * 4 + 2);
        const u8 *prog = GPTR(0x2521 + t * 0x16);
        if (shape_build(&gs[n], (int)src, prog) != 0) break;
        data_bytes += 4 + (long)(gs[n].w >> 1) * gs[n].h;
        ++n;
    }
    if (n == 0) return;

    /* pack into a standard bank image (count + {H,W,planar} records) */
    long len = 2 + data_bytes;
    u8 *bank = malloc((size_t)len);
    if (!bank) { for (int i = 0; i < n; ++i) free(gs[i].px); return; }
    bank[0] = (u8)(n & 0xFF); bank[1] = (u8)(n >> 8);
    long pos = 2;
    for (int i = 0; i < n; ++i) {
        GenSpr *g = &gs[i];
        bank[pos] = (u8)(g->h & 0xFF); bank[pos + 1] = (u8)(g->h >> 8);
        bank[pos + 2] = (u8)(g->w & 0xFF); bank[pos + 3] = (u8)(g->w >> 8);
        pos += 4;
        long rb = g->w >> 3, ps = rb * (long)g->h;
        memset(bank + pos, 0, (size_t)(g->w >> 1) * g->h);
        for (int y = 0; y < g->h; ++y)
            for (int x = 0; x < g->w; ++x) {
                u8 c = g->px[(long)y * g->w + x];
                if (!c) continue;
                u8 m = (u8)(0x80 >> (x & 7));
                long b = pos + (long)y * rb + (x >> 3);
                if (c & 8) bank[b]          |= m;
                if (c & 4) bank[b + ps]     |= m;
                if (c & 2) bank[b + 2 * ps] |= m;
                if (c & 1) bank[b + 3 * ps] |= m;
            }
        pos += (long)(g->w >> 1) * g->h;
        free(g->px);
    }
    if (GC.gen) ff_bob_free(GC.gen);
    GC.gen = ff_bob_parse_mem(bank, (int)len);
    if (!GC.gen) return;

    /* register at aDEEE[tEF52 .. tEF52+n), store real widths, bump tEF52 —
     * then tDE69 = tEF52 exactly as main (083C:104) saves it after 133B. */
    int base = (int)GW(0xEF52);
    for (int i = 0; i < n; ++i) {
        int idx = base + i;
        if ((u32)(0xDEEE + idx * 4) + 4 > sizeof(struct Globals)) break;
        GW(0xDEEE + idx * 4) = (u16)i;         /* aDEEE[idx].off = gen frame i */
        GW(0xDEF0 + idx * 4) = 0x0003;         /* aDEEE[idx].seg = bank 2 (+1) */
        GB(0xF087 + idx)     = (u8)(gs[i].real_w & 0xFF);   /* aF087 real W */
    }
    GW(0xEF52) = (u16)(base + n);              /* tEF52 = 300 */
    GW(0xDE69) = GW(0xEF52);                   /* main: tDE69 = tEF52 */
}
