#include "wolfvk/player_state.h"

#include <string.h>

/* The original episodes hold ten maps each; the status bar shows the map within the episode. */
#define WOLF_MAPS_PER_EPISODE 10u
/* Picking up a weapon always gives six bullets, as GiveWeapon does. */
#define WOLF_WEAPON_PICKUP_AMMO 6

/*
 * The original US_RndT steps through a fixed table of 256 bytes. Any
 * uniform byte source gives the same face behaviour, so a small xorshift
 * generator stands in for the table.
 */
static uint8_t next_random_byte(WolfPlayerState *player)
{
    uint32_t state = player->random_state != 0u ? player->random_state : 0x2545f491u;

    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    player->random_state = state;
    return (uint8_t)(state >> 24);
}

void wolf_player_state_new_game(WolfPlayerState *player, uint32_t level_index)
{
    if (player == NULL) {
        return;
    }
    memset(player, 0, sizeof(*player));
    player->health = WOLF_PLAYER_MAX_HEALTH;
    player->ammo = WOLF_PLAYER_START_AMMO;
    player->lives = WOLF_PLAYER_START_LIVES;
    player->next_extra_life = WOLF_PLAYER_EXTRA_LIFE_POINTS;
    player->weapon = WOLF_WEAPON_PISTOL;
    player->best_weapon = WOLF_WEAPON_PISTOL;
    player->chosen_weapon = WOLF_WEAPON_PISTOL;
    player->map_number = level_index % WOLF_MAPS_PER_EPISODE;
    player->random_state = 0x2545f491u;
    player->face_wait_ticks = next_random_byte(player);
}

bool wolf_player_state_heal(WolfPlayerState *player, int32_t points)
{
    if (player == NULL || points <= 0 || player->health >= WOLF_PLAYER_MAX_HEALTH) {
        return false;
    }
    player->health += points;
    player->got_gatling_face = false;
    if (player->health > WOLF_PLAYER_MAX_HEALTH) {
        player->health = WOLF_PLAYER_MAX_HEALTH;
    }
    return true;
}

bool wolf_player_state_take_damage(WolfPlayerState *player, int32_t points)
{
    if (player == NULL || points <= 0 || player->health <= 0) {
        return false;
    }
    player->health -= points;
    player->got_gatling_face = false;
    if (player->health <= 0) {
        player->health = 0;
        return true;
    }
    return false;
}

void wolf_player_state_give_ammo(WolfPlayerState *player, int32_t ammo)
{
    if (player == NULL || ammo <= 0) {
        return;
    }
    if (player->ammo == 0) {
        player->weapon = player->chosen_weapon;
    }
    player->ammo += ammo;
    if (player->ammo > WOLF_PLAYER_MAX_AMMO) {
        player->ammo = WOLF_PLAYER_MAX_AMMO;
    }
}

void wolf_player_state_give_weapon(WolfPlayerState *player, WolfWeaponType weapon)
{
    if (player == NULL || weapon < WOLF_WEAPON_KNIFE || weapon > WOLF_WEAPON_CHAINGUN) {
        return;
    }
    wolf_player_state_give_ammo(player, WOLF_WEAPON_PICKUP_AMMO);
    if (player->best_weapon < weapon) {
        player->best_weapon = weapon;
        player->weapon = weapon;
        player->chosen_weapon = weapon;
    }
}

bool wolf_player_state_select_weapon(WolfPlayerState *player, WolfWeaponType weapon)
{
    if (player == NULL || weapon < WOLF_WEAPON_KNIFE || weapon > player->best_weapon ||
        player->ammo <= 0) {
        return false;
    }
    player->weapon = weapon;
    player->chosen_weapon = weapon;
    return true;
}

void wolf_player_state_give_key(WolfPlayerState *player, uint8_t key)
{
    if (player != NULL) {
        player->keys |= key;
    }
}

bool wolf_player_state_has_key(const WolfPlayerState *player, uint8_t key_mask)
{
    return player != NULL && (player->keys & key_mask) == key_mask;
}

void wolf_player_state_give_points(WolfPlayerState *player, int32_t points)
{
    if (player == NULL || points <= 0) {
        return;
    }
    player->score += points;
    while (player->score >= player->next_extra_life) {
        player->next_extra_life += WOLF_PLAYER_EXTRA_LIFE_POINTS;
        if (player->lives < WOLF_PLAYER_MAX_LIVES) {
            ++player->lives;
        }
    }
}

bool wolf_player_state_update_face(WolfPlayerState *player, float delta_seconds)
{
    uint8_t previous_frame;

    if (player == NULL || !(delta_seconds > 0.0f)) {
        return false;
    }

    /*
     * WL_AGENT.C's UpdateFace accumulates game tics and compares them with
     * one random byte. The threshold stays fixed until a glance is selected;
     * drawing faster must not cause the face to glance more often.
     */
    player->face_ticks += delta_seconds * WOLF_PLAYER_FACE_TICKS_PER_SECOND;
    if (player->face_ticks <= (float)player->face_wait_ticks) {
        return false;
    }

    previous_frame = player->face_frame;
    player->got_gatling_face = false;
    player->face_frame = (uint8_t)(next_random_byte(player) >> 6);
    if (player->face_frame == 3u) {
        player->face_frame = 1u;
    }
    player->face_ticks = 0.0f;
    player->face_wait_ticks = next_random_byte(player);
    return player->face_frame != previous_frame;
}
