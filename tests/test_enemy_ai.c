#include "unity.h"

#include "wolfvk/actor_animation.h"
#include "wolfvk/actor_movement.h"
#include "wolfvk/enemy_ai.h"
#include "wolfvk/game_world.h"
#include "wolfvk/simulation_clock.h"
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

/*
 * SightPlayer's reaction windows differ by enemy class in the original game.
 * Trigger perception once and inspect the queued delay before it expires.
 */
void test_enemy_reaction_delays_match_original_class_specific_ranges(void)
{
    static const struct {
        uint16_t marker;
        uint16_t maximum_delay;
    } cases[] = {
        {108u, 64u},  /* Guard: 1 + random / 4. */
        {118u, 2u},   /* Officer: fixed two tics. */
        {128u, 43u},  /* SS: 1 + random / 6. */
        {216u, 43u}   /* Mutant: 1 + random / 6. */
    };
    const float player_x = tile_centre(3u, 8u);
    const float player_z = tile_centre(4u, 8u);

    for (size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        WolfWorldState world = {0};
        WolfActor *actors = NULL;
        uint32_t actor_count = 0u;

        TEST_ASSERT_TRUE(create_guard_world(
            &world, &actors, &actor_count, cases[index].marker));
        TEST_ASSERT_EQUAL_UINT32(1u, actor_count);
        actors[0].ai_random_state = 0x12345678u;

        TEST_ASSERT_TRUE(wolf_enemy_ai_update_tic(
            actors, actor_count, &world, player_x, player_z,
            1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, NULL));

        TEST_ASSERT_FALSE(actors[0].attack_mode);
        TEST_ASSERT_TRUE(actors[0].ai_reaction_tics >= 1u);
        TEST_ASSERT_TRUE(actors[0].ai_reaction_tics <= cases[index].maximum_delay);
        if (cases[index].marker == 118u) {
            TEST_ASSERT_EQUAL_UINT16(2u, actors[0].ai_reaction_tics);
        }

        wolf_actor_collection_destroy(actors);
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
    TEST_ASSERT_FALSE(actors[0].attack_mode);

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
    for (uint32_t tic = 0u; tic < 8u; ++tic) {
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


void test_lethal_enemy_attack_reports_the_killer_position(void)
{
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    WolfEnemyAIEvents events = {0};
    float player_x = tile_centre(3u, 8u);
    float player_z = tile_centre(4u, 8u);
    bool lethal_attack_reported = false;

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 110u));
    world.player.health = 1;
    advance_until_alert(actors, actor_count, &world, player_x, player_z, &events);
    for (uint32_t tic = 0u; tic < 400u && world.player.health > 0; ++tic) {
        TEST_ASSERT_TRUE(update_one_tic(
            actors, actor_count, &world, player_x, player_z,
            1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, &events));
        lethal_attack_reported = lethal_attack_reported || events.player_killed;
    }
    TEST_ASSERT_EQUAL_INT32(0, world.player.health);
    TEST_ASSERT_TRUE(lethal_attack_reported);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, actors[0].position_x, events.killer_position_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, actors[0].position_z, events.killer_position_z);

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
            1.57079632679f, WOLF_WEAPON_PISTOL, NULL));
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


void test_guard_drop_uses_physical_position_when_destination_is_reserved(void)
{
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    const size_t physical_tile = 4u * WOLF_LEVEL_PLANE_WIDTH + 4u;
    const size_t reserved_tile = 4u * WOLF_LEVEL_PLANE_WIDTH + 5u;
    const float player_x = tile_centre(2u, 8u);
    const float player_z = tile_centre(4u, 8u);

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 108u));

    /*
     * Movement reserves the destination tile before the actor's body has
     * crossed the tile boundary. Simulate that in-flight state at death.
     */
    actors[0].tile_x = 5u;
    actors[0].position_x = tile_centre(4u, 8u);
    actors[0].position_z = tile_centre(4u, 8u);

    for (uint32_t shot = 0u; shot < 100u && actors[0].shootable; ++shot) {
        TEST_ASSERT_TRUE(wolf_enemy_ai_player_attack(
            actors, actor_count, &world, player_x, player_z,
            1.57079632679f, WOLF_WEAPON_PISTOL, NULL));
    }

    TEST_ASSERT_FALSE(actors[0].shootable);
    TEST_ASSERT_EQUAL_UINT16(
        WOLF_LEVEL_STATIC_EXTRA_CLIP, world.level.object_tiles[physical_tile]);
    TEST_ASSERT_NOT_EQUAL(
        WOLF_LEVEL_STATIC_EXTRA_CLIP, world.level.object_tiles[reserved_tile]);

    wolf_actor_collection_destroy(actors);
}



void test_guard_facing_direction_gates_detection_until_player_is_in_front(void)
{
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    WolfEnemyAIEvents events = {0};

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 108u));
    for (uint32_t tic = 0u; tic < 80u; ++tic) {
        TEST_ASSERT_TRUE(update_one_tic(
            actors, actor_count, &world,
            tile_centre(2u, 8u), tile_centre(4u, 8u),
            0.0f, WOLF_DIFFICULTY_HARD, false, false, &events));
    }
    TEST_ASSERT_FALSE(actors[0].attack_mode);

    advance_until_alert(
        actors, actor_count, &world,
        tile_centre(6u, 8u), tile_centre(4u, 8u), &events);
    TEST_ASSERT_TRUE(actors[0].attack_mode);
    TEST_ASSERT_EQUAL_UINT32(1u, events.guard_alerts);

    wolf_actor_collection_destroy(actors);
}

void test_guard_revalidates_line_of_sight_at_the_shot_action_frame(void)
{
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    WolfEnemyAIEvents events = {0};
    int32_t initial_health;

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 110u));
    world.level.wall_tiles[4u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 1u;
    actors[0].attack_mode = true;
    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        &actors[0], WOLF_GUARD_STATE_SHOOT_ACTION));
    initial_health = world.player.health;

    TEST_ASSERT_TRUE(wolf_enemy_ai_update_tic(
        actors, actor_count, &world,
        tile_centre(2u, 8u), tile_centre(4u, 8u),
        1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, &events));
    TEST_ASSERT_EQUAL_UINT32(0u, events.guard_shots);
    TEST_ASSERT_EQUAL_INT32(initial_health, world.player.health);
    TEST_ASSERT_TRUE(actors[0].ai_action_fired);

    wolf_actor_collection_destroy(actors);
}

void test_first_player_hit_alerts_guard_and_enters_pain_state(void)
{
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 108u));
    TEST_ASSERT_TRUE(wolf_enemy_ai_player_attack(
        actors, actor_count, &world,
        tile_centre(2u, 8u), tile_centre(4u, 8u),
        1.57079632679f, WOLF_WEAPON_PISTOL, NULL));
    TEST_ASSERT_TRUE(actors[0].attack_mode);
    TEST_ASSERT_TRUE(actors[0].health > 0);
    TEST_ASSERT_TRUE(actors[0].health < 25);
    TEST_ASSERT_TRUE(
        actors[0].state_id == WOLF_GUARD_STATE_PAIN_FIRST ||
        actors[0].state_id == WOLF_GUARD_STATE_PAIN_LAST);

    wolf_actor_collection_destroy(actors);
}

void test_guard_line_of_sight_uses_the_sliding_door_aperture(void)
{
    WolfLevel level = {0};
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    WolfEnemyAIEvents events = {0};
    float player_x = tile_centre(2u, 8u);
    float player_z = tile_centre(4u, 8u);

    level.width = 8u;
    level.height = 8u;
    level.wall_tiles[4u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 90u;
    level.object_tiles[4u * WOLF_LEVEL_PLANE_WIDTH + 4u] = 110u;
    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 8u));
    TEST_ASSERT_TRUE(wolf_actor_collection_create(
        &world.level, WOLF_DIFFICULTY_HARD, &actors, &actor_count));

    world.doors[0].action = WOLF_DOOR_OPENING;
    world.doors[0].position = 16384u;
    for (uint32_t tic = 0u; tic < 80u; ++tic) {
        TEST_ASSERT_TRUE(update_one_tic(
            actors, actor_count, &world, player_x, player_z,
            1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, &events));
    }
    TEST_ASSERT_FALSE(actors[0].attack_mode);

    world.doors[0].position = 49152u;
    advance_until_alert(actors, actor_count, &world, player_x, player_z, &events);
    TEST_ASSERT_TRUE(actors[0].attack_mode);
    TEST_ASSERT_EQUAL_UINT32(1u, events.guard_alerts);

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
            1.57079632679f, WOLF_WEAPON_PISTOL, NULL));
    }
    TEST_ASSERT_EQUAL_INT32(initial_health, actors[0].health);

    wolf_actor_collection_destroy(actors);
}


void test_officer_uses_original_alert_speed_and_single_shot_action(void)
{
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    WolfEnemyAIEvents events = {0};
    float player_x = tile_centre(2u, 8u);
    float player_z = tile_centre(4u, 8u);

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 118u));
    actors[0].health = 1000;
    advance_until_alert(actors, actor_count, &world, player_x, player_z, &events);
    TEST_ASSERT_TRUE(actors[0].attack_mode);
    TEST_ASSERT_EQUAL_UINT16(WOLF_OFFICER_STATE_CHASE_FIRST, actors[0].state_id);
    TEST_ASSERT_EQUAL_UINT32(1u, events.officer_alerts);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, WOLF_ACTOR_PATROL_SPEED_WORLD_PER_TIC * 5.0f,
        actors[0].movement_speed_world_per_tic);

    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        &actors[0], WOLF_OFFICER_STATE_SHOOT_ACTION));
    TEST_ASSERT_TRUE(wolf_enemy_ai_update_tic(
        actors, actor_count, &world, player_x, player_z,
        1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, &events));
    TEST_ASSERT_EQUAL_UINT32(1u, events.officer_shots);
    TEST_ASSERT_EQUAL_UINT32(0u, events.ss_shots);

    wolf_actor_collection_destroy(actors);
}

void test_ss_uses_original_alert_speed_and_four_shot_burst(void)
{
    static const uint16_t action_states[] = {
        WOLF_SS_STATE_SHOOT_ACTION_1,
        WOLF_SS_STATE_SHOOT_ACTION_2,
        WOLF_SS_STATE_SHOOT_ACTION_3,
        WOLF_SS_STATE_SHOOT_ACTION_4
    };
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    WolfEnemyAIEvents events = {0};
    float player_x = tile_centre(2u, 8u);
    float player_z = tile_centre(4u, 8u);

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 128u));
    actors[0].health = 1000;
    advance_until_alert(actors, actor_count, &world, player_x, player_z, &events);
    TEST_ASSERT_TRUE(actors[0].attack_mode);
    TEST_ASSERT_EQUAL_UINT16(WOLF_SS_STATE_CHASE_FIRST, actors[0].state_id);
    TEST_ASSERT_EQUAL_UINT32(1u, events.ss_alerts);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, WOLF_ACTOR_PATROL_SPEED_WORLD_PER_TIC * 4.0f,
        actors[0].movement_speed_world_per_tic);

    for (size_t index = 0u; index < sizeof(action_states) / sizeof(action_states[0]); ++index) {
        TEST_ASSERT_TRUE(wolf_actor_collection_set_state(&actors[0], action_states[index]));
        TEST_ASSERT_TRUE(wolf_enemy_ai_update_tic(
            actors, actor_count, &world, player_x, player_z,
            1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, &events));
        TEST_ASSERT_EQUAL_UINT32(1u, events.ss_shots);
    }

    wolf_actor_collection_destroy(actors);
}

void test_officer_and_ss_deaths_award_original_points_and_drops(void)
{
    WolfWorldState officer_world = {0};
    WolfWorldState ss_world = {0};
    WolfActor *officer = NULL;
    WolfActor *ss = NULL;
    uint32_t officer_count = 0u;
    uint32_t ss_count = 0u;
    WolfEnemyAIEvents events = {0};
    float player_x = tile_centre(2u, 8u);
    float player_z = tile_centre(4u, 8u);
    bool officer_clip_found = false;
    bool ss_weapon_found = false;

    TEST_ASSERT_TRUE(create_guard_world(
        &officer_world, &officer, &officer_count, 118u));
    for (uint32_t shot = 0u; shot < 500u && officer[0].shootable; ++shot) {
        TEST_ASSERT_TRUE(wolf_enemy_ai_player_attack(
            officer, officer_count, &officer_world, player_x, player_z,
            1.57079632679f, WOLF_WEAPON_PISTOL, &events));
    }
    TEST_ASSERT_FALSE(officer[0].shootable);
    TEST_ASSERT_EQUAL_INT32(400, officer_world.player.score);
    TEST_ASSERT_EQUAL_UINT16(WOLF_OFFICER_STATE_DIE_FIRST, officer[0].state_id);
    TEST_ASSERT_EQUAL_UINT32(1u, events.officer_deaths);
    for (size_t index = 0u; index < WOLF_LEVEL_PLANE_WORDS; ++index) {
        officer_clip_found = officer_clip_found ||
            officer_world.level.object_tiles[index] == WOLF_LEVEL_STATIC_EXTRA_CLIP;
    }
    TEST_ASSERT_TRUE(officer_clip_found);

    TEST_ASSERT_TRUE(create_guard_world(&ss_world, &ss, &ss_count, 128u));
    for (uint32_t shot = 0u; shot < 1000u && ss[0].shootable; ++shot) {
        TEST_ASSERT_TRUE(wolf_enemy_ai_player_attack(
            ss, ss_count, &ss_world, player_x, player_z,
            1.57079632679f, WOLF_WEAPON_PISTOL, &events));
    }
    TEST_ASSERT_FALSE(ss[0].shootable);
    TEST_ASSERT_EQUAL_INT32(500, ss_world.player.score);
    TEST_ASSERT_EQUAL_UINT16(WOLF_SS_STATE_DIE_FIRST, ss[0].state_id);
    TEST_ASSERT_EQUAL_UINT32(1u, events.ss_deaths);
    for (size_t index = 0u; index < WOLF_LEVEL_PLANE_WORDS; ++index) {
        ss_weapon_found = ss_weapon_found ||
            ss_world.level.object_tiles[index] == 50u;
    }
    TEST_ASSERT_TRUE(ss_weapon_found);

    wolf_actor_collection_destroy(officer);
    wolf_actor_collection_destroy(ss);
}


void test_officer_and_ss_first_hits_alert_and_enter_their_pain_states(void)
{
    WolfWorldState officer_world = {0};
    WolfWorldState ss_world = {0};
    WolfActor *officer = NULL;
    WolfActor *ss = NULL;
    uint32_t officer_count = 0u;
    uint32_t ss_count = 0u;
    WolfEnemyAIEvents officer_events = {0};
    WolfEnemyAIEvents ss_events = {0};
    float player_x = tile_centre(2u, 8u);
    float player_z = tile_centre(4u, 8u);

    TEST_ASSERT_TRUE(create_guard_world(
        &officer_world, &officer, &officer_count, 118u));
    officer[0].health = 1000;
    officer[0].ai_random_state = 12288u;
    TEST_ASSERT_TRUE(wolf_enemy_ai_player_attack(
        officer, officer_count, &officer_world, player_x, player_z,
        1.57079632679f, WOLF_WEAPON_PISTOL, &officer_events));
    TEST_ASSERT_TRUE(officer[0].attack_mode);
    TEST_ASSERT_TRUE(officer[0].health < 1000);
    TEST_ASSERT_TRUE(officer[0].state_id >= WOLF_OFFICER_STATE_PAIN_FIRST);
    TEST_ASSERT_TRUE(officer[0].state_id <= WOLF_OFFICER_STATE_PAIN_LAST);
    TEST_ASSERT_EQUAL_UINT32(1u, officer_events.officer_alerts);

    TEST_ASSERT_TRUE(create_guard_world(&ss_world, &ss, &ss_count, 128u));
    ss[0].health = 1000;
    ss[0].ai_random_state = 12288u;
    TEST_ASSERT_TRUE(wolf_enemy_ai_player_attack(
        ss, ss_count, &ss_world, player_x, player_z,
        1.57079632679f, WOLF_WEAPON_PISTOL, &ss_events));
    TEST_ASSERT_TRUE(ss[0].attack_mode);
    TEST_ASSERT_TRUE(ss[0].health < 1000);
    TEST_ASSERT_TRUE(ss[0].state_id >= WOLF_SS_STATE_PAIN_FIRST);
    TEST_ASSERT_TRUE(ss[0].state_id <= WOLF_SS_STATE_PAIN_LAST);
    TEST_ASSERT_EQUAL_UINT32(1u, ss_events.ss_alerts);

    wolf_actor_collection_destroy(officer);
    wolf_actor_collection_destroy(ss);
}

void test_ss_attack_uses_original_two_thirds_effective_distance(void)
{
    WolfWorldState officer_world = {0};
    WolfWorldState ss_world = {0};
    WolfActor *officer = NULL;
    WolfActor *ss = NULL;
    uint32_t officer_count = 0u;
    uint32_t ss_count = 0u;
    WolfEnemyAIEvents officer_events = {0};
    WolfEnemyAIEvents ss_events = {0};
    float player_x = tile_centre(0u, 8u);
    float player_z = tile_centre(4u, 8u);
    float player_yaw = 1.57079632679f;

    TEST_ASSERT_TRUE(create_guard_world(
        &officer_world, &officer, &officer_count, 118u));
    officer_world.player.health = 100;
    officer[0].attack_mode = true;
    officer[0].ai_random_state = 12288u;
    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        &officer[0], WOLF_OFFICER_STATE_SHOOT_ACTION));
    TEST_ASSERT_TRUE(wolf_enemy_ai_update_tic(
        officer, officer_count, &officer_world, player_x, player_z,
        player_yaw, WOLF_DIFFICULTY_HARD, false, false, &officer_events));
    TEST_ASSERT_EQUAL_INT32(100, officer_world.player.health);
    TEST_ASSERT_EQUAL_UINT32(1u, officer_events.officer_shots);

    TEST_ASSERT_TRUE(create_guard_world(&ss_world, &ss, &ss_count, 128u));
    ss_world.player.health = 100;
    ss[0].attack_mode = true;
    ss[0].ai_random_state = 12288u;
    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        &ss[0], WOLF_SS_STATE_SHOOT_ACTION_1));
    TEST_ASSERT_TRUE(wolf_enemy_ai_update_tic(
        ss, ss_count, &ss_world, player_x, player_z,
        player_yaw, WOLF_DIFFICULTY_HARD, false, false, &ss_events));
    TEST_ASSERT_EQUAL_INT32(84, ss_world.player.health);
    TEST_ASSERT_EQUAL_UINT32(1u, ss_events.ss_shots);

    wolf_actor_collection_destroy(officer);
    wolf_actor_collection_destroy(ss);
}


void test_mutant_uses_original_two_shot_state_chain_and_animation_timing(void)
{
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 216u));
    TEST_ASSERT_EQUAL_UINT32(1u, actor_count);
    TEST_ASSERT_EQUAL(WOLF_ACTOR_MUTANT, actors[0].family);
    TEST_ASSERT_EQUAL_UINT16(WOLF_MUTANT_STATE_STAND, actors[0].state_id);
    TEST_ASSERT_EQUAL_INT32(65, actors[0].health);

    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        &actors[0], WOLF_MUTANT_STATE_SHOOT_FIRST));
    TEST_ASSERT_EQUAL_STRING("mut_shoot", actors[0].animation);
    TEST_ASSERT_EQUAL_UINT8(0u, actors[0].pose);
    TEST_ASSERT_EQUAL_UINT16(6u, actors[0].state_tics_remaining);
    for (uint32_t tic = 0u; tic < 6u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    }
    TEST_ASSERT_EQUAL_UINT16(14u, actors[0].state_id);
    TEST_ASSERT_EQUAL_UINT8(1u, actors[0].pose);
    TEST_ASSERT_EQUAL_UINT16(20u, actors[0].state_tics_remaining);

    for (uint32_t tic = 0u; tic < 20u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_MUTANT_STATE_SHOOT_ACTION_2, actors[0].state_id);
    TEST_ASSERT_EQUAL_UINT8(2u, actors[0].pose);
    TEST_ASSERT_EQUAL_UINT16(10u, actors[0].state_tics_remaining);
    for (uint32_t tic = 0u; tic < 10u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    }
    TEST_ASSERT_EQUAL_UINT16(16u, actors[0].state_id);
    TEST_ASSERT_EQUAL_UINT8(3u, actors[0].pose);
    TEST_ASSERT_EQUAL_UINT16(20u, actors[0].state_tics_remaining);
    for (uint32_t tic = 0u; tic < 20u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(actors, actor_count));
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_MUTANT_STATE_CHASE_FIRST, actors[0].state_id);

    wolf_actor_collection_destroy(actors);
}

void test_mutant_alerts_at_original_chase_speed_and_fires_on_both_action_frames(void)
{
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    WolfEnemyAIEvents events = {0};
    float player_x = tile_centre(2u, 8u);
    float player_z = tile_centre(4u, 8u);

    TEST_ASSERT_TRUE(create_guard_world(&world, &actors, &actor_count, 218u));
    actors[0].health = 1000;
    world.player.health = 100;
    advance_until_alert(actors, actor_count, &world, player_x, player_z, &events);
    TEST_ASSERT_TRUE(actors[0].attack_mode);
    TEST_ASSERT_EQUAL_UINT16(WOLF_MUTANT_STATE_CHASE_FIRST, actors[0].state_id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f,
        WOLF_ACTOR_PATROL_SPEED_WORLD_PER_TIC * 3.0f,
        actors[0].movement_speed_world_per_tic);

    actors[0].ai_random_state = 12288u;
    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        &actors[0], WOLF_MUTANT_STATE_SHOOT_ACTION_1));
    TEST_ASSERT_TRUE(wolf_enemy_ai_update_tic(
        actors, actor_count, &world, player_x, player_z,
        1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, &events));
    TEST_ASSERT_EQUAL_UINT32(1u, events.mutant_shots);
    TEST_ASSERT_EQUAL_INT32(84, world.player.health);
    TEST_ASSERT_EQUAL_UINT32(0u, events.guard_shots);
    TEST_ASSERT_EQUAL_UINT32(0u, events.ss_shots);

    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        &actors[0], WOLF_MUTANT_STATE_SHOOT_ACTION_2));
    TEST_ASSERT_TRUE(wolf_enemy_ai_update_tic(
        actors, actor_count, &world, player_x, player_z,
        1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, &events));
    TEST_ASSERT_EQUAL_UINT32(1u, events.mutant_shots);
    TEST_ASSERT_EQUAL_INT32(59, world.player.health);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, actors[0].position_x, events.mutant_shot_source_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, actors[0].position_z, events.mutant_shot_source_z);

    wolf_actor_collection_destroy(actors);
}

void test_mutant_damage_uses_original_pain_death_score_and_ammunition_drop(void)
{
    WolfWorldState pain_world = {0};
    WolfWorldState death_world = {0};
    WolfActor *pain_actor = NULL;
    WolfActor *dead_actor = NULL;
    uint32_t pain_count = 0u;
    uint32_t dead_count = 0u;
    WolfEnemyAIEvents events = {0};
    float player_x = tile_centre(2u, 8u);
    float player_z = tile_centre(4u, 8u);
    size_t actor_tile = 4u * WOLF_LEVEL_PLANE_WIDTH + 4u;

    TEST_ASSERT_TRUE(create_guard_world(&pain_world, &pain_actor, &pain_count, 216u));
    pain_actor[0].health = 1000;
    pain_actor[0].ai_random_state = 12288u;
    TEST_ASSERT_TRUE(wolf_enemy_ai_player_attack(
        pain_actor, pain_count, &pain_world, player_x, player_z,
        1.57079632679f, WOLF_WEAPON_PISTOL, &events));
    TEST_ASSERT_TRUE(pain_actor[0].attack_mode);
    TEST_ASSERT_EQUAL_INT32(934, pain_actor[0].health);
    TEST_ASSERT_EQUAL_UINT16(WOLF_MUTANT_STATE_PAIN_LAST, pain_actor[0].state_id);

    TEST_ASSERT_TRUE(create_guard_world(&death_world, &dead_actor, &dead_count, 216u));
    dead_actor[0].health = 1;
    dead_actor[0].attack_mode = true;
    dead_actor[0].ai_random_state = 12288u;
    TEST_ASSERT_TRUE(wolf_enemy_ai_player_attack(
        dead_actor, dead_count, &death_world, player_x, player_z,
        1.57079632679f, WOLF_WEAPON_PISTOL, &events));
    TEST_ASSERT_FALSE(dead_actor[0].shootable);
    TEST_ASSERT_EQUAL_UINT16(WOLF_MUTANT_STATE_DIE_FIRST, dead_actor[0].state_id);
    TEST_ASSERT_EQUAL_INT32(700, death_world.player.score);
    TEST_ASSERT_EQUAL_UINT32(1u, events.mutant_deaths);
    TEST_ASSERT_EQUAL_UINT16(WOLF_LEVEL_STATIC_EXTRA_CLIP,
        death_world.level.object_tiles[actor_tile]);

    for (uint32_t tic = 0u; tic < 28u; ++tic) {
        TEST_ASSERT_TRUE(wolf_actor_collection_update_tic(dead_actor, dead_count));
    }
    TEST_ASSERT_EQUAL_UINT16(WOLF_MUTANT_STATE_DEAD, dead_actor[0].state_id);
    TEST_ASSERT_TRUE(dead_actor[0].active);

    wolf_actor_collection_destroy(pain_actor);
    wolf_actor_collection_destroy(dead_actor);
}


/*
 * Exercise all four families in one corridor encounter. A closed door blocks
 * their shared sight trace; opening it lets each family alert, then each
 * family reaches its own original shot-action state and emits its own event.
 */
void test_four_enemy_families_integrate_across_a_shared_door_encounter(void)
{
    WolfLevel level = {0};
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    WolfEnemyAIEvents events = {0};
    uint32_t guard_alerts = 0u;
    uint32_t officer_alerts = 0u;
    uint32_t ss_alerts = 0u;
    WolfActor *guard = NULL;
    WolfActor *officer = NULL;
    WolfActor *ss = NULL;
    WolfActor *mutant = NULL;
    const float player_x = tile_centre(12u, 14u);
    const float player_z = tile_centre(4u, 8u);

    level.width = 14u;
    level.height = 8u;
    level.wall_tiles[4u * WOLF_LEVEL_PLANE_WIDTH + 9u] = 90u;
    level.object_tiles[4u * WOLF_LEVEL_PLANE_WIDTH + 1u] = 108u;
    level.object_tiles[4u * WOLF_LEVEL_PLANE_WIDTH + 3u] = 118u;
    level.object_tiles[4u * WOLF_LEVEL_PLANE_WIDTH + 5u] = 128u;
    level.object_tiles[4u * WOLF_LEVEL_PLANE_WIDTH + 7u] = 216u;

    TEST_ASSERT_TRUE(wolf_world_state_init(&world, &level, 98u));
    TEST_ASSERT_EQUAL_UINT8(1u, world.door_count);
    TEST_ASSERT_TRUE(wolf_actor_collection_create(
        &world.level, WOLF_DIFFICULTY_HARD, &actors, &actor_count));
    TEST_ASSERT_EQUAL_UINT32(4u, actor_count);

    for (uint32_t index = 0u; index < actor_count; ++index) {
        actors[index].movement_direction = WOLF_ACTOR_DIRECTION_EAST;
        switch (actors[index].family) {
        case WOLF_ACTOR_GUARD:
            guard = &actors[index];
            break;
        case WOLF_ACTOR_OFFICER:
            officer = &actors[index];
            break;
        case WOLF_ACTOR_SS:
            ss = &actors[index];
            break;
        case WOLF_ACTOR_MUTANT:
            mutant = &actors[index];
            break;
        default:
            break;
        }
    }

    TEST_ASSERT_NOT_NULL(guard);
    TEST_ASSERT_NOT_NULL(officer);
    TEST_ASSERT_NOT_NULL(ss);
    TEST_ASSERT_NOT_NULL(mutant);
    TEST_ASSERT_EQUAL_INT32(25, guard->health);
    TEST_ASSERT_EQUAL_INT32(50, officer->health);
    TEST_ASSERT_EQUAL_INT32(100, ss->health);
    TEST_ASSERT_EQUAL_INT32(65, mutant->health);

    /* A closed door must prevent every family from acquiring line of sight. */
    for (uint32_t tic = 0u; tic < 80u; ++tic) {
        TEST_ASSERT_TRUE(wolf_enemy_ai_update_tic(
            actors, actor_count, &world, player_x, player_z,
            1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, &events));
    }
    TEST_ASSERT_FALSE(guard->attack_mode);
    TEST_ASSERT_FALSE(officer->attack_mode);
    TEST_ASSERT_FALSE(ss->attack_mode);
    TEST_ASSERT_FALSE(mutant->attack_mode);

    /* Opening the shared doorway exposes the same corridor to all four enemies. */
    world.doors[0].position = WOLF_WORLD_DOOR_OPEN_POSITION;
    world.doors[0].action = WOLF_DOOR_OPEN;
    for (uint32_t tic = 0u; tic < 80u; ++tic) {
        TEST_ASSERT_TRUE(wolf_enemy_ai_update_tic(
            actors, actor_count, &world, player_x, player_z,
            1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, &events));
        guard_alerts += events.guard_alerts;
        officer_alerts += events.officer_alerts;
        ss_alerts += events.ss_alerts;
    }
    TEST_ASSERT_TRUE(guard->attack_mode);
    TEST_ASSERT_TRUE(officer->attack_mode);
    TEST_ASSERT_TRUE(ss->attack_mode);
    TEST_ASSERT_TRUE(mutant->attack_mode);
    TEST_ASSERT_EQUAL_UINT32(1u, guard_alerts);
    TEST_ASSERT_EQUAL_UINT32(1u, officer_alerts);
    TEST_ASSERT_EQUAL_UINT32(1u, ss_alerts);

    /* Family-specific action frames must coexist in the same actor update. */
    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        guard, WOLF_GUARD_STATE_SHOOT_ACTION));
    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        officer, WOLF_OFFICER_STATE_SHOOT_ACTION));
    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        ss, WOLF_SS_STATE_SHOOT_ACTION_1));
    TEST_ASSERT_TRUE(wolf_actor_collection_set_state(
        mutant, WOLF_MUTANT_STATE_SHOOT_ACTION_1));
    TEST_ASSERT_TRUE(wolf_enemy_ai_update_tic(
        actors, actor_count, &world, player_x, player_z,
        1.57079632679f, WOLF_DIFFICULTY_HARD, false, false, &events));
    TEST_ASSERT_EQUAL_UINT32(1u, events.guard_shots);
    TEST_ASSERT_EQUAL_UINT32(1u, events.officer_shots);
    TEST_ASSERT_EQUAL_UINT32(1u, events.ss_shots);
    TEST_ASSERT_EQUAL_UINT32(1u, events.mutant_shots);

    wolf_actor_collection_destroy(actors);
}


typedef struct MixedEnemySimulationSnapshot {
    WolfActor actors[4];
    WolfPlayerState player;
} MixedEnemySimulationSnapshot;

/* Run the same four-family chase using a render clock at the requested rate. */
static bool simulate_mixed_enemy_encounter(
    uint32_t frames_per_second,
    MixedEnemySimulationSnapshot *snapshot)
{
    WolfLevel level = {0};
    WolfWorldState world = {0};
    WolfActor *actors = NULL;
    uint32_t actor_count = 0u;
    WolfSimulationClock clock = {0};
    const float player_x = tile_centre(7u, 14u);
    const float player_z = tile_centre(7u, 14u);
    uint32_t total_tics = 0u;
    bool valid = true;

    if (frames_per_second == 0u || snapshot == NULL) {
        return false;
    }

    level.width = 14u;
    level.height = 14u;
    level.object_tiles[2u * WOLF_LEVEL_PLANE_WIDTH + 7u] = 108u;
    level.object_tiles[7u * WOLF_LEVEL_PLANE_WIDTH + 12u] = 118u;
    level.object_tiles[12u * WOLF_LEVEL_PLANE_WIDTH + 7u] = 128u;
    level.object_tiles[7u * WOLF_LEVEL_PLANE_WIDTH + 2u] = 216u;
    if (!wolf_world_state_init(&world, &level, 0u) ||
        !wolf_actor_collection_create(
            &world.level, WOLF_DIFFICULTY_HARD, &actors, &actor_count) ||
        actor_count != 4u) {
        wolf_actor_collection_destroy(actors);
        return false;
    }

    for (uint32_t index = 0u; index < actor_count; ++index) {
        uint16_t chase_state;

        switch (actors[index].family) {
        case WOLF_ACTOR_GUARD:
            chase_state = WOLF_GUARD_STATE_CHASE_FIRST;
            break;
        case WOLF_ACTOR_OFFICER:
            chase_state = WOLF_OFFICER_STATE_CHASE_FIRST;
            break;
        case WOLF_ACTOR_SS:
            chase_state = WOLF_SS_STATE_CHASE_FIRST;
            break;
        case WOLF_ACTOR_MUTANT:
            chase_state = WOLF_MUTANT_STATE_CHASE_FIRST;
            break;
        default:
            wolf_actor_collection_destroy(actors);
            return false;
        }
        if (!wolf_actor_collection_set_state(&actors[index], chase_state)) {
            wolf_actor_collection_destroy(actors);
            return false;
        }
        actors[index].attack_mode = true;
        actors[index].movement_direction = WOLF_ACTOR_DIRECTION_NONE;
        actors[index].movement_enabled = false;
        actors[index].ai_attack_cooldown = 0u;
    }

    wolf_simulation_clock_reset(&clock);
    for (uint32_t frame = 0u; frame < frames_per_second * 2u && valid; ++frame) {
        uint32_t frame_tics = wolf_simulation_clock_advance(
            &clock, 1.0 / (double)frames_per_second);

        total_tics += frame_tics;
        for (uint32_t tic = 0u; tic < frame_tics; ++tic) {
            if (!wolf_enemy_ai_update_tic(
                    actors, actor_count, &world, player_x, player_z,
                    0.0f, WOLF_DIFFICULTY_HARD, false, false, NULL) ||
                !wolf_actor_movement_update_tic(
                    actors, actor_count, &world, player_x, player_z) ||
                !wolf_actor_collection_update_tic(actors, actor_count)) {
                valid = false;
                break;
            }
        }
    }

    if (valid && total_tics == 140u) {
        memset(snapshot, 0, sizeof(*snapshot));
        memcpy(snapshot->actors, actors, sizeof(snapshot->actors));
        snapshot->player = world.player;
    } else {
        valid = false;
    }
    wolf_actor_collection_destroy(actors);
    return valid;
}

/* AI, movement, combat decisions and animation must consume the same 70 Hz tics at any render rate. */
void test_four_enemy_families_are_deterministic_at_thirty_and_144_fps(void)
{
    MixedEnemySimulationSnapshot thirty_fps = {0};
    MixedEnemySimulationSnapshot one_hundred_forty_four_fps = {0};

    TEST_ASSERT_TRUE(simulate_mixed_enemy_encounter(30u, &thirty_fps));
    TEST_ASSERT_TRUE(simulate_mixed_enemy_encounter(
        144u, &one_hundred_forty_four_fps));
    TEST_ASSERT_EQUAL_MEMORY(
        thirty_fps.actors, one_hundred_forty_four_fps.actors,
        sizeof(thirty_fps.actors));
    TEST_ASSERT_EQUAL_INT32(
        thirty_fps.player.health, one_hundred_forty_four_fps.player.health);
    TEST_ASSERT_EQUAL_INT32(
        thirty_fps.player.score, one_hundred_forty_four_fps.player.score);
    TEST_ASSERT_EQUAL_INT32(
        thirty_fps.player.ammo, one_hundred_forty_four_fps.player.ammo);
    TEST_ASSERT_EQUAL_INT(
        thirty_fps.player.weapon, one_hundred_forty_four_fps.player.weapon);
}
