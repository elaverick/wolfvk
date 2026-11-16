#include "vswap_sprites.h"

#include "file_io.h"

#include <stdlib.h>
#include <string.h>

#define VSWAP_HEADER_PREFIX_SIZE 6u
#define SPRITE_HEADER_FIXED_SIZE 4u
#define SPRITE_POST_SIZE 6u

bool wolf_sprite_page_decode(
    const uint8_t *page,
    size_t page_size,
    WolfSpriteImage *image,
    char *error_message,
    size_t error_message_size)
{
    uint16_t left_column;
    uint16_t right_column;
    size_t column_count;

    wolf_sprite_image_clear(image);
    if (page == NULL || page_size < SPRITE_HEADER_FIXED_SIZE) {
        wolf_set_error(error_message, error_message_size, "Sprite page is missing or too small.");
        return false;
    }

    left_column = wolf_read_u16_le(page);
    right_column = wolf_read_u16_le(page + 2u);
    if (left_column > right_column || right_column >= WOLF_SPRITE_SIZE) {
        wolf_set_error(error_message, error_message_size, "Sprite column range is invalid.");
        return false;
    }
    column_count = (size_t)(right_column - left_column) + 1u;
    if (SPRITE_HEADER_FIXED_SIZE + column_count * 2u > page_size) {
        wolf_set_error(error_message, error_message_size, "Sprite column table is truncated.");
        return false;
    }

    for (size_t column_slot = 0u; column_slot < column_count; ++column_slot) {
        int column = (int)left_column + (int)column_slot;
        size_t post_offset = wolf_read_u16_le(page + SPRITE_HEADER_FIXED_SIZE + column_slot * 2u);

        /*
         * Walk the column's posts. Each post names a contiguous run of drawn
         * rows; pixels between posts stay transparent. The pixel source word
         * is pre-biased by the start row so that source + row addresses the
         * texel, exactly as the original compiled scalers used it.
         */
        for (;;) {
            uint16_t end_row_doubled;
            uint16_t source_base;
            uint16_t start_row_doubled;

            if (post_offset + 2u > page_size) {
                wolf_set_error(error_message, error_message_size, "Sprite post list is truncated.");
                return false;
            }
            end_row_doubled = wolf_read_u16_le(page + post_offset);
            if (end_row_doubled == 0u) {
                break;
            }
            if (post_offset + SPRITE_POST_SIZE > page_size) {
                wolf_set_error(error_message, error_message_size, "Sprite post is truncated.");
                return false;
            }
            source_base = wolf_read_u16_le(page + post_offset + 2u);
            start_row_doubled = wolf_read_u16_le(page + post_offset + 4u);
            if ((end_row_doubled & 1u) != 0u || (start_row_doubled & 1u) != 0u ||
                start_row_doubled >= end_row_doubled ||
                end_row_doubled > WOLF_SPRITE_SIZE * 2u) {
                wolf_set_error(error_message, error_message_size, "Sprite post rows are invalid.");
                return false;
            }

            for (int row = start_row_doubled / 2; row < end_row_doubled / 2; ++row) {
                /* 16-bit wrap-around matches the original segment-offset arithmetic. */
                size_t source = (uint16_t)(source_base + (uint16_t)row);
                size_t pixel = wolf_sprite_pixel_index(column, row);

                if (source >= page_size) {
                    wolf_set_error(error_message, error_message_size, "Sprite pixel data is out of range.");
                    return false;
                }
                image->palette_index[pixel] = page[source];
                image->opaque[pixel] = 1u;
            }
            post_offset += SPRITE_POST_SIZE;
        }
    }
    return true;
}

bool wolf_sprite_page_encode(
    const WolfSpriteImage *image,
    uint8_t *page,
    size_t page_capacity,
    size_t *page_size)
{
    int left_column = WOLF_SPRITE_SIZE;
    int right_column = -1;
    size_t column_count;
    size_t post_bytes = 0u;
    size_t pixel_bytes = 0u;
    size_t post_cursor;
    size_t pixel_cursor;

    for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
        bool column_drawn = false;
        bool in_post = false;

        for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
            bool drawn = image->opaque[wolf_sprite_pixel_index(column, row)] != 0u;

            if (drawn && !in_post) {
                post_bytes += SPRITE_POST_SIZE;
            }
            pixel_bytes += drawn ? 1u : 0u;
            column_drawn = column_drawn || drawn;
            in_post = drawn;
        }
        if (column_drawn) {
            left_column = column < left_column ? column : left_column;
            right_column = column;
        }
    }

    /* The original format cannot express a blank sprite; use one empty column. */
    if (right_column < 0) {
        left_column = 0;
        right_column = 0;
    }
    column_count = (size_t)(right_column - left_column) + 1u;
    post_bytes += column_count * 2u;
    *page_size = SPRITE_HEADER_FIXED_SIZE + column_count * 2u + post_bytes + pixel_bytes;
    if (*page_size > page_capacity || *page_size > 0xffffu) {
        return false;
    }

    wolf_write_u16_le(page, (uint16_t)left_column);
    wolf_write_u16_le(page + 2u, (uint16_t)right_column);
    post_cursor = SPRITE_HEADER_FIXED_SIZE + column_count * 2u;
    pixel_cursor = post_cursor + post_bytes;

    for (int column = left_column; column <= right_column; ++column) {
        int row = 0;

        wolf_write_u16_le(
            page + SPRITE_HEADER_FIXED_SIZE + (size_t)(column - left_column) * 2u,
            (uint16_t)post_cursor);
        while (row < WOLF_SPRITE_SIZE) {
            int start_row;

            if (!image->opaque[wolf_sprite_pixel_index(column, row)]) {
                ++row;
                continue;
            }
            start_row = row;
            while (row < WOLF_SPRITE_SIZE && image->opaque[wolf_sprite_pixel_index(column, row)]) {
                page[pixel_cursor + (size_t)(row - start_row)] =
                    image->palette_index[wolf_sprite_pixel_index(column, row)];
                ++row;
            }
            wolf_write_u16_le(page + post_cursor, (uint16_t)(row * 2));
            wolf_write_u16_le(page + post_cursor + 2u, (uint16_t)(pixel_cursor - (size_t)start_row));
            wolf_write_u16_le(page + post_cursor + 4u, (uint16_t)(start_row * 2));
            post_cursor += SPRITE_POST_SIZE;
            pixel_cursor += (size_t)(row - start_row);
        }
        wolf_write_u16_le(page + post_cursor, 0u);
        post_cursor += 2u;
    }
    return true;
}

bool wolf_sprite_archive_decode(
    const uint8_t *file_data,
    size_t file_size,
    WolfSpriteArchive *archive,
    char *error_message,
    size_t error_message_size)
{
    uint16_t chunk_count;
    uint16_t first_sprite_page;
    uint16_t first_sound_page;
    size_t header_size;
    size_t length_table;
    WolfSpriteImage *sprites;

    memset(archive, 0, sizeof(*archive));
    if (file_data == NULL || file_size < VSWAP_HEADER_PREFIX_SIZE) {
        wolf_set_error(error_message, error_message_size, "VSWAP data is missing or too small.");
        return false;
    }

    chunk_count = wolf_read_u16_le(file_data);
    first_sprite_page = wolf_read_u16_le(file_data + 2u);
    first_sound_page = wolf_read_u16_le(file_data + 4u);
    if (chunk_count == 0u || first_sprite_page > first_sound_page ||
        first_sound_page > chunk_count || first_sprite_page == first_sound_page) {
        wolf_set_error(error_message, error_message_size, "VSWAP sprite page boundaries are invalid.");
        return false;
    }
    header_size = VSWAP_HEADER_PREFIX_SIZE + (size_t)chunk_count * 6u;
    if (header_size > file_size) {
        wolf_set_error(error_message, error_message_size, "VSWAP page tables are truncated.");
        return false;
    }
    length_table = VSWAP_HEADER_PREFIX_SIZE + (size_t)chunk_count * 4u;

    sprites = calloc((size_t)(first_sound_page - first_sprite_page), sizeof(*sprites));
    if (sprites == NULL) {
        wolf_set_error(error_message, error_message_size, "Could not allocate decoded sprites.");
        return false;
    }

    for (uint16_t page_index = first_sprite_page; page_index < first_sound_page; ++page_index) {
        uint32_t page_offset = wolf_read_u32_le(file_data + VSWAP_HEADER_PREFIX_SIZE + (size_t)page_index * 4u);
        uint16_t page_length = wolf_read_u16_le(file_data + length_table + (size_t)page_index * 2u);
        WolfSpriteImage *sprite = &sprites[page_index - first_sprite_page];
        char page_error[128];

        /* A zero offset marks a sparse page that the original loader never reads. */
        if (page_offset == 0u) {
            wolf_sprite_image_clear(sprite);
            continue;
        }
        if (page_offset < header_size || page_offset > file_size ||
            page_length > file_size - page_offset) {
            free(sprites);
            wolf_set_error(error_message, error_message_size, "A VSWAP sprite page lies outside the file.");
            return false;
        }
        if (!wolf_sprite_page_decode(file_data + page_offset, page_length, sprite,
                page_error, sizeof(page_error))) {
            if (error_message != NULL && error_message_size > 0u) {
                (void)snprintf(error_message, error_message_size, "Sprite %u: %s",
                    (unsigned)(page_index - first_sprite_page), page_error);
            }
            free(sprites);
            return false;
        }
    }

    archive->sprite_count = (uint16_t)(first_sound_page - first_sprite_page);
    archive->first_sprite_page = first_sprite_page;
    archive->sprites = sprites;
    return true;
}

bool wolf_sprite_archive_load(
    const char *path,
    WolfSpriteArchive *archive,
    char *error_message,
    size_t error_message_size)
{
    uint8_t *file_data;
    size_t file_size;
    bool decoded;

    memset(archive, 0, sizeof(*archive));
    if (!wolf_read_entire_file(path, &file_data, &file_size)) {
        wolf_set_error(error_message, error_message_size, "Could not read the VSWAP file.");
        return false;
    }
    decoded = wolf_sprite_archive_decode(file_data, file_size, archive, error_message, error_message_size);
    free(file_data);
    return decoded;
}

void wolf_sprite_archive_destroy(WolfSpriteArchive *archive)
{
    free(archive->sprites);
    memset(archive, 0, sizeof(*archive));
}
