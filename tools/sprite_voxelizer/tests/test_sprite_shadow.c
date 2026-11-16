#include "unity.h"

#include "sprite_shadow.h"
#include "test_support.h"

/* A barrel-like object with a flat dark shadow spreading to its right. */
static void make_object_with_shadow(WolfSpriteImage *image)
{
    wolf_sprite_image_clear(image);
    test_fill_rectangle(image, 20, 30, 35, 63, 100u);
    test_fill_rectangle(image, 36, 59, 49, 63, 0u);
    test_fill_rectangle(image, 50, 60, 52, 63, 28u);
}

void test_shadow_removal_lifts_a_flat_floor_shadow(void)
{
    WolfSpriteImage image;
    WolfSpriteImage shadow_free;
    uint8_t mask[WOLF_SPRITE_PIXELS];
    size_t removed;

    make_object_with_shadow(&image);
    removed = wolf_sprite_remove_floor_shadow(&image, &shadow_free, mask);
    TEST_ASSERT_EQUAL_size_t(14u * 5u + 3u * 4u, removed);
    TEST_ASSERT_EQUAL_UINT8(0u, shadow_free.opaque[wolf_sprite_pixel_index(40, 62)]);
    TEST_ASSERT_EQUAL_UINT8(1u, mask[wolf_sprite_pixel_index(51, 61)]);
    /* The object itself is untouched. */
    TEST_ASSERT_EQUAL_UINT8(1u, shadow_free.opaque[wolf_sprite_pixel_index(25, 63)]);
    TEST_ASSERT_EQUAL_size_t(16u * 34u, wolf_sprite_image_opaque_count(&shadow_free));
}

/*
 * Objects use the shadow greys too. A dark column that continues up into
 * the object is its shading, and a dark detail taller than it is wide is
 * standing up, not lying on the floor; neither may be removed.
 */
void test_shadow_removal_keeps_dark_parts_of_the_object(void)
{
    WolfSpriteImage image;
    WolfSpriteImage shadow_free;

    wolf_sprite_image_clear(&image);
    test_fill_rectangle(&image, 20, 30, 35, 63, 100u);
    test_fill_rectangle(&image, 33, 30, 35, 63, 30u);   /* shaded right side, top to bottom */
    test_fill_rectangle(&image, 40, 57, 41, 63, 0u);    /* a dark post, 2 wide and 7 tall */
    TEST_ASSERT_EQUAL_size_t(0u, wolf_sprite_remove_floor_shadow(&image, &shadow_free, NULL));
    TEST_ASSERT_EQUAL_size_t(wolf_sprite_image_opaque_count(&image), wolf_sprite_image_opaque_count(&shadow_free));
}

/* A dark crumb left beside a removed shadow goes with it; a sprite without a shadow keeps its crumbs. */
void test_shadow_removal_clears_detached_crumbs(void)
{
    WolfSpriteImage image;
    WolfSpriteImage shadow_free;

    make_object_with_shadow(&image);
    /* A crumb of dither floating above the shadow's edge. */
    test_fill_rectangle(&image, 54, 57, 54, 57, 27u);
    (void)wolf_sprite_remove_floor_shadow(&image, &shadow_free, NULL);
    TEST_ASSERT_EQUAL_UINT8(0u, shadow_free.opaque[wolf_sprite_pixel_index(54, 57)]);

    wolf_sprite_image_clear(&image);
    test_fill_rectangle(&image, 20, 30, 35, 63, 100u);
    test_fill_rectangle(&image, 54, 57, 54, 57, 27u);
    TEST_ASSERT_EQUAL_size_t(0u, wolf_sprite_remove_floor_shadow(&image, &shadow_free, NULL));
}

void test_shadow_palette_matches_the_painted_shadow_colours(void)
{
    TEST_ASSERT_TRUE(wolf_palette_index_is_shadow(0u));
    TEST_ASSERT_TRUE(wolf_palette_index_is_shadow(26u));
    TEST_ASSERT_TRUE(wolf_palette_index_is_shadow(31u));
    TEST_ASSERT_TRUE(wolf_palette_index_is_shadow(235u));
    TEST_ASSERT_FALSE(wolf_palette_index_is_shadow(25u));
    TEST_ASSERT_FALSE(wolf_palette_index_is_shadow(32u));
    TEST_ASSERT_FALSE(wolf_palette_index_is_shadow(100u));
}
