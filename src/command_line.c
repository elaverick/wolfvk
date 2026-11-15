#include "wolfvk/command_line.h"

#include "wolfvk/level.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_error(char *error_message, size_t error_message_size, const char *message)
{
    if (error_message != NULL && error_message_size > 0u) {
        (void)snprintf(error_message, error_message_size, "%s", message);
    }
}

bool wolf_parse_command_line(
    int argc,
    char *argv[],
    WolfOptions *options,
    char *error_message,
    size_t error_message_size)
{
    bool level_was_set = false;

    if (error_message != NULL && error_message_size > 0u) {
        error_message[0] = '\0';
    }

    if (argc < 0 || (argc > 0 && argv == NULL) || options == NULL) {
        write_error(error_message, error_message_size, "Invalid command line arguments.");
        return false;
    }

    options->level_index = 0u;
    options->show_help = false;

    for (int argument_index = 1; argument_index < argc; ++argument_index) {
        const char *argument = argv[argument_index];

        if (argument == NULL) {
            write_error(error_message, error_message_size, "A command line argument is null.");
            return false;
        }

        if (strcmp(argument, "--help") == 0) {
            options->show_help = true;
            return true;
        }

        if (strcmp(argument, "--level") != 0) {
            write_error(error_message, error_message_size, "Unknown command line option.");
            return false;
        }

        if (level_was_set) {
            write_error(error_message, error_message_size, "--level may only be specified once.");
            return false;
        }

        if (argument_index + 1 >= argc || argv[argument_index + 1] == NULL) {
            write_error(error_message, error_message_size, "--level requires a level number.");
            return false;
        }

        {
            char *end_pointer = NULL;
            unsigned long requested_level;

            errno = 0;
            requested_level = strtoul(argv[++argument_index], &end_pointer, 10);
            if (errno == ERANGE || end_pointer == argv[argument_index] ||
                *end_pointer != '\0' || requested_level == 0u ||
                requested_level > WOLF_LEVEL_MAX_MAPS) {
                write_error(
                    error_message, error_message_size,
                    "The level number must be between 1 and 60.");
                return false;
            }

            options->level_index = (uint32_t)(requested_level - 1u);
            level_was_set = true;
        }
    }

    return true;
}
