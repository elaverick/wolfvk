#ifndef WOLFVK_PLAYER_CONTROLLER_H
#define WOLFVK_PLAYER_CONTROLLER_H

#include "wolfvk/camera.h"
#include "wolfvk/game_world.h"
#include "wolfvk/input.h"

#include <stdbool.h>

/* Apply first-person keyboard movement and mouse-look using the mutable runtime world. */
bool wolf_player_update(
    WolfCamera *camera,
    const WolfInput *input,
    const WolfWorldState *world,
    float delta_seconds);

#endif
