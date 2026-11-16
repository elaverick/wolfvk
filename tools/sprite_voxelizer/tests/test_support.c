#include "test_support.h"

#include "vswap_sprites.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void test_fill_rectangle(WolfSpriteImage *image, int left, int top, int right, int bottom, uint8_t palette_index)
{
    for (int row = top; row <= bottom; ++row) {
        for (int column = left; column <= right; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);

            image->opaque[pixel] = 1u;
            image->palette_index[pixel] = palette_index;
        }
    }
}

void test_make_patterned_sprite(WolfSpriteImage *image, unsigned seed)
{
    wolf_sprite_image_clear(image);
    for (int column = 5; column < 59; ++column) {
        for (int row = 2; row < 64; ++row) {
            /* Leave periodic holes so columns contain several separate posts. */
            unsigned value = (unsigned)(column * 31 + row * 17) + seed * 101u;

            if ((value % 7u) == 0u || (row % 11) == 0) {
                continue;
            }
            image->opaque[wolf_sprite_pixel_index(column, row)] = 1u;
            image->palette_index[wolf_sprite_pixel_index(column, row)] = (uint8_t)(value % 256u);
        }
    }
}

static uint8_t banded_colour(int z)
{
    return (uint8_t)(16 + z / 4);
}

void test_make_box_model(WolfVoxelModel *model, int half_width, int half_depth, int height)
{
    wolf_voxel_model_clear(model);
    for (int z = 0; z < height; ++z) {
        for (int y = 32 - half_depth; y < 32 + half_depth; ++y) {
            for (int x = 32 - half_width; x < 32 + half_width; ++x) {
                model->occupied[wolf_voxel_index(x, y, z)] = 1u;
                model->palette_index[wolf_voxel_index(x, y, z)] = banded_colour(z);
            }
        }
    }
}

void test_make_cylinder_model(WolfVoxelModel *model, double radius, int height)
{
    wolf_voxel_model_clear(model);
    for (int z = 0; z < height; ++z) {
        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                double offset_x = wolf_voxel_center_offset(x);
                double offset_y = wolf_voxel_center_offset(y);

                if (offset_x * offset_x + offset_y * offset_y <= radius * radius) {
                    model->occupied[wolf_voxel_index(x, y, z)] = 1u;
                    model->palette_index[wolf_voxel_index(x, y, z)] = banded_colour(z);
                }
            }
        }
    }
}

bool test_build_vswap(
    const WolfSpriteImage *const *sprites,
    size_t sprite_count,
    uint16_t wall_page_count,
    WolfByteBuffer *output)
{
    uint16_t chunk_count = (uint16_t)(wall_page_count + sprite_count + 1u);
    size_t header_size = 6u + (size_t)chunk_count * 6u;
    uint8_t *header = calloc(header_size, 1u);
    uint8_t page[16384];
    size_t offset_cursor;

    if (header == NULL) {
        return false;
    }
    wolf_write_u16_le(header, chunk_count);
    wolf_write_u16_le(header + 2u, wall_page_count);
    wolf_write_u16_le(header + 4u, (uint16_t)(wall_page_count + sprite_count));
    wolf_buffer_append(output, header, header_size);
    offset_cursor = header_size;

    /* Blank 64x64 wall pages, as the sprite decoder must skip them. */
    for (uint16_t wall = 0u; wall < wall_page_count; ++wall) {
        memset(page, 0, 4096u);
        wolf_write_u32_le(output->data + 6u + (size_t)wall * 4u, (uint32_t)offset_cursor);
        wolf_write_u16_le(output->data + 6u + (size_t)chunk_count * 4u + (size_t)wall * 2u, 4096u);
        wolf_buffer_append(output, page, 4096u);
        offset_cursor += 4096u;
    }
    for (size_t sprite = 0u; sprite < sprite_count; ++sprite) {
        size_t chunk = wall_page_count + sprite;
        size_t page_size;

        if (sprites[sprite] == NULL) {
            continue;
        }
        if (!wolf_sprite_page_encode(sprites[sprite], page, sizeof(page), &page_size)) {
            free(header);
            return false;
        }
        wolf_write_u32_le(output->data + 6u + chunk * 4u, (uint32_t)offset_cursor);
        wolf_write_u16_le(output->data + 6u + (size_t)chunk_count * 4u + chunk * 2u, (uint16_t)page_size);
        wolf_buffer_append(output, page, page_size);
        offset_cursor += page_size;
    }
    /* One trailing sound page so the sound boundary is a real chunk. */
    {
        size_t chunk = (size_t)chunk_count - 1u;
        uint8_t sound[8] = {0};

        wolf_write_u32_le(output->data + 6u + chunk * 4u, (uint32_t)offset_cursor);
        wolf_write_u16_le(output->data + 6u + (size_t)chunk_count * 4u + chunk * 2u, (uint16_t)sizeof(sound));
        wolf_buffer_append(output, sound, sizeof(sound));
    }
    free(header);
    return !output->failed;
}

typedef struct BitReader {
    const uint8_t *data;
    size_t size;
    size_t bit_position;
} BitReader;

static int read_bit(BitReader *reader)
{
    size_t byte = reader->bit_position / 8u;
    int bit;

    if (byte >= reader->size) {
        return -1;
    }
    bit = (reader->data[byte] >> (reader->bit_position % 8u)) & 1;
    ++reader->bit_position;
    return bit;
}

static int read_bits(BitReader *reader, int count)
{
    int value = 0;

    for (int bit = 0; bit < count; ++bit) {
        int next = read_bit(reader);

        if (next < 0) {
            return -1;
        }
        value |= next << bit;
    }
    return value;
}

/* Read a Huffman code most-significant bit first, extending it one bit at a time. */
static int decode_fixed_literal(BitReader *reader)
{
    int code = 0;

    for (int length = 1; length <= 9; ++length) {
        int bit = read_bit(reader);

        if (bit < 0) {
            return -1;
        }
        code = (code << 1) | bit;
        if (length == 7 && code <= 0x17) {
            return 256 + code;
        }
        if (length == 8 && code >= 0x30 && code <= 0xbf) {
            return code - 0x30;
        }
        if (length == 8 && code >= 0xc0 && code <= 0xc7) {
            return 280 + (code - 0xc0);
        }
        if (length == 9 && code >= 0x190) {
            return 144 + (code - 0x190);
        }
    }
    return -1;
}

bool test_zlib_decompress(const uint8_t *data, size_t size, uint8_t *output, size_t capacity, size_t *output_size)
{
    static const int length_bases[29] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
    };
    static const int length_extra[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
    };
    static const int distance_bases[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
    };
    static const int distance_extra[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
    };
    BitReader reader;
    size_t written = 0u;
    int final_block = 0;

    if (size < 6u || (data[0] & 0x0fu) != 8u || ((data[0] << 8) | data[1]) % 31 != 0) {
        return false;
    }
    reader.data = data + 2u;
    reader.size = size - 6u;
    reader.bit_position = 0u;

    while (!final_block) {
        int block_type;

        final_block = read_bits(&reader, 1);
        block_type = read_bits(&reader, 2);
        if (final_block < 0 || block_type < 0) {
            return false;
        }
        if (block_type == 0) {
            size_t byte = (reader.bit_position + 7u) / 8u;
            size_t length;

            if (byte + 4u > reader.size) {
                return false;
            }
            length = (size_t)reader.data[byte] | ((size_t)reader.data[byte + 1u] << 8);
            if (byte + 4u + length > reader.size || written + length > capacity) {
                return false;
            }
            memcpy(output + written, reader.data + byte + 4u, length);
            written += length;
            reader.bit_position = (byte + 4u + length) * 8u;
            continue;
        }
        if (block_type != 1) {
            return false;
        }
        for (;;) {
            int symbol = decode_fixed_literal(&reader);

            if (symbol < 0) {
                return false;
            }
            if (symbol < 256) {
                if (written >= capacity) {
                    return false;
                }
                output[written++] = (uint8_t)symbol;
            } else if (symbol == 256) {
                break;
            } else {
                int length_code = symbol - 257;
                int length;
                int distance_code = 0;
                int distance;

                if (length_code >= 29) {
                    return false;
                }
                length = length_bases[length_code] + read_bits(&reader, length_extra[length_code]);
                for (int bit = 0; bit < 5; ++bit) {
                    distance_code = (distance_code << 1) | read_bit(&reader);
                }
                if (distance_code >= 30) {
                    return false;
                }
                distance = distance_bases[distance_code] + read_bits(&reader, distance_extra[distance_code]);
                if ((size_t)distance > written || written + (size_t)length > capacity) {
                    return false;
                }
                for (int copy = 0; copy < length; ++copy) {
                    output[written] = output[written - (size_t)distance];
                    ++written;
                }
            }
        }
    }
    *output_size = written;
    return true;
}
