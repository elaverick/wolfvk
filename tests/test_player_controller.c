#include "unity.h"

#include "wolfvk/player_controller.h"

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

static void create_open_test_level(WolfLevel *level)
{
    *level = (WolfLevel){0};
    level->width = 5u;
    level->height = 5u;
}

void test_player_moves_forward_relative_to_camera(void)
{
    WolfCamera camera;
    WolfLevel level;
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    create_open_test_level(&level);
    set_key(input, SDL_SCANCODE_W, true);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &level, 0.05f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, camera.position[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.175f, camera.position[2]);

    wolf_input_destroy(input);
}

void test_player_strafe_uses_camera_right_vector(void)
{
    WolfCamera camera;
    WolfLevel level;
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    create_open_test_level(&level);
    set_key(input, SDL_SCANCODE_D, true);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &level, 0.05f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.175f, camera.position[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, camera.position[2]);

    wolf_input_destroy(input);
}

void test_player_normalizes_diagonal_movement_speed(void)
{
    WolfCamera camera;
    WolfLevel level;
    WolfInput *input = create_test_input();
    float distance;

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    create_open_test_level(&level);
    set_key(input, SDL_SCANCODE_W, true);
    set_key(input, SDL_SCANCODE_D, true);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &level, 0.05f));
    distance = sqrtf(
        camera.position[0] * camera.position[0] +
        camera.position[2] * camera.position[2]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.175f, distance);

    wolf_input_destroy(input);
}

void test_player_mouse_motion_changes_yaw_and_pitch(void)
{
    WolfCamera camera;
    WolfLevel level;
    WolfInput *input = create_test_input();
    SDL_Event event = {0};

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    create_open_test_level(&level);

    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.xrel = 40.0f;
    event.motion.yrel = -20.0f;
    wolf_input_handle_event(input, &event);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &level, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.1f, camera.yaw_radians);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.05f, camera.pitch_radians);

    wolf_input_destroy(input);
}

void test_player_cannot_walk_through_wall_tiles(void)
{
    WolfCamera camera;
    WolfLevel level;
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    create_open_test_level(&level);
    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 1u;
    set_key(input, SDL_SCANCODE_D, true);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &level, 0.1f));
    TEST_ASSERT_TRUE(camera.position[0] > 0.20f);
    TEST_ASSERT_TRUE(camera.position[0] < 0.31f);

    wolf_input_destroy(input);
}

void test_player_slides_along_wall_when_forward_motion_is_blocked(void)
{
    WolfCamera camera;
    WolfLevel level;
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    create_open_test_level(&level);
    camera.position[0] = 0.2f;
    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 1u;
    set_key(input, SDL_SCANCODE_W, true);
    set_key(input, SDL_SCANCODE_D, true);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &level, 0.1f));
    TEST_ASSERT_TRUE(camera.position[0] < 0.31f);
    TEST_ASSERT_TRUE(camera.position[2] < -0.20f);

    wolf_input_destroy(input);
}

void test_player_forward_direction_tracks_camera_yaw(void)
{
    WolfCamera camera;
    WolfLevel level;
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    camera.yaw_radians = 1.57079633f;
    create_open_test_level(&level);
    set_key(input, SDL_SCANCODE_W, true);

    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &level, 0.05f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.175f, camera.position[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, camera.position[2]);

    wolf_input_destroy(input);
}

void test_player_mouse_look_clamps_pitch(void)
{
    WolfCamera camera;
    WolfLevel level;
    WolfInput *input = create_test_input();
    SDL_Event event = {0};

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    create_open_test_level(&level);

    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.yrel = -10000.0f;
    wolf_input_handle_event(input, &event);
    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &level, 0.0f));
    TEST_ASSERT_TRUE(camera.pitch_radians > 1.4f);
    TEST_ASSERT_TRUE(camera.pitch_radians < 1.5f);

    wolf_input_begin_frame(input);
    event.motion.yrel = 10000.0f;
    wolf_input_handle_event(input, &event);
    TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &level, 0.0f));
    TEST_ASSERT_TRUE(camera.pitch_radians < -1.4f);
    TEST_ASSERT_TRUE(camera.pitch_radians > -1.5f);

    wolf_input_destroy(input);
}

void test_player_can_move_through_omitted_door_cells(void)
{
    WolfCamera camera;
    WolfLevel level;
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    create_open_test_level(&level);

    /* Approach the door from the west; the ordinary wall beyond it remains blocking. */
    camera.position[0] = -1.0f;
    camera.position[2] = 0.0f;
    camera.yaw_radians = 1.57079633f;
    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] =
        WOLF_LEVEL_DOOR_TILE_FIRST;
    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 1u;
    set_key(input, SDL_SCANCODE_W, true);

    for (int frame = 0; frame < 16; ++frame) {
        TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &level, 0.1f));
    }

    /* The player must cross the door cell and stop at the solid wall behind it. */
    TEST_ASSERT_TRUE(camera.position[0] > 0.1f);
    TEST_ASSERT_TRUE(camera.position[0] < 0.4f);

    wolf_input_destroy(input);
}

void test_player_can_move_through_secret_door_cells(void)
{
    WolfCamera camera;
    WolfLevel level;
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    create_open_test_level(&level);

    /* A pushwall marker in the info plane makes its ordinary wall tile an omitted secret door. */
    camera.position[0] = -1.0f;
    camera.position[2] = 0.0f;
    camera.yaw_radians = 1.57079633f;
    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 1u;
    level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] =
        WOLF_LEVEL_INFO_PUSHWALL_MARKER;
    level.wall_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 1u;
    set_key(input, SDL_SCANCODE_W, true);

    for (int frame = 0; frame < 16; ++frame) {
        TEST_ASSERT_TRUE(wolf_player_update(&camera, input, &level, 0.1f));
    }

    /* The player crosses the marked cell but remains blocked by the next ordinary wall. */
    TEST_ASSERT_TRUE(camera.position[0] > 0.1f);
    TEST_ASSERT_TRUE(camera.position[0] < 0.4f);

    wolf_input_destroy(input);
}

void test_player_rejects_invalid_state_and_frame_time(void)
{
    WolfCamera camera;
    WolfLevel level;
    WolfInput *input = create_test_input();

    TEST_ASSERT_NOT_NULL(input);
    wolf_camera_initialize(&camera);
    create_open_test_level(&level);

    TEST_ASSERT_FALSE(wolf_player_update(NULL, input, &level, 0.016f));
    TEST_ASSERT_FALSE(wolf_player_update(&camera, input, NULL, 0.016f));
    TEST_ASSERT_FALSE(wolf_player_update(&camera, input, &level, -0.01f));
    TEST_ASSERT_FALSE(wolf_player_update(&camera, input, &level, NAN));

    wolf_input_destroy(input);
}
