#ifndef WOLFVK_ACTOR_MOVEMENT_H
#define WOLFVK_ACTOR_MOVEMENT_H

#include "wolfvk/actor.h"
#include "wolfvk/game_world.h"

#include <stdbool.h>
#include <stdint.h>

/* Original patrol movement is 512/65536 of a map tile per game tic. */
#define WOLF_ACTOR_PATROL_SPEED_WORLD_PER_TIC (WOLF_WORLD_TILE_SIZE * (512.0f / 65536.0f))
#define WOLF_ACTOR_COLLISION_RADIUS_TILES 0.20f

/* Enable grid movement in one of the original eight compass directions. */
bool wolf_actor_movement_set_direction(
    WolfActor *actor, WolfActorDirection direction);

/* Advance every enabled actor by exactly one 70 Hz tic. */
bool wolf_actor_movement_update_tic(
    WolfActor *actors,
    uint32_t actor_count,
    WolfWorldState *world,
    float player_world_x,
    float player_world_z);

#endif
