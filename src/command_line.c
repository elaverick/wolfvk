#include "wolfvk/command_line.h"

#include "wolfvk/level.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The original new-game menu lists four difficulties; users pick them by their position, 1 to 4. */
#define WOLF_DIFFICULTY_OPTION_COUNT 4u

/* Keep parser failures available to callers without requiring a non-null diagnostic buffer. */
static void write_error(char *error_message, size_t error_message_size, const char *message)
{
    if (error_message != NULL && error_message_size > 0u) {
        (void)snprintf(error_message, error_message_size, "%s", message);
    }
}

/* Accept only a whole decimal number from 1 to maximum, with nothing after it. */
static bool parse_one_based_number(const char *text, unsigned long maximum, unsigned long *value)
{
    char *end_pointer = NULL;
    unsigned long parsed;

    if (text == NULL || text[0] == '-' || text[0] == '+') {
        return false;
    }
    errno = 0;
    parsed = strtoul(text, &end_pointer, 10);
    if (errno == ERANGE || end_pointer == text || *end_pointer != '\0' ||
        parsed == 0u || parsed > maximum) {
        return false;
    }
    *value = parsed;
    return true;
}

/* Validate the one-based user options, then convert them once to the values the game uses. */
bool wolf_parse_command_line(
    int argc,
    char *argv[],
    WolfOptions *options,
    char *error_message,
    size_t error_message_size)
{
    bool level_was_set = false;
    bool difficulty_was_set = false;

    if (error_message != NULL && error_message_size > 0u) {
        error_message[0] = '\0';
    }

    if (argc < 0 || (argc > 0 && argv == NULL) || options == NULL) {
        write_error(error_message, error_message_size, "Invalid command line arguments.");
        return false;
    }

    options->level_index = 0u;
    options->difficulty = WOLF_DIFFICULTY_MEDIUM;
    options->show_help = false;

    for (int argument_index = 1; argument_index < argc; ++argument_index) {
        const char *argument = argv[argument_index];
        bool is_level;
        unsigned long value;

        if (argument == NULL) {
            write_error(error_message, error_message_size, "A command line argument is null.");
            return false;
        }

        if (strcmp(argument, "--help") == 0) {
            options->show_help = true;
            return true;
        }

        is_level = strcmp(argument, "--level") == 0;
        if (!is_level && strcmp(argument, "--difficulty") != 0) {
            write_error(error_message, error_message_size, "Unknown command line option.");
            return false;
        }

        if (is_level ? level_was_set : difficulty_was_set) {
            write_error(error_message, error_message_size, is_level
                ? "--level may only be specified once."
                : "--difficulty may only be specified once.");
            return false;
        }

        if (argument_index + 1 >= argc || argv[argument_index + 1] == NULL) {
            write_error(error_message, error_message_size, is_level
                ? "--level requires a level number."
                : "--difficulty requires a number from 1 to 4.");
            return false;
        }
        ++argument_index;

        if (is_level) {
            if (!parse_one_based_number(argv[argument_index], WOLF_LEVEL_MAX_MAPS, &value)) {
                write_error(
                    error_message, error_message_size,
                    "The level number must be between 1 and 60.");
                return false;
            }
            options->level_index = (uint32_t)(value - 1u);
            level_was_set = true;
        } else {
            if (!parse_one_based_number(argv[argument_index], WOLF_DIFFICULTY_OPTION_COUNT, &value)) {
                write_error(
                    error_message, error_message_size,
                    "The difficulty must be between 1 and 4.");
                return false;
            }
            options->difficulty = (WolfDifficulty)(value - 1u);
            difficulty_was_set = true;
        }
    }

    return true;
}
