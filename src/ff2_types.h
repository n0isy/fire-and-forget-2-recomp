/* ff2_types.h — base types, enums and game structures for the Fire & Forget II port.
 *
 * Build fragment of ff2.h (see src/HEADER_SPEC.md). Contains:
 *   - base integer types (u8/i8/.../farptr_t);
 *   - enum GameMode, enum VmOp, enum EnemyType;
 *   - game struct/typedef: Entity, GameObject, BlitEntry, SpriteHdr,
 *     AdlibVoice, Particle, VmState.
 *
 * Does NOT contain struct Globals (that lives in ff2_globals.h).
 *
 * The game's memory-mapped structures (Entity/GameObject/BlitEntry/SpriteHdr/
 * AdlibVoice/Particle) are packed with #pragma pack(1) — they reproduce the
 * 16-bit DOS code layout byte-for-byte (fields at odd offsets). Far pointers
 * are stored as farptr_t (4 bytes) and resolved during porting.
 */
#ifndef FF2_TYPES_H
#define FF2_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Base types                                                         */
/* ------------------------------------------------------------------ */
typedef uint8_t  u8;  typedef int8_t  i8;
typedef uint16_t u16; typedef int16_t i16;
typedef uint32_t u32; typedef int32_t i32;
/* 16-bit far seg:off, stored as 4 bytes (low=off, high=seg). */
typedef uint32_t farptr_t;

/* ------------------------------------------------------------------ */
/* enum GameMode  (Globals.game_mode @F6AF, 0..4)                     */
/* ------------------------------------------------------------------ */
enum GameMode {
    GM_GROUND  = 0,  /* ground driving (fn1069_0006 switch#1 case 0)          */
    GM_FLIGHT  = 1,  /* flight (case 1)                                        */
    GM_TAKEOFF = 2,  /* takeoff ramp (case 2)                                  */
    GM_LANDING = 3,  /* landing / respawn fly-in (case 3; the DRAW switch#2    */
                     /* shares the takeoff case — jump table @1069:0baa)       */
    GM_CRASH   = 4   /* crash (case 4: particle burst, throttle drain, respawn)*/
};

/* ------------------------------------------------------------------ */
/* enum VmOp  (Level Script-VM, docs/script_vm.md)                    */
/* Opcodes 0..9 are runtime; 10..13 are assembler-only directives.    */
/* ------------------------------------------------------------------ */
enum VmOp {
    VM_CLEAR   = 0,  /* reset GETCHAR marker */
    VM_GETCHAR = 1,  /* blocking sync on marker */
    VM_PUT     = 2,  /* immediate spawn burst */
    VM_WHILE   = 3,  /* loop over remaining distance */
    VM_GOTO    = 4,  /* unconditional jump */
    VM_SETDIST = 5,  /* set 32-bit objective/scroll distance */
    VM_GOSUB   = 6,  /* call (save far return) */
    VM_RETURN  = 7,  /* return from GOSUB */
    VM_BEQ     = 8,  /* branch when dist==0 */
    VM_WAVE    = 9,  /* timed drip + clear-gate */
    VM_DEFB    = 10, /* assembler-only: raw data bytes */
    VM_END     = 11, /* assembler-only / halt (op>=10 -> VM idle) */
    VM_INCLUDE = 12, /* assembler-only: inline a sub-script */
    VM_ERREUR  = 13  /* assembler-only: parser error token */
};

/* ------------------------------------------------------------------ */
/* enum EnemyType  (meta/enemy_types.csv, indices 0..80 = 81 types)    */
/* Name = ET_<ENEMYNAME without spaces/special chars>; for unnamed     */
/* ("placeholder") types — ET_<index>. Value = index into the         */
/* template/a3A63/name table.                                          */
/* ------------------------------------------------------------------ */
enum EnemyType {
    ET_0        = 0,    /* placeholder (ent_smart_bomb) */
    ET_1        = 1,    /* placeholder (ent_homing_missile) */
    ET_BIDFUEL  = 2,    /* "BID FUEL"  (pickup_shield) */
    ET_BIDKERO  = 3,    /* "BID KERO"  (pickup_bonus_a) */
    ET_MISSILES = 4,    /* "MISSILES"  (pickup_speed) */
    ET_5        = 5,    /* placeholder (ent_explosion_small) */
    ET_6        = 6,    /* placeholder (ent_explosion_large) */
    ET_7        = 7,    /* placeholder (ent_noop) */
    ET_8        = 8,    /* placeholder (ent_noop) */
    ET_9        = 9,    /* placeholder (ent_noop) */
    ET_10       = 10,   /* placeholder (ent_noop) */
    ET_RIDER    = 11,   /* "RIDER" */
    ET_SIDECAR  = 12,   /* "SIDE CAR" */
    ET_JEEP1    = 13,   /* "JEEP 1" */
    ET_JEEP2    = 14,   /* "JEEP 2" */
    ET_JEEP3    = 15,   /* "JEEP 3" */
    ET_MILICE1  = 16,   /* "MILICE1" */
    ET_MILICE2  = 17,   /* "MILICE2" */
    ET_TANK1    = 18,   /* "TANK1" */
    ET_TANK2    = 19,   /* "TANK2" */
    ET_TANK3    = 20,   /* "TANK3" */
    ET_CHAR1    = 21,   /* "CHAR1" (char = tank) */
    ET_CHAR2    = 22,   /* "CHAR2" */
    ET_CHAR3    = 23,   /* "CHAR3" */
    ET_JUMPER   = 24,   /* "JUMPER" */
    ET_MIRADOR  = 25,   /* "MIRADOR" (watchtower) */
    ET_CANON1   = 26,   /* "CANON1" */
    ET_CANON2   = 27,   /* "CANON2" */
    ET_WALKER   = 28,   /* "WALKER" */
    ET_GUARDIAN = 29,   /* "GUARDIAN" */
    ET_MINEAIR  = 30,   /* "MINE AIR" */
    ET_BOMBETH  = 31,   /* "BOMBE TH" */
    ET_MOUCHARD = 32,   /* "MOUCHARD" (template re-types to 56) */
    ET_GARDEDC  = 33,   /* "GARDE DC" */
    ET_FLYINGH  = 34,   /* "FLYING H" */
    ET_SUCKER   = 35,   /* "SUCKER" */
    ET_BOMBER   = 36,   /* "BOMBER" */
    ET_FLYINGM  = 37,   /* "FLYING M" */
    ET_POULE    = 38,   /* "POULE" (chicken — splitter) */
    ET_EGG      = 39,   /* "EGG" */
    ET_BAIMANT  = 40,   /* "B AIMANT" (magnet bonus) */
    ET_JERRY    = 41,   /* "JERRY" */
    ET_TOM      = 42,   /* "TOM" */
    ET_BANZAI   = 43,   /* "BANZAI" */
    ET_POTGUN   = 44,   /* "POT GUN" */
    ET_ECLAIR1  = 45,   /* "ECLAIR 1" (lightning) */
    ET_ECLAIR2  = 46,   /* "ECLAIR 2" */
    ET_MILICEI  = 47,   /* "MILICE I" */
    ET_GRIBOUY  = 48,   /* "GRIBOUY" */
    ET_HELICOP  = 49,   /* "HELICOP" */
    ET_BOMBER2  = 50,   /* "BOMBER 2" */
    ET_SPEEDB   = 51,   /* "SPEED B" (bonus) */
    ET_FIREB    = 52,   /* "FIRE B" (bonus) */
    ET_FIERYB   = 53,   /* "FIERY B" (bonus) */
    ET_54       = 54,   /* placeholder (ent_basic_alias) */
    ET_55       = 55,   /* placeholder (ent_aircraft) */
    ET_56       = 56,   /* placeholder (ent_aircraft) */
    ET_57       = 57,   /* placeholder (enemy_ai_default_thunk2) */
    ET_58       = 58,   /* placeholder (enemy_update_ballistic) */
    ET_59       = 59,   /* placeholder (enemy_ai_default_thunk3) */
    ET_60       = 60,   /* placeholder (enemy_ai_default_thunk4) */
    ET_61       = 61,   /* placeholder (ent_gunship) */
    ET_62       = 62,   /* placeholder (enemy_update_enqueue_top) */
    ET_63       = 63,   /* placeholder (enemy_ai_default_thunk5) */
    ET_64       = 64,   /* placeholder (enemy_ai_default_thunk5) */
    ET_65       = 65,   /* placeholder (enemy_update_offscreen_kill) */
    ET_66       = 66,   /* placeholder (enemy_update_home_player) */
    ET_67       = 67,   /* placeholder (ent_basic_alias) */
    ET_EXTLIFE  = 68,   /* "EXT-LIFE" (extra life, pickup_weapon) */
    ET_69       = 69,   /* placeholder (enemy_update_bounce) */
    ET_70       = 70,   /* placeholder (ent_basic_alias) */
    ET_71       = 71,   /* placeholder (ent_basic_alias) */
    ET_72       = 72,   /* placeholder (ent_basic_alias) */
    ET_BCITERN  = 73,   /* "B CITERN" (boss segment) */
    ET_BLMINES  = 74,   /* "BL MINES" (boss segment) */
    ET_BLRADAR  = 75,   /* "BL RADAR" (boss segment) */
    ET_BLMISSI  = 76,   /* "BL MISSI" (boss segment) */
    ET_BLTETE   = 77,   /* "BL  TETE" (boss head, hp 75, 144x240) */
    ET_78       = 78,   /* placeholder (enemy_update_charge_trigger) */
    ET_79       = 79,   /* placeholder (enemy_ai_default_thunk6) */
    ET_80       = 80,   /* placeholder (ent_boss_strafer) */
    ET_COUNT    = 81    /* number of types (sentinel, not a game type) */
};

/* ================================================================== */
/* Game memory-mapped structures — byte-exact DOS layout              */
/* ================================================================== */
#pragma pack(push, 1)

/* small shared data shapes (used by both the typed CONTENT tables in
 * src/game/data/ and the de-DGROUP'd runtime blocks) */
typedef struct { u16 off, seg; } FarPtr;            /* 16-bit far seg:off pair  */
typedef struct { i16 thresh, sprite; } MorphEnt;    /* morph-script LOD entry   */
typedef struct { i16 kind, side, phase; FarPtr morph; } DecorProto;  /* décor
                 * prototype AND the live aDDEC ring-entry layout (10 bytes) */

/* ------------------------------------------------------------------ */
/* Entity — entity pool slot (0x33 = 51 bytes), docs/entities.md       */
/* Pool: 20 slots x 0x33 @ Globals 0xE5CC. b0000<0 -> slot is free.   */
/* Fields 0x09..0x0E, 0x19..0x1C, 0x27..0x2A are handler scratch.      */
/* ------------------------------------------------------------------ */
typedef struct {
    /*0x00*/ u8       type_state;   /* type index (EnemyType); <0 free; 5=explosion; 0xFF=despawn */
    /*0x01*/ u16      screen_x;     /* projected screen X */
    /*0x03*/ i16      di;           /* screen distance/depth (z_pos - camera), signed */
    /*0x05*/ u16      screen_y;     /* screen Y (accumulator, += vy) */
    /*0x07*/ u16      x_accum;      /* world X accumulator (+= vx) */
    /*0x09*/ u8       scratch0[6];  /* PROJECTION STORES of fn0869_1726 (FUN_1069_1726):
                                       w0009 @0x09 = projected screen centre-x,
                                       w000B @0x0B = projected bottom-y,
                                       w000D @0x0D = ground-line y (ax_24+0x55).
                                       Persist in the record — read on despawn for the
                                       tF085 cockpit-flash position, carried by copies. */
    /*0x0F*/ i16      vx;           /* X velocity */
    /*0x11*/ i16      vz;           /* depth velocity (added to z_pos) */
    /*0x13*/ i16      vy;           /* Y velocity */
    /*0x15*/ u32      z_pos;        /* 32-bit depth position (+= vz) */
    /*0x19*/ u8       scratch1[4];  /* t0019 @0x19 = projected size (fn0869_1726 store);
                                       w001B @0x1B = runtime shape base (path VM writes
                                       it into the type PROTOTYPE; sprite = t0002+w001B) */
    /*0x1D*/ farptr_t anim_ptr;     /* far pointer to animation/morph script (seg @0x1F = DGROUP) */
    /*0x21*/ u16      state_phase;  /* phase timer OR index of the locked target */
    /*0x23*/ u16      timer;        /* animation frame timer / parameter */
    /*0x25*/ u16      param;        /* parameter (speed multiplier, etc.) */
    /*0x27*/ u8       scratch2[4];  /* TODO: handler scratch 0x27..0x2A (docs say "0x26..0x2A", 0x26 taken by param) */
    /*0x2B*/ u8       targetable;   /* flag "can be locked onto / collided with" */
    /*0x2C*/ u8       proj_mode;    /* 0 = screen-anchored, !=0 = perspective via a2CB1 */
    /*0x2D*/ u8       substate;     /* sub-state flag (set at spawn) */
    /*0x2E*/ u8       shape_kind;   /* sprite kind / set of sub-parts (switch 0..6) */
    /*0x2F*/ u8       hp;           /* hit points; 0 -> death */
    /*0x30*/ u8       hit_flag;     /* flag "hit this frame" */
    /*0x31*/ u8       box_halfw;    /* collision box half-width (X) */
    /*0x32*/ u8       box_h;        /* collision box height (Y) */
} Entity;

_Static_assert(sizeof(Entity) == 0x33, "Entity must be 0x33 (51) bytes");
_Static_assert(offsetof(Entity, screen_x)   == 0x01, "Entity.screen_x @0x01");
_Static_assert(offsetof(Entity, di)         == 0x03, "Entity.di @0x03");
_Static_assert(offsetof(Entity, screen_y)   == 0x05, "Entity.screen_y @0x05");
_Static_assert(offsetof(Entity, vx)         == 0x0F, "Entity.vx @0x0F");
_Static_assert(offsetof(Entity, vz)         == 0x11, "Entity.vz @0x11");
_Static_assert(offsetof(Entity, vy)         == 0x13, "Entity.vy @0x13");
_Static_assert(offsetof(Entity, z_pos)      == 0x15, "Entity.z_pos @0x15");
_Static_assert(offsetof(Entity, anim_ptr)   == 0x1D, "Entity.anim_ptr @0x1D");
_Static_assert(offsetof(Entity, shape_kind) == 0x2E, "Entity.shape_kind @0x2E");
_Static_assert(offsetof(Entity, hp)         == 0x2F, "Entity.hp @0x2F");
_Static_assert(offsetof(Entity, box_halfw)  == 0x31, "Entity.box_halfw @0x31");
_Static_assert(offsetof(Entity, box_h)      == 0x32, "Entity.box_h @0x32");

/* ------------------------------------------------------------------ */
/* GameObject — far object record (obj_ptr[i]->...), globals_structs §2 */
/* Low-confidence layout: the decompiler yields overlapping offsets;   */
/* there are also fields at negative offsets (wFFFA/FFFC/FFFE —         */
/* frame/extent) that are NOT modeled here. Total size unknown.        */
/* ------------------------------------------------------------------ */
typedef struct {
    /*0x00*/ u8       raw00[2];     /* TODO: header (t0000 sort key, unclear) */
    /*0x02*/ u8       sort_key;     /* TODO: t0002 collision sort key (docs: i16, but 0x03=sprite_id) */
    /*0x03*/ u8       sprite_id;    /* -> sprite loader fn0BA8_1FC8 */
    /*0x04*/ u8       raw04[5];     /* TODO: 0x04..0x08 */
    /*0x09*/ i16      world_x;      /* -> blit x */
    /*0x0B*/ i16      world_y;      /* -> blit y */
    /*0x0D*/ i16      world_y2;
    /*0x0F*/ u8       raw0F[0x12];  /* TODO: 0x0F..0x20 */
    /*0x21*/ farptr_t sub;          /* ptr0021 -> sub-data (.b3815 count) */
    /*0x25*/ u8       raw25[9];     /* TODO: 0x25..0x2D */
    /*0x2E*/ u8       shape_kind;   /* switch 0..6: extra blits (turret/trail/...) */
} GameObject;

_Static_assert(offsetof(GameObject, sprite_id)  == 0x03, "GameObject.sprite_id @0x03");
_Static_assert(offsetof(GameObject, world_x)    == 0x09, "GameObject.world_x @0x09");
_Static_assert(offsetof(GameObject, sub)        == 0x21, "GameObject.sub @0x21");
_Static_assert(offsetof(GameObject, shape_kind) == 0x2E, "GameObject.shape_kind @0x2E");

/* ------------------------------------------------------------------ */
/* BlitEntry — draw-queue element (Globals.blit_list @0xDBC8)          */
/* globals_structs §3. Total stride not confirmed (>= 0x26).          */
/* ------------------------------------------------------------------ */
typedef struct {
    /*0x00*/ i16      x;
    /*0x02*/ i16      y;
    /*0x04*/ u8       raw04[4];     /* TODO: 0x04..0x07 */
    /*0x08*/ u16      spr_lo;       /* sprite/shape pointer (low) */
    /*0x0A*/ u16      spr_hi;       /* sprite/shape pointer (high) */
    /*0x0C*/ u8       raw0C[6];     /* TODO: 0x0C..0x11 */
    /*0x12*/ farptr_t ds_ref;       /* back-reference into DGROUP */
    /*0x16*/ u16      shape_id;
    /*0x18*/ u16      flags;
    /*0x1A*/ u8       raw1A[8];     /* TODO: 0x1A..0x21 */
    /*0x22*/ i16      x2;           /* secondary blit (shadow/trail) */
    /*0x24*/ i16      y2;
} BlitEntry;

_Static_assert(offsetof(BlitEntry, spr_lo)   == 0x08, "BlitEntry.spr_lo @0x08");
_Static_assert(offsetof(BlitEntry, ds_ref)   == 0x12, "BlitEntry.ds_ref @0x12");
_Static_assert(offsetof(BlitEntry, shape_id) == 0x16, "BlitEntry.shape_id @0x16");
_Static_assert(offsetof(BlitEntry, flags)    == 0x18, "BlitEntry.flags @0x18");
_Static_assert(offsetof(BlitEntry, x2)       == 0x22, "BlitEntry.x2 @0x22");

/* ------------------------------------------------------------------ */
/* SpriteHdr — BOB sprite header (docs/bob_format.md)                 */
/* In the bank the engine keeps a pointer to the PIXELS (planes); the */
/* dimensions are read as [ptr-4]=H, [ptr-2]=W. This struct describes */
/* the full record: a 4-byte header + 4 EGA 4bpp planes in a row,     */
/* each (W/8)*H bytes. planes[] is a flexible array member.           */
/* ------------------------------------------------------------------ */
typedef struct {
    /*-0x04*/ u16 height;      /* H, rows    ([ptr-4]) */
    /*-0x02*/ u16 width;       /* W, pixels  ([ptr-2]); multiple of 16 */
    /* 0x00*/ u8  planes[];    /* 4 EGA planes, each (W/8)*H bytes, planar 4bpp */
} SpriteHdr;

/* ------------------------------------------------------------------ */
/* AdlibVoice — AdLib/OPL2 voice (0x12 = 18 bytes), docs/sound_format.md*/
/* One per active channel; the array is walked every ISR tick (di += 0x12).*/
/* ------------------------------------------------------------------ */
typedef struct {
    /*0x00*/ u8  dur_shift;     /* note-length shift: ticks = 0x60 >> this; 7 -> use alt_shift */
    /*0x01*/ u8  volume;        /* level scaler -> OPL reg 0x40 */
    /*0x02*/ u8  param2;        /* TODO: scratch (song cmd 2), exact semantics unclear */
    /*0x03*/ u8  alt_shift;     /* alt note-length shift (when dur_shift==7) */
    /*0x04*/ u8  gate;          /* key-on duration / gate; copied into prev_gate */
    /*0x05*/ u8  opl_channel;   /* OPL channel 0..8 */
    /*0x06*/ u8  cur_note;      /* current note-table index (note_index+0x0F) */
    /*0x07*/ u8  dur_counter;   /* ticks remaining for the current note */
    /*0x08*/ u8  note_lo;       /* low nibble of event -> index into F-number table D1D8 */
    /*0x09*/ u8  block;         /* high nibble -> OPL block/octave */
    /*0x0A*/ u16 return_ptr;    /* GOSUB/return address in song-stream */
    /*0x0C*/ u8  loop_count;    /* LOOP counter (DJNZ) */
    /*0x0D*/ u16 stream_ptr;    /* song-stream cursor; 0xFFFF = voice inactive/end */
    /*0x0F*/ u8  prev_note_lo;  /* previous note_lo (legato / re-key) */
    /*0x10*/ u8  prev_gate;     /* previous gate (re-key) */
    /*0x11*/ u8  keystate;      /* note on/off (1 = key on) */
} AdlibVoice;

_Static_assert(sizeof(AdlibVoice) == 0x12, "AdlibVoice must be 0x12 (18) bytes");
_Static_assert(offsetof(AdlibVoice, return_ptr) == 0x0A, "AdlibVoice.return_ptr @0x0A");
_Static_assert(offsetof(AdlibVoice, stream_ptr) == 0x0D, "AdlibVoice.stream_ptr @0x0D");
_Static_assert(offsetof(AdlibVoice, keystate)   == 0x11, "AdlibVoice.keystate @0x11");

/* ------------------------------------------------------------------ */
/* Particle — particle ring (10 bytes), update_particles (FUN_1069_13c0)*/
/* Init array @ DGROUP 0x35CA; elements (vx,x,vy,y,v0).                */
/* ------------------------------------------------------------------ */
typedef struct {
    /*0x00*/ i16 vx;   /* X velocity */
    /*0x02*/ i16 x;    /* X position */
    /*0x04*/ i16 vy;   /* Y velocity (init: -1,-3,-7,-11,...) */
    /*0x06*/ i16 y;    /* Y position */
    /*0x08*/ i16 v0;   /* TODO: base velocity / parameter */
} Particle;

_Static_assert(sizeof(Particle) == 10, "Particle must be 10 bytes");

#pragma pack(pop)

/* (the old speculative "VmState logical aggregation" typedef is gone: the
 *  REAL de-DGROUP'd VM state struct lives in game/gstate.h) */

#endif /* FF2_TYPES_H */
