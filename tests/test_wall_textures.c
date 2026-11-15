#include "unity.h"

#include "game_palette.h"
#include "wolfvk/wall_textures.h"

#include <stdlib.h>
#include <string.h>

#define TEST_VSWAP_PAGE_COUNT 4u
#define TEST_VSWAP_HEADER_SIZE \
    (6u + TEST_VSWAP_PAGE_COUNT * 4u + TEST_VSWAP_PAGE_COUNT * 2u)
#define TEST_VSWAP_PAGE_BYTES (64u * 64u)

static void write_u16_le(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)(value >> 8);
}

static void write_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)((value >> 8) & 0xffu);
    destination[2] = (uint8_t)((value >> 16) & 0xffu);
    destination[3] = (uint8_t)(value >> 24);
}

/*
 * Build four contiguous, uncompressed pages with deterministic indexed texels.
 * This mirrors the VSWAP header used by the original page manager without
 * relying on a copyrighted game archive in the automated test suite.
 */
static uint8_t *create_vswap_fixture(size_t *file_size)
{
    const size_t data_size = (size_t)TEST_VSWAP_PAGE_COUNT * TEST_VSWAP_PAGE_BYTES;
    uint8_t *data = calloc(1u, TEST_VSWAP_HEADER_SIZE + data_size);

    if (data == NULL) {
        return NULL;
    }

    write_u16_le(data, TEST_VSWAP_PAGE_COUNT);
    write_u16_le(data + 2u, TEST_VSWAP_PAGE_COUNT);
    write_u16_le(data + 4u, TEST_VSWAP_PAGE_COUNT);

    for (uint16_t page = 0u; page < TEST_VSWAP_PAGE_COUNT; ++page) {
        uint32_t page_offset =
            (uint32_t)(TEST_VSWAP_HEADER_SIZE + (size_t)page * TEST_VSWAP_PAGE_BYTES);
        size_t offset_entry = 6u + (size_t)page * 4u;
        size_t length_table = 6u + TEST_VSWAP_PAGE_COUNT * 4u;
        size_t page_start = page_offset;

        write_u32_le(data + offset_entry, page_offset);
        write_u16_le(data + length_table + (size_t)page * 2u, (uint16_t)TEST_VSWAP_PAGE_BYTES);

        /* Original wall-page texels are stored as 64 vertical columns. */
        for (uint32_t column = 0u; column < 64u; ++column) {
            for (uint32_t row = 0u; row < 64u; ++row) {
                data[page_start + (size_t)column * 64u + row] =
                    (uint8_t)((column * 3u + row * 5u + page) & 0xffu);
            }
        }
    }

    *file_size = TEST_VSWAP_HEADER_SIZE + data_size;
    return data;
}

void test_vswap_decoder_expands_column_major_palette_indices_to_rgba(void)
{
    uint8_t *fixture;
    size_t fixture_size = 0u;
    WolfWallTextures textures = {0};
    char error_message[128];
    const uint16_t page = 2u;
    const uint32_t column = 17u;
    const uint32_t row = 29u;
    const uint8_t palette_index =
        (uint8_t)((column * 3u + row * 5u + page) & 0xffu);
    const size_t rgba_offset =
        ((size_t)page * 64u * 64u + (size_t)row * 64u + column) * 4u;

    fixture = create_vswap_fixture(&fixture_size);
    TEST_ASSERT_NOT_NULL(fixture);

    TEST_ASSERT_TRUE_MESSAGE(
        wolf_wall_textures_decode_vswap(
            fixture, fixture_size, &textures, error_message, sizeof(error_message)),
        error_message);
    TEST_ASSERT_EQUAL_UINT16(TEST_VSWAP_PAGE_COUNT, textures.page_count);
    TEST_ASSERT_NOT_NULL(textures.rgba_pages);

    /* Transposition must preserve each indexed source texel and use the original palette. */
    TEST_ASSERT_EQUAL_UINT8(
        wolf_game_palette_rgba[(size_t)palette_index * 4u],
        textures.rgba_pages[rgba_offset]);
    TEST_ASSERT_EQUAL_UINT8(
        wolf_game_palette_rgba[(size_t)palette_index * 4u + 1u],
        textures.rgba_pages[rgba_offset + 1u]);
    TEST_ASSERT_EQUAL_UINT8(
        wolf_game_palette_rgba[(size_t)palette_index * 4u + 2u],
        textures.rgba_pages[rgba_offset + 2u]);
    TEST_ASSERT_EQUAL_UINT8(255u, textures.rgba_pages[rgba_offset + 3u]);

    wolf_wall_textures_destroy(&textures);
    wolf_wall_textures_destroy(&textures);
    free(fixture);
}

void test_vswap_decoder_rejects_truncated_wall_pages(void)
{
    uint8_t *fixture;
    size_t fixture_size = 0u;
    WolfWallTextures textures = {0};
    char error_message[128];

    fixture = create_vswap_fixture(&fixture_size);
    TEST_ASSERT_NOT_NULL(fixture);

    TEST_ASSERT_FALSE(wolf_wall_textures_decode_vswap(
        fixture, fixture_size - 1u, &textures, error_message, sizeof(error_message)));
    TEST_ASSERT_NULL(textures.rgba_pages);
    TEST_ASSERT_EQUAL_UINT16(0u, textures.page_count);

    free(fixture);
}

void test_vswap_decoder_rejects_short_wall_page_lengths(void)
{
    uint8_t *fixture;
    size_t fixture_size = 0u;
    WolfWallTextures textures = {0};
    char error_message[128];
    size_t length_table = 6u + TEST_VSWAP_PAGE_COUNT * 4u;

    fixture = create_vswap_fixture(&fixture_size);
    TEST_ASSERT_NOT_NULL(fixture);
    write_u16_le(fixture + length_table + 2u, 4095u);

    TEST_ASSERT_FALSE(wolf_wall_textures_decode_vswap(
        fixture, fixture_size, &textures, error_message, sizeof(error_message)));
    TEST_ASSERT_NULL(textures.rgba_pages);

    free(fixture);
}

void test_vswap_decoder_rejects_invalid_page_boundaries(void)
{
    uint8_t *fixture;
    size_t fixture_size = 0u;
    WolfWallTextures textures = {0};
    char error_message[128];

    fixture = create_vswap_fixture(&fixture_size);
    TEST_ASSERT_NOT_NULL(fixture);
    write_u16_le(fixture + 2u, 3u);

    TEST_ASSERT_FALSE(wolf_wall_textures_decode_vswap(
        fixture, fixture_size, &textures, error_message, sizeof(error_message)));
    TEST_ASSERT_NULL(textures.rgba_pages);

    free(fixture);
}

void test_vswap_decoder_rejects_invalid_arguments_and_nonempty_output(void)
{
    uint8_t *fixture;
    size_t fixture_size = 0u;
    WolfWallTextures textures = {0};
    char error_message[128];

    fixture = create_vswap_fixture(&fixture_size);
    TEST_ASSERT_NOT_NULL(fixture);

    TEST_ASSERT_FALSE(wolf_wall_textures_decode_vswap(
        NULL, fixture_size, &textures, error_message, sizeof(error_message)));
    TEST_ASSERT_FALSE(wolf_wall_textures_decode_vswap(
        fixture, fixture_size, NULL, error_message, sizeof(error_message)));

    textures.page_count = 1u;
    TEST_ASSERT_FALSE(wolf_wall_textures_decode_vswap(
        fixture, fixture_size, &textures, error_message, sizeof(error_message)));
    TEST_ASSERT_NULL(textures.rgba_pages);
    textures.page_count = 0u;

    free(fixture);
}

void test_wall_tile_selects_original_horizontal_and_vertical_pages(void)
{
    uint16_t page = UINT16_MAX;

    TEST_ASSERT_TRUE(wolf_wall_texture_page_for_tile(1u, false, 98u, &page));
    TEST_ASSERT_EQUAL_UINT16(0u, page);
    TEST_ASSERT_TRUE(wolf_wall_texture_page_for_tile(1u, true, 98u, &page));
    TEST_ASSERT_EQUAL_UINT16(1u, page);

    TEST_ASSERT_TRUE(wolf_wall_texture_page_for_tile(7u, false, 98u, &page));
    TEST_ASSERT_EQUAL_UINT16(12u, page);
    TEST_ASSERT_TRUE(wolf_wall_texture_page_for_tile(7u, true, 98u, &page));
    TEST_ASSERT_EQUAL_UINT16(13u, page);

    TEST_ASSERT_TRUE(wolf_wall_texture_page_for_tile(49u, false, 98u, &page));
    TEST_ASSERT_EQUAL_UINT16(96u, page);
    TEST_ASSERT_TRUE(wolf_wall_texture_page_for_tile(49u, true, 98u, &page));
    TEST_ASSERT_EQUAL_UINT16(97u, page);

    /* The 0x40 flag is stripped before resolving the original wall material. */
    TEST_ASSERT_TRUE(wolf_wall_texture_page_for_tile(0x47u, true, 98u, &page));
    TEST_ASSERT_EQUAL_UINT16(13u, page);

    /*
     * Retail WL6 maps use wall value 106 for flagged wall type 42.
     * It must resolve to pages 82/83 rather than being mistaken for a door.
     */
    TEST_ASSERT_TRUE(wolf_wall_texture_page_for_tile(106u, false, 98u, &page));
    TEST_ASSERT_EQUAL_UINT16(82u, page);
    TEST_ASSERT_TRUE(wolf_wall_texture_page_for_tile(106u, true, 98u, &page));
    TEST_ASSERT_EQUAL_UINT16(83u, page);
}

void test_wall_tile_page_selection_rejects_invalid_or_missing_pages(void)
{
    uint16_t page = UINT16_MAX;

    TEST_ASSERT_FALSE(wolf_wall_texture_page_for_tile(0u, false, 98u, &page));
    for (uint16_t door_tile = 90u; door_tile <= 101u; ++door_tile) {
        TEST_ASSERT_FALSE(wolf_wall_texture_page_for_tile(door_tile, false, 98u, &page));
    }
    TEST_ASSERT_FALSE(wolf_wall_texture_page_for_tile(107u, false, 98u, &page));
    TEST_ASSERT_FALSE(wolf_wall_texture_page_for_tile(49u, true, 97u, &page));
    TEST_ASSERT_FALSE(wolf_wall_texture_page_for_tile(7u, false, 98u, NULL));
}
