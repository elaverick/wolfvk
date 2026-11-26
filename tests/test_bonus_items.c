#include "unity.h"

#include "wolfvk/bonus_items.h"
#include "wolfvk/game_world.h"
#include "wolfvk/status_bar.h"
#include "wolfvk/world.h"

#define TEST_EAST 1.57079632679f
#define TEST_MAP_SIZE 5u

static float tile_centre_world(int tile)
{
    return ((float)tile + 0.5f - (float)TEST_MAP_SIZE * 0.5f) * WOLF_WORLD_TILE_SIZE;
}

/* A small open map with one marker, and a player half a tile west of it facing east. */
static void make_world_with_marker(WolfWorldState *world, uint16_t marker)
{
    WolfLevel level = {0};

    level.width = TEST_MAP_SIZE;
    level.height = TEST_MAP_SIZE;
    level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] = marker;
    TEST_ASSERT_TRUE(wolf_world_state_init(world, &level, 98u));
}

static bool walk_up_and_collect(WolfWorldState *world)
{
    return wolf_world_state_collect_bonuses(
        world, tile_centre_world(2) - 0.5f * WOLF_WORLD_TILE_SIZE, tile_centre_world(2), TEST_EAST);
}

static uint16_t marker_at_centre(const WolfWorldState *world)
{
    return world->level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u];
}

void test_bonus_markers_follow_the_original_statinfo_table(void)
{
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_DOG_FOOD, wolf_bonus_type_for_marker(29u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_GOLD_KEY, wolf_bonus_type_for_marker(43u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_SILVER_KEY, wolf_bonus_type_for_marker(44u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_FOOD, wolf_bonus_type_for_marker(47u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_FIRST_AID, wolf_bonus_type_for_marker(48u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_CLIP, wolf_bonus_type_for_marker(49u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_MACHINEGUN, wolf_bonus_type_for_marker(50u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_CHAINGUN, wolf_bonus_type_for_marker(51u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_CROSS, wolf_bonus_type_for_marker(52u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_CROWN, wolf_bonus_type_for_marker(55u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_EXTRA_LIFE, wolf_bonus_type_for_marker(56u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_GIBS, wolf_bonus_type_for_marker(57u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_GIBS, wolf_bonus_type_for_marker(61u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_USED_CLIP, wolf_bonus_type_for_marker(71u));
    /* Scenery, the player start and enemies are not bonuses. */
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_NONE, wolf_bonus_type_for_marker(23u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_NONE, wolf_bonus_type_for_marker(24u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_NONE, wolf_bonus_type_for_marker(70u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_NONE, wolf_bonus_type_for_marker(19u));
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_NONE, wolf_bonus_type_for_marker(108u));
}

void test_bonus_grab_range_matches_the_original_view_test(void)
{
    /* Facing east from half a tile west of the item's centre: grabbed. */
    TEST_ASSERT_TRUE(wolf_bonus_in_grab_range(2.0f, 2.5f, TEST_EAST, 2, 2));
    /* Up to about two thirds of a tile ahead, measured from the player. */
    TEST_ASSERT_TRUE(wolf_bonus_in_grab_range(1.9f, 2.5f, TEST_EAST, 2, 2));
    TEST_ASSERT_FALSE(wolf_bonus_in_grab_range(1.8f, 2.5f, TEST_EAST, 2, 2));
    /* Not when facing away, nor when it is more than half a tile to the side. */
    TEST_ASSERT_FALSE(wolf_bonus_in_grab_range(2.0f, 2.5f, -TEST_EAST, 2, 2));
    TEST_ASSERT_FALSE(wolf_bonus_in_grab_range(2.0f, 1.9f, TEST_EAST, 2, 2));
    /* Yaw 0 looks north, towards smaller map rows. */
    TEST_ASSERT_TRUE(wolf_bonus_in_grab_range(2.5f, 3.0f, 0.0f, 2, 2));
    TEST_ASSERT_FALSE(wolf_bonus_in_grab_range(2.5f, 2.0f, 0.0f, 2, 2));
}

void test_bonus_health_items_heal_only_when_useful(void)
{
    WolfPlayerState player;

    wolf_player_state_new_game(&player, 0u);
    TEST_ASSERT_FALSE(wolf_bonus_apply(&player, WOLF_BONUS_FIRST_AID));
    TEST_ASSERT_FALSE(wolf_bonus_apply(&player, WOLF_BONUS_FOOD));

    player.health = 50;
    TEST_ASSERT_TRUE(wolf_bonus_apply(&player, WOLF_BONUS_FIRST_AID));
    TEST_ASSERT_EQUAL_INT32(75, player.health);
    TEST_ASSERT_TRUE(wolf_bonus_apply(&player, WOLF_BONUS_FOOD));
    TEST_ASSERT_EQUAL_INT32(85, player.health);
    TEST_ASSERT_TRUE(wolf_bonus_apply(&player, WOLF_BONUS_DOG_FOOD));
    TEST_ASSERT_EQUAL_INT32(89, player.health);

    /* Gibs are only eaten at 10 health or less. */
    TEST_ASSERT_FALSE(wolf_bonus_apply(&player, WOLF_BONUS_GIBS));
    player.health = 10;
    TEST_ASSERT_TRUE(wolf_bonus_apply(&player, WOLF_BONUS_GIBS));
    TEST_ASSERT_EQUAL_INT32(11, player.health);
}

void test_bonus_ammunition_weapons_and_treasure(void)
{
    WolfPlayerState player;

    wolf_player_state_new_game(&player, 0u);
    TEST_ASSERT_TRUE(wolf_bonus_apply(&player, WOLF_BONUS_CLIP));
    TEST_ASSERT_EQUAL_INT32(16, player.ammo);
    TEST_ASSERT_TRUE(wolf_bonus_apply(&player, WOLF_BONUS_USED_CLIP));
    TEST_ASSERT_EQUAL_INT32(20, player.ammo);
    player.ammo = 99;
    TEST_ASSERT_FALSE(wolf_bonus_apply(&player, WOLF_BONUS_CLIP));

    player.ammo = 8;
    TEST_ASSERT_TRUE(wolf_bonus_apply(&player, WOLF_BONUS_MACHINEGUN));
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_MACHINEGUN, player.weapon);
    TEST_ASSERT_EQUAL_INT32(14, player.ammo);

    /* The chaingun switches weapon and makes BJ grin until his face next changes. */
    TEST_ASSERT_TRUE(wolf_bonus_apply(&player, WOLF_BONUS_CHAINGUN));
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_CHAINGUN, player.weapon);
    TEST_ASSERT_EQUAL_UINT32(WOLF_VGA_GOTGATLINGPIC, wolf_status_bar_face_picture(&player));
    while (!wolf_player_state_update_face(&player, 1.0f / 70.0f) && player.got_gatling_face) {
    }
    TEST_ASSERT_FALSE(player.got_gatling_face);
    TEST_ASSERT_NOT_EQUAL(WOLF_VGA_GOTGATLINGPIC, wolf_status_bar_face_picture(&player));

    TEST_ASSERT_TRUE(wolf_bonus_apply(&player, WOLF_BONUS_CROSS));
    TEST_ASSERT_TRUE(wolf_bonus_apply(&player, WOLF_BONUS_CHALICE));
    TEST_ASSERT_TRUE(wolf_bonus_apply(&player, WOLF_BONUS_BIBLE));
    TEST_ASSERT_TRUE(wolf_bonus_apply(&player, WOLF_BONUS_CROWN));
    TEST_ASSERT_EQUAL_INT32(6600, player.score);
    TEST_ASSERT_EQUAL_UINT32(4u, player.treasure_count);

    /* The one-up heals fully, adds 25 bullets and a life, and counts as treasure. */
    player.health = 5;
    player.ammo = 10;
    TEST_ASSERT_TRUE(wolf_bonus_apply(&player, WOLF_BONUS_EXTRA_LIFE));
    TEST_ASSERT_EQUAL_INT32(100, player.health);
    TEST_ASSERT_EQUAL_INT32(35, player.ammo);
    TEST_ASSERT_EQUAL_INT32(4, player.lives);
    TEST_ASSERT_EQUAL_UINT32(5u, player.treasure_count);

    TEST_ASSERT_FALSE(wolf_bonus_apply(&player, WOLF_BONUS_NONE));
    TEST_ASSERT_FALSE(wolf_bonus_apply(NULL, WOLF_BONUS_CROSS));
}

void test_world_removes_picked_up_items_and_leaves_unneeded_ones(void)
{
    WolfWorldState world = {0};

    /* A clip is taken while walking towards it and disappears from the map. */
    make_world_with_marker(&world, 49u);
    TEST_ASSERT_TRUE(walk_up_and_collect(&world));
    TEST_ASSERT_EQUAL_INT32(16, world.player.ammo);
    TEST_ASSERT_EQUAL_UINT16(0u, marker_at_centre(&world));

    /* First aid at full health stays where it is. */
    make_world_with_marker(&world, 48u);
    TEST_ASSERT_FALSE(walk_up_and_collect(&world));
    TEST_ASSERT_EQUAL_UINT16(48u, marker_at_centre(&world));

    /* Scenery is never picked up. */
    make_world_with_marker(&world, 24u);
    TEST_ASSERT_FALSE(walk_up_and_collect(&world));
    TEST_ASSERT_EQUAL_UINT16(24u, marker_at_centre(&world));

    /* Walking past with the item behind the player does not pick it up. */
    make_world_with_marker(&world, 52u);
    TEST_ASSERT_FALSE(wolf_world_state_collect_bonuses(
        &world, tile_centre_world(2) - 0.5f * WOLF_WORLD_TILE_SIZE, tile_centre_world(2), -TEST_EAST));
    TEST_ASSERT_TRUE(walk_up_and_collect(&world));
    TEST_ASSERT_EQUAL_INT32(100, world.player.score);
}

void test_new_game_holds_only_the_knife_and_pistol(void)
{
    WolfPlayerState player;

    wolf_player_state_new_game(&player, 0u);
    TEST_ASSERT_EQUAL_INT32(8, player.ammo);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_PISTOL, player.weapon);
    TEST_ASSERT_TRUE(wolf_player_state_select_weapon(&player, WOLF_WEAPON_KNIFE));
    TEST_ASSERT_TRUE(wolf_player_state_select_weapon(&player, WOLF_WEAPON_PISTOL));
    TEST_ASSERT_FALSE(wolf_player_state_select_weapon(&player, WOLF_WEAPON_MACHINEGUN));
    TEST_ASSERT_FALSE(wolf_player_state_select_weapon(&player, WOLF_WEAPON_CHAINGUN));
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_PISTOL, player.weapon);

    /* Once picked up, the machine gun can be chosen; the chaingun still cannot. */
    (void)wolf_bonus_apply(&player, WOLF_BONUS_MACHINEGUN);
    TEST_ASSERT_TRUE(wolf_player_state_select_weapon(&player, WOLF_WEAPON_PISTOL));
    TEST_ASSERT_TRUE(wolf_player_state_select_weapon(&player, WOLF_WEAPON_MACHINEGUN));
    TEST_ASSERT_FALSE(wolf_player_state_select_weapon(&player, WOLF_WEAPON_CHAINGUN));

    /* With no bullets the weapon cannot be changed, as in the original. */
    player.ammo = 0;
    TEST_ASSERT_FALSE(wolf_player_state_select_weapon(&player, WOLF_WEAPON_KNIFE));
}

void test_bonus_collection_reports_each_original_sound_event(void)
{
    WolfWorldState world = {0};
    WolfBonusType collected[9] = {WOLF_BONUS_NONE};
    size_t collected_count = 0u;
    WolfLevel level = {0};

    level.width = TEST_MAP_SIZE;
    level.height = TEST_MAP_SIZE;
    level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 48u;
    level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 49u;
    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 98u));

    TEST_ASSERT_TRUE(wolf_world_state_collect_bonuses_with_events(
        &world, tile_centre_world(2) - 0.5f * WOLF_WORLD_TILE_SIZE,
        tile_centre_world(2), TEST_EAST, collected,
        sizeof(collected) / sizeof(collected[0]), &collected_count));
    TEST_ASSERT_EQUAL_UINT(2u, collected_count);
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_FIRST_AID, collected[0]);
    TEST_ASSERT_EQUAL_INT(WOLF_BONUS_CLIP, collected[1]);
    TEST_ASSERT_EQUAL_UINT16(0u, world.level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 2u]);
    TEST_ASSERT_EQUAL_UINT16(0u, world.level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 3u]);
}
