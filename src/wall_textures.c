#include "wolfvk/wall_textures.h"

#include "game_palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WOLF_VSWAP_HEADER_PREFIX_SIZE 6u
#define WOLF_VSWAP_OFFSET_ENTRY_SIZE 4u
#define WOLF_VSWAP_LENGTH_ENTRY_SIZE 2u
#define WOLF_VSWAP_WALL_PAGE_BYTES \
    (WOLF_WALL_TEXTURE_WIDTH * WOLF_WALL_TEXTURE_HEIGHT)

/* VSWAP stores each 16-bit header value independently of host alignment and endianness. */
static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

/* VSWAP page offsets are absolute 32-bit file positions. */
static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

/* Retain a bounded explanation for malformed or inaccessible user-supplied assets. */
static void write_error(
    char *error_message,
    size_t error_message_size,
    const char *message)
{
    if (error_message != NULL && error_message_size > 0u) {
        (void)snprintf(error_message, error_message_size, "%s", message);
    }
}

/* Use the secure CRT on Windows while preserving normal binary fopen elsewhere. */
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

/* Read the complete page archive so every page range can be checked before copying. */
static bool read_vswap_file(
    const char *path,
    uint8_t **file_data,
    size_t *file_size,
    char *error_message,
    size_t error_message_size)
{
    FILE *file;
    long file_length;
    uint8_t *data;
    size_t bytes_read;

    if (path == NULL || file_data == NULL || file_size == NULL) {
        write_error(error_message, error_message_size, "Invalid VSWAP file arguments.");
        return false;
    }

    *file_data = NULL;
    *file_size = 0u;
    file = open_binary_file(path);
    if (file == NULL) {
        write_error(error_message, error_message_size, "Could not open VSWAP.WL6.");
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0 || (file_length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        write_error(error_message, error_message_size, "Could not determine the VSWAP file size.");
        return false;
    }

    if ((size_t)file_length < WOLF_VSWAP_HEADER_PREFIX_SIZE) {
        fclose(file);
        write_error(error_message, error_message_size, "VSWAP.WL6 is too small to contain its header.");
        return false;
    }

    data = malloc((size_t)file_length);
    if (data == NULL) {
        fclose(file);
        write_error(error_message, error_message_size, "Could not allocate memory for VSWAP.WL6.");
        return false;
    }

    bytes_read = fread(data, 1u, (size_t)file_length, file);
    if (fclose(file) != 0 || bytes_read != (size_t)file_length) {
        free(data);
        write_error(error_message, error_message_size, "Could not read the complete VSWAP file.");
        return false;
    }

    *file_data = data;
    *file_size = (size_t)file_length;
    return true;
}

/* Expand the indexed wall pages once so the renderer can upload one contiguous texture array. */
bool wolf_wall_textures_decode_vswap(
    const uint8_t *file_data,
    size_t file_size,
    WolfWallTextures *textures,
    char *error_message,
    size_t error_message_size)
{
    uint16_t chunk_count;
    uint16_t first_sprite_page;
    uint16_t first_sound_page;
    size_t header_size;
    size_t offset_table;
    size_t length_table;
    size_t rgba_size;
    uint8_t *rgba_pages;

    if (error_message != NULL && error_message_size > 0u) {
        error_message[0] = '\0';
    }

    if (file_data == NULL || textures == NULL ||
        file_size < WOLF_VSWAP_HEADER_PREFIX_SIZE) {
        write_error(error_message, error_message_size, "Invalid VSWAP data or output arguments.");
        return false;
    }
    if (textures->rgba_pages != NULL || textures->page_count != 0u) {
        write_error(error_message, error_message_size, "The wall texture output must be empty.");
        return false;
    }

    chunk_count = read_u16_le(file_data);
    first_sprite_page = read_u16_le(file_data + 2u);
    first_sound_page = read_u16_le(file_data + 4u);

    /*
     * Original Wolf3D places all uncompressed wall pages before the first
     * sprite page, then digitised sounds after the sprite range.
     */
    if (chunk_count == 0u || first_sprite_page < 2u ||
        (first_sprite_page & 1u) != 0u ||
        first_sprite_page > first_sound_page ||
        first_sound_page > chunk_count) {
        write_error(error_message, error_message_size, "VSWAP page boundaries are invalid.");
        return false;
    }

    if ((size_t)chunk_count >
        (SIZE_MAX - WOLF_VSWAP_HEADER_PREFIX_SIZE) /
            (WOLF_VSWAP_OFFSET_ENTRY_SIZE + WOLF_VSWAP_LENGTH_ENTRY_SIZE)) {
        write_error(error_message, error_message_size, "VSWAP page table size overflows.");
        return false;
    }

    header_size = WOLF_VSWAP_HEADER_PREFIX_SIZE +
        (size_t)chunk_count * WOLF_VSWAP_OFFSET_ENTRY_SIZE +
        (size_t)chunk_count * WOLF_VSWAP_LENGTH_ENTRY_SIZE;
    if (header_size > file_size) {
        write_error(error_message, error_message_size, "VSWAP page tables are truncated or oversized.");
        return false;
    }

    rgba_size = (size_t)first_sprite_page * WOLF_WALL_TEXTURE_RGBA_SIZE;
    rgba_pages = malloc(rgba_size);
    if (rgba_pages == NULL) {
        write_error(error_message, error_message_size, "Could not allocate the decoded wall textures.");
        return false;
    }

    offset_table = WOLF_VSWAP_HEADER_PREFIX_SIZE;
    length_table = offset_table + (size_t)chunk_count * WOLF_VSWAP_OFFSET_ENTRY_SIZE;

    for (uint16_t page_index = 0u; page_index < first_sprite_page; ++page_index) {
        uint32_t page_offset = read_u32_le(
            file_data + offset_table + (size_t)page_index * WOLF_VSWAP_OFFSET_ENTRY_SIZE);
        uint16_t page_length = read_u16_le(
            file_data + length_table + (size_t)page_index * WOLF_VSWAP_LENGTH_ENTRY_SIZE);
        const uint8_t *indexed_page;
        uint8_t *rgba_page = rgba_pages +
            (size_t)page_index * WOLF_WALL_TEXTURE_RGBA_SIZE;

        /*
         * A wall occupies every texel of a 64x64 page. The original renderer
         * indexes the last column and row as well, so a complete 4096-byte page
         * is required; sprite and sound chunks are deliberately not decoded.
         */
        if (page_offset < header_size || page_length < WOLF_VSWAP_WALL_PAGE_BYTES ||
            (size_t)page_offset > file_size ||
            (size_t)page_length > file_size - (size_t)page_offset) {
            free(rgba_pages);
            write_error(error_message, error_message_size, "A VSWAP wall page is missing or truncated.");
            return false;
        }

        indexed_page = file_data + page_offset;

        /*
         * Wall texels are stored as 64 consecutive columns. Convert that
         * column-major representation to row-major RGBA for a Vulkan image.
         */
        for (uint32_t column = 0u; column < WOLF_WALL_TEXTURE_WIDTH; ++column) {
            for (uint32_t row = 0u; row < WOLF_WALL_TEXTURE_HEIGHT; ++row) {
                uint8_t palette_index = indexed_page[
                    (size_t)column * WOLF_WALL_TEXTURE_HEIGHT + row];
                size_t destination = (
                    (size_t)row * WOLF_WALL_TEXTURE_WIDTH + column) *
                    WOLF_WALL_TEXTURE_PIXEL_BYTES;

                memcpy(
                    rgba_page + destination,
                    wolf_game_palette_rgba + (size_t)palette_index * WOLF_WALL_TEXTURE_PIXEL_BYTES,
                    WOLF_WALL_TEXTURE_PIXEL_BYTES);
            }
        }
    }

    textures->page_count = first_sprite_page;
    textures->rgba_pages = rgba_pages;
    return true;
}

/* Load the archive from disk, then delegate all page validation and expansion to the decoder. */
bool wolf_wall_textures_load_vswap(
    const char *path,
    WolfWallTextures *textures,
    char *error_message,
    size_t error_message_size)
{
    uint8_t *file_data = NULL;
    size_t file_size = 0u;
    bool decoded;

    if (textures == NULL || textures->rgba_pages != NULL || textures->page_count != 0u) {
        write_error(error_message, error_message_size, "The wall texture output must be empty.");
        return false;
    }

    if (!read_vswap_file(path, &file_data, &file_size, error_message, error_message_size)) {
        return false;
    }

    decoded = wolf_wall_textures_decode_vswap(
        file_data, file_size, textures, error_message, error_message_size);
    free(file_data);
    return decoded;
}

/* Match the original SetupWalls mapping: even pages are horizontal, odd pages vertical. */
bool wolf_wall_texture_page_for_tile(
    uint16_t wall_tile,
    bool vertical_face,
    uint16_t page_count,
    uint16_t *page_index)
{
    uint16_t wall_type;
    uint16_t selected_page;

    if (page_index == NULL || wall_tile == 0u || wall_tile >= 90u) {
        return false;
    }

    wall_type = (uint16_t)(wall_tile & 0x3fu);
    if (wall_type == 0u) {
        return false;
    }

    selected_page = (uint16_t)(((wall_type - 1u) * 2u) + (vertical_face ? 1u : 0u));
    if (selected_page >= page_count) {
        return false;
    }

    *page_index = selected_page;
    return true;
}

/* Free the decoded pixel array and leave the structure safe for repeated teardown. */
void wolf_wall_textures_destroy(WolfWallTextures *textures)
{
    if (textures == NULL) {
        return;
    }

    free(textures->rgba_pages);
    textures->rgba_pages = NULL;
    textures->page_count = 0u;
}
