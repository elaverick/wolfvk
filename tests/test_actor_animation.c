#include "unity.h"

#include "wolfvk/actor_animation.h"
#include "wolfvk/simulation_clock.h"

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
    TEST_ASSERT_EQUAL_UINT16(20u, actors[0].state_tics_remaining);

    for (uint32_t tic = 0u; tic < 24u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
        TEST_ASSERT_EQUAL_UINT8(0u, actors[0].pose);
    }
    TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    TEST_ASSERT_EQUAL_UINT8(1u, actors[0].pose);
    TEST_ASSERT_EQUAL_STRING("grd_w", actors[0].animation);
    TEST_ASSERT_EQUAL_UINT16(15u, actors[0].state_tics_remaining);

    wolf_actor_collection_destroy(actors);
    free(level);
}


void test_actor_animation_is_independent_of_render_frame_partitioning(void)
{
    WolfLevel *level_a = calloc(1u, sizeof(*level_a));
    WolfLevel *level_b = calloc(1u, sizeof(*level_b));
    WolfActor *actors_a = NULL;
    WolfActor *actors_b = NULL;
    uint32_t count_a = 0u;
    uint32_t count_b = 0u;
    WolfSimulationClock clock_a = {0.0};
    WolfSimulationClock clock_b = {0.0};

    TEST_ASSERT_NOT_NULL(level_a);
    TEST_ASSERT_NOT_NULL(level_b);
    level_a->width = level_b->width = 4u;
    level_a->height = level_b->height = 4u;
    level_a->object_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 1u] = 112u;
    level_b->object_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 1u] = 112u;
    TEST_ASSERT_TRUE(wolf_actor_collection_create(
        level_a, WOLF_DIFFICULTY_MEDIUM, &actors_a, &count_a));
    TEST_ASSERT_TRUE(wolf_actor_collection_create(
        level_b, WOLF_DIFFICULTY_MEDIUM, &actors_b, &count_b));
    TEST_ASSERT_EQUAL_UINT32(1u, count_a);
    TEST_ASSERT_EQUAL_UINT32(1u, count_b);

    /* Both clocks receive one second, but as different render-frame partitions. */
    for (uint32_t frame = 0u; frame < 10u; ++frame) {
        uint32_t ticks = wolf_simulation_clock_advance(&clock_a, 0.1);
        for (uint32_t tic = 0u; tic < ticks; ++tic) {
            TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors_a, count_a));
        }
    }
    for (uint32_t frame = 0u; frame < 100u; ++frame) {
        uint32_t ticks = wolf_simulation_clock_advance(&clock_b, 0.01);
        for (uint32_t tic = 0u; tic < ticks; ++tic) {
            TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors_b, count_b));
        }
    }
    TEST_ASSERT_EQUAL_UINT16(actors_a[0].state_id, actors_b[0].state_id);
    TEST_ASSERT_EQUAL_UINT8(actors_a[0].pose, actors_b[0].pose);
    TEST_ASSERT_EQUAL_UINT16(actors_a[0].state_tics_remaining, actors_b[0].state_tics_remaining);

    wolf_actor_collection_destroy(actors_a);
    wolf_actor_collection_destroy(actors_b);
    free(level_a);
    free(level_b);
}


void test_actor_animation_does_not_advance_while_window_delta_is_paused(void)
{
    WolfLevel *level = calloc(1u, sizeof(*level));
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    WolfSimulationClock clock = {0.0};
    uint16_t remaining_before_pause;

    TEST_ASSERT_NOT_NULL(level);
    level->width = 4u;
    level->height = 4u;
    level->object_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 1u] = 112u;
    TEST_ASSERT_TRUE(wolf_actor_collection_create(
        level, WOLF_DIFFICULTY_MEDIUM, &actors, &actor_count));
    TEST_ASSERT_EQUAL_UINT32(1u, actor_count);

    for (uint32_t frame = 0u; frame < 3u; ++frame) {
        uint32_t ticks = wolf_simulation_clock_advance(&clock, 1.0 / 70.0);
        for (uint32_t tic = 0u; tic < ticks; ++tic) {
            TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
        }
    }
    remaining_before_pause = actors[0].state_tics_remaining;
    for (uint32_t inactive_frame = 0u; inactive_frame < 100u; ++inactive_frame) {
        TEST_ASSERT_EQUAL_UINT32(0u, wolf_simulation_clock_advance(&clock, 0.0));
    }
    TEST_ASSERT_EQUAL_UINT16(remaining_before_pause, actors[0].state_tics_remaining);

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


void test_actor_collection_uses_officer_and_ss_specific_state_chains(void)
{
    WolfLevel *level = calloc(1u, sizeof(*level));
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    static const uint16_t ss_shot_durations[] = {20u, 20u, 10u, 10u, 10u, 10u, 10u, 10u, 10u};

    TEST_ASSERT_NOT_NULL(level);
    level->width = 8u;
    level->height = 8u;
    level->object_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 1u] = 116u;
    level->object_tiles[1u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 126u;
    TEST_ASSERT_TRUE(wolf_actor_collection_create(
        level, WOLF_DIFFICULTY_HARD, &actors, &actor_count));
    TEST_ASSERT_EQUAL_UINT32(2u, actor_count);
    TEST_ASSERT_EQUAL(WOLF_ACTOR_OFFICER, actors[0].family);
    TEST_ASSERT_EQUAL_INT32(50, actors[0].health);
    TEST_ASSERT_EQUAL(WOLF_ACTOR_SS, actors[1].family);
    TEST_ASSERT_EQUAL_INT32(100, actors[1].health);

    /* Officers use the original 6/20/10-tic single-shot sequence. */
    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        &actors[0], WOLF_OFFICER_STATE_SHOOT_FIRST));
    TEST_ASSERT_EQUAL_UINT16(6u, actors[0].state_tics_remaining);
    for (uint32_t tic = 0u; tic < 6u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_OFFICER_STATE_SHOOT_ACTION, actors[0].state_id);
    TEST_ASSERT_EQUAL_UINT16(20u, actors[0].state_tics_remaining);
    for (uint32_t tic = 0u; tic < 20u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_OFFICER_STATE_SHOOT_LAST, actors[0].state_id);
    TEST_ASSERT_EQUAL_UINT16(10u, actors[0].state_tics_remaining);
    for (uint32_t tic = 0u; tic < 10u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_OFFICER_STATE_CHASE_FIRST, actors[0].state_id);

    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        &actors[0], WOLF_OFFICER_STATE_PAIN_FIRST));
    TEST_ASSERT_EQUAL_UINT16(10u, actors[0].state_tics_remaining);
    for (uint32_t tic = 0u; tic < 10u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_OFFICER_STATE_PAIN_LAST, actors[0].state_id);
    TEST_ASSERT_EQUAL_UINT16(10u, actors[0].state_tics_remaining);
    for (uint32_t tic = 0u; tic < 10u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_OFFICER_STATE_CHASE_FIRST, actors[0].state_id);

    /* Four officer death poses last 11 tics each before the persistent corpse. */
    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        &actors[0], WOLF_OFFICER_STATE_DIE_FIRST));
    for (uint16_t state = WOLF_OFFICER_STATE_DIE_FIRST;
         state <= WOLF_OFFICER_STATE_DIE_LAST; ++state) {
        TEST_ASSERT_EQUAL_UINT16(state, actors[0].state_id);
        TEST_ASSERT_EQUAL_UINT16(11u, actors[0].state_tics_remaining);
        for (uint32_t tic = 0u; tic < 11u; ++tic) {
            TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
        }
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_OFFICER_STATE_DEAD, actors[0].state_id);

    /* SS use a nine-state burst, including four 10-tic action frames. */
    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        &actors[1], WOLF_SS_STATE_SHOOT_FIRST));
    for (uint16_t state = WOLF_SS_STATE_SHOOT_FIRST;
         state <= WOLF_SS_STATE_SHOOT_LAST; ++state) {
        uint16_t duration = ss_shot_durations[state - WOLF_SS_STATE_SHOOT_FIRST];
        TEST_ASSERT_EQUAL_UINT16(state, actors[1].state_id);
        TEST_ASSERT_EQUAL_UINT16(duration, actors[1].state_tics_remaining);
        for (uint32_t tic = 0u; tic < duration; ++tic) {
            TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
        }
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_SS_STATE_CHASE_FIRST, actors[1].state_id);

    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        &actors[1], WOLF_SS_STATE_PAIN_FIRST));
    TEST_ASSERT_EQUAL_UINT16(10u, actors[1].state_tics_remaining);
    for (uint32_t tic = 0u; tic < 10u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_SS_STATE_PAIN_LAST, actors[1].state_id);
    TEST_ASSERT_EQUAL_UINT16(10u, actors[1].state_tics_remaining);
    for (uint32_t tic = 0u; tic < 10u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_SS_STATE_CHASE_FIRST, actors[1].state_id);

    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        &actors[1], WOLF_SS_STATE_DIE_FIRST));
    for (uint16_t state = WOLF_SS_STATE_DIE_FIRST;
         state <= WOLF_SS_STATE_DIE_LAST; ++state) {
        TEST_ASSERT_EQUAL_UINT16(state, actors[1].state_id);
        TEST_ASSERT_EQUAL_UINT16(15u, actors[1].state_tics_remaining);
        for (uint32_t tic = 0u; tic < 15u; ++tic) {
            TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
        }
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_SS_STATE_DEAD, actors[1].state_id);
    TEST_ASSERT_EQUAL_UINT16(0u, actors[1].state_tics_remaining);

    wolf_actor_collection_destroy(actors);
    free(level);
}
