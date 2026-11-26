#include "unity.h"

#include "audio_archive.h"

#include <stdlib.h>
#include <string.h>

static void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xffu);
    data[1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value & 0xffu);
    data[1] = (uint8_t)((value >> 8) & 0xffu);
    data[2] = (uint8_t)((value >> 16) & 0xffu);
    data[3] = (uint8_t)(value >> 24);
}

/* Build a three-page VSWAP: one sprite page, one sample page and its DigiList. */
static uint8_t *make_test_vswap(size_t *file_size)
{
    uint8_t *data = calloc(32u, 1u);

    if (data == NULL) {
        return NULL;
    }

    write_u16(data, 3u);
    write_u16(data + 2u, 0u);
    write_u16(data + 4u, 1u);

    /* Page offsets start at byte 6; the page-length table follows them. */
    write_u32(data + 6u, 0u);
    write_u32(data + 10u, 24u);
    write_u32(data + 14u, 28u);
    write_u16(data + 18u, 0u);
    write_u16(data + 20u, 4u);
    write_u16(data + 22u, 4u);

    data[24] = 0u;
    data[25] = 64u;
    data[26] = 128u;
    data[27] = 255u;
    write_u16(data + 28u, 0u);
    write_u16(data + 30u, 4u);

    *file_size = 32u;
    return data;
}

void test_audio_archive_decodes_and_concatenates_sample_pages(void)
{
    WolfAudioArchive archive = {0};
    size_t file_size = 0u;
    uint8_t *data = make_test_vswap(&file_size);
    char error_message[128];

    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_TRUE(wolf_audio_archive_decode(
        data, file_size, &archive, error_message, sizeof(error_message)));
    TEST_ASSERT_EQUAL_UINT16(1u, archive.sample_count);
    TEST_ASSERT_EQUAL_UINT32(4u, archive.samples[0].length);
    TEST_ASSERT_EQUAL_UINT8(0u, archive.samples[0].data[0]);
    TEST_ASSERT_EQUAL_UINT8(64u, archive.samples[0].data[1]);
    TEST_ASSERT_EQUAL_UINT8(128u, archive.samples[0].data[2]);
    TEST_ASSERT_EQUAL_UINT8(255u, archive.samples[0].data[3]);

    wolf_audio_archive_destroy(&archive);
    free(data);
}

void test_audio_archive_rejects_invalid_index_page_offsets(void)
{
    WolfAudioArchive archive = {0};
    size_t file_size = 0u;
    uint8_t *data = make_test_vswap(&file_size);
    char error_message[128];

    TEST_ASSERT_NOT_NULL(data);
    write_u32(data + 14u, 100u);
    TEST_ASSERT_FALSE(wolf_audio_archive_decode(
        data, file_size, &archive, error_message, sizeof(error_message)));
    TEST_ASSERT_NOT_NULL(strstr(error_message, "index page"));

    free(data);
}

void test_audio_archive_rejects_samples_outside_sound_page_range(void)
{
    WolfAudioArchive archive = {0};
    size_t file_size = 0u;
    uint8_t *data = make_test_vswap(&file_size);
    char error_message[128];

    TEST_ASSERT_NOT_NULL(data);
    write_u16(data + 28u, 1u);
    TEST_ASSERT_FALSE(wolf_audio_archive_decode(
        data, file_size, &archive, error_message, sizeof(error_message)));
    TEST_ASSERT_NOT_NULL(strstr(error_message, "outside"));

    free(data);
}

void test_audio_archive_rejects_truncated_page_data(void)
{
    WolfAudioArchive archive = {0};
    size_t file_size = 0u;
    uint8_t *data = make_test_vswap(&file_size);
    char error_message[128];

    TEST_ASSERT_NOT_NULL(data);
    write_u16(data + 20u, 2u);
    TEST_ASSERT_FALSE(wolf_audio_archive_decode(
        data, file_size, &archive, error_message, sizeof(error_message)));
    TEST_ASSERT_NOT_NULL(strstr(error_message, "outside"));

    free(data);
}

void test_audio_archive_maps_named_wl6_sounds_to_original_samples(void)
{
    const WolfAudioDigiSound *sounds;
    size_t sound_count = 0u;
    bool found_pistol = false;
    bool found_scream_two = false;
    bool found_scream_three = false;

    sounds = wolf_audio_digi_sound_map(&sound_count);
    TEST_ASSERT_NOT_NULL(sounds);
    TEST_ASSERT_TRUE(sound_count >= 40u);

    for (size_t index = 0u; index < sound_count; ++index) {
        if (strcmp(sounds[index].name, "ATKPISTOLSND") == 0) {
            found_pistol = true;
            TEST_ASSERT_EQUAL_UINT16(5u, sounds[index].sample_index);
        } else if (strcmp(sounds[index].name, "DEATHSCREAM2SND") == 0) {
            found_scream_two = true;
            TEST_ASSERT_EQUAL_UINT16(13u, sounds[index].sample_index);
        } else if (strcmp(sounds[index].name, "DEATHSCREAM3SND") == 0) {
            found_scream_three = true;
            TEST_ASSERT_EQUAL_UINT16(13u, sounds[index].sample_index);
        }
    }

    TEST_ASSERT_TRUE(found_pistol);
    TEST_ASSERT_TRUE(found_scream_two);
    TEST_ASSERT_TRUE(found_scream_three);
}

void test_audio_archive_destroy_is_safe_for_empty_archives(void)
{
    WolfAudioArchive archive = {0};
    wolf_audio_archive_destroy(&archive);
    TEST_ASSERT_NULL(archive.samples);
    TEST_ASSERT_EQUAL_UINT16(0u, archive.sample_count);
}
