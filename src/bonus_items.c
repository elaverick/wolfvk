#include "wolfvk/bonus_items.h"

#include "wolfvk/level_objects.h"

#include <math.h>

/*
 * Bonus type of each WL6 statinfo entry, indexed by marker - 23. Entries
 * left as WOLF_BONUS_NONE are scenery, blocking or not.
 */
static const WolfBonusType static_bonus_types[] = {
    [6] = WOLF_BONUS_DOG_FOOD,      /* SPR_STAT_6, bad food */
    [20] = WOLF_BONUS_GOLD_KEY,     /* SPR_STAT_20, key 1 */
    [21] = WOLF_BONUS_SILVER_KEY,   /* SPR_STAT_21, key 2 */
    [24] = WOLF_BONUS_FOOD,         /* SPR_STAT_24, good food */
    [25] = WOLF_BONUS_FIRST_AID,    /* SPR_STAT_25 */
    [26] = WOLF_BONUS_CLIP,         /* SPR_STAT_26 */
    [27] = WOLF_BONUS_MACHINEGUN,   /* SPR_STAT_27 */
    [28] = WOLF_BONUS_CHAINGUN,     /* SPR_STAT_28 */
    [29] = WOLF_BONUS_CROSS,        /* SPR_STAT_29 */
    [30] = WOLF_BONUS_CHALICE,      /* SPR_STAT_30 */
    [31] = WOLF_BONUS_BIBLE,        /* SPR_STAT_31 */
    [32] = WOLF_BONUS_CROWN,        /* SPR_STAT_32 */
    [33] = WOLF_BONUS_EXTRA_LIFE,   /* SPR_STAT_33, one up */
    [34] = WOLF_BONUS_GIBS,         /* SPR_STAT_34 */
    [38] = WOLF_BONUS_GIBS,         /* SPR_STAT_38, gibs 2 */
    [48] = WOLF_BONUS_USED_CLIP     /* marker 71, a clip dropped by a guard */
};

WolfBonusType wolf_bonus_type_for_marker(uint16_t marker)
{
    uint32_t index;

    if (marker < WOLF_LEVEL_STATIC_FIRST || marker > WOLF_LEVEL_STATIC_EXTRA_CLIP) {
        return WOLF_BONUS_NONE;
    }
    index = (uint32_t)marker - WOLF_LEVEL_STATIC_FIRST;
    if (index >= sizeof(static_bonus_types) / sizeof(static_bonus_types[0])) {
        return WOLF_BONUS_NONE;
    }
    return static_bonus_types[index];
}

bool wolf_bonus_in_grab_range(
    float player_x,
    float player_y,
    float player_yaw_radians,
    int tile_x,
    int tile_y)
{
    /* Yaw 0 looks north (towards smaller map y) and turns clockwise towards east. */
    float forward_x = sinf(player_yaw_radians);
    float forward_y = -cosf(player_yaw_radians);
    float offset_x = (float)tile_x + 0.5f - player_x;
    float offset_y = (float)tile_y + 0.5f - player_y;
    float depth = offset_x * forward_x + offset_y * forward_y + WOLF_BONUS_FOCAL_LENGTH_TILES;
    float side = offset_x * -forward_y + offset_y * forward_x;

    if (!isfinite(depth) || !isfinite(side)) {
        return false;
    }
    return depth >= WOLF_BONUS_MIN_DISTANCE_TILES &&
        depth < WOLF_BONUS_GRAB_DEPTH_TILES &&
        side > -WOLF_BONUS_GRAB_HALF_WIDTH_TILES &&
        side < WOLF_BONUS_GRAB_HALF_WIDTH_TILES;
}

/* Treasure scores points and counts towards the floor's treasure ratio. */
static bool give_treasure(WolfPlayerState *player, int32_t points)
{
    wolf_player_state_give_points(player, points);
    ++player->treasure_count;
    return true;
}

/* Ammunition is left on the floor when the player already carries the maximum. */
static bool give_clip(WolfPlayerState *player, int32_t ammo)
{
    if (player->ammo >= WOLF_PLAYER_MAX_AMMO) {
        return false;
    }
    wolf_player_state_give_ammo(player, ammo);
    return true;
}

bool wolf_bonus_apply(WolfPlayerState *player, WolfBonusType bonus)
{
    if (player == NULL) {
        return false;
    }

    switch (bonus) {
    case WOLF_BONUS_GOLD_KEY:
        wolf_player_state_give_key(player, WOLF_PLAYER_GOLD_KEY);
        return true;
    case WOLF_BONUS_SILVER_KEY:
        wolf_player_state_give_key(player, WOLF_PLAYER_SILVER_KEY);
        return true;
    case WOLF_BONUS_FIRST_AID:
        return wolf_player_state_heal(player, 25);
    case WOLF_BONUS_FOOD:
        return wolf_player_state_heal(player, 10);
    case WOLF_BONUS_DOG_FOOD:
        return wolf_player_state_heal(player, 4);
    case WOLF_BONUS_GIBS:
        /* Only a desperate player eats these. */
        if (player->health > 10) {
            return false;
        }
        return wolf_player_state_heal(player, 1);
    case WOLF_BONUS_CLIP:
        return give_clip(player, 8);
    case WOLF_BONUS_USED_CLIP:
        return give_clip(player, 4);
    case WOLF_BONUS_MACHINEGUN:
        wolf_player_state_give_weapon(player, WOLF_WEAPON_MACHINEGUN);
        return true;
    case WOLF_BONUS_CHAINGUN:
        wolf_player_state_give_weapon(player, WOLF_WEAPON_CHAINGUN);
        player->got_gatling_face = true;
        player->face_ticks = 0.0f;
        return true;
    case WOLF_BONUS_CROSS:
        return give_treasure(player, 100);
    case WOLF_BONUS_CHALICE:
        return give_treasure(player, 500);
    case WOLF_BONUS_BIBLE:
        return give_treasure(player, 1000);
    case WOLF_BONUS_CROWN:
        return give_treasure(player, 5000);
    case WOLF_BONUS_EXTRA_LIFE:
        /* HealSelf(99), GiveAmmo(25) and GiveExtraMan(), and it counts as treasure. */
        (void)wolf_player_state_heal(player, 99);
        wolf_player_state_give_ammo(player, 25);
        if (player->lives < WOLF_PLAYER_MAX_LIVES) {
            ++player->lives;
        }
        ++player->treasure_count;
        return true;
    case WOLF_BONUS_NONE:
    default:
        return false;
    }
}
