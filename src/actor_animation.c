#include "wolfvk/actor_animation.h"

#include "wolfvk/world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define WOLF_ACTOR_WALK_STATE_COUNT 6u
#define WOLF_ACTOR_FAMILY_STATE_COUNT (WOLF_ACTOR_WALK_STATE_COUNT + 1u)

typedef struct ActorFamilyAnimation {
    const char *subject;
    WolfActorFamily family;
    int32_t health[4];
    WolfActorStateDefinition states[WOLF_ACTOR_FAMILY_STATE_COUNT];
} ActorFamilyAnimation;

#define ACTOR_STATES(prefix) { \
    {prefix "_s", 0u, 0u, WOLF_ACTOR_STATE_NONE, NULL, NULL}, \
    {prefix "_w", 0u, 10u, 2u, NULL, NULL}, \
    {prefix "_w", 0u, 3u, 3u, NULL, NULL}, \
    {prefix "_w", 1u, 8u, 4u, NULL, NULL}, \
    {prefix "_w", 2u, 10u, 5u, NULL, NULL}, \
    {prefix "_w", 2u, 3u, 6u, NULL, NULL}, \
    {prefix "_w", 3u, 8u, 1u, NULL, NULL} \
}

/*
 * The six walk-state entries follow the original grdpath/grdchase timing:
 * 10, 3, 8, 10, 3 and 8 tics. Each family uses its own named VOX animation.
 */
static const ActorFamilyAnimation actor_families[] = {
    {"grd", WOLF_ACTOR_GUARD, {25, 25, 25, 25}, ACTOR_STATES("grd")},
    {"ofc", WOLF_ACTOR_OFFICER, {50, 50, 50, 50}, ACTOR_STATES("ofc")},
    {"ss", WOLF_ACTOR_SS, {100, 100, 100, 100}, ACTOR_STATES("ss")},
    {"mut", WOLF_ACTOR_MUTANT, {45, 55, 55, 65}, ACTOR_STATES("mut")}
};

static const ActorFamilyAnimation *find_family(const char *subject)
{
    for (size_t index = 0u; index < sizeof(actor_families) / sizeof(actor_families[0]); ++index) {
        if (strcmp(subject, actor_families[index].subject) == 0) {
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

        /* The dead guard and all non-humanoid sprites remain static scene objects. */
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
                family->states, WOLF_ACTOR_FAMILY_STATE_COUNT, initial_state, NULL)) {
            free(objects);
            free(created);
            return false;
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

bool wolf_actor_collection_update_tic(WolfActor *actors, uint32_t actor_count)
{
    if (actor_count > 0u && actors == NULL) {
        return false;
    }
    for (uint32_t index = 0u; index < actor_count; ++index) {
        const ActorFamilyAnimation *family = NULL;

        for (size_t family_index = 0u;
             family_index < sizeof(actor_families) / sizeof(actor_families[0]);
             ++family_index) {
            if (actor_families[family_index].family == actors[index].family) {
                family = &actor_families[family_index];
                break;
            }
        }
        if (family != NULL && actors[index].active &&
            !wolf_actor_update_tic(
                &actors[index], family->states, WOLF_ACTOR_FAMILY_STATE_COUNT, NULL)) {
            return false;
        }
    }
    return true;
}

void wolf_actor_collection_destroy(WolfActor *actors)
{
    free(actors);
}
