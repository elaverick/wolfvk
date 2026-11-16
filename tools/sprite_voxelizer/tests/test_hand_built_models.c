#include "unity.h"

#include "hand_built_models.h"
#include "reconstruction.h"
#include "test_support.h"

/* Number of occupied voxels with x in [first, last]. */
static size_t count_in_columns(const WolfVoxelModel *model, int first, int last)
{
    size_t count = 0u;

    for (int z = 0; z < WOLF_VOXEL_DIM; ++z) {
        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = first; x <= last; ++x) {
                count += model->occupied[wolf_voxel_index(x, y, z)] ? 1u : 0u;
            }
        }
    }
    return count;
}

static bool has_colour_at_or_above(const WolfVoxelModel *model, uint8_t palette_index, int minimum_z)
{
    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        if (model->occupied[voxel] && model->palette_index[voxel] == palette_index &&
            (int)(voxel / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM)) >= minimum_z) {
            return true;
        }
    }
    return false;
}

/* The table stands on the floor with its candles above the top and a chair on each side. */
void test_table_and_chairs_model_is_furniture_shaped(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();

    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_TABLE_AND_CHAIRS, NULL, model));
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(33, 31, 0)]);   /* pedestal foot on the floor */
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(33, 31, 24)]);  /* table top */
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(14, 31, 24)]);  /* round top reaches the chairs */
    TEST_ASSERT_TRUE(has_colour_at_or_above(model, 41u, 27));         /* red candles above the top */
    /* Chair backs rise beside the table, one each side, mirror images in size. */
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(8, 26, 30)]);
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(57, 26, 30)]);
    TEST_ASSERT_EQUAL_size_t(count_in_columns(model, 0, 10), count_in_columns(model, 55, 63));
    /* A round table is as deep as it is wide. */
    TEST_ASSERT_DOUBLE_WITHIN(0.35, 1.0, wolf_voxel_model_depth_ratio(model));
    wolf_voxel_model_destroy(model);
}

/* The bed is seen end-on in the sprite, so it must be much longer than it is wide. */
void test_bed_model_is_long_and_raised(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();

    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_BED, NULL, model));
    TEST_ASSERT_EQUAL_INT(60, wolf_voxel_model_depth_extent(model));
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(11, 2, 0)]);    /* corner post on the floor */
    TEST_ASSERT_FALSE(model->occupied[wolf_voxel_index(31, 31, 0)]);  /* open space under the bed */
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(31, 31, 17)]);  /* mattress */
    TEST_ASSERT_TRUE(has_colour_at_or_above(model, 19u, 21));         /* pillow on top */
    TEST_ASSERT_FALSE(wolf_build_hand_model(WOLF_HAND_BUILT_NONE, NULL, model));
    wolf_voxel_model_destroy(model);
}

/* A turkey leg on a plate: a flat cyan plate on the floor, the meat and its bone on top. */
void test_food_model_is_a_drumstick_on_a_plate(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();

    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_FOOD, NULL, model));
    TEST_ASSERT_EQUAL_UINT8(126u, model->palette_index[wolf_voxel_index(30, 31, 0)]);  /* plate underside */
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(43, 31, 1)]);                    /* plate rim */
    TEST_ASSERT_TRUE(has_colour_at_or_above(model, 18u, 3));                           /* the bone */
    TEST_ASSERT_TRUE(has_colour_at_or_above(model, 61u, 6));                           /* roast skin on top */
    TEST_ASSERT_TRUE(wolf_voxel_model_depth_extent(model) >= 25);                      /* a round plate */
    wolf_voxel_model_destroy(model);
}

/* The first aid kit is a flat white box: blue trim at the base, a red cross on the white lid. */
void test_first_aid_model_is_a_flat_box_with_a_red_cross(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();

    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_FIRST_AID, NULL, model));
    TEST_ASSERT_EQUAL_UINT8(52u, model->palette_index[wolf_voxel_index(31, 31, 5)]);   /* cross centre */
    TEST_ASSERT_EQUAL_UINT8(16u, model->palette_index[wolf_voxel_index(20, 23, 5)]);   /* white lid corner */
    TEST_ASSERT_EQUAL_UINT8(151u, model->palette_index[wolf_voxel_index(19, 22, 1)]);  /* blue trim */
    TEST_ASSERT_FALSE(model->occupied[wolf_voxel_index(31, 31, 6)]);                   /* flat: nothing above the lid */
    wolf_voxel_model_destroy(model);
}

/* Gibs keep the sprite's skull, standing, with blood flat on the floor around it. */
void test_gibs_model_lays_blood_flat_under_the_skull(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfSpriteImage reference;
    size_t raised_blood = 0u;

    wolf_sprite_image_clear(&reference);
    test_fill_rectangle(&reference, 4, 58, 60, 63, 40u);   /* a wide pool of blood */
    test_fill_rectangle(&reference, 12, 50, 19, 57, 16u);  /* the skull above it */
    test_fill_rectangle(&reference, 30, 61, 40, 61, 18u);  /* a bone lying in the blood */
    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_GIBS, &reference, model));

    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        if (model->occupied[voxel] && model->palette_index[voxel] == 40u && voxel / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM) > 0u) {
            ++raised_blood;
        }
    }
    TEST_ASSERT_EQUAL_size_t(0u, raised_blood);
    TEST_ASSERT_TRUE(has_colour_at_or_above(model, 16u, 6));    /* the skull stands up */
    TEST_ASSERT_FALSE(has_colour_at_or_above(model, 16u, 10));  /* but sits on the floor, not in the air */
    TEST_ASSERT_TRUE(has_colour_at_or_above(model, 18u, 1));    /* the bone lies on the blood */
    TEST_ASSERT_TRUE(wolf_voxel_model_depth_extent(model) >= 12);
    wolf_voxel_model_destroy(model);
}
