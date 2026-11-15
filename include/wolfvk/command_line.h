#ifndef WOLFVK_COMMAND_LINE_H
#define WOLFVK_COMMAND_LINE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct WolfOptions {
    uint32_t level_index;
    bool show_help;
} WolfOptions;

/* Parse --level as a one-based game level; omitted means the first level. */
bool wolf_parse_command_line(
    int argc,
    char *argv[],
    WolfOptions *options,
    char *error_message,
    size_t error_message_size);

#endif
