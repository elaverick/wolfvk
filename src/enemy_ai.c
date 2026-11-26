#include "wolfvk/enemy_ai.h"

#include "wolfvk/actor_animation.h"
#include "wolfvk/actor_movement.h"
#include "wolfvk/world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define WOLF_ENEMY_TWO_PI 6.28318530717958647692f
#define WOLF_ENEMY_GUARD_BASE_SPEED WOLF_ACTOR_PATROL_SPEED_WORLD_PER_TIC
#define WOLF_ENEMY_ACTOR_RADIUS 0.20f
#define WOLF_ENEMY_SHOT_RANGE_TILES 64.0f
#define WOLF_ENEMY_GUN_AIM_RADIUS_TILES 0.16f
#define WOLF_ENEMY_KNIFE_RANGE_TILES 1.5f
#define WOLF_ENEMY_KNIFE_AIM_RADIUS_TILES 0.28f
#define WOLF_ENEMY_SIGHT_CLOSE_TILES 1.5f
#define WOLF_ENEMY_MAX_TRACE_STEPS 8192u

static uint8_t actor_random_byte(WolfActor *actor)
{
    uint32_t value = actor->ai_random_state;

    if (value == 0u) {
        value = 0x9e3779b9u ^ (actor->id + 1u) * 0x85ebca6bu;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    actor->ai_random_state = value;
    return (uint8_t)(value >> 24);
}

static float map_x(const WolfWorldState *world, float world_x)
{
    return world_x / WOLF_WORLD_TILE_SIZE + (float)world->level.width * 0.5f;
}

static float map_y(const WolfWorldState *world, float world_z)
{
    return world_z / WOLF_WORLD_TILE_SIZE + (float)world->level.height * 0.5f;
}

static bool door_is_fully_open(
    const WolfWorldState *world, uint16_t tile_x, uint16_t tile_y)
{
    for (uint16_t index = 0u; index < world->door_count; ++index) {
        if (world->doors[index].tile_x == tile_x &&
            world->doors[index].tile_y == tile_y) {
            return world->doors[index].action == WOLF_DOOR_OPEN;
        }
    }
    return false;
}

/* Trace the segment through the decoded wall plane; closed/part-open doors occlude it. */
static bool has_line_of_sight(
    const WolfWorldState *world,
    float from_x,
    float from_z,
    float to_x,
    float to_z)
{
    float start_x;
    float start_y;
    float end_x;
    float end_y;
    float delta_x;
    float delta_y;
    float max_delta;
    uint32_t steps;

    if (world == NULL || !isfinite(from_x) || !isfinite(from_z) ||
        !isfinite(to_x) || !isfinite(to_z)) {
        return false;
    }
    start_x = map_x(world, from_x);
    start_y = map_y(world, from_z);
    end_x = map_x(world, to_x);
    end_y = map_y(world, to_z);
    delta_x = end_x - start_x;
    delta_y = end_y - start_y;
    max_delta = fmaxf(fabsf(delta_x), fabsf(delta_y));
    steps = (uint32_t)ceilf(max_delta * 16.0f);
    if (steps > WOLF_ENEMY_MAX_TRACE_STEPS) {
        return false;
    }

    for (uint32_t step = 1u; step < steps; ++step) {
        float fraction = (float)step / (float)steps;
        int tile_x = (int)floorf(start_x + delta_x * fraction);
        int tile_y = (int)floorf(start_y + delta_y * fraction);
        uint16_t wall_tile;

        if (tile_x < 0 || tile_y < 0 ||
            tile_x >= world->level.width || tile_y >= world->level.height) {
            return false;
        }
        wall_tile = world->level.wall_tiles[
            (size_t)tile_y * WOLF_LEVEL_PLANE_WIDTH + (size_t)tile_x];
        if (wall_tile == 0u) {
            continue;
        }
        if (wolf_level_is_door_tile(wall_tile) &&
            door_is_fully_open(world, (uint16_t)tile_x, (uint16_t)tile_y)) {
            continue;
        }
        return false;
    }
    return true;
}

static bool actor_can_see_player(
    const WolfActor *actor,
    const WolfWorldState *world,
    float player_x,
    float player_z)
{
    float delta_x = player_x - actor->position_x;
    float delta_z = player_z - actor->position_z;

    /* CheckSight's close-range shortcut is an axis-aligned 1.5-tile box. */
    if (fabsf(delta_x) < WOLF_ENEMY_SIGHT_CLOSE_TILES * WOLF_WORLD_TILE_SIZE &&
        fabsf(delta_z) < WOLF_ENEMY_SIGHT_CLOSE_TILES * WOLF_WORLD_TILE_SIZE) {
        return true;
    }

    /* The original only applies a facing-hemisphere test to the four cardinal directions. */
    switch (actor->movement_direction) {
    case WOLF_ACTOR_DIRECTION_NORTH:
        if (delta_z > 0.0f) return false;
        break;
    case WOLF_ACTOR_DIRECTION_EAST:
        if (delta_x < 0.0f) return false;
        break;
    case WOLF_ACTOR_DIRECTION_SOUTH:
        if (delta_z < 0.0f) return false;
        break;
    case WOLF_ACTOR_DIRECTION_WEST:
        if (delta_x > 0.0f) return false;
        break;
    default:
        break;
    }
    return has_line_of_sight(
        world, actor->position_x, actor->position_z, player_x, player_z);
}

static int chebyshev_tile_distance(
    const WolfActor *actor,
    const WolfWorldState *world,
    float player_x,
    float player_z)
{
    int player_tile_x = (int)floorf(map_x(world, player_x));
    int player_tile_y = (int)floorf(map_y(world, player_z));
    int dx = abs((int)actor->tile_x - player_tile_x);
    int dy = abs((int)actor->tile_y - player_tile_y);

    return dx > dy ? dx : dy;
}

static void face_player(
    WolfActor *actor,
    float player_x,
    float player_z)
{
    float dx = player_x - actor->position_x;
    float dz = player_z - actor->position_z;

    if (fabsf(dx) + fabsf(dz) > 0.0001f) {
        actor->yaw_radians = atan2f(dx, -dz);
    }
}

static void first_sighting(
    WolfActor *actor,
    float player_x,
    float player_z,
    WolfEnemyAIEvents *events)
{
    actor->attack_mode = true;
    actor->movement_follow_path_arrows = false;
    actor->movement_speed_world_per_tic = WOLF_ENEMY_GUARD_BASE_SPEED * 3.0f;
    actor->ai_reaction_tics = 0u;
    actor->ai_attack_cooldown = 8u;
    actor->movement_in_progress = false;
    actor->movement_waiting_for_door = false;
    face_player(actor, player_x, player_z);
    (void)wolf_actor_movement_set_direction(actor, WOLF_ACTOR_DIRECTION_NONE);
    actor->movement_speed_world_per_tic = WOLF_ENEMY_GUARD_BASE_SPEED * 3.0f;
    (void)wolf_actor_collection_set_state(actor, WOLF_GUARD_STATE_CHASE_FIRST);

    if (events != NULL) {
        ++events->guard_alerts;
        events->alert_source_x = actor->position_x;
        events->alert_source_z = actor->position_z;
    }
}

static bool tile_is_available_for_chase(
    const WolfActor *actors,
    uint32_t actor_count,
    uint32_t self_index,
    const WolfWorldState *world,
    int tile_x,
    int tile_y,
    float player_x,
    float player_z)
{
    float centre_x;
    float centre_z;

    if (tile_x < 0 || tile_y < 0 ||
        tile_x >= world->level.width || tile_y >= world->level.height) {
        return false;
    }
    centre_x = ((float)tile_x + 0.5f - (float)world->level.width * 0.5f) *
        WOLF_WORLD_TILE_SIZE;
    centre_z = ((float)tile_y + 0.5f - (float)world->level.height * 0.5f) *
        WOLF_WORLD_TILE_SIZE;
    if (wolf_world_state_position_overlaps_solid(
            world, centre_x, centre_z, WOLF_ENEMY_ACTOR_RADIUS)) {
        return false;
    }
    if (fabsf(centre_x - player_x) < WOLF_WORLD_TILE_SIZE * 0.55f &&
        fabsf(centre_z - player_z) < WOLF_WORLD_TILE_SIZE * 0.55f) {
        return false;
    }
    for (uint32_t index = 0u; index < actor_count; ++index) {
        if (index != self_index && actors[index].active && actors[index].shootable &&
            actors[index].tile_x == (uint16_t)tile_x &&
            actors[index].tile_y == (uint16_t)tile_y) {
            return false;
        }
    }
    return true;
}

/* The original chase selector is greedy and direction-prioritised, not a global pathfinder. */
static WolfActorDirection select_chase_direction(
    const WolfActor *actor,
    const WolfActor *actors,
    uint32_t actor_count,
    uint32_t self_index,
    const WolfWorldState *world,
    float player_x,
    float player_z)
{
    static const WolfActorDirection directions[8] = {
        WOLF_ACTOR_DIRECTION_EAST, WOLF_ACTOR_DIRECTION_NORTH_EAST,
        WOLF_ACTOR_DIRECTION_NORTH, WOLF_ACTOR_DIRECTION_NORTH_WEST,
        WOLF_ACTOR_DIRECTION_WEST, WOLF_ACTOR_DIRECTION_SOUTH_WEST,
        WOLF_ACTOR_DIRECTION_SOUTH, WOLF_ACTOR_DIRECTION_SOUTH_EAST
    };
    static const int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    static const int dy[8] = {0, -1, -1, -1, 0, 1, 1, 1};
    int target_x = (int)floorf(map_x(world, player_x));
    int target_y = (int)floorf(map_y(world, player_z));
    int best_score = INT32_MAX;
    WolfActorDirection best = WOLF_ACTOR_DIRECTION_NONE;

    for (size_t index = 0u; index < 8u; ++index) {
        int candidate_x = (int)actor->tile_x + dx[index];
        int candidate_y = (int)actor->tile_y + dy[index];
        int score;

        if (!tile_is_available_for_chase(
                actors, actor_count, self_index, world,
                candidate_x, candidate_y, player_x, player_z)) {
            continue;
        }
        if (dx[index] != 0 && dy[index] != 0 &&
            (!tile_is_available_for_chase(
                 actors, actor_count, self_index, world,
                 (int)actor->tile_x + dx[index], (int)actor->tile_y, player_x, player_z) ||
             !tile_is_available_for_chase(
                 actors, actor_count, self_index, world,
                 (int)actor->tile_x, (int)actor->tile_y + dy[index], player_x, player_z))) {
            continue;
        }
        score = abs(target_x - candidate_x) + abs(target_y - candidate_y);
        if (score < best_score) {
            best_score = score;
            best = directions[index];
        }
    }
    return best;
}

static int32_t guard_attack_damage(
    WolfActor *actor,
    int distance,
    bool player_is_moving,
    bool player_can_see_guard)
{
    int hit_chance = 128;
    int damage;

    if (player_is_moving) {
        hit_chance = 160 - distance * (player_can_see_guard ? 16 : 8);
    } else {
        hit_chance = 256 - distance * (player_can_see_guard ? 16 : 8);
    }
    if (hit_chance < 0) hit_chance = 0;
    if (hit_chance > 255) hit_chance = 255;
    if ((int)actor_random_byte(actor) >= hit_chance) {
        return 0;
    }

    if (distance < 2) {
        damage = (int)(actor_random_byte(actor) >> 2);
    } else if (distance < 4) {
        damage = (int)(actor_random_byte(actor) >> 3);
    } else {
        damage = (int)(actor_random_byte(actor) >> 4);
    }
    return damage;
}

static void guard_shoot_player(
    WolfActor *actor,
    WolfWorldState *world,
    float player_x,
    float player_z,
    float player_yaw,
    WolfDifficulty difficulty,
    bool player_is_moving,
    WolfEnemyAIEvents *events)
{
    float dx = actor->position_x - player_x;
    float dz = actor->position_z - player_z;
    float angle_to_guard = atan2f(dx, -dz);
    float angle_error = remainderf(angle_to_guard - player_yaw, WOLF_ENEMY_TWO_PI);
    bool player_can_see_guard = fabsf(angle_error) < 0.65f;
    int distance = chebyshev_tile_distance(actor, world, player_x, player_z);
    int32_t damage;

    if (!has_line_of_sight(world, actor->position_x, actor->position_z, player_x, player_z)) {
        return;
    }
    damage = guard_attack_damage(actor, distance, player_is_moving, player_can_see_guard);
    if (difficulty == WOLF_DIFFICULTY_BABY) {
        damage >>= 2;
    }
    if (damage > 0) {
        (void)wolf_player_state_take_damage(&world->player, damage);
    }
    if (events != NULL) {
        ++events->guard_shots;
        events->shot_source_x = actor->position_x;
        events->shot_source_z = actor->position_z;
    }
}

static bool is_guard_state_shooting(uint16_t state_id)
{
    return state_id >= WOLF_GUARD_STATE_SHOOT_FIRST &&
        state_id <= WOLF_GUARD_STATE_SHOOT_LAST;
}

static bool is_guard_state_pain_or_dead(uint16_t state_id)
{
    return state_id >= WOLF_GUARD_STATE_PAIN_FIRST;
}

static void choose_guard_chase_step(
    WolfActor *actor,
    const WolfActor *actors,
    uint32_t actor_count,
    uint32_t actor_index,
    const WolfWorldState *world,
    float player_x,
    float player_z)
{
    WolfActorDirection direction;

    if (actor->movement_in_progress || actor->movement_waiting_for_door ||
        actor->state_id < WOLF_GUARD_STATE_CHASE_FIRST ||
        actor->state_id > WOLF_GUARD_STATE_CHASE_LAST) {
        return;
    }
    direction = select_chase_direction(
        actor, actors, actor_count, actor_index, world, player_x, player_z);
    (void)wolf_actor_movement_set_direction(actor, direction);
    actor->movement_follow_path_arrows = false;
    actor->movement_speed_world_per_tic = WOLF_ENEMY_GUARD_BASE_SPEED * 3.0f;
}

static void drop_guard_clip(WolfActor *actor, WolfWorldState *world)
{
    int centre_x = (int)actor->tile_x;
    int centre_y = (int)actor->tile_y;

    /* DropItem prefers the actor's current tile, then searches the surrounding 3x3 cells. */
    for (int radius = 0; radius <= 1; ++radius) {
        for (int y = centre_y - radius; y <= centre_y + radius; ++y) {
            for (int x = centre_x - radius; x <= centre_x + radius; ++x) {
                size_t index;
                uint16_t marker;

                if (x < 0 || y < 0 || x >= world->level.width || y >= world->level.height) {
                    continue;
                }
                if (radius == 1 && x != centre_x - 1 && x != centre_x + 1 &&
                    y != centre_y - 1 && y != centre_y + 1) {
                    continue;
                }
                index = (size_t)y * WOLF_LEVEL_PLANE_WIDTH + (size_t)x;
                marker = world->level.object_tiles[index];
                if (world->level.wall_tiles[index] != 0u ||
                    (marker >= WOLF_LEVEL_STATIC_FIRST &&
                     marker <= WOLF_LEVEL_STATIC_EXTRA_CLIP)) {
                    continue;
                }
                world->level.object_tiles[index] = WOLF_LEVEL_STATIC_EXTRA_CLIP;
                return;
            }
        }
    }
}

static bool damage_guard(
    WolfActor *actor,
    WolfWorldState *world,
    int32_t damage)
{
    if (actor == NULL || world == NULL || !actor->active || !actor->shootable || damage <= 0) {
        return false;
    }

    if (!actor->attack_mode) {
        damage *= 2;
    }
    actor->health -= damage;
    if (actor->health <= 0) {
        actor->health = 0;
        actor->shootable = false;
        actor->movement_enabled = false;
        actor->movement_direction = WOLF_ACTOR_DIRECTION_NONE;
        actor->movement_in_progress = false;
        actor->movement_waiting_for_door = false;
        if (!actor->ai_score_awarded) {
            wolf_player_state_give_points(&world->player, 100);
            actor->ai_score_awarded = true;
        }
        (void)wolf_actor_collection_set_state(actor, WOLF_GUARD_STATE_DIE_FIRST);
        drop_guard_clip(actor, world);
        return true;
    }

    if (!actor->attack_mode) {
        actor->attack_mode = true;
        actor->movement_follow_path_arrows = false;
        actor->movement_speed_world_per_tic = WOLF_ENEMY_GUARD_BASE_SPEED * 3.0f;
    }
    actor->movement_enabled = false;
    actor->movement_direction = WOLF_ACTOR_DIRECTION_NONE;
    actor->movement_in_progress = false;
    (void)wolf_actor_collection_set_state(
        actor, (actor->health & 1) != 0
            ? WOLF_GUARD_STATE_PAIN_FIRST
            : WOLF_GUARD_STATE_PAIN_LAST);
    return true;
}

bool wolf_enemy_ai_player_attack(
    WolfActor *actors,
    uint32_t actor_count,
    WolfWorldState *world,
    float player_world_x,
    float player_world_z,
    float player_yaw_radians,
    WolfWeaponType weapon)
{
    float forward_x;
    float forward_z;
    float best_distance = INFINITY;
    WolfActor *target = NULL;
    float max_range;
    float aim_radius;

    if ((actor_count > 0u && actors == NULL) || world == NULL ||
        !isfinite(player_world_x) || !isfinite(player_world_z) ||
        !isfinite(player_yaw_radians) ||
        weapon < WOLF_WEAPON_KNIFE || weapon >= WOLF_WEAPON_COUNT) {
        return false;
    }

    forward_x = sinf(player_yaw_radians);
    forward_z = -cosf(player_yaw_radians);
    max_range = weapon == WOLF_WEAPON_KNIFE
        ? WOLF_ENEMY_KNIFE_RANGE_TILES * WOLF_WORLD_TILE_SIZE
        : WOLF_ENEMY_SHOT_RANGE_TILES * WOLF_WORLD_TILE_SIZE;
    aim_radius = (weapon == WOLF_WEAPON_KNIFE
        ? WOLF_ENEMY_KNIFE_AIM_RADIUS_TILES
        : WOLF_ENEMY_GUN_AIM_RADIUS_TILES) * WOLF_WORLD_TILE_SIZE;

    for (uint32_t index = 0u; index < actor_count; ++index) {
        WolfActor *actor = &actors[index];
        float relative_x;
        float relative_z;
        float along_ray;
        float perpendicular;

        if (!actor->active || !actor->shootable || actor->family != WOLF_ACTOR_GUARD) {
            continue;
        }
        relative_x = actor->position_x - player_world_x;
        relative_z = actor->position_z - player_world_z;
        along_ray = relative_x * forward_x + relative_z * forward_z;
        perpendicular = fabsf(relative_x * forward_z - relative_z * forward_x);
        if (along_ray <= 0.0f || along_ray > max_range ||
            perpendicular > aim_radius || along_ray >= best_distance) {
            continue;
        }
        if (!has_line_of_sight(
                world, player_world_x, player_world_z,
                actor->position_x, actor->position_z)) {
            continue;
        }
        best_distance = along_ray;
        target = actor;
    }

    if (target == NULL) {
        return true;
    }

    {
        int distance = chebyshev_tile_distance(
            target, world, player_world_x, player_world_z);
        int32_t damage;

        if (weapon == WOLF_WEAPON_KNIFE) {
            damage = (int32_t)(actor_random_byte(target) >> 4);
        } else if (distance < 2) {
            damage = (int32_t)(actor_random_byte(target) / 4u);
        } else if (distance < 4) {
            damage = (int32_t)(actor_random_byte(target) / 6u);
        } else {
            if ((int)(actor_random_byte(target) / 12u) < distance) {
                return true;
            }
            damage = (int32_t)(actor_random_byte(target) / 6u);
        }
        if (damage > 0) {
            (void)damage_guard(target, world, damage);
        }
    }
    return true;
}

bool wolf_enemy_ai_update_tic(
    WolfActor *actors,
    uint32_t actor_count,
    WolfWorldState *world,
    float player_world_x,
    float player_world_z,
    float player_yaw_radians,
    WolfDifficulty difficulty,
    bool player_is_moving,
    bool player_made_noise,
    WolfEnemyAIEvents *events)
{
    if (events != NULL) {
        memset(events, 0, sizeof(*events));
    }
    if ((actor_count > 0u && actors == NULL) || world == NULL ||
        !isfinite(player_world_x) || !isfinite(player_world_z) ||
        !isfinite(player_yaw_radians) ||
        difficulty < WOLF_DIFFICULTY_BABY || difficulty > WOLF_DIFFICULTY_HARD) {
        return false;
    }

    for (uint32_t index = 0u; index < actor_count; ++index) {
        WolfActor *actor = &actors[index];

        if (!actor->active || !actor->shootable || actor->family != WOLF_ACTOR_GUARD) {
            continue;
        }
        if (!actor->attack_mode) {
            if (actor->ai_reaction_tics > 0u) {
                --actor->ai_reaction_tics;
                if (actor->ai_reaction_tics == 0u) {
                    /* A reaction timer expires only after the original random delay. */
                    first_sighting(actor, player_world_x, player_world_z, events);
                }
            } else if ((player_made_noise &&
                has_line_of_sight(world, actor->position_x, actor->position_z,
                    player_world_x, player_world_z)) ||
                actor_can_see_player(actor, world, player_world_x, player_world_z)) {
                actor->ai_reaction_tics =
                    (uint16_t)(1u + actor_random_byte(actor) / 4u);
            }
        } else {
            if (is_guard_state_shooting(actor->state_id)) {
                if (actor->state_id == WOLF_GUARD_STATE_SHOOT_ACTION &&
                    !actor->ai_action_fired) {
                    actor->ai_action_fired = true;
                    guard_shoot_player(
                        actor, world, player_world_x, player_world_z,
                        player_yaw_radians, difficulty, player_is_moving, events);
                }
            } else if (!is_guard_state_pain_or_dead(actor->state_id)) {
                if (actor->ai_attack_cooldown > 0u) {
                    --actor->ai_attack_cooldown;
                } else {
                    actor->ai_attack_cooldown = 8u;
                    if (has_line_of_sight(
                            world, actor->position_x, actor->position_z,
                            player_world_x, player_world_z)) {
                        int distance = chebyshev_tile_distance(
                            actor, world, player_world_x, player_world_z);
                        int chance = distance <= 1 ? 300 : 16 / distance;
                        if ((int)actor_random_byte(actor) < chance) {
                            face_player(actor, player_world_x, player_world_z);
                            (void)wolf_actor_movement_set_direction(
                                actor, WOLF_ACTOR_DIRECTION_NONE);
                            (void)wolf_actor_collection_set_state(
                                actor, WOLF_GUARD_STATE_SHOOT_FIRST);
                            continue;
                        }
                    }
                }
                choose_guard_chase_step(
                    actor, actors, actor_count, index, world,
                    player_world_x, player_world_z);
            }
        }
    }
    return true;
}
