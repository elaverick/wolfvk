#include "unity.h"

#include "wolfvk/player_death.h"

void test_player_death_fizzles_to_solid_red_and_holds_until_restart(void)
{
    WolfPlayerDeath death = {0};
    WolfPlayerState player;

    wolf_player_state_new_game(&player, 0u);
    TEST_ASSERT_TRUE(wolf_player_death_begin(&death));
    TEST_ASSERT_FALSE(wolf_player_death_begin(&death));
    TEST_ASSERT_EQUAL_INT(WOLF_PLAYER_DEATH_ANIMATING, death.phase);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, death.fizzle_progress);

    wolf_player_death_update(&death, &player, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, death.fizzle_progress);
    TEST_ASSERT_EQUAL_INT(WOLF_PLAYER_DEATH_ANIMATING, death.phase);
    TEST_ASSERT_EQUAL_INT32(3, player.lives);

    wolf_player_death_update(&death, &player, 0.6f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, death.fizzle_progress);
    TEST_ASSERT_EQUAL_INT(WOLF_PLAYER_DEATH_ANIMATING, death.phase);

    wolf_player_death_update(&death, &player, 0.25f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, death.fizzle_progress);
    TEST_ASSERT_EQUAL_INT(WOLF_PLAYER_DEATH_WAITING_FOR_FIRE, death.phase);
    TEST_ASSERT_EQUAL_INT32(2, player.lives);
}

void test_player_death_deducts_one_life_then_waits_for_fire(void)
{
    WolfPlayerDeath death = {0};
    WolfPlayerState player;

    wolf_player_state_new_game(&player, 0u);
    TEST_ASSERT_TRUE(wolf_player_death_begin(&death));
    wolf_player_death_update(&death, &player, 2.0f);
    TEST_ASSERT_EQUAL_INT32(2, player.lives);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, death.fizzle_progress);
    TEST_ASSERT_EQUAL_INT(WOLF_PLAYER_DEATH_WAITING_FOR_FIRE, death.phase);
    TEST_ASSERT_TRUE(death.life_deducted);
    wolf_player_death_update(&death, &player, 2.0f);
    TEST_ASSERT_EQUAL_INT32(2, player.lives);
    TEST_ASSERT_TRUE(wolf_player_death_fire_pressed(&death, &player));
}

void test_player_death_allows_the_original_final_life_but_defers_game_over(void)
{
    WolfPlayerDeath death = {0};
    WolfPlayerState player;

    wolf_player_state_new_game(&player, 0u);
    player.lives = 0;
    TEST_ASSERT_TRUE(wolf_player_death_begin(&death));
    wolf_player_death_update(&death, &player, 2.0f);
    TEST_ASSERT_EQUAL_INT32(-1, player.lives);
    TEST_ASSERT_EQUAL_INT(WOLF_PLAYER_DEATH_NO_LIVES, death.phase);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, death.fizzle_progress);
    TEST_ASSERT_FALSE(wolf_player_death_fire_pressed(&death, &player));
}

void test_player_death_restart_restores_original_loadout_and_preserves_totals(void)
{
    WolfPlayerDeath death = {0};
    WolfPlayerState player;

    wolf_player_state_new_game(&player, 4u);
    player.health = 0;
    player.ammo = 0;
    player.lives = 2;
    player.score = 12345;
    player.treasure_count = 7u;
    player.keys = WOLF_PLAYER_GOLD_KEY | WOLF_PLAYER_SILVER_KEY;
    player.weapon = WOLF_WEAPON_CHAINGUN;
    player.best_weapon = WOLF_WEAPON_CHAINGUN;
    player.chosen_weapon = WOLF_WEAPON_CHAINGUN;

    TEST_ASSERT_TRUE(wolf_player_death_begin(&death));
    wolf_player_death_update(&death, &player, 2.0f);
    wolf_player_death_prepare_restart(&death, &player);

    TEST_ASSERT_EQUAL_INT(WOLF_PLAYER_DEATH_ACTIVE, death.phase);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, death.fizzle_progress);
    TEST_ASSERT_EQUAL_INT32(100, player.health);
    TEST_ASSERT_EQUAL_INT32(8, player.ammo);
    TEST_ASSERT_EQUAL_INT32(1, player.lives);
    TEST_ASSERT_EQUAL_INT32(12345, player.score);
    TEST_ASSERT_EQUAL_UINT32(0u, player.treasure_count);
    TEST_ASSERT_EQUAL_UINT8(0u, player.keys);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_PISTOL, player.weapon);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_PISTOL, player.best_weapon);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_PISTOL, player.chosen_weapon);
    TEST_ASSERT_EQUAL_UINT32(4u, player.map_number);
}

void test_player_death_ignores_invalid_time_and_non_active_updates(void)
{
    WolfPlayerDeath death = {0};
    WolfPlayerState player;

    wolf_player_state_new_game(&player, 0u);
    wolf_player_death_update(&death, &player, 1.0f);
    TEST_ASSERT_EQUAL_INT32(3, player.lives);
    TEST_ASSERT_FALSE(wolf_player_death_begin(NULL));
    wolf_player_death_update(&death, &player, -1.0f);
    TEST_ASSERT_EQUAL_INT32(3, player.lives);
}
