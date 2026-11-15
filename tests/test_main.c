#include "unity.h"

void test_window_creation(void);
void test_window_is_resizable(void);
void test_window_close_event_can_be_queued(void);
void test_graphics_and_present_on_same_queue_family(void);
void test_graphics_and_present_on_separate_queue_families(void);
void test_queue_selection_rejects_missing_graphics_queue(void);
void test_queue_selection_rejects_missing_present_queue(void);
void test_queue_selection_rejects_null_arguments(void);
void test_queue_selection_ignores_empty_queue_families(void);
void test_renderer_rejects_window_without_vulkan_support(void);
void test_surface_format_prefers_srgb(void);
void test_surface_format_falls_back_to_first_supported_format(void);
void test_present_mode_prefers_mailbox(void);
void test_present_mode_falls_back_to_fifo(void);
void test_surface_format_handles_undefined_single_format(void);
void test_swapchain_selection_handles_empty_lists(void);
void test_renderer_creates_swapchain_and_presents_triangle(void);
void test_device_scoring_prefers_discrete_gpus(void);

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_window_creation);
    RUN_TEST(test_window_is_resizable);
    RUN_TEST(test_window_close_event_can_be_queued);
    RUN_TEST(test_graphics_and_present_on_same_queue_family);
    RUN_TEST(test_graphics_and_present_on_separate_queue_families);
    RUN_TEST(test_queue_selection_rejects_missing_graphics_queue);
    RUN_TEST(test_queue_selection_rejects_missing_present_queue);
    RUN_TEST(test_queue_selection_rejects_null_arguments);
    RUN_TEST(test_queue_selection_ignores_empty_queue_families);
    RUN_TEST(test_renderer_rejects_window_without_vulkan_support);
    RUN_TEST(test_surface_format_prefers_srgb);
    RUN_TEST(test_surface_format_falls_back_to_first_supported_format);
    RUN_TEST(test_present_mode_prefers_mailbox);
    RUN_TEST(test_present_mode_falls_back_to_fifo);
    RUN_TEST(test_surface_format_handles_undefined_single_format);
    RUN_TEST(test_swapchain_selection_handles_empty_lists);
    RUN_TEST(test_renderer_creates_swapchain_and_presents_triangle);
    RUN_TEST(test_device_scoring_prefers_discrete_gpus);
    return UNITY_END();
}
