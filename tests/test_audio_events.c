#include "unity.h"

#include "wolfvk/audio_events.h"

void test_audio_world_snapshot_detects_door_open_and_close_transitions(void)
{
    WolfAudioWorldSnapshot previous = {0};
    WolfAudioWorldSnapshot current = {0};
    WolfAudioWorldEvent events[WOLF_WORLD_MAX_DOORS + 1u];

    previous.door_count = 1u;
    previous.door_actions[0] = WOLF_DOOR_CLOSED;
    current.door_count = 1u;
    current.door_actions[0] = WOLF_DOOR_OPENING;

    TEST_ASSERT_EQUAL_UINT(1u, wolf_audio_world_snapshot_diff(
        &previous, &current, events, sizeof(events) / sizeof(events[0])));
    TEST_ASSERT_EQUAL_INT(WOLF_AUDIO_WORLD_DOOR_OPENED, events[0]);
    TEST_ASSERT_EQUAL_STRING("OPENDOORSND", wolf_audio_world_event_sound(events[0]));

    previous = current;
    current.door_actions[0] = WOLF_DOOR_CLOSING;
    TEST_ASSERT_EQUAL_UINT(1u, wolf_audio_world_snapshot_diff(
        &previous, &current, events, sizeof(events) / sizeof(events[0])));
    TEST_ASSERT_EQUAL_INT(WOLF_AUDIO_WORLD_DOOR_CLOSED, events[0]);
    TEST_ASSERT_EQUAL_STRING("CLOSEDOORSND", wolf_audio_world_event_sound(events[0]));
}

void test_audio_world_snapshot_detects_secret_wall_start_once(void)
{
    WolfAudioWorldSnapshot previous = {0};
    WolfAudioWorldSnapshot current = {0};
    WolfAudioWorldEvent events[WOLF_WORLD_MAX_DOORS + 1u];

    current.pushwall_active = true;
    TEST_ASSERT_EQUAL_UINT(1u, wolf_audio_world_snapshot_diff(
        &previous, &current, events, sizeof(events) / sizeof(events[0])));
    TEST_ASSERT_EQUAL_INT(WOLF_AUDIO_WORLD_SECRET_WALL_STARTED, events[0]);
    TEST_ASSERT_EQUAL_STRING("PUSHWALLSND", wolf_audio_world_event_sound(events[0]));

    previous = current;
    current.pushwall_active = true;
    TEST_ASSERT_EQUAL_UINT(0u, wolf_audio_world_snapshot_diff(
        &previous, &current, events, sizeof(events) / sizeof(events[0])));
}

void test_audio_world_snapshot_ignores_door_animation_without_state_change(void)
{
    WolfAudioWorldSnapshot previous = {0};
    WolfAudioWorldSnapshot current = {0};
    WolfAudioWorldEvent events[WOLF_WORLD_MAX_DOORS + 1u];

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
    WolfAudioWorldEvent events[WOLF_WORLD_MAX_DOORS + 1u];

    previous.door_count = 2u;
    previous.door_actions[0] = WOLF_DOOR_CLOSED;
    previous.door_actions[1] = WOLF_DOOR_OPEN;
    current.door_count = 2u;
    current.door_actions[0] = WOLF_DOOR_OPENING;
    current.door_actions[1] = WOLF_DOOR_CLOSING;
    current.pushwall_active = true;

    TEST_ASSERT_EQUAL_UINT(3u, wolf_audio_world_snapshot_diff(
        &previous, &current, events, sizeof(events) / sizeof(events[0])));
    TEST_ASSERT_EQUAL_INT(WOLF_AUDIO_WORLD_DOOR_OPENED, events[0]);
    TEST_ASSERT_EQUAL_INT(WOLF_AUDIO_WORLD_DOOR_CLOSED, events[1]);
    TEST_ASSERT_EQUAL_INT(WOLF_AUDIO_WORLD_SECRET_WALL_STARTED, events[2]);
    TEST_ASSERT_NULL(wolf_audio_world_event_sound((WolfAudioWorldEvent)99));
    TEST_ASSERT_EQUAL_UINT(0u, wolf_audio_world_snapshot_diff(
        NULL, &current, events, sizeof(events) / sizeof(events[0])));
    TEST_ASSERT_EQUAL_UINT(0u, wolf_audio_world_snapshot_diff(
        &previous, &current, NULL, sizeof(events) / sizeof(events[0])));
}
