/* ff_game.h — integration contract for the gameplay port.
 * The orchestrator (game_main.c) drives these; each subsystem .c implements its
 * functions (strong defs override the weak stubs in game_main.c). All share `G`. */
#ifndef FF_GAME_H
#define FF_GAME_H
#include "../ff2.h"
#include "../asset/ff_assets.h"
#include "entity_dispatch.h"
#include "data/wave_data.h"

/* ---- WAVE BYTECODE FETCH (de-DGROUP'd) ------------------------------------
 * The two script VMs EXECUTE straight from the const wave_bytecode[] array
 * (assembled from wave/levels.wave) — the bytecode is no longer copied into
 * G. PCs / GOSUB returns / record operands KEEP the original DGROUP-offset
 * convention (base 0xA0), so all faithful address arithmetic is unchanged;
 * only the fetch is redirected. */
static inline u8 wv_b(u16 off)              /* byte at bytecode offset `off` */
{
    return wave_bytecode[off - WAVE_BYTECODE_BASE];
}
static inline u16 wv_w(u16 off)             /* little-endian word */
{
    return (u16)(wv_b(off) | ((u16)wv_b((u16)(off + 1)) << 8));
}
static inline const unsigned char *wv_ptr(u16 off)   /* record pointer */
{
    return wave_bytecode + (off - WAVE_BYTECODE_BASE);
}

/* ---- shared runtime context (the port's live state not yet pinned into G) ---- */
typedef struct {
    BobBank *bob;                 /* sprite bank (BOB.CPT, main / 1st bank) */
    BobBank *nuc;                 /* NUC.CPT — the 20-frame NUCLEAR-scene sheet
                                     (round_transition fn13a8_048a: mushroom cloud
                                     frames 0-11, rocket+flame 12-18, launcher 19),
                                     loaded per level by fn0A0D_0ADB, registered at
                                     aDEEE[tEF52=300..] by fn0A0D_0B35 */
    BobBank *gen;                 /* boot-generated décor bank (fn0A0D_133B),
                                     aDEEE[236..299], directory bank id 3 */
    uint8_t  pal[FF_NCOL][3];     /* current palette */
    int      level;
    int      scroll_x;            /* mirror of G scroll for the port shell */
    int      input[FF_K_COUNT];   /* edge/held state snapshot for this frame */
} GameCtx;
extern GameCtx GC;

/* (the dgroup.bin blob loader is GONE: G starts zeroed — the original BSS —
 * and ff_apply_data_tables() populates it from the typed data sources.) */

/* lifecycle (game_main.c) */
int  game_init(const char *asset_dir);   /* data tables + bob + first level */
void game_start_level(int lvl);
/* populate G from the SOURCE data (data/wave_data + the game/data tables). */
void ff_apply_data_tables(void);

/* subsystems (each ported in its game/*.c) */
void vm_step(void);                /* game/vm.c — fn0BA8_13FD(ds,0): one wave-VM dispatch */
void vm_arm(void);                 /* game/vm.c — fn0BA8_13FD(ds,1): arm + one dispatch  */
/* path VM + runtime shape composer (game/path_vm.c) — fn0BA8_175D/fn0A0D_0D08 */
void path_vm_tick(int arm, int side);
void rt_shape_dims(int slot, int *w, int *h);   /* runtime bank (dir bank id 4) */
void rt_shape_blit(int slot, int x, int y);
int  rt_shape_sample(int slot, int x, int y);
int  ff_dir_sample(int idx, int x, int y);      /* chunky pixel of aDEEE[idx]  */
int  shape_measure(const u8 *prog, int n);      /* fn0A0D_17C8 (shape_gen.c)   */
void camera_update(void);          /* game/camera.c — scroll / pseudo-3D */
void entities_update_all(void);    /* game/entity.c — update+collide all slots */
void render_frame(void);           /* game/render.c — bg + display list -> ff_fb */
void render_world(void);    /* game/render_world.c — faithful sky+road+scenery */

/* road track curve data (game/render_world.c): far-data curve profile from the
 * EXE (segment 0de1), walked by tF6CD (fn0869_154D) — NOT in the DGROUP blob. */
void track_load(const char *path);   /* (historical no-op: curve is ffd data)    */
void track_seed(void);               /* boot: assemble g_track from ffd_track_*  */
void track_reset(void);              /* per-level: tF6CD = slot 0                */
void track_step(void);               /* per-frame gate: advance on (c84c&0x30)   */
void track_advance(void);            /* fn0869_154D: one curve-pointer step      */
int  track_curve_now(void);          /* current curve byte (tF6CD.u1)->b0000     */
int  track_pos(void);                /* diag: current tF6CD (vs the QEMU trace)  */

/* green objective-panel typewriter (game/panel_text.c) — fn13a8_00a1 */
void panel_text_reset(void);   /* run_level prologue: arm message 0           */
void panel_text_set(int idx);  /* arm a specific objective message (events)   */
void panel_text_advance(void); /* per GAME ITERATION: advance typing state    */
void panel_text_draw(void);    /* per RENDER: redraw revealed text + counter  */
void panel_text_frame(void);   /* advance + draw (single-iteration helper)    */

/* front-end boot sequence (game/frontend.c) — main boot screens + fn0DAE_0001
 * title + fn0DAE_00D5 timed screens + fn0DAE_0118 menu. Frame-driven port. */
void frontend_reset(const char *asset_dir); /* arm the cold-boot sequence      */
int  frontend_frame(void);          /* 1 = frontend owns the frame, 0 = race   */
void frontend_reenter(void);        /* back to menu after a game/demo cycle    */
void frontend_post_race(void);      /* run_level exited: death-branch resets + re-enter */
int  frontend_stage_advance(void);  /* fn0DAE_03AA start_level: STAGE-clear interlude + next stage */
int  frontend_phase(void);          /* debug/testing: current FeState ordinal (3=CONTINUE, 4=GAMEOVER, 5=HIGHSCORE, 7=MENU, 10=RACE) */
int  frontend_wipe_frame(void);     /* fn13a8_132F spiral wipe: 1 while animating (one ring/frame) */
void frontend_title_screen(void);   /* fn0DAE_0001: TITRE + overlays + © glyph */

/* round_transition cutscene (game/cutscene.c) — fn0BA8_048A @0BA8:048A: the
 * death-branch NUC vehicle-rise scene + the level-5 rocket-launch finale.
 * start() = the function entry (bF6B3 gate, finale dispatch, pool reset, NUC
 * load); frame() = one presented frame, 0 when finished/skipped. */
void round_transition_start(void);
int  round_transition_frame(void);
/* sprite_dl extra-enqueue hook (between the pool walk and the flush) — the
 * cutscene finale's rocket block (fn0A0D_082A entries). NULL when unused. */
extern void (*g_dl_extra)(void);
/* mountain strip-slot mask: 7 in run_level, 3 in the cutscene finale. */
extern int g_mtn_mask;

/* sprite directory init (game/display_init.c) — fn0A0D_09E4 + fn0A0D_0B35 */
void ff_display_list_init(void);          /* fn0A0D_09E4: build ptrDBC8 chain + register main bank into aDEEE */
void sprite_dir_register_bank2(void);  /* fn0A0D_0B35: append the 2nd bank (NUC) into aDEEE[tEF52..] */
void shape_gen_build(void);            /* fn0A0D_133B: generate + register the décor shapes (game/shape_gen.c) */
/* aDEEE sprite-directory accessors: idx -> (bank,frame) via aDEEE[idx].seg/.off. */
void ff_dir_blit(int idx, int x, int y);   /* blit registered sprite idx into ff_fb */
void ff_dir_dims(int idx, int *w, int *h); /* dims of registered sprite idx */
int  ff_dir_count(void);                   /* number of registered aDEEE entries */

/* entity sprite render pipeline (game/sprite_dl.c) */
/* per-slot snapshot of the explosion morph-threshold 0x135C, captured after each
 * slot's AI in entities_update_all so render_entity reproduces the original's
 * interleaved AI+projection (different-parity explosions differ in size). */
extern u16 g_slot_thresh[20];
/* per-frame "slot was active at its loop turn" mask (fn0DAE_0446 interleave):
 * render_entities draws only masked slots — a copy spawned mid-pass into an
 * already-visited (lower) slot is not projected until the next frame. */
extern u8  g_slot_drawn[20];
void morph_patch(void);      /* fn0A0D_0C8F: patch morph thresholds = BOB heights */
void render_entities(void);  /* fn0DAE_0446 tail: project+enqueue+draw pool sprites */
void dl_enqueue_sorted(i16 xc, i16 yb, int frame, i16 depth); /* fn0A0D_082A */

/* roadside décor ring (game/decor_ring.c) — fn0869_1946 spawn + fn0869_17B9 draw */
void decor_reset(void);        /* level entry: arm the spawn accumulator           */
void decor_set_accum(int v);   /* stage-entry residue override (uninit stack local) */
void decor_spawn_step(void);   /* per frame: accum wFFFFFFF2 += tE9CC; >0x40 spawn */
void decor_ring_enqueue(void); /* fn0869_17B9: enqueue ring sprites (from render)  */
void decor_ring_vm_spawn(void);/* fn0869_1946 direct (wave-VM GETCHAR-wait path)   */

/* HUD cockpit gauges (game/hud_cockpit.c) — fn0BA8_1B27 fuel/aim/weapon columns */
void hud_cockpit_reset(void); /* run_level init: clear the persistent VRAM overlay */
void hud_cockpit_step(void);  /* per GAME ITERATION: advance the gauge state machine */
void hud_cockpit_draw(void);  /* per RENDER: composite the gauge overlay over the panel */
void hud_stage_banner(void);  /* fn0BA8_1B27 @5185: STAGE COMPLETED banner (bDC6E set) */
void hud_number_draw(u32 v, int nd, int xc, int yc); /* fn0A0D_034C digits -> the persistent overlay */

/* background parallax layers (game/bg_layers.c) */
void decor_load(const char *cpt_dir, int level);  /* fn15ae_05e2 + fn18ef_056c */
void draw_clouds(void);     /* fn10EF_039F — 5 sky bands, single plane (white)  */
void draw_mountains(void);  /* fn10EF_0307 — 16-row horizon band, h-scroll      */

/* PRNG seed (game/behaviors.c) — fn0A0D_02D3, called once at startup by main. */
void ff_rng_seed(void);

/* ---- AdLib sound driver (game/sound.c — the segment 143A port) ------------
 * Output = the OPL register write stream (opl_write -> ff_opl_out platform
 * sink + optional log); the acceptance comparison is that stream + the
 * per-frame voice state, not audio samples. One presented frame = 5 music
 * ticks, run AFTER the frame's game logic (snd_frame). */
void snd_init(void);                 /* fn143A_0349 AdLib branch + boot song  */
void snd_frame(void);                /* 5 ISR ticks; call once per frame      */
void snd_run_ticks(int n);           /* extra ticks (the run_level entry spin) */
void snd_sfx_trigger(int id, int r); /* fn143A_05B7: replace the song stack   */
void snd_sfx_queue(int id);          /* fn143A_0643: push onto the song stack */
void snd_sfx_play(int id);           /* fn143A_027B: one-shots (F3-gated)     */
void snd_toggle_mute(void);          /* the F2 toggle (b409F)                 */
void snd_toggle_oneshot(void);       /* the F3 toggle (b40A0)                 */
int  snd_muted(void);                /* b409F != 0 (read by fn1069_13c0)      */
void snd_set_log(void *fp);          /* FILE*: 'W,frame,seq,reg,val' lines    */
void snd_state_row(char *out, int cap);  /* per-frame state CSV tail          */
u32  snd_frame_no(void);

/* high-score table load (game/game_main.c) — fn13a8_12f4: read the HIGH file into
 * the DGROUP table @0x27d9 (the scores are NOT in the static blob) + set 27cd/27cf. */
void ff_load_high(const char *asset_dir);
/* high-score table SAVE — fn13a8_0e13 tail (fn0A0D_0483 "HIGH"): write the 0x6C-byte
 * table (names @0x27d9 + scores @0x282d) back to the HIGH file after name entry. */
void ff_save_high(const char *asset_dir);

/* helpers shared across subsystems (game/game_main.c) */
/* THE ENTITY POOL — de-DGROUP'd: a real typed array outside G (was the 20x0x33
 * byte region at DGROUP 0xE5CC). All faithful in-slot offset arithmetic keeps
 * working on the packed Entity layout; only the BASE moved. ENT_BASE is the
 * byte view for transliterated code that walks slots by 0x33 strides. */
extern Entity g_pool[ENTITY_POOL_SLOTS];
#define ENT_BASE ((u8 *)g_pool)
Entity *entity_pool(void);         /* g_pool as Entity[ENTITY_POOL_SLOTS] */

/* RUNTIME LUT BLOCKS (de-DGROUP'd): tables BUILT by code (boot / level init /
 * per frame), moved out of the G byte image into standalone typed arrays.
 * Comments give the original DGROUP home + the builder routine. */
extern u16 g_speed_tbl[0x100];    /* aF482 @0xF482: throttle -> speed (fn0869_1480; run_level.c) */
extern u16 g_ramp_tbl[0x3F];      /* wDE6E @0xDE6E: ramp LUT, [i]=i+1 (fn0869_1480)              */
extern u16 g_shade_tbl[300];      /* aF209 @0xF209: shade table = ramp[speed[i]] (written-only)  */
extern u8  g_persp_scale[0x100];  /* E4CC @0xE4CC: perspective scale LUT (fn0869_158D, per frame) */
extern u8  g_persp_halfw[0x100];  /* DC84 @0xDC84: road half-width LUT (fn0869_158D)             */
extern u8  g_keystate[0x80];      /* INT9 keystate[scancode] @0x3FE3 (fn114C_0012; keyboard.c)   */

/* RUNTIME ARRAY BLOCKS (de-DGROUP'd, step 4): the live arrays that used to sit
 * at fixed offsets in G, now typed blocks. Original homes + owners noted. */
/* the roadside-décor ring (was aDDEC @0xDDEC, 10 entries x 0x0A; decor_ring.c).
 * Entry layout == DecorProto: kind (0 = free), side, spawn phase, morph far-ptr
 * (.off = the morph script's offset in G — the scripts are still content in G). */
extern DecorProto g_decor_ring[10];
/* the crash-particle ring (was @0x35CA, 8 x 10 bytes; player_render.c). The
 * vx/v0 columns hold their DATA values (ffd_particle_init) — never rewritten;
 * x/vy/y are re-seeded per crash by fn1069_13c0. */
extern Particle g_particles[8];
/* the road coord table (was a2C51 @0x2C51: 16 depth levels x 4 columns, column
 * stride 0x20; render_world.c fn0869_15D6 fills it per frame). center[] is the
 * ORIGINAL a2CB1 @0x2CB1 (= column 3), the entity projector's perspective
 * interpolation source; entries 16..17 take the fn0869_15D6 trailing spill. */
typedef struct { i16 left[16]; i16 right[16]; i16 y[16]; i16 center[18]; } RoadCoords;
extern RoadCoords g_road;
/* the sprite DIRECTORY (was aDEEE @0xDEEE) + real-width table (was aF087
 * @0xF087); display_init.c owns them. g_sprdir[idx] = {.off = frame/rt-slot,
 * .seg = bank id + 1 (1 BOB, 2 NUC, 3 generated décor, 4 runtime-composed)}.
 * 384 covers the original extent (main 0..235, décor 236..299, runtime
 * 300..379 via the tDE69+0/40 double-buffer halves). */
#define SPRDIR_MAX 384
extern FarPtr g_sprdir[SPRDIR_MAX];
extern u8     g_sprw[SPRDIR_MAX];
/* (the mutable enemy-prototype pool g_protos[81] is declared in gstate.h) */

/* faithful spawner (game/entity_faithful.c): consume one 5-byte WAVE record
 * [type, depth, y, x, vz], spawn into a free slot, return bytes consumed (5). */
int     spawn_enemy_rec(const unsigned char *rec);

/* run_level (game/run_level.c) — faithful port of fn0869_0006 @0869:0006.
 *   run_level_init():  one-time level setup; builds all runtime LUTs/tables
 *                      (aF482 speed table, E4CC/DC84 persp LUTs, wDE6E/aF209
 *                       shade tables, a2CB1 entity-perspective) and zeroes state.
 *   run_level_frame(): exactly ONE game frame (the original loop body minus the
 *                      VBL spin), driving the ported subsystems in order. */
void run_level_init(void);
int  run_level_frame(void);   /* 1 = a race frame ran; 0 = race ended (local_38→0) */

/* keyboard / input module (game/keyboard.c) — fn0A0D_0002 poll_controls + the
 * INT9 key-state array @0x3FE3. poll_controls() reads GC.input via keystate and
 * sets the game input flags (0x24B7 fire, 0x24B9 aux/start, 0x24BB/BD/BF/C1,
 * wF6B4 missile, wF6C2 take-off). Called every frame by run_level + the front-end. */
void ff_poll_controls(void);        /* fn0A0D_0002: keystate -> input flags */
void ff_keystate_update(void);   /* the port's "INT9 ISR": GC.input -> keystate @0x3FE3 */

/* external input TAPE (run_level.c): drive demo_input_step from a per-frame key-mask
 * file instead of the embedded RLE @0x2849 — the recorded level-1 keypresses,
 * replayable in the port and injectable into the original for a deterministic run. */
void demo_input_set_tape(const u8 *tape, int len);

/* Throttle / "gear" — the run_level stack local di_1195. It is owned globally so
 * the camera (camera_faithful.c) can ramp it and the speed-table (aF482) lookup
 * uses it. DEFINED in camera_faithful.c; run_level_init zeroes it on level entry. */
extern int g_throttle;
/* run_level/camera stack locals exposed for the player-car banking column
 * (fn1069_0006 @621-623: wFFFFFFD0 -= si_2063 + tFFFFFFB0). Defined in
 * camera_faithful.c, evolved each frame by camera_update(). */
extern int g_steer_lean;   /* tFFFFFFB0 — steering lean (-3..3) */
extern int g_decel_e6;     /* wFFFFFFE6 — throttle decelerating flag */
extern int g_roll;         /* local_2e — takeoff/flight/landing roll accumulator */
extern int g_car_col;      /* wFFFFFFD0 / local_32 — player-car banking column [0..9] */
extern int g_car_row;      /* local_30 — player-car sprite row (4 ground, 0..3 flight) */
extern int g_rotor;        /* local_34 — flight rotor/shadow sprite id (0x15/0x16/0x17, 0 = none) */
extern int g_dust;         /* local_24 — wheel-dust sprite id drawn at (bb94,0x98), 0 = none */
extern int g_crash_phase;  /* bDE6D pre-decrement this frame (0 = no crash draw); gates the
                              fn1069_13c0 particle burst instead of the car (run_level.c) */
extern int g_flash_f6a9;   /* wF6A9 as the car draw @1756 saw it (pre 20-frame-block dec);
                              the damage-flash gate must read THIS, not the live field */
extern int g_steer_flag;   /* cVar13 — steering direction (0/1/2), read by switch#2 flight case */
extern u8  g_exhaust_phase;/* local_6 — exhaust-flame animation phase [0..3] */

#endif
