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

/* A well sprite: grey brick wall, a light rim along the top and an opening showing the inside. */
static void make_well_reference(WolfSpriteImage *reference, uint8_t inside)
{
    wolf_sprite_image_clear(reference);
    test_fill_rectangle(reference, 16, 40, 47, 59, 24u);    /* brick wall */
    test_fill_rectangle(reference, 20, 60, 43, 61, 24u);    /* bottom edge, narrowed by perspective */
    test_fill_rectangle(reference, 16, 40, 47, 42, 16u);    /* light rim */
    test_fill_rectangle(reference, 20, 44, 43, 45, inside); /* the opening */
}

/* Leftmost and rightmost occupied x on the row y = 31 at layer z, through the wall. */
static void ring_extent(const WolfVoxelModel *model, int z, int *left, int *right)
{
    *left = -1;
    *right = -1;
    for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
        if (model->occupied[wolf_voxel_index(x, 31, z)]) {
            *left = *left < 0 ? x : *left;
            *right = x;
        }
    }
}

/* The well is a hollow ring of one radius from bottom to rim, open above a dark floor. */
void test_well_model_is_a_hollow_ring_of_constant_radius(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfSpriteImage reference;
    int bottom_left;
    int bottom_right;
    int top_left;
    int top_right;
    int front = -1;
    int back = -1;

    make_well_reference(&reference, 8u);
    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_WELL, &reference, model));
    ring_extent(model, 1, &bottom_left, &bottom_right);
    ring_extent(model, 15, &top_left, &top_right);
    /* No narrowing neck: the perspective-narrowed bottom rows do not shrink the wall. */
    TEST_ASSERT_EQUAL_INT(top_left, bottom_left);
    TEST_ASSERT_EQUAL_INT(top_right, bottom_right);
    TEST_ASSERT_INT_WITHIN(1, 32, bottom_right - bottom_left + 1);
    /* As deep as it is wide, and hollow: the centre is open above the floor. */
    TEST_ASSERT_DOUBLE_WITHIN(0.1, 1.0, wolf_voxel_model_depth_ratio(model));
    TEST_ASSERT_FALSE(model->occupied[wolf_voxel_index(31, 31, 8)]);
    TEST_ASSERT_EQUAL_UINT8(8u, model->palette_index[wolf_voxel_index(31, 31, 0)]);
    /* Bricks below, the light rim on top, all round the back as well as the front. */
    for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
        if (model->occupied[wolf_voxel_index(31, y, 5)]) {
            front = front < 0 ? y : front;
            back = y;
        }
    }
    TEST_ASSERT_TRUE(front >= 0 && front <= 17 && back >= 46);
    TEST_ASSERT_EQUAL_UINT8(24u, model->palette_index[wolf_voxel_index(31, front, 5)]);
    TEST_ASSERT_EQUAL_UINT8(24u, model->palette_index[wolf_voxel_index(31, back, 5)]);
    TEST_ASSERT_TRUE(has_colour_at_or_above(model, 16u, 17));
    TEST_ASSERT_FALSE(has_colour_at_or_above(model, 1u, 18));
    wolf_voxel_model_destroy(model);
}

/* Water seen through the opening fills the shaft to just below the rim. */
void test_well_model_holds_the_sprite_water(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfSpriteImage reference;

    make_well_reference(&reference, 151u);
    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_WELL, &reference, model));
    TEST_ASSERT_EQUAL_UINT8(151u, model->palette_index[wolf_voxel_index(31, 31, 12)]);
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(31, 31, 12)]);
    TEST_ASSERT_FALSE(model->occupied[wolf_voxel_index(31, 31, 13)]);

    /* A sprite too short to be a well builds nothing. */
    wolf_voxel_model_clear(model);
    wolf_sprite_image_clear(&reference);
    test_fill_rectangle(&reference, 16, 58, 47, 63, 24u);
    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_WELL, &reference, model));
    TEST_ASSERT_EQUAL_size_t(0u, wolf_voxel_model_count(model));
    wolf_voxel_model_destroy(model);
}

/* A puddle is one voxel thick and spreads back across the floor, keeping the sprite's shades. */
void test_puddle_model_is_one_voxel_thick(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfSpriteImage reference;
    bool used[256] = {false};
    int shades = 0;

    wolf_sprite_image_clear(&reference);
    test_fill_rectangle(&reference, 8, 58, 55, 63, 148u);
    test_fill_rectangle(&reference, 12, 60, 50, 60, 146u);
    test_fill_rectangle(&reference, 14, 62, 40, 62, 152u);
    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_PUDDLE, &reference, model));
    TEST_ASSERT_TRUE(wolf_voxel_model_count(model) > 500u);
    TEST_ASSERT_FALSE(has_colour_at_or_above(model, 148u, 1));
    TEST_ASSERT_FALSE(has_colour_at_or_above(model, 146u, 1));
    TEST_ASSERT_FALSE(has_colour_at_or_above(model, 152u, 1));
    TEST_ASSERT_TRUE(wolf_voxel_model_depth_extent(model) >= 20);
    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        if (model->occupied[voxel] && !used[model->palette_index[voxel]]) {
            used[model->palette_index[voxel]] = true;
            ++shades;
            /* Only the sprite's own colours. */
            TEST_ASSERT_TRUE(model->palette_index[voxel] == 146u || model->palette_index[voxel] == 148u ||
                model->palette_index[voxel] == 152u);
        }
    }
    TEST_ASSERT_EQUAL_INT(3, shades);
    wolf_voxel_model_destroy(model);
}

/* The bone pile is low and roughly round, with one dark-eyed skull among the bones. */
void test_bone_pile_model_is_a_low_pile_with_a_skull(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfSpriteImage reference;
    int top = 0;

    wolf_sprite_image_clear(&reference);
    test_fill_rectangle(&reference, 2, 49, 62, 63, 19u);
    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_BONE_PILE, &reference, model));
    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        if (model->occupied[voxel]) {
            int z = (int)(voxel / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM));

            top = z > top ? z : top;
        }
    }
    /* Far lower than the sprite's 15 rows: bones lying in a heap, not stacked up. */
    TEST_ASSERT_TRUE(top >= 4 && top <= 12);
    /* Round: about as deep as it is wide. */
    TEST_ASSERT_DOUBLE_WITHIN(0.3, 0.9, wolf_voxel_model_depth_ratio(model));
    TEST_ASSERT_TRUE(has_colour_at_or_above(model, 30u, 0));   /* eye sockets */
    TEST_ASSERT_TRUE(has_colour_at_or_above(model, 15u, 4));   /* the white top of the skull */
    wolf_voxel_model_destroy(model);
}

/* The chest stands on the floor, open, its face as drawn and treasure heaped above the rim. */
void test_treasure_chest_model_is_open_and_full(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    WolfSpriteImage reference;

    wolf_sprite_image_clear(&reference);
    test_fill_rectangle(&reference, 23, 54, 38, 61, 39u);
    test_fill_rectangle(&reference, 29, 57, 32, 58, 125u);   /* the lock */
    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_TREASURE_CHEST, &reference, model));
    /* The face spans rows 54-61, so its bottom row 61 is layer 0. */
    TEST_ASSERT_EQUAL_UINT8(125u, model->palette_index[wolf_voxel_index(30, 26, 61 - 57)]);
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(30, 31, 0)]);
    /* Coins rise above the walls (top layer 7) and the open lid stands up behind. */
    TEST_ASSERT_TRUE(model->occupied[wolf_voxel_index(30, 31, 8)]);
    TEST_ASSERT_TRUE(has_colour_at_or_above(model, 39u, 14));
    TEST_ASSERT_TRUE(has_colour_at_or_above(model, 102u, 6) || has_colour_at_or_above(model, 140u, 6) ||
        has_colour_at_or_above(model, 105u, 6) || has_colour_at_or_above(model, 137u, 6) ||
        has_colour_at_or_above(model, 146u, 6));
    TEST_ASSERT_FALSE(model->occupied[wolf_voxel_index(30, 31, 20)]);
    wolf_voxel_model_destroy(model);
}

/* The flag's cloth is pleated round the pole: folds cut into the turned cloth, the pole intact. */
void test_flag_model_pleats_the_cloth_round_the_pole(void)
{
    WolfVoxelModel *flag = wolf_voxel_model_create();
    WolfVoxelModel *plain = wolf_voxel_model_create();
    WolfSpriteImage reference;
    WolfSingleViewOptions lathe = {WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, NULL, false, false, 0, 0, false};

    wolf_sprite_image_clear(&reference);
    test_fill_rectangle(&reference, 31, 4, 33, 60, 210u);   /* pole */
    test_fill_rectangle(&reference, 26, 14, 37, 50, 39u);   /* red cloth */
    test_fill_rectangle(&reference, 24, 60, 40, 63, 73u);   /* gold base */
    TEST_ASSERT_TRUE(wolf_build_hand_model(WOLF_HAND_BUILT_FLAG, &reference, flag));
    wolf_reconstruct_single_view(&reference, &lathe, plain);
    TEST_ASSERT_TRUE(wolf_voxel_model_count(flag) < wolf_voxel_model_count(plain));
    TEST_ASSERT_TRUE(wolf_voxel_model_count(flag) > wolf_voxel_model_count(plain) / 2u);
    TEST_ASSERT_TRUE(flag->occupied[wolf_voxel_index(32, 31, 63 - 30)]);   /* round the pole */
    TEST_ASSERT_TRUE(has_colour_at_or_above(flag, 73u, 0));
    TEST_ASSERT_TRUE(has_colour_at_or_above(flag, 210u, 55));
    wolf_voxel_model_destroy(plain);
    wolf_voxel_model_destroy(flag);
}
