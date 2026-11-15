#include "unity.h"

#include "wolfvk/game_world.h"
#include "wolfvk/world.h"

#include <math.h>
#include <stdlib.h>

static WolfLevel create_test_level(void)
{
    WolfLevel level = {0};

    level.width = 5u;
    level.height = 5u;
    return level;
}

static float tile_center_world(int tile, uint16_t map_extent)
{
    return ((float)tile + 0.5f - (float)map_extent * 0.5f) *
        WOLF_WORLD_TILE_SIZE;
}

static void advance_world_ticks(
    WolfWorldState *world,
    float ticks,
    float player_world_x,
    float player_world_z)
{
    while (ticks > 7.0001f) {
        TEST_ASSERT_TRUE(wolf_world_state_update(
            world, 0.1f, player_world_x, player_world_z, NULL));
        ticks -= 7.0f;
    }

    if (ticks > 0.0f) {
        TEST_ASSERT_TRUE(wolf_world_state_update(
            world, ticks / WOLF_WORLD_DOOR_TICKS_PER_SECOND,
            player_world_x, player_world_z, NULL));
    }
}

static bool initialise_door_world(
    WolfWorldState *world,
    uint16_t door_tile,
    uint16_t texture_page_count)
{
    WolfLevel level = create_test_level();

    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] = door_tile;
    return wolf_world_state_init(world, &level, texture_page_count);
}

void test_world_state_spawns_original_door_orientations_and_locks(void)
{
    WolfLevel level = create_test_level();
    WolfWorldState world = {0};

    level.wall_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 1u] = 90u;
    level.wall_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 91u;
    level.wall_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 92u;
    level.wall_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 4u] = 101u;

    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 98u));
    TEST_ASSERT_EQUAL_UINT16(4u, world.door_count);

    TEST_ASSERT_TRUE(world.doors[0].vertical);
    TEST_ASSERT_EQUAL_UINT8(0u, world.doors[0].lock_type);
    TEST_ASSERT_FALSE(world.doors[1].vertical);
    TEST_ASSERT_EQUAL_UINT8(0u, world.doors[1].lock_type);
    TEST_ASSERT_TRUE(world.doors[2].vertical);
    TEST_ASSERT_EQUAL_UINT8(1u, world.doors[2].lock_type);
    TEST_ASSERT_FALSE(world.doors[3].vertical);
    TEST_ASSERT_EQUAL_UINT8(5u, world.doors[3].lock_type);

    for (uint16_t index = 0u; index < world.door_count; ++index) {
        TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_CLOSED, world.doors[index].action);
        TEST_ASSERT_EQUAL_UINT32(0u, world.doors[index].position);
    }
}

void test_world_state_rejects_more_than_sixty_four_doors(void)
{
    WolfLevel level = create_test_level();
    WolfWorldState world = {0};

    level.width = WOLF_LEVEL_PLANE_WIDTH;
    level.height = 2u;
    for (uint16_t index = 0u; index < WOLF_WORLD_MAX_DOORS + 1u; ++index) {
        level.wall_tiles[index] = 90u;
    }

    TEST_ASSERT_FALSE(wolf_world_state_init(&world, &level, 98u));
}

void test_world_state_door_opens_at_original_tics_times_1024_speed(void)
{
    WolfWorldState world = {0};
    bool geometry_changed = false;

    TEST_ASSERT_TRUE(initialise_door_world(&world, 90u, 98u));
    TEST_ASSERT_TRUE(wolf_world_state_use(
        &world, tile_center_world(1, 5u), 0.0f, 1.57079632679f));
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_OPENING, world.doors[0].action);

    advance_world_ticks(&world, 63.0f, -3.2f, 0.0f);
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_OPENING, world.doors[0].action);
    TEST_ASSERT_EQUAL_UINT32(64512u, world.doors[0].position);

    TEST_ASSERT_TRUE(wolf_world_state_update(
        &world, 7.0f / WOLF_WORLD_DOOR_TICKS_PER_SECOND,
        -3.2f, 0.0f, &geometry_changed));
    TEST_ASSERT_TRUE(geometry_changed);
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_OPEN, world.doors[0].action);
    TEST_ASSERT_EQUAL_UINT32(WOLF_WORLD_DOOR_OPEN_POSITION, world.doors[0].position);
    TEST_ASSERT_FALSE(wolf_world_state_has_animated_geometry(&world));
}

void test_world_state_door_auto_closes_after_original_three_hundred_tics(void)
{
    WolfWorldState world = {0};
    float player_x = tile_center_world(0, 5u);
    float player_z = tile_center_world(0, 5u);

    TEST_ASSERT_TRUE(initialise_door_world(&world, 90u, 98u));
    world.doors[0].action = WOLF_DOOR_OPEN;
    world.doors[0].position = WOLF_WORLD_DOOR_OPEN_POSITION;

    advance_world_ticks(&world, 294.0f, player_x, player_z);
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_OPEN, world.doors[0].action);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 294.0f, world.doors[0].open_ticks);

    advance_world_ticks(&world, 6.0f, player_x, player_z);
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_CLOSING, world.doors[0].action);
    TEST_ASSERT_EQUAL_UINT32(WOLF_WORLD_DOOR_OPEN_POSITION, world.doors[0].position);

    advance_world_ticks(&world, 1.0f, player_x, player_z);
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_CLOSING, world.doors[0].action);
    TEST_ASSERT_EQUAL_UINT32(64511u, world.doors[0].position);
}

void test_world_state_keeps_door_open_while_player_occupies_doorway(void)
{
    WolfWorldState world = {0};
    float door_x = tile_center_world(2, 5u);
    float door_z = tile_center_world(2, 5u);

    TEST_ASSERT_TRUE(initialise_door_world(&world, 90u, 98u));
    world.doors[0].action = WOLF_DOOR_OPEN;
    world.doors[0].position = WOLF_WORLD_DOOR_OPEN_POSITION;

    advance_world_ticks(&world, 300.0f, door_x, door_z);
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_OPEN, world.doors[0].action);
    TEST_ASSERT_FALSE(wolf_world_state_position_overlaps_solid(
        &world, door_x, door_z, 0.20f));
}

void test_world_state_manual_door_use_reverses_opening_direction(void)
{
    WolfWorldState world = {0};
    float player_x = tile_center_world(1, 5u);
    float player_z = tile_center_world(2, 5u);

    TEST_ASSERT_TRUE(initialise_door_world(&world, 90u, 98u));
    TEST_ASSERT_TRUE(wolf_world_state_use(
        &world, player_x, player_z, 1.57079632679f));
    advance_world_ticks(&world, 64.0f, player_x, player_z);
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_OPEN, world.doors[0].action);

    TEST_ASSERT_TRUE(wolf_world_state_use(
        &world, player_x, player_z, 1.57079632679f));
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_CLOSING, world.doors[0].action);

    advance_world_ticks(&world, 64.0f, player_x, player_z);
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_CLOSED, world.doors[0].action);
    TEST_ASSERT_EQUAL_UINT32(0u, world.doors[0].position);
}

void test_world_state_locked_door_requires_and_collects_original_key_marker(void)
{
    WolfLevel level = create_test_level();
    WolfWorldState world = {0};
    float player_x = tile_center_world(1, 5u);
    float player_z = tile_center_world(2, 5u);

    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 92u;
    level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 1u] = 43u;

    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 98u));
    TEST_ASSERT_FALSE(wolf_world_state_use(
        &world, player_x, player_z, 1.57079632679f));
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_CLOSED, world.doors[0].action);

    TEST_ASSERT_TRUE(wolf_world_state_collect_key(&world, player_x, player_z));
    TEST_ASSERT_EQUAL_UINT8(1u, world.key_mask);
    TEST_ASSERT_EQUAL_UINT16(
        0u, world.level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 1u]);

    TEST_ASSERT_TRUE(wolf_world_state_use(
        &world, player_x, player_z, 1.57079632679f));
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_OPENING, world.doors[0].action);
}

void test_world_state_door_geometry_uses_original_vswap_door_pages(void)
{
    WolfLevel level = create_test_level();
    WolfWorldState world = {0};
    VulkanQuad quads[8];

    level.wall_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 1u] = 90u;
    level.wall_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 91u;
    level.wall_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 92u;
    level.wall_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 4u] = 101u;

    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 98u));
    TEST_ASSERT_EQUAL_UINT32(8u, wolf_world_state_dynamic_quad_count(&world));
    TEST_ASSERT_TRUE(wolf_world_state_build_dynamic_quads(&world, quads, 8u));

    /* Standard, horizontal and locked door tiles select their original VSWAP pages. */
    TEST_ASSERT_EQUAL_UINT32(91u, quads[0].vertices[0].texture_layer);
    TEST_ASSERT_EQUAL_UINT32(90u, quads[2].vertices[0].texture_layer);
    TEST_ASSERT_EQUAL_UINT32(97u, quads[4].vertices[0].texture_layer);
    TEST_ASSERT_EQUAL_UINT32(94u, quads[6].vertices[0].texture_layer);
    TEST_ASSERT_EQUAL_UINT32(1u, quads[0].vertices[0].use_texture);

    world.doors[0].position = 32768u;
    TEST_ASSERT_TRUE(wolf_world_state_build_dynamic_quads(&world, quads, 8u));

    /* A vertical door keeps its X plane fixed and slides halfway along Z. */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.2f, quads[0].vertices[0].position[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.2f, quads[0].vertices[0].position[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, quads[0].vertices[1].position[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.2f, quads[1].vertices[0].position[0]);

    /* A horizontal door slides along X while its Z plane remains fixed. */
    world.doors[1].position = 32768u;
    TEST_ASSERT_TRUE(wolf_world_state_build_dynamic_quads(&world, quads, 8u));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.2f, quads[2].vertices[0].position[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.2f, quads[2].vertices[0].position[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, quads[2].vertices[1].position[0]);

    /* Fully-open panels are removed from the dynamic draw list on both orientations. */
    world.doors[0].position = WOLF_WORLD_DOOR_OPEN_POSITION;
    world.doors[0].action = WOLF_DOOR_OPEN;
    world.doors[1].position = WOLF_WORLD_DOOR_OPEN_POSITION;
    world.doors[1].action = WOLF_DOOR_OPEN;
    TEST_ASSERT_TRUE(wolf_world_state_build_dynamic_quads(&world, quads, 8u));
    TEST_ASSERT_EQUAL_UINT8(0u, quads[0].vertices[0].use_texture);
    TEST_ASSERT_EQUAL_UINT8(0u, quads[1].vertices[0].use_texture);
    TEST_ASSERT_EQUAL_UINT8(0u, quads[2].vertices[0].use_texture);
    TEST_ASSERT_EQUAL_UINT8(0u, quads[3].vertices[0].use_texture);
}

void test_world_state_rejects_door_pages_missing_from_vswap(void)
{
    WolfWorldState world = {0};

    TEST_ASSERT_FALSE(initialise_door_world(&world, 90u, 7u));
}

void test_world_state_use_selects_pushwall_direction_from_player_yaw(void)
{
    static const int8_t expected_x[] = {0, 1, 0, -1};
    static const int8_t expected_y[] = {-1, 0, 1, 0};
    static const float yaws[] = {
        0.0f, 1.57079632679f, 3.14159265359f, 4.71238898038f
    };

    for (size_t direction = 0u; direction < 4u; ++direction) {
        WolfLevel level = create_test_level();
        WolfWorldState world = {0};
        int player_x = 2 - expected_x[direction];
        int player_y = 2 - expected_y[direction];

        level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 7u;
        level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] =
            WOLF_LEVEL_INFO_PUSHWALL_MARKER;
        TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 98u));

        TEST_ASSERT_TRUE(wolf_world_state_use(
            &world, tile_center_world(player_x, 5u),
            tile_center_world(player_y, 5u), yaws[direction]));
        TEST_ASSERT_TRUE(world.pushwall.active);
        TEST_ASSERT_EQUAL_INT8(expected_x[direction], world.pushwall.direction_x);
        TEST_ASSERT_EQUAL_INT8(expected_y[direction], world.pushwall.direction_y);
    }
}

void test_world_state_pushwall_moves_one_tile_per_original_128_tics(void)
{
    WolfLevel level = create_test_level();
    WolfWorldState world = {0};
    float player_x = tile_center_world(1, 5u);
    float player_z = tile_center_world(2, 5u);
    bool geometry_changed = false;

    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 7u;
    level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] =
        WOLF_LEVEL_INFO_PUSHWALL_MARKER;
    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 98u));

    TEST_ASSERT_TRUE(wolf_world_state_use(
        &world, player_x, player_z, 1.57079632679f));
    TEST_ASSERT_TRUE(wolf_world_state_position_overlaps_solid(
        &world, tile_center_world(3, 5u), player_z, 0.20f));

    advance_world_ticks(&world, 126.0f, player_x, player_z);
    TEST_ASSERT_TRUE(world.pushwall.active);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 127.0f, world.pushwall.elapsed_ticks);

    TEST_ASSERT_TRUE(wolf_world_state_update(
        &world, 1.0f / WOLF_WORLD_DOOR_TICKS_PER_SECOND,
        player_x, player_z, &geometry_changed));
    TEST_ASSERT_TRUE(geometry_changed);
    TEST_ASSERT_TRUE(world.pushwall.active);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 128.0f, world.pushwall.elapsed_ticks);
    TEST_ASSERT_EQUAL_UINT32(4u, wolf_world_state_dynamic_quad_count(&world));

    advance_world_ticks(&world, 128.0f, player_x, player_z);
    TEST_ASSERT_FALSE(world.pushwall.active);
    TEST_ASSERT_EQUAL_UINT16(
        7u, world.level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 4u]);
    TEST_ASSERT_EQUAL_UINT16(
        0u, world.level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u]);
}

void test_world_state_pushwall_stops_after_one_tile_when_second_cell_is_blocked(void)
{
    WolfLevel level = create_test_level();
    WolfWorldState world = {0};
    float player_x = tile_center_world(1, 5u);
    float player_z = tile_center_world(2, 5u);

    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 7u;
    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 4u] = 1u;
    level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] =
        WOLF_LEVEL_INFO_PUSHWALL_MARKER;
    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 98u));

    TEST_ASSERT_TRUE(wolf_world_state_use(
        &world, player_x, player_z, 1.57079632679f));
    advance_world_ticks(&world, 127.0f, player_x, player_z);

    TEST_ASSERT_FALSE(world.pushwall.active);
    TEST_ASSERT_EQUAL_UINT16(
        7u, world.level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 3u]);
    TEST_ASSERT_EQUAL_UINT16(
        1u, world.level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 4u]);
}

void test_world_state_rejects_pushwall_when_first_cell_is_blocked(void)
{
    WolfLevel level = create_test_level();
    WolfWorldState world = {0};

    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 7u;
    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 1u;
    level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] =
        WOLF_LEVEL_INFO_PUSHWALL_MARKER;
    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 98u));

    TEST_ASSERT_FALSE(wolf_world_state_use(
        &world, tile_center_world(1, 5u), tile_center_world(2, 5u),
        1.57079632679f));
    TEST_ASSERT_FALSE(world.pushwall.active);
}

void test_world_state_builds_dynamic_secret_wall_geometry(void)
{
    WolfLevel level = create_test_level();
    WolfWorldState world = {0};
    VulkanQuad quads[4];

    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 7u;
    level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] =
        WOLF_LEVEL_INFO_PUSHWALL_MARKER;
    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 98u));
    TEST_ASSERT_TRUE(wolf_world_state_use(
        &world, tile_center_world(1, 5u), tile_center_world(2, 5u),
        1.57079632679f));

    TEST_ASSERT_EQUAL_UINT32(4u, wolf_world_state_dynamic_quad_count(&world));
    TEST_ASSERT_TRUE(wolf_world_state_build_dynamic_quads(&world, quads, 4u));
    for (size_t index = 0u; index < 4u; ++index) {
        TEST_ASSERT_EQUAL_UINT32(1u, quads[index].vertices[0].use_texture);
    }
}

void test_world_state_collects_original_yellow_key_marker(void)
{
    WolfLevel level = create_test_level();
    WolfWorldState world = {0};
    float player_x = tile_center_world(2, 5u);
    float player_z = tile_center_world(2, 5u);

    level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 44u;
    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 98u));
    TEST_ASSERT_TRUE(wolf_world_state_collect_key(&world, player_x, player_z));
    TEST_ASSERT_EQUAL_UINT8(2u, world.key_mask);
}

