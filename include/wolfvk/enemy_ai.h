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
    uint32_t officer_alerts;
    uint32_t ss_alerts;
    uint32_t guard_shots;
    uint32_t officer_shots;
    uint32_t ss_shots;
    uint32_t mutant_shots;
    bool player_killed;
    float killer_position_x;
    float killer_position_z;
    uint32_t guard_deaths;
    uint32_t officer_deaths;
    uint32_t ss_deaths;
    uint32_t mutant_deaths;
    float alert_source_x;
    float alert_source_z;
    float shot_source_x;
    float shot_source_z;
    float officer_alert_source_x;
    float officer_alert_source_z;
    float ss_alert_source_x;
    float ss_alert_source_z;
    float officer_shot_source_x;
    float officer_shot_source_z;
    float ss_shot_source_x;
    float ss_shot_source_z;
    float mutant_shot_source_x;
    float mutant_shot_source_z;
    float guard_death_source_x;
    float guard_death_source_z;
    float officer_death_source_x;
    float officer_death_source_z;
    float ss_death_source_x;
    float ss_death_source_z;
    float mutant_death_source_x;
    float mutant_death_source_z;
} WolfEnemyAIEvents;

/* Advance all four supported humanoid families by one original 70 Hz tic. */
bool wolf_enemy_ai_update_tic(
    WolfActor *actors,
    uint32_t actor_count,
    WolfWorldState *world,
    float player_world_x,
    float player_world_z,
    float player_yaw_radians,
    WolfDifficulty difficulty,
    bool player_is_moving,
    bool player_made_noise,
    WolfEnemyAIEvents *events);

/* Resolve one player weapon event against the nearest visible supported humanoid in the view ray. */
bool wolf_enemy_ai_player_attack(
    WolfActor *actors,
    uint32_t actor_count,
    WolfWorldState *world,
    float player_world_x,
    float player_world_z,
    float player_yaw_radians,
    WolfWeaponType weapon,
    WolfEnemyAIEvents *events);

#endif
