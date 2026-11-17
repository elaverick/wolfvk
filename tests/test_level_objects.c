#include "unity.h"

#include "wolfvk/level_objects.h"

#include <stdlib.h>
#include <string.h>

#define TEST_PI 3.14159265358979f

static WolfLevel test_level;

/* An open 64x64 map surrounded by a one-tile wall, so every cell has a finite view. */
static WolfLevel *open_level(void)
{
    memset(&test_level, 0, sizeof(test_level));
    test_level.width = WOLF_LEVEL_PLANE_WIDTH;
    test_level.height = WOLF_LEVEL_PLANE_HEIGHT;
    for (uint32_t index = 0u; index < WOLF_LEVEL_PLANE_WIDTH; ++index) {
        test_level.wall_tiles[index] = 1u;
        test_level.wall_tiles[(WOLF_LEVEL_PLANE_HEIGHT - 1u) * WOLF_LEVEL_PLANE_WIDTH + index] = 1u;
        test_level.wall_tiles[index * WOLF_LEVEL_PLANE_WIDTH] = 1u;
        test_level.wall_tiles[index * WOLF_LEVEL_PLANE_WIDTH + WOLF_LEVEL_PLANE_WIDTH - 1u] = 1u;
    }
    return &test_level;
}

static void place(WolfLevel *level, uint16_t tile_x, uint16_t tile_y, uint16_t marker)
{
    level->object_tiles[(size_t)tile_y * WOLF_LEVEL_PLANE_WIDTH + tile_x] = marker;
}

/* Classify the single marker at (10, 10); false when it places nothing. */
static bool classify_one(uint16_t marker, WolfDifficulty difficulty, WolfLevelObject *object)
{
    WolfLevel *level = open_level();
    WolfLevelObject *objects = NULL;
    uint32_t count = 0u;

    place(level, 10u, 10u, marker);
    TEST_ASSERT_TRUE(wolf_level_find_objects(level, difficulty, &objects, &count));
    TEST_ASSERT_TRUE(count <= 1u);
    if (count == 1u) {
        *object = objects[0];
    }
    free(objects);
    return count == 1u;
}

void test_level_objects_map_static_markers_to_their_sprites(void)
{
    WolfLevelObject object;

    TEST_ASSERT_TRUE(classify_one(23u, WOLF_DIFFICULTY_MEDIUM, &object));
    TEST_ASSERT_EQUAL_STRING("stat_0", object.subject);
    TEST_ASSERT_EQUAL_STRING("stat_0", object.animation);
    TEST_ASSERT_EQUAL_UINT8(0u, object.pose);
    TEST_ASSERT_EQUAL_UINT16(10u, object.tile_x);
    TEST_ASSERT_EQUAL_UINT16(10u, object.tile_y);
    TEST_ASSERT_EQUAL_UINT16(23u, object.info_marker);

    /* Marker 43 is statinfo[20], the gold key. */
    TEST_ASSERT_TRUE(classify_one(43u, WOLF_DIFFICULTY_MEDIUM, &object));
    TEST_ASSERT_EQUAL_STRING("stat_20", object.subject);
    TEST_ASSERT_TRUE(classify_one(70u, WOLF_DIFFICULTY_MEDIUM, &object));
    TEST_ASSERT_EQUAL_STRING("stat_47", object.subject);

    /* WL6 statinfo[48] is a second clip drawn with SPR_STAT_26; later entries exist only in Spear. */
    TEST_ASSERT_TRUE(classify_one(71u, WOLF_DIFFICULTY_MEDIUM, &object));
    TEST_ASSERT_EQUAL_STRING("stat_26", object.subject);
    TEST_ASSERT_FALSE(classify_one(72u, WOLF_DIFFICULTY_MEDIUM, &object));
}

void test_level_objects_ignore_player_starts_and_secret_markers(void)
{
    WolfLevelObject object;

    TEST_ASSERT_FALSE(classify_one(19u, WOLF_DIFFICULTY_HARD, &object));
    TEST_ASSERT_FALSE(classify_one(22u, WOLF_DIFFICULTY_HARD, &object));
    TEST_ASSERT_FALSE(classify_one(WOLF_LEVEL_INFO_PUSHWALL_MARKER, WOLF_DIFFICULTY_HARD, &object));
    TEST_ASSERT_FALSE(classify_one(300u, WOLF_DIFFICULTY_HARD, &object));
}

void test_level_objects_stand_or_patrol_with_the_original_facing(void)
{
    WolfLevelObject object;

    /* Standing guards: markers count east, north, west, south. */
    TEST_ASSERT_TRUE(classify_one(108u, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_EQUAL_STRING("grd", object.subject);
    TEST_ASSERT_EQUAL_STRING("grd_s", object.animation);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, TEST_PI * 0.5f, object.yaw_radians);
    TEST_ASSERT_TRUE(classify_one(109u, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, object.yaw_radians);
    TEST_ASSERT_TRUE(classify_one(110u, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, TEST_PI * 1.5f, object.yaw_radians);

    /* A patrolling guard walks: it is placed in the first stride, s_grdpath1. */
    TEST_ASSERT_TRUE(classify_one(115u, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_EQUAL_STRING("grd_w", object.animation);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, TEST_PI, object.yaw_radians);

    TEST_ASSERT_TRUE(classify_one(117u, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_EQUAL_STRING("ofc_s", object.animation);
    TEST_ASSERT_TRUE(classify_one(130u, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_EQUAL_STRING("ss_w", object.animation);
    TEST_ASSERT_TRUE(classify_one(216u, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_EQUAL_STRING("mut_s", object.animation);

    /* Dogs have no standing frames. */
    TEST_ASSERT_TRUE(classify_one(134u, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_EQUAL_STRING("dog_w", object.animation);
    TEST_ASSERT_TRUE(classify_one(141u, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_EQUAL_STRING("dog_w", object.animation);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, TEST_PI, object.yaw_radians);
}

void test_level_objects_spawn_higher_tiers_only_at_higher_difficulty(void)
{
    WolfLevelObject object;

    /* Guards repeat every 36 markers; the medium tier needs "Bring 'em on!". */
    TEST_ASSERT_FALSE(classify_one(144u, WOLF_DIFFICULTY_EASY, &object));
    TEST_ASSERT_TRUE(classify_one(144u, WOLF_DIFFICULTY_MEDIUM, &object));
    TEST_ASSERT_EQUAL_STRING("grd_s", object.animation);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, TEST_PI * 0.5f, object.yaw_radians);

    /* The hard tier needs "I am Death incarnate!". */
    TEST_ASSERT_FALSE(classify_one(187u, WOLF_DIFFICULTY_MEDIUM, &object));
    TEST_ASSERT_TRUE(classify_one(187u, WOLF_DIFFICULTY_HARD, &object));
    TEST_ASSERT_EQUAL_STRING("grd_w", object.animation);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, TEST_PI, object.yaw_radians);

    TEST_ASSERT_FALSE(classify_one(209u, WOLF_DIFFICULTY_MEDIUM, &object));
    TEST_ASSERT_TRUE(classify_one(209u, WOLF_DIFFICULTY_HARD, &object));
    TEST_ASSERT_EQUAL_STRING("dog", object.subject);

    /* Mutants repeat every 18 markers instead. */
    TEST_ASSERT_FALSE(classify_one(234u, WOLF_DIFFICULTY_EASY, &object));
    TEST_ASSERT_TRUE(classify_one(234u, WOLF_DIFFICULTY_MEDIUM, &object));
    TEST_ASSERT_EQUAL_STRING("mut_s", object.animation);
    TEST_ASSERT_FALSE(classify_one(259u, WOLF_DIFFICULTY_MEDIUM, &object));
    TEST_ASSERT_TRUE(classify_one(259u, WOLF_DIFFICULTY_HARD, &object));
    TEST_ASSERT_EQUAL_STRING("mut_w", object.animation);
}

void test_level_objects_place_bosses_ghosts_and_the_dead_guard(void)
{
    WolfLevelObject object;

    TEST_ASSERT_TRUE(classify_one(214u, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_EQUAL_STRING("boss", object.subject);
    TEST_ASSERT_EQUAL_STRING("boss_w", object.animation);
    /* Hitler is spawned inside his mech suit. */
    TEST_ASSERT_TRUE(classify_one(178u, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_EQUAL_STRING("mecha_w", object.animation);
    TEST_ASSERT_TRUE(classify_one(160u, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_EQUAL_STRING("fake_w", object.animation);
    TEST_ASSERT_TRUE(classify_one(227u, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_EQUAL_STRING("inky_w", object.animation);

    TEST_ASSERT_TRUE(classify_one(WOLF_LEVEL_DEAD_GUARD, WOLF_DIFFICULTY_BABY, &object));
    TEST_ASSERT_EQUAL_STRING("grd", object.subject);
    TEST_ASSERT_EQUAL_STRING("grd_dead", object.animation);
}

void test_level_objects_face_camera_sprites_towards_open_floor(void)
{
    WolfLevel *level = open_level();
    WolfLevelObject *objects = NULL;
    uint32_t count = 0u;

    /*
     * A barrel at (5, 30) has a wall directly to its south and north; the
     * corridor runs east for 57 cells and west for 4, so it faces east.
     */
    level->wall_tiles[29u * WOLF_LEVEL_PLANE_WIDTH + 5u] = 1u;
    level->wall_tiles[31u * WOLF_LEVEL_PLANE_WIDTH + 5u] = 1u;
    place(level, 5u, 30u, 24u);
    /* A barrel in a dead end open only to the north faces north. */
    level->wall_tiles[40u * WOLF_LEVEL_PLANE_WIDTH + 19u] = 1u;
    level->wall_tiles[40u * WOLF_LEVEL_PLANE_WIDTH + 21u] = 1u;
    level->wall_tiles[41u * WOLF_LEVEL_PLANE_WIDTH + 20u] = 1u;
    place(level, 20u, 40u, 24u);

    TEST_ASSERT_TRUE(wolf_level_find_objects(level, WOLF_DIFFICULTY_MEDIUM, &objects, &count));
    TEST_ASSERT_EQUAL_UINT32(2u, count);
    /* Objects are listed in map order, row by row. */
    TEST_ASSERT_EQUAL_UINT16(30u, objects[0].tile_y);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, TEST_PI * 0.5f, objects[0].yaw_radians);
    TEST_ASSERT_EQUAL_UINT16(40u, objects[1].tile_y);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, objects[1].yaw_radians);
    free(objects);
}

void test_level_object_block_flags_match_original_wl6_statinfo(void)
{
    static const uint16_t blocking_markers[] = {
        24u, 25u, 26u, 28u, 30u, 31u, 33u, 34u, 35u, 36u, 39u,
        40u, 41u, 45u, 58u, 59u, 60u, 62u, 63u, 68u, 69u
    };
    static const uint16_t passable_markers[] = {
        23u, /* puddle */
        32u, /* flat skeleton */
        37u, /* ceiling light */
        42u, /* relaxed skeleton */
        43u, 44u, /* keys */
        48u, 57u, 61u, 64u, 65u, 66u, 67u, 70u, /* pickups and decoration */
        71u, /* extra ammo clip */
        124u /* dead guard: SpawnDeadGuard creates an inert, non-shootable actor */
    };

    for (size_t index = 0u; index < sizeof(blocking_markers) / sizeof(blocking_markers[0]); ++index) {
        TEST_ASSERT_TRUE(wolf_level_object_marker_blocks_player(blocking_markers[index]));
    }
    for (size_t index = 0u; index < sizeof(passable_markers) / sizeof(passable_markers[0]); ++index) {
        TEST_ASSERT_FALSE(wolf_level_object_marker_blocks_player(passable_markers[index]));
    }
    TEST_ASSERT_FALSE(wolf_level_object_marker_blocks_player(0u));
    TEST_ASSERT_FALSE(wolf_level_object_marker_blocks_player(WOLF_LEVEL_INFO_PUSHWALL_MARKER));
}

void test_level_objects_reject_invalid_arguments(void)
{
    WolfLevel *level = open_level();
    WolfLevelObject *objects = (WolfLevelObject *)level;
    uint32_t count = 7u;

    TEST_ASSERT_FALSE(wolf_level_find_objects(NULL, WOLF_DIFFICULTY_MEDIUM, &objects, &count));
    TEST_ASSERT_NULL(objects);
    TEST_ASSERT_EQUAL_UINT32(0u, count);
    TEST_ASSERT_FALSE(wolf_level_find_objects(level, WOLF_DIFFICULTY_MEDIUM, NULL, &count));
    TEST_ASSERT_FALSE(wolf_level_find_objects(level, WOLF_DIFFICULTY_MEDIUM, &objects, NULL));
    TEST_ASSERT_FALSE(wolf_level_find_objects(level, (WolfDifficulty)4, &objects, &count));

    /* A map with no objects succeeds with an empty list. */
    TEST_ASSERT_TRUE(wolf_level_find_objects(level, WOLF_DIFFICULTY_MEDIUM, &objects, &count));
    TEST_ASSERT_NULL(objects);
    TEST_ASSERT_EQUAL_UINT32(0u, count);
}
