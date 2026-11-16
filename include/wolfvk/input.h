#ifndef WOLFVK_INPUT_H
#define WOLFVK_INPUT_H

#include <SDL3/SDL.h>

#include <stdbool.h>

typedef struct WolfInput WolfInput;

/* Create an input manager and open the first available SDL gamepad. */
WolfInput *wolf_input_create(void);

/* Close any open gamepad and release the input manager. */
void wolf_input_destroy(WolfInput *input);

/* Clear per-frame mouse motion and refresh the connected gamepad state. */
void wolf_input_begin_frame(WolfInput *input);

/* Translate SDL events once so gameplay reads stable device-independent state each frame. */
void wolf_input_handle_event(WolfInput *input, const SDL_Event *event);

/* Query a held mouse button; left mouse is the primary fire control. */
bool wolf_input_mouse_button_down(const WolfInput *input, uint8_t button);

/* Query keyboard state and accumulated relative mouse movement. */
bool wolf_input_key_down(const WolfInput *input, SDL_Scancode key);
void wolf_input_mouse_delta(const WolfInput *input, float *x, float *y);

/* Query the currently selected gamepad, if one is connected. */
bool wolf_input_has_gamepad(const WolfInput *input);
float wolf_input_gamepad_axis(const WolfInput *input, SDL_GamepadAxis axis);
bool wolf_input_gamepad_button_down(const WolfInput *input, SDL_GamepadButton button);

/* Convert SDL's signed joystick axis range to [-1, 1], applying a stick deadzone. */
float wolf_input_normalize_axis(Sint16 value, float deadzone);

#endif
