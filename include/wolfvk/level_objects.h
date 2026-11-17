#ifndef WOLFVK_LEVEL_OBJECTS_H
#define WOLFVK_LEVEL_OBJECTS_H

#include "wolfvk/level.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * The original difficulty levels. ScanInfoPlane spawns the info plane's
 * second and third tiers of enemies only at medium and hard.
 */
typedef enum WolfDifficulty {
    WOLF_DIFFICULTY_BABY = 0,   /* "Can I play, Daddy?" */
    WOLF_DIFFICULTY_EASY = 1,   /* "Don't hurt me." */
    WOLF_DIFFICULTY_MEDIUM = 2, /* "Bring 'em on!", the original menu's default */
    WOLF_DIFFICULTY_HARD = 3    /* "I am Death incarnate!" */
} WolfDifficulty;

#define WOLF_LEVEL_STATIC_FIRST 23u
#define WOLF_LEVEL_STATIC_LAST 70u
/* WL6 statinfo[48] is a second ammunition clip that reuses SPR_STAT_26. */
#define WOLF_LEVEL_STATIC_EXTRA_CLIP 71u
#define WOLF_LEVEL_STATIC_EXTRA_CLIP_SPRITE 26u
#define WOLF_LEVEL_DEAD_GUARD 124u

#define WOLF_LEVEL_OBJECT_NAME_LENGTH 24u

/*
 * An actor or static object placed on the map, named by the voxel asset that
 * draws it: the subject is the VOX file ("grd" for grd.vox) and the animation
 * and pose select a model inside it ("grd_s", pose 0).
 */
typedef struct WolfLevelObject {
    char subject[WOLF_LEVEL_OBJECT_NAME_LENGTH];
    char animation[WOLF_LEVEL_OBJECT_NAME_LENGTH];
    uint8_t pose;
    uint16_t tile_x;
    uint16_t tile_y;
    /* The info-plane marker that placed this object, so collected items can be dropped. */
    uint16_t info_marker;
    /* Facing, using the camera's convention: 0 faces north, pi/2 faces east. */
    float yaw_radians;
} WolfLevelObject;

/*
 * Find every actor and static object the original ScanInfoPlane would spawn
 * at this difficulty, in map order. Callers own and must free *objects.
 *
 * Guards, officers, SS, dogs and mutants were drawn from eight directions
 * and keep the compass facing the map gives them. Standing actors use their
 * standing pose and patrolling ones the first stride of their walk, as the
 * original s_*stand and s_*path1 states do. Everything else (static objects,
 * bosses, ghosts, the dead guard) was a sprite that always faced the player;
 * a solid model needs a fixed facing, so it faces along its longest
 * unobstructed line of floor, which turns it away from nearby walls and
 * towards the room.
 */
bool wolf_level_find_objects(
    const WolfLevel *level,
    WolfDifficulty difficulty,
    WolfLevelObject **objects,
    uint32_t *object_count);

/*
 * Match WL6 WL_ACT1.C statinfo[]: true only for static markers whose type is
 * block. Pickups, floor corpses and non-blocking decoration return false.
 */
bool wolf_level_object_marker_blocks_player(uint16_t marker);

#endif
