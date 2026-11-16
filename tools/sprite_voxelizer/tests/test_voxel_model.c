#include "unity.h"

#include "game_palette.h"
#include "test_support.h"
#include "voxel_model.h"

#include <math.h>
#include <stdlib.h>

void test_view_basis_matches_original_rotation_convention(void)
{
    WolfViewBasis front = wolf_view_basis(0.0);
    WolfViewBasis left_side = wolf_view_basis(90.0);
    WolfViewBasis diagonal = wolf_view_basis(45.0);

    /* Frame 1 is seen from -y with +x to the right. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, front.right_x);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, front.right_y);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -1.0, front.toward_viewer_y);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, front.half_footprint);

    /*
     * Frame 3 (a quarter turn anticlockwise) is seen from +x, the actor's
     * left when it faces -y, and the actor's front then points screen-left.
     */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, left_side.toward_viewer_x);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, left_side.right_y);

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, sqrt(2.0) * 0.5, diagonal.half_footprint);
}

/* A single voxel lands on its own column and row, at the floor for z = 0. */
void test_front_render_maps_voxels_one_to_one(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfSpriteImage image;
    int32_t voxel_for_pixel[WOLF_SPRITE_PIXELS];

    model->occupied[wolf_voxel_index(10, 20, 0)] = 1u;
    model->palette_index[wolf_voxel_index(10, 20, 0)] = 42u;
    wolf_voxel_render_view(model, 0.0, 0, &image, voxel_for_pixel);

    TEST_ASSERT_EQUAL_size_t(1u, wolf_sprite_image_opaque_count(&image));
    TEST_ASSERT_EQUAL_UINT8(1u, image.opaque[wolf_sprite_pixel_index(10, 63)]);
    TEST_ASSERT_EQUAL_UINT8(42u, image.palette_index[wolf_sprite_pixel_index(10, 63)]);
    TEST_ASSERT_EQUAL_INT32((int32_t)wolf_voxel_index(10, 20, 0), voxel_for_pixel[wolf_sprite_pixel_index(10, 63)]);

    /* The registration offset moves the image sideways. */
    wolf_voxel_render_view(model, 0.0, 2, &image, NULL);
    TEST_ASSERT_EQUAL_UINT8(1u, image.opaque[wolf_sprite_pixel_index(12, 63)]);
    wolf_voxel_model_destroy(model);
}

/* From the side, depth becomes the horizontal axis; from behind, the image mirrors. */
void test_side_and_back_renders_follow_view_direction(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfSpriteImage image;

    model->occupied[wolf_voxel_index(10, 20, 5)] = 1u;
    wolf_voxel_render_view(model, 90.0, 0, &image, NULL);
    TEST_ASSERT_EQUAL_UINT8(1u, image.opaque[wolf_sprite_pixel_index(20, 58)]);

    wolf_voxel_render_view(model, 180.0, 0, &image, NULL);
    TEST_ASSERT_EQUAL_UINT8(1u, image.opaque[wolf_sprite_pixel_index(63 - 10, 58)]);
    wolf_voxel_model_destroy(model);
}

/* The voxel nearest the viewer supplies the pixel. */
void test_render_keeps_the_nearest_voxel(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfSpriteImage image;

    model->occupied[wolf_voxel_index(30, 10, 3)] = 1u;
    model->palette_index[wolf_voxel_index(30, 10, 3)] = 1u;
    model->occupied[wolf_voxel_index(30, 50, 3)] = 1u;
    model->palette_index[wolf_voxel_index(30, 50, 3)] = 2u;

    wolf_voxel_render_view(model, 0.0, 0, &image, NULL);
    TEST_ASSERT_EQUAL_UINT8(1u, image.palette_index[wolf_sprite_pixel_index(30, 60)]);
    wolf_voxel_render_view(model, 180.0, 0, &image, NULL);
    TEST_ASSERT_EQUAL_UINT8(2u, image.palette_index[wolf_sprite_pixel_index(63 - 30, 60)]);
    wolf_voxel_model_destroy(model);
}

/* A diagonal cube covers its full projected width, so a solid box has no gaps. */
void test_diagonal_render_has_no_gaps_and_wider_footprint(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfSpriteImage image;
    int first_column;
    int last_column;
    WolfViewBasis diagonal = wolf_view_basis(45.0);
    int drawn_columns = 0;

    wolf_voxel_footprint_columns(&diagonal, 31, 32, 0, &first_column, &last_column);
    TEST_ASSERT_TRUE(last_column >= first_column);

    test_make_box_model(model, 8, 8, 20);
    wolf_voxel_render_view(model, 45.0, 0, &image, NULL);
    for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
        drawn_columns += image.opaque[wolf_sprite_pixel_index(column, 63)] ? 1 : 0;
    }
    /* A 16x16 square seen corner-on is 16 * sqrt(2) ~ 22.6 columns wide. */
    TEST_ASSERT_INT_WITHIN(2, 23, drawn_columns);
    for (int column = 32 - 10; column < 32 + 10; ++column) {
        TEST_ASSERT_EQUAL_UINT8(1u, image.opaque[wolf_sprite_pixel_index(column, 50)]);
    }
    wolf_voxel_model_destroy(model);
}

void test_surface_detection_and_counting(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();

    test_make_box_model(model, 2, 2, 3);
    TEST_ASSERT_EQUAL_size_t(4u * 4u * 3u, wolf_voxel_model_count(model));
    TEST_ASSERT_TRUE(wolf_voxel_is_surface(model, 30, 30, 0));
    TEST_ASSERT_FALSE(wolf_voxel_is_surface(model, 0, 0, 0));
    wolf_voxel_model_destroy(model);

    model = wolf_voxel_model_create();
    test_make_box_model(model, 3, 3, 5);
    /* Fully enclosed voxel. */
    TEST_ASSERT_FALSE(wolf_voxel_is_surface(model, 31, 31, 2));
    wolf_voxel_model_destroy(model);
}

/* The shaded preview must draw the model and leave the background transparent. */
void test_preview_render_draws_shaded_model(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    uint8_t *rgba = malloc(96u * 96u * 4u);
    size_t drawn = 0u;
    bool shaded = false;
    uint8_t first_red = 0u;

    test_make_box_model(model, 8, 8, 30);
    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        model->palette_index[voxel] = 15u; /* white, so shading is visible */
    }
    wolf_voxel_render_preview(model, wolf_game_palette_rgba, -35.0, 25.0, 96, rgba);
    for (size_t pixel = 0u; pixel < 96u * 96u; ++pixel) {
        if (rgba[pixel * 4u + 3u] != 0u) {
            if (drawn == 0u) {
                first_red = rgba[pixel * 4u];
            }
            shaded = shaded || rgba[pixel * 4u] != first_red;
            ++drawn;
        }
    }
    /* A 16x16x30 box fills a few hundred of the 96x96 preview pixels. */
    TEST_ASSERT_GREATER_THAN(300u, drawn);
    TEST_ASSERT_LESS_THAN(96u * 96u, drawn);
    TEST_ASSERT_TRUE_MESSAGE(shaded, "different faces should receive different shading");
    free(rgba);
    wolf_voxel_model_destroy(model);
}
