/* enemies.c -- TYPED game data reconstructed from the original DGROUP image
 * (values byte-exact AS-IS, names/types ours). Bootstrapped once by
 * tools/gendata2.py; maintained as source. Read DIRECTLY by the engine
 * (mutable tables are seeded into typed runtime blocks by
 * ff_apply_data_tables, game_main.c). */

#include "gamedata.h"

/* the 81 enemy spawn prototypes (0x33 bytes each; the spawn memcpy
 * source). MUTABLE at runtime: the path VM records the runtime shape
 * base into +0x1B (scratch1[2..3]). Values byte-exact AS-IS. */
const Entity ffd_enemy_protos[81] = {
  /*  0 ZERO     */ { .di=3, .anim_ptr=0x0DE11288UL /*mips C*/, .hp=1, .box_halfw=0xa, .box_h=0xa },
  /*  1 UN       */ { .type_state=1, .di=3, .anim_ptr=0x0DE11390UL /*missile*/, .state_phase=0xffff, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x14, .box_h=0x14 },
  /*  2 BID FUEL */ { .type_state=2, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE112C0UL /*pickup*/, .proj_mode=1, .hp=1, .box_halfw=0x10, .box_h=0x20 },
  /*  3 BID KERO */ { .type_state=3, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE112F4UL /*pickup B*/, .proj_mode=1, .hp=1, .box_halfw=0x10, .box_h=0x20 },
  /*  4 MISSILES */ { .type_state=4, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE11328UL /*pickup C*/, .proj_mode=1, .hp=1, .box_halfw=0x10, .box_h=0x20 },
  /*  5 CINQ     */ { .type_state=5, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE1135CUL /*explosion*/, .timer=4, .proj_mode=1, .hp=1 },
  /*  6 SIX      */ { .type_state=6, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE1135CUL /*explosion*/, .proj_mode=1, .hp=1 },
  /*  7 SEPT     */ { .type_state=7, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE11328UL /*pickup C*/, .proj_mode=1, .hp=1 },
  /*  8 HUIT     */ { .type_state=8, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE11328UL /*pickup C*/, .proj_mode=1, .hp=1 },
  /*  9 NEUF     */ { .type_state=9, .z_pos=0x12c, .anim_ptr=0x0DE11390UL /*missile*/, .state_phase=0xffff, .proj_mode=1, .shape_kind=1, .hp=1 },
  /* 10 10       */ { .type_state=0xa, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE11328UL /*pickup C*/, .proj_mode=1, .hp=1 },
  /* 11 RIDER    */ { .type_state=0xb, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=1, .box_halfw=0x18, .box_h=0x26, .scratch2={0,0,50,0} },
  /* 12 SIDE CAR */ { .type_state=0xc, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=1, .box_halfw=0x1b, .box_h=0x28, .scratch2={0,0,238,2} },
  /* 13 JEEP 1   */ { .type_state=0xd, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=2, .box_halfw=0x1c, .box_h=0x1c, .scratch2={0,0,100,0} },
  /* 14 JEEP 2   */ { .type_state=0xe, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=3, .box_halfw=0x1f, .box_h=0x21, .scratch2={0,0,100,0} },
  /* 15 JEEP 3   */ { .type_state=0xf, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=2, .box_halfw=0x20, .box_h=0x20, .scratch2={0,0,100,0} },
  /* 16 MILICE1  */ { .type_state=0x10, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=3, .box_halfw=0x1f, .box_h=0x22, .scratch2={0,0,120,0} },
  /* 17 MILICE2  */ { .type_state=0x11, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=4, .box_halfw=0x19, .box_h=0x24, .scratch2={0,0,120,0} },
  /* 18 TANK1    */ { .type_state=0x12, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=2, .box_halfw=0x20, .box_h=0x21, .scratch2={0,0,150,0} },
  /* 19 TANK2    */ { .type_state=0x13, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=2, .box_halfw=0x1f, .box_h=0x24, .scratch2={0,0,150,0} },
  /* 20 TANK3    */ { .type_state=0x14, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=4, .box_halfw=0x20, .box_h=0x23, .scratch2={0,0,150,0} },
  /* 21 CHAR1    */ { .type_state=0x15, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=4, .box_halfw=0x1f, .box_h=0x22, .scratch2={0,0,200,0} },
  /* 22 CHAR2    */ { .type_state=0x16, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=6, .box_halfw=0x1f, .box_h=0x26, .scratch2={0,0,200,0} },
  /* 23 CHAR3    */ { .type_state=0x17, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=8, .box_halfw=0x20, .box_h=0x2c, .scratch2={0,0,200,0} },
  /* 24 JUMPER   */ { .type_state=0x18, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=5, .box_halfw=0x17, .box_h=0x2e, .scratch2={0,0,200,0} },
  /* 25 MIRADOR  */ { .type_state=0x19, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=1, .box_halfw=0x1a, .box_h=0x30, .scratch2={0,0,50,0} },
  /* 26 CANON1   */ { .type_state=0x1a, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=3, .box_halfw=0x1c, .box_h=0x26, .scratch2={0,0,238,2} },
  /* 27 CANON2   */ { .type_state=0x1b, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .hp=3, .box_halfw=0x21, .box_h=0x17, .scratch2={0,0,238,2} },
  /* 28 WALKER   */ { .type_state=0x1c, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=0xa, .box_halfw=0x17, .box_h=0x30, .scratch2={0,0,100,0} },
  /* 29 GUARDIAN */ { .type_state=0x1d, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x20, .box_h=0x28, .scratch2={0,0,250,0} },
  /* 30 MINE AIR */ { .type_state=0x1e, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x1e, .box_h=0x2f, .scratch2={0,0,50,0} },
  /* 31 BOMBE TH */ { .type_state=0x1f, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x18, .box_h=0x28, .scratch2={0,0,50,0} },
  /* 32 MOUCHARD */ { .type_state=0x38, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x14, .box_h=0x27, .scratch2={0,0,50,0} },
  /* 33 GARDE DC */ { .type_state=0x21, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=0x1e, .box_halfw=0x10, .box_h=0x24, .scratch2={0,0,50,0} },
  /* 34 FLYING H */ { .type_state=0x22, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=0xa, .box_halfw=0x19, .box_h=0x1a, .scratch2={0,0,150,0} },
  /* 35 SUCKER   */ { .type_state=0x23, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x18, .box_h=0x24, .scratch2={0,0,150,0} },
  /* 36 BOMBER   */ { .type_state=0x24, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=0x1e, .box_halfw=0x20, .box_h=0x1e, .scratch2={0,0,200,0} },
  /* 37 FLYING M */ { .type_state=0x25, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=4, .box_halfw=0x20, .box_h=0x23, .scratch2={0,0,100,0} },
  /* 38 POULE    */ { .type_state=0x26, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x20, .box_h=0x1e, .scratch2={0,0,50,0} },
  /* 39 EGG      */ { .type_state=0x27, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .proj_mode=1, .shape_kind=1, .hp=0xa, .box_halfw=0xf, .box_h=0x26 },
  /* 40 B AIMANT */ { .type_state=0x28, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x18, .box_h=0x24, .scratch2={0,0,50,0} },
  /* 41 JERRY    */ { .type_state=0x29, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x10, .box_h=0x1d, .scratch2={0,0,50,0} },
  /* 42 TOM      */ { .type_state=0x2a, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x18, .box_h=0x25, .scratch2={0,0,150,0} },
  /* 43 BANZAI   */ { .type_state=0x2b, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=4, .box_halfw=0x2e, .box_h=0x1e, .scratch2={0,0,100,0} },
  /* 44 POT GUN  */ { .type_state=0x2c, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=4, .box_halfw=0x1f, .box_h=0x1d, .scratch2={0,0,200,0} },
  /* 45 ECLAIR 1 */ { .type_state=0x2d, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x28, .box_h=0x1e, .scratch2={0,0,100,0} },
  /* 46 ECLAIR 2 */ { .type_state=0x2e, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=2, .box_halfw=0x28, .box_h=0x1e, .scratch2={0,0,100,0} },
  /* 47 MILICE I */ { .type_state=0x2f, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=0xc, .box_halfw=0x1f, .box_h=0x1c, .scratch2={0,0,100,0} },
  /* 48 GRIBOUY  */ { .type_state=0x30, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x1f, .box_h=0x23, .scratch2={0,0,100,0} },
  /* 49 HELICOP  */ { .type_state=0x31, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x20, .box_h=0x26, .scratch2={0,0,150,0} },
  /* 50 BOMBER 2 */ { .type_state=0x32, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=0x1e, .box_halfw=0x20, .box_h=0x25, .scratch2={0,0,250,0} },
  /* 51 SPEED B  */ { .type_state=0x33, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=2, .box_halfw=0x20, .box_h=0x1c, .scratch2={0,0,100,0} },
  /* 52 FIRE B   */ { .type_state=0x34, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=4, .box_halfw=0x20, .box_h=0x1c, .scratch2={0,0,200,0} },
  /* 53 FIERY B  */ { .type_state=0x35, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=4, .box_halfw=0x20, .box_h=0x1c, .scratch2={0,0,44,1} },
  /* 54 54       */ { .type_state=0x36, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=5, .box_halfw=0x17, .box_h=0x2e, .scratch2={0,0,200,0} },
  /* 55 55       */ { .type_state=0x37, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x14, .box_h=0x27, .scratch2={0,0,50,0} },
  /* 56 56       */ { .type_state=0x38, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x14, .box_h=0x27, .scratch2={0,0,50,0} },
  /* 57 57       */ { .type_state=0x39, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1, .box_halfw=0x18, .box_h=0x24, .scratch2={0,0,150,0} },
  /* 58 58       */ { .type_state=0x3a, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE112A4UL /*enemy shot*/, .proj_mode=1, .hp=1 },
  /* 59 59       */ { .type_state=0x3b, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .proj_mode=1, .hp=1 },
  /* 60 60       */ { .type_state=0x3c, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .proj_mode=1, .hp=1 },
  /* 61 61       */ { .type_state=0x3d, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=0xa, .scratch2={0,0,100,0} },
  /* 62 62       */ { .type_state=0x3e, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .proj_mode=1, .substate=1, .shape_kind=1, .hp=0x64 },
  /* 63 63       */ { .type_state=0x3f, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE1135CUL /*explosion*/, .state_phase=5, .proj_mode=1, .hp=1 },
  /* 64 64       */ { .type_state=0x40, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE1135CUL /*explosion*/, .proj_mode=1, .hp=1 },
  /* 65 65       */ { .type_state=0x41, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .proj_mode=1, .substate=1, .hp=1 },
  /* 66 66       */ { .type_state=0x42, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE11390UL /*missile*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1 },
  /* 67 67       */ { .type_state=0x43, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .proj_mode=1, .hp=1 },
  /* 68 EXT-LIFE */ { .type_state=0x44, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .proj_mode=1, .shape_kind=1, .hp=0xa },
  /* 69 69       */ { .type_state=0x45, .di=300, .screen_y=0x14, .vy=5, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .state_phase=1, .proj_mode=1, .substate=1, .shape_kind=1, .hp=1 },
  /* 70 70       */ { .type_state=0x46, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1 },
  /* 71 71       */ { .type_state=0x47, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1 },
  /* 72 72       */ { .type_state=0x48, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1 },
  /* 73 B CITERN */ { .type_state=0x49, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE11228UL /*mips B*/, .targetable=1, .proj_mode=1, .hp=0xf, .box_halfw=0x60, .box_h=0x93 },
  /* 74 BL MINES */ { .type_state=0x4a, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE11228UL /*mips B*/, .targetable=1, .proj_mode=1, .hp=0x19, .box_halfw=0x60, .box_h=0x78 },
  /* 75 BL RADAR */ { .type_state=0x4b, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE11228UL /*mips B*/, .targetable=1, .proj_mode=1, .hp=0x23, .box_halfw=0x64, .box_h=0x82 },
  /* 76 BL MISSI */ { .type_state=0x4c, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE11228UL /*mips B*/, .targetable=1, .proj_mode=1, .hp=0x2d, .box_halfw=0x7b, .box_h=0xc3 },
  /* 77 BL  TETE */ { .type_state=0x4d, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE11228UL /*mips B*/, .targetable=1, .proj_mode=1, .hp=0x4b, .box_halfw=0x90, .box_h=0xf0 },
  /* 78 78       */ { .type_state=0x4e, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE11228UL /*mips B*/, .targetable=1, .proj_mode=1, .hp=0x4b, .box_halfw=0x90, .box_h=0xf0 },
  /* 79 79       */ { .type_state=0x4f, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE111F8UL /*generic mips*/, .proj_mode=1, .shape_kind=1, .hp=1 },
  /* 80 80       */ { .type_state=0x50, .di=300, .z_pos=0x12c, .anim_ptr=0x0DE11228UL /*mips B*/, .targetable=1, .proj_mode=1, .shape_kind=1, .hp=1 },
};

/* morph (LOD) scripts: {threshold, sprite} pairs, negative sprite =
 * terminator. Thresholds are PATCHED at boot with sprite heights
 * (morph_patch); the explosion script thresh[0] is the AI->render
 * size channel; the missile thresh[0] is doubled at boot. */
const MorphEnt ffd_morph_11f8[12] = {   /* generic mips */
    {   44,    0 },
    {   40,    0 },
    {   36,    1 },
    {   32,    2 },
    {   28,    3 },
    {   24,    4 },
    {   20,    5 },
    {   16,    6 },
    {   12,    7 },
    {    8,    8 },
    {    4,    9 },
    {    0,   -1 },
};
const MorphEnt ffd_morph_1228[12] = {   /* mips B */
    {  150,    0 },
    {   40,    0 },
    {   36,    1 },
    {   32,    2 },
    {   28,    3 },
    {   24,    4 },
    {   20,    5 },
    {   16,    6 },
    {   12,    7 },
    {    8,    8 },
    {    4,    9 },
    {    0,   -1 },
};
const MorphEnt ffd_morph_1258[12] = {   /* firing pose */
    {   75,    0 },
    {   40,    0 },
    {   36,    1 },
    {   32,    2 },
    {   28,    3 },
    {   24,    4 },
    {   20,    5 },
    {   16,    6 },
    {   12,    7 },
    {    8,    8 },
    {    4,    9 },
    {    0,   -1 },
};
const MorphEnt ffd_morph_1288[7] = {   /* mips C */
    {  128,   38 },
    {   33,   38 },
    {   19,   39 },
    {   13,   40 },
    {   10,   41 },
    {    8,   42 },
    {    0,   -1 },
};
const MorphEnt ffd_morph_12a4[7] = {   /* enemy shot */
    {   64,   38 },
    {   33,   38 },
    {   19,   39 },
    {   13,   40 },
    {   10,   41 },
    {    8,   42 },
    {    0,   -1 },
};
const MorphEnt ffd_morph_12c0[13] = {   /* pickup */
    {    0,   26 },
    {    0,   27 },
    {    0,   28 },
    {    0,   29 },
    {    0,  236 },
    {    0,  237 },
    {    0,  238 },
    {    0,  239 },
    {    0,  240 },
    {    0,  241 },
    {    0,  242 },
    {    0,  243 },
    {    0,   -1 },
};
const MorphEnt ffd_morph_12f4[13] = {   /* pickup B */
    {    0,   30 },
    {    0,   31 },
    {    0,   32 },
    {    0,   33 },
    {    0,  252 },
    {    0,  253 },
    {    0,  254 },
    {    0,  255 },
    {    0,  256 },
    {    0,  257 },
    {    0,  258 },
    {    0,  259 },
    {    0,   -1 },
};
const MorphEnt ffd_morph_1328[13] = {   /* pickup C */
    {    0,   34 },
    {    0,   35 },
    {    0,   36 },
    {    0,   37 },
    {    0,  260 },
    {    0,  261 },
    {    0,  262 },
    {    0,  263 },
    {    0,  264 },
    {    0,  265 },
    {    0,  266 },
    {    0,  267 },
    {    0,   -1 },
};
const MorphEnt ffd_morph_135c[13] = {   /* explosion */
    {    0,   46 },
    {    0,   47 },
    {    0,   48 },
    {    0,   49 },
    {    0,  244 },
    {    0,  245 },
    {    0,  246 },
    {    0,  247 },
    {    0,  248 },
    {    0,  249 },
    {    0,  250 },
    {    0,  251 },
    {    0,   -1 },
};
const MorphEnt ffd_morph_1390[13] = {   /* missile */
    {    0,   61 },
    {    0,   62 },
    {    0,   63 },
    {    0,   64 },
    {    0,  268 },
    {    0,  269 },
    {    0,  270 },
    {    0,  271 },
    {    0,  272 },
    {    0,  273 },
    {    0,  274 },
    {    0,  275 },
    {    0,   -1 },
};
const MorphEnt ffd_morph_13c4[13] = {   /* decor 5 */
    {    0,   68 },
    {    0,   69 },
    {    0,   70 },
    {    0,   71 },
    {    0,  276 },
    {    0,  277 },
    {    0,  278 },
    {    0,  279 },
    {    0,  280 },
    {    0,  281 },
    {    0,  282 },
    {    0,  283 },
    {    0,   -1 },
};
const MorphEnt ffd_morph_13f8[13] = {   /* decor 6 */
    {    0,   72 },
    {    0,   73 },
    {    0,   74 },
    {    0,   75 },
    {    0,  284 },
    {    0,  285 },
    {    0,  286 },
    {    0,  287 },
    {    0,  288 },
    {    0,  289 },
    {    0,  290 },
    {    0,  291 },
    {    0,   -1 },
};
const MorphEnt ffd_morph_142c[13] = {   /* decor 7 */
    {    0,   76 },
    {    0,   77 },
    {    0,   78 },
    {    0,   79 },
    {    0,  292 },
    {    0,  293 },
    {    0,  294 },
    {    0,  295 },
    {    0,  296 },
    {    0,  297 },
    {    0,  298 },
    {    0,  299 },
    {    0,   -1 },
};

/* morph script directory (far ptrs, null-terminated) + tail pad */
const FarPtr ffd_morph_dir[12] = {
    { 0x12C0, 0x0DE1 },   /* pickup */
    { 0x12F4, 0x0DE1 },   /* pickup B */
    { 0x1328, 0x0DE1 },   /* pickup C */
    { 0x135C, 0x0DE1 },   /* explosion */
    { 0x1390, 0x0DE1 },   /* missile */
    { 0x13C4, 0x0DE1 },   /* decor 5 */
    { 0x13F8, 0x0DE1 },   /* decor 6 */
    { 0x142C, 0x0DE1 },   /* decor 7 */
    { 0x0000, 0x0000 },
    { 0x13C4, 0x0DE1 },   /* decor 5 */
    { 0x13F8, 0x0DE1 },   /* decor 6 */
    { 0x142C, 0x0DE1 },   /* decor 7 */
};

/* shape-composer stream directory: per enemy type -> its build
 * program in ffd_shape_streams (segment 0x0DE1 = DGROUP) */
const FarPtr ffd_composer_dir[81] = {
    { 0x0FA0, 0x0DE1 },   /*  0 ZERO */
    { 0x0F3A, 0x0DE1 },   /*  1 UN */
    { 0x0F3A, 0x0DE1 },   /*  2 BID FUEL */
    { 0x0F3A, 0x0DE1 },   /*  3 BID KERO */
    { 0x1079, 0x0DE1 },   /*  4 MISSILES */
    { 0x0F3A, 0x0DE1 },   /*  5 CINQ */
    { 0x0F3A, 0x0DE1 },   /*  6 SIX */
    { 0x0F3A, 0x0DE1 },   /*  7 SEPT */
    { 0x0F3A, 0x0DE1 },   /*  8 HUIT */
    { 0x0F3A, 0x0DE1 },   /*  9 NEUF */
    { 0x0F3A, 0x0DE1 },   /* 10 10 */
    { 0x0F3B, 0x0DE1 },   /* 11 RIDER */
    { 0x0F3D, 0x0DE1 },   /* 12 SIDE CAR */
    { 0x0F3F, 0x0DE1 },   /* 13 JEEP 1 */
    { 0x0F41, 0x0DE1 },   /* 14 JEEP 2 */
    { 0x0F43, 0x0DE1 },   /* 15 JEEP 3 */
    { 0x0F45, 0x0DE1 },   /* 16 MILICE1 */
    { 0x0F47, 0x0DE1 },   /* 17 MILICE2 */
    { 0x0F49, 0x0DE1 },   /* 18 TANK1 */
    { 0x0F4B, 0x0DE1 },   /* 19 TANK2 */
    { 0x0F4D, 0x0DE1 },   /* 20 TANK3 */
    { 0x0F4F, 0x0DE1 },   /* 21 CHAR1 */
    { 0x0F51, 0x0DE1 },   /* 22 CHAR2 */
    { 0x0F53, 0x0DE1 },   /* 23 CHAR3 */
    { 0x0F55, 0x0DE1 },   /* 24 JUMPER */
    { 0x0F84, 0x0DE1 },   /* 25 MIRADOR */
    { 0x0F9C, 0x0DE1 },   /* 26 CANON1 */
    { 0x0F9E, 0x0DE1 },   /* 27 CANON2 */
    { 0x0FA0, 0x0DE1 },   /* 28 WALKER */
    { 0x0FDF, 0x0DE1 },   /* 29 GUARDIAN */
    { 0x0FE1, 0x0DE1 },   /* 30 MINE AIR */
    { 0x0FE3, 0x0DE1 },   /* 31 BOMBE TH */
    { 0x0FFB, 0x0DE1 },   /* 32 MOUCHARD */
    { 0x102A, 0x0DE1 },   /* 33 GARDE DC */
    { 0x102C, 0x0DE1 },   /* 34 FLYING H */
    { 0x102E, 0x0DE1 },   /* 35 SUCKER */
    { 0x105D, 0x0DE1 },   /* 36 BOMBER */
    { 0x105F, 0x0DE1 },   /* 37 FLYING M */
    { 0x1061, 0x0DE1 },   /* 38 POULE */
    { 0x1063, 0x0DE1 },   /* 39 EGG */
    { 0x1065, 0x0DE1 },   /* 40 B AIMANT */
    { 0x1067, 0x0DE1 },   /* 41 JERRY */
    { 0x1069, 0x0DE1 },   /* 42 TOM */
    { 0x106B, 0x0DE1 },   /* 43 BANZAI */
    { 0x106D, 0x0DE1 },   /* 44 POT GUN */
    { 0x106F, 0x0DE1 },   /* 45 ECLAIR 1 */
    { 0x1071, 0x0DE1 },   /* 46 ECLAIR 2 */
    { 0x1073, 0x0DE1 },   /* 47 MILICE I */
    { 0x1075, 0x0DE1 },   /* 48 GRIBOUY */
    { 0x1077, 0x0DE1 },   /* 49 HELICOP */
    { 0x1079, 0x0DE1 },   /* 50 BOMBER 2 */
    { 0x1091, 0x0DE1 },   /* 51 SPEED B */
    { 0x1093, 0x0DE1 },   /* 52 FIRE B */
    { 0x1095, 0x0DE1 },   /* 53 FIERY B */
    { 0x0F3A, 0x0DE1 },   /* 54 54 */
    { 0x0F3A, 0x0DE1 },   /* 55 55 */
    { 0x0F3A, 0x0DE1 },   /* 56 56 */
    { 0x0F3A, 0x0DE1 },   /* 57 57 */
    { 0x0F3A, 0x0DE1 },   /* 58 58 */
    { 0x0F3A, 0x0DE1 },   /* 59 59 */
    { 0x0F3A, 0x0DE1 },   /* 60 60 */
    { 0x0F3A, 0x0DE1 },   /* 61 61 */
    { 0x109B, 0x0DE1 },   /* 62 62 */
    { 0x0F3A, 0x0DE1 },   /* 63 63 */
    { 0x0F3A, 0x0DE1 },   /* 64 64 */
    { 0x109D, 0x0DE1 },   /* 65 65 */
    { 0x0F3A, 0x0DE1 },   /* 66 66 */
    { 0x0F3A, 0x0DE1 },   /* 67 67 */
    { 0x1097, 0x0DE1 },   /* 68 EXT-LIFE */
    { 0x1099, 0x0DE1 },   /* 69 69 */
    { 0x0F3A, 0x0DE1 },   /* 70 70 */
    { 0x0F3A, 0x0DE1 },   /* 71 71 */
    { 0x0F3A, 0x0DE1 },   /* 72 72 */
    { 0x109F, 0x0DE1 },   /* 73 B CITERN */
    { 0x10A1, 0x0DE1 },   /* 74 BL MINES */
    { 0x10A3, 0x0DE1 },   /* 75 BL RADAR */
    { 0x10A5, 0x0DE1 },   /* 76 BL MISSI */
    { 0x10A7, 0x0DE1 },   /* 77 BL  TETE */
    { 0x10A7, 0x0DE1 },   /* 78 78 */
    { 0x0F3A, 0x0DE1 },   /* 79 79 */
    { 0x0F3A, 0x0DE1 },   /* 80 80 */
};

/* boss death break-apart {dx,dy} offsets (4 sub-explosions each) */
const XY ffd_boss73_offsets[4] = { {80,130}, {-80,130}, {120,0}, {-120,0} };   /* B CITERN */
const unsigned char ffd_boss_kind_tbl[5] = { 2,0,4,5,0 };   /* a3A3E: shape_kind by level */
const XY ffd_boss74_offsets[4] = { {80,130}, {-80,130}, {120,0}, {-120,0} };   /* BL MINES */
const XY ffd_boss77_offsets[4] = { {80,130}, {-80,130}, {120,0}, {-120,0} };   /* BL TETE */
