/* panel_text.c — faithful port of the green objective-panel typewriter.
 *
 * Source of truth:
 *   fn13a8_00a1  (Ghidra FUN_13a8_00a1 @13a8:00a1 / Reko fn0BA8_00A1 @0ba8:00a1).
 *   Drawn each frame from the in-game loop fn1069_0006 (and fn13a8_048a).
 *
 * What it does (verified from code + DGROUP + a DOSBox runtime dump used only for
 * cross-checking — all data here comes from the blob/code, none copied from the dump):
 *   - Messages are ASCII strings in DGROUP, indexed by a far-ptr table @0x352A
 *     (msg[bDE50]).  Each string is text + embedded "01 col row" cursor codes,
 *     terminated by 00.  e.g. msg[0] @0x343D =
 *       01 0F 02 "BEFORE" 01 0F 03 "VISUAL" 01 0E 04 "CONTACT." 00
 *     -> BEFORE@(120,16), VISUAL@(120,24), CONTACT.@(112,32).
 *   - State (DGROUP):
 *       bDE50 (bc60)  message index
 *       bDE52 (bc62)  state: 0 idle, >0 start, <0 (0xFF) typing
 *       tDE53 (bc63)  current byte cursor into the message (offset; seg always DGROUP)
 *       wF7B8 (d5c8)  pen X (pixels)        wF7BA (d5ca) pen Y (pixels)
 *   - Per frame (typing): if *cursor==1 -> set pen (col<<3,row<<3), skip 3;
 *     draw ONE glyph at the pen via fn1187_0232 (=ff_font_draw, colour 0), pen X+=8;
 *     if next byte==0 -> done.  On start: load msg ptr, clear 4 rows, pen=(0x68,8).
 *   - When bDE50==0 a 5-digit counter (tF6C4>>4) is drawn at (0x78,8).
 *
 * Single-framebuffer adaptation (faithful pixels): the original types ONE glyph per
 * frame into a persistent HUD page; our render_frame repaints the HUD (BOB 25) every
 * frame, so we keep the exact per-frame STATE advance (bit-identical timing) and then
 * REDRAW the already-revealed glyphs by re-walking msg[start..cursor).  Identical
 * pixels; consistent with the rest of the chunky port (everything is rebuilt/frame).
 */
#include "ff_game.h"
#include "gmem.h"
#include "../render/ff_font.h"

/* DGROUP offsets (Ghidra _DAT_2000_XXXX -> real = XXXX+0x21F0; reko names are real) */
#define O_BC60   0xDE50   /* message index (byte)            */
#define O_BC62   0xDE52   /* state (signed byte)             */
#define O_BC63   0xDE53   /* cursor offset (word)            */
#define O_D5C8   0xF7B8   /* pen X (word)                    */
#define O_D5CA   0xF7BA   /* pen Y (word)                    */
#define O_MSGTAB 0x352A   /* far-ptr message table           */
#define O_CNT    0xF6C4   /* objective distance counter (d4d4, dword) */

/* message[i] offset within DGROUP (segment half is always the DGROUP, ignored) */
static u16 msg_off(int i) { return GW(O_MSGTAB + (u16)i * 4); }

/* Arm message `idx` (faithful: bDE50=idx, bDE52=1 -> "start" next tick). */
void panel_text_set(int idx) { GB(O_BC60) = (u8)idx; GB(O_BC62) = 1; }

/* run_level prologue trigger (fn1069_0006 line ~1071): bc60=0, bc62=1. */
void panel_text_reset(void) { panel_text_set(0); }

/* Advance the typewriter state by exactly one tick (port of fn13a8_00a1's body,
 * minus the drawing — drawing is rebuilt in panel_text_draw). Called once per GAME
 * ITERATION (the demo runs 2 iterations per rendered frame). */
void panel_text_advance(void)
{
    signed char st = (signed char)GB(O_BC62);
    if (st == 0) return;
    if (st < 0) {                                   /* typing */
        u16 p = GW(O_BC63);
        if (GB(p) == 0x01) {                        /* "01 col row" cursor set */
            GW(O_D5C8) = (u16)((i16)(i8)GB(p + 1) << 3);
            GW(O_D5CA) = (u16)((i16)(i8)GB(p + 2) << 3);
            p += 3;
        }
        p += 1;                                     /* consume the glyph byte    */
        GW(O_BC63) = p;
        GW(O_D5C8) += 8;
        if (GB(p) == 0x00) GB(O_BC62) = 0;          /* terminator -> done        */
    } else {                                        /* start a new message       */
        GW(O_BC63) = msg_off(GB(O_BC60));
        GB(O_BC62) = 0xFF;                          /* -> typing                 */
        /* fn13a8_00a1 start branch: ERASE the old text — draw the 10-space
         * string @DGROUP 0x38D7 at x=0x68 on rows y=8,16,24,32 (colour 0).
         * Invisible in run_level (render_frame repaints the HUD after), but
         * required by round_transition where the HUD page PERSISTS. */
        for (GW(O_D5CA) = 8; (i16)GW(O_D5CA) < 0x28; GW(O_D5CA) += 8)
            ff_font_draw((const char *)GPTR(0x38D7), 0x0A, 0x68, (i16)GW(O_D5CA), 0);
        GW(O_D5CA) = 8;
        GW(O_D5C8) = 0x68;
    }
}

/* zero-padded `n`-digit decimal of v, drawn at pixel (x,y) colour 0 (fn18dc_004f
 * + fn1187_0232). */
static void draw_decimal(u32 v, int n, int x, int y)
{
    char d[10];
    if (n > 10) n = 10;
    for (int i = n - 1; i >= 0; --i) { d[i] = (char)('0' + (v % 10)); v /= 10; }
    ff_font_draw(d, n, x, y, 0);
}

/* Redraw the revealed glyphs of the current message + the counter (over the HUD bg
 * already painted this frame). Called ONCE per rendered frame. */
void panel_text_draw(void)
{
    u16 end = GW(O_BC63);
    if (end != 0) {            /* a message is loaded — persists after typing done */
        u16 start = msg_off(GB(O_BC60));
        int x = 0x68, y = 8;
        for (u16 p = start; p < end; ) {
            u8 c = GB(p);
            if (c == 0x00) break;
            if (c == 0x01) { x = (i8)GB(p + 1) << 3; y = (i8)GB(p + 2) << 3; p += 3; continue; }
            char ch = (char)c;
            ff_font_draw(&ch, 1, x, y, 0);
            x += 8;
            p += 1;
        }
    }
    if (GB(O_BC60) == 0)                             /* objective counter         */
        draw_decimal(GD(O_CNT) >> 4, 5, 0x78, 8);
}

/* Convenience: advance + draw (single-iteration use). The faithful per-frame path
 * calls panel_text_advance() per game iteration and panel_text_draw() per render. */
void panel_text_frame(void)
{
    panel_text_advance();
    panel_text_draw();
}
