#include "wolfvk/player_controller.h"
#include "wolfvk/game_world.h"
#include "wolfvk/world.h"

#include <math.h>

#define WOLF_PLAYER_MOVE_SPEED_TILES_PER_SECOND 3.5f
#define WOLF_PLAYER_COLLISION_RADIUS_TILES 0.20f
#define WOLF_PLAYER_MOUSE_SENSITIVITY 0.0025f
#define WOLF_PLAYER_MAX_PITCH 1.48352986f
#define WOLF_PLAYER_MAX_FRAME_TIME 0.10f
#define WOLF_PLAYER_MAX_STEP_DISTANCE_TILES 0.08f
#define WOLF_PLAYER_TWO_PI 6.28318531f

/* Keep the player's circular footprint clear of ordinary walls, doors and moving pushwalls. */
static bool position_overlaps_wall(
    const WolfWorldState *world,
    float world_x,
    float world_z)
{
    return wolf_world_state_position_overlaps_solid(
        world, world_x, world_z, WOLF_PLAYER_COLLISION_RADIUS_TILES);
}

/* Find the furthest collision-free point along one axis of a movement step. */
static float resolve_axis_movement(
    const WolfWorldState *world,
    float start_x,
    float start_z,
    float movement,
    bool horizontal)
{
    const float target_x = horizontal ? start_x + movement : start_x;
    const float target_z = horizontal ? start_z : start_z + movement;

    if (!position_overlaps_wall(world, target_x, target_z)) {
        return movement;
    }

    /*
     * The coarse movement step detected the wall but may stop short by almost
     * one full step. Binary-search the safe portion so the player's collision
     * radius reaches the wall boundary without crossing it.
     */
    {
        float safe_fraction = 0.0f;
        float blocked_fraction = 1.0f;

        for (int iteration = 0; iteration < 12; ++iteration) {
            const float fraction = (safe_fraction + blocked_fraction) * 0.5f;
            const float candidate_x = horizontal
                ? start_x + movement * fraction
                : start_x;
            const float candidate_z = horizontal
                ? start_z
                : start_z + movement * fraction;

            if (position_overlaps_wall(world, candidate_x, candidate_z)) {
                blocked_fraction = fraction;
            } else {
                safe_fraction = fraction;
            }
        }

        return movement * safe_fraction;
    }
}

/* Subdivide movement to avoid tunnelling, then resolve axes independently to retain wall sliding. */
static void move_with_wall_sliding(
    WolfCamera *camera,
    const WolfWorldState *world,
    float movement_x,
    float movement_z,
    float delta_seconds)
{
    const float distance =
        WOLF_PLAYER_MOVE_SPEED_TILES_PER_SECOND * WOLF_WORLD_TILE_SIZE * delta_seconds;
    const float maximum_step_distance =
        WOLF_PLAYER_MAX_STEP_DISTANCE_TILES * WOLF_WORLD_TILE_SIZE;
    const float step_count_float = ceilf(distance / maximum_step_distance);
    const int step_count = step_count_float > 1.0f ? (int)step_count_float : 1;
    const float step_x = movement_x * distance / (float)step_count;
    const float step_z = movement_z * distance / (float)step_count;

    for (int step = 0; step < step_count; ++step) {
        /* Resolve each axis independently so blocked motion can still slide. */
        camera->position[0] += resolve_axis_movement(
            world, camera->position[0], camera->position[2], step_x, true);
        camera->position[2] += resolve_axis_movement(
            world, camera->position[0], camera->position[2], step_z, false);
    }
}

/* Apply mouse look and camera-relative movement while keeping the player inside the map. */
bool wolf_player_update(
    WolfCamera *camera,
    const WolfInput *input,
    const WolfWorldState *world,
    float delta_seconds)
{
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    float forward_axis = 0.0f;
    float strafe_axis = 0.0f;
    float movement_x;
    float movement_z;
    float movement_length;

    if (camera == NULL || input == NULL || world == NULL ||
        world->level.width == 0u || world->level.height == 0u ||
        world->level.width > WOLF_LEVEL_PLANE_WIDTH ||
        world->level.height > WOLF_LEVEL_PLANE_HEIGHT ||
        !isfinite(delta_seconds) || delta_seconds < 0.0f ||
        !isfinite(camera->position[0]) || !isfinite(camera->position[1]) ||
        !isfinite(camera->position[2]) || !isfinite(camera->yaw_radians) ||
        !isfinite(camera->pitch_radians)) {
        return false;
    }

    wolf_input_mouse_delta(input, &mouse_x, &mouse_y);
    if (!isfinite(mouse_x) || !isfinite(mouse_y)) {
        return false;
    }

    camera->yaw_radians += mouse_x * WOLF_PLAYER_MOUSE_SENSITIVITY;
    if (!isfinite(camera->yaw_radians)) {
        return false;
    }
    camera->yaw_radians = remainderf(camera->yaw_radians, WOLF_PLAYER_TWO_PI);
    camera->pitch_radians -= mouse_y * WOLF_PLAYER_MOUSE_SENSITIVITY;
    if (camera->pitch_radians > WOLF_PLAYER_MAX_PITCH) {
        camera->pitch_radians = WOLF_PLAYER_MAX_PITCH;
    } else if (camera->pitch_radians < -WOLF_PLAYER_MAX_PITCH) {
        camera->pitch_radians = -WOLF_PLAYER_MAX_PITCH;
    }

    if (wolf_input_key_down(input, SDL_SCANCODE_W)) {
        forward_axis += 1.0f;
    }
    if (wolf_input_key_down(input, SDL_SCANCODE_S)) {
        forward_axis -= 1.0f;
    }
    if (wolf_input_key_down(input, SDL_SCANCODE_D)) {
        strafe_axis += 1.0f;
    }
    if (wolf_input_key_down(input, SDL_SCANCODE_A)) {
        strafe_axis -= 1.0f;
    }

    movement_x = sinf(camera->yaw_radians) * forward_axis +
        cosf(camera->yaw_radians) * strafe_axis;
    movement_z = -cosf(camera->yaw_radians) * forward_axis +
        sinf(camera->yaw_radians) * strafe_axis;
    movement_length = sqrtf(movement_x * movement_x + movement_z * movement_z);

    if (movement_length > 1.0f) {
        movement_x /= movement_length;
        movement_z /= movement_length;
    }

    if (delta_seconds > WOLF_PLAYER_MAX_FRAME_TIME) {
        delta_seconds = WOLF_PLAYER_MAX_FRAME_TIME;
    }
    if (delta_seconds > 0.0f && movement_length > 0.0f) {
        move_with_wall_sliding(
            camera, world, movement_x, movement_z, delta_seconds);
    }

    return true;
}
