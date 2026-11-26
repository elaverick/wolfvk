#ifndef WOLFVK_ENEMY_AI_H
#define WOLFVK_ENEMY_AI_H

#include "wolfvk/actor.h"
#include "wolfvk/game_world.h"
#include "wolfvk/level_objects.h"
#include "wolfvk/player_state.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct WolfEnemyAIEvents {
    uint32_t guard_alerts;
    uint32_t guard_shots;
    float alert_source_x;
    float alert_source_z;
    float shot_source_x;
    float shot_source_z;
} WolfEnemyAIEvents;

/* Advance the shared perception and guard combat logic by one original 70 Hz tic. */
bool wolf_enemy_ai_update_tic(
    WolfActor *actors,
    uint32_t actor_count,
    WolfWorldState *world,
    float player_yaw_radians,
    WolfDifficulty difficulty,
    bool player_is_moving,
    bool player_made_noise,
    WolfEnemyAIEvents *events);

/* Resolve one player weapon event against the nearest visible guard in the view ray. */
bool wolf_enemy_ai_player_attack(
    WolfActor *actors,
    uint32_t actor_count,
    WolfWorldState *world,
    float player_yaw_radians,
    WolfWeaponType weapon);

#endif
