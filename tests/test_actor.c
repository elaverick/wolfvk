#include "unity.h"

#include "wolfvk/actor.h"

#include <math.h>
#include <string.h>

typedef struct ActorCallbackCounts {
    uint32_t think_calls;
    uint32_t enter_calls;
} ActorCallbackCounts;

static void count_think(WolfActor *actor, void *context)
{
    ActorCallbackCounts *counts = context;
    (void)actor;
    ++counts->think_calls;
}

static void count_enter(WolfActor *actor, void *context)
{
    ActorCallbackCounts *counts = context;
    (void)actor;
    ++counts->enter_calls;
}

static const WolfActorStateDefinition test_states[] = {
    {"grd_s", 0u, 0u, WOLF_ACTOR_STATE_NONE, count_think, count_enter},
    {"grd_w", 2u, 3u, 2u, count_think, count_enter},
    {"grd_pain", 1u, 2u, 3u, NULL, count_enter},
    {"grd_dead", 0u, 0u, WOLF_ACTOR_STATE_NONE, NULL, count_enter}
};

void test_actor_initialization_copies_spawn_data_and_enters_state(void)
{
    WolfActor actor;
    ActorCallbackCounts counts = {0u, 0u};

    TEST_ASSERT_TRUE(wolf_actor_initialize(
        &actor, 17u, WOLF_ACTOR_GUARD, 29u, 57u, -4.0f, 12.0f, 1.25f,
        25, test_states, 4u, 0u, &counts));
    TEST_ASSERT_EQUAL_UINT32(17u, actor.id);
    TEST_ASSERT_EQUAL(WOLF_ACTOR_GUARD, actor.family);
    TEST_ASSERT_EQUAL_UINT16(29u, actor.tile_x);
    TEST_ASSERT_EQUAL_UINT16(57u, actor.tile_y);
    TEST_ASSERT_EQUAL_INT32(25, actor.health);
    TEST_ASSERT_TRUE(actor.active);
    TEST_ASSERT_TRUE(actor.shootable);
    TEST_ASSERT_EQUAL_UINT16(0u, actor.state_id);
    TEST_ASSERT_EQUAL_STRING("grd_s", actor.animation);
    TEST_ASSERT_EQUAL_UINT8(0u, actor.pose);
    TEST_ASSERT_EQUAL_UINT32(1u, counts.enter_calls);
}

void test_actor_state_uses_exact_tic_duration_and_enters_next_state_once(void)
{
    WolfActor actor;
    ActorCallbackCounts counts = {0u, 0u};

    TEST_ASSERT_TRUE(wolf_actor_initialize(
        &actor, 1u, WOLF_ACTOR_GUARD, 10u, 10u, 0.0f, 0.0f, 0.0f,
        25, test_states, 4u, 1u, &counts));
    TEST_ASSERT_EQUAL_UINT16(3u, actor.state_tics_remaining);

    TEST_ASSERT_TRUE(wolf_actor_update_tic(&actor, test_states, 4u, &counts));
    TEST_ASSERT_EQUAL_UINT16(1u, actor.state_id);
    TEST_ASSERT_EQUAL_UINT16(2u, actor.state_tics_remaining);
    TEST_ASSERT_EQUAL_UINT32(1u, counts.think_calls);
    TEST_ASSERT_EQUAL_UINT32(1u, counts.enter_calls);

    TEST_ASSERT_TRUE(wolf_actor_update_tic(&actor, test_states, 4u, &counts));
    TEST_ASSERT_EQUAL_UINT16(1u, actor.state_id);
    TEST_ASSERT_TRUE(wolf_actor_update_tic(&actor, test_states, 4u, &counts));
    TEST_ASSERT_EQUAL_UINT16(2u, actor.state_id);
    TEST_ASSERT_EQUAL_STRING("grd_pain", actor.animation);
    TEST_ASSERT_EQUAL_UINT16(2u, actor.state_tics_remaining);
    TEST_ASSERT_EQUAL_UINT32(2u, counts.enter_calls);
}

void test_actor_persistent_state_thinks_once_per_simulation_tic(void)
{
    WolfActor actor;
    ActorCallbackCounts counts = {0u, 0u};

    TEST_ASSERT_TRUE(wolf_actor_initialize(
        &actor, 1u, WOLF_ACTOR_GUARD, 1u, 1u, 0.0f, 0.0f, 0.0f,
        25, test_states, 4u, 0u, &counts));
    for (uint32_t tick = 0u; tick < 70u; ++tick) {
        TEST_ASSERT_TRUE(wolf_actor_update_tic(&actor, test_states, 4u, &counts));
    }
    TEST_ASSERT_EQUAL_UINT32(70u, counts.think_calls);
    TEST_ASSERT_EQUAL_UINT32(1u, counts.enter_calls);
    TEST_ASSERT_EQUAL_UINT16(0u, actor.state_id);
}

void test_actor_one_shot_state_becomes_inactive_after_its_final_pose(void)
{
    WolfActor actor;
    ActorCallbackCounts counts = {0u, 0u};

    TEST_ASSERT_TRUE(wolf_actor_initialize(
        &actor, 1u, WOLF_ACTOR_GUARD, 1u, 1u, 0.0f, 0.0f, 0.0f,
        25, test_states, 4u, 2u, &counts));
    TEST_ASSERT_TRUE(wolf_actor_update_tic(&actor, test_states, 4u, &counts));
    TEST_ASSERT_TRUE(actor.active);
    TEST_ASSERT_TRUE(wolf_actor_update_tic(&actor, test_states, 4u, &counts));
    TEST_ASSERT_EQUAL_UINT16(3u, actor.state_id);
    TEST_ASSERT_TRUE(wolf_actor_update_tic(&actor, test_states, 4u, &counts));
    TEST_ASSERT_TRUE(actor.active);
    TEST_ASSERT_EQUAL_UINT16(3u, actor.state_id);
    TEST_ASSERT_EQUAL_UINT32(3u, counts.enter_calls);
}

void test_actor_state_rejects_invalid_definitions_without_corrupting_instance(void)
{
    WolfActor actor;
    ActorCallbackCounts counts = {0u, 0u};

    TEST_ASSERT_TRUE(wolf_actor_initialize(
        &actor, 1u, WOLF_ACTOR_GUARD, 1u, 1u, 0.0f, 0.0f, 0.0f,
        25, test_states, 4u, 0u, &counts));
    TEST_ASSERT_FALSE(wolf_actor_set_state(&actor, test_states, 4u, 99u, &counts));
    TEST_ASSERT_EQUAL_UINT16(0u, actor.state_id);
    TEST_ASSERT_EQUAL_STRING("grd_s", actor.animation);
    TEST_ASSERT_FALSE(wolf_actor_initialize(
        &actor, 1u, WOLF_ACTOR_FAMILY_COUNT, 1u, 1u, 0.0f, 0.0f, 0.0f,
        25, test_states, 4u, 0u, &counts));
    TEST_ASSERT_FALSE(wolf_actor_initialize(
        &actor, 1u, WOLF_ACTOR_GUARD, 1u, 1u, NAN, 0.0f, 0.0f,
        25, test_states, 4u, 0u, &counts));
}

void test_actor_update_rejects_inactive_or_invalid_state(void)
{
    WolfActor actor;
    ActorCallbackCounts counts = {0u, 0u};

    TEST_ASSERT_TRUE(wolf_actor_initialize(
        &actor, 1u, WOLF_ACTOR_GUARD, 1u, 1u, 0.0f, 0.0f, 0.0f,
        25, test_states, 4u, 3u, &counts));
    actor.active = false;
    TEST_ASSERT_FALSE(wolf_actor_update_tic(&actor, test_states, 4u, &counts));
    actor.active = true;
    actor.state_id = 99u;
    TEST_ASSERT_FALSE(wolf_actor_update_tic(&actor, test_states, 4u, &counts));
}
