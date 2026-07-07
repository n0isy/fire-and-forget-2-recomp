/* entity_dispatch.h — native enemy-behavior dispatch (Phase 1b).
 *
 * Replaces the original 16-bit DGROUP far-pointer table `a3A63`
 * (seg:off pointers into segment 161c, meaningless natively) with a
 * native C function-pointer table built from meta/enemy_types.csv.
 *
 * Handler contract (docs/entities.md): void handler(Entity *e, int di)
 * where `di` = on-screen distance (slot z_pos - camera; signed).
 *
 * There are 81 enemy types (indices 0..80) mapping onto 53 unique
 * handler names (many types share a handler). NOTE: the original ROM has
 * 57 distinct handler *addresses*; two names cover several addresses that
 * behave identically (ent_noop = 161c:06af/06b4/06b9, ent_basic_alias =
 * 161c:08f5/22ba/1f61), so by name there are 53.
 */
#ifndef FF2_ENTITY_DISPATCH_H
#define FF2_ENTITY_DISPATCH_H

#include "ff2.h"   /* Entity, NUM_ENEMY_TYPES */

/* Behavior handler signature: f(slot, di) — di = on-screen distance. */
typedef void (*behavior_fn)(Entity *, int);

/* type index (0..NUM_ENEMY_TYPES-1) -> ported handler. */
extern behavior_fn g_behavior[NUM_ENEMY_TYPES];

/* Bounds-checked lookup; returns NULL for out-of-range type. */
behavior_fn ff_behavior_for(int type);

/* --- unique enemy-behavior handlers (53; weak stubs in entity_dispatch.c,
 *     real ports override them later) --- */
void ent_smart_bomb(Entity *e, int di);
void ent_homing_missile(Entity *e, int di);
void pickup_shield(Entity *e, int di);
void pickup_bonus_a(Entity *e, int di);
void pickup_speed(Entity *e, int di);
void ent_explosion_small(Entity *e, int di);
void ent_explosion_large(Entity *e, int di);
void ent_noop(Entity *e, int di);
void ent_basic(Entity *e, int di);
void ent_basic_shooter(Entity *e, int di);
void ent_swooper(Entity *e, int di);
void ent_periodic_shooter(Entity *e, int di);
void ent_basic_alias(Entity *e, int di);
void ent_wobble_shooter(Entity *e, int di);
void ent_dive_bomber(Entity *e, int di);
void ent_strafer(Entity *e, int di);
void ent_shooter_b(Entity *e, int di);
void ent_gunship(Entity *e, int di);
void ent_mine(Entity *e, int di);
void ent_crash_target(Entity *e, int di);
void ent_aircraft(Entity *e, int di);
void ent_boss_strafer(Entity *e, int di);
void ent_interceptor(Entity *e, int di);
void ent_kamikaze(Entity *e, int di);
void ent_mine_layer(Entity *e, int di);
void ent_side_gunner(Entity *e, int di);
void ent_splitter(Entity *e, int di);
void ent_bouncing_hazard(Entity *e, int di);
void ent_descender(Entity *e, int di);
void ent_formation_leader(Entity *e, int di);
void ent_formation_follower(Entity *e, int di);
void ent_tracker(Entity *e, int di);
void ent_bomber(Entity *e, int di);
void ent_mine_dropper(Entity *e, int di);
void ent_strafer_layer(Entity *e, int di);
void ent_boss_sweeper(Entity *e, int di);
void ent_sine_flyer(Entity *e, int di);
void ent_sine_gunner(Entity *e, int di);
void enemy_ai_default_thunk2(Entity *e, int di);
void enemy_update_ballistic(Entity *e, int di);
void enemy_ai_default_thunk3(Entity *e, int di);
void enemy_ai_default_thunk4(Entity *e, int di);
void enemy_update_enqueue_top(Entity *e, int di);
void enemy_ai_default_thunk5(Entity *e, int di);
void enemy_update_offscreen_kill(Entity *e, int di);
void enemy_update_home_player(Entity *e, int di);
void pickup_weapon(Entity *e, int di);
void enemy_update_bounce(Entity *e, int di);
void enemy_update_split4_descend(Entity *e, int di);
void enemy_update_split4_spawner(Entity *e, int di);
void enemy_update_split4_strafe(Entity *e, int di);
void enemy_update_charge_trigger(Entity *e, int di);
void enemy_ai_default_thunk6(Entity *e, int di);

#endif /* FF2_ENTITY_DISPATCH_H */
