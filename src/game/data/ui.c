/* ui.c -- TYPED game data reconstructed from the original DGROUP image
 * (values byte-exact AS-IS, names/types ours). Bootstrapped once by
 * tools/gendata2.py; maintained as source. Read DIRECTLY by the engine
 * (mutable tables are seeded into typed runtime blocks by
 * ff_apply_data_tables, game_main.c). */

#include "gamedata.h"

/* ============================ GAME TEXTS ==================================
 * These were byte-blobs at fixed DGROUP offsets, addressed by far-ptr tables /
 * offset aliases. They are now REAL NAMED C STRINGS, used directly by the
 * renderer (no more G offsets). Embedded NULs are kept where a string carries
 * the "\001 col row" typewriter pen codes.
 * ------------------------------------------------------------------------- */

/* Objective-panel typewriter body: plain ASCII with embedded "\001 col row"
 * pen-position codes, each message NUL-terminated. The 12 messages the game
 * arms (by index) are pointers INTO this array (ffd_panel_msg below), which
 * keeps the exact original message boundaries; the panel typewriter walks the
 * bytes of the selected pointer. (The leading unused "FIRE AND FORGET II"
 * panel string @ the original 0x340A is dropped — no message references it.) */
const char ffd_panel_texts[0x121] =
    "\000"
    "\001\020\002FIRE\001\015\003AND FORGET\001\021\004II\000"
    "\001\020\002....\000"
    "\001\017\002PAUSE\000"
    "\001\020\002EXIT\000"
    "\001\017\002BEFORE\001\017\003VISUAL\001\016\004CONTACT.\000"
    "\001\016\002LANDING.\000"
    "\001\017\001READY\001\020\002FOR\001\020\003TAKE\001\020\004OFF\000"
    "\001\020\002DEMO\001\020\003MODE\000"
    "\001\017\002OUT OF\001\020\003TIME\000"
    "\001\017\002OUT OF\001\020\003FUEL\000"
    "\001\017\002LEADER\001\016\003SPOTTED.\000"
    "\001\017\001LEADER\001\020\002SHOT\001\020\003DOWN\000"
    "\001\017\002X\000"
    "\001\020\002O\000"
    "\001\020\003X\000"
    "\001\021\004O\000"
    "\001\017\004X\000"
    "\001\021\002O\000"
    "\001\021\003X\000"
    "\001\017\003O\000"
    "\001\020\002X\001\021\002O\001\021\003X\001\022\004O\001\020\004X\001\022\002O\001\022\003X\001\020\003O\000";

/* message index -> the string within ffd_panel_texts (was a far-ptr table of
 * DGROUP offsets @0x352A; PT() reproduces each offset as a real pointer). */
#define PT(off)  (ffd_panel_texts + ((off) - 0x3409))
const char *const ffd_panel_msg[12] = {
    PT(0x343D),   /*  0 BEFORE VISUAL CONTACT */
    PT(0x345B),   /*  1 LANDING */
    PT(0x3467),   /*  2 READY FOR TAKE OFF */
    PT(0x3483),   /*  3 DEMO MODE */
    PT(0x3492),   /*  4 OUT OF TIME */
    PT(0x34A3),   /*  5 OUT OF FUEL */
    PT(0x34B4),   /*  6 LEADER SPOTTED */
    PT(0x34C9),   /*  7 LEADER SHOT DOWN */
    PT(0x342C),   /*  8 PAUSE */
    PT(0x3435),   /*  9 EXIT */
    PT(0x3509),   /* 10 tic-tac-toe grid */
    PT(0x3424),   /* 11 GAME OVER (".....") */
};
#undef PT

/* the typewriter START-CLEAR eraser (was the 10-space run @0x38D7 inside the
 * HUD glyph table; drawn on 4 rows to blank the previous message). */
const char ffd_panel_clear[11] = "          ";

/* HUD banner strings (drawn as BOB glyphs by draw_text_row). */
const char ffd_txt_bonus[]     = "BONUS";
const char ffd_txt_stage[]     = "STAGE";
const char ffd_txt_completed[] = "COMPLETED";

/* Title / menu / interlude texts. For the 8x8-font strings, '[' and '/' are the
 * font's SPACE glyph (kept literally). The interlude strings are BOB glyphs. */
const char ffd_txt_deathconvoy[] = "THE[DEATH[CONVOY";
const char ffd_txt_titus1990[]   = "TITUS/1990";
const char ffd_txt_credits[]     = "CREDITS/00";
const char ffd_txt_insertcoins[] = "INSERT/COINS";
const char ffd_txt_pressbutton[] = "PRESS/BUTTON/TO/PLAY";
const char ffd_txt_spaces20[]    = "////////////////////";
const char ffd_txt_continue[]    = "CONTINUE//08";
const char ffd_txt_yougotme[]    = "YOU GOT ME";
const char ffd_txt_notdead[]     = "BUT I AM NOT DEAD";

/* VGA gameplay palette: 16 x RGB 6-bit + the colour-2 level patches */
const unsigned char ffd_pal_race[16][3] = {
    {  0,  0,  0 },
    {  0,  0,  0 },
    { 20, 24, 16 },
    { 40, 48, 52 },
    { 16, 36, 56 },
    {  0, 20,  8 },
    { 56, 56, 56 },
    { 16,  4,  0 },
    { 24,  0,  0 },
    { 36,  0,  0 },
    { 60, 48, 28 },
    { 20, 16, 12 },
    { 48,  0,  0 },
    {  0, 28,  8 },
    { 32, 28, 24 },
    { 44, 40, 36 },
};
const unsigned char ffd_pal_patch_hi[3] = { 20, 24, 16 };  /* colour 2, level >= 2 */
const unsigned char ffd_pal_patch_lo[3] = { 40, 32, 24 };  /* colour 2, level < 2 */

/* HUD gauge glyph CHAR CODE tables (drawn with the 8x8 font):
 * +0x00 weapon rows 1/2/3 + terminators, +0x11 aim-up, +0x1D fuel,
 * +0x24 orb fill, +0x25 erase/aim-down, +0x26 "          " eraser */
const unsigned char ffd_hud_glyph_tables[0x55] = {
    0x00,0x3F,0x3F,0x3A,0x3B,0x3C,0x00,0x3B,0x3C,0x3A,0x3B,0x3C,
    0x00,0x3B,0x3D,0x3E,0x3F,0x3F,0x3A,0x3B,0x3C,0x00,0x3B,0x3D,
    0x3E,0x3F,0x3F,0x3F,0x00,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
    0x40,0x3F,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
    0x00,0x43,0x4D,0x41,0x50,0x00,0x42,0x4F,0x44,0x59,0x00,0x47,
    0x41,0x4D,0x45,0x4F,0x56,0x45,0x52,0x2E,0x43,0x50,0x54,0x00,
    0x48,0x49,0x47,0x48,0x53,0x43,0x4F,0x52,0x2E,0x43,0x50,0x54,
    0x00,
};

/* high-score defaults (replaced by the HIGH file at boot) */
const HighRow ffd_high_names[6] = {
    { "1 ABC .......\000" },
    { "2 DEF .......\000" },
    { "3 GHI .......\000" },
    { "4 JKL .......\000" },
    { "5 MNO .......\000" },
    { "6 PQR .......\000" },
};
const u32 ffd_high_scores[6] = { 60, 50, 40, 30, 20, 10 };
