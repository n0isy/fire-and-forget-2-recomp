/* gnames.h — semantic names for the game state fields (the naming layer).
 *
 * HISTORY: originally every alias expanded to a G-offset accessor into the
 * 64KB DGROUP image, so the rename was provably semantics-neutral. The
 * de-DGROUP migration then pointed every alias at the typed state blocks
 * (gstate.h) — the alias NAMES never changed, so no call site moved; only
 * the storage did. THE DGROUP IMAGE IS GONE NOW: content is read as named
 * ffd_* data and all state lives in the blocks.
 *
 * Width rules survive the move: Gw_ = u16 view, Gi_ = i16 view, Gb_ = u8
 * view, Gd_ = u32 view, Gp_ = byte pointer. Where one original memory cell
 * was accessed with two widths, the extra alias is a pointer-cast view of
 * the SAME struct field (documented in gstate.h). Every entry keeps the
 * original reko/ghidra field name for cross-referencing the decompile.
 */
#ifndef FF_GNAMES_H
#define FF_GNAMES_H
#include "gstate.h"

/* ---- morph scripts (the MUTABLE g_morph bank, gstate.h) ------------------- */
#define Gi_explo_size       (morph_at(0x135C)->thresh)  /* explosion script thresh[0]: the AI->render size channel (0x3C/0x50/0x78/0xA0 pulse) */
#define Gw_explo_size       (*(u16 *)&morph_at(0x135C)->thresh)  /* unsigned view */
#define Gi_missile_thresh0  (morph_at(0x1390)->thresh)  /* missile script thresh[0]; morph_patch doubles it (<<=1) */

/* ---- enemy prototypes (the MUTABLE g_protos pool, ff_game.h) -------------- */
#define Gp_proto_gun        ((u8 *)&g_protos[0])  /* prototype[0]: the player GUN shot (also the pool-reset filler) */
#define Gp_proto_missile    ((u8 *)&g_protos[1])  /* prototype[1]: the player homing missile */

/* ---- input flags (g_in; set by poll_controls fn0A0D_0002 / the demo RLE) - */
#define Gw_btn_fire         (g_in.fire)         /* w24B7 */
#define Gw_btn_start        (g_in.start)        /* t24B9: AUX/START; consumes fire when held */
#define Gw_btn_accel        (g_in.accel)        /* w24BB: accelerate (UP) */
#define Gw_btn_brake        (g_in.brake)        /* w24BD: brake (DOWN/kp5) */
#define Gw_btn_right        (g_in.right)        /* w24BF: RIGHT arrow -> car right */
#define Gw_btn_left         (g_in.left)         /* w24C1: LEFT arrow -> car left */
#define Gw_mouse_fire       (g_in.mouse_fire)   /* a24C3 (always 0 in the port) */
#define Gw_mouse_start      (g_in.mouse_start)  /* t24C5 */
#define Gw_mouse_up         (g_in.mouse_up)     /* w24C7 */
#define Gw_mouse_down       (g_in.mouse_down)   /* w24C9 */
#define Gw_mouse_right      (g_in.mouse_right)  /* w24CB */
#define Gw_mouse_left       (g_in.mouse_left)   /* w24CD */
#define Gw_mouse_fire_b     (g_in.mouse_fire_b) /* t24CF */
#define Gw_mouse_start_b    (g_in.mouse_start_b)/* t24D1 */
#define Gw_joy_up           (g_in.joy_up)       /* w24D3 (always 0 in the port) */
#define Gw_joy_down         (g_in.joy_down)     /* w24D5 */
#define Gw_joy_right        (g_in.joy_right)    /* w24D7 */
#define Gw_joy_left         (g_in.joy_left)     /* w24D9 */
#define Gw_w24E7            (g_tim.w24E7)       /* w24E7: zeroed at init; semantics unproven */
#define Gw_w24E9            (g_tim.w24E9)       /* w24E9: zeroed at init + on take-off; semantics unproven */

/* ---- timers / presentation (g_tim) --------------------------------------- */
#define Gi_tick_timer       (g_tim.tick_timer)  /* t24F1: ISR countdown; SIGNED, goes negative */
#define Gw_tick_timer       (*(u16 *)&g_tim.tick_timer)  /* unsigned view (the &4 demo-mode gate) */
#define Gb_playfield_h      (g_tim.playfield_h) /* t24F3: playfield height (0x98 race / 0xF8) */
#define Gw_playfield_w      (g_tim.playfield_w) /* w24F5: playfield width (0x140) */
#define Gw_w24F7            (g_tim.w24F7)       /* w24F7: zeroed at init; semantics unproven */
#define Gw_blink            (g_tim.blink)       /* w24FF: blink parity, ^=1 once per frame */
#define Gw_scale_budget     (g_vm.scale_budget) /* w2505: composer scaler budget (0x80 PUT / 0xC0 GETCHAR wait) */

/* ---- score / stage (g_score) ---------------------------------------------- */
#define Gw_stage            (g_score.stage)     /* t27C7: stage index 0..4 (5 = finale script) */
#define Gi_stage            (*(i16 *)&g_score.stage)
#define Gw_score_lo         (g_score.score_lo)  /* 27c9:27cb 32-bit score */
#define Gw_score_hi         (g_score.score_hi)
#define Gw_high_lo          (g_score.high_lo)   /* 27cd:27cf displayed HIGH (table #1 entry) */
#define Gw_high_hi          (g_score.high_hi)
#define Gw_bonus_lo         (g_score.bonus_lo)  /* 27d1:27d3 32-bit bonus */
#define Gb_bonus_lo         (*(u8 *)&g_score.bonus_lo)
#define Gw_bonus_hi         (g_score.bonus_hi)
#define Gw_best_lo          (g_score.best_lo)   /* 27d5:27d7 session best (CONTINUE keeps it) */
#define Gw_best_hi          (g_score.best_hi)
#define Gw_throttle_step    (g_race.throttle_step)  /* w2845: throttle increment (run_level_init = 8) */

/* (palettes and texts are read straight from the named data now:
 *  ffd_pal_race / ffd_pal_patch_* and ffd_txt_* / ffd_panel_msg /
 *  ffd_panel_clear — src/game/data/ui.c) */

/* ---- décor / composer state (g_decor_st, g_in, g_vm, g_hud_st) ------------- */
#define Gw_decor_kind_cur   (g_decor_st.kind_cur)   /* w379F: 0..15 prototype-table cursor */
#define Gb_pause_vis        (g_in.pause_vis)        /* b37A2: pause "squeeze" visual state */
#define Gw_w38A4            (g_vm.w38A4)            /* w38A4: set 1 on VM arm; no reader found */
#define Gb_wave_phase       (g_vm.wave_phase)       /* b38A6: PUT/WAVE op phase */
#define Gb_path_side_latch  (g_vm.path_side_latch)  /* b38A7: buffer-half latch (CLEAR side sync) */
#define Gw_w38A8            (g_vm.w38A8)            /* w38A8: path-VM build tick flag */
#define Gb_build_state      (g_vm.build_state)      /* b38AA: shape-build state */
#define Gw_fbar_shown       (g_hud_st.fbar_shown)   /* w38AB: F bar level drawn */
#define Gw_kbar_shown       (g_hud_st.kbar_shown)   /* w38AD: K bar level drawn */
#define Gw_fuel_shown       (g_hud_st.fuel_shown)   /* w38AF: fuel gauge level drawn */

/* ---- keyboard state bytes (INT9 keystate array, de-DGROUP'd: g_keystate
 * [scancode] — was the @0x3FE3 region; the alias = base + the SAME scancode) */
#define Gb_key_T            (g_keystate[0x14])  /* 'T' (pause chord) */
#define Gb_key_I            (g_keystate[0x17])  /* 'I' (pause chord) */
#define Gb_key_enter        (g_keystate[0x1C])  /* ENTER */
#define Gb_key_space        (g_keystate[0x39])  /* SPACE */
#define Gb_key_kp7          (g_keystate[0x47])  /* kp7 */
#define Gb_key_up           (g_keystate[0x48])  /* UP */
#define Gb_key_kp9          (g_keystate[0x49])  /* kp9 (fire AND aux) */
#define Gb_key_left         (g_keystate[0x4B])  /* LEFT */
#define Gb_key_kp5          (g_keystate[0x4C])  /* kp5 */
#define Gb_key_right        (g_keystate[0x4D])  /* RIGHT */
#define Gb_key_down         (g_keystate[0x50])  /* DOWN */
#define Gb_exit_flag        (g_in.exit_flag)    /* b40A2: exit/edge event flag (name-entry timeout) */
#define Gb_b40A7            (g_in.b40A7)        /* b40A7: loop guard flag; semantics unproven */

/* ---- the two VMs (g_vm; PCs are OFFSETS into the G bytecode) ---------------- */
#define Gw_path_pc          (g_vm.path_pc)          /* tD7B4: PATH-VM program counter */
#define Gw_wave_pc          (g_vm.wave_pc)          /* tF6BA: WAVE-VM program counter */
#define Gw_wave_ret         (g_vm.wave_ret)         /* ptrF7C6: wave-VM GOSUB return */
#define Gw_path_ret         (g_vm.path_ret)         /* ptrF7D5: path-VM GOSUB return */
#define Gw_wave_side        (g_vm.wave_side)        /* wF7C0: wave build side flag */
#define Gw_wave_rearm       (g_vm.wave_rearm)       /* wF7C2: side re-arm latch (CLEAR) */
#define Gw_marker_idx       (g_vm.marker_idx)       /* wF7C4: GETCHAR marker read cursor */
#define Gb_wave_count       (g_vm.wave_count)       /* bF7CA: WAVE drip count latch */
#define Gw_wave_deadline_lo (g_vm.deadline_lo)      /* tF7CB: WAVE next-spawn deadline (lo) */
#define Gw_wave_deadline_hi (g_vm.deadline_hi)
#define Gw_shape_cursor     (g_vm.shape_cursor)     /* tF7CF: composer stream cursor */
#define Gw_path_marker_idx  (g_vm.path_marker_idx)  /* wF7D3: path-VM marker write cursor */
#define Gw_buf_half         (g_vm.buf_half)         /* wF7D9: runtime bank half (0 / 0x4000) */
#define Gb_path_side        (g_vm.path_side)        /* bF7DB: path-VM side sync flag */
#define Gi_shape_marker0    (g_vm.markers[0])       /* wF46C[0]: shape marker (-idx building / +idx ready) */
#define G_vm_marker(i)      (g_vm.markers[i])       /* wF46C[i] (was GI(0xF46C + (i)*2)) */

/* ---- race core state (g_race) ----------------------------------------------- */
#define Gw_lives            (g_race.lives)          /* tDC68: player lives/vehicles */
#define Gi_lives            (*(i16 *)&g_race.lives)
#define Gb_carrier_flag     (g_race.carrier_flag)   /* ba7a: carrier / weapon-crate active flag */
#define Gb_aircraft_flag    (g_race.aircraft_flag)  /* ba7b: parent-aircraft-alive flag */
#define Gw_ptrDC68_seg      (*(u16 *)&g_race.carrier_flag)  /* ptrDC68 far-ptr SEG half (display_init) */
#define Gw_fuel_window      (g_race.fuel_window)    /* wDC6C: fuel/road window; "invuln_timer" was a misnomer */
#define Gi_fuel_window      (*(i16 *)&g_race.fuel_window)
#define Gb_stage_clear      (g_race.stage_clear)    /* bDC6E: anim_step — boss death = stage clear */
#define Gb_demo_flag        (g_race.demo_flag)      /* bDC6F: attract-demo mode (menu timeout) */
#define Gw_kero             (g_race.kero)           /* wDC78: kerosene/K-bar level */
#define Gi_kero             (*(i16 *)&g_race.kero)
#define Gw_decor_count      (g_decor_st.count)      /* wDC7E: live décor-ring entries */
#define Gw_car_x            (g_race.car_x)          /* wDD84 (bb94): car lateral position */
#define Gi_car_x            (*(i16 *)&g_race.car_x)
#define Gw_horizon          (g_race.horizon)        /* wDD86 (bb96): horizon/altitude */
#define Gi_horizon          (*(i16 *)&g_race.horizon)
#define Gb_panel_msg        (g_panel.msg)           /* bDE50 (bc60): panel message index */
#define Gb_panel_delay      (g_panel.delay)         /* bDE51: message re-arm countdown */
#define Gb_panel_state      (g_panel.state)         /* bDE52 (bc62): 0 idle / 1 start / 0xFF typing */
#define Gw_panel_cursor     (g_panel.cursor)        /* tDE53 (bc63): typewriter offset (into ffd_panel_texts) */
#define Gw_pen_x            (g_panel.pen_x)         /* wF7B8 (d5c8): typewriter pen X */
#define Gw_pen_y            (g_panel.pen_y)         /* wF7BA (d5ca): typewriter pen Y */
#define Gw_ring_tail        (g_decor_st.tail)       /* tDE5D: ring draw/recycle cursor */
#define Gb_ring_tail        (*(u8 *)&g_decor_st.tail)
#define Gw_cflash_x         (g_race.cflash_x)       /* bc77: cockpit-flash overlay x */
#define Gw_spr_count_saved  (g_race.spr_count_saved)/* tDE69: directory count saved by main */
#define Gw_cflash_y         (g_race.cflash_y)       /* bc7b: cockpit-flash overlay y */
#define Gb_crash_ctr        (g_race.crash_ctr)      /* bDE6D (bc7d): crash counter 0x18 -> 0 */
/* (wDE6E ramp LUT moved out of G: g_ramp_tbl — ff_game.h) */
#define Gw_wDEAE            (g_tim.wDEAE)  /* wDEAE: shade tail seed (read-only zero) */
#define Gw_wE4CA            (g_tim.wE4CA)           /* wE4CA: zeroed at init; semantics unproven */
#define Gw_missile_fuel     (g_race.missile_fuel)   /* wE9C8: missile fuel/engine level */
#define Gi_missile_fuel     (*(i16 *)&g_race.missile_fuel)
#define Gw_speed            (g_race.speed)          /* tE9CC: scroll/vehicle speed */
#define Gi_speed            (*(i16 *)&g_race.speed)
#define Gw_wE9CE            (g_tim.wE9CE)           /* wE9CE: zeroed at init; semantics unproven */
#define Gw_credits          (g_race.credits)        /* tEA3A: inserted credits / players */
#define Gw_dist_lo          (g_race.dist_lo)        /* tEA3C:wEA3E 32-bit distance accumulator */
#define Gw_dist_hi          (g_race.dist_hi)
#define Gw_cloud_acc0       (g_race.cloud_acc[0])   /* cloud parallax band accumulators */
#define Gw_cloud_acc1       (g_race.cloud_acc[1])
#define Gw_cloud_acc2       (g_race.cloud_acc[2])
#define Gw_cloud_acc3       (g_race.cloud_acc[3])
#define Gw_cloud_acc4       (g_race.cloud_acc[4])
#define Gw_ring_head        (g_decor_st.head)       /* tEF4A: ring write cursor */
#define Gb_ring_head        (*(u8 *)&g_decor_st.head)
#define Gw_mtn_scroll       (g_race.mtn_scroll)     /* wEF50 (cd60): mountain scroll; PERSISTS across races */
#define Gw_spr_count        (g_race.spr_count)      /* tEF52: live sprite-directory count */
#define Gb_pickup_banner    (*(u8 *)&g_decor_st.pickup_banner)  /* bEF56 (cd66) */
#define Gw_pickup_banner    (g_decor_st.pickup_banner)  /* the ROM's WORD write (see gstate.h) */
#define Gw_cflash_spr       (g_race.cflash_spr)     /* tF085 (ce95): cockpit-flash sprite, 0 = none */
#define Gb_cflash_spr       (*(u8 *)&g_race.cflash_spr)
#define Gw_road_center      (g_race.road_center)    /* wF1FD: road perspective centre = 0xA0 - car_x */
#define Gw_flash_colour     (g_race.flash_colour)   /* d013: damage-flash colour source */

/* ---- demo replay / décor spawn (g_in, g_decor_st, g_hud_st) ------------------ */
#define Gb_rle_count        (g_in.rle_count)        /* d271: RLE run-length countdown */
#define Gb_weapon_anim      (g_hud_st.weapon_vars[0])   /* bF462: HUD weapon-icon anim index */
#define Gp_hud_weapon_vars  (g_hud_st.weapon_vars)  /* the 6 HUD weapon bytes zeroed at init */
#define Gw_decor_density    (g_decor_st.density)    /* wF468: adaptive décor density */
#define Gw_enemy_count      (g_race.enemy_count)    /* wF46A (d27a): live enemy count */
#define Gi_enemy_count      (*(i16 *)&g_race.enemy_count)
#define Gw_track_seg        (g_race.track_seg)      /* wF480: track segment index (cycled by PUT waves!) */
/* (aF482 speed table moved out of G: g_speed_tbl — ff_game.h) */
#define Gw_rle_ptr          (g_in.rle_ptr)          /* d492: RLE stream ptr (offset into G tape data) */
#define Gb_obj_done         (g_race.obj_done)       /* bF688: objective-counter-done latch */

/* ---- combat / mode state (g_race, g_in, g_tim) -------------------------------- */
#define Gw_flash_timer      (g_race.flash_timer)    /* wF6A9 (d4b9): damage-flash/invuln timer */
#define Gw_game_mode        (g_race.game_mode)      /* wF6AF (d4bf): 0..4 game mode */
#define Gi_game_mode        (*(i16 *)&g_race.game_mode)
#define Gb_player_hit       (g_race.player_hit)     /* bF6B1 (d4c1): player hit this frame */
#define Gb_bonus_banner     (g_race.bonus_banner)   /* d4c2: BONUS banner countdown */
#define Gb_demo_abort       (g_race.demo_abort)     /* bF6B3: demo aborted by a keypress */
#define Gw_btn_missile      (g_in.missile)          /* wF6B4: missile button */
#define Gw_frames_per_tick  (g_tim.frames_per_tick) /* wF6B6: measured frames per ISR tick (18) */
#define Gw_kill_ctr         (g_race.kill_ctr)       /* tF6B8 (d4c8): wave kill counter */
#define Gi_kill_ctr         (*(i16 *)&g_race.kill_ctr)
#define Gb_kill_ctr         (*(u8 *)&g_race.kill_ctr)
#define Gw_btn_takeoff      (g_in.takeoff)          /* wF6C2 (d4d2): take-off/land edge */
#define Gw_objective_lo     (g_race.objective_lo)   /* d4d4:d4d6 32-bit objective counter */
#define Gw_objective_hi     (g_race.objective_hi)
#define Gd_objective        (*(u32 *)&g_race.objective_lo)
#define Gw_charge_flag      (g_race.charge_flag)    /* d4d8: finale charge-truck trigger */
#define Gw_track_ptr        (g_race.track_ptr)      /* tF6CD: road-curve ptr (offset into G track data) */
#define Gw_shot_count       (g_race.shot_count)     /* wF6D1 (d4e1): live enemy-shot count */
#define Gi_shot_count       (*(i16 *)&g_race.shot_count)
#define Gb_hud_phase        (g_hud_st.hud_phase)    /* bF6D3: HUD gauge interleave phase */
#define Gw_decor_latch      (g_decor_st.latch)      /* wF6D8: one-spawn-per-16-units latch */
#define Gb_obj_sfx_latch    (g_race.obj_sfx_latch)  /* bF6DA: objective-done sfx one-shot latch */
#define Gb_bF6DB            (g_race.bF6DB)          /* bF6DB: zeroed at init; semantics unproven */
#define Gb_leader_sfx       (g_race.leader_sfx)     /* bF6DC: LEADER-SPOTTED sfx queue flag */
#define Gb_aux_edge         (g_in.aux_edge)         /* tF6EA: aux-button edge latch */
#define Gb_pause_edge       (g_in.pause_edge)       /* bF6EB: pause-chord edge latch */
#define Gb_input_mask       (g_in.mask)             /* d4fc: current demo/tape input mask */
#define Gw_chord_edge       (g_in.chord_edge)       /* d4c4: fire+aux chord edge */

/* ---- HUD gauge caches (g_hud_st) ----------------------------------------------- */
#define Gw_high_cache_lo    (g_hud_st.high_cache_lo)    /* tF7E1: HIGH digits redraw cache */
#define Gw_high_cache_hi    (g_hud_st.high_cache_hi)
#define Gw_wF7E5            (g_hud_st.wF7E5)            /* init 0xFFFF; reader unproven */
#define Gw_wF7E7            (g_hud_st.wF7E7)
#define Gw_bonus_cache_lo   (g_hud_st.bonus_cache_lo)   /* tF7E9: BONUS digits redraw cache */
#define Gw_bonus_cache_hi   (g_hud_st.bonus_cache_hi)
#define Gb_orbs_shown       (g_hud_st.orbs_shown)       /* bF7ED: lives-orbs currently drawn */
#define Gb_stage_digit      (g_hud_st.stage_digit)      /* bF7EE: ASCII stage digit for the banner */
#define Gp_stage_digit      ((u8 *)&g_hud_st.stage_digit)   /* read as a C string (nul field follows) */
#define Gb_bF7EF            (g_hud_st.stage_digit_nul)  /* bF7EF: the banner string terminator (0) */
#define Gi_engine_pitch     (*(i16 *)&g_race.engine_pitch)  /* d600: engine-rumble pitch */
#define Gw_engine_pitch     (g_race.engine_pitch)
#define Gi_engine_x         (*(i16 *)&g_race.engine_x)  /* d602: engine-rumble x */
#define Gw_leader_slot      (g_race.leader_slot)        /* d604: leader slot offset (POOL-relative) */
#define Gw_leader_seg       (g_race.leader_seg)         /* d606: leader ptr seg half (0) */

#endif /* FF_GNAMES_H */
