#ifndef FF2_TABLES_H
#define FF2_TABLES_H
/* ff2_tables.h — static DGROUP tables and numeric constants (Fire & Forget II).
 *
 * Data sources: docs/sound_format.md, docs/entities.md, meta/enemy_types.csv,
 * docs/script_vm.md, docs/bob_format.md, docs/asset_pipeline.md, intermediate/globals_structs.md.
 *
 * All offsets are DGROUP-relative (Reko `ds:` offset). The whole DGROUP is loaded into
 * `struct Globals G` (declared in ff2_globals.h) as a byte-exact blob of the 16-bit image,
 * so the tables are accessed via `(type*)((u8*)&G + DG_xxx ...)` accessors.
 *
 * Include order: platform.h -> ff2_types.h -> ff2_globals.h -> ff2_tables.h -> ...
 * (accessors use `G` from ff2_globals.h; they expand only at the call sites.)
 */
#include "ff2_types.h"

/* =========================================================================
 * 1. Offsets of key tables within DGROUP
 * =========================================================================
 * Calibration: DGROUP image-base seg = 0x0DE1 (Ghidra 0x1DE1); file_off = linear - 0xF200;
 *             DGROUP file base = 0xEC10. (docs/entities.md, docs/asset_pipeline.md)
 */

/* --- Level Script-VM (docs/script_vm.md) --- */
#define DG_SCRIPT_DIR        0x0094  /* script directory: 6 word start offsets             */
#define DG_BYTECODE          0x00A0  /* bytecode base; script i = DG_BYTECODE + dir[i]      */

/* --- Enemies / entities (docs/entities.md, meta/enemy_types.csv) --- */
#define DG_ENEMY_NAMES       0x0CB2  /* enemy names: 81 fixed-width 8-byte strings          */
#define DG_ENEMY_PROTOS      0x1490  /* slot templates: 81 records of 0x33 bytes            */
#define DG_BEHAVIOR_TBL      0x3A63  /* a3A63: 81 far pointers (stride 4) to seg 161c handlers */
#define DG_ENTITIES          0xE5CC  /* pool of active entities: 20 slots of 0x33           */
#define DG_ENEMY_RING        0x33D2  /* enemy_list: ring buffer of enemy pointers           */

/* --- Engine lookup tables (intermediate/globals_structs.md) --- */
#define DG_COORD_TBL         0x2C51  /* coord_table: [i]=x0 [i+0x10]=x1 [i+0x20]=y          */
#define DG_PERSP_TBL         0x2CB1  /* a2CB1: perspective scale over 16 depth levels        */
#define DG_SINE              0x2DD5  /* sine_table[256] (i16); idx = scroll_accum & 0xFF     */
#define DG_PARTICLE_INIT     0x35CA  /* 8-object init array of particle rings (update_particles) */
#define DG_RAMP_TBL          0xDE6E  /* ramp_table: ramp_table[i] = i+1                      */
#define DG_SPEED_TBL         0xF482  /* speed_table -> scroll_speed                          */

/* --- Asset names (.CPT) (docs/asset_pipeline.md) --- */
#define DG_ASSET_NAMES       0x361A  /* first cluster of "<NAME>.CPT" strings (TITUS.CPT @ds:0x361A) */

/* --- Sound: AdLib/OPL2 + PC speaker (docs/sound_format.md §6) --- */
#define DG_PCSPK_SFX         0xBCC5  /* PC-speaker SFX table (8-byte records)               */
#define DG_SONG_STACK_PTR    0xBD8E  /* ptrBD8E: stack pointer for nested songs/SFX         */
#define DG_PCSPK_SFX_FREQ    0xBE25  /* PC-speaker SFX freq/len (parallel arrays)           */
#define DG_ADLIB_SFX_PTR     0xCFCC  /* AdLib SFX: instrument pointer table                  */
#define DG_ADLIB_SFX_ATTR    0xCFCE  /* AdLib SFX: attribute table                           */
#define DG_INSTR_PTRS        0xD050  /* pointer table (words) to OPL patch records          */
#define DG_SONG_DIR          0xD0C2  /* default song directory (word offsets of voices, 0xFFFF-term) */
#define DG_SONG_HDR_STACK    0xD15C  /* base of stack of 16-byte song headers (+ ptrBD8E)   */
#define DG_SONG_CMD_TBL      0xD18C  /* funcptr table of song commands (7 records)          */
#define DG_SONG_EXTCMD_TBL   0xD19A  /* funcptr table of extended commands (16 records)     */
#define DG_OPL_CELL_REG      0xD1BA  /* operator-cell -> register-offset (00 01 02 08 ...)   */
#define DG_OPL_CHAN_CELL     0xD1CD  /* OPL channel -> operator-cell                         */
#define DG_ADLIB_FNUM        0xD1D8  /* note -> F-number (12 values, C..B)                  */

/* =========================================================================
 * 2. Sizes / counts
 * ========================================================================= */

/* --- Enemies / entities --- */
#define NUM_ENEMY_TYPES      81      /* indices 0..80 (meta/enemy_types.csv)                 */
#define NUM_ENEMY_HANDLERS   57      /* unique handlers (many types share one)               */
#define ENEMY_PROTO_STRIDE   0x33    /* 51 bytes per template record                         */
#define ENEMY_NAME_LEN       8       /* fixed name width (not necessarily NUL-terminated)    */
#define ENTITY_POOL_SLOTS    20      /* slots in the pool @DG_ENTITIES                       */
#define ENTITY_SLOT_SIZE     0x33    /* 51 bytes per slot                                    */
#define ENTITY_ENEMY_SLOTS   15      /* first 0..0xE slots are scanned as "enemies"          */
#define ENEMY_REC_SIZE       5       /* WAVE/PUT enemy-record: type,t,x,depth,ang            */

/* --- Sprites / levels --- */
#define NUM_BOB_SPRITES      236     /* BOB.CPT (docs/bob_format.md, count @+0 = 0xEC)       */
#define NUM_LEVELS           6       /* entries in the script directory @DG_SCRIPT_DIR       */
#define NUM_ASSETS           15      /* distinct .CPT files (see ff_asset_names below)       */

/* --- Lookup --- */
#define SINE_TABLE_LEN       256     /* sine_table[256]                                      */

/* --- Sound --- */
#define NUM_VOICES           9       /* OPL2 melodic channels (0..8)                         */
#define ADLIB_VOICE_SIZE     0x12    /* 18 bytes per voice (docs/sound_format.md §4)         */
#define SONG_HDR_SIZE        0x10    /* 16 bytes per song header                             */
#define NUM_FNUM             12      /* semitones in the F-number table                      */
#define SONG_CMD_COUNT       7       /* records in DG_SONG_CMD_TBL                           */
#define SONG_EXTCMD_COUNT    16      /* records in DG_SONG_EXTCMD_TBL                        */

/* --- Script-VM --- */
#define NUM_VM_OPCODES       14      /* CLEAR..ERREUR (0..9 runtime, 10..13 assembler-only)  */

/* =========================================================================
 * 3. Asset names (.CPT)
 * =========================================================================
 * The strings are scattered across DGROUP (first cluster @DG_ASSET_NAMES); callers pass
 * load_level (0BA8:0AC5) a far pointer to "<NAME>.CPT". For the port we keep a host-side
 * array of names.
 *
 * Order (NUM_ASSETS = 15):
 *   0 TITUS.CPT  1 BOB.CPT   2 GAMEOVER.CPT  3 PRES.CPT     4 CONG.CPT
 *   5 DROGUE.CPT 6 ETAPE.CPT 7 NUC.CPT       8 HIGHSCOR.CPT 9 TITRE.CPT
 *   10 DECA.CPT 11 DECB.CPT 12 DECC.CPT 13 DECD.CPT 14 DECE.CPT
 *   (DECA..E.CPT expand from the printf template "DEC0.CPT" @ds:0x3932 — decoration tiles.)
 */
extern const char *const ff_asset_names[NUM_ASSETS];

/* =========================================================================
 * 4. Accessors to tables inside the G blob (struct Globals G — from ff2_globals.h)
 * =========================================================================
 * All DGROUP tables are read as `(type*)((u8*)&G + DG_xxx + index*stride)`.
 */

/* --- Lookup tables --- */
#define FNUM(i)        (*(u16*)((u8*)&G + DG_ADLIB_FNUM  + (i)*2))   /* F-number of semitone i   */
#define SINE(i)        (*(i16*)((u8*)&G + DG_SINE        + (i)*2))   /* sine_table[i] (signed)   */
#define SPEED(i)       (*(u16*)((u8*)&G + DG_SPEED_TBL   + (i)*2))   /* speed_table[i]           */
#define RAMP(i)        (*(u16*)((u8*)&G + DG_RAMP_TBL    + (i)*2))   /* ramp_table[i] (= i+1)    */
#define COORD(i)       (*(i16*)((u8*)&G + DG_COORD_TBL   + (i)*2))   /* coord_table[i]           */

/* --- Script-VM --- */
#define SCRIPT_DIR(i)  (*(u16*)((u8*)&G + DG_SCRIPT_DIR  + (i)*2))   /* start of script i (rel. to bytecode) */
#define BYTECODE_AT(o) ((u8*)&G + DG_BYTECODE + (o))                 /* bytecode at offset o     */

/* --- Enemies / entities --- */
#define BEHAVIOR_PTR(i) (*(farptr_t*)((u8*)&G + DG_BEHAVIOR_TBL + (i)*4))      /* far pointer to handler of type i */
#define ENEMY_NAME(i)   ((const char*)((u8*)&G + DG_ENEMY_NAMES + (i)*ENEMY_NAME_LEN)) /* fixed-8 name */
#define ENEMY_PROTO(i)  ((u8*)&G + DG_ENEMY_PROTOS + (i)*ENEMY_PROTO_STRIDE)   /* slot template of type i */
#define ENTITY_SLOT(i)  ((u8*)&G + DG_ENTITIES     + (i)*ENTITY_SLOT_SIZE)     /* active slot i          */

/* --- Sound --- */
#define SONG_DIR(i)        (*(u16*)((u8*)&G + DG_SONG_DIR        + (i)*2))     /* stream offset of voice i */
#define INSTR_PTR(i)       (*(u16*)((u8*)&G + DG_INSTR_PTRS      + (i)*2))     /* ptr to patch record i  */
#define SONG_CMD_PTR(i)    (*(farptr_t*)((u8*)&G + DG_SONG_CMD_TBL    + (i)*4))/* handler of command i   */
#define SONG_EXTCMD_PTR(i) (*(farptr_t*)((u8*)&G + DG_SONG_EXTCMD_TBL + (i)*4))/* handler of ext command i */
#define OPL_CHAN_CELL(i)   (*(u8*)((u8*)&G + DG_OPL_CHAN_CELL + (i)))          /* channel -> cell        */
#define OPL_CELL_REG(i)    (*(u8*)((u8*)&G + DG_OPL_CELL_REG  + (i)))          /* cell -> reg-offset     */

/* =========================================================================
 * 5. Canonical reference F-number table (to cross-check against FNUM())
 * =========================================================================
 * The C..B values from docs/sound_format.md §1 — byte-for-byte from AdLib/SB programming docs.
 * Used only to validate the correctness of the loaded DGROUP blob.
 */
static const u16 ADLIB_FNUM_REF[NUM_FNUM] = {
    0x0157, 0x016B, 0x0181, 0x0198, 0x01B0, 0x01CA,
    0x01E5, 0x0202, 0x0220, 0x0241, 0x0263, 0x0287
};

#endif /* FF2_TABLES_H */
