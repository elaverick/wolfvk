#include "wolfvk/player_controller.h"

#include <math.h>

#define WOLF_PLAYER_MOVE_SPEED 3.5f
#define WOLF_PLAYER_COLLISION_RADIUS 0.20f
#define WOLF_PLAYER_MOUSE_SENSITIVITY 0.0025f
#define WOLF_PLAYER_MAX_PITCH 1.48352986f
#define WOLF_PLAYER_MAX_FRAME_TIME 0.10f
#define WOLF_PLAYER_MAX_STEP_DISTANCE 0.08f
#define WOLF_PLAYER_TWO_PI 6.28318531f

static bool position_overlaps_wall(
    const WolfLevel *level,
    float world_x,
    float world_z)
{
    const float radius = WOLF_PLAYER_COLLISION_RADIUS;
    const float map_x = world_x + (float)level->width * 0.5f;
    const float map_z = world_z + (float)level->height * 0.5f;
    int minimum_x;
    int maximum_x;
    int minimum_z;
    int maximum_z;

    /* Keep the complete player footprint inside the level, even on malformed maps. */
    if (!isfinite(map_x) || !isfinite(map_z) ||
        map_x - radius < 0.0f || map_z - radius < 0.0f ||
        map_x + radius > (float)level->width ||
        map_z + radius > (float)level->height) {
        return true;
    }

    minimum_x = (int)floorf(map_x - radius);
    maximum_x = (int)floorf(map_x + radius);
    minimum_z = (int)floorf(map_z - radius);
    maximum_z = (int)floorf(map_z + radius);

    for (int tile_z = minimum_z; tile_z <= maximum_z; ++tile_z) {
        for (int tile_x = minimum_x; tile_x <= maximum_x; ++tile_x) {
            float closest_x;
            float closest_z;
            float distance_x;
            float distance_z;

            if (tile_x < 0 || tile_z < 0 ||
                tile_x >= level->width || tile_z >= level->height) {
                return true;
            }

            if (level->wall_tiles[
                    (size_t)tile_z * WOLF_LEVEL_PLANE_WIDTH + (size_t)tile_x] == 0u) {
                continue;
            }

            closest_x = fmaxf((float)tile_x, fminf(map_x, (float)tile_x + 1.0f));
            closest_z = fmaxf((float)tile_z, fminf(map_z, (float)tile_z + 1.0f));
            distance_x = map_x - closest_x;
            distance_z = map_z - closest_z;

            if (distance_x * distance_x + distance_z * distance_z <
                radius * radius) {
                return true;
            }
        }
    }

    return false;
}

static void move_with_wall_sliding(
    WolfCamera *camera,
    const WolfLevel *level,
    float movement_x,
    float movement_z,
    float delta_seconds)
{
    const float distance = WOLF_PLAYER_MOVE_SPEED * delta_seconds;
    const float step_count_float = ceilf(
        distance / WOLF_PLAYER_MAX_STEP_DISTANCE);
    const int step_count = step_count_float > 1.0f ? (int)step_count_float : 1;
    const float step_x = movement_x * distance / (float)step_count;
    const float step_z = movement_z * distance / (float)step_count;

    for (int step = 0; step < step_count; ++step) {
        const float candidate_x = camera->position[0] + step_x;
        const float candidate_z = camera->position[2] + step_z;

        /* Resolve axes separately so a blocked direction can still slide along a wall. */
        if (!position_overlaps_wall(level, candidate_x, camera->position[2])) {
            camera->position[0] = candidate_x;
        }
        if (!position_overlaps_wall(level, camera->position[0], candidate_z)) {
            camera->position[2] = candidate_z;
        }
    }
}

bool wolf_player_update(
    WolfCamera *camera,
    const WolfInput *input,
    const WolfLevel *level,
    float delta_seconds)
{
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    float forward_axis = 0.0f;
    float strafe_axis = 0.0f;
    float movement_x;
    float movement_z;
    float movement_length;

    if (camera == NULL || input == NULL || level == NULL ||
        level->width == 0u || level->height == 0u ||
        level->width > WOLF_LEVEL_PLANE_WIDTH ||
        level->height > WOLF_LEVEL_PLANE_HEIGHT ||
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
            camera, level, movement_x, movement_z, delta_seconds);
    }

    return true;
}
