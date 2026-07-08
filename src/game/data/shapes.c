/* shapes.c -- TYPED game data reconstructed from the original DGROUP image
 * (values byte-exact AS-IS, names/types ours). Bootstrapped once by
 * tools/gendata2.py; maintained as source. Read DIRECTLY by the engine
 * (mutable tables are seeded into typed runtime blocks by
 * ff_apply_data_tables, game_main.c). */

#include "gamedata.h"

/* shape-composer build programs (fn0A0D_0D08 ops):
 *   SH_NEW(h,w) new canvas (+base latch) | SH_NEW2 without latch
 *   SH_BLIT/SH_SYM/SH_MIRROR(src,x,y) blit sprite src (aDEEE idx)
 *   SH_MIPS build the scaled mip chain | SH_PAIR/SH_PAIRM(src) 2-sprite shape
 *   SH_END end of program. Referenced by ffd_composer_dir[type]. */
const unsigned char ffd_shape_streams[0x17A] = {
    /* @0x0F3A: 1 UN,2 BID FUEL,3 BID KERO,.. */
    SH_END,
    /* @0x0F3B: 11 RIDER */
    SH_PAIRM(116),
    /* @0x0F3D: 12 SIDE CAR */
    SH_PAIR(118),
    /* @0x0F3F: 13 JEEP 1 */
    SH_PAIRM(120),
    /* @0x0F41: 14 JEEP 2 */
    SH_PAIRM(122),
    /* @0x0F43: 15 JEEP 3 */
    SH_PAIRM(124),
    /* @0x0F45: 16 MILICE1 */
    SH_PAIR(126),
    /* @0x0F47: 17 MILICE2 */
    SH_PAIR(128),
    /* @0x0F49: 18 TANK1 */
    SH_PAIRM(130),
    /* @0x0F4B: 19 TANK2 */
    SH_PAIRM(132),
    /* @0x0F4D: 20 TANK3 */
    SH_PAIRM(134),
    /* @0x0F4F: 21 CHAR1 */
    SH_PAIRM(136),
    /* @0x0F51: 22 CHAR2 */
    SH_PAIRM(138),
    /* @0x0F53: 23 CHAR3 */
    SH_PAIR(140),
    /* @0x0F55: 24 JUMPER */
    SH_NEW(33,64),
    SH_SYM(144,0,9),   /* src spr 144 */
    SH_SYM(146,16,0),   /* src spr 146 */
    SH_NEW2(32,64),
    SH_SYM(145,0,10),   /* src spr 145 */
    SH_SYM(147,16,0),   /* src spr 147 */
    SH_MIPS,
    SH_NEW(45,64),
    SH_SYM(142,0,11),   /* src spr 142 */
    SH_SYM(146,16,0),   /* src spr 146 */
    SH_NEW2(42,64),
    SH_SYM(143,0,10),   /* src spr 143 */
    SH_SYM(147,16,0),   /* src spr 147 */
    SH_MIPS,
    SH_END,
    /* @0x0F84: 25 MIRADOR */
    SH_NEW(46,64),
    SH_SYM(148,16,0),   /* src spr 148 */
    SH_SYM(150,0,24),   /* src spr 150 */
    SH_NEW2(43,64),
    SH_SYM(149,16,0),   /* src spr 149 */
    SH_SYM(151,0,22),   /* src spr 151 */
    SH_MIPS,
    SH_END,
    /* @0x0F9C: 26 CANON1 */
    SH_PAIR(152),
    /* @0x0F9E: 27 CANON2 */
    SH_PAIRM(154),
    /* @0x0FA0: 0 ZERO,28 WALKER */
    SH_NEW(49,64),
    SH_BLIT(142,0,15),   /* src spr 142 */
    SH_MIRROR(144,32,15),   /* src spr 144 */
    SH_SYM(156,16,0),   /* src spr 156 */
    SH_NEW2(45,64),
    SH_BLIT(143,0,13),   /* src spr 143 */
    SH_MIRROR(145,32,13),   /* src spr 145 */
    SH_SYM(157,16,0),   /* src spr 157 */
    SH_MIPS,
    SH_NEW(49,64),
    SH_BLIT(144,0,15),   /* src spr 144 */
    SH_MIRROR(142,32,15),   /* src spr 142 */
    SH_SYM(156,16,0),   /* src spr 156 */
    SH_NEW2(45,64),
    SH_BLIT(145,0,13),   /* src spr 145 */
    SH_MIRROR(143,32,13),   /* src spr 143 */
    SH_SYM(157,16,0),   /* src spr 157 */
    SH_MIPS,
    SH_END,
    /* @0x0FDF: 29 GUARDIAN */
    SH_PAIRM(158),
    /* @0x0FE1: 30 MINE AIR */
    SH_PAIRM(160),
    /* @0x0FE3: 31 BOMBE TH */
    SH_NEW(36,64),
    SH_SYM(204,16,0),   /* src spr 204 */
    SH_SYM(162,0,17),   /* src spr 162 */
    SH_NEW2(34,64),
    SH_SYM(205,16,0),   /* src spr 205 */
    SH_SYM(163,0,18),   /* src spr 163 */
    SH_MIPS,
    SH_END,
    /* @0x0FFB: 32 MOUCHARD */
    SH_NEW(39,64),
    SH_SYM(164,16,0),   /* src spr 164 */
    SH_SYM(168,0,11),   /* src spr 168 */
    SH_NEW2(35,64),
    SH_SYM(165,16,0),   /* src spr 165 */
    SH_SYM(169,0,10),   /* src spr 169 */
    SH_MIPS,
    SH_NEW(39,64),
    SH_SYM(166,16,0),   /* src spr 166 */
    SH_SYM(168,0,11),   /* src spr 168 */
    SH_NEW2(35,64),
    SH_SYM(167,16,0),   /* src spr 167 */
    SH_SYM(169,0,10),   /* src spr 169 */
    SH_MIPS,
    SH_END,
    /* @0x102A: 33 GARDE DC */
    SH_PAIRM(170),
    /* @0x102C: 34 FLYING H */
    SH_PAIRM(172),
    /* @0x102E: 35 SUCKER */
    SH_NEW(36,64),
    SH_SYM(214,16,0),   /* src spr 214 */
    SH_SYM(176,0,16),   /* src spr 176 */
    SH_NEW2(32,64),
    SH_SYM(215,16,0),   /* src spr 215 */
    SH_SYM(177,0,15),   /* src spr 177 */
    SH_MIPS,
    SH_NEW(36,64),
    SH_SYM(174,16,0),   /* src spr 174 */
    SH_SYM(176,0,16),   /* src spr 176 */
    SH_NEW2(32,64),
    SH_SYM(175,16,0),   /* src spr 175 */
    SH_SYM(177,0,15),   /* src spr 177 */
    SH_MIPS,
    SH_END,
    /* @0x105D: 36 BOMBER */
    SH_PAIRM(178),
    /* @0x105F: 37 FLYING M */
    SH_PAIRM(180),
    /* @0x1061: 38 POULE */
    SH_PAIRM(182),
    /* @0x1063: 39 EGG */
    SH_PAIRM(184),
    /* @0x1065: 40 B AIMANT */
    SH_PAIRM(186),
    /* @0x1067: 41 JERRY */
    SH_PAIRM(188),
    /* @0x1069: 42 TOM */
    SH_PAIRM(190),
    /* @0x106B: 43 BANZAI */
    SH_PAIRM(192),
    /* @0x106D: 44 POT GUN */
    SH_PAIRM(194),
    /* @0x106F: 45 ECLAIR 1 */
    SH_PAIRM(196),
    /* @0x1071: 46 ECLAIR 2 */
    SH_PAIRM(196),
    /* @0x1073: 47 MILICE I */
    SH_PAIRM(198),
    /* @0x1075: 48 GRIBOUY */
    SH_PAIRM(200),
    /* @0x1077: 49 HELICOP */
    SH_PAIRM(202),
    /* @0x1079: 4 MISSILES,50 BOMBER 2 */
    SH_NEW(35,64),
    SH_SYM(204,16,0),   /* src spr 204 */
    SH_SYM(206,0,17),   /* src spr 206 */
    SH_NEW2(33,64),
    SH_SYM(205,16,0),   /* src spr 205 */
    SH_SYM(207,0,18),   /* src spr 207 */
    SH_MIPS,
    SH_END,
    /* @0x1091: 51 SPEED B */
    SH_PAIRM(208),
    /* @0x1093: 52 FIRE B */
    SH_PAIRM(210),
    /* @0x1095: 53 FIERY B */
    SH_PAIRM(212),
    /* @0x1097: 68 EXT-LIFE */
    SH_PAIR(216),
    /* @0x1099: 69 69 */
    SH_PAIR(220),
    /* @0x109B: 62 62 */
    SH_PAIR(222),
    /* @0x109D: 65 65 */
    SH_PAIR(224),
    /* @0x109F: 73 B CITERN */
    SH_PAIR(226),
    /* @0x10A1: 74 BL MINES */
    SH_PAIR(228),
    /* @0x10A3: 75 BL RADAR */
    SH_PAIR(230),
    /* @0x10A5: 76 BL MISSI */
    SH_PAIR(232),
    /* @0x10A7: 77 BL  TETE,78 78 */
    SH_PAIR(234),
    /* @0x10A9 raw tail */ 0x00,0x00,0x00,0x01,0x24,0x40,0x03,0x74,0x00,0x00,0x00,
};

/* decimation transform programs (rows AND columns): 01 n = copy n,
 * 02 n = replicate 1 unit n times, 03 n = skip n, 00 = restart;
 * first byte >= 4 selects the one-shot half-scale path. */
const unsigned char ffd_decimation_programs[11][0x16] = {
  /* [ 0] copy 3, skip 1, copy 2                 */ { 1,3,3,1,1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  /* [ 1] copy 1, skip 1, copy 2, skip 1, copy 2, skip 1, copy 2, skip 1, copy 2, skip 1 */ { 1,1,3,1,1,2,3,1,1,2,3,1,1,2,3,1,1,2,3,1,0,0 },
  /* [ 2] copy 2, skip 1, copy 1, skip 1, copy 1, skip 1 */ { 1,2,3,1,1,1,3,1,1,1,3,1,0,0,0,0,0,0,0,0,0,0 },
  /* [ 3] HALF-SCALE                             */ { 4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  /* [ 4] copy 1, skip 2, copy 1, skip 2, copy 1, skip 2, copy 1, skip 2, copy 1, skip 1 */ { 1,1,3,2,1,1,3,2,1,1,3,2,1,1,3,2,1,1,3,1,0,0 },
  /* [ 5] copy 1, skip 3, copy 1, skip 2         */ { 1,1,3,3,1,1,3,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  /* [ 6] copy 1, skip 4, copy 1, skip 4, copy 1, skip 3 */ { 1,1,3,4,1,1,3,4,1,1,3,3,0,0,0,0,0,0,0,0,0,0 },
  /* [ 7] copy 1, skip 6                         */ { 1,1,3,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  /* [ 8] copy 4, skip 1                         */ { 1,4,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  /* [ 9] copy 2, skip 1, copy 2, skip 1, copy 3, skip 1 */ { 1,2,3,1,1,2,3,1,1,3,3,1,0,0,0,0,0,0,0,0,0,0 },
  /* [10] copy 2, skip 1, copy 1, skip 1         */ { 1,2,3,1,1,1,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
};

/* boot decor shape generator: {src BOB sprite, transform} pairs,
 * negative src terminates (fn0A0D_133B) */
const SrcXf ffd_shapegen_pairs[69] = {
    {  26,  0 },
    {  26,  1 },
    {  26,  2 },
    {  26,  3 },
    {  26,  4 },
    {  26,  5 },
    {  26,  6 },
    {  26,  7 },
    {  47,  0 },
    {  47,  1 },
    {  47,  2 },
    {  47,  3 },
    {  47,  4 },
    {  47,  5 },
    {  47,  6 },
    {  47,  7 },
    {  30,  0 },
    {  30,  1 },
    {  30,  2 },
    {  30,  3 },
    {  30,  4 },
    {  30,  5 },
    {  30,  6 },
    {  30,  7 },
    {  34,  0 },
    {  34,  1 },
    {  34,  2 },
    {  34,  3 },
    {  34,  4 },
    {  34,  5 },
    {  34,  6 },
    {  34,  7 },
    {  61,  0 },
    {  61,  1 },
    {  61,  2 },
    {  61,  3 },
    {  61,  4 },
    {  61,  5 },
    {  61,  6 },
    {  61,  7 },
    {  69,  0 },
    {  69,  1 },
    {  69,  2 },
    {  69,  3 },
    {  69,  4 },
    {  69,  5 },
    {  69,  6 },
    {  69,  7 },
    {  73,  0 },
    {  73,  1 },
    {  73,  2 },
    {  73,  3 },
    {  73,  4 },
    {  73,  5 },
    {  73,  6 },
    {  73,  7 },
    {  77,  0 },
    {  77,  1 },
    {  77,  2 },
    {  77,  3 },
    {  77,  4 },
    {  77,  5 },
    {  77,  6 },
    {  77,  7 },
    {  -1, -1 },
    {  -1, -1 },
    { 10005, 3553 },
    { 10007, 3553 },
    { 10009, 3553 },
};

const i16 ffd_spawn_offsets[8] = { 0, 0, 0, 0, 2, 4, 6, 8 };   /* composer mip idx offsets a3827 */
const unsigned char ffd_turret_frames[0x12] = { 219,218,218,0,0,218,218,219,78,85,67,46,67,80,84,0,0,0 };   /* shape_kind-3 turret */
const u16 ffd_crash_particle_sprites[13] = { 251, 251, 250, 249, 248, 247, 246, 245, 244, 49, 48, 47, 46 };   /* by bDE6D>>1: 46..49 + mips */

/* composer mip-list init @0x3837: {src, xform} pairs + the -1 terminator.
 * The SRC halves are recomputed per build (composer_build_miplist writes
 * 0x3827[k]+base into them); the XFORM halves (programs 8,9,10,3,3,3,3,3)
 * and the terminator are STATIC — without them the mip walker never ends,
 * the GETCHAR marker never flips and the wave VM stalls (found by the
 * FF2_NO_BLOB run: JUMPERs failed to spawn at f164). */
const i16 ffd_miplist_init[17] = {
    0,8,  0,9,  0,10,  0,3,  0,3,  0,3,  0,3,  0,3,  -1,
};
