/* sprite_dl.c — chunky port of the entity sprite render pipeline:
 *   fn0A0D_0C8F  patch_anim_refs : patch each morph script's thresholds
 *                                  (t0000) = BOB sprite height (one-time, init).
 *   fn0DAE_0446 tail + fn0869_1726 object_shadow_project + fn0A0D_053E
 *                enqueue_enemy_sprite : per active pool slot, compute the
 *                perspective-scaled screen position and pick the morph sprite,
 *                then enqueue into a depth-sorted display list.
 *   fn1187_1C73 draw_object_list : the original is a low-level EGA masked blit;
 *                our framebuffer is chunky, so we collect (centre-x, bottom-y,
 *                BOB frame, depth) and ff_bob_blit far->near (painter's order).
 *
 * aDEEE[idx] == BOB frame idx (the main bank registers at obj-table base 0;
 * verified for the low indices the demo uses). Sprite drawn with centre-x at the
 * enqueue x and BOTTOM at the enqueue y (fn1187_1C73: t0000-=W>>1, t0002-=H),
 * plus the playfield origin PF_Y=48.
 */
#include "ff_game.h"
#include "gmem.h"
#include <string.h>

#define PF_Y 48

/* per-slot explosion morph-threshold snapshot (see entity_faithful.c) */
u16 g_slot_thresh[20];
/* per-frame "active at its fn0DAE_0446 loop turn" mask (see entity_faithful.c) */
u8  g_slot_drawn[20];

/* optional extra enqueue between the pool walk and the flush — used by the
 * round_transition finale to insert its rocket/burst fn0A0D_082A entries into
 * the same depth-sorted display list (game/cutscene.c). NULL otherwise. */
void (*g_dl_extra)(void);

/* ---- display list ------------------------------------------------------- */
#define DL_MAX 64
/* kind 0 = sprite (centre-x/bottom-y/frame); kind 1 = FILL-RECT box — the
 * original's type-1 entry ([+0x18]=1, colour @[+0x16] -> fn1187_1A95), used by
 * the shape_kind-1 ground shadow (fn0A0D_053E case 0x12703). A box is a normal
 * sorted entry: later-drawn equal/nearer sprites COVER it (the port used to
 * paint shadows immediately before all sprites — the f201/f217 layering gap). */
typedef struct { i16 x, y; int frame; i16 depth; i16 x1, y1; u8 kind, colour; } DLEnt;
static DLEnt g_dl[DL_MAX];
static int   g_dln;

void dl_reset(void) { g_dln = 0; }

/* enqueue: xc = centre x, yb = bottom y (playfield-local), depth = di (sort key) */
static void dl_add(i16 xc, i16 yb, int frame, i16 depth)
{
    if (g_dln < DL_MAX && frame >= 0) {
        g_dl[g_dln].x = xc; g_dl[g_dln].y = yb;
        g_dl[g_dln].frame = frame; g_dl[g_dln].depth = depth;
        g_dl[g_dln].kind = 0;
        ++g_dln;
    }
}

/* enqueue a type-1 FILL-RECT entry (x0..x1, rows y1..y0 inclusive, playfield-
 * local) — fn0A0D_053E case 0x12703 writes [+0]=x0 [+2]=y0 [+4]=x1 [+6]=y1
 * [+0x16]=colour [+0x18]=1 and inserts via fn0BA8_1FC8 with the slot's di. */
static void dl_add_box(i16 x0, i16 y0, i16 x1, i16 y1, u8 colour, i16 depth)
{
    if (g_dln < DL_MAX) {
        g_dl[g_dln].x = x0; g_dl[g_dln].y = y0;
        g_dl[g_dln].x1 = x1; g_dl[g_dln].y1 = y1;
        g_dl[g_dln].frame = 0; g_dl[g_dln].depth = depth;
        g_dl[g_dln].kind = 1; g_dl[g_dln].colour = colour;
        ++g_dln;
    }
}

/* fn0A0D_082A enqueue_sprite_sorted — public enqueue for the décor ring (the
 * original writes the same display-list entry shape as fn0A0D_07CF and inserts
 * depth-sorted via fn0BA8_1FC8; our list is sorted once in dl_flush). */
void dl_enqueue_sorted(i16 xc, i16 yb, int frame, i16 depth)
{
    dl_add(xc, yb, frame, depth);
}

/* draw all enqueued sprites far (largest di) -> near, then clear. */
void dl_flush(void)
{
    /* insertion sort, far (largest depth) first. TIE ORDER PROVEN from
     * fn0BA8_1FC8 (13a8:1fc8): the list walk `while (param_1 < key[cursor])`
     * is STRICT, so a new entry with an EQUAL key is inserted BEFORE the
     * existing equal-key entries — i.e. among equal depths the LATER-enqueued
     * sprite is drawn FIRST and the EARLIER-enqueued one lands ON TOP. The
     * `<=` below reproduces that (a plain stable `<` sort had it reversed —
     * visible as the boss break-apart flame texture at f2020-2040, where the
     * 4 sub-explosion copies + the main slot all share the boss's depth). */
    for (int i = 1; i < g_dln; ++i) {
        DLEnt k = g_dl[i]; int j = i - 1;
        while (j >= 0 && g_dl[j].depth <= k.depth) { g_dl[j + 1] = g_dl[j]; --j; }
        g_dl[j + 1] = k;
    }
    ff_blit_clip_top = PF_Y;                     /* fn1987_1c73 playfield top clip */
    ff_blit_quirk6   = 1;                        /* + the shift-6 leading-byte ROM bug */
    for (int i = 0; i < g_dln; ++i) {
        if (g_dl[i].kind == 1) {                 /* type-1 entry: fn1187_1A95 fill */
            int ya = g_dl[i].y1 < g_dl[i].y ? g_dl[i].y1 : g_dl[i].y;
            int yb = g_dl[i].y1 < g_dl[i].y ? g_dl[i].y  : g_dl[i].y1;
            for (int yy = ya; yy <= yb; ++yy) {
                int sy = yy + PF_Y;
                if (sy < ff_blit_clip_top || sy >= FF_H) continue;
                for (int xx = g_dl[i].x; xx <= g_dl[i].x1; ++xx)
                    if (xx >= 0 && xx < FF_W) ff_fb[(long)sy * FF_W + xx] = g_dl[i].colour;
            }
            continue;
        }
        /* fn1187_1C73 draws whatever aDEEE[frame] points to — NO tEF52 range
         * check (the runtime shapes live at aDEEE[340..379] but tEF52 is reset
         * to tDE69/tDE69+40 by the build's side-sync, so `frame >= tEF52` wrongly
         * dropped live runtime enemies mid-build). ff_dir_dims already returns 0
         * for an unregistered/empty aDEEE slot, which is the real guard. */
        int w = 0, h = 0;
        ff_dir_dims(g_dl[i].frame, &w, &h);      /* aDEEE[frame] (any bank) */
        if (w <= 0 || h <= 0) continue;
        /* fn1187_1C73 top-clip QUIRK — PROVEN BY DISASSEMBLY (ndisasm @1187:
         * 1CDD `sub [si+2],ax` y_top=y-H; 1CF0 jns = unclipped; clip path:
         * 1CF2 `add ax,[si+4]` =y, 1CFA `INC AX` =y+1 visible rows, 1D01
         * `sub bx,ax` source skip H-(y+1), 1D14 top=0): a sprite crossing the
         * playfield top (y-H < 0) draws rows 0..y showing source rows
         * H-1-y..H-1 — bottom row = y INCLUSIVE, ONE ROW LOWER than the
         * unclipped anchor (bottom = y-1). Pixel-verified on the finale
         * rocket (NUC frame 13 fits orig at bottom 93 = y+PF_Y, mismatch 0). */
        int top = g_dl[i].y - h;
        if (top < 0) top += 1;
        ff_dir_blit(g_dl[i].frame, g_dl[i].x - (w >> 1), top + PF_Y);
    }
    ff_blit_clip_top = 0;
    ff_blit_quirk6   = 0;
    g_dln = 0;
}

/* ---- fn0A0D_0C8F: patch morph-script thresholds with BOB heights -------- *
 * Morph-script directory = far pointers @0x1460 (stride 4: off,seg), terminated
 * by a null entry. Each script is a list of [t0000=threshold, t0002=spriteIdx]
 * 4-byte entries, ending when t0002 < 0. The patch sets t0000 = height of the
 * BOB sprite t0002 (fn0A0D_0B97 = aDEEE[t0002]->wFFFC), so the morph walk later
 * selects the largest sprite that fits the distance-scaled size. */
void morph_patch(void)
{
    if (!GC.bob) return;
    for (int s = 0; ; ++s) {
        u16 off = GW(0x1460 + s * 4);
        u16 seg = GW(0x1462 + s * 4);
        if ((off | seg) == 0) break;            /* null terminator */
        if (s > 64) break;                      /* safety */
        u16 p = off;
        for (int k = 0; k < 13; ++k) {          /* up to 13 entries/script */
            i16 spr = GI(p + 2);                 /* t0002 */
            if (spr < 0) break;                  /* end marker */
            int w = 0, h = 0;
            ff_dir_dims((int)spr, &w, &h);       /* fn0A0D_0B97: aDEEE[spr] height */
            GI(p) = (i16)h;                      /* t0000 = sprite height */
            p += 4;
        }
    }
    /* fn0A0D_0C8F tail: `ds->w1390 <<= 1` — the missile script (@0x1390, script 4)
     * gets its FIRST threshold DOUBLED after the height patch. fn0869_1726 derives
     * the entity size t0019 = ax_24 * ptr001D->t0000 >> 6 from that first threshold,
     * so doubling it doubles the effective scale of the missile's morph selection
     * (the missile is drawn twice as large as a plain height-scaled sprite would be).
     * Proven from the decompiled fn0A0D_0C8F; without it the missile picks a mip that
     * is one step too small. */
    GI(0x1390) = (i16)(GI(0x1390) << 1);

    /* fn0A0D_0C8F final tails (ghidra @3008-3013): build the 300-byte table at
     * DGROUP 0xEF57 — bytes [0..0xB3] = i/18 (0..9), then [0x8C..0x12B] = 9
     * (the second loop overwrites the 0x8C..0xB3 overlap; "@0xEFE3" is just
     * 0xEF57+0x8C, the same table). A distance→level ramp (di/18 capped at 9),
     * but it has NO READER anywhere in either decompile (searched by name,
     * by the -0x10a9 encoding and by the 0xEF57 constant) — written-only init
     * data. Ported for byte-exact DGROUP fidelity. */
    for (int i = 0; i < 0xB4; ++i) GB(0xEF57 + i) = (u8)(i / 0x12);
    for (int i = 0x8C; i < 300; ++i) GB(0xEF57 + i) = 0x09;
}

/* ---- fn0869_1726 + fn0A0D_053E (main sprite) for one pool slot ----------- */
static void render_entity(u8 *e, int slot)
{
    i16 di = *(i16 *)(e + 3);                    /* t0003 = on-screen distance */
    if (!(di >= 0 && di < 0x100)) return;        /* fn0869_1726 guard */

    u16 ax_24 = (u16)(GB(0xE4CC + di) >> 1);     /* perspective scale */
    u16 anim  = *(u16 *)(e + 0x1D);              /* ptr001D offset (DGROUP) */
    /* explosions share the morph threshold 0x135C, which the original writes
     * per-slot in the interleaved AI+projection loop. Restore THIS slot's
     * snapshot (captured after its AI in entities_update_all) so different-parity
     * bursts pick their own mip, instead of all seeing the last explosion's value. */
    if (anim == 0x135C) GI(0x135C) = (i16)g_slot_thresh[slot];
    i16 base0 = GI(anim);                        /* ptr001D->t0000 (1st thresh) */
    /* fn0869_1726 does `mul si` (16-bit UNSIGNED multiply, keep the LOW 16 bits)
     * then `sar ax,cl` (ARITHMETIC shift of that i16) — proven by ndisasm of the
     * EXE @0869:1726 (F7E6 mul si; D3F8 sar ax,cl). So the product must be
     * TRUNCATED to 16 bits and reinterpreted as SIGNED before the >>6; my old
     * `(ax_24 * (u16)v) >> 6` shifted the full 32-bit positive int, which sent
     * a close enemy with a large negative screen_x (e.g. the RIDER at -126) far
     * off-screen so it never drew. */
    i16 t0019 = (i16)((i16)(u16)(ax_24 * (u16)base0) >> 6);
    i16 w0005 = *(i16 *)(e + 5);                 /* screen_y */
    i16 w000D = (i16)(ax_24 + 0x55);
    i16 w000B = w0005 ? (i16)(w000D - ((i16)(u16)(ax_24 * (u16)w0005) >> 6)) : w000D;
    i16 w0001 = *(i16 *)(e + 1);                 /* projected screen_x */
    i16 w0009 = (i16)(((i16)(u16)(ax_24 * (u16)w0001) >> 6) + 0xA0);  /* screen centre x */

    /* fn0A0D_053E morph walk: advance while t0019 < threshold, then take t0002.
     * If the slot's w001B (the runtime-shape base recorded into the prototype
     * by the path VM) is nonzero, the sprite index is t0002 + w001B. */
    u16 p = anim;
    for (int k = 0; k < 13 && t0019 < GI(p); ++k) p += 4;
    i16 si = GI(p + 2);                          /* t0002 sprite index */
    if (si < 0) si = GI(p - 2);                  /* tFFFE = prev entry's sprite */
    u16 base1B = *(u16 *)(e + 0x1B);             /* w001B runtime base */
    if (base1B != 0) si = (i16)(si + base1B);

    /* fn0869_1726 STORES the projection into the SLOT (ghidra FUN_1069_1726:
     * [iVar2+0x19]=t0019, [+0x0D]=w000D, [+0x0B]=w000B, [+0x09]=w0009). The
     * scratch persists in the record: ent_explosion_small/large read +0x09/
     * +0x0B on despawn for the tF085 cockpit-flash position (the LAST frame's
     * projected spot), and spawn-copies (memcpy) carry it. The port computed
     * these as locals only — the flash overlay then drew from garbage (the
     * missing mid-road explosion at capext f1370/f1750). */
    *(i16 *)(e + 0x19) = t0019;
    *(i16 *)(e + 0x0D) = w000D;
    *(i16 *)(e + 0x0B) = w000B;
    *(i16 *)(e + 0x09) = w0009;

    dl_add(w0009, w000B, (int)si, di);           /* main sprite (shape_kind 0) */

    /* shape_kind (b002E) sub-parts — fn0A0D_053E switch (jump table @120d:0620,
     * EXE file 0x34F0: kind 0->62E none, 1->633 shadow, 2->6A6, 3->6AB,
     * 4->70F, 5->714, 6->719). */
    if (e[0x2E] == 1 && si >= 0) {
        /* case 0x12703 (kind 1, the diving JUMPER / missile): a GROUND SHADOW —
         * a type-1 FILL-RECT display-list entry of colour 0xB at the ground y
         * (w000D), half-width iVar6 = (aF087[spr]>>2)+1, x [w0009-iVar6,
         * w0009+iVar6], rows [w000D-(iVar6>>3), w000D], inserted via 1FC8 with
         * the SAME key di as the main sprite (enqueued right after it). Being a
         * normal sorted entry it is COVERED by later-drawn equal/nearer sprites
         * (an immediate pre-sprite fill hid it completely — f201/f217). */
        int iVar6 = ((int)(u8)GB(0xF087 + si) >> 2) + 1;
        dl_add_box((i16)(w0009 - iVar6), w000D,
                   (i16)(w0009 + iVar6), (i16)(w000D - (iVar6 >> 3)), 0x0B, di);
    } else if (e[0x2E] >= 2 && e[0x2E] <= 6) {
        /* kinds 2..6 (convoy trucks / bosses): BLINKING TAIL-LAMP pairs + the
         * kind-3 turret (ghidra FUN_120d_053e cases 0x12776..0x127e9). Lamp
         * strip sprite = the far ptr @0xDFD2/0xDFD4 == aDEEE[57] (0xDFD2 =
         * 0xDEEE + 57*4, same aliasing as the F/K bars); pairs at
         * (w0009 +/- v, w000D-5 -/+ blink) depth di-1, v from the per-kind
         * base {2,3:0x18, 4:0x1b, 5:0x22, 6:0x27} stepping -10 while > 0x17. */
        static const i16 lamp_base[7] = { 0, 0, 0x18, 0x18, 0x1B, 0x22, 0x27 };
        if (e[0x2E] == 3) {                       /* case 0x6AB: + the TURRET */
            u8 t = GB(0x3815 + *(u16 *)(e + 0x21));   /* frame @0x3815[phase] */
            if (t != 0)
                dl_add(w0009, (i16)(w000D - 13), (int)t, (i16)(di - 1));
        }
        i16 par = (GW(0x24FF) != 0) ? 1 : -1;     /* blink parity (24FF)      */
        for (i16 v = lamp_base[e[0x2E]]; v > 0x17; v = (i16)(v - 10)) {
            dl_add((i16)(w0009 + v), (i16)(w000D - 5 - par), 57, (i16)(di - 1));
            dl_add((i16)(w0009 - v), (i16)(w000D - 5 + par), 57, (i16)(di - 1));
        }
    }
}

/* render all active entities into the display list (pass 1, behind the car). */
void render_entities(void)
{
    dl_reset();
    /* ENQUEUE ORDER PROVEN from run_level (ghidra @1624/1720/1744) and the
     * cutscene finale (@4007/4012/4059/4079): ENTITIES first (fn0DAE_0446),
     * then the rocket block (fn0A0D_082A), then the DÉCOR ring (fn0869_17B9),
     * then the draw (fn1187_1C73). Combined with the 1FC8 equal-key rule
     * (later-enqueued drawn first) this puts an entity ON TOP of a same-depth
     * décor sprite and an earlier pool slot ON TOP of a later one. (Décor
     * enqueued first + a stable sort happened to match the décor/entity case
     * but reversed the entity/entity ties — the f2020-2040 flame residual.) */
    u8 *pool = (u8 *)&G + 0xE5CC;
    for (int i = 0; i < 20; ++i) {
        u8 *e = pool + i * 0x33;
        if ((i8)e[0] < 0) continue;             /* free slot */
        if (!g_slot_drawn[i]) continue;         /* spawned mid-pass into an
                                                 * already-visited slot: the
                                                 * original's interleaved loop
                                                 * never projects it this frame */
        render_entity(e, i);
    }
    if (g_dl_extra) g_dl_extra();               /* cutscene rocket block */
    decor_ring_enqueue();                       /* fn0869_17B9 */
    dl_flush();
}
