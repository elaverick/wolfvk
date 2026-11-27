#ifndef WOLFVK_PLAYER_DEATH_H
#define WOLFVK_PLAYER_DEATH_H

#include "wolfvk/player_state.h"

#include <stdbool.h>

typedef enum WolfPlayerDeathPhase {
    WOLF_PLAYER_DEATH_ACTIVE = 0,
    WOLF_PLAYER_DEATH_ANIMATING,
    WOLF_PLAYER_DEATH_WAITING_FOR_FIRE,
    WOLF_PLAYER_DEATH_NO_LIVES
} WolfPlayerDeathPhase;

typedef struct WolfPlayerDeath {
    WolfPlayerDeathPhase phase;
    float elapsed_seconds;
    float fizzle_progress; /* 0 = clear view, 1 = fully revealed red view */
    bool life_deducted;
} WolfPlayerDeath;

/* Start a single death sequence when health reaches zero. Repeated calls are harmless. */
bool wolf_player_death_begin(WolfPlayerDeath *death);

/* Advance the death presentation and deduct exactly one life when it completes. */
void wolf_player_death_update(
    WolfPlayerDeath *death,
    WolfPlayerState *player,
    float delta_seconds);

/* A fresh fire press requests a same-level restart only after the death sequence. */
bool wolf_player_death_fire_pressed(
    WolfPlayerDeath *death,
    const WolfPlayerState *player);

/* Restore the original restart loadout while retaining score, lives and map number. */
void wolf_player_death_prepare_restart(WolfPlayerDeath *death, WolfPlayerState *player);

#endif
