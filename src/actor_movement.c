#include "wolfvk/actor_movement.h"

#include "wolfvk/level_objects.h"
#include "wolfvk/world.h"

#include <math.h>

#define WOLF_ACTOR_TWO_PI 6.28318530717958647692f

static const int direction_x[] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int direction_y[] = {0, -1, -1, -1, 0, 1, 1, 1};

static float map_origin_x(const WolfWorldState *world)
{
    return -((float)world->level.width * WOLF_WORLD_TILE_SIZE) * 0.5f;
}

static float map_origin_z(const WolfWorldState *world)
{
    return -((float)world->level.height * WOLF_WORLD_TILE_SIZE) * 0.5f;
}

static void tile_center(
    const WolfWorldState *world, uint16_t x, uint16_t y, float *world_x, float *world_z)
{
    *world_x = map_origin_x(world) + ((float)x + 0.5f) * WOLF_WORLD_TILE_SIZE;
    *world_z = map_origin_z(world) + ((float)y + 0.5f) * WOLF_WORLD_TILE_SIZE;
}

static float yaw_for_direction(WolfActorDirection direction)
{
    static const float yaws[8] = {
        1.57079632679f, 0.78539816339f, 0.0f, -0.78539816339f,
        -1.57079632679f, -2.35619449019f, 3.14159265359f, 2.35619449019f
    };
    return yaws[(unsigned int)direction];
}

bool wolf_actor_movement_set_direction(
    WolfActor *actor, WolfActorDirection direction)
{
    if (actor == NULL || direction < WOLF_ACTOR_DIRECTION_EAST ||
        direction > WOLF_ACTOR_DIRECTION_NONE) {
        return false;
    }
    actor->movement_direction = direction;
    actor->movement_enabled = direction != WOLF_ACTOR_DIRECTION_NONE;
    if (actor->movement_speed_world_per_tic <= 0.0f) {
        actor->movement_speed_world_per_tic = WOLF_ACTOR_PATROL_SPEED_WORLD_PER_TIC;
    }
    actor->movement_waiting_for_door = false;
    actor->movement_in_progress = false;
    return true;
}

static bool cell_is_door(
    const WolfWorldState *world, uint16_t tile_x, uint16_t tile_y)
{
    return wolf_level_is_door_tile(
        world->level.wall_tiles[(size_t)tile_y * WOLF_LEVEL_PLANE_WIDTH + tile_x]);
}

/* A centre-point query with the actor's footprint covers walls, doors, pushwalls and blocking statics. */
static bool cell_is_blocked(
    const WolfWorldState *world, uint16_t tile_x, uint16_t tile_y)
{
    float x;
    float z;

    if (tile_x >= world->level.width || tile_y >= world->level.height) {
        return true;
    }
    tile_center(world, tile_x, tile_y, &x, &z);
    return wolf_world_state_position_overlaps_solid(
        world, x, z, WOLF_ACTOR_COLLISION_RADIUS_TILES);
}

static bool actor_reserves_tile(
    const WolfActor *actors,
    uint32_t actor_count,
    uint32_t moving_index,
    uint16_t tile_x,
    uint16_t tile_y)
{
    for (uint32_t index = 0u; index < actor_count; ++index) {
        if (index != moving_index && actors[index].active && actors[index].shootable &&
            actors[index].tile_x == tile_x && actors[index].tile_y == tile_y) {
            return true;
        }
    }
    return false;
}

static bool actor_position_is_clear(
    const WolfActor *actors,
    uint32_t actor_count,
    uint32_t moving_index,
    const WolfWorldState *world,
    float x,
    float z,
    float player_x,
    float player_z)
{
    const float minimum_actor_distance = WOLF_WORLD_TILE_SIZE;
    const float actor_clearance = WOLF_ACTOR_COLLISION_RADIUS_TILES * 2.0f *
        WOLF_WORLD_TILE_SIZE;
    float dx = x - player_x;
    float dz = z - player_z;

    if (wolf_world_state_position_overlaps_solid(
            world, x, z, WOLF_ACTOR_COLLISION_RADIUS_TILES) ||
        (fabsf(dx) <= minimum_actor_distance && fabsf(dz) <= minimum_actor_distance)) {
        return false;
    }

    for (uint32_t index = 0u; index < actor_count; ++index) {
        float actor_dx;
        float actor_dz;

        if (index == moving_index || !actors[index].active || !actors[index].shootable) {
            continue;
        }
        actor_dx = x - actors[index].position_x;
        actor_dz = z - actors[index].position_z;
        if (actor_dx * actor_dx + actor_dz * actor_dz <
            actor_clearance * actor_clearance) {
            return false;
        }
    }
    return true;
}

/* Diagonal steps must have both side cells clear, matching TryWalk's corner checks. */
static bool can_reserve_destination(
    WolfActor *actor,
    uint32_t actor_index,
    WolfActor *actors,
    uint32_t actor_count,
    WolfWorldState *world,
    uint16_t target_x,
    uint16_t target_y)
{
    int dx = direction_x[actor->movement_direction];
    int dy = direction_y[actor->movement_direction];

    if (target_x >= world->level.width || target_y >= world->level.height ||
        actor_reserves_tile(actors, actor_count, actor_index, target_x, target_y)) {
        return false;
    }

    if (dx != 0 && dy != 0) {
        int side_x = (int)actor->tile_x + dx;
        int side_y = (int)actor->tile_y + dy;

        if (side_x < 0 || side_y < 0 ||
            cell_is_blocked(world, (uint16_t)side_x, actor->tile_y) ||
            cell_is_blocked(world, actor->tile_x, (uint16_t)side_y)) {
            return false;
        }
        /* The original diagonal checks do not pass through door cells. */
        if (cell_is_door(world, (uint16_t)side_x, actor->tile_y) ||
            cell_is_door(world, actor->tile_x, (uint16_t)side_y)) {
            return false;
        }
    }

    if (cell_is_blocked(world, target_x, target_y)) {
        if (cell_is_door(world, target_x, target_y) &&
            wolf_world_state_actor_request_door_open(world, target_x, target_y)) {
            actor->movement_waiting_for_door = true;
            actor->movement_door_tile_x = target_x;
            actor->movement_door_tile_y = target_y;
            actor->tile_x = target_x;
            actor->tile_y = target_y;
            actor->movement_in_progress = true;
            return true;
        }
        return false;
    }

    actor->tile_x = target_x;
    actor->tile_y = target_y;
    actor->movement_in_progress = true;
    return true;
}

static bool begin_next_tile(
    WolfActor *actor,
    uint32_t actor_index,
    WolfActor *actors,
    uint32_t actor_count,
    WolfWorldState *world)
{
    uint16_t path_marker = world->level.object_tiles[
        (size_t)actor->tile_y * WOLF_LEVEL_PLANE_WIDTH + actor->tile_x];
    int target_x;
    int target_y;

    /* WL_STATE.C SelectPathDir reads the eight ICONARROWS from the current tile. */
    if (actor->movement_follow_path_arrows && path_marker >= 90u && path_marker <= 97u) {
        actor->movement_direction = (WolfActorDirection)(path_marker - 90u);
    }
    target_x = (int)actor->tile_x + direction_x[actor->movement_direction];
    target_y = (int)actor->tile_y + direction_y[actor->movement_direction];

    if (actor->movement_direction == WOLF_ACTOR_DIRECTION_NONE) {
        return true;
    }
    if (target_x < 0 || target_y < 0 ||
        target_x >= world->level.width || target_y >= world->level.height) {
        actor->movement_enabled = false;
        return true;
    }
    return can_reserve_destination(
        actor, actor_index, actors, actor_count, world,
        (uint16_t)target_x, (uint16_t)target_y);
}

static bool advance_actor(
    WolfActor *actor,
    uint32_t actor_index,
    WolfActor *actors,
    uint32_t actor_count,
    WolfWorldState *world,
    float player_x,
    float player_z)
{
    float target_x;
    float target_z;
    float next_x;
    float next_z;
    float delta_x;
    float delta_z;
    float speed = actor->movement_speed_world_per_tic > 0.0f
        ? actor->movement_speed_world_per_tic
        : WOLF_ACTOR_PATROL_SPEED_WORLD_PER_TIC;

    if (actor->tile_x >= world->level.width || actor->tile_y >= world->level.height ||
        actor->movement_direction < WOLF_ACTOR_DIRECTION_EAST ||
        actor->movement_direction > WOLF_ACTOR_DIRECTION_NONE ||
        !isfinite(actor->movement_speed_world_per_tic)) {
        return false;
    }
    if (!actor->movement_enabled || actor->movement_direction == WOLF_ACTOR_DIRECTION_NONE) {
        return true;
    }

    if (actor->movement_waiting_for_door) {
        if (!wolf_world_state_actor_door_is_open(
                world, actor->movement_door_tile_x, actor->movement_door_tile_y)) {
            (void)wolf_world_state_actor_request_door_open(
                world, actor->movement_door_tile_x, actor->movement_door_tile_y);
            return true;
        }
        actor->movement_waiting_for_door = false;
    }

    /* The actor reserves the next tile before moving, as the original actorat grid did. */
    if (!actor->movement_in_progress) {
        if (!begin_next_tile(actor, actor_index, actors, actor_count, world)) {
            /* TryWalk leaves a patrol actor directionless when its next tile is blocked. */
            actor->movement_enabled = false;
            actor->movement_direction = WOLF_ACTOR_DIRECTION_NONE;
            return true;
        }
        if (actor->movement_waiting_for_door) {
            return true;
        }
        actor->movement_in_progress = true;
    }

    tile_center(world, actor->tile_x, actor->tile_y, &target_x, &target_z);
    delta_x = target_x - actor->position_x;
    delta_z = target_z - actor->position_z;
    if (fabsf(delta_x) < 0.0001f && fabsf(delta_z) < 0.0001f) {
        actor->position_x = target_x;
        actor->position_z = target_z;
        actor->movement_in_progress = false;
        return true;
    }

    next_x = actor->position_x;
    next_z = actor->position_z;
    if (delta_x > 0.0f) {
        next_x += fminf(speed, delta_x);
    } else if (delta_x < 0.0f) {
        next_x -= fminf(speed, -delta_x);
    }
    if (delta_z > 0.0f) {
        next_z += fminf(speed, delta_z);
    } else if (delta_z < 0.0f) {
        next_z -= fminf(speed, -delta_z);
    }

    if (!actor_position_is_clear(
            actors, actor_count, actor_index, world,
            next_x, next_z, player_x, player_z)) {
        /* Do not push through the player or another actor; retry next tic. */
        return true;
    }

    actor->position_x = next_x;
    actor->position_z = next_z;
    if (fabsf(target_x - next_x) < 0.0001f &&
        fabsf(target_z - next_z) < 0.0001f) {
        actor->movement_in_progress = false;
    }
    actor->yaw_radians = yaw_for_direction(actor->movement_direction);
    return true;
}

bool wolf_actor_movement_update_tic(
    WolfActor *actors,
    uint32_t actor_count,
    WolfWorldState *world,
    float player_world_x,
    float player_world_z)
{
    if ((actor_count > 0u && actors == NULL) || world == NULL ||
        !isfinite(player_world_x) || !isfinite(player_world_z)) {
        return false;
    }

    for (uint32_t index = 0u; index < actor_count; ++index) {
        if (actors[index].active &&
            !advance_actor(
                &actors[index], index, actors, actor_count, world,
                player_world_x, player_world_z)) {
            return false;
        }
    }
    return true;
}
