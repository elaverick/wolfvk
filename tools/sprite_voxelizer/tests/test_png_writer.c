#include "unity.h"

#include "png_writer.h"
#include "test_support.h"

#include <stdlib.h>
#include <string.h>

/* Published check values for the two checksums PNG and zlib require. */
void test_png_checksums_match_reference_values(void)
{
    TEST_ASSERT_EQUAL_HEX32(0xcbf43926u, wolf_png_crc32((const uint8_t *)"123456789", 9u));
    TEST_ASSERT_EQUAL_HEX32(0x11e60398u, wolf_png_adler32((const uint8_t *)"Wikipedia", 9u));
    TEST_ASSERT_EQUAL_HEX32(1u, wolf_png_adler32(NULL, 0u));
}

/* The deflate stream must decode back to the input for repetitive and noisy data. */
void test_zlib_stream_round_trips(void)
{
    const size_t size = 70000u;
    uint8_t *input = malloc(size);
    uint8_t *output = malloc(size);
    WolfByteBuffer compressed = {0};
    size_t decoded_size = 0u;
    uint32_t state = 12345u;

    for (size_t position = 0u; position < size; ++position) {
        /* Long runs, short repeats beyond the 32 KiB window and noise. */
        if (position < 20000u) {
            input[position] = (uint8_t)(position / 500u);
        } else if (position < 50000u) {
            input[position] = (uint8_t)("voxel"[position % 5u]);
        } else {
            state = state * 1103515245u + 12345u;
            input[position] = (uint8_t)(state >> 16);
        }
    }
    TEST_ASSERT_TRUE(wolf_zlib_compress(input, size, &compressed));
    TEST_ASSERT_TRUE_MESSAGE(compressed.size < size, "repetitive data should compress");
    TEST_ASSERT_TRUE(test_zlib_decompress(compressed.data, compressed.size, output, size, &decoded_size));
    TEST_ASSERT_EQUAL_size_t(size, decoded_size);
    TEST_ASSERT_EQUAL_MEMORY(input, output, size);
    TEST_ASSERT_EQUAL_HEX32(wolf_png_adler32(input, size),
        ((uint32_t)compressed.data[compressed.size - 4u] << 24) |
        ((uint32_t)compressed.data[compressed.size - 3u] << 16) |
        ((uint32_t)compressed.data[compressed.size - 2u] << 8) |
        (uint32_t)compressed.data[compressed.size - 1u]);
    wolf_buffer_free(&compressed);
    free(input);
    free(output);
}

/* PNG container: signature, IHDR fields, chunk CRCs and filtered RGBA rows. */
void test_png_encoder_writes_valid_rgba_image(void)
{
    uint8_t rgba[3u * 2u * 4u];
    WolfByteBuffer png = {0};
    size_t cursor = 8u;
    uint8_t raw[2u * (1u + 3u * 4u)];
    size_t raw_size = 0u;
    bool saw_end = false;

    for (size_t byte = 0u; byte < sizeof(rgba); ++byte) {
        rgba[byte] = (uint8_t)(byte * 9u);
    }
    TEST_ASSERT_TRUE(wolf_png_encode_rgba(rgba, 3u, 2u, &png));
    TEST_ASSERT_EQUAL_MEMORY("\x89PNG\r\n\x1a\n", png.data, 8u);

    while (cursor + 12u <= png.size) {
        uint32_t length = ((uint32_t)png.data[cursor] << 24) | ((uint32_t)png.data[cursor + 1u] << 16) |
            ((uint32_t)png.data[cursor + 2u] << 8) | png.data[cursor + 3u];
        const uint8_t *type = png.data + cursor + 4u;
        uint32_t stored_crc;

        TEST_ASSERT_TRUE(cursor + 12u + length <= png.size);
        stored_crc = ((uint32_t)type[4u + length] << 24) | ((uint32_t)type[5u + length] << 16) |
            ((uint32_t)type[6u + length] << 8) | type[7u + length];
        TEST_ASSERT_EQUAL_HEX32(wolf_png_crc32(type, 4u + length), stored_crc);

        if (memcmp(type, "IHDR", 4u) == 0) {
            TEST_ASSERT_EQUAL_UINT32(13u, length);
            TEST_ASSERT_EQUAL_UINT8(3u, type[7]);  /* width low byte */
            TEST_ASSERT_EQUAL_UINT8(2u, type[11]); /* height low byte */
            TEST_ASSERT_EQUAL_UINT8(8u, type[12]); /* bit depth */
            TEST_ASSERT_EQUAL_UINT8(6u, type[13]); /* RGBA */
        } else if (memcmp(type, "IDAT", 4u) == 0) {
            TEST_ASSERT_TRUE(test_zlib_decompress(type + 4u, length, raw, sizeof(raw), &raw_size));
        } else if (memcmp(type, "IEND", 4u) == 0) {
            saw_end = true;
        }
        cursor += 12u + length;
    }
    TEST_ASSERT_TRUE(saw_end);
    TEST_ASSERT_EQUAL_size_t(sizeof(raw), raw_size);
    TEST_ASSERT_EQUAL_UINT8(0u, raw[0]);
    TEST_ASSERT_EQUAL_MEMORY(rgba, raw + 1u, 12u);
    TEST_ASSERT_EQUAL_MEMORY(rgba + 12u, raw + 14u, 12u);
    TEST_ASSERT_FALSE(wolf_png_encode_rgba(rgba, 0u, 2u, &png));
    wolf_buffer_free(&png);
}
