/* dgroup.c — THE HOME OF THE GAME STATE BLOCKS.
 *
 * HISTORY: this file used to define `struct Globals G` — the byte-exact image
 * of the original 16-bit engine's single 64KB DGROUP data segment, which the
 * whole port addressed by offset. The de-DGROUP migration moved everything
 * out: the entity pool, the runtime LUTs, all scalar state (the semantic
 * blocks below), the runtime arrays (ring/particles/coords/sprite directory)
 * and finally the content fetches (wave bytecode executed from its const
 * array; every table read as named ffd_* data). G IS GONE — the DGROUP
 * emulation is over. The original field homes survive as comments in
 * gstate.h/gnames.h for cross-referencing the decompile.
 */
#include "ff_game.h"
#include "gstate.h"

/* the de-DGROUP'd semantic state blocks (gstate.h). Static storage = zeroed
 * at start, exactly like the original's zero-initialised data segment. */
RaceState   g_race;
ScoreState  g_score;
PanelState  g_panel;
InputState  g_in;
TimingState g_tim;
HudState    g_hud_st;
VmState     g_vm;
DecorState  g_decor_st;

/* the mutable morph-script bank (gstate.h; seeded from the ffd_morph_* data) */
MorphEnt g_morph[MORPH_N];
/* the composer mip list (gstate.h; seeded from ffd_miplist_init) */
i16 g_miplist[17];
/* the high-score table (gstate.h; the HIGH file's 0x6C-byte image) */
HighTable g_high;
