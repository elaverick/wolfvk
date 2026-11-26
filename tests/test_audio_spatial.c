#include "unity.h"

#include "wolfvk/audio_spatial.h"

#include <math.h>

void test_audio_source_in_front_of_north_facing_listener_is_forward(void)
{
    WolfAudioVector3 relative = {0.0f, 0.0f, 0.0f};
    TEST_ASSERT_TRUE(wolf_audio_relative_position(
        (WolfAudioVector3){0.0f, 0.0f, 0.0f}, 0.0f,
        (WolfAudioVector3){0.0f, 0.0f, -5.0f}, &relative));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, relative.x);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, relative.y);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -5.0f, relative.z);
}

void test_audio_source_to_east_of_north_facing_listener_is_right(void)
{
    WolfAudioVector3 relative = {0.0f, 0.0f, 0.0f};
    TEST_ASSERT_TRUE(wolf_audio_relative_position(
        (WolfAudioVector3){0.0f, 0.0f, 0.0f}, 0.0f,
        (WolfAudioVector3){3.0f, 0.0f, 0.0f}, &relative));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.0f, relative.x);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, relative.z);
}

void test_audio_listener_rotation_changes_relative_direction(void)
{
    WolfAudioVector3 relative = {0.0f, 0.0f, 0.0f};
    TEST_ASSERT_TRUE(wolf_audio_relative_position(
        (WolfAudioVector3){0.0f, 0.0f, 0.0f}, 1.57079632679f,
        (WolfAudioVector3){0.0f, 0.0f, -4.0f}, &relative));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -4.0f, relative.x);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, relative.z);
}

void test_audio_relative_position_preserves_vertical_offset(void)
{
    WolfAudioVector3 relative = {0.0f, 0.0f, 0.0f};
    TEST_ASSERT_TRUE(wolf_audio_relative_position(
        (WolfAudioVector3){1.0f, 2.0f, 3.0f}, 0.7f,
        (WolfAudioVector3){1.0f, 5.5f, 3.0f}, &relative));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.5f, relative.y);
}

void test_audio_relative_position_rejects_invalid_arguments(void)
{
    WolfAudioVector3 relative = {0.0f, 0.0f, 0.0f};
    TEST_ASSERT_FALSE(wolf_audio_relative_position(
        (WolfAudioVector3){0.0f, 0.0f, 0.0f}, 0.0f,
        (WolfAudioVector3){0.0f, 0.0f, 0.0f}, NULL));
    TEST_ASSERT_FALSE(wolf_audio_relative_position(
        (WolfAudioVector3){NAN, 0.0f, 0.0f}, 0.0f,
        (WolfAudioVector3){0.0f, 0.0f, 0.0f}, &relative));
}
