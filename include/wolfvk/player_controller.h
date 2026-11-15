#ifndef WOLFVK_PLAYER_CONTROLLER_H
#define WOLFVK_PLAYER_CONTROLLER_H

#include "wolfvk/camera.h"
#include "wolfvk/input.h"
#include "wolfvk/level.h"

#include <stdbool.h>

/*
 * Apply first-person keyboard movement and relative mouse-look.
 * World-space collision treats non-zero wall tiles and the map boundary as solid.
 */
bool wolf_player_update(
    WolfCamera *camera,
    const WolfInput *input,
    const WolfLevel *level,
    float delta_seconds);

#endif
