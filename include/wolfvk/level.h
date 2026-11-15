#ifndef WOLFVK_LEVEL_H
#define WOLFVK_LEVEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WOLF_LEVEL_MAX_MAPS 60u
#define WOLF_LEVEL_MAPHEAD_OFFSET_COUNT 100u
#define WOLF_LEVEL_PLANE_WIDTH 64u
#define WOLF_LEVEL_PLANE_HEIGHT 64u
#define WOLF_LEVEL_PLANE_WORDS (WOLF_LEVEL_PLANE_WIDTH * WOLF_LEVEL_PLANE_HEIGHT)
#define WOLF_LEVEL_DOOR_TILE_FIRST 90u
#define WOLF_LEVEL_DOOR_TILE_LAST 101u
#define WOLF_LEVEL_INFO_PUSHWALL_MARKER 98u
#define WOLF_LEVEL_AREA_TILE_FIRST 107u

typedef struct WolfLevel {
    uint16_t width;
    uint16_t height;
    char name[17];
    uint16_t wall_tiles[WOLF_LEVEL_PLANE_WORDS];
    /* Original plane 1 is the info plane (player starts, actors, statics and pushwall markers). */
    uint16_t object_tiles[WOLF_LEVEL_PLANE_WORDS];
} WolfLevel;

/* Door cells are omitted from the pre-door milestone so they do not become invisible barriers. */
bool wolf_level_is_door_tile(uint16_t wall_tile);

/* Identify secret pushwall cells from the info plane rather than their ordinary wall texture. */
bool wolf_level_is_secret_door_cell(
    const WolfLevel *level,
    int tile_x,
    int tile_y);

/* Return whether the pre-door milestone should treat this map cell as an opening. */
bool wolf_level_is_omitted_door_cell(
    const WolfLevel *level,
    int tile_x,
    int tile_y);

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

#endif
