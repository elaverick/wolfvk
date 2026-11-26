#include "unity.h"

#include "adlib_archive.h"

#include <stdlib.h>
#include <string.h>

static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value & 0xffu);
    data[1] = (uint8_t)((value >> 8) & 0xffu);
    data[2] = (uint8_t)((value >> 16) & 0xffu);
    data[3] = (uint8_t)(value >> 24);
}

static void write_archive_offsets(uint8_t *header, uint32_t chunk_size)
{
    for (size_t index = 0u; index <= WOLF_AUDIO_ADLIB_CHUNK_COUNT; ++index) {
        write_u32(header + index * 4u, (uint32_t)(index * chunk_size));
    }
}

static uint8_t *make_test_audio_archive(
    size_t *header_size,
    uint8_t **audio_data,
    size_t *audio_size)
{
    const uint32_t chunk_size = 32u;
    uint8_t *header = calloc(WOLF_AUDIO_ADLIB_HEADER_SIZE, 1u);

    *audio_size = (size_t)WOLF_AUDIO_ADLIB_CHUNK_COUNT * chunk_size;
    *audio_data = calloc(*audio_size, 1u);
    if (header == NULL || *audio_data == NULL) {
        free(header);
        free(*audio_data);
        *audio_data = NULL;
        return NULL;
    }

    write_archive_offsets(header, chunk_size);
    *header_size = WOLF_AUDIO_ADLIB_HEADER_SIZE;

    /* Sound zero is chunk 87: common header, 16-byte instrument, block, data. */
    {
        uint8_t *chunk = *audio_data + (size_t)WOLF_AUDIO_ADLIB_START_CHUNK * chunk_size;
        write_u32(chunk, 2u);
        write_u32(chunk + WOLF_AUDIO_ADLIB_DATA_OFFSET, 0u);
        chunk[WOLF_AUDIO_ADLIB_DATA_OFFSET + 1u] = 0x80u;
        chunk[WOLF_AUDIO_ADLIB_COMMON_HEADER_SIZE + WOLF_AUDIO_ADLIB_INSTRUMENT_SIZE] = 5u;
    }
    return header;
}

void test_adlib_archive_reads_original_header_and_sound_chunk(void)
{
    WolfAudioAdlibArchive archive = {0};
    WolfAudioAdlibSound sound = {0};
    size_t header_size = 0u;
    size_t audio_size = 0u;
    uint8_t *audio_data = NULL;
    uint8_t *header = make_test_audio_archive(&header_size, &audio_data, &audio_size);
    char error_message[160];

    TEST_ASSERT_NOT_NULL(header);
    TEST_ASSERT_TRUE(wolf_audio_adlib_archive_decode(
        header, header_size, audio_data, audio_size,
        &archive, error_message, sizeof(error_message)));
    TEST_ASSERT_TRUE(wolf_audio_adlib_archive_get_sound(
        &archive, 0u, &sound, error_message, sizeof(error_message)));
    TEST_ASSERT_EQUAL_UINT32(2u, sound.frequency_data_length);
    TEST_ASSERT_EQUAL_UINT8(5u, sound.block);
    TEST_ASSERT_EQUAL_UINT8(0u, sound.frequency_data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x80u, sound.frequency_data[1]);

    wolf_audio_adlib_archive_destroy(&archive);
    free(audio_data);
    free(header);
}

void test_adlib_archive_rejects_short_and_non_monotonic_headers(void)
{
    WolfAudioAdlibArchive archive = {0};
    size_t header_size = 0u;
    size_t audio_size = 0u;
    uint8_t *audio_data = NULL;
    uint8_t *header = make_test_audio_archive(&header_size, &audio_data, &audio_size);
    char error_message[160];

    TEST_ASSERT_NOT_NULL(header);
    TEST_ASSERT_FALSE(wolf_audio_adlib_archive_decode(
        header, header_size - 1u, audio_data, audio_size,
        &archive, error_message, sizeof(error_message)));
    TEST_ASSERT_NOT_NULL(strstr(error_message, "invalid or incomplete"));

    write_u32(header + 88u * 4u, 10u);
    TEST_ASSERT_FALSE(wolf_audio_adlib_archive_decode(
        header, header_size, audio_data, audio_size,
        &archive, error_message, sizeof(error_message)));
    TEST_ASSERT_NOT_NULL(strstr(error_message, "non-monotonic"));

    free(audio_data);
    free(header);
}

void test_adlib_archive_rejects_offset_outside_audio_data(void)
{
    WolfAudioAdlibArchive archive = {0};
    size_t header_size = 0u;
    size_t audio_size = 0u;
    uint8_t *audio_data = NULL;
    uint8_t *header = make_test_audio_archive(&header_size, &audio_data, &audio_size);
    char error_message[160];

    TEST_ASSERT_NOT_NULL(header);
    write_u32(header + 10u * 4u, (uint32_t)(audio_size + 1u));
    TEST_ASSERT_FALSE(wolf_audio_adlib_archive_decode(
        header, header_size, audio_data, audio_size,
        &archive, error_message, sizeof(error_message)));
    TEST_ASSERT_NOT_NULL(strstr(error_message, "out-of-range"));

    free(audio_data);
    free(header);
}

void test_adlib_archive_rejects_invalid_effect_length_and_indices(void)
{
    WolfAudioAdlibArchive archive = {0};
    WolfAudioAdlibSound sound = {0};
    size_t header_size = 0u;
    size_t audio_size = 0u;
    uint8_t *audio_data = NULL;
    uint8_t *header = make_test_audio_archive(&header_size, &audio_data, &audio_size);
    char error_message[160];

    TEST_ASSERT_NOT_NULL(header);
    TEST_ASSERT_TRUE(wolf_audio_adlib_archive_decode(
        header, header_size, audio_data, audio_size,
        &archive, error_message, sizeof(error_message)));
    write_u32(archive.audio_data +
        (size_t)WOLF_AUDIO_ADLIB_START_CHUNK * 32u, 100u);

    TEST_ASSERT_FALSE(wolf_audio_adlib_archive_get_sound(
        &archive, 0u, &sound, error_message, sizeof(error_message)));
    TEST_ASSERT_NOT_NULL(strstr(error_message, "frequency-data length"));
    TEST_ASSERT_FALSE(wolf_audio_adlib_archive_get_sound(
        &archive, WOLF_AUDIO_ADLIB_SOUND_COUNT, &sound,
        error_message, sizeof(error_message)));

    wolf_audio_adlib_archive_destroy(&archive);
    free(audio_data);
    free(header);
}

void test_adlib_archive_destroy_is_safe_for_empty_archive(void)
{
    WolfAudioAdlibArchive archive = {0};
    wolf_audio_adlib_archive_destroy(&archive);
    TEST_ASSERT_NULL(archive.audio_data);
    TEST_ASSERT_EQUAL_UINT(0u, archive.audio_size);
}
