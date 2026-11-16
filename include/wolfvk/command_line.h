#ifndef WOLFVK_COMMAND_LINE_H
#define WOLFVK_COMMAND_LINE_H

#include "wolfvk/level_objects.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct WolfOptions {
    uint32_t level_index;
    /* Decides which info-plane enemies spawn; defaults to the original menu's "Bring 'em on!". */
    WolfDifficulty difficulty;
    bool show_help;
} WolfOptions;

/*
 * Parse --level as a one-based game level (omitted means the first level)
 * and --difficulty as 1-4 in the original menu's order (omitted means 3).
 */
bool wolf_parse_command_line(
    int argc,
    char *argv[],
    WolfOptions *options,
    char *error_message,
    size_t error_message_size);

#endif
