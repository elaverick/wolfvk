#include "wolfvk/level.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WOLF_MAPHEAD_HEADER_SIZE (2u + WOLF_LEVEL_MAX_MAPS * 4u)
#define WOLF_MAP_HEADER_SIZE 38u
#define WOLF_CARMACK_NEAR_TAG 0xa7u
#define WOLF_CARMACK_FAR_TAG 0xa8u
#define WOLF_RLEW_PLANE_BYTES (WOLF_LEVEL_PLANE_WORDS * sizeof(uint16_t))

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

static void write_error(char *error_message, size_t error_message_size, const char *message)
{
    if (error_message != NULL && error_message_size > 0u) {
        (void)snprintf(error_message, error_message_size, "%s", message);
    }
}

static FILE *open_binary_file(const char *path)
{
    FILE *file = NULL;

#if defined(_MSC_VER)
    if (fopen_s(&file, path, "rb") != 0) {
        return NULL;
    }
#else
    file = fopen(path, "rb");
#endif

    return file;
}

static bool read_file(
    const char *path,
    uint8_t **data,
    size_t *data_size,
    char *error_message,
    size_t error_message_size)
{
    FILE *file = open_binary_file(path);
    long file_length;
    uint8_t *file_data;
    size_t bytes_read;

    if (file == NULL) {
        write_error(error_message, error_message_size, "Could not open a map data file.");
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        write_error(error_message, error_message_size, "Could not seek to the end of a map data file.");
        return false;
    }

    file_length = ftell(file);
    if (file_length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        write_error(error_message, error_message_size, "Could not determine a map data file size.");
        return false;
    }

    file_data = malloc(file_length > 0 ? (size_t)file_length : 1u);
    if (file_data == NULL) {
        fclose(file);
        write_error(error_message, error_message_size, "Could not allocate memory for a map data file.");
        return false;
    }

    bytes_read = fread(file_data, 1u, (size_t)file_length, file);
    if (bytes_read != (size_t)file_length || fclose(file) != 0) {
        free(file_data);
        write_error(error_message, error_message_size, "Could not read a complete map data file.");
        return false;
    }

    *data = file_data;
    *data_size = (size_t)file_length;
    return true;
}

bool wolf_level_decode_carmack(
    const uint8_t *source,
    size_t source_size,
    uint8_t *destination,
    size_t destination_size)
{
    size_t input_offset = 2u;
    size_t output_word_count;
    size_t output_word_index = 0u;

    if (source == NULL || destination == NULL || source_size < 2u ||
        destination_size < 2u || (destination_size & 1u) != 0u ||
        destination_size > UINT16_MAX ||
        read_u16_le(source) != destination_size) {
        return false;
    }

    output_word_count = destination_size / sizeof(uint16_t);

    while (output_word_index < output_word_count) {
        uint16_t word;
        uint8_t tag;
        uint8_t count;

        if (source_size - input_offset < 2u) {
            return false;
        }

        word = read_u16_le(source + input_offset);
        input_offset += 2u;
        tag = (uint8_t)(word >> 8);
        count = (uint8_t)(word & 0xffu);

        if (tag != WOLF_CARMACK_NEAR_TAG && tag != WOLF_CARMACK_FAR_TAG) {
            destination[output_word_index * 2u] = (uint8_t)(word & 0xffu);
            destination[output_word_index * 2u + 1u] = (uint8_t)(word >> 8);
            ++output_word_index;
            continue;
        }

        if (count == 0u) {
            if (input_offset >= source_size) {
                return false;
            }

            destination[output_word_index * 2u] = source[input_offset++];
            destination[output_word_index * 2u + 1u] = tag;
            ++output_word_index;
            continue;
        }

        if ((size_t)count > output_word_count - output_word_index) {
            return false;
        }

        {
            size_t copy_word_index;

            if (tag == WOLF_CARMACK_NEAR_TAG) {
                uint8_t near_offset;

                if (input_offset >= source_size) {
                    return false;
                }

                near_offset = source[input_offset++];
                if (near_offset == 0u || (size_t)near_offset > output_word_index) {
                    return false;
                }

                copy_word_index = output_word_index - (size_t)near_offset;
            } else {
                uint16_t far_offset;

                if (source_size - input_offset < 2u) {
                    return false;
                }

                far_offset = read_u16_le(source + input_offset);
                input_offset += 2u;
                if ((size_t)far_offset >= output_word_index) {
                    return false;
                }

                copy_word_index = far_offset;
            }

            /* Copy one word at a time so overlapping back-references expand correctly. */
            for (uint8_t copy_index = 0u; copy_index < count; ++copy_index) {
                destination[output_word_index * 2u] =
                    destination[copy_word_index * 2u];
                destination[output_word_index * 2u + 1u] =
                    destination[copy_word_index * 2u + 1u];
                ++output_word_index;
                ++copy_word_index;
            }
        }
    }

    return true;
}

bool wolf_level_decode_rlew(
    const uint8_t *source,
    size_t source_size,
    uint16_t rlew_tag,
    uint16_t *destination,
    size_t destination_word_count)
{
    size_t input_offset = 2u;
    size_t output_word_index = 0u;
    size_t expected_byte_count;

    if (source == NULL || destination == NULL ||
        destination_word_count > SIZE_MAX / sizeof(uint16_t)) {
        return false;
    }

    expected_byte_count = destination_word_count * sizeof(uint16_t);
    if (source_size < 2u || expected_byte_count > UINT16_MAX ||
        read_u16_le(source) != expected_byte_count) {
        return false;
    }

    while (output_word_index < destination_word_count) {
        uint16_t value;

        if (source_size - input_offset < 2u) {
            return false;
        }

        value = read_u16_le(source + input_offset);
        input_offset += 2u;

        if (value != rlew_tag) {
            destination[output_word_index++] = value;
            continue;
        }

        if (source_size - input_offset < 4u) {
            return false;
        }

        {
            uint16_t repeat_count = read_u16_le(source + input_offset);
            uint16_t repeat_value = read_u16_le(source + input_offset + 2u);

            input_offset += 4u;
            if (repeat_count == 0u ||
                (size_t)repeat_count > destination_word_count - output_word_index) {
                return false;
            }

            for (uint16_t repeat_index = 0u; repeat_index < repeat_count; ++repeat_index) {
                destination[output_word_index++] = repeat_value;
            }
        }
    }

    return true;
}

static bool decode_map_plane(
    const uint8_t *compressed_plane,
    size_t compressed_plane_size,
    uint16_t rlew_tag,
    uint16_t *destination)
{
    uint16_t carmack_expanded_size;
    uint8_t *rlew_stream;
    bool decoded;

    if (compressed_plane == NULL || compressed_plane_size < 2u || destination == NULL) {
        return false;
    }

    carmack_expanded_size = read_u16_le(compressed_plane);
    if (carmack_expanded_size < 2u || (carmack_expanded_size & 1u) != 0u) {
        return false;
    }

    rlew_stream = malloc(carmack_expanded_size);
    if (rlew_stream == NULL) {
        return false;
    }

    decoded = wolf_level_decode_carmack(
        compressed_plane, compressed_plane_size,
        rlew_stream, carmack_expanded_size);
    if (decoded) {
        decoded = wolf_level_decode_rlew(
            rlew_stream, carmack_expanded_size,
            rlew_tag, destination, WOLF_LEVEL_PLANE_WORDS);
    }

    free(rlew_stream);
    return decoded;
}

bool wolf_level_load(
    const char *maphead_path,
    const char *gamemaps_path,
    uint32_t map_index,
    WolfLevel *level,
    char *error_message,
    size_t error_message_size)
{
    uint8_t *maphead_data = NULL;
    uint8_t *gamemaps_data = NULL;
    size_t maphead_size = 0u;
    size_t gamemaps_size = 0u;
    uint16_t rlew_tag;
    uint32_t map_header_offset;
    const uint8_t *map_header;
    WolfLevel decoded_level = {0};
    bool loaded = false;

    if (error_message != NULL && error_message_size > 0u) {
        error_message[0] = '\0';
    }

    if (maphead_path == NULL || gamemaps_path == NULL || level == NULL ||
        map_index >= WOLF_LEVEL_MAX_MAPS) {
        write_error(error_message, error_message_size, "Invalid map loader arguments.");
        return false;
    }

    if (!read_file(
            maphead_path, &maphead_data, &maphead_size,
            error_message, error_message_size)) {
        goto cleanup;
    }

    if (!read_file(
            gamemaps_path, &gamemaps_data, &gamemaps_size,
            error_message, error_message_size)) {
        goto cleanup;
    }

    if (maphead_size < WOLF_MAPHEAD_HEADER_SIZE) {
        write_error(error_message, error_message_size, "MAPHEAD file is too small for its offset table.");
        goto cleanup;
    }

    rlew_tag = read_u16_le(maphead_data);
    map_header_offset = read_u32_le(maphead_data + 2u + (size_t)map_index * 4u);
    if (map_header_offset == UINT32_MAX) {
        write_error(error_message, error_message_size, "The requested map is a sparse map entry.");
        goto cleanup;
    }

    if ((size_t)map_header_offset > gamemaps_size ||
        gamemaps_size - (size_t)map_header_offset < WOLF_MAP_HEADER_SIZE) {
        write_error(error_message, error_message_size, "The map header points outside GAMEMAPS.");
        goto cleanup;
    }

    map_header = gamemaps_data + map_header_offset;
    decoded_level.width = read_u16_le(map_header + 18u);
    decoded_level.height = read_u16_le(map_header + 20u);
    if (decoded_level.width == 0u || decoded_level.height == 0u ||
        decoded_level.width > WOLF_LEVEL_PLANE_WIDTH ||
        decoded_level.height > WOLF_LEVEL_PLANE_HEIGHT) {
        write_error(error_message, error_message_size, "The map dimensions are outside the original 64 by 64 plane.");
        goto cleanup;
    }

    memcpy(decoded_level.name, map_header + 22u, 16u);
    decoded_level.name[16] = '\0';

    for (uint32_t plane_index = 0u; plane_index < 2u; ++plane_index) {
        uint32_t plane_offset = read_u32_le(map_header + plane_index * 4u);
        uint16_t plane_length = read_u16_le(map_header + 12u + plane_index * 2u);
        uint16_t *destination = plane_index == 0u
            ? decoded_level.wall_tiles
            : decoded_level.object_tiles;

        if (plane_length == 0u || (size_t)plane_offset > gamemaps_size ||
            (size_t)plane_length > gamemaps_size - (size_t)plane_offset) {
            write_error(error_message, error_message_size, "A map plane points outside GAMEMAPS.");
            goto cleanup;
        }

        if (!decode_map_plane(
                gamemaps_data + plane_offset, plane_length, rlew_tag, destination)) {
            write_error(error_message, error_message_size, "A map plane failed Carmack or RLEW decompression.");
            goto cleanup;
        }
    }

    *level = decoded_level;
    loaded = true;

cleanup:
    free(maphead_data);
    free(gamemaps_data);
    return loaded;
}
