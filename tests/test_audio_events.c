#include "unity.h"

#include "wolfvk/audio_events.h"

void test_audio_world_snapshot_detects_door_open_and_close_transitions(void)
{
    WolfAudioWorldSnapshot previous = {0};
    WolfAudioWorldSnapshot current = {0};
    WolfAudioWorldEventInfo events[WOLF_WORLD_MAX_DOORS + 1u];

    previous.door_count = 1u;
    previous.door_actions[0] = WOLF_DOOR_CLOSED;
    current.door_count = 1u;
    current.door_actions[0] = WOLF_DOOR_OPENING;
    current.map_width = 5u;
    current.map_height = 5u;
    current.door_tile_x[0] = 2u;
    current.door_tile_y[0] = 2u;

    TEST_ASSERT_EQUAL_UINT(1u, wolf_audio_world_snapshot_diff(
        &previous, &current, events, sizeof(events) / sizeof(events[0])));
    TEST_ASSERT_EQUAL_INT(WOLF_AUDIO_WORLD_DOOR_OPENED, events[0].type);
    TEST_ASSERT_EQUAL_STRING("OPENDOORSND", wolf_audio_world_event_sound(events[0].type));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, events[0].source_position.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, WOLF_WORLD_EYE_HEIGHT, events[0].source_position.y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, events[0].source_position.z);

    previous = current;
    current.door_actions[0] = WOLF_DOOR_CLOSING;
    TEST_ASSERT_EQUAL_UINT(1u, wolf_audio_world_snapshot_diff(
        &previous, &current, events, sizeof(events) / sizeof(events[0])));
    TEST_ASSERT_EQUAL_INT(WOLF_AUDIO_WORLD_DOOR_CLOSED, events[0].type);
    TEST_ASSERT_EQUAL_STRING("CLOSEDOORSND", wolf_audio_world_event_sound(events[0].type));
}

void test_audio_world_snapshot_detects_secret_wall_start_once(void)
{
    WolfAudioWorldSnapshot previous = {0};
    WolfAudioWorldSnapshot current = {0};
    WolfAudioWorldEventInfo events[WOLF_WORLD_MAX_DOORS + 1u];

    current.pushwall_active = true;
    current.map_width = 5u;
    current.map_height = 5u;
    current.pushwall_tile_x = 1u;
    current.pushwall_tile_y = 3u;
    TEST_ASSERT_EQUAL_UINT(1u, wolf_audio_world_snapshot_diff(
        &previous, &current, events, sizeof(events) / sizeof(events[0])));
    TEST_ASSERT_EQUAL_INT(WOLF_AUDIO_WORLD_SECRET_WALL_STARTED, events[0].type);
    TEST_ASSERT_EQUAL_STRING("PUSHWALLSND", wolf_audio_world_event_sound(events[0].type));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.2f, events[0].source_position.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.2f, events[0].source_position.z);

    previous = current;
    current.pushwall_active = true;
    TEST_ASSERT_EQUAL_UINT(0u, wolf_audio_world_snapshot_diff(
        &previous, &current, events, sizeof(events) / sizeof(events[0])));
}

void test_audio_world_snapshot_ignores_door_animation_without_state_change(void)
{
    WolfAudioWorldSnapshot previous = {0};
    WolfAudioWorldSnapshot current = {0};
    WolfAudioWorldEventInfo events[WOLF_WORLD_MAX_DOORS + 1u];

    previous.door_count = 1u;
    current.door_count = 1u;
    previous.door_actions[0] = WOLF_DOOR_OPENING;
    current.door_actions[0] = WOLF_DOOR_OPENING;

    TEST_ASSERT_EQUAL_UINT(0u, wolf_audio_world_snapshot_diff(
        &previous, &current, events, sizeof(events) / sizeof(events[0])));
}

void test_audio_world_snapshot_handles_multiple_transitions_and_invalid_inputs(void)
{
    WolfAudioWorldSnapshot previous = {0};
    WolfAudioWorldSnapshot current = {0};
    WolfAudioWorldEventInfo events[WOLF_WORLD_MAX_DOORS + 1u];

    previous.door_count = 2u;
    previous.door_actions[0] = WOLF_DOOR_CLOSED;
    previous.door_actions[1] = WOLF_DOOR_OPEN;
    current.door_count = 2u;
    current.door_actions[0] = WOLF_DOOR_OPENING;
    current.door_actions[1] = WOLF_DOOR_CLOSING;
    current.pushwall_active = true;

    TEST_ASSERT_EQUAL_UINT(3u, wolf_audio_world_snapshot_diff(
        &previous, &current, events, sizeof(events) / sizeof(events[0])));
    TEST_ASSERT_EQUAL_INT(WOLF_AUDIO_WORLD_DOOR_OPENED, events[0].type);
    TEST_ASSERT_EQUAL_INT(WOLF_AUDIO_WORLD_DOOR_CLOSED, events[1].type);
    TEST_ASSERT_EQUAL_INT(WOLF_AUDIO_WORLD_SECRET_WALL_STARTED, events[2].type);
    TEST_ASSERT_NULL(wolf_audio_world_event_sound((WolfAudioWorldEvent)99));
    TEST_ASSERT_EQUAL_UINT(0u, wolf_audio_world_snapshot_diff(
        NULL, &current, events, sizeof(events) / sizeof(events[0])));
    TEST_ASSERT_EQUAL_UINT(0u, wolf_audio_world_snapshot_diff(
        &previous, &current, NULL, sizeof(events) / sizeof(events[0])));
}

void test_audio_bonus_events_map_to_original_wl6_effect_names(void)
{
    TEST_ASSERT_EQUAL_STRING("GETKEYSND", wolf_audio_bonus_sound_name(WOLF_BONUS_GOLD_KEY));
    TEST_ASSERT_EQUAL_STRING("GETKEYSND", wolf_audio_bonus_sound_name(WOLF_BONUS_SILVER_KEY));
    TEST_ASSERT_EQUAL_STRING("HEALTH1SND", wolf_audio_bonus_sound_name(WOLF_BONUS_DOG_FOOD));
    TEST_ASSERT_EQUAL_STRING("HEALTH1SND", wolf_audio_bonus_sound_name(WOLF_BONUS_FOOD));
    TEST_ASSERT_EQUAL_STRING("HEALTH2SND", wolf_audio_bonus_sound_name(WOLF_BONUS_FIRST_AID));
    TEST_ASSERT_EQUAL_STRING("SLURPIESND", wolf_audio_bonus_sound_name(WOLF_BONUS_GIBS));
    TEST_ASSERT_EQUAL_STRING("GETAMMOSND", wolf_audio_bonus_sound_name(WOLF_BONUS_CLIP));
    TEST_ASSERT_EQUAL_STRING("GETAMMOSND", wolf_audio_bonus_sound_name(WOLF_BONUS_USED_CLIP));
    TEST_ASSERT_EQUAL_STRING("GETMACHINESND", wolf_audio_bonus_sound_name(WOLF_BONUS_MACHINEGUN));
    TEST_ASSERT_EQUAL_STRING("GETGATLINGSND", wolf_audio_bonus_sound_name(WOLF_BONUS_CHAINGUN));
    TEST_ASSERT_EQUAL_STRING("BONUS1SND", wolf_audio_bonus_sound_name(WOLF_BONUS_CROSS));
    TEST_ASSERT_EQUAL_STRING("BONUS2SND", wolf_audio_bonus_sound_name(WOLF_BONUS_CHALICE));
    TEST_ASSERT_EQUAL_STRING("BONUS3SND", wolf_audio_bonus_sound_name(WOLF_BONUS_BIBLE));
    TEST_ASSERT_EQUAL_STRING("BONUS4SND", wolf_audio_bonus_sound_name(WOLF_BONUS_CROWN));
    TEST_ASSERT_EQUAL_STRING("BONUS1UPSND", wolf_audio_bonus_sound_name(WOLF_BONUS_EXTRA_LIFE));
    TEST_ASSERT_NULL(wolf_audio_bonus_sound_name(WOLF_BONUS_NONE));
    TEST_ASSERT_NULL(wolf_audio_bonus_sound_name((WolfBonusType)999));
}
