#ifndef WOLFVK_LEVEL_H
#define WOLFVK_LEVEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WOLF_LEVEL_MAX_MAPS 100u
#define WOLF_LEVEL_PLANE_WIDTH 64u
#define WOLF_LEVEL_PLANE_HEIGHT 64u
#define WOLF_LEVEL_PLANE_WORDS (WOLF_LEVEL_PLANE_WIDTH * WOLF_LEVEL_PLANE_HEIGHT)
#define WOLF_LEVEL_AREA_TILE_FIRST 107u
#define WOLF_LEVEL_PLAYER_START_FIRST 19u
#define WOLF_LEVEL_PLAYER_START_LAST 22u

typedef struct WolfLevel {
    uint16_t width;
    uint16_t height;
    char name[17];
    uint16_t wall_tiles[WOLF_LEVEL_PLANE_WORDS];
    /* Original plane 1 is the info plane (player, actors, and statics). */
    uint16_t object_tiles[WOLF_LEVEL_PLANE_WORDS];
} WolfLevel;

typedef struct WolfPlayerStart {
    uint16_t tile_x;
    uint16_t tile_y;
    float yaw_radians;
} WolfPlayerStart;

/* Decode the length-prefixed Carmack stream used by original Wolf3D map planes. */
bool wolf_level_decode_carmack(
    const uint8_t *source,
    size_t source_size,
    uint8_t *destination,
    size_t destination_size);

/* Expand a length-prefixed RLEW stream into a fixed number of 16-bit tile values. */
bool wolf_level_decode_rlew(
    const uint8_t *source,
    size_t source_size,
    uint16_t rlew_tag,
    uint16_t *destination,
    size_t destination_word_count);

/* Load both map planes from the original MAPHEAD/GAMEMAPS file pair. Map indices are zero-based. */
bool wolf_level_load(
    const char *maphead_path,
    const char *gamemaps_path,
    uint32_t map_index,
    WolfLevel *level,
    char *error_message,
    size_t error_message_size);

/* Read the original info-plane player start marker (tiles 19 through 22). */
bool wolf_level_find_player_start(const WolfLevel *level, WolfPlayerStart *player_start);

/* Find a deterministic open-cell fallback for maps without an original start marker. */
bool wolf_level_find_open_tile(
    const WolfLevel *level,
    uint16_t *tile_x,
    uint16_t *tile_y);

#endif
