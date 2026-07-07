# ff2.h build spec (for agents)

Goal: a single header for the port. Each agent writes ITS OWN fragment into `src/gen/`,
and I assemble them into `src/ff2.h`. Everything must compile with `gcc -std=c11 -fsyntax-only`.

## Base types (declared in ff2_types.h, used by EVERYONE)
```c
#include <stdint.h>
#include <stddef.h>
typedef uint8_t  u8;  typedef int8_t  i8;
typedef uint16_t u16; typedef int16_t i16;
typedef uint32_t u32; typedef int32_t i32;
typedef uint32_t farptr_t;   /* 16-bit far seg:off, stored as 4 bytes (low=off, high=seg) */
```
- NO `far`/`near`/segments in the code — the target model is linear (Linux).
- In **struct Globals** the field sizes are BYTE-EXACT to the 16-bit DGROUP: word->u16, byte->u8,
  far_ptr->farptr_t(4b), arrays->`u8 name[N]`. This is so we can load the EXE's static data.
- In game structures (not Globals) far pointers are also `farptr_t` (resolved during porting).

## Canonical type and enum names (ff2_types.h)
- `struct Globals` — declare it (definition in ff2_globals.h as `extern struct Globals G;`).
- `typedef struct { ... } GameObject;`  — object record (source: docs/entities.md, globals_structs.md).
- `typedef struct { ... } Entity;` — 0x33-byte slot (docs/entities.md: +00 type,+2F hp,+0F vx,...).
- `typedef struct { ... } BlitEntry;` — draw-queue element (globals_structs.md @0xDBC8).
- `typedef struct { ... } SpriteHdr;` — BOB sprite header (docs/bob_format.md: H@-4,W@-2,planes).
- `typedef struct { ... } Particle;` — 10 bytes (vx,x,vy,y,v0) (docs from F1/update_particles).
- `typedef struct { ... } AdlibVoice;` — 0x12 bytes (docs/sound_format.md).
- `enum GameMode { ... };` (game_mode @F6AF 0..4).
- `enum VmOp { VM_CLEAR,VM_GETCHAR,VM_PUT,VM_WHILE,VM_GOTO,VM_SETDIST,VM_GOSUB,VM_RETURN,VM_BEQ,VM_WAVE,VM_DEFB,VM_END,VM_INCLUDE,VM_ERREUR };` (docs/script_vm.md).
- `enum EnemyType { ... };` — 81 types (meta/enemy_types.csv); name = ET_<ENEMYNAME> or ET_<n>.

## Fragment files (what goes where)
- `src/gen/ff2_types.h` — base types, enums, game structs (EXCEPT Globals).
- `src/gen/ff2_globals.h` — `struct Globals { ... };` byte-exact from meta/globals_named.csv +
  `extern struct Globals G;` + `_Static_assert(offsetof(struct Globals,field)==0xNNNN,...)` for named ones.
- `src/gen/ff2_tables.h` — `extern` declarations of static DGROUP tables (sine_table, behavior a3A63,
  adlib F-num, enemy prototypes, asset-names) + numeric #define (FF_W=320 etc. already in platform.h — do not duplicate).
- `src/gen/ff2_proto.h` — prototypes of ALL functions from meta/functions.csv (suggested_name column);
  take signatures from Reko (raw/reko/FF2EGA_*.c, `// SEG:OFF:` headers / definitions) and simplify
  far->farptr/pointer. Group with comments by subsystem. Mark runtime/copyprotect/dos wrappers
  with a comment "// replaced by libc / removed during port", but still provide the prototype.

## Include order (I will assemble ff2.h like this)
`platform.h` -> `ff2_types.h` -> `ff2_globals.h` -> `ff2_tables.h` -> `ff2_proto.h`.

## Rules
- Do not duplicate definitions across fragments. Types — only in ff2_types.h.
- Each fragment compiles when included after the previous ones (you can temporarily
  build a test: `#include` in order + `int main(){return 0;}` and `gcc -fsyntax-only`).
- Where the data for a type is insufficient — make the field/type a raw `u8 raw[N]` with a TODO comment; do not invent.
