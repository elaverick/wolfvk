#include "unity.h"

#include "wolfvk/actor_animation.h"

#include <stdlib.h>
#include <string.h>

static WolfLevel *make_actor_level(void)
{
    WolfLevel *level = calloc(1u, sizeof(*level));

    if (level != NULL) {
        level->width = 8u;
        level->height = 8u;
        level->object_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 1u] = 112u;
        level->object_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 120u;
        level->object_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 130u;
        level->object_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 4u] = 220u;
    }
    return level;
}

void test_actor_collection_spawns_four_humanoid_families_with_original_health(void)
{
    WolfLevel *level = make_actor_level();
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;

    TEST_ASSERT_NOT_NULL(level);
    TEST_ASSERT_TRUE(wolf_actor_collection_create(
        level, WOLF_DIFFICULTY_HARD, &actors, &actor_count));
    TEST_ASSERT_EQUAL_UINT32(4u, actor_count);
    TEST_ASSERT_EQUAL(WOLF_ACTOR_GUARD, actors[0].family);
    TEST_ASSERT_EQUAL_INT32(25, actors[0].health);
    TEST_ASSERT_EQUAL(WOLF_ACTOR_OFFICER, actors[1].family);
    TEST_ASSERT_EQUAL_INT32(50, actors[1].health);
    TEST_ASSERT_EQUAL(WOLF_ACTOR_SS, actors[2].family);
    TEST_ASSERT_EQUAL_INT32(100, actors[2].health);
    TEST_ASSERT_EQUAL(WOLF_ACTOR_MUTANT, actors[3].family);
    TEST_ASSERT_EQUAL_INT32(65, actors[3].health);
    TEST_ASSERT_EQUAL_STRING("grd_w", actors[0].animation);
    TEST_ASSERT_EQUAL_UINT8(0u, actors[0].pose);

    wolf_actor_collection_destroy(actors);
    free(level);
}

void test_actor_collection_uses_standing_state_and_mutant_difficulty_health(void)
{
    WolfLevel *level = calloc(1u, sizeof(*level));
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;

    TEST_ASSERT_NOT_NULL(level);
    level->width = 4u;
    level->height = 4u;
    level->object_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 1u] = 216u;
    TEST_ASSERT_TRUE(wolf_actor_collection_create(
        level, WOLF_DIFFICULTY_BABY, &actors, &actor_count));
    TEST_ASSERT_EQUAL_UINT32(1u, actor_count);
    TEST_ASSERT_EQUAL_INT32(45, actors[0].health);
    TEST_ASSERT_EQUAL_STRING("mut_s", actors[0].animation);
    TEST_ASSERT_EQUAL_UINT16(0u, actors[0].state_id);
    TEST_ASSERT_EQUAL_UINT16(0u, actors[0].state_tics_remaining);

    wolf_actor_collection_destroy(actors);
    free(level);
}

void test_actor_collection_advances_patrol_poses_on_exact_original_tics(void)
{
    WolfLevel *level = calloc(1u, sizeof(*level));
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;

    TEST_ASSERT_NOT_NULL(level);
    level->width = 4u;
    level->height = 4u;
    level->object_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 1u] = 112u;
    TEST_ASSERT_TRUE(wolf_actor_collection_create(
        level, WOLF_DIFFICULTY_MEDIUM, &actors, &actor_count));
    TEST_ASSERT_EQUAL_UINT32(1u, actor_count);
    TEST_ASSERT_EQUAL_UINT16(10u, actors[0].state_tics_remaining);

    for (uint32_t tic = 0u; tic < 12u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
        TEST_ASSERT_EQUAL_UINT8(0u, actors[0].pose);
    }
    TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    TEST_ASSERT_EQUAL_UINT8(1u, actors[0].pose);
    TEST_ASSERT_EQUAL_STRING("grd_w", actors[0].animation);
    TEST_ASSERT_EQUAL_UINT16(3u, actors[0].state_tics_remaining);

    wolf_actor_collection_destroy(actors);
    free(level);
}

void test_actor_collection_handles_empty_levels_and_invalid_arguments(void)
{
    WolfLevel level = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 99u;

    level.width = 4u;
    level.height = 4u;
    TEST_ASSERT_TRUE(wolf_actor_collection_create(
        &level, WOLF_DIFFICULTY_MEDIUM, &actors, &actor_count));
    TEST_ASSERT_NULL(actors);
    TEST_ASSERT_EQUAL_UINT32(0u, actor_count);
    TEST_ASSERT_FALSE(wolf_actor_collection_create(
        NULL, WOLF_DIFFICULTY_MEDIUM, &actors, &actor_count));
    TEST_ASSERT_FALSE(wolf_actor_collection_create(
        &level, WOLF_DIFFICULTY_MEDIUM, NULL, &actor_count));
    TEST_ASSERT_FALSE(wolf_actor_collection_update_tic(NULL, 1u));
    wolf_actor_collection_destroy(NULL);
}
