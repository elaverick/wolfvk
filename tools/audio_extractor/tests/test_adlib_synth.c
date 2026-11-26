#include "unity.h"

#include "adlib_synth.h"

#include <stdlib.h>

void test_adlib_synth_renders_a_non_silent_original_frequency_stream(void)
{
    static const uint8_t instrument[WOLF_AUDIO_ADLIB_INSTRUMENT_SIZE] = {
        0x21u, 0x01u, 0x00u, 0x00u,
        0xf2u, 0xf2u, 0x74u, 0x74u,
        0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u
    };
    static const uint8_t frequency_data[] = {0x70u, 0x78u, 0x80u, 0x88u, 0u, 0x88u};
    WolfAudioAdlibSound sound = {0};
    WolfAudioPcm16 rendered = {0};
    int32_t peak = 0;

    for (size_t index = 0u; index < sizeof(instrument); ++index) {
        sound.instrument[index] = instrument[index];
    }
    sound.block = 5u;
    sound.frequency_data = frequency_data;
    sound.frequency_data_length = (uint32_t)sizeof(frequency_data);

    TEST_ASSERT_TRUE(wolf_audio_adlib_render(&sound, &rendered));
    TEST_ASSERT_EQUAL_UINT32(
        (uint32_t)(sizeof(frequency_data) * 315u + 4410u),
        rendered.frame_count);
    TEST_ASSERT_EQUAL_UINT32(WOLF_AUDIO_ADLIB_SAMPLE_RATE, rendered.sample_rate);

    for (uint32_t index = 0u; index < rendered.frame_count; ++index) {
        int32_t value = rendered.samples[index];
        int32_t magnitude = value < 0 ? -value : value;
        if (magnitude > peak) {
            peak = magnitude;
        }
    }
    TEST_ASSERT_TRUE(peak > 0);
    TEST_ASSERT_TRUE(peak < INT16_MAX);

    wolf_audio_pcm16_destroy(&rendered);
    TEST_ASSERT_NULL(rendered.samples);
    TEST_ASSERT_EQUAL_UINT32(0u, rendered.frame_count);
}

void test_adlib_synth_rejects_missing_and_oversized_streams(void)
{
    WolfAudioAdlibSound sound = {0};
    WolfAudioPcm16 rendered = {0};
    uint8_t frequency = 0x80u;

    TEST_ASSERT_FALSE(wolf_audio_adlib_render(NULL, &rendered));
    sound.frequency_data = &frequency;
    sound.frequency_data_length = 0u;
    TEST_ASSERT_FALSE(wolf_audio_adlib_render(&sound, &rendered));
    sound.frequency_data_length = WOLF_AUDIO_ADLIB_TICK_RATE * 31u;
    TEST_ASSERT_FALSE(wolf_audio_adlib_render(&sound, &rendered));
    TEST_ASSERT_NULL(rendered.samples);
}
