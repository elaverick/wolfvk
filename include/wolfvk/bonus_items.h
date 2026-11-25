#ifndef WOLFVK_BONUS_ITEMS_H
#define WOLFVK_BONUS_ITEMS_H

#include "wolfvk/player_state.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * The static objects the player can pick up, from the original `statinfo`
 * table in WL_ACT1.C (WL6 build). Each info-plane static marker 23-71 is one
 * entry of that table; the ones without a bonus type are scenery.
 */
typedef enum WolfBonusType {
    WOLF_BONUS_NONE = 0,
    WOLF_BONUS_GOLD_KEY,
    WOLF_BONUS_SILVER_KEY,
    WOLF_BONUS_DOG_FOOD,
    WOLF_BONUS_FOOD,
    WOLF_BONUS_FIRST_AID,
    WOLF_BONUS_GIBS,
    WOLF_BONUS_CLIP,
    WOLF_BONUS_USED_CLIP,
    WOLF_BONUS_MACHINEGUN,
    WOLF_BONUS_CHAINGUN,
    WOLF_BONUS_CROSS,
    WOLF_BONUS_CHALICE,
    WOLF_BONUS_BIBLE,
    WOLF_BONUS_CROWN,
    WOLF_BONUS_EXTRA_LIFE
} WolfBonusType;

/*
 * The original grabs a bonus when its tile centre is less than one tile in
 * front of the view point and less than half a tile to either side
 * (TransformTile). The view point sits FOCALLENGTH (0x5700/0x10000 tiles)
 * behind the player, and anything nearer than MINDIST (0x5800/0x10000
 * tiles) to it is ignored.
 */
#define WOLF_BONUS_FOCAL_LENGTH_TILES (22272.0f / 65536.0f)
#define WOLF_BONUS_MIN_DISTANCE_TILES (22528.0f / 65536.0f)
#define WOLF_BONUS_GRAB_DEPTH_TILES 1.0f
#define WOLF_BONUS_GRAB_HALF_WIDTH_TILES 0.5f

/* The bonus an info-plane marker places, or WOLF_BONUS_NONE for scenery and non-static markers. */
WolfBonusType wolf_bonus_type_for_marker(uint16_t marker);

/*
 * True if a bonus centred on (tile_x, tile_y) is in the original grab range
 * of a player at map position (player_x, player_y) looking along yaw (0 is
 * north, increasing clockwise, matching the camera).
 */
bool wolf_bonus_in_grab_range(
    float player_x,
    float player_y,
    float player_yaw_radians,
    int tile_x,
    int tile_y);

/*
 * Apply a bonus to the player as GetBonus does. Health, ammunition and
 * scraps are left on the floor when they would do nothing (full health,
 * 99 bullets, more than 10 health for gibs); returns false in that case.
 */
bool wolf_bonus_apply(WolfPlayerState *player, WolfBonusType bonus);

#endif
