#include "unity.h"

#include "wolfvk/player_state.h"

#include <stdbool.h>

void test_player_state_new_game_matches_the_original_defaults(void)
{
    WolfPlayerState player;

    /* Level 14 is the fourth map of episode two, shown on the bar as floor 4. */
    wolf_player_state_new_game(&player, 13u);
    TEST_ASSERT_EQUAL_INT32(100, player.health);
    TEST_ASSERT_EQUAL_INT32(8, player.ammo);
    TEST_ASSERT_EQUAL_INT32(3, player.lives);
    TEST_ASSERT_EQUAL_INT32(0, player.score);
    TEST_ASSERT_EQUAL_INT32(40000, player.next_extra_life);
    TEST_ASSERT_EQUAL_UINT8(0u, player.keys);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_PISTOL, player.weapon);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_PISTOL, player.best_weapon);
    TEST_ASSERT_EQUAL_UINT32(3u, player.map_number);
}

void test_player_state_health_is_kept_between_zero_and_one_hundred(void)
{
    WolfPlayerState player;

    wolf_player_state_new_game(&player, 0u);
    TEST_ASSERT_FALSE(wolf_player_state_heal(&player, 25));

    TEST_ASSERT_FALSE(wolf_player_state_take_damage(&player, 90));
    TEST_ASSERT_EQUAL_INT32(10, player.health);
    TEST_ASSERT_TRUE(wolf_player_state_heal(&player, 25));
    TEST_ASSERT_EQUAL_INT32(35, player.health);
    TEST_ASSERT_TRUE(wolf_player_state_heal(&player, 100));
    TEST_ASSERT_EQUAL_INT32(100, player.health);

    TEST_ASSERT_TRUE(wolf_player_state_take_damage(&player, 150));
    TEST_ASSERT_EQUAL_INT32(0, player.health);
    TEST_ASSERT_FALSE(wolf_player_state_take_damage(&player, 10));
}

void test_player_state_ammo_caps_at_99_and_restores_the_gun(void)
{
    WolfPlayerState player;

    wolf_player_state_new_game(&player, 0u);
    wolf_player_state_give_ammo(&player, 200);
    TEST_ASSERT_EQUAL_INT32(99, player.ammo);

    /* With no bullets the knife comes out; new ammunition brings back the chosen gun. */
    player.ammo = 0;
    player.weapon = WOLF_WEAPON_KNIFE;
    wolf_player_state_give_ammo(&player, 8);
    TEST_ASSERT_EQUAL_INT32(8, player.ammo);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_PISTOL, player.weapon);
}

void test_player_state_weapon_pickups_add_ammo_and_only_upgrade(void)
{
    WolfPlayerState player;

    wolf_player_state_new_game(&player, 0u);
    wolf_player_state_give_weapon(&player, WOLF_WEAPON_CHAINGUN);
    TEST_ASSERT_EQUAL_INT32(14, player.ammo);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_CHAINGUN, player.weapon);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_CHAINGUN, player.best_weapon);

    wolf_player_state_give_weapon(&player, WOLF_WEAPON_MACHINEGUN);
    TEST_ASSERT_EQUAL_INT32(20, player.ammo);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_CHAINGUN, player.weapon);

    /* Choosing a weapon changes the one in hand and the one restored when ammo returns. */
    wolf_player_state_select_weapon(&player, WOLF_WEAPON_KNIFE);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_KNIFE, player.weapon);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_KNIFE, player.chosen_weapon);
    wolf_player_state_select_weapon(&player, WOLF_WEAPON_COUNT);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_KNIFE, player.weapon);
}

void test_player_state_keys_and_extra_lives(void)
{
    WolfPlayerState player;

    wolf_player_state_new_game(&player, 0u);
    TEST_ASSERT_FALSE(wolf_player_state_has_key(&player, WOLF_PLAYER_GOLD_KEY));
    wolf_player_state_give_key(&player, WOLF_PLAYER_SILVER_KEY);
    TEST_ASSERT_TRUE(wolf_player_state_has_key(&player, WOLF_PLAYER_SILVER_KEY));
    TEST_ASSERT_FALSE(wolf_player_state_has_key(&player, WOLF_PLAYER_GOLD_KEY));

    /* A life at every 40,000 points, even when one award crosses two thresholds. */
    wolf_player_state_give_points(&player, 39999);
    TEST_ASSERT_EQUAL_INT32(3, player.lives);
    wolf_player_state_give_points(&player, 40001);
    TEST_ASSERT_EQUAL_INT32(5, player.lives);
    TEST_ASSERT_EQUAL_INT32(80000, player.score);
    TEST_ASSERT_EQUAL_INT32(120000, player.next_extra_life);

    /* Lives stop at 9. */
    wolf_player_state_give_points(&player, 1000000);
    TEST_ASSERT_EQUAL_INT32(9, player.lives);
}

void test_player_state_face_wait_uses_one_original_random_tic_threshold(void)
{
    WolfPlayerState player;
    float threshold_seconds;
    uint8_t initial_wait;

    wolf_player_state_new_game(&player, 0u);
    initial_wait = player.face_wait_ticks;
    threshold_seconds = ((float)initial_wait + 0.25f) /
        WOLF_PLAYER_FACE_TICKS_PER_SECOND;

    /* Many render frames cannot resample the wait and accelerate the glance. */
    for (int frame = 0; frame < 30; ++frame) {
        TEST_ASSERT_FALSE(wolf_player_state_update_face(&player, threshold_seconds / 60.0f));
        TEST_ASSERT_EQUAL_UINT8(initial_wait, player.face_wait_ticks);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.001f, (float)initial_wait + 0.25f, player.face_ticks);

    /* The original comparison is strict: the face changes only after the threshold. */
    TEST_ASSERT_TRUE(wolf_player_state_update_face(&player, 0.01f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, player.face_ticks);
    TEST_ASSERT_TRUE(player.face_frame < 3u);
}

void test_player_state_face_wait_is_reselected_after_each_glance(void)
{
    WolfPlayerState player;
    uint8_t initial_wait;
    uint32_t initial_random_state;

    wolf_player_state_new_game(&player, 0u);
    initial_wait = player.face_wait_ticks;
    initial_random_state = player.random_state;

    while (player.face_ticks <= (float)player.face_wait_ticks) {
        (void)wolf_player_state_update_face(&player, 1.0f / WOLF_PLAYER_FACE_TICKS_PER_SECOND);
    }

    TEST_ASSERT_EQUAL_FLOAT(0.0f, player.face_ticks);
    TEST_ASSERT_TRUE(player.face_frame < 3u);
    TEST_ASSERT_TRUE(player.random_state != initial_random_state);
    TEST_ASSERT_TRUE(player.face_wait_ticks <= UINT8_MAX);
    TEST_ASSERT_TRUE(initial_wait <= UINT8_MAX);
}

void test_player_state_face_glances_only_ahead_left_or_right(void)
{
    WolfPlayerState player;
    bool seen[3] = {false, false, false};

    wolf_player_state_new_game(&player, 0u);
    TEST_ASSERT_FALSE(wolf_player_state_update_face(&player, 0.0f));
    for (int frame = 0; frame < 20000; ++frame) {
        (void)wolf_player_state_update_face(&player, 1.0f / 70.0f);
        TEST_ASSERT_TRUE(player.face_frame < 3u);
        seen[player.face_frame] = true;
    }
    TEST_ASSERT_TRUE(seen[0] && seen[1] && seen[2]);
}
