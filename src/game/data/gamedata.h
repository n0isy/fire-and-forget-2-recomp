/* gamedata.h -- TYPED game data reconstructed from the original DGROUP image
 * (values byte-exact AS-IS, names/types ours). Bootstrapped once by
 * tools/gendata2.py; maintained as source. The engine reads these tables
 * DIRECTLY (no DGROUP image exists anymore); the mutable ones (morph bank,
 * prototypes, mip list, track, particles, coords, high table) are seeded
 * into their typed runtime blocks by ff_apply_data_tables (game_main.c). */

#ifndef FF_GAMEDATA_H
#define FF_GAMEDATA_H
#include "../../ff2_types.h"

#pragma pack(push, 1)
/* (FarPtr / MorphEnt / DecorProto moved to ff2_types.h — shared with the
 *  de-DGROUP'd runtime blocks) */
typedef struct { i16 src, xform; } SrcXf;
typedef struct { i16 dx, dy; } XY;
typedef struct { char row[14]; } HighRow;
typedef struct { u8 frames, mask; } RlePair;
#pragma pack(pop)

/* shape-composer op macros (fn0A0D_0D08 opcodes) */
#define SH_END          0
#define SH_NEW(h,w)     1,(h),(w)
#define SH_NEW2(h,w)    8,(h),(w)
#define SH_BLIT(s,x,y)  2,(s),(x),(y)
#define SH_SYM(s,x,y)   3,(s),(x),(y)
#define SH_MIRROR(s,x,y) 7,(s),(x),(y)
#define SH_MIPS         4
#define SH_PAIR(s)      5,(s)
#define SH_PAIRM(s)     6,(s)

/* (the FFDataRegion manifest is GONE: nothing copies data into a DGROUP image
 *  anymore — the engine reads these tables DIRECTLY and the mutable ones are
 *  seeded into their typed blocks by ff_apply_data_tables.) */

extern const Entity ffd_enemy_protos[81];
extern const MorphEnt ffd_morph_11f8[12];
extern const MorphEnt ffd_morph_1228[12];
extern const MorphEnt ffd_morph_1258[12];
extern const MorphEnt ffd_morph_1288[7];
extern const MorphEnt ffd_morph_12a4[7];
extern const MorphEnt ffd_morph_12c0[13];
extern const MorphEnt ffd_morph_12f4[13];
extern const MorphEnt ffd_morph_1328[13];
extern const MorphEnt ffd_morph_135c[13];
extern const MorphEnt ffd_morph_1390[13];
extern const MorphEnt ffd_morph_13c4[13];
extern const MorphEnt ffd_morph_13f8[13];
extern const MorphEnt ffd_morph_142c[13];
extern const FarPtr ffd_morph_dir[12];
extern const FarPtr ffd_composer_dir[81];
extern const XY ffd_boss73_offsets[4];
extern const unsigned char ffd_boss_kind_tbl[5];
extern const XY ffd_boss74_offsets[4];
extern const XY ffd_boss77_offsets[4];
extern const unsigned char ffd_shape_streams[0x17A];
extern const unsigned char ffd_decimation_programs[11][0x16];
extern const SrcXf ffd_shapegen_pairs[69];
extern const i16 ffd_spawn_offsets[8];
extern const unsigned char ffd_turret_frames[0x12];
extern const u16 ffd_crash_particle_sprites[13];
extern const i16 ffd_miplist_init[17];   /* DGROUP 0x3837 */
extern const unsigned char ffd_persp_ramp[256];
extern const i16 ffd_sine_lut[256];
extern const i16 ffd_cosine_lut[256];
extern const FarPtr ffd_wave_mid_ptrs[2];
extern const signed char ffd_track_31dd[33];
extern const signed char ffd_track_31fe[48];
extern const signed char ffd_track_322e[78];
extern const signed char ffd_track_327c[108];
extern const signed char ffd_track_32e8[48];
extern const signed char ffd_track_3318[78];
extern const signed char ffd_track_3366[108];
extern const FarPtr ffd_track_seg_ptrs[12];
extern const unsigned char ffd_track_tbl_tail[7];
extern const u16 ffd_road_coord_seeds[70];
extern const char ffd_panel_texts[0x121];      /* objective-panel typewriter body */
extern const char *const ffd_panel_msg[12];    /* message index -> string (was a far-ptr table) */
extern const char ffd_panel_clear[11];         /* typewriter start-clear (10 spaces) */
extern const char ffd_txt_bonus[], ffd_txt_stage[], ffd_txt_completed[];
extern const char ffd_txt_deathconvoy[], ffd_txt_titus1990[], ffd_txt_credits[];
extern const char ffd_txt_insertcoins[], ffd_txt_pressbutton[], ffd_txt_spaces20[];
extern const char ffd_txt_continue[], ffd_txt_yougotme[], ffd_txt_notdead[];
extern const unsigned char ffd_pal_race[16][3];
extern const unsigned char ffd_pal_patch_hi[3];
extern const unsigned char ffd_pal_patch_lo[3];
extern const unsigned char ffd_hud_glyph_tables[0x55];
extern const HighRow ffd_high_names[6];
extern const u32 ffd_high_scores[6];
extern const i16 ffd_car_sprites[5][16];
extern const i16 ffd_car_tail_a3727[10];
extern const signed char ffd_exhaust_offsets[21][4];
extern const signed char ffd_exhaust_wobble[16];
extern const Particle ffd_particle_init[8];
extern const DecorProto ffd_decor_protos[16];
extern const u16 ffd_rng_seed[55];
extern const RlePair ffd_demo_tape[217];
extern const u16 ffd_game_consts[2];

#endif
