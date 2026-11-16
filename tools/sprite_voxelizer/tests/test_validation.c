#include "unity.h"

#include "game_palette.h"
#include "test_support.h"
#include "validation.h"

#include <string.h>

void test_compare_views_scores_identical_and_disjoint_images(void)
{
    WolfSpriteImage first;
    WolfSpriteImage second;
    WolfViewMetrics metrics;

    wolf_sprite_image_clear(&first);
    test_fill_rectangle(&first, 10, 10, 19, 19, 40u);
    wolf_compare_views(&first, &first, wolf_game_palette_rgba, &metrics);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, metrics.silhouette_iou);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, metrics.colour_accuracy);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, metrics.mean_colour_error);

    wolf_sprite_image_clear(&second);
    test_fill_rectangle(&second, 40, 40, 49, 49, 40u);
    wolf_compare_views(&first, &second, wolf_game_palette_rgba, &metrics);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, metrics.silhouette_iou);
    TEST_ASSERT_EQUAL_size_t(200u, metrics.union_pixels);

    /* Two empty frames agree perfectly. */
    wolf_sprite_image_clear(&first);
    wolf_sprite_image_clear(&second);
    wolf_compare_views(&first, &second, wolf_game_palette_rgba, &metrics);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, metrics.silhouette_iou);
}

/* Exact colour is strict; the near metric forgives a detail drawn one pixel away. */
void test_compare_views_separates_exact_and_nearby_colour(void)
{
    WolfSpriteImage original;
    WolfSpriteImage rendered;
    WolfViewMetrics metrics;

    wolf_sprite_image_clear(&original);
    test_fill_rectangle(&original, 10, 10, 19, 10, 15u);  /* white row */
    test_fill_rectangle(&original, 10, 11, 19, 11, 0u);   /* black row */
    memcpy(&rendered, &original, sizeof(rendered));
    /* Swap the rows: every pixel is wrong, but its colour is one pixel away. */
    test_fill_rectangle(&rendered, 10, 10, 19, 10, 0u);
    test_fill_rectangle(&rendered, 10, 11, 19, 11, 15u);

    wolf_compare_views(&original, &rendered, wolf_game_palette_rgba, &metrics);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, metrics.silhouette_iou);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, metrics.colour_accuracy);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, metrics.near_colour_accuracy);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 252.0, metrics.mean_colour_error);
}

/* A self-consistent rotation set has ceiling 1; disagreeing opposite views lower it. */
void test_rotation_ceiling_reflects_opposite_view_agreement(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfSpriteImage views[8];
    const WolfSpriteImage *view_pointers[8];
    double ceiling;

    test_make_box_model(model, 5, 9, 30);
    for (int rotation = 0; rotation < 8; ++rotation) {
        wolf_voxel_render_view(model, 45.0 * rotation, 0, &views[rotation], NULL);
        view_pointers[rotation] = &views[rotation];
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, wolf_rotation_set_silhouette_ceiling(view_pointers, 0));

    /* Halve the back view: pair 0/4 can now agree on at most half its area. */
    for (int row = 0; row < 48; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            views[4].opaque[wolf_sprite_pixel_index(column, row)] = 0u;
        }
    }
    ceiling = wolf_rotation_set_silhouette_ceiling(view_pointers, 6);
    TEST_ASSERT_TRUE(ceiling < 1.0);
    TEST_ASSERT_TRUE(ceiling > 0.75);
    /* Only the damaged pair is limited; the others still allow a perfect solid. */
    TEST_ASSERT_TRUE(wolf_opposite_pair_ceiling(view_pointers[0], view_pointers[4], 6) < 0.9);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, wolf_opposite_pair_ceiling(view_pointers[2], view_pointers[6], 0));
    wolf_voxel_model_destroy(model);
}

void test_comparison_sheet_lays_out_views_and_previews(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfSpriteImage originals[2];
    WolfSpriteImage rendered[2];
    const WolfSpriteImage *original_pointers[2] = {&originals[0], &originals[1]};
    WolfComparisonSheet sheet;
    const uint8_t *top_left_cell;

    test_make_box_model(model, 4, 4, 20);
    for (int view = 0; view < 2; ++view) {
        wolf_voxel_render_view(model, 90.0 * view, 0, &originals[view], NULL);
        wolf_voxel_render_view(model, 90.0 * view, 0, &rendered[view], NULL);
    }
    TEST_ASSERT_TRUE(wolf_comparison_sheet_build(original_pointers, NULL, NULL, rendered, 2u, model,
        wolf_game_palette_rgba, &sheet));
    TEST_ASSERT_EQUAL_UINT32(WOLF_SHEET_MARGIN + 3u * (WOLF_SHEET_CELL_SIZE + WOLF_SHEET_MARGIN), sheet.height);
    TEST_ASSERT_TRUE(sheet.width > 2u * WOLF_SHEET_CELL_SIZE);
    /* The difference cell of a perfect render is grey where the box is drawn. */
    top_left_cell = sheet.rgba + ((size_t)(WOLF_SHEET_MARGIN + 2u * (WOLF_SHEET_CELL_SIZE + WOLF_SHEET_MARGIN) +
        60u * WOLF_SHEET_CELL_SCALE) * sheet.width + WOLF_SHEET_MARGIN + 32u * WOLF_SHEET_CELL_SCALE) * 4u;
    TEST_ASSERT_EQUAL_UINT8(150u, top_left_cell[0]);
    TEST_ASSERT_EQUAL_UINT8(150u, top_left_cell[2]);
    wolf_comparison_sheet_destroy(&sheet);
    TEST_ASSERT_NULL(sheet.rgba);
    wolf_voxel_model_destroy(model);
}
