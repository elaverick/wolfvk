#include "wolfvk/level_geometry.h"
#include "wolfvk/wall_textures.h"
#include "wolfvk/world.h"

#include <stdlib.h>

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

/* Store one textured vertex; colour remains a subtle face-lighting multiplier. */
static void set_vertex(
    VulkanVertex *vertex,
    const float position[3],
    const float texcoord[2],
    uint16_t texture_page,
    float shade)
{
    vertex->position[0] = position[0];
    vertex->position[1] = position[1];
    vertex->position[2] = position[2];
    vertex->colour[0] = shade;
    vertex->colour[1] = shade;
    vertex->colour[2] = shade;
    vertex->texcoord[0] = texcoord[0];
    vertex->texcoord[1] = texcoord[1];
    vertex->texture_layer = texture_page;
    vertex->use_texture = 1u;
}

/*
 * Add a face using its original VSWAP page and a consistent top-to-bottom UV
 * convention. The array capacity is precomputed from the maximum faces per cell.
 */
static bool append_wall_face(
    VulkanQuad *quads,
    size_t capacity,
    size_t *quad_count,
    const float positions[4][3],
    const float texcoords[4][2],
    uint16_t texture_page,
    float shade)
{
    if (*quad_count >= capacity) {
        return false;
    }

    for (size_t vertex_index = 0u; vertex_index < 4u; ++vertex_index) {
        set_vertex(
            &quads[*quad_count].vertices[vertex_index],
            positions[vertex_index],
            texcoords[vertex_index],
            texture_page,
            shade);
    }

    ++(*quad_count);
    return true;
}

/* Build square wall panels using the original horizontal/vertical VSWAP page pairing. */
bool wolf_level_build_wall_quads(
    const WolfLevel *level,
    uint16_t texture_page_count,
    VulkanQuad **quads,
    uint32_t *quad_count)
{
    static const float horizontal_uvs[4][2] = {
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}
    };
    static const float reversed_horizontal_uvs[4][2] = {
        {1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}
    };
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
            uint16_t horizontal_page;
            uint16_t vertical_page;
            float x0;
            float x1;
            float z0;
            float z1;

            if (tile == 0u || wolf_level_is_omitted_door_cell(level, x, y)) {
                continue;
            }

            /*
             * A map wall ID selects two directional panels in the original
             * renderer. Fail rather than silently substituting a plausible but
             * incorrect image if the archive does not contain the required page.
             */
            if (!wolf_wall_texture_page_for_tile(
                    tile, false, texture_page_count, &horizontal_page) ||
                !wolf_wall_texture_page_for_tile(
                    tile, true, texture_page_count, &vertical_page)) {
                free(generated_quads);
                return false;
            }

            x0 = -((float)level->width * WOLF_WORLD_TILE_SIZE) * 0.5f +
                (float)x * WOLF_WORLD_TILE_SIZE;
            x1 = x0 + WOLF_WORLD_TILE_SIZE;
            z0 = -((float)level->height * WOLF_WORLD_TILE_SIZE) * 0.5f +
                (float)y * WOLF_WORLD_TILE_SIZE;
            z1 = z0 + WOLF_WORLD_TILE_SIZE;

            /* Only emit faces bordering open space; shared wall faces can never be seen. */
            if (is_open_space_for_geometry(level, x, y - 1)) {
                const float face[4][3] = {
                    {x0, 0.0f, z0}, {x1, 0.0f, z0},
                    {x1, WOLF_WORLD_WALL_HEIGHT, z0}, {x0, WOLF_WORLD_WALL_HEIGHT, z0}
                };
                if (!append_wall_face(
                        generated_quads, capacity, &generated_count,
                        face, horizontal_uvs, horizontal_page, 0.82f)) {
                    free(generated_quads);
                    return false;
                }
            }

            if (is_open_space_for_geometry(level, x, y + 1)) {
                const float face[4][3] = {
                    {x1, 0.0f, z1}, {x0, 0.0f, z1},
                    {x0, WOLF_WORLD_WALL_HEIGHT, z1}, {x1, WOLF_WORLD_WALL_HEIGHT, z1}
                };
                if (!append_wall_face(
                        generated_quads, capacity, &generated_count,
                        face, reversed_horizontal_uvs, horizontal_page, 0.95f)) {
                    free(generated_quads);
                    return false;
                }
            }

            if (is_open_space_for_geometry(level, x - 1, y)) {
                const float face[4][3] = {
                    {x0, 0.0f, z1}, {x0, 0.0f, z0},
                    {x0, WOLF_WORLD_WALL_HEIGHT, z0}, {x0, WOLF_WORLD_WALL_HEIGHT, z1}
                };
                if (!append_wall_face(
                        generated_quads, capacity, &generated_count,
                        face, reversed_horizontal_uvs, vertical_page, 0.72f)) {
                    free(generated_quads);
                    return false;
                }
            }

            if (is_open_space_for_geometry(level, x + 1, y)) {
                const float face[4][3] = {
                    {x1, 0.0f, z0}, {x1, 0.0f, z1},
                    {x1, WOLF_WORLD_WALL_HEIGHT, z1}, {x1, WOLF_WORLD_WALL_HEIGHT, z0}
                };
                if (!append_wall_face(
                        generated_quads, capacity, &generated_count,
                        face, horizontal_uvs, vertical_page, 0.88f)) {
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
