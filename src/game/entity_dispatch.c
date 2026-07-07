/* entity_dispatch.c — native enemy-behavior dispatch table (Phase 1b).
 *
 * Built from meta/enemy_types.csv: 81 enemy types (0..80) -> 53 unique
 * handler names. Each handler below is a WEAK stub;
 * ALL of them are overridden by the strong faithful ports in behaviors.c
 * (the weak defs remain only as the link-time contract for the table).
 *
 * Handler contract (docs/entities.md): void f(Entity *e, int di), di = on-screen
 * distance. Comments carry the original 161c:<off> address(es).
 */
#include "game/entity_dispatch.h"

/* --- weak handler stubs (53 unique; ALL overridden by behaviors.c) --- */
__attribute__((weak)) void ent_smart_bomb(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:0038 */ }
__attribute__((weak)) void ent_homing_missile(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:0205 */ }
__attribute__((weak)) void pickup_shield(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:04c7 */ }
__attribute__((weak)) void pickup_bonus_a(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:047a */ }
__attribute__((weak)) void pickup_speed(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:0508 */ }
__attribute__((weak)) void ent_explosion_small(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:0555 */ }
__attribute__((weak)) void ent_explosion_large(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:060d */ }
__attribute__((weak)) void ent_noop(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:06af/06b4/06b9 */ }
__attribute__((weak)) void ent_basic(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:06be */ }
__attribute__((weak)) void ent_basic_shooter(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:0715 */ }
__attribute__((weak)) void ent_swooper(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:078d */ }
__attribute__((weak)) void ent_periodic_shooter(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:088f */ }
__attribute__((weak)) void ent_basic_alias(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:08f5/22ba/1f61 */ }
__attribute__((weak)) void ent_wobble_shooter(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:0976 */ }
__attribute__((weak)) void ent_dive_bomber(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:0a33 */ }
__attribute__((weak)) void ent_strafer(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:0b4f */ }
__attribute__((weak)) void ent_shooter_b(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:0909 */ }
__attribute__((weak)) void ent_gunship(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:0c1a */ }
__attribute__((weak)) void ent_mine(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:0de6 */ }
__attribute__((weak)) void ent_crash_target(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:0e29 */ }
__attribute__((weak)) void ent_aircraft(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:0ec6 */ }
__attribute__((weak)) void ent_boss_strafer(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:107b */ }
__attribute__((weak)) void ent_interceptor(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:123d */ }
__attribute__((weak)) void ent_kamikaze(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:1483 */ }
__attribute__((weak)) void ent_mine_layer(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:16a3 */ }
__attribute__((weak)) void ent_side_gunner(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:17ab */ }
__attribute__((weak)) void ent_splitter(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:1abe */ }
__attribute__((weak)) void ent_bouncing_hazard(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:1b69 */ }
__attribute__((weak)) void ent_descender(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:1c14 */ }
__attribute__((weak)) void ent_formation_leader(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:1ce7 */ }
__attribute__((weak)) void ent_formation_follower(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:1d9f */ }
__attribute__((weak)) void ent_tracker(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:1e40 */ }
__attribute__((weak)) void ent_bomber(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:18f4 */ }
__attribute__((weak)) void ent_mine_dropper(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:198d */ }
__attribute__((weak)) void ent_strafer_layer(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:1f75 */ }
__attribute__((weak)) void ent_boss_sweeper(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:20ad */ }
__attribute__((weak)) void ent_sine_flyer(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:2175 */ }
__attribute__((weak)) void ent_sine_gunner(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:2203 */ }
__attribute__((weak)) void enemy_ai_default_thunk2(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:22e2 */ }
__attribute__((weak)) void enemy_update_ballistic(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:22f6 */ }
__attribute__((weak)) void enemy_ai_default_thunk3(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:23e0 */ }
__attribute__((weak)) void enemy_ai_default_thunk4(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:23f4 */ }
__attribute__((weak)) void enemy_update_enqueue_top(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:2408 */ }
__attribute__((weak)) void enemy_ai_default_thunk5(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:24c4 */ }
__attribute__((weak)) void enemy_update_offscreen_kill(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:24d8 */ }
__attribute__((weak)) void enemy_update_home_player(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:29fa */ }
__attribute__((weak)) void pickup_weapon(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:1ea6 */ }
__attribute__((weak)) void enemy_update_bounce(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:2a86 */ }
__attribute__((weak)) void enemy_update_split4_descend(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:2504 */ }
__attribute__((weak)) void enemy_update_split4_spawner(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:2648 */ }
__attribute__((weak)) void enemy_update_split4_strafe(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:281f */ }
__attribute__((weak)) void enemy_update_charge_trigger(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:2987 */ }
__attribute__((weak)) void enemy_ai_default_thunk6(Entity *e, int di){ (void)e; (void)di; /* strong port: behaviors.c 161c:2b4f */ }

/* --- type -> handler dispatch table (replaces DGROUP a3A63) --- */
behavior_fn g_behavior[NUM_ENEMY_TYPES] = {
    [0] = ent_smart_bomb,
    [1] = ent_homing_missile,
    [2] = pickup_shield,
    [3] = pickup_bonus_a,
    [4] = pickup_speed,
    [5] = ent_explosion_small,
    [6] = ent_explosion_large,
    [7] = ent_noop,
    [8] = ent_noop,
    [9] = ent_noop,
    [10] = ent_noop,
    [11] = ent_basic,
    [12] = ent_basic_shooter,
    [13] = ent_basic,
    [14] = ent_basic,
    [15] = ent_basic_shooter,
    [16] = ent_swooper,
    [17] = ent_swooper,
    [18] = ent_periodic_shooter,
    [19] = ent_basic_alias,
    [20] = ent_periodic_shooter,
    [21] = ent_wobble_shooter,
    [22] = ent_wobble_shooter,
    [23] = ent_wobble_shooter,
    [24] = ent_dive_bomber,
    [25] = ent_strafer,
    [26] = ent_periodic_shooter,
    [27] = ent_shooter_b,
    [28] = ent_gunship,
    [29] = ent_shooter_b,
    [30] = ent_mine,
    [31] = ent_crash_target,
    [32] = ent_aircraft,
    [33] = ent_boss_strafer,
    [34] = ent_interceptor,
    [35] = ent_kamikaze,
    [36] = ent_mine_layer,
    [37] = ent_side_gunner,
    [38] = ent_splitter,
    [39] = ent_bouncing_hazard,
    [40] = ent_descender,
    [41] = ent_formation_leader,
    [42] = ent_formation_follower,
    [43] = ent_tracker,
    [44] = ent_shooter_b,
    [45] = ent_basic,
    [46] = ent_basic,
    [47] = ent_side_gunner,
    [48] = ent_bomber,
    [49] = ent_mine_dropper,
    [50] = ent_strafer_layer,
    [51] = ent_boss_sweeper,
    [52] = ent_sine_flyer,
    [53] = ent_sine_gunner,
    [54] = ent_basic_alias,
    [55] = ent_aircraft,
    [56] = ent_aircraft,
    [57] = enemy_ai_default_thunk2,
    [58] = enemy_update_ballistic,
    [59] = enemy_ai_default_thunk3,
    [60] = enemy_ai_default_thunk4,
    [61] = ent_gunship,
    [62] = enemy_update_enqueue_top,
    [63] = enemy_ai_default_thunk5,
    [64] = enemy_ai_default_thunk5,
    [65] = enemy_update_offscreen_kill,
    [66] = enemy_update_home_player,
    [67] = ent_basic_alias,
    [68] = pickup_weapon,
    [69] = enemy_update_bounce,
    [70] = ent_basic_alias,
    [71] = ent_basic_alias,
    [72] = ent_basic_alias,
    [73] = enemy_update_split4_descend,
    [74] = enemy_update_split4_spawner,
    [75] = enemy_update_split4_descend,
    [76] = enemy_update_split4_descend,
    [77] = enemy_update_split4_strafe,
    [78] = enemy_update_charge_trigger,
    [79] = enemy_ai_default_thunk6,
    [80] = ent_boss_strafer,
};

/* Bounds-checked lookup; NULL for out-of-range type. */
behavior_fn ff_behavior_for(int type)
{
    if (type < 0 || type >= NUM_ENEMY_TYPES)
        return NULL;
    return g_behavior[type];
}
