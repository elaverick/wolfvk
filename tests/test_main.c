#include "unity.h"

void test_window_creation(void);
void test_window_is_resizable(void);
void test_window_close_event_can_be_queued(void);
void test_graphics_and_present_on_same_queue_family(void);
void test_graphics_and_present_on_separate_queue_families(void);
void test_queue_selection_rejects_missing_graphics_queue(void);
void test_queue_selection_rejects_missing_present_queue(void);
void test_queue_selection_rejects_null_arguments(void);

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
    return UNITY_END();
}
