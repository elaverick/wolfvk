#include "unity.h"

#include "wolfvk/input.h"

void test_keyboard_events_update_key_state(void)
{
    WolfInput *input = wolf_input_create();
    SDL_Event event = {0};

    TEST_ASSERT_NOT_NULL(input);

    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_W;
    wolf_input_handle_event(input, &event);
    TEST_ASSERT_TRUE(wolf_input_key_down(input, SDL_SCANCODE_W));

    event.type = SDL_EVENT_KEY_UP;
    wolf_input_handle_event(input, &event);
    TEST_ASSERT_FALSE(wolf_input_key_down(input, SDL_SCANCODE_W));

    wolf_input_destroy(input);
}

void test_mouse_motion_accumulates_until_next_frame(void)
{
    WolfInput *input = wolf_input_create();
    SDL_Event event = {0};
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;

    TEST_ASSERT_NOT_NULL(input);

    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.xrel = 4.0f;
    event.motion.yrel = -2.0f;
    wolf_input_handle_event(input, &event);

    event.motion.xrel = 3.0f;
    event.motion.yrel = 5.0f;
    wolf_input_handle_event(input, &event);

    wolf_input_mouse_delta(input, &mouse_x, &mouse_y);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, mouse_x);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, mouse_y);

    wolf_input_begin_frame(input);
    wolf_input_mouse_delta(input, &mouse_x, &mouse_y);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mouse_x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mouse_y);

    wolf_input_destroy(input);
}


void test_mouse_button_events_update_held_state(void)
{
    WolfInput *input = wolf_input_create();
    SDL_Event event = {0};

    TEST_ASSERT_NOT_NULL(input);
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    wolf_input_handle_event(input, &event);
    TEST_ASSERT_TRUE(wolf_input_mouse_button_down(input, SDL_BUTTON_LEFT));

    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    wolf_input_handle_event(input, &event);
    TEST_ASSERT_FALSE(wolf_input_mouse_button_down(input, SDL_BUTTON_LEFT));
    TEST_ASSERT_FALSE(wolf_input_mouse_button_down(input, 0u));
    TEST_ASSERT_FALSE(wolf_input_mouse_button_down(NULL, SDL_BUTTON_LEFT));
    wolf_input_destroy(input);
}

void test_gamepad_axis_normalization_applies_deadzone(void)
{
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wolf_input_normalize_axis(0, 0.15f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wolf_input_normalize_axis(4000, 0.15f));
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, 1.0f, wolf_input_normalize_axis(32767, 0.15f));
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, -1.0f, wolf_input_normalize_axis(-32768, 0.15f));
}

void test_input_queries_handle_invalid_arguments(void)
{
    WolfInput *input = wolf_input_create();

    TEST_ASSERT_NOT_NULL(input);
    TEST_ASSERT_FALSE(wolf_input_key_down(input, SDL_SCANCODE_UNKNOWN));
    TEST_ASSERT_FALSE(wolf_input_key_down(input, SDL_SCANCODE_COUNT));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wolf_input_gamepad_axis(input, SDL_GAMEPAD_AXIS_INVALID));
    TEST_ASSERT_FALSE(
        wolf_input_gamepad_button_down(input, SDL_GAMEPAD_BUTTON_INVALID));
    TEST_ASSERT_FALSE(wolf_input_key_down(NULL, SDL_SCANCODE_W));

    wolf_input_destroy(input);
}
