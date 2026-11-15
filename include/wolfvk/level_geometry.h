#ifndef WOLFVK_LEVEL_GEOMETRY_H
#define WOLFVK_LEVEL_GEOMETRY_H

#include "wolfvk/level.h"
#include "wolfvk/vulkan_renderer.h"

#include <stdbool.h>
#include <stdint.h>

/* Build exposed wall faces only; callers own and must free the returned quad array. */
bool wolf_level_build_wall_quads(
    const WolfLevel *level,
    VulkanQuad **quads,
    uint32_t *quad_count);

#define WOLF_LEVEL_PLAYER_START_FIRST 19u
#define WOLF_LEVEL_PLAYER_START_LAST 22u

typedef struct WolfPlayerStart {
    uint16_t tile_x;
    uint16_t tile_y;
    float yaw_radians;
} WolfPlayerStart;

/* Read the original info-plane player start marker (tiles 19 through 22). */
bool wolf_level_find_player_start(const WolfLevel *level, WolfPlayerStart *player_start);

/* Find a deterministic open-cell fallback for maps without an original start marker. */
bool wolf_level_find_open_tile(
    const WolfLevel *level,
    uint16_t *tile_x,
    uint16_t *tile_y);

#endif
