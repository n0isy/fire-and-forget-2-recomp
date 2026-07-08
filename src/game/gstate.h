/* gstate.h — the de-DGROUP'd GAME STATE BLOCKS.
 *
 * Scalar state that used to live at fixed offsets inside the single 64KB `G`
 * byte image now lives in SEMANTIC, typed blocks — one struct per subsystem,
 * with meaningful field names. Every field's ORIGINAL DGROUP home is kept in
 * the comment (cross-reference to the decompile / QEMU captures).
 *
 * INVARIANTS the layout must preserve (documented per field):
 *  - 32-bit pairs (dist, objective, deadline, score words) keep their lo/hi
 *    halves ADJACENT: the ported carry/borrow math reads both halves and the
 *    Gd_ aliases take a 32-bit view of the pair.
 *  - the DC68 cluster (lives / carrier_flag / aircraft_flag) stays adjacent:
 *    display_init reads the far-ptr SEG half of ptrDC68 = the u16 overlaying
 *    carrier|aircraft (Gw_ptrDC68_seg).
 *  - stage_digit is read as a C STRING by the banner draw: the following
 *    stage_digit_nul byte (always 0) is its terminator.
 *  - dual-width aliases (Gw_/Gi_/Gb_ on one field) are pointer-cast views of
 *    the SAME storage, exactly like the original's one memory cell.
 * Cursor values (VM pcs, panel cursor, rle ptr, track ptr) keep the ORIGINAL
 * DGROUP-offset conventions as opaque keys/indices — resolved against the
 * const data arrays (wave_bytecode, ffd_panel_texts, ffd_demo_tape, g_track),
 * so state traces match the QEMU captures byte-for-byte.
 *
 * Access goes through the gnames.h aliases; these structs are the storage.
 */
#ifndef FF_GSTATE_H
#define FF_GSTATE_H
#include "../ff2_types.h"

#pragma pack(push, 1)

/* ---- core per-race state --------------------------------------------------- */
typedef struct {
    u16 speed;           /* tE9CC: scroll/vehicle speed = g_speed_tbl[throttle] */
    u16 missile_fuel;    /* wE9C8: missile fuel/engine level */
    u16 dist_lo;         /* tEA3C ┐ 32-bit distance accumulator (+= speed);     */
    u16 dist_hi;         /* wEA3E ┘ keep adjacent (32-bit carry math + Gd view) */
    u16 car_x;           /* wDD84 (bb94): car lateral position, clamp ±0x96 */
    u16 horizon;         /* wDD86 (bb96): horizon/altitude, 0x87 ground .. 8 ceiling */
    u16 road_center;     /* wF1FD: road perspective centre = 0xA0 - car_x */
    u16 game_mode;       /* wF6AF (d4bf): 0 ground/1 flight/2 takeoff/3 landing/4 crash */
    u16 lives;           /* tDC68 ┐ the ptrDC68 cluster — KEEP ADJACENT:      */
    u8  carrier_flag;    /* ba7a  │ Gw_ptrDC68_seg = the u16 over carrier|    */
    u8  aircraft_flag;   /* ba7b  ┘ aircraft (far-ptr seg half, display_init) */
    u16 fuel_window;     /* wDC6C: fuel/road window 63->0 (FUEL pickups refill) */
    u8  stage_clear;     /* bDC6E: anim_step — boss death = stage clear */
    u8  demo_flag;       /* bDC6F: attract-demo mode (menu timeout) */
    u16 kero;            /* wDC78: kerosene/K-bar level (flight fuel) */
    u16 objective_lo;    /* d4d4 @F6C4 ┐ 32-bit objective counter;   */
    u16 objective_hi;    /* @F6C6      ┘ keep adjacent (Gd_objective) */
    u8  obj_done;        /* bF688: objective-counter-done latch */
    u8  obj_sfx_latch;   /* bF6DA: objective-done sfx one-shot latch */
    u8  bF6DB;           /* bF6DB: zeroed at init; semantics unproven */
    u8  leader_sfx;      /* bF6DC: LEADER-SPOTTED sfx queue flag (sound port) */
    u16 flash_timer;     /* wF6A9 (d4b9): damage-flash/invuln timer */
    u16 flash_colour;    /* d013 @F203: flash colour source (car = colour-1) */
    u8  player_hit;      /* bF6B1 (d4c1): player hit this frame (1 soft/2 hard) */
    u8  bonus_banner;    /* d4c2 @F6B2: BONUS banner parity-frame countdown */
    u8  demo_abort;      /* bF6B3: demo aborted by a keypress */
    u8  crash_ctr;       /* bDE6D (bc7d): crash counter 0x18 -> 0 */
    u16 cflash_spr;      /* tF085 (ce95): cockpit-flash overlay sprite, 0 = none */
    u16 cflash_x;        /* bc77 @DE67: cockpit-flash overlay x */
    u16 cflash_y;        /* bc7b @DE6B: cockpit-flash overlay y */
    u16 enemy_count;     /* wF46A (d27a): live enemy count */
    u16 shot_count;      /* wF6D1 (d4e1): live enemy-shot count */
    u16 kill_ctr;        /* tF6B8 (d4c8): wave kill counter (0 -> BONUS, re-arm 0xFF) */
    u16 charge_flag;     /* d4d8 @F6C8: finale charge-truck trigger */
    u16 engine_pitch;    /* d600 @F7F0: engine-rumble pitch (boss vz mirror) */
    u16 engine_x;        /* d602 @F7F2: engine-rumble x (boss x_accum mirror) */
    u16 leader_slot;     /* d604 @F7F4: formation-leader slot offset (POOL-relative) */
    u16 leader_seg;      /* d606 @F7F6: leader ptr seg half (0) */
    u16 mtn_scroll;      /* cd60 @EF50: mountain scroll, mod 0x2800; PERSISTS across races */
    u16 cloud_acc[5];    /* c850.. @EA40-EA48: cloud parallax band accumulators */
    u16 track_ptr;       /* tF6CD: road-curve byte pointer (OFFSET into G track data) */
    u16 track_seg;       /* wF480: track segment index (cycled by PUT waves) */
    u16 spr_count;       /* tEF52: live sprite-directory count */
    u16 spr_count_saved; /* tDE69: directory count saved by main */
    u16 credits;         /* tEA3A: inserted credits / players */
    u16 throttle_step;   /* w2845: throttle increment (run_level_init sets 8;
                          * was blob-init data + the original's DEBUG keys) */
} RaceState;

/* ---- score / stage ---------------------------------------------------------- */
typedef struct {
    u16 stage;           /* t27C7: stage index 0..4 (5 = finale script) */
    u16 score_lo, score_hi;  /* 27c9:27cb 32-bit score */
    u16 high_lo,  high_hi;   /* 27cd:27cf displayed HIGH (table #1 entry) */
    u16 bonus_lo, bonus_hi;  /* 27d1:27d3 32-bit bonus */
    u16 best_lo,  best_hi;   /* 27d5:27d7 session best (CONTINUE keeps it) */
} ScoreState;

/* ---- green objective-panel typewriter ---------------------------------------- */
typedef struct {
    u8  msg;             /* bc60 @DE50: message index */
    u8  delay;           /* bDE51: message re-arm countdown (per tick) */
    u8  state;           /* bc62 @DE52: 0 idle / 1 start / 0xFF typing */
    u16 cursor;          /* bc63 @DE53: typewriter byte cursor — the original's
                          * panel-region OFFSET (0x3409-based; kept so the tDE53
                          * trace matches QEMU), resolved against ffd_panel_texts */
    u16 pen_x;           /* d5c8 @F7B8: pen X (pixels) */
    u16 pen_y;           /* d5ca @F7BA: pen Y (pixels) */
} PanelState;

/* ---- input flags (poll_controls fn0A0D_0002 / the demo replay) --------------- */
typedef struct {
    u16 fire;            /* w24B7 */
    u16 start;           /* t24B9 (aux/START) */
    u16 accel;           /* w24BB (UP) */
    u16 brake;           /* w24BD (DOWN/kp5) */
    u16 right;           /* w24BF (RIGHT arrow -> car right) */
    u16 left;            /* w24C1 (LEFT arrow -> car left) */
    u16 mouse_fire, mouse_start, mouse_up, mouse_down,   /* a24C3..t24D1: mouse */
        mouse_right, mouse_left, mouse_fire_b, mouse_start_b;  /* (always 0)   */
    u16 joy_up, joy_down, joy_right, joy_left;   /* w24D3..w24D9 (always 0) */
    u16 missile;         /* wF6B4: missile button (chord / demo mask 0x40) */
    u16 takeoff;         /* wF6C2 (d4d2): take-off/land edge */
    u16 chord_edge;      /* d4c4 @F6F4: fire+aux chord edge */
    u8  aux_edge;        /* tF6EA: aux-button edge latch */
    u8  pause_edge;      /* bF6EB: pause-chord edge latch */
    u8  mask;            /* d4fc @F6EC: current demo/tape input mask */
    u8  rle_count;       /* d271 @F461: RLE run-length countdown */
    u16 rle_ptr;         /* d492 @F682: RLE stream ptr (OFFSET into the G tape data) */
    u8  pause_vis;       /* b37A2: pause "squeeze" visual state */
    u8  exit_flag;       /* b40A2: exit/edge event flag (name-entry timeout) */
    u8  b40A7;           /* b40A7: loop guard flag; semantics unproven */
} InputState;

/* ---- frame timing / presentation --------------------------------------------- */
typedef struct {
    i16 tick_timer;      /* t24F1: ISR countdown; SIGNED, goes negative */
    u8  playfield_h;     /* t24F3: playfield height (0x98 race / 0xF8) */
    u16 playfield_w;     /* w24F5: playfield width (0x140) */
    u16 w24F7;           /* w24F7: zeroed at init; semantics unproven */
    u16 blink;           /* w24FF: blink parity, ^=1 once per frame */
    u16 frames_per_tick; /* wF6B6: measured frames per ISR tick (18) */
    u16 w24E7, w24E9;    /* w24E7/w24E9: zeroed at init; semantics unproven */
    u16 wE4CA, wE9CE;    /* wE4CA/wE9CE: zeroed at init; semantics unproven */
    u16 wDEAE;           /* wDEAE: shade-table tail seed (read-only zero) */
} TimingState;

/* ---- HUD cockpit gauge state -------------------------------------------------- */
typedef struct {
    u8  weapon_vars[6];  /* @F462..F467: the 6 weapon bytes zeroed at init
                          * ([0] = bF462 weapon-icon anim index) */
    u8  hud_phase;       /* bF6D3: gauge interleave phase */
    u16 fbar_shown;      /* w38AB: F bar level drawn (ramps to fuel_window>>1) */
    u16 kbar_shown;      /* w38AD: K bar level drawn (ramps to kero) */
    u16 fuel_shown;      /* w38AF: fuel gauge level drawn (ramps to missile_fuel) */
    u16 high_cache_lo;   /* tF7E1 ┐ HIGH digits redraw cache */
    u16 high_cache_hi;   /* wF7E3 ┘ */
    u16 wF7E5, wF7E7;    /* init 0xFFFF; cache pair, reader unproven */
    u16 bonus_cache_lo;  /* tF7E9 ┐ BONUS digits redraw cache */
    u16 bonus_cache_hi;  /* wF7EB ┘ */
    u8  orbs_shown;      /* bF7ED: lives-orbs currently drawn */
    u8  stage_digit;     /* bF7EE: ASCII stage digit — read as a C STRING by the  */
    u8  stage_digit_nul; /* bF7EF: banner draw; this 0 byte terminates it (ADJACENT) */
} HudState;

/* ---- the two script VMs --------------------------------------------------------
 * PCs / return addresses / cursors are OFFSETS into the wave bytecode, which
 * still lives at its original place in G — only the VM STATE moved. */
typedef struct {
    u16 wave_pc;         /* tF6BA: WAVE-VM program counter */
    u16 wave_ret;        /* ptrF7C6: wave-VM GOSUB return */
    u16 wave_side;       /* wF7C0: wave build side flag */
    u16 wave_rearm;      /* wF7C2: side re-arm latch (CLEAR) */
    u16 marker_idx;      /* wF7C4: GETCHAR marker read cursor */
    u8  wave_phase;      /* b38A6: PUT/WAVE op phase */
    u8  wave_count;      /* bF7CA: WAVE drip count latch */
    u16 deadline_lo;     /* tF7CB ┐ WAVE next-spawn distance deadline */
    u16 deadline_hi;     /* wF7CD ┘ */
    u16 path_pc;         /* tD7B4: PATH-VM program counter */
    u16 path_ret;        /* ptrF7D5: path-VM GOSUB return */
    u16 path_marker_idx; /* wF7D3: path-VM marker write cursor */
    u16 shape_cursor;    /* tF7CF: composer stream cursor */
    u16 buf_half;        /* wF7D9: runtime bank half offset (0 / 0x4000) */
    u8  path_side;       /* bF7DB: path-VM side sync flag */
    u8  path_side_latch; /* b38A7: buffer-half latch (CLEAR side sync) */
    u8  build_state;     /* b38AA: shape-build state (0 start/1 stepping/2 done) */
    u16 w38A4;           /* w38A4: set 1 on VM arm; no reader found */
    u16 w38A8;           /* w38A8: path-VM build tick flag */
    u16 scale_budget;    /* w2505: composer scaler budget (0x80 PUT / 0xC0 GETCHAR wait) */
    i16 markers[10];     /* wF46C[0..9] @F46C..F47F: shape markers (-idx building /
                          * +idx ready); scripts use at most 5 per CLEAR (measured) */
} VmState;

/* ---- roadside décor bookkeeping (the aDDEC ring itself is still in G) ---------- */
typedef struct {
    u16 density;         /* wF468: adaptive décor density */
    u16 count;           /* wDC7E: live ring entries */
    u16 head;            /* tEF4A: ring write cursor */
    u16 tail;            /* tDE5D: ring draw/recycle cursor */
    u16 kind_cur;        /* w379F: 0..15 cursor cycling the prototype table */
    u16 latch;           /* wF6D8: one-spawn-per-16-units latch */
    u16 pickup_banner;   /* bEF56 (cd66): weapon-crate banner; the ROM writes it as a
                          * WORD (=0x14) whose high byte hit the 0xEF57 table[0] (=0) —
                          * as a u16 field the quirk is preserved and harmless */
} DecorState;

#pragma pack(pop)

extern RaceState   g_race;
extern ScoreState  g_score;
extern PanelState  g_panel;
extern InputState  g_in;
extern TimingState g_tim;
extern HudState    g_hud_st;
extern VmState     g_vm;
extern DecorState  g_decor_st;

/* ---- THE MORPH-SCRIPT BANK (mutable; was the G region 0x11F8..0x1460) ------
 * The 13 LOD scripts tile the region exactly (154 MorphEnt entries). MUTABLE
 * at runtime: morph_patch writes the thresholds (= sprite heights) at boot,
 * the explosion AI pulses the 0x135C script's thresh[0] per frame, and
 * morph_patch doubles the missile script's thresh[0]. Seeded from the
 * ffd_morph_* data by ff_apply_data_tables.
 * KEYS: entity anim fields / décor morph ptrs still hold the ORIGINAL
 * region offsets (they come from the as-is prototype data, e.g. 0x0DE1135C)
 * — morph_at() resolves such a key into the typed bank. */
#define MORPH_BASE 0x11F8
#define MORPH_N    ((0x1460 - 0x11F8) / 4)   /* 154 entries */
extern MorphEnt g_morph[MORPH_N];

/* the composer MIP LIST (mutable; was @0x3837): 8 {src, xform} i16 pairs +
 * the -1 terminator. The SRC halves are recomputed per shape build; the
 * XFORM halves + terminator are STATIC (seeded from ffd_miplist_init) —
 * the documented 0x3837 mixed static/dynamic pattern, explicit here. */
extern i16 g_miplist[17];

/* THE HIGH-SCORE TABLE (mutable; was names @0x27D9 + 32-bit scores @0x282D).
 * Exactly the HIGH file's 0x6C-byte image: 6 x 14-char rows ("N XXX ......")
 * then 6 lo/hi score pairs. Defaults seeded from ffd_high_names/_scores;
 * ff_load_high overwrites from the HIGH file at boot; the name entry edits
 * rows in place and ff_save_high writes the block back. */
#pragma pack(push, 1)
typedef struct {
    char name[6][14];                    /* @27D9: "1 ABC ......." rows */
    struct { u16 lo, hi; } score[6];     /* @282D: 32-bit scores (lo:hi) */
} HighTable;
#pragma pack(pop)
extern HighTable g_high;
static inline MorphEnt *morph_at(u16 key) { return &g_morph[(u16)(key - MORPH_BASE) >> 2]; }

/* ---- THE ENEMY-PROTOTYPE POOL (mutable; was the G region @0x1490) ----------
 * 81 packed 0x33-byte records, seeded from ffd_enemy_protos. MUTABLE: the
 * path VM records each type's runtime shape base at +0x1B; spawns memcpy
 * whole records out. (Entity comes from ff2_types.h.) */
extern Entity g_protos[81];

#endif /* FF_GSTATE_H */
