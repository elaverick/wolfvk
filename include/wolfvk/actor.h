#ifndef WOLFVK_ACTOR_H
#define WOLFVK_ACTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WOLF_ACTOR_ANIMATION_NAME_LENGTH 24u
#define WOLF_ACTOR_STATE_NONE UINT16_MAX

typedef enum WolfActorFamily {
    WOLF_ACTOR_GUARD = 0,
    WOLF_ACTOR_OFFICER,
    WOLF_ACTOR_SS,
    WOLF_ACTOR_MUTANT,
    WOLF_ACTOR_FAMILY_COUNT
} WolfActorFamily;

typedef struct WolfActor WolfActor;
typedef void (*WolfActorStateCallback)(WolfActor *actor, void *context);

/*
 * A state definition is immutable shared data. Its duration is measured in
 * original 70 Hz game tics; zero denotes a persistent state such as standing.
 */
typedef struct WolfActorStateDefinition {
    const char *animation;
    uint8_t pose;
    uint16_t duration_tics;
    uint16_t next_state;
    WolfActorStateCallback think;
    WolfActorStateCallback on_enter;
} WolfActorStateDefinition;

/*
 * Per-instance actor data. Animation/state data remains in the shared
 * definition table, while position, health and elapsed timing belong here.
 */
struct WolfActor {
    uint32_t id;
    WolfActorFamily family;
    uint16_t tile_x;
    uint16_t tile_y;
    float position_x;
    float position_z;
    float yaw_radians;
    int32_t health;
    uint16_t state_id;
    uint16_t state_tics_remaining;
    uint32_t state_elapsed_tics;
    char animation[WOLF_ACTOR_ANIMATION_NAME_LENGTH];
    uint8_t pose;
    bool active;
    bool shootable;
};

/* Initialise a live actor at its decoded map position and enter its first state. */
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
    void *context);

/* Enter a state, resetting its tic timer and invoking its entry action once. */
bool wolf_actor_set_state(
    WolfActor *actor,
    const WolfActorStateDefinition *states,
    size_t state_count,
    uint16_t state_id,
    void *context);

/* Advance exactly one 70 Hz actor tic; a timed state changes at most once. */
bool wolf_actor_update_tic(
    WolfActor *actor,
    const WolfActorStateDefinition *states,
    size_t state_count,
    void *context);

#endif
