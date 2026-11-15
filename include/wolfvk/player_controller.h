#ifndef WOLFVK_PLAYER_CONTROLLER_H
#define WOLFVK_PLAYER_CONTROLLER_H

#include "wolfvk/camera.h"
#include "wolfvk/input.h"
#include "wolfvk/level.h"

#include <stdbool.h>

/*
 * Apply first-person keyboard movement and relative mouse-look.
 * Ordinary door tiles (90–101) and info-plane secret-door markers (98) are
 * passable placeholders until door interaction is implemented; other wall
 * tiles and the map boundary remain solid.
 */
bool wolf_player_update(
    WolfCamera *camera,
    const WolfInput *input,
    const WolfLevel *level,
    float delta_seconds);

#endif
