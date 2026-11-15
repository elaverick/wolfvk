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

/* Find the empty map cell nearest the level centre for the initial camera position. */
bool wolf_level_find_open_tile(
    const WolfLevel *level,
    uint16_t *tile_x,
    uint16_t *tile_y);

#endif
