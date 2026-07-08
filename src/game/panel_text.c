/* panel_text.c — faithful port of the green objective-panel typewriter.
 *
 * Source of truth:
 *   fn13a8_00a1  (Ghidra FUN_13a8_00a1 @13a8:00a1 / Reko fn0BA8_00A1 @0ba8:00a1).
 *   Drawn each frame from the in-game loop fn1069_0006 (and fn13a8_048a).
 *
 * What it does (verified from code + DGROUP + a DOSBox runtime dump used only for
 * cross-checking — all data here comes from the blob/code, none copied from the dump):
 *   - Messages are real named C strings (ffd_panel_msg[bDE50], src/game/data/ui.c;
 *     were a far-ptr table of DGROUP offsets @0x352A). Each string is text + embedded
 *     "01 col row" cursor codes, terminated by 00. e.g. msg[0] BEFORE VISUAL CONTACT =
 *       01 0F 02 "BEFORE" 01 0F 03 "VISUAL" 01 0E 04 "CONTACT." 00
 *     -> BEFORE@(120,16), VISUAL@(120,24), CONTACT.@(112,32).
 *   - State (gstate.h): Gb_panel_msg (bc60) message index; Gb_panel_state (bc62)
 *     0 idle / >0 start / <0 (0xFF) typing; Gw_panel_cursor (bc63) = the current
 *     byte cursor as the original's DGROUP-region OFFSET (kept so the tDE53 trace
 *     matches the QEMU capture) — resolved against the named ffd_panel_texts array,
 *     not read from G; Gw_pen_x/y (d5c8/d5ca) pen position in pixels.
 *   - Per frame (typing): if byte==1 -> set pen (col<<3,row<<3), skip 3; draw ONE
 *     glyph at the pen via fn1187_0232 (=ff_font_draw, colour 0), pen X+=8; if next
 *     byte==0 -> done. On start: cursor = the message's start offset, clear 4 rows.
 *   - When bDE50==0 a 5-digit counter (tF6C4>>4) is drawn at (0x78,8).
 *
 * Single-framebuffer adaptation (faithful pixels): the original types ONE glyph per
 * frame into a persistent HUD page; our render_frame repaints the HUD (BOB 25) every
 * frame, so we keep the exact per-frame STATE advance (bit-identical timing) and then
 * REDRAW the already-revealed glyphs by re-walking msg[start..cursor).  Identical
 * pixels; consistent with the rest of the chunky port (everything is rebuilt/frame).
 */
#include "ff_game.h"
#include "gnames.h"
#include "data/gamedata.h"   /* ffd_panel_texts / ffd_panel_msg / ffd_panel_clear */
#include "../render/ff_font.h"

/* The typewriter cursor (tDE53) is the original's OFFSET into the panel-text
 * region (base 0x3409) — reproduced exactly so the state trace matches QEMU —
 * but resolved against the named ffd_panel_texts array instead of G. */
#define PANEL_BASE 0x3409
/* message i's original start offset (was the far-ptr table @0x352A). */
static int msg_start(int i) { return PANEL_BASE + (int)(ffd_panel_msg[i] - ffd_panel_texts); }
/* the message byte at region offset `off`. */
static char panel_byte(int off) { return ffd_panel_texts[off - PANEL_BASE]; }

/* Arm message `idx` (faithful: bDE50=idx, bDE52=1 -> "start" next tick). */
void panel_text_set(int idx) { Gb_panel_msg = (u8)idx; Gb_panel_state = 1; }

/* run_level prologue trigger (fn1069_0006 line ~1071): bc60=0, bc62=1. */
void panel_text_reset(void) { panel_text_set(0); }

/* Advance the typewriter state by exactly one tick (port of fn13a8_00a1's body,
 * minus the drawing — drawing is rebuilt in panel_text_draw). Called once per GAME
 * ITERATION (the demo runs 2 iterations per rendered frame). */
void panel_text_advance(void)
{
    signed char st = (signed char)Gb_panel_state;
    if (st == 0) return;
    if (st < 0) {                                   /* typing */
        int p = (int)Gw_panel_cursor;               /* region offset */
        if (panel_byte(p) == 0x01) {                /* "01 col row" cursor set */
            Gw_pen_x = (u16)((i16)(i8)panel_byte(p + 1) << 3);
            Gw_pen_y = (u16)((i16)(i8)panel_byte(p + 2) << 3);
            p += 3;
        }
        p += 1;                                     /* consume the glyph byte    */
        Gw_panel_cursor = (u16)p;
        Gw_pen_x += 8;
        if (panel_byte(p) == 0x00) Gb_panel_state = 0;  /* terminator -> done        */
    } else {                                        /* start a new message       */
        Gw_panel_cursor = (u16)msg_start(Gb_panel_msg); /* -> typing (offset)        */
        Gb_panel_state = 0xFF;
        /* fn13a8_00a1 start branch: ERASE the old text — draw the 10-space
         * eraser at x=0x68 on rows y=8,16,24,32 (colour 0). Invisible in
         * run_level (render_frame repaints the HUD after), but required by
         * round_transition where the HUD page PERSISTS. */
        for (Gw_pen_y = 8; (i16)Gw_pen_y < 0x28; Gw_pen_y += 8)
            ff_font_draw(ffd_panel_clear, 0x0A, 0x68, (i16)Gw_pen_y, 0);
        Gw_pen_y = 8;
        Gw_pen_x = 0x68;
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
    int end = (int)Gw_panel_cursor;
    if (end != 0) {            /* a message is loaded — persists after typing done */
        int start = msg_start(Gb_panel_msg);
        int x = 0x68, y = 8;
        for (int p = start; p < end; ) {
            u8 c = (u8)panel_byte(p);
            if (c == 0x00) break;
            if (c == 0x01) { x = (i8)panel_byte(p + 1) << 3; y = (i8)panel_byte(p + 2) << 3; p += 3; continue; }
            char ch = (char)c;
            ff_font_draw(&ch, 1, x, y, 0);
            x += 8;
            p += 1;
        }
    }
    if (Gb_panel_msg == 0)                             /* objective counter         */
        draw_decimal(Gd_objective >> 4, 5, 0x78, 8);
}

/* Convenience: advance + draw (single-iteration use). The faithful per-frame path
 * calls panel_text_advance() per game iteration and panel_text_draw() per render. */
void panel_text_frame(void)
{
    panel_text_advance();
    panel_text_draw();
}
