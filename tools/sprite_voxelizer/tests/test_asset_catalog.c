#include "unity.h"

#include "asset_catalog.h"

#include <stdlib.h>
#include <string.h>

static int sprite_index(const char *name)
{
    for (int sprite = 0; sprite < WOLF_WL6_SPRITE_COUNT; ++sprite) {
        if (strcmp(wolf_wl6_sprite_names[sprite], name) == 0) {
            return sprite;
        }
    }
    return -1;
}

static const WolfAnimation *find_animation(const WolfAnimation *animations, size_t count, const char *name)
{
    for (size_t animation = 0u; animation < count; ++animation) {
        if (strcmp(animations[animation].name, name) == 0) {
            return &animations[animation];
        }
    }
    return NULL;
}

/* The transcribed tables must match the WL6 enum layout the VSWAP pages follow. */
void test_wl6_tables_match_original_enumeration(void)
{
    TEST_ASSERT_EQUAL_STRING("SPR_DEMO", wolf_wl6_sprite_names[0]);
    TEST_ASSERT_EQUAL_INT(2, sprite_index("SPR_STAT_0"));
    TEST_ASSERT_EQUAL_INT(50, sprite_index("SPR_GRD_S_1"));
    TEST_ASSERT_EQUAL_STRING("SPR_CHAINATK4", wolf_wl6_sprite_names[WOLF_WL6_SPRITE_COUNT - 1]);
    TEST_ASSERT_EQUAL_INT(WOLF_WL6_SPRITE_COUNT - 20, sprite_index("SPR_KNIFEREADY"));

    for (int state = 0; state < WOLF_WL6_STATE_COUNT; ++state) {
        TEST_ASSERT_TRUE(wolf_wl6_states[state].sprite_index >= 0 &&
            wolf_wl6_states[state].sprite_index < WOLF_WL6_SPRITE_COUNT);
        TEST_ASSERT_TRUE(wolf_wl6_states[state].next_state_index >= -1 &&
            wolf_wl6_states[state].next_state_index < WOLF_WL6_STATE_COUNT);
    }
}

/* Rotation sets come from the original rotate flags: 25 sets of eight frames. */
void test_catalog_groups_rotation_sets_from_state_flags(void)
{
    WolfAssetCatalog *catalog = malloc(sizeof(WolfAssetCatalog));
    size_t rotation_sets = 0u;
    int guard_stand = sprite_index("SPR_GRD_S_1");
    int asset;

    wolf_asset_catalog_build(catalog);
    for (size_t index = 0u; index < catalog->asset_count; ++index) {
        rotation_sets += catalog->assets[index].kind == WOLF_ASSET_ROTATION_SET ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_size_t(25u, rotation_sets);
    TEST_ASSERT_EQUAL_size_t(WOLF_WL6_SPRITE_COUNT - 25u * 7u, catalog->asset_count);

    asset = catalog->asset_for_sprite[guard_stand + 5];
    TEST_ASSERT_EQUAL_STRING("grd_s", catalog->assets[asset].name);
    TEST_ASSERT_EQUAL_INT(5, catalog->rotation_for_sprite[guard_stand + 5]);
    TEST_ASSERT_EQUAL_INT(guard_stand, catalog->assets[asset].first_sprite);
    TEST_ASSERT_EQUAL_STRING("GRD", catalog->assets[asset].subject);
    TEST_ASSERT_EQUAL_INT(asset, wolf_asset_catalog_reference_for_subject(catalog, "GRD"));
    TEST_ASSERT_EQUAL_STRING("rocket", catalog->assets[catalog->asset_for_sprite[sprite_index("SPR_ROCKET_3")]].name);

    /* Every sprite belongs to exactly one asset. */
    for (int sprite = 0; sprite < WOLF_WL6_SPRITE_COUNT; ++sprite) {
        TEST_ASSERT_TRUE(catalog->asset_for_sprite[sprite] >= 0);
    }
    TEST_ASSERT_EQUAL_DOUBLE(135.0, wolf_rotation_degrees(3));
    free(catalog);
}

void test_catalog_assigns_depth_profiles_to_camera_facing_sprites(void)
{
    WolfAssetCatalog *catalog = malloc(sizeof(WolfAssetCatalog));
    const WolfVoxelAsset *barrel;
    const WolfVoxelAsset *key;
    const WolfVoxelAsset *pain;
    const WolfVoxelAsset *weapon;

    wolf_asset_catalog_build(catalog);
    barrel = &catalog->assets[catalog->asset_for_sprite[sprite_index("SPR_STAT_1")]];
    key = &catalog->assets[catalog->asset_for_sprite[sprite_index("SPR_STAT_20")]];
    pain = &catalog->assets[catalog->asset_for_sprite[sprite_index("SPR_GRD_PAIN_1")]];
    weapon = &catalog->assets[catalog->asset_for_sprite[sprite_index("SPR_PISTOLATK2")]];

    TEST_ASSERT_EQUAL_STRING("stat_1", barrel->name);
    TEST_ASSERT_EQUAL_STRING("green barrel", barrel->description);
    TEST_ASSERT_EQUAL_INT(WOLF_DEPTH_PROFILE_LATHE, barrel->single_view.profile);
    TEST_ASSERT_EQUAL_INT(WOLF_DEPTH_PROFILE_EXTRUDE, key->single_view.profile);
    TEST_ASSERT_EQUAL_INT(WOLF_ASSET_SINGLE_VIEW, pain->kind);
    TEST_ASSERT_TRUE(pain->depth_from_subject);
    TEST_ASSERT_FALSE(weapon->depth_from_subject);
    TEST_ASSERT_EQUAL_STRING("first-person weapon", weapon->description);
    /* The original enum spells one weapon frame without the SPR_ prefix. */
    TEST_ASSERT_EQUAL_STRING("machinegunatk3",
        catalog->assets[catalog->asset_for_sprite[sprite_index("MACHINEGUNATK3")]].name);
    free(catalog);
}

/* Animation sequences follow the original next pointers, tics and loops. */
void test_animation_catalog_rebuilds_original_sequences(void)
{
    WolfAnimation *animations = malloc(sizeof(WolfAnimation) * WOLF_ANIMATION_CAPACITY);
    size_t count = wolf_animation_catalog_build(animations, WOLF_ANIMATION_CAPACITY);
    const WolfAnimation *chase = find_animation(animations, count, "grdchase");
    const WolfAnimation *die = find_animation(animations, count, "grddie");
    const WolfAnimation *shoot = find_animation(animations, count, "grdshoot");
    const WolfAnimation *pain = find_animation(animations, count, "grdpain");
    const WolfAnimation *pain_variant = find_animation(animations, count, "grdpain1");
    const WolfAnimation *pistol = find_animation(animations, count, "player_pistol_attack");
    static const int chase_tics[6] = {10, 3, 8, 10, 3, 8};

    TEST_ASSERT_NOT_NULL(chase);
    TEST_ASSERT_EQUAL_size_t(6u, chase->frame_count);
    TEST_ASSERT_EQUAL_INT(0, chase->loop_frame);
    for (size_t frame = 0u; frame < 6u; ++frame) {
        TEST_ASSERT_EQUAL_INT(chase_tics[frame], chase->frames[frame].tics);
        TEST_ASSERT_EQUAL_INT(1, chase->frames[frame].rotate);
    }
    TEST_ASSERT_EQUAL_INT(sprite_index("SPR_GRD_W2_1"), chase->frames[2].sprite_index);

    TEST_ASSERT_NOT_NULL(die);
    TEST_ASSERT_EQUAL_size_t(4u, die->frame_count);
    TEST_ASSERT_EQUAL_INT(3, die->loop_frame);
    TEST_ASSERT_EQUAL_INT(sprite_index("SPR_GRD_DEAD"), die->frames[3].sprite_index);

    TEST_ASSERT_NOT_NULL(shoot);
    TEST_ASSERT_EQUAL_size_t(3u, shoot->frame_count);
    TEST_ASSERT_EQUAL_INT(-1, shoot->loop_frame);
    TEST_ASSERT_EQUAL_STRING("grdchase", shoot->next_animation);

    /* Two entry points into one family become two named sequences. */
    TEST_ASSERT_NOT_NULL(pain);
    TEST_ASSERT_NOT_NULL(pain_variant);
    TEST_ASSERT_EQUAL_INT(sprite_index("SPR_GRD_PAIN_2"), pain_variant->frames[0].sprite_index);

    TEST_ASSERT_NOT_NULL(pistol);
    TEST_ASSERT_EQUAL_size_t(4u, pistol->frame_count);
    TEST_ASSERT_EQUAL_INT(6, pistol->frames[0].tics);
    TEST_ASSERT_EQUAL_INT(sprite_index("SPR_PISTOLATK1"), pistol->frames[0].sprite_index);
    TEST_ASSERT_EQUAL_STRING("player_pistol_ready", pistol->next_animation);

    /* Names must be unique because they become file names. */
    for (size_t first = 0u; first < count; ++first) {
        for (size_t second = first + 1u; second < count; ++second) {
            TEST_ASSERT_TRUE(strcmp(animations[first].name, animations[second].name) != 0);
        }
    }
    free(animations);
}
