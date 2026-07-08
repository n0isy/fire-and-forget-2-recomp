#ifndef FF2_PROTO_H
#define FF2_PROTO_H

#include "ff2_types.h"

/* HISTORICAL SIGNATURES: the decompiled 16-bit functions took the DGROUP
 * pointer (`struct Globals *ds`). The DGROUP image no longer exists in the
 * port (fully de-DGROUP'd into typed blocks); the incomplete type below only
 * keeps these reference prototypes compiling. */
struct Globals;

/* Prototypes for all reverse-engineered FF2 functions.
 * Signatures simplified from Reko / Ghidra decompiler output:
 *   word16/Eq_* -> u16, byte -> u8, int16 -> i16, far/dword -> u32/farptr_t,
 *   DGROUP ds pointer -> struct Globals*, out-references -> pointers,
 *   unknown struct pointers -> void*.
 * Address comment = Ghidra seg:off. [drop: libc/DOS] = replace at port time. */

/* ==== runtime ==== */
void rt_startup(void);  // 1000:0000 [drop: libc/DOS]
void rt_cmdline_split(u16 param_1, u16 param_2);  // 1000:010d [drop: libc/DOS]
void rt_dos_get_info(void);  // 1000:012f [drop: libc/DOS]
void rt_dos_setup_io(void);  // 1000:0172 [drop: libc/DOS]
void rt_clear_argc(void);  // 1000:019f [drop: libc/DOS]
void rt_nop(void);  // 1000:01a6 [drop: libc/DOS]
void rt_dos_call(void);  // 1000:01a7 [drop: libc/DOS]
void rt_exit_abort(void);  // 1000:01af [drop: libc/DOS]
void rt_setargv(void);  // 1000:01d1 [drop: libc/DOS]
i16 rt_arg_scan_char(void);  // 1000:025f [drop: libc/DOS]
void rt_alloc_argv(void);  // 1000:02cf [drop: libc/DOS]
u16 rt_dos_errno(struct Globals *ds, u16 wArg04);  // 1d32:0008 [drop: libc/DOS]
void rt_stub_1d32(void);  // 1d32:0043 [drop: libc/DOS]
void rt_exit(u16 param_1);  // 1d36:0004 [drop: libc/DOS]
u8 rt_atexit(u16 param_1, u16 param_2);  // 1d36:0037 [drop: libc/DOS]
u16 rt_malloc(struct Globals *ds, u16 wArg04, u16 *dxOut);  // 1d3c:0006 [drop: libc/DOS]
void rt_heap_unlink(struct Globals *ds, u16 ptrArg04);  // 1d3c:001b [drop: libc/DOS]
u16 rt_heap_split(struct Globals *ds, u16 ptrArg04, u32 dwArg08, u16 *dxOut);  // 1d3c:0083 [drop: libc/DOS]
u16 rt_heap_morecore2(struct Globals *ds, u32 dwArg04, u16 *dxOut);  // 1d3c:0133 [drop: libc/DOS]
u16 rt_heap_morecore_init(struct Globals *ds, u32 dwArg04, u16 *dxOut);  // 1d3c:01a1 [drop: libc/DOS]
u16 rt_malloc_core(struct Globals *ds, u32 dwArg04, u16 *dxOut);  // 1d3c:0207 [drop: libc/DOS]
u16 rt_heap_extend_break(struct Globals *ds, void *ptrArg02);  // 1d6d:0006 [drop: libc/DOS]
u16 rt_heap_extend_check(u16 param_1, u16 param_2);  // 1d6d:0096 [drop: libc/DOS]
u16 rt_heap_sbrk(struct Globals *ds, u32 dwArg04);  // 1d6d:00dd [drop: libc/DOS]
u16 rt_dos_setblock(u8 al, struct Globals *ds, void *psegArg04, u16 wArg06);  // 1d82:0008 [drop: libc/DOS]
u16 rt_dos_allocmem(u8 al, struct Globals *ds, u16 wArg04, u16 ptrArg06);  // 1dde:0003 [drop: libc/DOS]

/* ==== copyprotect ==== */
u16 copyprotect_check_writeprotect(void);  // 1c3a:0008 [drop: libc/DOS]
void fdc_send_byte(u8 ah);  // 1c3a:0057 [drop: libc/DOS]
u8 fdc_read_byte(void);  // 1c3a:0066 [drop: libc/DOS]
void install_int24_handler(void);  // 1c3a:0072 [drop: libc/DOS]
void copyprotect_fail_reset(struct Globals *ds, u16 wArg04);  // 1c3a:0d51 [drop: libc/DOS]
void copyprotect_restore_vectors(void);  // 1d11:00f5 [drop: libc/DOS]
void copyprotect_verify_disk(void);  // 1d11:0118 [drop: libc/DOS]
void copyprotect_done_stub(void);  // 1d11:01dd [drop: libc/DOS]

/* ==== fileio ==== */
u16 rt_creat(struct Globals *ds, u16 wArg02, char *ptrArg04, u16 *cxOut);  // 1d84:0008
u16 rt_chsize_zero(struct Globals *ds, u16 wArg02, u16 *bxOut);  // 1d84:0027
u16 rt_open(struct Globals *ds, char *dwArg04, u16 wArg08, u16 wArg0A, u16 *cxOut, u16 *bxOut);  // 1d84:003b
u16 rt_dos_open(struct Globals *ds, char *ptrArg04, u16 wArg08, u16 *cxOut, u16 *bxOut);  // 1d9e:0008
u16 rt_ioctl(struct Globals *ds, u16 wArg06, u16 *cxOut, u16 *bxOut);  // 1da3:0005
void rt_close(u8 al, struct Globals *ds, u16 wArg04);  // 1da6:0002
void rt_dos_close(u8 al, struct Globals *ds, u16 wArg04);  // 1da9:0003
void rt_read(struct Globals *ds, u16 wArg04, u16 ptrArg06, u16 wArg0A);  // 1dab:0009
u16 rt_dos_read(struct Globals *ds, u16 wArg04, u16 ptrArg06, u16 wArg0A);  // 1db7:000e
u8 rt_write(struct Globals *ds, u16 wArg04, char *ptrArg06, u16 wArg0A);  // 1db9:000e
u16 rt_dos_write(struct Globals *ds, u16 wArg04, char *ptrArg06, u16 wArg0A);  // 1dd0:0004
void rt_lseek(struct Globals *ds, u16 wArg04, void *dwArg06, u8 bArg0A);  // 1dd5:0000
u16 rt_get_set_fattr(struct Globals *ds, u8 bArg08, u16 *cxOut);  // 1dd7:000d
void rt_filelength(struct Globals *ds, u16 wArg04);  // 1dd9:000e

/* ==== asset_pipeline ==== */
void load_anim(u16 bp, u16 si, u16 di, struct Globals *ds, char *dwArg04);  // 120d:0394
u16 load_anim_cond(u16 si, struct Globals *ds, char *dwArg04, char *dwArg08, u16 wArg0C);  // 120d:0483
u16 load_anim_dims(u16 si, struct Globals *ds, char *dwArg04, u16 *dxOut, u16 *bxOut);  // 120d:04e1
u16 load_level(u16 bp, u16 si, u16 di, struct Globals *ds, u16 *bpOut);  // 13a8:0ac5
void remap_palette_ega(struct Globals *ds, u8 *ptrArg04, u8 *ptrArg08);  // 13a8:0bce
u16 rle_unpack_packbits(void *ptrArg04, u8 *ptrArg08);  // 13a8:0d1c
u16 str_equal_z(u8 *ptrArg04, u8 *ptrArg08);  // 13a8:0d94
void blit_image_planes(struct Globals *ds, u32 ptrArg04);  // 18ef:0521
void copy_image_rows(void *si, void *di, void *es, void *ds);  // 18ef:0559
void ega_set_palette(void *cx, u16 bx, struct Globals *ds, u32 ptrArg04);  // 1987:2a6f
void depack_powerpacker(u16 wArg04, u16 wArg06, u16 ptrArg08, u16 ptrArg0C);  // 1c3a:00b2
u16 depack_read_bits(void *ds_si, u16 ax, u16 bp, u16 *bpOut);  // 1c3a:021f
u16 depack_norm_ptr_hi(u16 ax, u16 bx, u16 *bxOut);  // 1c3a:024f
u16 depack_norm_ptr(u16 ax, u16 cx, u16 dx, u16 bx, u16 *dxOut);  // 1c3a:025f

/* ==== util ==== */
u32 rt_hugeptr_add(u16 cx_bx, u16 ax, u16 dx);  // 1000:0313
u32 rt_hugeptr_cmp(u16 ax, u16 cx, u16 dx, u16 bx);  // 1000:036e
u16 rt_lshr32(u16 dx_ax, u8 cl, u8 *clOut, u16 *bxOut);  // 1000:038f
u16 rt_memcpy_words(u16 cx, void *ptrArg04, u16 ptrArg08);  // 1000:03ad
void rng_seed_init(struct Globals *ds);  // 120d:02d3
u16 rng_next(void);  // 120d:0319

/* ==== game ==== */
u16 stage_hud_init(u16 si, u16 di, struct Globals *ds, u16 *bxOut, u16 *bpOut, u16 *siOut, u16 *diOut, struct Globals *dsOut);  // 15ae:0001
void show_screen_timed(u16 si, u16 di, struct Globals *ds);  // 15ae:00d5
u16 stage_controller(struct Globals *ds, struct Globals *siOut, u16 *diOut, struct Globals *dsOut);  // 15ae:0118
u16 bonus_stage_loop(u16 di, struct Globals *ds, u16 *bpOut, u16 *siOut, u16 *diOut, struct Globals *dsOut);  // 15ae:02d0
u16 start_level(u16 bp, u16 si, u16 di, struct Globals *ds, struct Globals *dsOut);  // 15ae:03aa

/* ==== gameloop ==== */
void game_main(u16 di, struct Globals *ds);  // 103c:0009
u16 run_level(struct Globals *ds, u16 *bxOut);  // 1069:0006
u16 round_transition(u16 cx, u16 bx, u16 si, struct Globals *ds, u16 *bpOut, u16 *siOut, u16 *diOut);  // 13a8:048a

/* ==== input ==== */
u16 poll_controls(u16 ax, u16 cx, u16 bx, struct Globals *ds);  // 120d:0002
void demo_play_step(struct Globals *ds);  // 120d:0251
void input_timer_install(void);  // 194c:0007
void kbd_isr_setup(void);  // 194c:0012
void kbd_isr_install(void);  // 194c:0046
void kbd_isr_restore(void);  // 194c:0059
void joystick_calibrate(struct Globals *ds);  // 194c:00ea
void joystick_read(u8 al, struct Globals *ds);  // 194c:01e2
void mouse_init(struct Globals *ds);  // 194c:02f3
void mouse_read(u16 cx, u16 dx, u8 bl, struct Globals *ds);  // 194c:0351

/* ==== graphics ==== */
u8 build_shade_tables(struct Globals *ds, u16 *bxOut);  // 1069:1480
void terrain_anim_next(struct Globals *ds);  // 1069:154d
u16 terrain_anim_init(struct Globals *ds);  // 1069:158d
void road_perspective(struct Globals *ds);  // 1069:15d6
struct Globals * load_level_sheet(void *bp, u16 si, u16 di, struct Globals *ds, u16 *bpOut);  // 120d:0adb
void draw_tiled_bg(struct Globals *ds);  // 120d:0bc7
void init_parallax_layers(struct Globals *bp, u16 si, u16 di, struct Globals *ds);  // 13a8:01c9
void set_palette(u32 dwArg04);  // 13a8:02d5
void decode_blit_sprite(u16 param_1, u16 param_2);  // 13a8:0dc1
u16 wipe_in(struct Globals *ds);  // 13a8:1315
u16 wipe_out(struct Globals *ds, u16 *cxOut);  // 13a8:1325
i16 spiral_wipe(struct Globals *ds, u16 wArg04);  // 13a8:132f
i16 setup_level_background(struct Globals *bp, u16 si, u16 di, struct Globals *ds, u16 *cxOut, u16 *bxOut, u16 *bpOut);  // 15ae:05e2
void ega_clear_screen(struct Globals *ds);  // 18ef:0008
void draw_vector_lines(struct Globals *ds, u16 wArg04);  // 18ef:0051
void line_raster_setup(u16 *ds_si, u16 ax, u16 cx, i16 dx, i16 bx);  // 18ef:00f7
void * line_emit_point(u16 ax, i16 bx, void *si, void *ds);  // 18ef:017e
void blit_points_span(struct Globals *si, void *es, struct Globals *ds);  // 18ef:01c0
void blit_points_xor(struct Globals *si, void *es, struct Globals *ds);  // 18ef:0222
void blit_points_edges(struct Globals *si, void *es, struct Globals *ds);  // 18ef:0252
void plot_hspan(u16 ax, u16 bx, struct Globals *si, void *es, struct Globals *ds);  // 18ef:0291
void blit_bg_hscroll(struct Globals *ds, void *ptrArg04);  // 18ef:0307
void draw_packed_tiles(struct Globals *ds, void *ptrArg04);  // 18ef:039f
void blit_sprite_doublebank(u16 wArg04, u16 wArg06, u32 ptrArg08);  // 18ef:04c7
void shift_buffer_left1(u32 ptrArg04);  // 18ef:056c
void ega_set_plane_mask(u16 wArg04);  // 18ef:05b2
void detect_video_adapter(struct Globals *ds);  // 1987:0009
void init_video_mode(struct Globals *ds);  // 1987:009f
void wait_vretrace(struct Globals *ds);  // 1987:0136
void clear_page0(struct Globals *ds);  // 1987:01ac
void clear_screen_color(struct Globals *ds, u8 bArg04);  // 1987:01c8
void flip_page(struct Globals *ds);  // 1987:0203
void draw_sprite_masked(struct Globals *ds, u16 ptrArg04, u16 bArg08, u16 wArg0A, i16 wArg0C, u16 wArg0E);  // 1987:0232
void draw_sprite_masked_v2(struct Globals *ds, u32 ptrArg04, u8 bArg08, u16 wArg0A, u16 wArg0C, u16 wArg0E);  // 1987:035f
void unknown_misdecoded_15d6(void);  // 1987:15d6
void draw_sprite4_thunk(void *ptrArg04);  // 1987:17a9
void sprite4_mask_init(void *ptrArg04);  // 1987:17ae
void sample_bg_column(struct Globals *ds, void *ptrArg04);  // 1987:1807
void put_pixel(struct Globals *ds, void *ptrArg04);  // 1987:186d
void sprite4_state_reset(void *ptrArg04);  // 1987:18e4
void sample_pixels4(void *ptrArg04);  // 1987:191b
void blit_pixel4_masked(void *ptrArg04);  // 1987:199d
void sprite_blit_init(void *ptrArg04);  // 1987:1a30
void fill_rect_clipped(struct Globals *ds, u16 wArg04, u16 wArg06, u16 wArg08, u16 wArg0A, u16 bArg0C);  // 1987:1a95
u16 draw_object_list(struct Globals *ds, u16 *cxOut, u16 *bxOut, u16 *bpOut, u16 *siOut, u16 *diOut, struct Globals *dsOut);  // 1987:1c73
void init_bitreverse_table(struct Globals *ds);  // 1987:263c
void copy_block_pages(struct Globals *ds, u16 wArg04, u16 wArg06);  // 1987:278a
void blit_sprite_shift_or(void *ptrArg04);  // 

/* ==== ega ==== */
void ega_blit_sprite(void *ptrArg04);  // 
void ega_blit_sprite_mirror(void *ptrArg04);  // 
void ega_build_shifted_sprite(void *ptrArg04, u16 ptrArg08);  // 1987:29a5
void ega_blit_image(struct Globals *ds, u32 ptrArg04, u8 bArg08, u16 wArg0A, u16 wArg0C, u16 wArg0E);  // 1987:2abc

/* ==== sprite ==== */
void object_shadow_project(struct Globals *ds, u16 ptrArg04);  // 1069:1726
void hud_draw_value(struct Globals *ds, u32 dwArg04, u16 wArg08, u16 wArg0A, u16 wArg0C);  // 120d:034c
void enqueue_enemy_sprite(struct Globals *ds, u16 ptrArg04);  // 120d:053e
u16 enqueue_sprite(struct Globals *ds, i16 wArg04, u16 wArg06, u16 wArg08, u16 *bxOut);  // 120d:07cf
void enqueue_sprite_sorted(struct Globals *ds, u16 wArg04, i16 wArg06, u16 wArg08, u16 wArg0A);  // 120d:082a
void enqueue_sprite_v2(u16 param_1, u16 param_2, i16 param_3);  // 120d:0876
void enqueue_sprite_p(struct Globals *ds, i16 wArg04, u16 wArg06, u16 wArg08, u16 wArg0A);  // 120d:08d7
void enqueue_sprite_sorted_v2(u16 param_1, u16 param_2, i16 param_3, u16 param_4, u16 param_5);  // 120d:0939
u16 enqueue_box(struct Globals *ds, u16 wArg04, u16 wArg06, u16 wArg08, u16 wArg0A, u16 wArg0C, u16 *bxOut);  // 120d:098c
void display_list_init(struct Globals *ds);  // 120d:09e4
void sprite_dir_register(struct Globals *ds);  // 120d:0b35
u16 sprite_width(struct Globals *ds, u16 wArg04);  // 120d:0b97
u16 sprite_height(struct Globals *ds, u16 wArg04);  // 120d:0baf
void patch_anim_refs(struct Globals *ds);  // 120d:0c8f
void register_vector_shape(struct Globals *ds, u16 wArg04, u16 wArg06, void *ptrArg08);  // 120d:116a
void register_shape_list(struct Globals *ds, void *ptrArg04);  // 120d:133b
u8 draw_rle_row(void *ptrArg04);  // 120d:13fc
u8 rle_stream_step(struct Globals *ds, void *ptrArg04, u16 wArg08);  // 120d:1594
u16 rle_measure_width(void *ptrArg04, u16 wArg08);  // 120d:17c8
void rle_blit_column(void *ptrArg04);  // 120d:186e
void draw_sprite_simple(u16 param_1, u16 param_2);  // 120d:1976
i16 draw_text_row(struct Globals *ds, u8 *ptrArg04, i16 wArg08, u16 wArg0A, u16 *cxOut, u16 *bxOut, struct Globals *dsOut);  // 13a8:000b
void displist_insert(struct Globals *ds, u16 wArg04);  // 13a8:1fc8
u16 displist_reset(struct Globals *ds, u16 *bxOut);  // 13a8:203a

/* ==== entity ==== */
void spawn_enemy_shot(u32 param_1);  // 13a8:1a1e
void aim_projectile(u8 *param_1);  // 13a8:1ac0
u16 entities_update(struct Globals *ds, i16 wArg04, u16 *diOut);  // 15ae:0446
u16 hit_test_narrow(u32 param_1);  // 15ae:056a
i16 count_active_subentities(void);  // 161c:0009
/* Enemy behavior handlers — uniform native contract f(Entity*, int di), di = on-screen
 * distance (docs/entities.md). Canonical names from meta/enemy_types.csv; reconciled
 * here (Phase 1b) so the dispatch table and later real ports agree. 53 unique names
 * over 57 ROM addresses: ent_noop = 06af/06b4/06b9, ent_basic_alias = 08f5/22ba/1f61. */
void ent_smart_bomb(Entity *e, int di);  // 161c:0038
void ent_homing_missile(Entity *e, int di);  // 161c:0205
void pickup_bonus_a(Entity *e, int di);  // 161c:047a
void pickup_shield(Entity *e, int di);  // 161c:04c7
void pickup_speed(Entity *e, int di);  // 161c:0508
void ent_explosion_small(Entity *e, int di);  // 161c:0555
void ent_explosion_large(Entity *e, int di);  // 161c:060d
void ent_noop(Entity *e, int di);  // 161c:06af/06b4/06b9
void ent_basic(Entity *e, int di);  // 161c:06be
void ent_basic_shooter(Entity *e, int di);  // 161c:0715
void ent_swooper(Entity *e, int di);  // 161c:078d
void ent_periodic_shooter(Entity *e, int di);  // 161c:088f
void ent_basic_alias(Entity *e, int di);  // 161c:08f5/22ba/1f61
void ent_shooter_b(Entity *e, int di);  // 161c:0909
void ent_wobble_shooter(Entity *e, int di);  // 161c:0976
void ent_dive_bomber(Entity *e, int di);  // 161c:0a33
void ent_strafer(Entity *e, int di);  // 161c:0b4f
void ent_gunship(Entity *e, int di);  // 161c:0c1a
void ent_mine(Entity *e, int di);  // 161c:0de6
void ent_crash_target(Entity *e, int di);  // 161c:0e29
void ent_aircraft(Entity *e, int di);  // 161c:0ec6
void ent_boss_strafer(Entity *e, int di);  // 161c:107b
void ent_interceptor(Entity *e, int di);  // 161c:123d
void ent_kamikaze(Entity *e, int di);  // 161c:1483
void ent_mine_layer(Entity *e, int di);  // 161c:16a3
void ent_side_gunner(Entity *e, int di);  // 161c:17ab
void ent_bomber(Entity *e, int di);  // 161c:18f4
void ent_mine_dropper(Entity *e, int di);  // 161c:198d
void ent_splitter(Entity *e, int di);  // 161c:1abe
void ent_bouncing_hazard(Entity *e, int di);  // 161c:1b69
void ent_descender(Entity *e, int di);  // 161c:1c14
void ent_formation_leader(Entity *e, int di);  // 161c:1ce7
void ent_formation_follower(Entity *e, int di);  // 161c:1d9f
void ent_tracker(Entity *e, int di);  // 161c:1e40
void pickup_weapon(Entity *e, int di);  // 161c:1ea6
void ent_strafer_layer(Entity *e, int di);  // 161c:1f75
void ent_boss_sweeper(Entity *e, int di);  // 161c:20ad
void ent_sine_flyer(Entity *e, int di);  // 161c:2175
void ent_sine_gunner(Entity *e, int di);  // 161c:2203
void enemy_ai_default_thunk2(Entity *e, int di);  // 161c:22e2
void enemy_update_ballistic(Entity *e, int di);  // 161c:22f6
void enemy_ai_default_thunk3(Entity *e, int di);  // 161c:23e0
void enemy_ai_default_thunk4(Entity *e, int di);  // 161c:23f4
void enemy_update_enqueue_top(Entity *e, int di);  // 161c:2408
void enemy_ai_default_thunk5(Entity *e, int di);  // 161c:24c4
void enemy_update_offscreen_kill(Entity *e, int di);  // 161c:24d8
void enemy_update_split4_descend(Entity *e, int di);  // 161c:2504
void enemy_update_split4_spawner(Entity *e, int di);  // 161c:2648
void enemy_update_split4_strafe(Entity *e, int di);  // 161c:281f
void enemy_update_charge_trigger(Entity *e, int di);  // 161c:2987
void enemy_update_home_player(Entity *e, int di);  // 161c:29fa
void enemy_update_bounce(Entity *e, int di);  // 161c:2a86
void enemy_ai_default_thunk6(Entity *e, int di);  // 161c:2b4f
void enemy_reset_idle(u8 *param_1);  // 161c:2b63
void enemy_death_explode_score(u8 *param_1);  // 161c:2b95

/* ==== enemy_ai ==== */
void enemy_ai_default_thunk1(u16 param_1, u16 param_2, u16 param_3);  // 161c:22ce

/* ==== collision ==== */
u16 hit_test_wide(u32 param_1);  // 15ae:05a6

/* ==== particle ==== */
u16 update_particles(struct Globals *ds);  // 1069:13c0
void draw_effect_ring(struct Globals *ds);  // 1069:17b9
u16 reset_effect_pools(struct Globals *ds);  // 1069:19e7
u8 play_effect_anim(struct Globals *ds, u16 *bxOut);  // 13a8:00a1

/* ==== hud ==== */
struct Globals * hud_update(u16 cx, struct Globals *ds, u16 wArg04);  // 13a8:1b27
u16 format_hex16(void);  // 18dc:0006
void format_hex_byte(void);  // 18dc:002d
void format_hex_nibble_hi(void);  // 18dc:0034
void format_hex_nibble(void);  // 18dc:0043
void format_decimal32(struct Globals *ds, u32 dwArg04, u16 wArg08, u16 wArg0A, struct Globals *psegArg0C);  // 18dc:004f

/* ==== ui ==== */
u16 highscore_name_entry(u16 cx, u16 si, struct Globals *ds, u16 *dsOut);  // 13a8:0e13
void highscore_commit(u16 bp, u16 si, u16 di, struct Globals *ds);  // 13a8:12f4

/* ==== script ==== */
void spawn_trail_particle(struct Globals *ds);  // 1069:1946
u16 vector_anim_step(struct Globals *ds, u16 ptrArg04);  // 120d:0d08
u16 morph_anim_step(struct Globals *ds, u32 ptrArg04, u16 wArg08);  // 120d:11f3
u16 spawn_enemy(struct Globals *ds, u8 *ptrArg04, u16 *dxOut);  // 13a8:038b
void script_vm_step(struct Globals *ds, u16 wArg04);  // 13a8:13fd
void path_vm_step(struct Globals *ds, u16 wArg04, i16 wArg06);  // 13a8:175d

/* ==== sound ==== */
void update_engine_sound(struct Globals *ds, u16 wArg04);  // 15ae:0661
void sfx_play(struct Globals *ds, u16 wArg04);  // 1c3a:027b
void sfx_load_table(u16 wArg04, u16 wArg06, void *ptrArg08);  // 1c3a:02f2
void sound_init(struct Globals *ds);  // 1c3a:0349
void music_load_song(struct Globals *ax, u8 cl, u16 bx, struct Globals *ds);  // 1c3a:0429
void adlib_silence_all(struct Globals *ds);  // 1c3a:0482
u8 adlib_detect(struct Globals *ds, u8 *chOut);  // 1c3a:04ab
void sound_shutdown(struct Globals *ds);  // 1c3a:04e6
void sfx_trigger(u8 ch, struct Globals *ds, u16 wArg04, u16 wArg06);  // 1c3a:05b7
void sfx_queue(u8 ch, struct Globals *ds, u16 wArg04);  // 1c3a:0643
u16 music_player_isr(void);  // 1c3a:0901
void adlib_note_update(void);  // 1c3a:0a4b
u16 music_play_voice(void);  // 1c3a:0ae5
void adlib_set_reg1(void);  // 1c3a:0b0e
void adlib_note_on(void);  // 1c3a:0b76
void adlib_note_freq(void);  // 1c3a:0b7d
void adlib_write5_forced(void);  // 1c3a:0cb4
void adlib_write5(void);  // 1c3a:0ce6
void adlib_write_reg(u16 ax, struct Globals *ds);  // 1c3a:0d18
u16 adlib_write_reg_forced(void);  // 1c3a:0d38

#endif /* FF2_PROTO_H */
