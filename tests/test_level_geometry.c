#include "unity.h"

#include "wolfvk/level_geometry.h"

#include <stdlib.h>

void test_level_geometry_emits_four_faces_for_isolated_wall(void)
{
    WolfLevel level = {0};
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;

    level.width = 3u;
    level.height = 3u;
    level.wall_tiles[WOLF_LEVEL_PLANE_WIDTH + 1u] = 7u;

    TEST_ASSERT_TRUE(wolf_level_build_wall_quads(&level, &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(4u, quad_count);
    TEST_ASSERT_NOT_NULL(quads);

    TEST_ASSERT_EQUAL_FLOAT(-0.5f, quads[0].vertices[0].position[0]);
    TEST_ASSERT_EQUAL_FLOAT(-0.5f, quads[0].vertices[0].position[2]);
    TEST_ASSERT_EQUAL_FLOAT(3.2f, quads[0].vertices[2].position[1]);

    free(quads);
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

    TEST_ASSERT_TRUE(wolf_level_build_wall_quads(&level, &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(6u, quad_count);

    free(quads);
}

void test_level_geometry_returns_empty_array_for_empty_map(void)
{
    WolfLevel level = {0};
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 99u;

    level.width = 3u;
    level.height = 3u;

    TEST_ASSERT_TRUE(wolf_level_build_wall_quads(&level, &quads, &quad_count));
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

    TEST_ASSERT_FALSE(wolf_level_build_wall_quads(NULL, &quads, &quad_count));
    TEST_ASSERT_FALSE(wolf_level_build_wall_quads(&level, NULL, &quad_count));
    TEST_ASSERT_FALSE(wolf_level_build_wall_quads(&level, &quads, NULL));
    TEST_ASSERT_FALSE(wolf_level_find_open_tile(NULL, NULL, NULL));

    level.width = (uint16_t)(WOLF_LEVEL_PLANE_WIDTH + 1u);
    level.height = 1u;
    TEST_ASSERT_FALSE(wolf_level_build_wall_quads(&level, &quads, &quad_count));
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
