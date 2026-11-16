#include "unity.h"

#include "hand_built_models.h"
#include "reconstruction.h"

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

    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_TABLE_AND_CHAIRS, model));
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

    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_BED, model));
    TEST_ASSERT_EQUAL_INT(60, wolf_voxel_model_depth_extent(model));
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(11, 2, 0)]);    /* corner post on the floor */
    TEST_ASSERT_FALSE(model->occupied[wolf_voxel_index(31, 31, 0)]);  /* open space under the bed */
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(31, 31, 17)]);  /* mattress */
    TEST_ASSERT_TRUE(has_colour_at_or_above(model, 17u, 19));         /* pillow on top */
    TEST_ASSERT_FALSE(wolf_build_hand_model(WOLF_HAND_BUILT_NONE, model));
    wolf_voxel_model_destroy(model);
}
