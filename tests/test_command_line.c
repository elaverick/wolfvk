#include "unity.h"

#include "wolfvk/command_line.h"

#include <stdio.h>
#include <string.h>

void test_command_line_defaults_to_first_level(void)
{
    char program[] = "wolfvk";
    char *arguments[] = {program};
    WolfOptions options;
    char error_message[128];

    TEST_ASSERT_TRUE(wolf_parse_command_line(
        1, arguments, &options, error_message, sizeof(error_message)));
    TEST_ASSERT_EQUAL_UINT32(0u, options.level_index);
    TEST_ASSERT_FALSE(options.show_help);
}

void test_command_line_converts_one_based_level_to_zero_based_index(void)
{
    char program[] = "wolfvk";
    char option[] = "--level";
    char level[] = "3";
    char *arguments[] = {program, option, level};
    WolfOptions options;
    char error_message[128];

    TEST_ASSERT_TRUE(wolf_parse_command_line(
        3, arguments, &options, error_message, sizeof(error_message)));
    TEST_ASSERT_EQUAL_UINT32(2u, options.level_index);
    TEST_ASSERT_FALSE(options.show_help);
}

void test_command_line_accepts_first_level_explicitly(void)
{
    char program[] = "wolfvk";
    char option[] = "--level";
    char level[16] = "1";
    char *arguments[] = {program, option, level};
    WolfOptions options;
    char error_message[128];

    TEST_ASSERT_TRUE(wolf_parse_command_line(
        3, arguments, &options, error_message, sizeof(error_message)));
    TEST_ASSERT_EQUAL_UINT32(0u, options.level_index);

    /* The retail WL6 campaign has 60 maps and uses a 100-entry MAPHEAD table. */
    (void)snprintf(level, sizeof(level), "60");
    TEST_ASSERT_TRUE(wolf_parse_command_line(
        3, arguments, &options, error_message, sizeof(error_message)));
    TEST_ASSERT_EQUAL_UINT32(59u, options.level_index);
}

void test_command_line_rejects_invalid_level_numbers(void)
{
    const char *invalid_levels[] = {"0", "61", "101", "-1", "abc", "2abc"};
    char program[] = "wolfvk";
    char option[] = "--level";
    WolfOptions options;
    char error_message[128];

    for (size_t index = 0; index < sizeof(invalid_levels) / sizeof(invalid_levels[0]); ++index) {
        char level[16];
        char *arguments[] = {program, option, level};

        (void)snprintf(level, sizeof(level), "%s", invalid_levels[index]);
        TEST_ASSERT_FALSE(wolf_parse_command_line(
            3, arguments, &options, error_message, sizeof(error_message)));
        TEST_ASSERT_TRUE(error_message[0] != '\0');
    }
}

void test_command_line_rejects_missing_and_duplicate_level_options(void)
{
    char program[] = "wolfvk";
    char option[] = "--level";
    char level[] = "2";
    char *missing_arguments[] = {program, option};
    char *duplicate_arguments[] = {program, option, level, option, level};
    WolfOptions options;
    char error_message[128];

    TEST_ASSERT_FALSE(wolf_parse_command_line(
        2, missing_arguments, &options, error_message, sizeof(error_message)));
    TEST_ASSERT_FALSE(wolf_parse_command_line(
        5, duplicate_arguments, &options, error_message, sizeof(error_message)));
}

void test_command_line_rejects_unknown_options(void)
{
    char program[] = "wolfvk";
    char option[] = "--unknown";
    char *arguments[] = {program, option};
    WolfOptions options;
    char error_message[128];

    TEST_ASSERT_FALSE(wolf_parse_command_line(
        2, arguments, &options, error_message, sizeof(error_message)));
    TEST_ASSERT_TRUE(strstr(error_message, "Unknown") != NULL);
}

void test_command_line_supports_help(void)
{
    char program[] = "wolfvk";
    char option[] = "--help";
    char *arguments[] = {program, option};
    WolfOptions options;
    char error_message[128];

    TEST_ASSERT_TRUE(wolf_parse_command_line(
        2, arguments, &options, error_message, sizeof(error_message)));
    TEST_ASSERT_TRUE(options.show_help);
}

void test_command_line_rejects_invalid_api_arguments(void)
{
    WolfOptions options;
    char error_message[128];

    TEST_ASSERT_FALSE(wolf_parse_command_line(
        -1, NULL, &options, error_message, sizeof(error_message)));
    TEST_ASSERT_FALSE(wolf_parse_command_line(
        1, NULL, &options, error_message, sizeof(error_message)));
    TEST_ASSERT_FALSE(wolf_parse_command_line(
        1, NULL, NULL, error_message, sizeof(error_message)));
}

void test_command_line_defaults_to_the_original_menu_difficulty(void)
{
    char program[] = "wolfvk";
    char *arguments[] = {program};
    WolfOptions options;
    char error_message[128];

    TEST_ASSERT_TRUE(wolf_parse_command_line(
        1, arguments, &options, error_message, sizeof(error_message)));
    TEST_ASSERT_EQUAL_INT(WOLF_DIFFICULTY_MEDIUM, options.difficulty);
}

void test_command_line_selects_difficulty_in_menu_order(void)
{
    char program[] = "wolfvk";
    char option[] = "--difficulty";
    char value[16] = "1";
    char level_option[] = "--level";
    char level[] = "4";
    char *arguments[] = {program, option, value, level_option, level};
    WolfOptions options;
    char error_message[128];

    TEST_ASSERT_TRUE(wolf_parse_command_line(
        3, arguments, &options, error_message, sizeof(error_message)));
    TEST_ASSERT_EQUAL_INT(WOLF_DIFFICULTY_BABY, options.difficulty);

    /* Difficulty and level combine in either order. */
    (void)snprintf(value, sizeof(value), "4");
    TEST_ASSERT_TRUE(wolf_parse_command_line(
        5, arguments, &options, error_message, sizeof(error_message)));
    TEST_ASSERT_EQUAL_INT(WOLF_DIFFICULTY_HARD, options.difficulty);
    TEST_ASSERT_EQUAL_UINT32(3u, options.level_index);
}

void test_command_line_rejects_invalid_difficulties(void)
{
    const char *invalid_values[] = {"0", "5", "-1", "+2", "two", "2x", ""};
    char program[] = "wolfvk";
    char option[] = "--difficulty";
    char *missing_arguments[] = {program, option};
    WolfOptions options;
    char error_message[128];

    for (size_t index = 0; index < sizeof(invalid_values) / sizeof(invalid_values[0]); ++index) {
        char value[16];
        char *arguments[] = {program, option, value};

        (void)snprintf(value, sizeof(value), "%s", invalid_values[index]);
        TEST_ASSERT_FALSE(wolf_parse_command_line(
            3, arguments, &options, error_message, sizeof(error_message)));
        TEST_ASSERT_TRUE(strstr(error_message, "difficulty") != NULL);
    }

    TEST_ASSERT_FALSE(wolf_parse_command_line(
        2, missing_arguments, &options, error_message, sizeof(error_message)));

    {
        char first[] = "2";
        char second[] = "3";
        char *duplicate_arguments[] = {program, option, first, option, second};

        TEST_ASSERT_FALSE(wolf_parse_command_line(
            5, duplicate_arguments, &options, error_message, sizeof(error_message)));
        TEST_ASSERT_TRUE(strstr(error_message, "once") != NULL);
    }
}
