#include "wolfvk/input.h"

#include <math.h>
#include <stdlib.h>

#define WOLF_INPUT_STICK_DEADZONE 0.15f

struct WolfInput {
    bool keys[SDL_SCANCODE_COUNT];
    float mouse_delta_x;
    float mouse_delta_y;
    SDL_Gamepad *gamepad;
    SDL_JoystickID gamepad_id;
    float gamepad_axes[SDL_GAMEPAD_AXIS_COUNT];
    bool gamepad_buttons[SDL_GAMEPAD_BUTTON_COUNT];
};

static void close_gamepad(WolfInput *input)
{
    if (input->gamepad != NULL) {
        SDL_CloseGamepad(input->gamepad);
        input->gamepad = NULL;
    }

    input->gamepad_id = 0;
    for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis) {
        input->gamepad_axes[axis] = 0.0f;
    }
    for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button) {
        input->gamepad_buttons[button] = false;
    }
}

static bool open_gamepad(WolfInput *input, SDL_JoystickID instance_id)
{
    SDL_Gamepad *gamepad = SDL_OpenGamepad(instance_id);

    if (gamepad == NULL) {
        return false;
    }

    input->gamepad = gamepad;
    input->gamepad_id = SDL_GetGamepadID(gamepad);
    return true;
}

static void open_first_available_gamepad(WolfInput *input)
{
    SDL_JoystickID *gamepad_ids;
    int gamepad_count = 0;

    if (input->gamepad != NULL) {
        return;
    }

    gamepad_ids = SDL_GetGamepads(&gamepad_count);
    if (gamepad_ids == NULL) {
        return;
    }

    for (int index = 0; index < gamepad_count; ++index) {
        if (open_gamepad(input, gamepad_ids[index])) {
            break;
        }
    }

    SDL_free(gamepad_ids);
}

float wolf_input_normalize_axis(Sint16 value, float deadzone)
{
    float normalized = value < 0
        ? (float)value / 32768.0f
        : (float)value / 32767.0f;
    float magnitude = fabsf(normalized);

    if (deadzone < 0.0f) {
        deadzone = 0.0f;
    } else if (deadzone > 0.95f) {
        deadzone = 0.95f;
    }

    if (magnitude <= deadzone) {
        return 0.0f;
    }

    magnitude = (magnitude - deadzone) / (1.0f - deadzone);
    return normalized < 0.0f ? -magnitude : magnitude;
}

WolfInput *wolf_input_create(void)
{
    WolfInput *input = calloc(1, sizeof(*input));

    if (input == NULL) {
        return NULL;
    }

    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        free(input);
        return NULL;
    }

    open_first_available_gamepad(input);
    return input;
}

void wolf_input_destroy(WolfInput *input)
{
    if (input == NULL) {
        return;
    }

    close_gamepad(input);
    free(input);
}

void wolf_input_begin_frame(WolfInput *input)
{
    if (input == NULL) {
        return;
    }

    input->mouse_delta_x = 0.0f;
    input->mouse_delta_y = 0.0f;

    if (input->gamepad == NULL) {
        open_first_available_gamepad(input);
        return;
    }

    for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis) {
        Sint16 raw_value = SDL_GetGamepadAxis(input->gamepad, (SDL_GamepadAxis)axis);
        float deadzone = axis <= SDL_GAMEPAD_AXIS_RIGHTY
            ? WOLF_INPUT_STICK_DEADZONE
            : 0.0f;

        if (axis >= SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
            input->gamepad_axes[axis] = (float)raw_value / 32767.0f;
        } else {
            input->gamepad_axes[axis] = wolf_input_normalize_axis(raw_value, deadzone);
        }
    }

    for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button) {
        input->gamepad_buttons[button] =
            SDL_GetGamepadButton(input->gamepad, (SDL_GamepadButton)button);
    }
}

void wolf_input_handle_event(WolfInput *input, const SDL_Event *event)
{
    if (input == NULL || event == NULL) {
        return;
    }

    switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
        if (event->key.scancode > SDL_SCANCODE_UNKNOWN &&
            event->key.scancode < SDL_SCANCODE_COUNT) {
            input->keys[event->key.scancode] = true;
        }
        break;

    case SDL_EVENT_KEY_UP:
        if (event->key.scancode > SDL_SCANCODE_UNKNOWN &&
            event->key.scancode < SDL_SCANCODE_COUNT) {
            input->keys[event->key.scancode] = false;
        }
        break;

    case SDL_EVENT_MOUSE_MOTION:
        input->mouse_delta_x += event->motion.xrel;
        input->mouse_delta_y += event->motion.yrel;
        break;

    case SDL_EVENT_GAMEPAD_ADDED:
        open_first_available_gamepad(input);
        break;

    case SDL_EVENT_GAMEPAD_REMOVED:
        if (input->gamepad != NULL && input->gamepad_id == event->gdevice.which) {
            close_gamepad(input);
            open_first_available_gamepad(input);
        }
        break;

    default:
        break;
    }
}

bool wolf_input_key_down(const WolfInput *input, SDL_Scancode key)
{
    if (input == NULL || key <= SDL_SCANCODE_UNKNOWN || key >= SDL_SCANCODE_COUNT) {
        return false;
    }

    return input->keys[key];
}

void wolf_input_mouse_delta(const WolfInput *input, float *x, float *y)
{
    if (x != NULL) {
        *x = input != NULL ? input->mouse_delta_x : 0.0f;
    }
    if (y != NULL) {
        *y = input != NULL ? input->mouse_delta_y : 0.0f;
    }
}

bool wolf_input_has_gamepad(const WolfInput *input)
{
    return input != NULL && input->gamepad != NULL;
}

float wolf_input_gamepad_axis(const WolfInput *input, SDL_GamepadAxis axis)
{
    if (input == NULL || input->gamepad == NULL ||
        axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT) {
        return 0.0f;
    }

    return input->gamepad_axes[axis];
}

bool wolf_input_gamepad_button_down(const WolfInput *input, SDL_GamepadButton button)
{
    if (input == NULL || input->gamepad == NULL ||
        button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT) {
        return false;
    }

    return input->gamepad_buttons[button];
}
