#include "unity.h"

#include "test_support.h"
#include "vswap_sprites.h"

#include <string.h>

static void assert_images_equal(const WolfSpriteImage *expected, const WolfSpriteImage *actual)
{
    for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected->opaque[pixel], actual->opaque[pixel], "opacity differs");
        if (expected->opaque[pixel]) {
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected->palette_index[pixel], actual->palette_index[pixel], "colour differs");
        }
    }
}

/* Encoding then decoding must reproduce gaps, multiple posts and every colour. */
void test_sprite_page_round_trips_original_post_layout(void)
{
    WolfSpriteImage original;
    WolfSpriteImage decoded;
    uint8_t page[16384];
    size_t page_size;

    test_make_patterned_sprite(&original, 3u);
    TEST_ASSERT_TRUE(wolf_sprite_page_encode(&original, page, sizeof(page), &page_size));
    TEST_ASSERT_TRUE(wolf_sprite_page_decode(page, page_size, &decoded, NULL, 0u));
    assert_images_equal(&original, &decoded);
}

/* A hand-assembled page in the exact t_compshape layout, including a biased source offset. */
void test_sprite_page_decodes_hand_built_compshape(void)
{
    uint8_t page[64] = {0};
    WolfSpriteImage decoded;

    /* One drawn column (column 31) with one post covering rows 10..12. */
    wolf_write_u16_le(page + 0u, 31u);
    wolf_write_u16_le(page + 2u, 31u);
    wolf_write_u16_le(page + 4u, 6u);           /* column 31 posts start at byte 6 */
    wolf_write_u16_le(page + 6u, 13u * 2u);     /* end row * 2 */
    wolf_write_u16_le(page + 8u, (uint16_t)(20u - 10u)); /* pixels at 20, biased by start row */
    wolf_write_u16_le(page + 10u, 10u * 2u);    /* start row * 2 */
    wolf_write_u16_le(page + 12u, 0u);          /* column terminator */
    page[20] = 7u;
    page[21] = 8u;
    page[22] = 9u;

    TEST_ASSERT_TRUE(wolf_sprite_page_decode(page, 23u, &decoded, NULL, 0u));
    TEST_ASSERT_EQUAL_size_t(3u, wolf_sprite_image_opaque_count(&decoded));
    TEST_ASSERT_EQUAL_UINT8(7u, decoded.palette_index[wolf_sprite_pixel_index(31, 10)]);
    TEST_ASSERT_EQUAL_UINT8(9u, decoded.palette_index[wolf_sprite_pixel_index(31, 12)]);
    TEST_ASSERT_EQUAL_UINT8(0u, decoded.opaque[wolf_sprite_pixel_index(31, 13)]);
}

void test_sprite_page_rejects_malformed_pages(void)
{
    uint8_t page[64] = {0};
    WolfSpriteImage decoded;
    char error[128];

    /* Left column after right column. */
    wolf_write_u16_le(page, 10u);
    wolf_write_u16_le(page + 2u, 5u);
    TEST_ASSERT_FALSE(wolf_sprite_page_decode(page, sizeof(page), &decoded, error, sizeof(error)));

    /* Column beyond the 64-pixel canvas. */
    wolf_write_u16_le(page, 0u);
    wolf_write_u16_le(page + 2u, 64u);
    TEST_ASSERT_FALSE(wolf_sprite_page_decode(page, sizeof(page), &decoded, error, sizeof(error)));

    /* Post rows past the bottom of the canvas. */
    wolf_write_u16_le(page, 0u);
    wolf_write_u16_le(page + 2u, 0u);
    wolf_write_u16_le(page + 4u, 6u);
    wolf_write_u16_le(page + 6u, 70u * 2u);
    wolf_write_u16_le(page + 8u, 20u);
    wolf_write_u16_le(page + 10u, 60u * 2u);
    TEST_ASSERT_FALSE(wolf_sprite_page_decode(page, sizeof(page), &decoded, error, sizeof(error)));

    /* Pixel source outside the page. */
    wolf_write_u16_le(page + 6u, 2u * 2u);
    wolf_write_u16_le(page + 8u, 60u);
    wolf_write_u16_le(page + 10u, 0u);
    TEST_ASSERT_FALSE(wolf_sprite_page_decode(page, 20u, &decoded, error, sizeof(error)));

    /* Truncated column table. */
    TEST_ASSERT_FALSE(wolf_sprite_page_decode(page, 3u, &decoded, error, sizeof(error)));
}

/* The archive decoder must skip wall pages, honour sparse pages and stop at the sound boundary. */
void test_sprite_archive_reads_only_the_sprite_range(void)
{
    WolfSpriteImage first;
    WolfSpriteImage third;
    const WolfSpriteImage *sprites[3] = {&first, NULL, &third};
    WolfByteBuffer file = {0};
    WolfSpriteArchive archive;
    char error[128];

    test_make_patterned_sprite(&first, 1u);
    wolf_sprite_image_clear(&third);
    test_fill_rectangle(&third, 30, 40, 33, 63, 99u);
    TEST_ASSERT_TRUE(test_build_vswap(sprites, 3u, 4u, &file));

    TEST_ASSERT_TRUE(wolf_sprite_archive_decode(file.data, file.size, &archive, error, sizeof(error)));
    TEST_ASSERT_EQUAL_UINT16(3u, archive.sprite_count);
    TEST_ASSERT_EQUAL_UINT16(4u, archive.first_sprite_page);
    assert_images_equal(&first, &archive.sprites[0]);
    TEST_ASSERT_EQUAL_size_t(0u, wolf_sprite_image_opaque_count(&archive.sprites[1]));
    assert_images_equal(&third, &archive.sprites[2]);
    wolf_sprite_archive_destroy(&archive);
    TEST_ASSERT_NULL(archive.sprites);
    wolf_buffer_free(&file);
}

void test_sprite_archive_rejects_invalid_boundaries_and_ranges(void)
{
    WolfSpriteImage sprite;
    const WolfSpriteImage *sprites[1] = {&sprite};
    WolfByteBuffer file = {0};
    WolfSpriteArchive archive;
    char error[128];

    test_make_patterned_sprite(&sprite, 2u);
    TEST_ASSERT_TRUE(test_build_vswap(sprites, 1u, 2u, &file));

    /* Sprite range starting after the sound range. */
    wolf_write_u16_le(file.data + 2u, 3u);
    TEST_ASSERT_FALSE(wolf_sprite_archive_decode(file.data, file.size, &archive, error, sizeof(error)));
    wolf_write_u16_le(file.data + 2u, 2u);

    /* A sprite page pointing beyond the end of the file. */
    wolf_write_u32_le(file.data + 6u + 2u * 4u, (uint32_t)file.size + 10u);
    TEST_ASSERT_FALSE(wolf_sprite_archive_decode(file.data, file.size, &archive, error, sizeof(error)));

    /* Too small to hold a header at all. */
    TEST_ASSERT_FALSE(wolf_sprite_archive_decode(file.data, 4u, &archive, error, sizeof(error)));
    TEST_ASSERT_FALSE(wolf_sprite_archive_load("this-file-does-not-exist.wl6", &archive, error, sizeof(error)));
    wolf_buffer_free(&file);
}
