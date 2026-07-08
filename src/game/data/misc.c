/* misc.c -- TYPED game data reconstructed from the original DGROUP image
 * (values byte-exact AS-IS, names/types ours). Bootstrapped once by
 * tools/gendata2.py; maintained as source. Read DIRECTLY by the engine
 * (mutable tables are seeded into typed runtime blocks by
 * ff_apply_data_tables, game_main.c). */

#include "gamedata.h"

/* player car sprite table a3687: sprite id = [row][banking column],
 * rows: 0-2 flight (by altitude), 3 takeoff/landing, 4 ground */
const i16 ffd_car_sprites[5][16] = {
    {  16,  16,  16,  10,  10,  10,  10,  13,  13,  13,  19,  20,   0,   0,   0,   0 },
    {  15,  15,  15,   9,   9,   9,   9,  12,  12,  12,  17,  18,   0,   0,   0,   0 },
    {  14,  14,  14,   8,   8,   8,   8,  11,  11,  11,  17,  18,   0,   0,   0,   0 },
    {   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   0,   0,   0,   0,   0,   0 },
    {   3,   2,   1,   1,   0,   0,   4,   4,   5,   6,   0,   0,   0,   0,   0,   0 },
};
const i16 ffd_car_tail_a3727[10] = { 6, 4, 2, 1, 0, 0, -1, -2, -4, -6 };   /* words after the grid; reader unproven */

/* twin exhaust / dust offsets per car frame: {dxL,dyL,dxR,dyR} */
const signed char ffd_exhaust_offsets[21][4] = {
    {  -24,   -9,   24,   -9 },   /* frame 0 */
    {  -24,  -10,   24,   -9 },   /* frame 1 */
    {  -24,  -10,   16,   -8 },   /* frame 2 */
    {  -24,  -10,   12,   -9 },   /* frame 3 */
    {  -20,   -9,   28,  -10 },   /* frame 4 */
    {  -16,   -8,   28,  -10 },   /* frame 5 */
    {   -8,   -9,   28,  -11 },   /* frame 6 */
    {  -21,  -12,   21,  -12 },   /* frame 7 */
    {  -21,   -8,   21,   -8 },   /* frame 8 */
    {  -21,   -8,   21,   -8 },   /* frame 9 */
    {  -21,  -14,   21,  -14 },   /* frame 10 */
    {  -19,   -9,   22,   -9 },   /* frame 11 */
    {  -19,   -9,   22,   -9 },   /* frame 12 */
    {  -19,  -14,   22,  -14 },   /* frame 13 */
    {  -22,   -9,   19,   -9 },   /* frame 14 */
    {  -22,   -9,   19,   -9 },   /* frame 15 */
    {  -22,  -14,   19,  -14 },   /* frame 16 */
    {  -13,   -8,   24,  -25 },   /* frame 17 */
    {  -21,  -25,   13,   -8 },   /* frame 18 */
    {  -13,  -10,   23,  -27 },   /* frame 19 */
    {  -21,  -26,   13,   -9 },   /* frame 20 */
};
const signed char ffd_exhaust_wobble[16] = { 5, 4, 2, 0, 5, 4, 3, 1, 5, 3, 1, 0, 5, 4, 2, 0 };   /* Y flicker by phase */

/* crash particle ring init: 8 x {vx, x, vy, y, v0} */
const Particle ffd_particle_init[8] = {
    {    2,    0,    9,    0,    9 },
    {    4,    0,    5,    0,    5 },
    {    8,    0,   18,    0,   12 },
    {   10,    0,    8,    0,    8 },
    {   -1,    0,    2,    0,    2 },
    {   -3,    0,    8,    0,    8 },
    {   -7,    0,   10,    0,   10 },
    {  -11,    0,   16,    0,   14 },
};

/* roadside decor prototypes: kind>0x0D full scale else half;
 * side>0 left of road (shift), <=0 right; morph = decor script ptr */
const DecorProto ffd_decor_protos[16] = {
    {  1, -1,  0, {0x13C4,0x0DE1} },
    {  2,  2,  0, {0x13F8,0x0DE1} },
    {  3, -2,  0, {0x13C4,0x0DE1} },
    {  4,  1,  0, {0x13C4,0x0DE1} },
    {  5, -3,  0, {0x13C4,0x0DE1} },
    {  6,  2,  0, {0x13F8,0x0DE1} },
    {  7, -2,  0, {0x13F8,0x0DE1} },
    {  8,  3,  0, {0x13C4,0x0DE1} },
    {  9, -1,  0, {0x13C4,0x0DE1} },
    { 10,  2,  0, {0x13F8,0x0DE1} },
    { 11, -2,  0, {0x13C4,0x0DE1} },
    { 12,  1,  0, {0x13C4,0x0DE1} },
    { 13, -3,  0, {0x13C4,0x0DE1} },
    { 14,  0,  0, {0x142C,0x0DE1} },
    { 15,  0,  0, {0x142C,0x0DE1} },
    { 16,  1,  0, {0x142C,0x0DE1} },
};

const u16 ffd_rng_seed[55] = {   /* additive-Fibonacci seed pool */
    0x6A71,0x9020,0xF2D0,0xD0A5,0xBECD,0x2459,0x43FE,0x1BBE,
    0x7E02,0x5221,0x60F1,0xB59C,0x50F7,0x461D,0x4C9B,0xE1DC,
    0x4AB1,0xB5D5,0x5614,0xBBC7,0xDEDF,0x0606,0x8A7D,0x712D,
    0x2AFC,0xDC9B,0xD1DE,0xDAF1,0xABA7,0x6BF7,0xB32D,0x18F7,
    0x3E22,0x9509,0x63C7,0xF6D0,0x2CA1,0x026F,0x37F5,0x06EE,
    0x9F5F,0x5185,0x52F0,0x738C,0x6189,0xC34E,0x8C02,0x9422,
    0x4F01,0x1902,0x3F1C,0xAB76,0x6787,0x4D15,0xECDF,
};

/* the attract-demo input recording: {frames, control mask} RLE pairs.
 * mask: 01 fire 02 aux 04 right 08 left 10 accel 20 brake 40 missile.
 * BYTE-EXACT VERBATIM: the bit-exact demo depends on it. */
const RlePair ffd_demo_tape[217] = {
    { 19,0x00}, {  7,0x10}, {  2,0x18}, {  3,0x10}, {  4,0x14}, {  0,0x10},
    {  6,0x18}, {  1,0x10}, {  5,0x14}, { 10,0x10}, {  1,0x18}, { 65,0x10},
    { 41,0x00}, {  2,0x04}, {  7,0x00}, {  8,0x01}, {  0,0x43}, {  1,0x03},
    { 12,0x01}, {  0,0x43}, {  1,0x03}, {  7,0x01}, {  5,0x00}, {  4,0x10},
    {  1,0x18}, { 15,0x10}, {  0,0x18}, { 70,0x10}, {  2,0x00}, {  8,0x01},
    {  1,0x09}, {  9,0x01}, {  1,0x05}, { 13,0x01}, {  3,0x05}, { 29,0x01},
    {  3,0x09}, { 12,0x01}, {  4,0x05}, {  7,0x01}, {  1,0x09}, { 11,0x01},
    {  1,0x09}, { 15,0x01}, {  1,0x08}, {  5,0x00}, {  2,0x04}, { 19,0x00},
    {  1,0x08}, { 48,0x00}, {  0,0x08}, {  6,0x00}, {  2,0x08}, { 53,0x00},
    {  0,0x10}, { 10,0x00}, {  1,0x10}, { 98,0x00}, {  6,0x04}, { 22,0x00},
    {  2,0x10}, {163,0x00}, {  0,0x08}, {  1,0x09}, { 39,0x01}, {  0,0x00},
    {  0,0x04}, {  6,0x00}, {  0,0x04}, {  5,0x00}, {  1,0x08}, { 15,0x00},
    {  1,0x10}, { 76,0x00}, {  2,0x08}, {243,0x00}, {  0,0x82}, {  2,0x02},
    {  2,0x00}, {  7,0x20}, {  2,0x04}, {  6,0x01}, {  2,0x05}, {  8,0x01},
    {  2,0x09}, {  7,0x01}, {  7,0x00}, {  1,0x04}, { 13,0x00}, {  2,0x08},
    {  2,0x00}, {  0,0x82}, {  1,0x02}, { 14,0x00}, {  6,0x10}, { 35,0x00},
    {  2,0x04}, { 16,0x01}, {  1,0x05}, {  3,0x01}, {  1,0x09}, {  5,0x01},
    {  1,0x09}, { 13,0x01}, {  2,0x05}, { 10,0x01}, {  0,0x05}, { 14,0x01},
    {  1,0x09}, {  0,0x08}, {  4,0x00}, {  6,0x01}, {  2,0x09}, { 12,0x01},
    {  2,0x05}, { 18,0x01}, {  0,0x09}, {  4,0x01}, {  1,0x05}, {  0,0x01},
    {  2,0x09}, {  6,0x01}, {  2,0x09}, { 17,0x01}, {  1,0x05}, {111,0x01},
    {  5,0x05}, { 12,0x01}, {103,0x00}, {  0,0x82}, {  1,0x02}, {  4,0x00},
    {  9,0x20}, {  8,0x00}, {  2,0x10}, {  2,0x00}, {  4,0x01}, {  0,0x43},
    {  0,0x03}, {  6,0x01}, {  9,0x00}, {  0,0x82}, {  0,0x02}, { 11,0x00},
    {  3,0x08}, {  0,0x00}, {  5,0x01}, {  2,0x05}, { 14,0x01}, {  1,0x05},
    {  9,0x01}, {  1,0x09}, {  7,0x01}, {  0,0x09}, { 24,0x01}, {  0,0x43},
    {  3,0x01}, { 17,0x00}, {  0,0x09}, {  2,0x01}, { 19,0x00}, {  4,0x10},
    { 32,0x00}, {  0,0x08}, {106,0x00}, {  1,0x08}, { 30,0x00}, {  2,0x10},
    {  2,0x00}, {  2,0x08}, { 18,0x00}, {  0,0x82}, {  3,0x02}, {  3,0x00},
    {  5,0x20}, {  6,0x00}, {  1,0x10}, { 10,0x00}, {  8,0x01}, {  3,0x00},
    {  0,0x04}, {  7,0x00}, {  1,0x01}, {  0,0x43}, {  0,0x03}, {  7,0x01},
    {  6,0x00}, {  0,0x82}, {  0,0x02}, { 11,0x00}, {  2,0x10}, { 17,0x00},
    {  2,0x10}, { 48,0x00}, { 28,0x01}, {  0,0x09}, {  4,0x01}, {  0,0x09},
    {  3,0x01}, {  1,0x05}, {  2,0x01}, {  0,0x05}, {  2,0x01}, {  1,0x09},
    {  1,0x01}, {  0,0x09}, {  1,0x01}, {  1,0x09}, {  2,0x01}, {  1,0x05},
    {  6,0x01}, {  4,0x00}, {  0,0x04}, {  1,0x00}, { 12,0x20}, {139,0x00},
    {  0,0x00},
};

const u16 ffd_game_consts[2] = { 1, 65280 };   /* w2845 throttle step + pad */
