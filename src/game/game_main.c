/* game_main.c — gameplay bootstrap: asset loading + the one-time init chain
 * (fn0A0D_09E4 display list, fn0A0D_133B shape generator, fn0A0D_0C8F morph
 * patch, fn120d_02d3 RNG seed, fn13a8_01c9 parallax + HIGH load) + the level
 * entry. (The old shell-era weak fallbacks and game_frame() were dead code —
 * every subsystem has its faithful strong definition — and were removed.) */
#include "ff_game.h"
#include "gnames.h"
#include "../render/ff_font.h"
#include "data/wave_data.h"
#include "data/gamedata.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

GameCtx GC;

/* ---- THE ENTITY POOL (de-DGROUP'd) ----------------------------------------
 * A real typed block instead of the DGROUP byte region @0xE5CC: 20 packed
 * Entity slots (0x33 bytes each, layout asserted in ff2_types.h). Static
 * storage = zeroed at start, exactly like the original pool inside the
 * zero-initialised data segment. */
Entity g_pool[ENTITY_POOL_SLOTS];
Entity *entity_pool(void) { return g_pool; }

/* THE ENEMY-PROTOTYPE POOL (de-DGROUP'd): was the 81 x 0x33 region @0x1490.
 * MUTABLE: the path VM records each type's runtime shape base at +0x1B. */
Entity g_protos[81];

/* ---- lifecycle ---- */
/* Reset every runtime STATE BLOCK to its cold-boot value and seed the
 * data-initialised blocks from the typed SOURCE tables (game/data/*.c).
 * THE DGROUP EMULATION IS OVER: there is no G image — the wave bytecode is
 * executed from its const array (wave_data.c <- wave/levels.wave), every
 * other content table is read as named ffd_* data, and all mutable state
 * lives in the semantic blocks (gstate.h / the g_* arrays). */
void ff_apply_data_tables(void) {
    /* zero the state blocks (== the original's zero-initialised data segment;
     * boot-time call only) */
    memset(g_pool, 0, sizeof g_pool);
    memset(&g_race, 0, sizeof g_race);
    memset(&g_score, 0, sizeof g_score);
    memset(&g_panel, 0, sizeof g_panel);
    memset(&g_in, 0, sizeof g_in);
    memset(&g_tim, 0, sizeof g_tim);
    memset(&g_hud_st, 0, sizeof g_hud_st);
    memset(&g_vm, 0, sizeof g_vm);
    memset(&g_decor_st, 0, sizeof g_decor_st);
    /* data-seeded MUTABLE blocks: */
    memcpy(g_particles, ffd_particle_init, sizeof g_particles);   /* @0x35CA */
    /* the coord-table seeds (@0x2C49): the first 4 words precede the table
     * (no reader); the tail seeds the 0x84-byte table exactly. Rebuilt by
     * fn0869_15D6 before any read — kept for byte-faithful cold state. */
    memcpy(&g_road, (const u8 *)ffd_road_coord_seeds + 8, sizeof g_road);
    /* the mutable enemy-prototype pool (was @0x1490) */
    memcpy(g_protos, ffd_enemy_protos, sizeof g_protos);
    /* the composer mip list (was @0x3837) */
    memcpy(g_miplist, ffd_miplist_init, sizeof g_miplist);
    /* the flat track image (was @0x31DD..0x3408; render_world.c) */
    track_seed();
    /* the mutable morph-script bank (below) + the high-score defaults (the
     * HIGH file overwrites them at boot via ff_load_high) */
    memcpy(g_high.name, ffd_high_names, sizeof g_high.name);
    memcpy(g_high.score, ffd_high_scores, sizeof g_high.score);
    /* the mutable morph-script bank (was @0x11F8..0x1460): the 13 scripts
     * land at their original relative positions inside the bank. */
    {
        static const struct { u16 off, len; const MorphEnt *src; } ms[13] = {
            { 0x11F8, 0x30, ffd_morph_11f8 }, { 0x1228, 0x30, ffd_morph_1228 },
            { 0x1258, 0x30, ffd_morph_1258 }, { 0x1288, 0x1C, ffd_morph_1288 },
            { 0x12A4, 0x1C, ffd_morph_12a4 }, { 0x12C0, 0x34, ffd_morph_12c0 },
            { 0x12F4, 0x34, ffd_morph_12f4 }, { 0x1328, 0x34, ffd_morph_1328 },
            { 0x135C, 0x34, ffd_morph_135c }, { 0x1390, 0x34, ffd_morph_1390 },
            { 0x13C4, 0x34, ffd_morph_13c4 }, { 0x13F8, 0x34, ffd_morph_13f8 },
            { 0x142C, 0x34, ffd_morph_142c },
        };
        for (int m = 0; m < 13; m++)
            memcpy((u8 *)g_morph + (ms[m].off - MORPH_BASE), ms[m].src, ms[m].len);
    }
}

int game_init(const char *asset_dir) {
    char p[320];
    ff_apply_data_tables();

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
    Gw_mtn_scroll = 0xA0;

    /* fn13a8_12f4 (called from fn13a8_01c9, boot): LOAD the real high-score table
     * from the HIGH file. The DGROUP blob only holds placeholder defaults
     * ("1 ABC"/10..60) — the actual table lives in the HIGH data file. */
    ff_load_high(asset_dir);

    game_start_level(0);
    return 0;
}

/* fn13a8_12f4 highscore_load (@13a8:12f4). Faithful port: read the HIGH file
 * (108 bytes = 6 names + 6 32-bit scores, exactly the g_high image) via the
 * file loader fn120d_0394, then set the in-race HIGH display = the #1 entry's
 * score. The scores are NOT in the static data (rule #5: the table is filled
 * at runtime from the file); the g_high defaults are placeholders. */
void ff_load_high(const char *asset_dir) {
    char p[320];
    snprintf(p, sizeof p, "%s/HIGH", asset_dir);
    FILE *f = fopen(p, "rb");
    if (f) {
        if (fread(&g_high, 1, 0x6c, f) != 0x6c)     /* fn120d_0394: 108 B image */
            fprintf(stderr, "warning: short HIGH read (%s)\n", p);
        fclose(f);
    } else {
        fprintf(stderr, "warning: no HIGH file (%s) — using the data defaults\n", p);
    }
    Gw_high_hi = g_high.score[0].hi;                /* race HIGH = table[0].score */
    Gw_high_lo = g_high.score[0].lo;
}

/* fn13a8_0e13 tail (fn0A0D_0483 with the name "HIGH"): WRITE the high-score
 * table back to the HIGH file — the 0x6C-byte g_high image — after a name
 * entry completes. */
void ff_save_high(const char *asset_dir) {
    char p[320];
    snprintf(p, sizeof p, "%s/HIGH", asset_dir);
    FILE *f = fopen(p, "wb");
    if (!f) { fprintf(stderr, "warning: cannot write HIGH (%s)\n", p); return; }
    fwrite(&g_high, 1, 0x6C, f);
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

