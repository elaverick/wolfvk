#include "unity.h"

#include "wolfvk/player_controller.h"
#include "wolfvk/world.h"

#include <math.h>

static WolfInput *create_test_input(void)
{
    return wolf_input_create();
}

static void set_key(WolfInput *input, SDL_Scancode key, bool pressed)
{
    SDL_Event event = {0};

    event.type = pressed ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = key;
    wolf_input_handle_event(input, &event);
}

static bool create_open_test_world(WolfWorldState *world)
{
    WolfLevel level = {0};

    level.width = 5u;
    level.height = 5u;
    return wolf_world_state_init(world, &level, 98u);
}

void test_player_moves_forward_relative_to_camera(void)
{
    WolfCamera camera;
    WolfWorldState world = {0};
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    TEST_ASSERT_TRUE(create_open_test_world(&world));
    set_key(input, SDL_SCANCODE_W, true);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &world, 0.05f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, camera.position[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.56f, camera.position[2]);

    wolf_input_destroy(input);
}

void test_player_strafe_uses_camera_right_vector(void)
{
    WolfCamera camera;
    WolfWorldState world = {0};
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    TEST_ASSERT_TRUE(create_open_test_world(&world));
    set_key(input, SDL_SCANCODE_D, true);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &world, 0.05f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.56f, camera.position[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, camera.position[2]);

    wolf_input_destroy(input);
}

void test_player_normalizes_diagonal_movement_speed(void)
{
    WolfCamera camera;
    WolfWorldState world = {0};
    WolfInput *input = create_test_input();
    float distance;

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    TEST_ASSERT_TRUE(create_open_test_world(&world));
    set_key(input, SDL_SCANCODE_W, true);
    set_key(input, SDL_SCANCODE_D, true);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &world, 0.05f));
    distance = sqrtf(
        camera.position[0] * camera.position[0] +
        camera.position[2] * camera.position[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.56f, distance);

    wolf_input_destroy(input);
}

void test_player_mouse_motion_changes_yaw_and_pitch(void)
{
    WolfCamera camera;
    WolfWorldState world = {0};
    WolfInput *input = create_test_input();
    SDL_Event event = {0};

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    TEST_ASSERT_TRUE(create_open_test_world(&world));

    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.xrel = 40.0f;
    event.motion.yrel = -20.0f;
    wolf_input_handle_event(input, &event);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &world, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.1f, camera.yaw_radians);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.05f, camera.pitch_radians);

    wolf_input_destroy(input);
}

void test_player_cannot_walk_through_wall_tiles(void)
{
    WolfCamera camera;
    WolfWorldState world = {0};
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    TEST_ASSERT_TRUE(create_open_test_world(&world));
    world.level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 1u;
    set_key(input, SDL_SCANCODE_D, true);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &world, 0.1f));
    TEST_ASSERT_TRUE(camera.position[0] > 0.85f);
    TEST_ASSERT_TRUE(camera.position[0] < 0.97f);

    wolf_input_destroy(input);
}

void test_player_slides_along_wall_when_forward_motion_is_blocked(void)
{
    WolfCamera camera;
    WolfWorldState world = {0};
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    TEST_ASSERT_TRUE(create_open_test_world(&world));
    camera.position[0] = 0.2f * WOLF_WORLD_TILE_SIZE;
    world.level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 1u;
    set_key(input, SDL_SCANCODE_W, true);
    set_key(input, SDL_SCANCODE_D, true);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &world, 0.1f));
    TEST_ASSERT_TRUE(camera.position[0] < 0.97f);
    TEST_ASSERT_TRUE(camera.position[2] < -0.20f);

    wolf_input_destroy(input);
}

void test_player_forward_direction_tracks_camera_yaw(void)
{
    WolfCamera camera;
    WolfWorldState world = {0};
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    camera.yaw_radians = 1.57079633f;
    TEST_ASSERT_TRUE(create_open_test_world(&world));
    set_key(input, SDL_SCANCODE_W, true);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &world, 0.05f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.56f, camera.position[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, camera.position[2]);

    wolf_input_destroy(input);
}

void test_player_mouse_look_clamps_pitch(void)
{
    WolfCamera camera;
    WolfWorldState world = {0};
    WolfInput *input = create_test_input();
    SDL_Event event = {0};

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    TEST_ASSERT_TRUE(create_open_test_world(&world));

    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.yrel = -10000.0f;
    wolf_input_handle_event(input, &event);
    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &world, 0.0f));
    TEST_ASSERT_TRUE(camera.pitch_radians > 1.4f);
    TEST_ASSERT_TRUE(camera.pitch_radians < 1.5f);

    wolf_input_begin_frame(input);
    event.motion.yrel = 10000.0f;
    wolf_input_handle_event(input, &event);
    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &world, 0.0f));
    TEST_ASSERT_TRUE(camera.pitch_radians < -1.4f);
    TEST_ASSERT_TRUE(camera.pitch_radians > -1.5f);

    wolf_input_destroy(input);
}

void test_player_opens_door_before_crossing_and_door_closes_on_original_timer(void)
{
    WolfCamera camera;
    WolfWorldState world = {0};
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    TEST_ASSERT_TRUE(create_open_test_world(&world));

    /* Approach the vertical normal door from the west, as the original Use action expects. */
    camera.position[0] = -2.0f;
    camera.position[2] = 0.0f;
    camera.yaw_radians = 1.57079633f;
    world.level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 90u;
    world.level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 1u;
    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &world.level, 98u));

    set_key(input, SDL_SCANCODE_W, true);
    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &world, 0.1f));
    TEST_ASSERT_TRUE(camera.position[0] < -2.0f);

    /* A single Use press starts the 70 Hz, tics<<10 opening animation. */
    TEST_ASSERT_TRUE(wolf_world_state_use(
        &world, camera.position[0], camera.position[2], camera.yaw_radians));
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_OPENING, world.doors[0].action);

    for (int frame = 0; frame < 10; ++frame) {
        bool geometry_changed = false;
        TEST_ASSERT_TRUE(wolf_world_state_update(
            &world, 0.1f, camera.position[0], camera.position[2], &geometry_changed));
    }
    TEST_ASSERT_EQUAL_UINT8(WOLF_DOOR_OPEN, world.doors[0].action);
    TEST_ASSERT_EQUAL_UINT32(WOLF_WORLD_DOOR_OPEN_POSITION, world.doors[0].position);

    /* The player now passes through the fully-open door and stops at the next wall. */
    for (int frame = 0; frame < 16; ++frame) {
        bool geometry_changed = false;
        TEST_ASSERT_TRUE(wolf_world_state_update(
            &world, 0.1f, camera.position[0], camera.position[2], &geometry_changed));
        TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &world, 0.1f));
    }
    TEST_ASSERT_TRUE(camera.position[0] > 0.85f);
    TEST_ASSERT_TRUE(camera.position[0] < 0.97f);

    wolf_input_destroy(input);
}

void test_player_pushes_secret_wall_in_facing_direction_at_original_speed(void)
{
    WolfCamera camera;
    WolfWorldState world = {0};
    WolfInput *input = create_test_input();
    bool geometry_changed = false;

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    TEST_ASSERT_TRUE(create_open_test_world(&world));

    camera.position[0] = -2.0f;
    camera.position[2] = 0.0f;
    camera.yaw_radians = 1.57079633f;
    world.level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 1u;
    world.level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] =
        WOLF_LEVEL_INFO_PUSHWALL_MARKER;
    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &world.level, 98u));

    /* The P marker is actionable only when the player faces the marked wall. */
    TEST_ASSERT_TRUE(wolf_world_state_use(
        &world, camera.position[0], camera.position[2], camera.yaw_radians));
    TEST_ASSERT_TRUE(world.pushwall.active);
    TEST_ASSERT_EQUAL_INT8(1, world.pushwall.direction_x);
    TEST_ASSERT_EQUAL_INT8(0, world.pushwall.direction_y);

    /* One original tile takes 128 tics at 70 tics per second (about 1.83 seconds). */
    TEST_ASSERT_TRUE(wolf_world_state_update(
        &world, 128.0f / 70.0f, camera.position[0], camera.position[2],
        &geometry_changed));
    TEST_ASSERT_TRUE(geometry_changed);
    TEST_ASSERT_TRUE(world.pushwall.active);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 128.0f, world.pushwall.elapsed_ticks);

    /* A clear second tile allows the original two-cell push to finish at 256 tics. */
    TEST_ASSERT_TRUE(wolf_world_state_update(
        &world, 128.0f / 70.0f, camera.position[0], camera.position[2],
        &geometry_changed));
    TEST_ASSERT_FALSE(world.pushwall.active);
    TEST_ASSERT_EQUAL_UINT16(
        1u, world.level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 4u]);
    TEST_ASSERT_EQUAL_UINT16(
        0u, world.level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u]);

    wolf_input_destroy(input);
}

void test_player_rejects_invalid_state_and_frame_time(void)
{
    WolfCamera camera;
    WolfWorldState world = {0};
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    TEST_ASSERT_TRUE(create_open_test_world(&world));

    TEST_ASSERT_FALSE(wolf_player_update(NULL, input, &world, 0.016f));
    TEST_ASSERT_FALSE(wolf_player_update(&camera, input, NULL, 0.016f));
    TEST_ASSERT_FALSE(wolf_player_update(&camera, input, &world, -0.01f));
    TEST_ASSERT_FALSE(wolf_player_update(&camera, input, &world, NAN));

    wolf_input_destroy(input);
}
