#include "wolfvk/actor.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Validate state tables at their boundary so bad animation data cannot corrupt an actor. */
static bool state_definition_is_valid(
    const WolfActorStateDefinition *states,
    size_t state_count,
    uint16_t state_id)
{
    return states != NULL && state_count > 0u && state_id < state_count &&
        states[state_id].animation != NULL && states[state_id].animation[0] != '\0' &&
        (states[state_id].next_state == WOLF_ACTOR_STATE_NONE ||
         states[state_id].next_state < state_count);
}

bool wolf_actor_set_state(
    WolfActor *actor,
    const WolfActorStateDefinition *states,
    size_t state_count,
    uint16_t state_id,
    void *context)
{
    const WolfActorStateDefinition *definition;

    if (actor == NULL || !state_definition_is_valid(states, state_count, state_id)) {
        return false;
    }

    definition = &states[state_id];
    actor->state_id = state_id;
    actor->state_tics_remaining = definition->duration_tics;
    actor->state_elapsed_tics = 0u;
    actor->ai_action_fired = false;
    (void)snprintf(actor->animation, sizeof(actor->animation), "%s", definition->animation);
    actor->pose = definition->pose;
    actor->active = true;

    if (definition->on_enter != NULL) {
        definition->on_enter(actor, context);
    }
    return true;
}

bool wolf_actor_initialize(
    WolfActor *actor,
    uint32_t id,
    WolfActorFamily family,
    uint16_t tile_x,
    uint16_t tile_y,
    float position_x,
    float position_z,
    float yaw_radians,
    int32_t health,
    const WolfActorStateDefinition *states,
    size_t state_count,
    uint16_t initial_state,
    void *context)
{
    if (actor == NULL || family < WOLF_ACTOR_GUARD || family >= WOLF_ACTOR_FAMILY_COUNT ||
        !isfinite(position_x) || !isfinite(position_z) || !isfinite(yaw_radians) ||
        health <= 0 || !state_definition_is_valid(states, state_count, initial_state)) {
        return false;
    }

    memset(actor, 0, sizeof(*actor));
    actor->id = id;
    actor->family = family;
    actor->tile_x = tile_x;
    actor->tile_y = tile_y;
    actor->position_x = position_x;
    actor->position_z = position_z;
    actor->yaw_radians = yaw_radians;
    actor->health = health;
    actor->shootable = true;
    actor->state_id = WOLF_ACTOR_STATE_NONE;

    return wolf_actor_set_state(actor, states, state_count, initial_state, context);
}

bool wolf_actor_update_tic(
    WolfActor *actor,
    const WolfActorStateDefinition *states,
    size_t state_count,
    void *context)
{
    const WolfActorStateDefinition *definition;
    uint16_t entered_state;

    if (actor == NULL || !actor->active ||
        !state_definition_is_valid(states, state_count, actor->state_id)) {
        return false;
    }

    entered_state = actor->state_id;
    definition = &states[entered_state];
    if (definition->think != NULL) {
        definition->think(actor, context);
    }
    /* Sight and action callbacks may deliberately change state immediately. */
    if (!actor->active || actor->state_id != entered_state) {
        return true;
    }
    if (actor->state_elapsed_tics < UINT32_MAX) {
        ++actor->state_elapsed_tics;
    }

    /* Persistent states keep thinking without manufacturing timer transitions. */
    if (definition->duration_tics == 0u) {
        return true;
    }
    if (actor->state_tics_remaining > 0u) {
        --actor->state_tics_remaining;
    }
    if (actor->state_tics_remaining != 0u) {
        return true;
    }

    if (definition->next_state == WOLF_ACTOR_STATE_NONE) {
        actor->active = false;
        actor->shootable = false;
        return true;
    }

    return wolf_actor_set_state(
        actor, states, state_count, definition->next_state, context);
}
