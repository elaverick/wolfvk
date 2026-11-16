#include "unity.h"

#include "game_palette.h"
#include "reconstruction.h"
#include "test_support.h"
#include "validation.h"

#include <math.h>
#include <string.h>

/* Render the eight original rotations of a model, as the hand-drawn frames would be. */
static void render_rotations(const WolfVoxelModel *model, WolfSpriteImage *views, double *angles)
{
    for (int rotation = 0; rotation < 8; ++rotation) {
        angles[rotation] = 45.0 * rotation;
        wolf_voxel_render_view(model, angles[rotation], 0, &views[rotation], NULL);
    }
}

static void assert_front_view_exact(const WolfVoxelModel *model, const WolfSpriteImage *image)
{
    WolfSpriteImage rendered;
    WolfViewMetrics metrics;

    wolf_voxel_render_view(model, 0.0, 0, &rendered, NULL);
    wolf_compare_views(image, &rendered, wolf_game_palette_rgba, &metrics);
    TEST_ASSERT_EQUAL_size_t(metrics.union_pixels, metrics.shared_pixels);
    TEST_ASSERT_EQUAL_size_t(metrics.shared_pixels, metrics.exact_colour_pixels);
}

/* Depth along y for one canvas pixel of a single-view model. */
static int column_thickness(const WolfVoxelModel *model, int column, int row)
{
    int thickness = 0;

    for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
        thickness += model->occupied[wolf_voxel_index(column, y, 63 - row)] ? 1 : 0;
    }
    return thickness;
}

void test_extrude_profile_reproduces_front_view_with_constant_depth(void)
{
    WolfSpriteImage image;
    WolfSingleViewOptions options = {WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 3.0};
    WolfVoxelModel *model = wolf_voxel_model_create();

    test_make_patterned_sprite(&image, 5u);
    wolf_reconstruct_single_view(&image, &options, model);
    assert_front_view_exact(model, &image);
    TEST_ASSERT_EQUAL_INT(6, column_thickness(model, 20, 30));
    TEST_ASSERT_EQUAL_size_t(6u * wolf_sprite_image_opaque_count(&image), wolf_voxel_model_count(model));
    wolf_voxel_model_destroy(model);
}

/* A run of width w becomes a disc: depth w at the centre, thinner at the ends. */
void test_lathe_profile_turns_each_run_into_a_disc(void)
{
    WolfSpriteImage image;
    WolfSingleViewOptions options = {WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0};
    WolfVoxelModel *model = wolf_voxel_model_create();

    wolf_sprite_image_clear(&image);
    test_fill_rectangle(&image, 22, 30, 41, 63, 80u);
    wolf_reconstruct_single_view(&image, &options, model);
    assert_front_view_exact(model, &image);
    TEST_ASSERT_INT_WITHIN(1, 20, column_thickness(model, 31, 40));
    TEST_ASSERT_LESS_THAN(10, column_thickness(model, 22, 40));
    /* A barrel is round, so its side view is as wide as its front view. */
    TEST_ASSERT_DOUBLE_WITHIN(0.1, 1.0, wolf_voxel_model_depth_ratio(model));
    wolf_voxel_model_destroy(model);
}

/* Inflation follows local thickness: broad areas are deep, thin limbs stay thin. */
void test_inflate_profile_rounds_by_local_thickness(void)
{
    WolfSpriteImage image;
    WolfSingleViewOptions options = {WOLF_DEPTH_PROFILE_INFLATE, 1.0, 32.0, 0.0};
    WolfVoxelModel *model = wolf_voxel_model_create();
    double half_depth[WOLF_SPRITE_PIXELS];

    wolf_sprite_image_clear(&image);
    test_fill_rectangle(&image, 16, 20, 47, 51, 50u);  /* 32-wide body */
    test_fill_rectangle(&image, 30, 52, 32, 63, 60u);  /* 3-wide leg */
    wolf_single_view_half_depths(&image, &options, half_depth);

    TEST_ASSERT_DOUBLE_WITHIN(1.5, 16.0, half_depth[wolf_sprite_pixel_index(31, 35)]);
    TEST_ASSERT_TRUE(half_depth[wolf_sprite_pixel_index(16, 35)] < half_depth[wolf_sprite_pixel_index(31, 35)]);
    TEST_ASSERT_TRUE(half_depth[wolf_sprite_pixel_index(31, 60)] <= 2.0);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, half_depth[wolf_sprite_pixel_index(5, 5)]);

    wolf_reconstruct_single_view(&image, &options, model);
    assert_front_view_exact(model, &image);

    /* Scale and cap keep flat items flat. */
    options.depth_scale = 0.5;
    options.maximum_half_depth = 4.0;
    wolf_single_view_half_depths(&image, &options, half_depth);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, half_depth[wolf_sprite_pixel_index(31, 35)]);
    wolf_voxel_model_destroy(model);
}

/* Every drawn pixel gets at least one voxel, however thin its profile. */
void test_single_view_never_drops_a_pixel(void)
{
    WolfSpriteImage image;
    WolfSingleViewOptions options = {WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 0.0};
    WolfVoxelModel *model = wolf_voxel_model_create();

    wolf_sprite_image_clear(&image);
    image.opaque[wolf_sprite_pixel_index(3, 3)] = 1u;
    image.palette_index[wolf_sprite_pixel_index(3, 3)] = 200u;
    wolf_reconstruct_single_view(&image, &options, model);
    TEST_ASSERT_EQUAL_size_t(1u, wolf_voxel_model_count(model));
    assert_front_view_exact(model, &image);
    wolf_voxel_model_destroy(model);
}

/* Eight consistent views of a solid must rebuild that solid closely. */
void test_multi_view_rebuilds_a_consistent_solid(void)
{
    WolfVoxelModel *truth = wolf_voxel_model_create();
    WolfVoxelModel *rebuilt = wolf_voxel_model_create();
    WolfSpriteImage views[8];
    const WolfSpriteImage *view_pointers[8];
    double angles[8];
    int shifts[8];
    WolfMultiViewOptions options = wolf_multi_view_default_options();

    test_make_box_model(truth, 6, 11, 40);
    render_rotations(truth, views, angles);
    for (int rotation = 0; rotation < 8; ++rotation) {
        view_pointers[rotation] = &views[rotation];
    }
    TEST_ASSERT_TRUE(wolf_reconstruct_multi_view(view_pointers, angles, 8u, &options, rebuilt, shifts));

    for (int rotation = 0; rotation < 8; ++rotation) {
        WolfSpriteImage rendered;
        WolfViewMetrics metrics;

        TEST_ASSERT_EQUAL_INT(0, shifts[rotation]);
        wolf_voxel_render_view(rebuilt, angles[rotation], shifts[rotation], &rendered, NULL);
        wolf_compare_views(&views[rotation], &rendered, wolf_game_palette_rgba, &metrics);
        TEST_ASSERT_TRUE_MESSAGE(metrics.silhouette_iou >= 0.95, "silhouette should match a consistent solid");
        TEST_ASSERT_TRUE_MESSAGE(metrics.colour_accuracy >= 0.95, "height bands should be recoloured exactly");
    }
    /* The box is deeper than wide, and the rebuild must recover that from the side views. */
    TEST_ASSERT_DOUBLE_WITHIN(0.15, 11.0 / 6.0, wolf_voxel_model_depth_ratio(rebuilt));
    wolf_voxel_model_destroy(truth);
    wolf_voxel_model_destroy(rebuilt);
}

/* A frame drawn two columns off-axis is registered back rather than carving the model away. */
void test_multi_view_registers_an_off_centre_frame(void)
{
    WolfVoxelModel *truth = wolf_voxel_model_create();
    WolfVoxelModel *rebuilt = wolf_voxel_model_create();
    WolfSpriteImage views[8];
    const WolfSpriteImage *view_pointers[8];
    double angles[8];
    int shifts[8];
    WolfMultiViewOptions options = wolf_multi_view_default_options();
    WolfSpriteImage rendered;
    WolfViewMetrics metrics;

    /* An asymmetric box so every rotation constrains the shape. */
    test_make_box_model(truth, 5, 9, 36);
    render_rotations(truth, views, angles);
    wolf_voxel_render_view(truth, angles[2], 2, &views[2], NULL);
    for (int rotation = 0; rotation < 8; ++rotation) {
        view_pointers[rotation] = &views[rotation];
    }
    TEST_ASSERT_TRUE(wolf_reconstruct_multi_view(view_pointers, angles, 8u, &options, rebuilt, shifts));
    TEST_ASSERT_EQUAL_INT(0, shifts[0]);
    TEST_ASSERT_EQUAL_INT(2, shifts[2]);

    wolf_voxel_render_view(rebuilt, angles[2], shifts[2], &rendered, NULL);
    wolf_compare_views(&views[2], &rendered, wolf_game_palette_rgba, &metrics);
    TEST_ASSERT_TRUE(metrics.silhouette_iou >= 0.95);

    /*
     * With registration disabled, strict carving trims depth instead; the
     * loss shows in the opposite side view, which sees that depth edge-on.
     */
    options.maximum_registration_shift = 0;
    TEST_ASSERT_TRUE(wolf_reconstruct_multi_view(view_pointers, angles, 8u, &options, rebuilt, shifts));
    wolf_voxel_render_view(rebuilt, angles[6], 0, &rendered, NULL);
    wolf_compare_views(&views[6], &rendered, wolf_game_palette_rgba, &metrics);
    TEST_ASSERT_TRUE(metrics.silhouette_iou < 0.95);
    wolf_voxel_model_destroy(truth);
    wolf_voxel_model_destroy(rebuilt);
}

/*
 * One inconsistent face-on frame: its opposite frame still carries the
 * material, and pruning must not uncover any pixel the face-on frames draw.
 */
void test_tolerant_carving_survives_one_inconsistent_frame(void)
{
    WolfVoxelModel *truth = wolf_voxel_model_create();
    WolfVoxelModel *rebuilt = wolf_voxel_model_create();
    WolfSpriteImage views[8];
    const WolfSpriteImage *view_pointers[8];
    double angles[8];
    WolfMultiViewOptions options = wolf_multi_view_default_options();

    test_make_cylinder_model(truth, 9.0, 40);
    render_rotations(truth, views, angles);
    /* Frame 4 forgets the top half of the object. */
    for (int row = 0; row < 40; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            views[4].opaque[wolf_sprite_pixel_index(column, row)] = 0u;
        }
    }
    for (int rotation = 0; rotation < 8; ++rotation) {
        view_pointers[rotation] = &views[rotation];
    }
    options.maximum_registration_shift = 0;
    TEST_ASSERT_TRUE(wolf_reconstruct_multi_view(view_pointers, angles, 8u, &options, rebuilt, NULL));

    for (int rotation = 0; rotation < 8; ++rotation) {
        WolfSpriteImage rendered;

        /* Only face-on coverage is guaranteed; frame 4 is the one that lies. */
        if (rotation == 4 || rotation % 2 != 0) {
            continue;
        }
        wolf_voxel_render_view(rebuilt, angles[rotation], 0, &rendered, NULL);
        for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
            if (views[rotation].opaque[pixel]) {
                TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, rendered.opaque[pixel], "a drawn pixel was uncovered");
            }
        }
    }
    wolf_voxel_model_destroy(truth);
    wolf_voxel_model_destroy(rebuilt);
}

void test_multi_view_rejects_invalid_arguments(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfMultiViewOptions options = wolf_multi_view_default_options();
    double angle = 0.0;
    WolfSpriteImage view;
    const WolfSpriteImage *views[1] = {&view};

    TEST_ASSERT_FALSE(wolf_reconstruct_multi_view(NULL, &angle, 1u, &options, model, NULL));
    TEST_ASSERT_FALSE(wolf_reconstruct_multi_view(views, &angle, 0u, &options, model, NULL));
    TEST_ASSERT_FALSE(wolf_reconstruct_multi_view(views, &angle, 17u, &options, model, NULL));
    TEST_ASSERT_FALSE(wolf_reconstruct_multi_view(views, &angle, 1u, NULL, model, NULL));
    wolf_voxel_model_destroy(model);
}

/*
 * A camera-facing frame on a reference body: the frame paints the front,
 * the body's own colours remain behind it, and parts the body lacks are
 * inflated around its centre line.
 */
void test_pose_transfer_builds_on_the_reference_body(void)
{
    WolfVoxelModel *reference = wolf_voxel_model_create();
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfSpriteImage frame;
    WolfSingleViewOptions fallback = {WOLF_DEPTH_PROFILE_INFLATE, 0.6, 32.0, 0.0};

    test_make_box_model(reference, 6, 4, 40);
    wolf_voxel_render_view(reference, 0.0, 0, &frame, NULL);
    for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        frame.palette_index[pixel] = 200u;
    }
    /* An arm held out to the side, beyond the body. */
    test_fill_rectangle(&frame, 38, 30, 47, 32, 201u);

    wolf_reconstruct_pose_transfer(&frame, reference, &fallback, model);
    assert_front_view_exact(model, &frame);
    /* The body keeps its real depth, with the frame on the front face only. */
    TEST_ASSERT_EQUAL_INT(8, column_thickness(model, 30, 45));
    TEST_ASSERT_EQUAL_UINT8(200u, model->palette_index[wolf_voxel_index(30, 28, 63 - 45)]);
    TEST_ASSERT_EQUAL_UINT8(reference->palette_index[wolf_voxel_index(30, 31, 63 - 45)],
        model->palette_index[wolf_voxel_index(30, 31, 63 - 45)]);
    /* The arm is a thin tube centred on the body's depth. */
    TEST_ASSERT_TRUE(column_thickness(model, 44, 31) >= 1);
    TEST_ASSERT_TRUE(column_thickness(model, 44, 31) <= 4);
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(44, 31, 63 - 31)] || model->occupied[wolf_voxel_index(44, 32, 63 - 31)]);
    wolf_voxel_model_destroy(reference);
    wolf_voxel_model_destroy(model);
}

/* A first-person weapon reaches into the screen: higher rows are further from the eye. */
void test_first_person_profile_recedes_into_the_screen(void)
{
    WolfSpriteImage weapon;
    WolfSingleViewOptions options = {WOLF_DEPTH_PROFILE_FIRST_PERSON, 1.0, 6.0, 0.0};
    WolfVoxelModel *model = wolf_voxel_model_create();
    int nearest_bottom = WOLF_VOXEL_DIM;
    int nearest_top = WOLF_VOXEL_DIM;

    wolf_sprite_image_clear(&weapon);
    test_fill_rectangle(&weapon, 29, 30, 34, 63, 24u);  /* barrel and hand, rising up the screen */
    wolf_reconstruct_single_view(&weapon, &options, model);
    assert_front_view_exact(model, &weapon);

    for (int y = WOLF_VOXEL_DIM - 1; y >= 0; --y) {
        if (model->occupied[wolf_voxel_index(31, y, 0)]) {
            nearest_bottom = y;
        }
        if (model->occupied[wolf_voxel_index(31, y, 63 - 30)]) {
            nearest_top = y;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(nearest_top > nearest_bottom + 30, "the muzzle must be far from the hand");
    TEST_ASSERT_TRUE(wolf_voxel_model_depth_extent(model) >= 40);
    wolf_voxel_model_destroy(model);
}
