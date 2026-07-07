/* game_main.c — gameplay bootstrap: asset loading + the one-time init chain
 * (fn0A0D_09E4 display list, fn0A0D_133B shape generator, fn0A0D_0C8F morph
 * patch, fn120d_02d3 RNG seed, fn13a8_01c9 parallax + HIGH load) + the level
 * entry. (The old shell-era weak fallbacks and game_frame() were dead code —
 * every subsystem has its faithful strong definition — and were removed.) */
#include "ff_game.h"
#include "../render/ff_font.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

GameCtx GC;

/* ---- entity pool base (the 20-slot pool lives in DGROUP @ DG_ENTITIES) ---- */
Entity *entity_pool(void) { return (Entity *)((uint8_t *)&G + DG_ENTITIES); }

/* ---- lifecycle ---- */
int game_init(const char *asset_dir) {
    char p[320];
    snprintf(p, sizeof p, "%s/dgroup.bin", asset_dir);
    if (ff_load_dgroup(p) != 0) {
        /* dgroup.bin may live next to the EXE assets; try assets/ too */
        if (ff_load_dgroup("assets/dgroup.bin") != 0)
            fprintf(stderr, "warning: no dgroup.bin (%s)\n", p);
    }
    snprintf(p, sizeof p, "%s/cpt/BOB.CPT", asset_dir);
    GC.bob = ff_bob_load(p);
    if (!GC.bob) fprintf(stderr, "warning: BOB load failed (%s)\n", p);

    /* NUC.CPT = the 20-frame NUCLEAR-strike scene sheet of round_transition
     * (fn13a8_048a: mushroom cloud 0-11, rocket+flame 12-18, launcher 19),
     * loaded per level by fn0A0D_0ADB and appended at aDEEE[tEF52=300..] by
     * fn0A0D_0B35 (sprite_dir_register_bank2, called from game/cutscene.c).
     * (NOT the décor bank — that one is GENERATED, see shape_gen.c.) */
    snprintf(p, sizeof p, "%s/cpt/NUC.CPT", asset_dir);
    GC.nuc = ff_bob_load(p);
    if (!GC.nuc) fprintf(stderr, "warning: NUC load failed (%s)\n", p);

    /* décor (parallax mountains): demo = level 0 -> DECA.CPT into slot 0,
     * then fn18ef_056c builds the 7 sub-pixel shifted slots. */
    snprintf(p, sizeof p, "%s/cpt", asset_dir);
    decor_load(p, 0);

    /* HUD digit/text font (fn1187_0232 glyph block from the EXE code segment) */
    snprintf(p, sizeof p, "%s/font_ega.bin", asset_dir);
    if (ff_font_load(p) != 0) fprintf(stderr, "warning: font load failed (%s)\n", p);

    /* road track curve profile (far-data from the EXE, segment 0de1) */
    snprintf(p, sizeof p, "%s/track.bin", asset_dir);
    track_load(p);

    /* fn0A0D_09E4 (ff_display_list_init, "the init of everything", from main):
     * build the 40-entry display-list chain and register the whole sprite
     * directory aDEEE from the loaded banks (tEF52 = main count). Must run after
     * the banks load and before morph_patch (which resolves sprite heights via
     * the directory) and before the first level. */
    ff_display_list_init();            /* fn0A0D_09E4: main bank -> aDEEE[0..tEF52) */
    /* fn0A0D_133B (register_shape_list @0x2613): GENERATE the 64 roadside-décor
     * sprites by decimating 8 BOB sources through the 8 transform programs
     * @0x2521 into the tDC74 bank; registers aDEEE[236..299], tEF52 -> 300 and
     * saves tDE69 = tEF52 (main 083C:100-104). Verified vs QEMU at the first
     * run_level entry: tEF52 == 300, headers (19,32)(15,32)(14,32)(12,16). */
    shape_gen_build();
    /* sprite_dir_register_bank2() (fn0A0D_0B35) appends the NUC sheet at
     * aDEEE[tEF52=300..] — called by the round_transition cutscene
     * (game/cutscene.c) when the scene loads it; the race never draws it. */

    /* fn0A0D_0C8F: patch the morph-script thresholds with BOB sprite heights
     * (one-time, needs dgroup + BOB loaded). Entity sprite selection depends on it. */
    morph_patch();

    /* fn120d_02d3: seed the enemy PRNG once at startup (main). The pool @0xF749 is
     * BSS-zero without this, so rng16() would return 0 and enemy fire/spawn RNG
     * (MIRADOR etc.) would diverge from the reference. */
    ff_rng_seed();

    /* fn13a8_01c9 (parallax init, called ONCE from game_main at boot, NOT per level):
     * cd60 (wEF50, mountain-scroll accumulator) = 0xA0. This PERSISTS across demo
     * cycles (the run_level prologue never resets it, and the mountain keeps
     * scrolling from where the previous race left it — verified vs qemu/caprace2).
     * The cloud band accumulators c850..c858 (0xEA40..) stay BSS-zero here (the
     * fn13a8_01c9 zero loop @3783 has a `5 < iVar3` guard that never runs) and also
     * persist. Doing these per-race (as run_level_init used to) desynced cycle 2. */
    *(u16 *)((u8 *)&G + 0xEF50) = 0xA0;

    /* fn13a8_12f4 (called from fn13a8_01c9, boot): LOAD the real high-score table
     * from the HIGH file. The DGROUP blob only holds placeholder defaults
     * ("1 ABC"/10..60) — the actual table lives in the HIGH data file. */
    ff_load_high(asset_dir);

    game_start_level(0);
    return 0;
}

/* fn13a8_12f4 highscore_load (@13a8:12f4). Faithful port: read the HIGH file
 * (108 bytes = 6 names @0x27d9 [14B each] + 6 scores @0x282d [32-bit lo/hi]) into
 * the DGROUP score table via the file loader fn120d_0394, then set the in-race
 * HIGH display 27cd/27cf = the #1 entry's score (282d/282f). The scores are NOT
 * in the static DGROUP blob (rule #5: the table is filled at runtime from the
 * file); this replaces the earlier hardcoded 27cd. */
void ff_load_high(const char *asset_dir) {
    char p[320];
    u8 *g = (u8 *)&G;
    snprintf(p, sizeof p, "%s/HIGH", asset_dir);
    FILE *f = fopen(p, "rb");
    if (f) {
        if (fread(g + 0x27d9, 1, 0x6c, f) != 0x6c)   /* fn120d_0394: 108 B -> 0x27d9 */
            fprintf(stderr, "warning: short HIGH read (%s)\n", p);
        fclose(f);
    } else {
        fprintf(stderr, "warning: no HIGH file (%s) — using DGROUP defaults\n", p);
    }
    *(u16 *)(g + 0x27CF) = *(u16 *)(g + 0x282F);      /* race HIGH = table[0].score */
    *(u16 *)(g + 0x27CD) = *(u16 *)(g + 0x282D);
}

/* fn13a8_0e13 tail (fn0A0D_0483 with the name @0x3906 "HIGH"): WRITE the
 * high-score table back to the HIGH file — 0x6C bytes (6 rows of 14 name chars
 * @0x27D9 + 6 32-bit scores @0x282D) after a name entry completes. */
void ff_save_high(const char *asset_dir) {
    char p[320];
    snprintf(p, sizeof p, "%s/HIGH", asset_dir);
    FILE *f = fopen(p, "wb");
    if (!f) { fprintf(stderr, "warning: cannot write HIGH (%s)\n", p); return; }
    fwrite((u8 *)&G + 0x27D9, 1, 0x6C, f);
    fclose(f);
}

void game_start_level(int lvl) {
    GC.level = lvl;
    GC.scroll_x = 0;
    Entity *e = entity_pool();
    for (int i = 0; i < ENTITY_POOL_SLOTS; i++) e[i].type_state = 0xFF;   /* all free */

    /* Keystone level init (run_level prologue, fn0869_0006): build the runtime
     * LUTs/tables (speed table aF482, persp LUTs E4CC/DC84, shade wDE6E/aF209,
     * entity-perspective a2CB1) and zero the race state. Without this the camera
     * reads aF482==0 (no scroll) and the entity projector reads a2CB1==0 (enemies
     * off the road). See src/game/run_level.c. */
    run_level_init();
}

