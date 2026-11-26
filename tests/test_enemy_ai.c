#include "unity.h"

#include "wolfvk/actor_animation.h"
#include "wolfvk/actor_movement.h"
#include "wolfvk/enemy_ai.h"
#include "wolfvk/game_world.h"
#include "wolfvk/world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static float tile_centre(uint16_t tile, uint16_t extent)
{
    return ((float)tile + 0.5f - (float)extent * 0.5f) * WOLF_WORLD_TILE_SIZE;
}

static bool create_guard_world(
    WolfWorldState *world,
    WolfActor **actors,
    uint32_t *actor_count,
    uint16_t guard_marker)
{
    WolfLevel level = {0};

    level.width = 8u;
    level.height = 8u;
    level.object_tiles[4u * WOLF_LEVEL_PLANE_WIDTH + 4u] = guard_marker;
    if (!wolf_world_state_init(world, &level, 0u)) {
        return false;
    }
    return wolf_actor_collection_create(
        &world->level, WOLF_DIFFICULTY_HARD, actors, actor_count);
}

static bool update_one_tic(
    WolfActor *actors,
    uint32_t actor_count,
    WolfWorldState *world,
    float player_x,
    float player_z,
    float player_yaw,
    WolfDifficulty difficulty,
    bool player_moving,
    bool noise,
    WolfEnemyAIEvents *events)
{
    return wolf_enemy_ai_update_tic(
            actors, actor_count, world, player_x, player_z, player_yaw,
            difficulty, player_moving, noise, events) &&
        wolf_actor_movement_update_tic(actors, actor_count, world, player_x, player_z) &&
        wolf_actor_collection_update_tic(actors, actor_count);
}

static void advance_until_alert(
    WolfActor *actors,
    uint32_t actor_count,
    WolfWorldState *world,
    float player_x,
    float player_z,
    WolfEnemyAIEvents *events)
{
    for (uint32_t tic = 0u; tic < 80u && !actors[0].attack_mode; ++tic) {
        TEST_ASSERT_TRUE(update_one_tic(
            actors, actor_count, world, player_x, player_z,
            1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, events));
    }
}

void test_guard_perception_is_blocked_by_walls_and_alerts_after_reaction_delay(void)
{
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    WolfEnemyAIEvents events = {0};
    float player_x = tile_centre(2u, 8u);
    float player_z = tile_centre(4u, 8u);

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 110u));
    TEST_ASSERT_EQUAL_UINT32(1u, actor_count);
    TEST_ASSERT_EQUAL_UINT16(WOLF_GUARD_STATE_STAND, actors[0].state_id);
    TEST_ASSERT_EQUAL_INT(WOLF_ACTOR_DIRECTION_WEST, actors[0].movement_direction);

    world.level.wall_tiles[4u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 1u;
    for (uint32_t tic = 0u; tic < 80u; ++tic) {
        TEST_ASSERT_TRUE(update_one_tic(
            actors, actor_count, &world, player_x, player_z,
            1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, &events));
    }
    TEST_ASSERT_FALSE(actors[0].attack_mode);
    TEST_ASSERT_EQUAL_UINT32(0u, events.guard_alerts);

    world.level.wall_tiles[4u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 0u;
    advance_until_alert(actors, actor_count, &world, player_x, player_z, &events);
    TEST_ASSERT_TRUE(actors[0].attack_mode);
    TEST_ASSERT_EQUAL_UINT16(WOLF_GUARD_STATE_CHASE_FIRST, actors[0].state_id);
    TEST_ASSERT_EQUAL_UINT32(1u, events.guard_alerts);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, WOLF_ACTOR_PATROL_SPEED_WORLD_PER_TIC * 3.0f,
        actors[0].movement_speed_world_per_tic);

    wolf_actor_collection_destroy(actors);
}

void test_guard_chase_uses_player_direction_and_moves_at_alert_speed(void)
{
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    WolfEnemyAIEvents events = {0};
    float player_x = tile_centre(2u, 8u);
    float player_z = tile_centre(4u, 8u);

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 110u));
    advance_until_alert(actors, actor_count, &world, player_x, player_z, &events);
    for (uint32_t tic = 0u; tic < 55u; ++tic) {
        TEST_ASSERT_TRUE(update_one_tic(
            actors, actor_count, &world, player_x, player_z,
            1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, &events));
    }
    TEST_ASSERT_TRUE(actors[0].tile_x < 4u);
    TEST_ASSERT_EQUAL_INT(WOLF_ACTOR_DIRECTION_WEST, actors[0].movement_direction);
    TEST_ASSERT_TRUE(actors[0].position_x < tile_centre(4u, 8u));

    wolf_actor_collection_destroy(actors);
}

void test_guard_shooting_state_damages_player_and_emits_fire_event(void)
{
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    WolfEnemyAIEvents events = {0};
    float player_x = tile_centre(3u, 8u);
    float player_z = tile_centre(4u, 8u);
    uint32_t total_shots = 0u;

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 110u));
    advance_until_alert(actors, actor_count, &world, player_x, player_z, &events);
    for (uint32_t tic = 0u; tic < 180u; ++tic) {
        TEST_ASSERT_TRUE(update_one_tic(
            actors, actor_count, &world, player_x, player_z,
            1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, &events));
        total_shots += events.guard_shots;
    }
    TEST_ASSERT_TRUE(total_shots > 0u);
    TEST_ASSERT_TRUE(world.player.health < WOLF_PLAYER_MAX_HEALTH);

    wolf_actor_collection_destroy(actors);
}

void test_player_gunfire_damages_guard_and_death_awards_score_and_drops_clip(void)
{
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    float player_x = tile_centre(2u, 8u);
    float player_z = tile_centre(4u, 8u);

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 108u));
    for (uint32_t shot = 0u; shot < 100u && actors[0].shootable; ++shot) {
        TEST_ASSERT_TRUE(wolf_enemy_ai_player_attack(
            actors, actor_count, &world, player_x, player_z,
            1.57079632679f, WOLF_WEAPON_PISTOL));
    }
    TEST_ASSERT_FALSE(actors[0].shootable);
    TEST_ASSERT_EQUAL_INT32(100, world.player.score);
    TEST_ASSERT_EQUAL_UINT16(WOLF_GUARD_STATE_DIE_FIRST, actors[0].state_id);

    {
        bool clip_found = false;
        for (size_t index = 0u; index < WOLF_LEVEL_PLANE_WORDS; ++index) {
            if (world.level.object_tiles[index] == WOLF_LEVEL_STATIC_EXTRA_CLIP) {
                clip_found = true;
                break;
            }
        }
        TEST_ASSERT_TRUE(clip_found);
    }

    for (uint32_t tic = 0u; tic < 45u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_GUARD_STATE_DEAD, actors[0].state_id);
    TEST_ASSERT_TRUE(actors[0].active);

    wolf_actor_collection_destroy(actors);
}

void test_player_weapon_cannot_damage_guard_through_a_wall(void)
{
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    int32_t initial_health;

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 108u));
    world.level.wall_tiles[4u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 1u;
    initial_health = actors[0].health;
    for (uint32_t shot = 0u; shot < 10u; ++shot) {
        TEST_ASSERT_TRUE(wolf_enemy_ai_player_attack(
            actors, actor_count, &world,
            tile_centre(2u, 8u), tile_centre(4u, 8u),
            1.57079632679f, WOLF_WEAPON_PISTOL));
    }
    TEST_ASSERT_EQUAL_INT32(initial_health, actors[0].health);

    wolf_actor_collection_destroy(actors);
}
