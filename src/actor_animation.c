#include "wolfvk/actor_animation.h"
#include "wolfvk/actor_movement.h"

#include "wolfvk/world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define ACTOR_BASE_STATES(prefix) \
    {prefix "_s", 0u, 0u, WOLF_ACTOR_STATE_NONE, NULL, NULL}, \
    {prefix "_w", 0u, 20u, 2u, NULL, NULL}, \
    {prefix "_w", 0u, 5u, 3u, NULL, NULL}, \
    {prefix "_w", 1u, 15u, 4u, NULL, NULL}, \
    {prefix "_w", 2u, 20u, 5u, NULL, NULL}, \
    {prefix "_w", 2u, 5u, 6u, NULL, NULL}, \
    {prefix "_w", 3u, 15u, 1u, NULL, NULL}, \
    {prefix "_w", 0u, 10u, 8u, NULL, NULL}, \
    {prefix "_w", 0u, 3u, 9u, NULL, NULL}, \
    {prefix "_w", 1u, 8u, 10u, NULL, NULL}, \
    {prefix "_w", 2u, 10u, 11u, NULL, NULL}, \
    {prefix "_w", 2u, 3u, 12u, NULL, NULL}, \
    {prefix "_w", 3u, 8u, 7u, NULL, NULL},

#define GUARD_STATES(prefix) { \
    ACTOR_BASE_STATES(prefix) \
    {prefix "_shoot", 0u, 20u, 14u, NULL, NULL}, \
    {prefix "_shoot", 1u, 20u, 15u, NULL, NULL}, \
    {prefix "_shoot", 2u, 20u, 7u, NULL, NULL}, \
    {prefix "_pain", 0u, 10u, 7u, NULL, NULL}, \
    {prefix "_pain", 1u, 10u, 7u, NULL, NULL}, \
    {prefix "_die", 0u, 15u, 19u, NULL, NULL}, \
    {prefix "_die", 1u, 15u, 20u, NULL, NULL}, \
    {prefix "_die", 2u, 15u, 21u, NULL, NULL}, \
    {prefix "_dead", 0u, 0u, WOLF_ACTOR_STATE_NONE, NULL, NULL} \
}

#define OFFICER_STATES(prefix) { \
    ACTOR_BASE_STATES(prefix) \
    {prefix "_shoot", 0u, 6u, 14u, NULL, NULL}, \
    {prefix "_shoot", 1u, 20u, 15u, NULL, NULL}, \
    {prefix "_shoot", 2u, 10u, 7u, NULL, NULL}, \
    {prefix "_pain", 0u, 10u, 7u, NULL, NULL}, \
    {prefix "_pain", 1u, 10u, 7u, NULL, NULL}, \
    {prefix "_die", 0u, 11u, 19u, NULL, NULL}, \
    {prefix "_die", 1u, 11u, 20u, NULL, NULL}, \
    {prefix "_die", 2u, 11u, 21u, NULL, NULL}, \
    {prefix "_die", 3u, 11u, 22u, NULL, NULL}, \
    {prefix "_dead", 0u, 0u, WOLF_ACTOR_STATE_NONE, NULL, NULL} \
}

#define SS_STATES(prefix) { \
    ACTOR_BASE_STATES(prefix) \
    {prefix "_shoot", 0u, 20u, 14u, NULL, NULL}, \
    {prefix "_shoot", 1u, 20u, 15u, NULL, NULL}, \
    {prefix "_shoot", 2u, 10u, 16u, NULL, NULL}, \
    {prefix "_shoot", 1u, 10u, 17u, NULL, NULL}, \
    {prefix "_shoot", 2u, 10u, 18u, NULL, NULL}, \
    {prefix "_shoot", 1u, 10u, 19u, NULL, NULL}, \
    {prefix "_shoot", 2u, 10u, 20u, NULL, NULL}, \
    {prefix "_shoot", 1u, 10u, 21u, NULL, NULL}, \
    {prefix "_shoot", 2u, 10u, 7u, NULL, NULL}, \
    {prefix "_pain", 0u, 10u, 7u, NULL, NULL}, \
    {prefix "_pain", 1u, 10u, 7u, NULL, NULL}, \
    {prefix "_die", 0u, 15u, 25u, NULL, NULL}, \
    {prefix "_die", 1u, 15u, 26u, NULL, NULL}, \
    {prefix "_die", 2u, 15u, 27u, NULL, NULL}, \
    {prefix "_dead", 0u, 0u, WOLF_ACTOR_STATE_NONE, NULL, NULL} \
}


#define MUTANT_STATES(prefix) { \
    ACTOR_BASE_STATES(prefix) \
    {prefix "_shoot", 0u, 6u, 14u, NULL, NULL}, \
    {prefix "_shoot", 1u, 20u, 15u, NULL, NULL}, \
    {prefix "_shoot", 2u, 10u, 16u, NULL, NULL}, \
    {prefix "_shoot", 3u, 20u, 7u, NULL, NULL}, \
    {prefix "_pain", 0u, 10u, 7u, NULL, NULL}, \
    {prefix "_pain", 1u, 10u, 7u, NULL, NULL}, \
    {prefix "_die", 0u, 7u, 20u, NULL, NULL}, \
    {prefix "_die", 1u, 7u, 21u, NULL, NULL}, \
    {prefix "_die", 2u, 7u, 22u, NULL, NULL}, \
    {prefix "_die", 3u, 7u, 23u, NULL, NULL}, \
    {prefix "_dead", 0u, 0u, WOLF_ACTOR_STATE_NONE, NULL, NULL} \
}

typedef struct ActorFamilyAnimation {
    const char *subject;
    WolfActorFamily family;
    int32_t health[4];
    uint16_t state_count;
    WolfActorStateDefinition states[WOLF_ACTOR_STATE_MAX_COUNT];
} ActorFamilyAnimation;

/* State chains retain each enemy's original timing and animation sequence. */
static const ActorFamilyAnimation actor_families[] = {
    {"grd", WOLF_ACTOR_GUARD, {25, 25, 25, 25}, WOLF_GUARD_STATE_COUNT, GUARD_STATES("grd")},
    {"ofc", WOLF_ACTOR_OFFICER, {50, 50, 50, 50}, WOLF_OFFICER_STATE_COUNT, OFFICER_STATES("ofc")},
    {"ss", WOLF_ACTOR_SS, {100, 100, 100, 100}, WOLF_SS_STATE_COUNT, SS_STATES("ss")},
    {"mut", WOLF_ACTOR_MUTANT, {45, 55, 55, 65}, WOLF_MUTANT_STATE_COUNT, MUTANT_STATES("mut")}
};

#define WOLF_ACTOR_TWO_PI 6.28318530717958647692f

static WolfActorDirection direction_from_yaw(float yaw)
{
    static const float direction_yaws[8] = {
        1.57079632679f, 0.78539816339f, 0.0f, -0.78539816339f,
        -1.57079632679f, -2.35619449019f, 3.14159265359f, 2.35619449019f
    };
    WolfActorDirection best = WOLF_ACTOR_DIRECTION_EAST;
    float smallest_error = WOLF_ACTOR_TWO_PI;

    for (unsigned int direction = 0u; direction < 8u; ++direction) {
        float error = fabsf(remainderf(yaw - direction_yaws[direction], WOLF_ACTOR_TWO_PI));
        if (error < smallest_error) {
            smallest_error = error;
            best = (WolfActorDirection)direction;
        }
    }
    return best;
}

static const ActorFamilyAnimation *find_family(const char *subject)
{
    for (size_t index = 0u; index < sizeof(actor_families) / sizeof(actor_families[0]); ++index) {
        if (strcmp(subject, actor_families[index].subject) == 0) {
            return &actor_families[index];
        }
    }
    return NULL;
}

static const ActorFamilyAnimation *find_family_by_type(WolfActorFamily family_type)
{
    for (size_t index = 0u; index < sizeof(actor_families) / sizeof(actor_families[0]); ++index) {
        if (actor_families[index].family == family_type) {
            return &actor_families[index];
        }
    }
    return NULL;
}

static bool object_is_patrolling(const WolfLevelObject *object)
{
    size_t length = strlen(object->animation);
    return length >= 2u && object->animation[length - 2u] == '_' &&
        object->animation[length - 1u] == 'w';
}

static int32_t starting_health(
    const ActorFamilyAnimation *family,
    WolfDifficulty difficulty)
{
    return family->health[(size_t)difficulty];
}

bool wolf_actor_collection_create(
    const WolfLevel *level,
    WolfDifficulty difficulty,
    WolfActor **actors,
    uint32_t *actor_count)
{
    WolfLevelObject *objects = NULL;
    WolfActor *created = NULL;
    uint32_t object_count = 0u;
    uint32_t created_count = 0u;
    float level_origin_x;
    float level_origin_z;

    if (actors != NULL) {
        *actors = NULL;
    }
    if (actor_count != NULL) {
        *actor_count = 0u;
    }
    if (level == NULL || actors == NULL || actor_count == NULL ||
        (int)difficulty < (int)WOLF_DIFFICULTY_BABY ||
        (int)difficulty > (int)WOLF_DIFFICULTY_HARD ||
        !wolf_level_find_objects(level, difficulty, &objects, &object_count)) {
        return false;
    }

    created = object_count > 0u ? calloc(object_count, sizeof(*created)) : NULL;
    if (object_count > 0u && created == NULL) {
        free(objects);
        return false;
    }

    level_origin_x = -((float)level->width * WOLF_WORLD_TILE_SIZE) * 0.5f;
    level_origin_z = -((float)level->height * WOLF_WORLD_TILE_SIZE) * 0.5f;
    for (uint32_t index = 0u; index < object_count; ++index) {
        const WolfLevelObject *object = &objects[index];
        const ActorFamilyAnimation *family = find_family(object->subject);
        uint16_t initial_state;

        /* Dogs, bosses, ghosts, scenery and the dead guard are outside this humanoid actor collection. */
        if (family == NULL ||
            (!object_is_patrolling(object) &&
             (strlen(object->animation) < 2u ||
              object->animation[strlen(object->animation) - 2u] != '_' ||
              object->animation[strlen(object->animation) - 1u] != 's'))) {
            continue;
        }

        initial_state = object_is_patrolling(object) ? 1u : 0u;
        if (!wolf_actor_initialize(
                &created[created_count], created_count, family->family,
                object->tile_x, object->tile_y,
                level_origin_x + ((float)object->tile_x + 0.5f) * WOLF_WORLD_TILE_SIZE,
                level_origin_z + ((float)object->tile_y + 0.5f) * WOLF_WORLD_TILE_SIZE,
                object->yaw_radians, starting_health(family, difficulty),
                family->states, family->state_count, initial_state, NULL)) {
            free(objects);
            free(created);
            return false;
        }
        created[created_count].ai_action_state_id = WOLF_ACTOR_STATE_NONE;
        created[created_count].ai_random_state =
            0x9e3779b9u ^ (created_count + 1u) * 0x85ebca6bu;
        if (created[created_count].ai_random_state == 0u) {
            created[created_count].ai_random_state = 1u;
        }
        created[created_count].movement_direction =
            direction_from_yaw(object->yaw_radians);
        if (initial_state != 0u) {
            created[created_count].movement_follow_path_arrows = true;
            if (!wolf_actor_movement_set_direction(
                    &created[created_count], created[created_count].movement_direction)) {
                free(objects);
                free(created);
                return false;
            }
        }
        ++created_count;
    }

    free(objects);
    if (created_count == 0u) {
        free(created);
        created = NULL;
    }
    *actors = created;
    *actor_count = created_count;
    return true;
}

bool wolf_actor_collection_set_state(WolfActor *actor, uint16_t state_id)
{
    const ActorFamilyAnimation *family;

    if (actor == NULL) {
        return false;
    }
    family = find_family_by_type(actor->family);
    return family != NULL && wolf_actor_set_state(
        actor, family->states, family->state_count, state_id, NULL);
}

bool wolf_actor_collection_update_tic(WolfActor *actors, uint32_t actor_count)
{
    if (actor_count > 0u && actors == NULL) {
        return false;
    }
    for (uint32_t index = 0u; index < actor_count; ++index) {
        const ActorFamilyAnimation *family = find_family_by_type(actors[index].family);

        if (family != NULL && actors[index].active &&
            !wolf_actor_update_tic(
                &actors[index], family->states, family->state_count, NULL)) {
            return false;
        }
    }
    return true;
}

void wolf_actor_collection_destroy(WolfActor *actors)
{
    free(actors);
}
