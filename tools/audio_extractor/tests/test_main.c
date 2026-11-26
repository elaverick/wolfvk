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
void test_adlib_archive_reads_original_header_and_sound_chunk(void);
void test_adlib_archive_rejects_short_and_non_monotonic_headers(void);
void test_adlib_archive_rejects_offset_outside_audio_data(void);
void test_adlib_archive_rejects_invalid_effect_length_and_indices(void);
void test_adlib_archive_destroy_is_safe_for_empty_archive(void);
void test_adlib_synth_renders_a_non_silent_original_frequency_stream(void);
void test_adlib_synth_rejects_missing_and_oversized_streams(void);

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_audio_archive_decodes_and_concatenates_sample_pages);
    RUN_TEST(test_audio_archive_rejects_invalid_index_page_offsets);
    RUN_TEST(test_audio_archive_rejects_samples_outside_sound_page_range);
    RUN_TEST(test_audio_archive_rejects_truncated_page_data);
    RUN_TEST(test_audio_archive_maps_named_wl6_sounds_to_original_samples);
    RUN_TEST(test_audio_archive_destroy_is_safe_for_empty_archives);
    RUN_TEST(test_adlib_archive_reads_original_header_and_sound_chunk);
    RUN_TEST(test_adlib_archive_rejects_short_and_non_monotonic_headers);
    RUN_TEST(test_adlib_archive_rejects_offset_outside_audio_data);
    RUN_TEST(test_adlib_archive_rejects_invalid_effect_length_and_indices);
    RUN_TEST(test_adlib_archive_destroy_is_safe_for_empty_archive);
    RUN_TEST(test_adlib_synth_renders_a_non_silent_original_frequency_stream);
    RUN_TEST(test_adlib_synth_rejects_missing_and_oversized_streams);
    return UNITY_END();
}
