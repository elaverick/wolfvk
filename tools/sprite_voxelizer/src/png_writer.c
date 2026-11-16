#include "png_writer.h"

#include <stdlib.h>
#include <string.h>

#define DEFLATE_WINDOW_SIZE 32768u
#define DEFLATE_MIN_MATCH 3u
#define DEFLATE_MAX_MATCH 258u
#define DEFLATE_HASH_SIZE 32768u
#define DEFLATE_MAX_CHAIN 48u

uint32_t wolf_png_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xffffffffu;

    for (size_t position = 0u; position < size; ++position) {
        crc ^= data[position];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return crc ^ 0xffffffffu;
}

uint32_t wolf_png_adler32(const uint8_t *data, size_t size)
{
    uint32_t low = 1u;
    uint32_t high = 0u;

    for (size_t position = 0u; position < size; ++position) {
        low = (low + data[position]) % 65521u;
        high = (high + low) % 65521u;
    }
    return (high << 16) | low;
}

/* Deflate emits bits least-significant first into consecutive bytes. */
typedef struct BitWriter {
    WolfByteBuffer *output;
    uint32_t pending_bits;
    int pending_count;
} BitWriter;

static void write_bits(BitWriter *writer, uint32_t value, int count)
{
    writer->pending_bits |= value << writer->pending_count;
    writer->pending_count += count;
    while (writer->pending_count >= 8) {
        wolf_buffer_append_u8(writer->output, (uint8_t)(writer->pending_bits & 0xffu));
        writer->pending_bits >>= 8;
        writer->pending_count -= 8;
    }
}

/* Huffman codes are defined most-significant bit first, so reverse them before writing. */
static void write_huffman_code(BitWriter *writer, uint32_t code, int length)
{
    uint32_t reversed = 0u;

    for (int bit = 0; bit < length; ++bit) {
        reversed = (reversed << 1) | ((code >> bit) & 1u);
    }
    write_bits(writer, reversed, length);
}

/* The fixed literal/length code from RFC 1951 section 3.2.6. */
static void write_literal_or_length_symbol(BitWriter *writer, uint32_t symbol)
{
    if (symbol <= 143u) {
        write_huffman_code(writer, 0x30u + symbol, 8);
    } else if (symbol <= 255u) {
        write_huffman_code(writer, 0x190u + (symbol - 144u), 9);
    } else if (symbol <= 279u) {
        write_huffman_code(writer, symbol - 256u, 7);
    } else {
        write_huffman_code(writer, 0xc0u + (symbol - 280u), 8);
    }
}

static const uint16_t length_bases[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const uint8_t length_extra_bits[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const uint16_t distance_bases[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const uint8_t distance_extra_bits[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static void write_match(BitWriter *writer, uint32_t length, uint32_t distance)
{
    int length_code = 28;
    int distance_code = 29;

    while (length_bases[length_code] > length) {
        --length_code;
    }
    write_literal_or_length_symbol(writer, 257u + (uint32_t)length_code);
    write_bits(writer, length - length_bases[length_code], length_extra_bits[length_code]);

    while (distance_bases[distance_code] > distance) {
        --distance_code;
    }
    write_huffman_code(writer, (uint32_t)distance_code, 5);
    write_bits(writer, distance - distance_bases[distance_code], distance_extra_bits[distance_code]);
}

static uint32_t hash_three_bytes(const uint8_t *data)
{
    return (((uint32_t)data[0] << 10) ^ ((uint32_t)data[1] << 5) ^ data[2]) & (DEFLATE_HASH_SIZE - 1u);
}

bool wolf_zlib_compress(const uint8_t *data, size_t size, WolfByteBuffer *output)
{
    BitWriter writer = {output, 0u, 0};
    int32_t *hash_heads = malloc(sizeof(int32_t) * DEFLATE_HASH_SIZE);
    int32_t *previous = malloc(sizeof(int32_t) * DEFLATE_WINDOW_SIZE);
    size_t position = 0u;

    if (hash_heads == NULL || previous == NULL) {
        free(hash_heads);
        free(previous);
        return false;
    }
    for (size_t slot = 0u; slot < DEFLATE_HASH_SIZE; ++slot) {
        hash_heads[slot] = -1;
    }

    /* zlib header: deflate with a 32 KiB window and no preset dictionary. */
    wolf_buffer_append_u8(output, 0x78u);
    wolf_buffer_append_u8(output, 0x01u);

    /* One final block using the fixed Huffman code (BFINAL=1, BTYPE=01). */
    write_bits(&writer, 1u, 1);
    write_bits(&writer, 1u, 2);

    while (position < size) {
        uint32_t best_length = 0u;
        uint32_t best_distance = 0u;

        /*
         * Greedy LZ77: follow the hash chain of earlier positions with the
         * same three-byte prefix and keep the longest match in the window.
         */
        if (position + DEFLATE_MIN_MATCH <= size) {
            uint32_t hash = hash_three_bytes(data + position);
            int32_t candidate = hash_heads[hash];
            uint32_t chain = 0u;
            size_t limit = size - position < DEFLATE_MAX_MATCH ? size - position : DEFLATE_MAX_MATCH;

            while (candidate >= 0 && position - (size_t)candidate <= DEFLATE_WINDOW_SIZE &&
                chain < DEFLATE_MAX_CHAIN) {
                uint32_t length = 0u;

                while (length < limit && data[(size_t)candidate + length] == data[position + length]) {
                    ++length;
                }
                if (length > best_length) {
                    best_length = length;
                    best_distance = (uint32_t)(position - (size_t)candidate);
                    if (length == limit) {
                        break;
                    }
                }
                candidate = previous[(size_t)candidate % DEFLATE_WINDOW_SIZE];
                ++chain;
            }
        }

        if (best_length < DEFLATE_MIN_MATCH) {
            best_length = 1u;
            write_literal_or_length_symbol(&writer, data[position]);
        } else {
            write_match(&writer, best_length, best_distance);
        }

        /* Index every consumed position so later matches can refer back to it. */
        for (uint32_t step = 0u; step < best_length; ++step) {
            size_t indexed = position + step;

            if (indexed + DEFLATE_MIN_MATCH <= size) {
                uint32_t hash = hash_three_bytes(data + indexed);

                previous[indexed % DEFLATE_WINDOW_SIZE] = hash_heads[hash];
                hash_heads[hash] = (int32_t)indexed;
            }
        }
        position += best_length;
    }

    write_literal_or_length_symbol(&writer, 256u);
    if (writer.pending_count > 0) {
        write_bits(&writer, 0u, 8 - writer.pending_count);
    }
    wolf_buffer_append_u32_be(output, wolf_png_adler32(data, size));
    free(hash_heads);
    free(previous);
    return !output->failed;
}

/* PNG chunks carry a big-endian length and a CRC over the type and data. */
static void append_png_chunk(WolfByteBuffer *output, const char *type, const uint8_t *data, size_t size)
{
    uint8_t *crc_input = malloc(size + 4u);

    if (crc_input == NULL) {
        output->failed = true;
        return;
    }
    memcpy(crc_input, type, 4u);
    if (size > 0u) {
        memcpy(crc_input + 4u, data, size);
    }
    wolf_buffer_append_u32_be(output, (uint32_t)size);
    wolf_buffer_append(output, crc_input, size + 4u);
    wolf_buffer_append_u32_be(output, wolf_png_crc32(crc_input, size + 4u));
    free(crc_input);
}

bool wolf_png_encode_rgba(const uint8_t *rgba, uint32_t width, uint32_t height, WolfByteBuffer *output)
{
    static const uint8_t signature[8] = {137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u};
    uint8_t header[13];
    size_t row_bytes = (size_t)width * 4u;
    size_t raw_size = (row_bytes + 1u) * height;
    uint8_t *raw = malloc(raw_size);
    WolfByteBuffer compressed = {0};
    bool encoded;

    if (raw == NULL || width == 0u || height == 0u) {
        free(raw);
        return false;
    }

    /* Filter type 0 on every row: the LZ77 stage already captures flat backgrounds well. */
    for (uint32_t row = 0u; row < height; ++row) {
        raw[row * (row_bytes + 1u)] = 0u;
        memcpy(raw + row * (row_bytes + 1u) + 1u, rgba + row * row_bytes, row_bytes);
    }

    header[0] = (uint8_t)(width >> 24);
    header[1] = (uint8_t)(width >> 16);
    header[2] = (uint8_t)(width >> 8);
    header[3] = (uint8_t)width;
    header[4] = (uint8_t)(height >> 24);
    header[5] = (uint8_t)(height >> 16);
    header[6] = (uint8_t)(height >> 8);
    header[7] = (uint8_t)height;
    header[8] = 8u;  /* bit depth */
    header[9] = 6u;  /* truecolour with alpha */
    header[10] = 0u; /* deflate */
    header[11] = 0u; /* adaptive filtering */
    header[12] = 0u; /* no interlace */

    encoded = wolf_zlib_compress(raw, raw_size, &compressed);
    free(raw);
    if (encoded) {
        wolf_buffer_append(output, signature, sizeof(signature));
        append_png_chunk(output, "IHDR", header, sizeof(header));
        append_png_chunk(output, "IDAT", compressed.data, compressed.size);
        append_png_chunk(output, "IEND", NULL, 0u);
    }
    wolf_buffer_free(&compressed);
    return encoded && !output->failed;
}

bool wolf_png_write_rgba(const char *path, const uint8_t *rgba, uint32_t width, uint32_t height)
{
    WolfByteBuffer buffer = {0};
    bool written = false;

    if (wolf_png_encode_rgba(rgba, width, height, &buffer)) {
        written = wolf_write_entire_file(path, buffer.data, buffer.size);
    }
    wolf_buffer_free(&buffer);
    return written;
}
