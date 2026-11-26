#include "unity.h"

/* The current tests need no per-test fixture setup or teardown. */
void setUp(void) {}
void tearDown(void) {}

void test_audio_archive_decodes_and_concatenates_sample_pages(void);
void test_audio_archive_rejects_invalid_index_page_offsets(void);
void test_audio_archive_rejects_samples_outside_sound_page_range(void);
void test_audio_archive_rejects_truncated_page_data(void);
void test_audio_archive_maps_named_wl6_sounds_to_original_samples(void);
void test_audio_archive_destroy_is_safe_for_empty_archives(void);

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_audio_archive_decodes_and_concatenates_sample_pages);
    RUN_TEST(test_audio_archive_rejects_invalid_index_page_offsets);
    RUN_TEST(test_audio_archive_rejects_samples_outside_sound_page_range);
    RUN_TEST(test_audio_archive_rejects_truncated_page_data);
    RUN_TEST(test_audio_archive_maps_named_wl6_sounds_to_original_samples);
    RUN_TEST(test_audio_archive_destroy_is_safe_for_empty_archives);
    return UNITY_END();
}
