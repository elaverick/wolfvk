#ifndef WOLFVK_ACTOR_ANIMATION_H
#define WOLFVK_ACTOR_ANIMATION_H

#include "wolfvk/actor.h"
#include "wolfvk/level_objects.h"

#include <stdbool.h>
#include <stdint.h>

/* Build the runtime actor list from the same difficulty-filtered WL6 placements as the voxel scene. */
bool wolf_actor_collection_create(
    const WolfLevel *level,
    WolfDifficulty difficulty,
    WolfActor **actors,
    uint32_t *actor_count);

/* Advance every active actor by one original 70 Hz game tic. */
bool wolf_actor_collection_update_tic(WolfActor *actors, uint32_t actor_count);

/* Release a collection returned by wolf_actor_collection_create. */
void wolf_actor_collection_destroy(WolfActor *actors);

#endif
