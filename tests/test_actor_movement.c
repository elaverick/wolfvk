#include "unity.h"

#include "wolfvk/actor_movement.h"
#include "wolfvk/level_objects.h"
#include "wolfvk/world.h"

#include <math.h>
#include <string.h>

static const WolfActorStateDefinition movement_states[] = {
    {"grd_w", 0u, 0u, WOLF_ACTOR_STATE_NONE, NULL, NULL}
};

static WolfLevel open_level(void)
{
    WolfLevel level = {0};
    level.width = 8u;
    level.height = 8u;
    return level;
}

static float world_coordinate(uint16_t tile, uint16_t extent)
{
    return ((float)tile + 0.5f - (float)extent * 0.5f) * WOLF_WORLD_TILE_SIZE;
}

static bool create_actor(
    WolfActor *actor, uint32_t id, uint16_t x, uint16_t y, float yaw)
{
    return wolf_actor_initialize(
        actor, id, WOLF_ACTOR_GUARD, x, y,
        world_coordinate(x, 8u), world_coordinate(y, 8u),
        yaw, 25, movement_states, 1u, 0u, NULL);
}

static void advance_actor(
    WolfActor *actor, uint32_t count, WolfWorldState *world, float player_x, float player_z,
    uint32_t tics)
{
    for (uint32_t tic = 0u; tic < tics; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_movement_update_tic(
            actor, count, world, player_x, player_z));
    }
}

void test_actor_movement_traverses_all_eight_grid_directions(void)
{
    static const WolfActorDirection directions[] = {
        WOLF_ACTOR_DIRECTION_EAST, WOLF_ACTOR_DIRECTION_NORTH_EAST,
        WOLF_ACTOR_DIRECTION_NORTH, WOLF_ACTOR_DIRECTION_NORTH_WEST,
        WOLF_ACTOR_DIRECTION_WEST, WOLF_ACTOR_DIRECTION_SOUTH_WEST,
        WOLF_ACTOR_DIRECTION_SOUTH, WOLF_ACTOR_DIRECTION_SOUTH_EAST
    };
    static const int expected_x[] = {4, 4, 3, 2, 2, 2, 3, 4};
    static const int expected_y[] = {3, 2, 2, 2, 3, 4, 4, 4};

    for (size_t index = 0u; index < sizeof(directions) / sizeof(directions[0]); ++index) {
        WolfLevel level = open_level();
        WolfWorldState world = {0};
        WolfActor actor;

        TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 0u));
        TEST_ASSERT_TRUE(create_actor(&actor, 1u, 3u, 3u, 0.0f));
        TEST_ASSERT_TRUE(wolf_actor_movement_set_direction(&actor, directions[index]));
        advance_actor(&actor, 1u, &world, -100.0f, -100.0f, 40u);
        TEST_ASSERT_EQUAL_UINT16(expected_x[index], actor.tile_x);
        TEST_ASSERT_EQUAL_UINT16(expected_y[index], actor.tile_y);
        TEST_ASSERT_FLOAT_WITHIN(0.001f,
            world_coordinate((uint16_t)expected_x[index], 8u), actor.position_x);
        TEST_ASSERT_FLOAT_WITHIN(0.001f,
            world_coordinate((uint16_t)expected_y[index], 8u), actor.position_z);
    }
}

void test_actor_movement_stops_at_solid_walls_and_map_boundaries(void)
{
    WolfLevel level = open_level();
    WolfWorldState world = {0};
    WolfActor actor;

    level.wall_tiles[3u * WOLF_LEVEL_PLANE_WIDTH + 4u] = 1u;
    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 0u));
    TEST_ASSERT_TRUE(create_actor(&actor, 1u, 3u, 3u, 0.0f));
    TEST_ASSERT_TRUE(wolf_actor_movement_set_direction(&actor, WOLF_ACTOR_DIRECTION_EAST));
    advance_actor(&actor, 1u, &world, -100.0f, -100.0f, 60u);
    TEST_ASSERT_EQUAL_UINT16(3u, actor.tile_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, world_coordinate(3u, 8u), actor.position_x);

    world.level.wall_tiles[3u * WOLF_LEVEL_PLANE_WIDTH + 4u] = 0u;
    TEST_ASSERT_TRUE(wolf_actor_movement_set_direction(&actor, WOLF_ACTOR_DIRECTION_EAST));
    advance_actor(&actor, 1u, &world, -100.0f, -100.0f, 40u);
    TEST_ASSERT_EQUAL_UINT16(4u, actor.tile_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, world_coordinate(4u, 8u), actor.position_x);

    TEST_ASSERT_TRUE(wolf_actor_movement_set_direction(&actor, WOLF_ACTOR_DIRECTION_EAST));
    advance_actor(&actor, 1u, &world, -100.0f, -100.0f, 60u);
    TEST_ASSERT_EQUAL_UINT16(7u, actor.tile_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, world_coordinate(7u, 8u), actor.position_x);
}

void test_actor_movement_respects_blocking_statics_but_ignores_passable_objects(void)
{
    WolfLevel level = open_level();
    WolfWorldState world = {0};
    WolfActor actor;

    level.object_tiles[3u * WOLF_LEVEL_PLANE_WIDTH + 4u] = 24u;
    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 0u));
    TEST_ASSERT_TRUE(create_actor(&actor, 1u, 3u, 3u, 0.0f));
    TEST_ASSERT_TRUE(wolf_actor_movement_set_direction(&actor, WOLF_ACTOR_DIRECTION_EAST));
    advance_actor(&actor, 1u, &world, -100.0f, -100.0f, 45u);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, world_coordinate(3u, 8u), actor.position_x);

    world.level.object_tiles[3u * WOLF_LEVEL_PLANE_WIDTH + 4u] = 32u;
    advance_actor(&actor, 1u, &world, -100.0f, -100.0f, 40u);
    TEST_ASSERT_EQUAL_UINT16(4u, actor.tile_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, world_coordinate(4u, 8u), actor.position_x);
}

void test_actor_movement_waits_for_door_to_fully_open_before_crossing(void)
{
    WolfLevel level = open_level();
    WolfWorldState world = {0};
    WolfActor actor;
    float player_x = -100.0f;
    float player_z = -100.0f;

    level.wall_tiles[3u * WOLF_LEVEL_PLANE_WIDTH + 4u] = 90u;
    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 8u));
    TEST_ASSERT_TRUE(create_actor(&actor, 1u, 3u, 3u, 0.0f));
    TEST_ASSERT_TRUE(wolf_actor_movement_set_direction(&actor, WOLF_ACTOR_DIRECTION_EAST));
    advance_actor(&actor, 1u, &world, player_x, player_z, 1u);
    TEST_ASSERT_TRUE(actor.movement_waiting_for_door);
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_OPENING, world.doors[0].action);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, world_coordinate(3u, 8u), actor.position_x);

    for (uint32_t frame = 0u; frame < 10u; ++frame) {
        TEST_ASSERT_TRUE(wolf_world_state_update(&world, 0.1f, player_x, player_z, NULL));
    }
    TEST_ASSERT_TRUE(wolf_world_state_actor_door_is_open(&world, 4u, 3u));
    advance_actor(&actor, 1u, &world, player_x, player_z, 40u);
    TEST_ASSERT_FALSE(actor.movement_waiting_for_door);
    TEST_ASSERT_EQUAL_UINT16(4u, actor.tile_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, world_coordinate(4u, 8u), actor.position_x);
}

void test_actor_movement_prevents_actor_overlap_and_reserves_destination_tiles(void)
{
    WolfLevel level = open_level();
    WolfWorldState world = {0};
    WolfActor actors[2];

    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 0u));
    TEST_ASSERT_TRUE(create_actor(&actors[0], 1u, 3u, 3u, 0.0f));
    TEST_ASSERT_TRUE(create_actor(&actors[1], 2u, 4u, 3u, 0.0f));
    TEST_ASSERT_TRUE(wolf_actor_movement_set_direction(
        &actors[0], WOLF_ACTOR_DIRECTION_EAST));
    advance_actor(actors, 2u, &world, -100.0f, -100.0f, 50u);
    TEST_ASSERT_EQUAL_UINT16(3u, actors[0].tile_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, world_coordinate(3u, 8u), actors[0].position_x);
}

void test_actor_movement_keeps_clearance_from_the_player(void)
{
    WolfLevel level = open_level();
    WolfWorldState world = {0};
    WolfActor actor;
    float player_x = world_coordinate(4u, 8u);
    float player_z = world_coordinate(3u, 8u);

    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 0u));
    TEST_ASSERT_TRUE(create_actor(&actor, 1u, 3u, 3u, 0.0f));
    TEST_ASSERT_TRUE(wolf_actor_movement_set_direction(&actor, WOLF_ACTOR_DIRECTION_EAST));
    advance_actor(&actor, 1u, &world, player_x, player_z, 40u);
    TEST_ASSERT_TRUE(actor.position_x < player_x);
    TEST_ASSERT_TRUE(player_x - actor.position_x >=
        (WOLF_ACTOR_COLLISION_RADIUS_TILES + 0.34375f) * WOLF_WORLD_TILE_SIZE - 0.09f);
}

void test_actor_movement_is_repeatable_for_identical_tic_sequences(void)
{
    WolfLevel level = open_level();
    WolfWorldState world_a = {0};
    WolfWorldState world_b = {0};
    WolfActor actor_a;
    WolfActor actor_b;

    TEST_ASSERT_TRUE(wolf_world_state_init(&world_a, &level, 0u));
    TEST_ASSERT_TRUE(wolf_world_state_init(&world_b, &level, 0u));
    TEST_ASSERT_TRUE(create_actor(&actor_a, 1u, 2u, 2u, 0.0f));
    TEST_ASSERT_TRUE(create_actor(&actor_b, 1u, 2u, 2u, 0.0f));
    TEST_ASSERT_TRUE(wolf_actor_movement_set_direction(&actor_a, WOLF_ACTOR_DIRECTION_SOUTH_EAST));
    TEST_ASSERT_TRUE(wolf_actor_movement_set_direction(&actor_b, WOLF_ACTOR_DIRECTION_SOUTH_EAST));
    advance_actor(&actor_a, 1u, &world_a, -100.0f, -100.0f, 40u);
    for (uint32_t batch = 0u; batch < 4u; ++batch) {
        advance_actor(&actor_b, 1u, &world_b, -100.0f, -100.0f, 10u);
    }
    TEST_ASSERT_EQUAL_UINT16(actor_a.tile_x, actor_b.tile_x);
    TEST_ASSERT_EQUAL_UINT16(actor_a.tile_y, actor_b.tile_y);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, actor_a.position_x, actor_b.position_x);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, actor_a.position_z, actor_b.position_z);
}

void test_actor_movement_rejects_invalid_arguments_and_directions(void)
{
    WolfActor actor;
    WolfLevel level = open_level();
    WolfWorldState world = {0};

    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 0u));
    TEST_ASSERT_FALSE(wolf_actor_movement_set_direction(NULL, WOLF_ACTOR_DIRECTION_NORTH));
    TEST_ASSERT_FALSE(wolf_actor_movement_set_direction(
        &actor, (WolfActorDirection)99));
    TEST_ASSERT_FALSE(wolf_actor_movement_update_tic(
        NULL, 1u, &world, 0.0f, 0.0f));
    TEST_ASSERT_FALSE(wolf_actor_movement_update_tic(
        &actor, 1u, NULL, 0.0f, 0.0f));
    TEST_ASSERT_FALSE(wolf_actor_movement_update_tic(
        NULL, 0u, &world, NAN, 0.0f));
}
