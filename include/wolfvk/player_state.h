#ifndef WOLFVK_PLAYER_STATE_H
#define WOLFVK_PLAYER_STATE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * The player's running totals, mirroring the fields of the original
 * `gamestate` that the status bar shows. Everything that changes health,
 * ammunition, keys or score goes through the functions below so the
 * original limits (100 health, 99 ammo, 9 lives, a life every 40,000
 * points) are applied in one place.
 */
#define WOLF_PLAYER_MAX_HEALTH 100
#define WOLF_PLAYER_MAX_AMMO 99
#define WOLF_PLAYER_MAX_LIVES 9
#define WOLF_PLAYER_START_AMMO 8
#define WOLF_PLAYER_START_LIVES 3
#define WOLF_PLAYER_EXTRA_LIFE_POINTS 40000
#define WOLF_PLAYER_GOLD_KEY 1u
#define WOLF_PLAYER_SILVER_KEY 2u
/* UpdateFace in the original changes the face after a random wait of up to 255 tics. */
#define WOLF_PLAYER_FACE_TICKS_PER_SECOND 70.0f

/* The four weapons in the original order: KNIFEPIC + weapon picks the status bar picture. */
typedef enum WolfWeaponType {
    WOLF_WEAPON_KNIFE = 0,
    WOLF_WEAPON_PISTOL,
    WOLF_WEAPON_MACHINEGUN,
    WOLF_WEAPON_CHAINGUN,
    WOLF_WEAPON_COUNT
} WolfWeaponType;

typedef struct WolfPlayerState {
    int32_t health;
    int32_t ammo;
    int32_t lives;
    int32_t score;
    int32_t next_extra_life;
    /* Bit 0 is the gold key and bit 1 the silver key, as in the original `keys`. */
    uint8_t keys;
    WolfWeaponType weapon;
    WolfWeaponType best_weapon;
    WolfWeaponType chosen_weapon;
    /* Zero-based map within the episode; the status bar shows it plus one as the floor. */
    uint32_t map_number;
    /* Treasure picked up on this floor, for the end-of-floor tally. */
    uint32_t treasure_count;
    /* 0-2 picks the look-ahead, look-left or look-right version of the face. */
    uint8_t face_frame;
    float face_ticks;
    uint32_t random_state;
    /* Set by the chaingun pickup: BJ grins until the face next changes, as with gotgatgun. */
    bool got_gatling_face;
} WolfPlayerState;

/*
 * Start a new game as the original NewGame does: full health, 8 bullets,
 * 3 lives, and only the knife and pistol, with the pistol in hand.
 */
void wolf_player_state_new_game(WolfPlayerState *player, uint32_t level_index);

/* Add health, capped at 100. Returns false if the player was already at full health. */
bool wolf_player_state_heal(WolfPlayerState *player, int32_t points);

/* Remove health, never below zero. Returns true if this killed the player. */
bool wolf_player_state_take_damage(WolfPlayerState *player, int32_t points);

/* Add ammunition, capped at 99, bringing out the chosen gun again if the knife was out because the ammo had run dry. */
void wolf_player_state_give_ammo(WolfPlayerState *player, int32_t ammo);

/* Pick up a weapon: 6 bullets, and switch to it if it is better than anything carried. */
void wolf_player_state_give_weapon(WolfPlayerState *player, WolfWeaponType weapon);

/*
 * Change the weapon in hand, as the original CheckWeaponChange does for keys
 * 1-4: only weapons already picked up can be chosen, and with no ammunition
 * the knife stays in hand. Returns false when the choice is not allowed.
 */
bool wolf_player_state_select_weapon(WolfPlayerState *player, WolfWeaponType weapon);

/* Add a key bit (WOLF_PLAYER_GOLD_KEY or WOLF_PLAYER_SILVER_KEY). */
void wolf_player_state_give_key(WolfPlayerState *player, uint8_t key);

/* True if every bit in key_mask is held. */
bool wolf_player_state_has_key(const WolfPlayerState *player, uint8_t key_mask);

/* Add points, awarding an extra life (up to 9) at every 40,000 points. */
void wolf_player_state_give_points(WolfPlayerState *player, int32_t points);

/*
 * Advance the face animation. As in UpdateFace, after a random wait the face
 * looks in a new direction. Returns true when the face frame changed.
 */
bool wolf_player_state_update_face(WolfPlayerState *player, float delta_seconds);

#endif
