#include "wolfvk/level_geometry.h"

#include <stdlib.h>

#define WOLF_WALL_HEIGHT 3.2f

typedef struct WallColour {
    float red;
    float green;
    float blue;
} WallColour;

static const WallColour wall_palette[] = {
    {0.56f, 0.36f, 0.25f},
    {0.43f, 0.47f, 0.52f},
    {0.37f, 0.48f, 0.38f},
    {0.62f, 0.49f, 0.29f},
    {0.42f, 0.39f, 0.57f},
    {0.58f, 0.32f, 0.34f},
    {0.48f, 0.48f, 0.43f},
    {0.35f, 0.43f, 0.52f}
};

/* Out-of-map neighbours are empty so perimeter walls expose their outside faces. */
static uint16_t get_wall_tile(const WolfLevel *level, int x, int y)
{
    if (x < 0 || y < 0 || x >= level->width || y >= level->height) {
        return 0u;
    }

    return level->wall_tiles[(size_t)y * WOLF_LEVEL_PLANE_WIDTH + (size_t)x];
}

/*
 * Doors are not part of this milestone. Treat their cells as openings in the
 * static wall mesh rather than drawing a closed panel that the player cannot use.
 */
static bool is_open_space_for_geometry(const WolfLevel *level, int x, int y)
{
    return get_wall_tile(level, x, y) == 0u ||
        wolf_level_is_omitted_door_cell(level, x, y);
}

/* Convert one world-space corner into the renderer's interleaved vertex format. */
static void set_vertex(
    VulkanVertex *vertex,
    float x,
    float y,
    float z,
    const WallColour *colour,
    float shade)
{
    vertex->position[0] = x;
    vertex->position[1] = y;
    vertex->position[2] = z;
    vertex->colour[0] = colour->red * shade;
    vertex->colour[1] = colour->green * shade;
    vertex->colour[2] = colour->blue * shade;
}

/* Append a consistently shaded wall quad without writing beyond the precomputed capacity. */
static bool append_wall_face(
    VulkanQuad *quads,
    size_t capacity,
    size_t *quad_count,
    const float positions[4][3],
    const WallColour *colour,
    float shade)
{
    if (*quad_count >= capacity) {
        return false;
    }

    for (size_t vertex_index = 0u; vertex_index < 4u; ++vertex_index) {
        set_vertex(
            &quads[*quad_count].vertices[vertex_index],
            positions[vertex_index][0],
            positions[vertex_index][1],
            positions[vertex_index][2],
            colour,
            shade);
    }

    ++(*quad_count);
    return true;
}

/* Build only visible wall boundaries; open cells and unimplemented doors remain traversable. */
bool wolf_level_build_wall_quads(
    const WolfLevel *level,
    VulkanQuad **quads,
    uint32_t *quad_count)
{
    size_t capacity;
    size_t generated_count = 0u;
    VulkanQuad *generated_quads;

    if (level == NULL || quads == NULL || quad_count == NULL ||
        level->width == 0u || level->height == 0u ||
        level->width > WOLF_LEVEL_PLANE_WIDTH ||
        level->height > WOLF_LEVEL_PLANE_HEIGHT) {
        return false;
    }

    *quads = NULL;
    *quad_count = 0u;
    capacity = (size_t)level->width * (size_t)level->height * 4u;
    if (capacity == 0u || capacity > UINT32_MAX ||
        capacity > SIZE_MAX / sizeof(VulkanQuad)) {
        return false;
    }

    generated_quads = malloc(capacity * sizeof(VulkanQuad));
    if (generated_quads == NULL) {
        return false;
    }

    for (int y = 0; y < level->height; ++y) {
        for (int x = 0; x < level->width; ++x) {
            uint16_t tile = get_wall_tile(level, x, y);
            float x0;
            float x1;
            float z0;
            float z1;
            const WallColour *colour;

            if (tile == 0u || wolf_level_is_omitted_door_cell(level, x, y)) {
                continue;
            }

            x0 = -(float)level->width * 0.5f + (float)x;
            x1 = x0 + 1.0f;
            z0 = -(float)level->height * 0.5f + (float)y;
            z1 = z0 + 1.0f;
            colour = &wall_palette[tile % (sizeof(wall_palette) / sizeof(wall_palette[0]))];

            /* Only emit faces bordering empty space; shared wall faces can never be seen. */
            if (is_open_space_for_geometry(level, x, y - 1)) {
                const float face[4][3] = {
                    {x0, 0.0f, z0}, {x1, 0.0f, z0},
                    {x1, WOLF_WALL_HEIGHT, z0}, {x0, WOLF_WALL_HEIGHT, z0}
                };
                if (!append_wall_face(
                        generated_quads, capacity, &generated_count, face, colour, 0.82f)) {
                    free(generated_quads);
                    return false;
                }
            }

            if (is_open_space_for_geometry(level, x, y + 1)) {
                const float face[4][3] = {
                    {x1, 0.0f, z1}, {x0, 0.0f, z1},
                    {x0, WOLF_WALL_HEIGHT, z1}, {x1, WOLF_WALL_HEIGHT, z1}
                };
                if (!append_wall_face(
                        generated_quads, capacity, &generated_count, face, colour, 0.95f)) {
                    free(generated_quads);
                    return false;
                }
            }

            if (is_open_space_for_geometry(level, x - 1, y)) {
                const float face[4][3] = {
                    {x0, 0.0f, z1}, {x0, 0.0f, z0},
                    {x0, WOLF_WALL_HEIGHT, z0}, {x0, WOLF_WALL_HEIGHT, z1}
                };
                if (!append_wall_face(
                        generated_quads, capacity, &generated_count, face, colour, 0.72f)) {
                    free(generated_quads);
                    return false;
                }
            }

            if (is_open_space_for_geometry(level, x + 1, y)) {
                const float face[4][3] = {
                    {x1, 0.0f, z0}, {x1, 0.0f, z1},
                    {x1, WOLF_WALL_HEIGHT, z1}, {x1, WOLF_WALL_HEIGHT, z0}
                };
                if (!append_wall_face(
                        generated_quads, capacity, &generated_count, face, colour, 0.88f)) {
                    free(generated_quads);
                    return false;
                }
            }
        }
    }

    if (generated_count == 0u) {
        free(generated_quads);
        generated_quads = NULL;
    }

    *quads = generated_quads;
    *quad_count = (uint32_t)generated_count;
    return true;
}

/* Preserve the start tile and compass direction encoded by the original info plane. */
bool wolf_level_find_player_start(const WolfLevel *level, WolfPlayerStart *player_start)
{
    /* The original engine maps info-plane markers 19..22 to four compass directions. */
    static const float start_yaws[4] = {
        0.0f,
        1.57079632679f,
        3.14159265359f,
        4.71238898038f
    };

    if (level == NULL || player_start == NULL ||
        level->width == 0u || level->height == 0u ||
        level->width > WOLF_LEVEL_PLANE_WIDTH ||
        level->height > WOLF_LEVEL_PLANE_HEIGHT) {
        return false;
    }

    for (uint16_t y = 0u; y < level->height; ++y) {
        for (uint16_t x = 0u; x < level->width; ++x) {
            size_t tile_index = (size_t)y * WOLF_LEVEL_PLANE_WIDTH + x;
            uint16_t marker = level->object_tiles[tile_index];

            if (marker >= WOLF_LEVEL_PLAYER_START_FIRST &&
                marker <= WOLF_LEVEL_PLAYER_START_LAST) {
                if (level->wall_tiles[tile_index] != 0u) {
                    return false;
                }

                player_start->tile_x = x;
                player_start->tile_y = y;
                player_start->yaw_radians =
                    start_yaws[marker - WOLF_LEVEL_PLAYER_START_FIRST];
                return true;
            }
        }
    }

    return false;
}

/* Choose a stable fallback spawn near the map centre when no original start marker exists. */
bool wolf_level_find_open_tile(
    const WolfLevel *level,
    uint16_t *tile_x,
    uint16_t *tile_y)
{
    float centre_x;
    float centre_y;
    float nearest_distance = 0.0f;
    bool found_tile = false;

    if (level == NULL || tile_x == NULL || tile_y == NULL ||
        level->width == 0u || level->height == 0u ||
        level->width > WOLF_LEVEL_PLANE_WIDTH ||
        level->height > WOLF_LEVEL_PLANE_HEIGHT) {
        return false;
    }

    centre_x = ((float)level->width - 1.0f) * 0.5f;
    centre_y = ((float)level->height - 1.0f) * 0.5f;

    for (uint16_t y = 0u; y < level->height; ++y) {
        for (uint16_t x = 0u; x < level->width; ++x) {
            float delta_x;
            float delta_y;
            float distance;

            if (get_wall_tile(level, x, y) != 0u &&
                !wolf_level_is_omitted_door_cell(level, x, y)) {
                continue;
            }

            delta_x = (float)x - centre_x;
            delta_y = (float)y - centre_y;
            distance = delta_x * delta_x + delta_y * delta_y;

            if (!found_tile || distance < nearest_distance) {
                nearest_distance = distance;
                *tile_x = x;
                *tile_y = y;
                found_tile = true;
            }
        }
    }

    return found_tile;
}
