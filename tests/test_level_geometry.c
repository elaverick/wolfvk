#include "unity.h"

#include "wolfvk/level_geometry.h"
#include "wolfvk/world.h"

#include <math.h>
#include <stdlib.h>

void test_level_geometry_emits_four_faces_for_isolated_wall(void)
{
    WolfLevel level = {0};
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;

    level.width = 3u;
    level.height = 3u;
    level.wall_tiles[WOLF_LEVEL_PLANE_WIDTH + 1u] = 7u;

    TEST_ASSERT_TRUE(wolf_level_build_wall_quads(&level, UINT16_MAX, &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(4u, quad_count);
    TEST_ASSERT_NOT_NULL(quads);

    TEST_ASSERT_EQUAL_FLOAT(-1.6f, quads[0].vertices[0].position[0]);
    TEST_ASSERT_EQUAL_FLOAT(-1.6f, quads[0].vertices[0].position[2]);
    TEST_ASSERT_EQUAL_FLOAT(3.2f, quads[0].vertices[2].position[1]);

    free(quads);
}

void test_level_geometry_wall_panels_are_square_in_world_space(void)
{
    WolfLevel level = {0};
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;
    float horizontal_extent;
    float vertical_extent;

    level.width = 3u;
    level.height = 3u;
    level.wall_tiles[WOLF_LEVEL_PLANE_WIDTH + 1u] = 1u;

    TEST_ASSERT_TRUE(wolf_level_build_wall_quads(&level, UINT16_MAX, &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(4u, quad_count);

    /* A single wall panel must occupy one square map cell in both world axes. */
    horizontal_extent = fabsf(
        quads[0].vertices[1].position[0] - quads[0].vertices[0].position[0]);
    vertical_extent = fabsf(
        quads[0].vertices[2].position[1] - quads[0].vertices[1].position[1]);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, WOLF_WORLD_TILE_SIZE, horizontal_extent);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, WOLF_WORLD_WALL_HEIGHT, vertical_extent);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, horizontal_extent, vertical_extent);

    free(quads);
}

void test_level_geometry_uses_original_vswap_orientation_pages(void)
{
    WolfLevel level = {0};
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;

    level.width = 3u;
    level.height = 3u;
    level.wall_tiles[WOLF_LEVEL_PLANE_WIDTH + 1u] = 7u;

    TEST_ASSERT_TRUE(wolf_level_build_wall_quads(&level, 14u, &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(4u, quad_count);

    /* Wolf3D pairs horizontal and vertical wall pages as even/odd VSWAP indices. */
    TEST_ASSERT_EQUAL_UINT32(12u, quads[0].vertices[0].texture_layer);
    TEST_ASSERT_EQUAL_UINT32(12u, quads[1].vertices[0].texture_layer);
    TEST_ASSERT_EQUAL_UINT32(13u, quads[2].vertices[0].texture_layer);
    TEST_ASSERT_EQUAL_UINT32(13u, quads[3].vertices[0].texture_layer);
    TEST_ASSERT_EQUAL_UINT32(1u, quads[0].vertices[0].use_texture);

    /*
     * The first two vertices of every face follow its ordered world-space
     * edge. The original HitHorizWall and HitVertWall routines both reverse
     * their source coordinate for one approach direction; the opposing face
     * vertex order accounts for the other direction. All four exposed faces
     * therefore carry this same local corner sequence, with U increasing in
     * the original engine's direction on each individual face.
     */
    for (size_t face = 0u; face < 4u; ++face) {
        TEST_ASSERT_FLOAT_WITHIN(
            0.001f, 1.0f, quads[face].vertices[0].texcoord[0]);
        TEST_ASSERT_FLOAT_WITHIN(
            0.001f, 0.0f, quads[face].vertices[1].texcoord[0]);
    }

    /* Confirm that each pair of faces is wound along the correct map axis. */
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, -1.6f, quads[0].vertices[0].position[0]);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, 1.6f, quads[0].vertices[1].position[0]);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, 1.6f, quads[1].vertices[0].position[0]);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, -1.6f, quads[1].vertices[1].position[0]);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, 1.6f, quads[2].vertices[0].position[2]);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, -1.6f, quads[2].vertices[1].position[2]);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, -1.6f, quads[3].vertices[0].position[2]);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, 1.6f, quads[3].vertices[1].position[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, quads[0].vertices[0].texcoord[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, quads[0].vertices[2].texcoord[1]);

    free(quads);
    quads = NULL;
    quad_count = 0u;

    /*
     * Level 1 uses tile 106 (0x6A): its 0x40 flag leaves wall type 42,
     * which selects the final pages needed by that retail map.
     */
    level.wall_tiles[WOLF_LEVEL_PLANE_WIDTH + 1u] = 106u;
    TEST_ASSERT_TRUE(wolf_level_build_wall_quads(&level, 98u, &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(4u, quad_count);
    TEST_ASSERT_EQUAL_UINT32(82u, quads[0].vertices[0].texture_layer);
    TEST_ASSERT_EQUAL_UINT32(82u, quads[1].vertices[0].texture_layer);
    TEST_ASSERT_EQUAL_UINT32(83u, quads[2].vertices[0].texture_layer);
    TEST_ASSERT_EQUAL_UINT32(83u, quads[3].vertices[0].texture_layer);

    free(quads);
}

void test_level_geometry_rejects_missing_vswap_texture_page(void)
{
    WolfLevel level = {0};
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 99u;

    level.width = 3u;
    level.height = 3u;
    level.wall_tiles[WOLF_LEVEL_PLANE_WIDTH + 1u] = 7u;

    TEST_ASSERT_FALSE(wolf_level_build_wall_quads(&level, 13u, &quads, &quad_count));
    TEST_ASSERT_NULL(quads);
    TEST_ASSERT_EQUAL_UINT32(0u, quad_count);
}

void test_level_geometry_omits_shared_faces_between_adjacent_walls(void)
{
    WolfLevel level = {0};
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;

    level.width = 3u;
    level.height = 3u;
    level.wall_tiles[WOLF_LEVEL_PLANE_WIDTH + 1u] = 1u;
    level.wall_tiles[WOLF_LEVEL_PLANE_WIDTH + 2u] = 2u;

    TEST_ASSERT_TRUE(wolf_level_build_wall_quads(&level, UINT16_MAX, &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(6u, quad_count);

    free(quads);
}

void test_level_geometry_treats_door_cells_as_openings(void)
{
    WolfLevel level = {0};
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;

    level.width = 3u;
    level.height = 3u;

    /* The adjacent door must leave the neighbouring wall face exposed. */
    level.wall_tiles[WOLF_LEVEL_PLANE_WIDTH + 1u] = 7u;
    level.wall_tiles[WOLF_LEVEL_PLANE_WIDTH + 2u] = WOLF_LEVEL_DOOR_TILE_FIRST;

    TEST_ASSERT_TRUE(wolf_level_build_wall_quads(&level, UINT16_MAX, &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(4u, quad_count);
    TEST_ASSERT_NOT_NULL(quads);

    free(quads);
}

void test_level_door_tile_classifier_matches_retail_range(void)
{
    for (uint16_t tile = WOLF_LEVEL_DOOR_TILE_FIRST;
         tile <= WOLF_LEVEL_DOOR_TILE_LAST;
         ++tile) {
        TEST_ASSERT_TRUE(wolf_level_is_door_tile(tile));
    }

    TEST_ASSERT_FALSE(wolf_level_is_door_tile(WOLF_LEVEL_DOOR_TILE_FIRST - 1u));
    TEST_ASSERT_FALSE(wolf_level_is_door_tile(WOLF_LEVEL_DOOR_TILE_LAST + 1u));
    TEST_ASSERT_FALSE(wolf_level_is_door_tile(WOLF_LEVEL_AREA_TILE_FIRST));
}

void test_level_geometry_keeps_stationary_secret_walls_in_static_geometry(void)
{
    WolfLevel level = {0};
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;
    size_t wall_index = WOLF_LEVEL_PLANE_WIDTH + 1u;

    level.width = 3u;
    level.height = 3u;
    level.wall_tiles[wall_index] = 7u;
    level.object_tiles[wall_index] = WOLF_LEVEL_INFO_PUSHWALL_MARKER;

    /* A secret wall remains an ordinary solid panel until the player pushes it. */
    TEST_ASSERT_TRUE(wolf_level_build_wall_quads(&level, UINT16_MAX, &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(4u, quad_count);
    TEST_ASSERT_NOT_NULL(quads);
}

void test_level_geometry_uses_original_door_side_pages_on_secret_wall_faces(void)
{
    WolfLevel level = {0};
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;
    const size_t wall_index = 2u * WOLF_LEVEL_PLANE_WIDTH + 2u;

    level.width = 5u;
    level.height = 5u;
    level.wall_tiles[wall_index] = 7u;
    level.object_tiles[wall_index] = WOLF_LEVEL_INFO_PUSHWALL_MARKER;

    /* A vertical door to the north and horizontal door to the west mark this wall's side faces. */
    level.wall_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 90u;
    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 1u] = 91u;

    TEST_ASSERT_TRUE(wolf_level_build_wall_quads(&level, 98u, &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(4u, quad_count);

    /* North/south faces: DOORWALL+2 beside a vertical door, ordinary page otherwise. */
    TEST_ASSERT_EQUAL_UINT32(92u, quads[0].vertices[0].texture_layer);
    TEST_ASSERT_EQUAL_UINT32(12u, quads[1].vertices[0].texture_layer);

    /* West/east faces: DOORWALL+3 beside a horizontal door, ordinary page otherwise. */
    TEST_ASSERT_EQUAL_UINT32(93u, quads[2].vertices[0].texture_layer);
    TEST_ASSERT_EQUAL_UINT32(13u, quads[3].vertices[0].texture_layer);

    free(quads);
}

void test_level_geometry_can_choose_a_door_as_fallback_spawn_cell(void)
{
    WolfLevel level = {0};
    uint16_t tile_x = UINT16_MAX;
    uint16_t tile_y = UINT16_MAX;

    level.width = 3u;
    level.height = 3u;
    for (uint16_t y = 0u; y < level.height; ++y) {
        for (uint16_t x = 0u; x < level.width; ++x) {
            level.wall_tiles[(size_t)y * WOLF_LEVEL_PLANE_WIDTH + x] = 1u;
        }
    }
    level.wall_tiles[WOLF_LEVEL_PLANE_WIDTH + 1u] = WOLF_LEVEL_DOOR_TILE_FIRST;

    TEST_ASSERT_TRUE(wolf_level_find_open_tile(&level, &tile_x, &tile_y));
    TEST_ASSERT_EQUAL_UINT16(1u, tile_x);
    TEST_ASSERT_EQUAL_UINT16(1u, tile_y);
}

void test_level_geometry_returns_empty_array_for_empty_map(void)
{
    WolfLevel level = {0};
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 99u;

    level.width = 3u;
    level.height = 3u;

    TEST_ASSERT_TRUE(wolf_level_build_wall_quads(&level, UINT16_MAX, &quads, &quad_count));
    TEST_ASSERT_NULL(quads);
    TEST_ASSERT_EQUAL_UINT32(0u, quad_count);
}

void test_level_geometry_finds_nearest_open_tile(void)
{
    WolfLevel level = {0};
    uint16_t tile_x = UINT16_MAX;
    uint16_t tile_y = UINT16_MAX;

    level.width = 3u;
    level.height = 3u;
    level.wall_tiles[WOLF_LEVEL_PLANE_WIDTH + 1u] = 1u;

    TEST_ASSERT_TRUE(wolf_level_find_open_tile(&level, &tile_x, &tile_y));
    TEST_ASSERT_EQUAL_UINT16(1u, tile_x);
    TEST_ASSERT_EQUAL_UINT16(0u, tile_y);
}

void test_level_geometry_rejects_map_without_open_tiles(void)
{
    WolfLevel level = {0};
    uint16_t tile_x = 0u;
    uint16_t tile_y = 0u;

    level.width = 2u;
    level.height = 2u;
    for (uint16_t y = 0u; y < level.height; ++y) {
        for (uint16_t x = 0u; x < level.width; ++x) {
            level.wall_tiles[(size_t)y * WOLF_LEVEL_PLANE_WIDTH + x] = 1u;
        }
    }

    TEST_ASSERT_FALSE(wolf_level_find_open_tile(&level, &tile_x, &tile_y));
}

void test_level_geometry_rejects_invalid_arguments(void)
{
    WolfLevel level = {0};
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;

    TEST_ASSERT_FALSE(wolf_level_build_wall_quads(NULL, UINT16_MAX, &quads, &quad_count));
    TEST_ASSERT_FALSE(wolf_level_build_wall_quads(&level, UINT16_MAX, NULL, &quad_count));
    TEST_ASSERT_FALSE(wolf_level_build_wall_quads(&level, UINT16_MAX, &quads, NULL));
    TEST_ASSERT_FALSE(wolf_level_find_open_tile(NULL, NULL, NULL));

    level.width = (uint16_t)(WOLF_LEVEL_PLANE_WIDTH + 1u);
    level.height = 1u;
    TEST_ASSERT_FALSE(wolf_level_build_wall_quads(&level, UINT16_MAX, &quads, &quad_count));
}

void test_level_geometry_reads_original_wolf3d_player_start_markers(void)
{
    static const float expected_yaws[] = {
        0.0f, 1.57079632679f, 3.14159265359f, 4.71238898038f
    };

    for (uint16_t marker = WOLF_LEVEL_PLAYER_START_FIRST;
         marker <= WOLF_LEVEL_PLAYER_START_LAST; ++marker) {
        WolfLevel level = {0};
        WolfPlayerStart player_start = {0};

        level.width = 4u;
        level.height = 3u;
        level.object_tiles[WOLF_LEVEL_PLANE_WIDTH + 2u] = marker;

        TEST_ASSERT_TRUE(wolf_level_find_player_start(&level, &player_start));
        TEST_ASSERT_EQUAL_UINT16(2u, player_start.tile_x);
        TEST_ASSERT_EQUAL_UINT16(1u, player_start.tile_y);
        TEST_ASSERT_FLOAT_WITHIN(
            0.0001f,
            expected_yaws[marker - WOLF_LEVEL_PLAYER_START_FIRST],
            player_start.yaw_radians);
    }
}

void test_level_geometry_rejects_player_start_inside_wall(void)
{
    WolfLevel level = {0};
    WolfPlayerStart player_start = {0};

    level.width = 3u;
    level.height = 3u;
    level.wall_tiles[WOLF_LEVEL_PLANE_WIDTH + 1u] = 1u;
    level.object_tiles[WOLF_LEVEL_PLANE_WIDTH + 1u] = 19u;

    TEST_ASSERT_FALSE(wolf_level_find_player_start(&level, &player_start));
}

void test_level_geometry_reports_missing_player_start_marker(void)
{
    WolfLevel level = {0};
    WolfPlayerStart player_start = {0};

    level.width = 2u;
    level.height = 2u;
    TEST_ASSERT_FALSE(wolf_level_find_player_start(&level, &player_start));
}
