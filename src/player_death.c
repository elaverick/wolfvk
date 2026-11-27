#include "wolfvk/player_death.h"

#include <math.h>

#define WOLF_PLAYER_DEATH_DURATION_SECONDS 1.35f

bool wolf_player_death_begin(WolfPlayerDeath *death)
{
    if (death == NULL || death->phase != WOLF_PLAYER_DEATH_ACTIVE) {
        return false;
    }

    death->phase = WOLF_PLAYER_DEATH_ANIMATING;
    death->elapsed_seconds = 0.0f;
    death->red_flash = 1.0f;
    death->life_deducted = false;
    return true;
}

void wolf_player_death_update(
    WolfPlayerDeath *death,
    WolfPlayerState *player,
    float delta_seconds)
{
    if (death == NULL || player == NULL ||
        death->phase != WOLF_PLAYER_DEATH_ANIMATING ||
        !isfinite(delta_seconds) || delta_seconds <= 0.0f) {
        return;
    }

    death->elapsed_seconds += delta_seconds;
    if (death->elapsed_seconds >= WOLF_PLAYER_DEATH_DURATION_SECONDS) {
        death->elapsed_seconds = WOLF_PLAYER_DEATH_DURATION_SECONDS;
        death->red_flash = 0.0f;
        if (!death->life_deducted) {
            --player->lives;
            death->life_deducted = true;
        }
        /* The original keeps playing while lives is zero; -1 means exhausted. */
        death->phase = player->lives >= 0
            ? WOLF_PLAYER_DEATH_WAITING_FOR_FIRE
            : WOLF_PLAYER_DEATH_NO_LIVES;
        return;
    }

    /* Keep the initial damage red briefly, then fade it during the fall. */
    death->red_flash = 1.0f - death->elapsed_seconds /
        WOLF_PLAYER_DEATH_DURATION_SECONDS;
}

bool wolf_player_death_fire_pressed(
    WolfPlayerDeath *death,
    const WolfPlayerState *player)
{
    return death != NULL && player != NULL &&
        death->phase == WOLF_PLAYER_DEATH_WAITING_FOR_FIRE &&
        player->lives >= 0;
}

void wolf_player_death_prepare_restart(WolfPlayerDeath *death, WolfPlayerState *player)
{
    if (death == NULL || player == NULL ||
        death->phase != WOLF_PLAYER_DEATH_WAITING_FOR_FIRE ||
        player->lives < 0) {
        return;
    }

    player->health = WOLF_PLAYER_MAX_HEALTH;
    player->ammo = WOLF_PLAYER_START_AMMO;
    player->keys = 0u;
    player->treasure_count = 0u;
    player->weapon = WOLF_WEAPON_PISTOL;
    player->best_weapon = WOLF_WEAPON_PISTOL;
    player->chosen_weapon = WOLF_WEAPON_PISTOL;
    player->face_frame = 0u;
    player->face_ticks = 0.0;
    player->face_count = 0u;
    player->got_gatling_face = false;

    death->phase = WOLF_PLAYER_DEATH_ACTIVE;
    death->elapsed_seconds = 0.0f;
    death->red_flash = 0.0f;
    death->life_deducted = false;
}
