#include "unity.h"

#include "asset_catalog.h"

#include <stdio.h>
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

static const WolfAnimation *find_sequence(const WolfAnimation *sequences, size_t count, const char *name)
{
    for (size_t sequence = 0u; sequence < count; ++sequence) {
        if (strcmp(sequences[sequence].name, name) == 0) {
            return &sequences[sequence];
        }
    }
    return NULL;
}

static const WolfVoxelAsset *asset_for(const WolfAssetCatalog *catalog, const char *sprite_name)
{
    return &catalog->assets[catalog->asset_for_sprite[sprite_index(sprite_name)]];
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

/* Rotation sets come from the original rotate flags: 25 sets of eight frames, the rocket revolved. */
void test_catalog_groups_rotation_sets_from_state_flags(void)
{
    WolfAssetCatalog *catalog = malloc(sizeof(WolfAssetCatalog));
    size_t rotation_sets = 0u;
    int guard_stand = sprite_index("SPR_GRD_S_1");
    const WolfVoxelAsset *asset;

    wolf_asset_catalog_build(catalog);
    for (size_t index = 0u; index < catalog->asset_count; ++index) {
        rotation_sets += catalog->assets[index].method == WOLF_ASSET_ROTATION_SET ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_size_t(24u, rotation_sets);
    TEST_ASSERT_EQUAL_INT(WOLF_ASSET_AXIAL_REVOLVE, asset_for(catalog, "SPR_ROCKET_1")->method);
    TEST_ASSERT_EQUAL_INT(8, asset_for(catalog, "SPR_ROCKET_1")->sprite_count);
    TEST_ASSERT_EQUAL_size_t(WOLF_WL6_SPRITE_COUNT - 25u * 7u, catalog->asset_count);

    asset = asset_for(catalog, "SPR_GRD_S_6");
    TEST_ASSERT_EQUAL_STRING("grd_s", asset->name);
    TEST_ASSERT_EQUAL_INT(5, catalog->rotation_for_sprite[guard_stand + 5]);
    TEST_ASSERT_EQUAL_INT(guard_stand, asset->first_sprite);
    TEST_ASSERT_TRUE(asset->strip_shadow);
    TEST_ASSERT_EQUAL_STRING("rocket", asset_for(catalog, "SPR_ROCKET_3")->name);
    TEST_ASSERT_FALSE(asset_for(catalog, "SPR_ROCKET_3")->strip_shadow);

    for (int sprite = 0; sprite < WOLF_WL6_SPRITE_COUNT; ++sprite) {
        TEST_ASSERT_TRUE(catalog->asset_for_sprite[sprite] >= 0);
    }
    TEST_ASSERT_EQUAL_DOUBLE(135.0, wolf_rotation_degrees(3));
    free(catalog);
}

/*
 * One file per subject, its animations grouping the poses as the original
 * names do: the guard is s, w (four strides), pain (two), die (three), dead
 * and shoot (three).
 */
void test_catalog_groups_poses_into_subject_animations(void)
{
    WolfAssetCatalog *catalog = malloc(sizeof(WolfAssetCatalog));
    static const char *const guard_animations[6] = {"grd_s", "grd_w", "grd_pain", "grd_die", "grd_dead", "grd_shoot"};
    static const size_t guard_poses[6] = {1u, 4u, 2u, 3u, 1u, 3u};
    const WolfSubject *guard;
    const WolfSubject *mutant;
    int subject;

    wolf_asset_catalog_build(catalog);
    subject = wolf_asset_catalog_find_subject(catalog, "grd");
    TEST_ASSERT_TRUE(subject >= 0);
    guard = &catalog->subjects[subject];
    TEST_ASSERT_EQUAL_size_t(6u, guard->animation_count);
    for (size_t animation = 0u; animation < 6u; ++animation) {
        TEST_ASSERT_EQUAL_STRING(guard_animations[animation], guard->animations[animation].name);
        TEST_ASSERT_EQUAL_size_t(guard_poses[animation], guard->animations[animation].pose_count);
    }
    /* Poses play in drawing order. */
    TEST_ASSERT_EQUAL_INT(sprite_index("SPR_GRD_W2_1"), catalog->assets[guard->animations[1].assets[1]].first_sprite);
    TEST_ASSERT_EQUAL_INT(sprite_index("SPR_GRD_PAIN_2"), catalog->assets[guard->animations[2].assets[1]].first_sprite);
    TEST_ASSERT_EQUAL_INT(guard->animations[0].assets[0], guard->reference_asset);

    /* The mutant's fourth death frame comes after its second pain frame in the enum. */
    mutant = &catalog->subjects[wolf_asset_catalog_find_subject(catalog, "mut")];
    TEST_ASSERT_EQUAL_STRING("mut_die", mutant->animations[3].name);
    TEST_ASSERT_EQUAL_size_t(4u, mutant->animations[3].pose_count);

    /* All four VOX subjects use <subject>_ready and <subject>_atk scene-object names. */
    {
        static const char *const weapon_subjects[] = {"knife", "pistol", "machinegun", "chain"};
        for (size_t weapon = 0u; weapon < 4u; ++weapon) {
            int weapon_index = wolf_asset_catalog_find_subject(catalog, weapon_subjects[weapon]);
            const WolfSubject *weapon_subject;

            TEST_ASSERT_TRUE(weapon_index >= 0);
            weapon_subject = &catalog->subjects[weapon_index];
            TEST_ASSERT_EQUAL_size_t(2u, weapon_subject->animation_count);
            {
                char expected_ready[64];
                char expected_attack[64];

                (void)snprintf(expected_ready, sizeof(expected_ready), "%s_ready", weapon_subjects[weapon]);
                (void)snprintf(expected_attack, sizeof(expected_attack), "%s_atk", weapon_subjects[weapon]);
                TEST_ASSERT_EQUAL_STRING(expected_ready, weapon_subject->animations[0].name);
                TEST_ASSERT_EQUAL_STRING(expected_attack, weapon_subject->animations[1].name);
            }
            TEST_ASSERT_EQUAL_size_t(1u, weapon_subject->animations[0].pose_count);
            TEST_ASSERT_EQUAL_size_t(4u, weapon_subject->animations[1].pose_count);
        }
    }

    /* Each static object is its own subject, and projectiles keep a single animation. */
    TEST_ASSERT_EQUAL_STRING("stat_7",
        catalog->subjects[wolf_asset_catalog_find_subject(catalog, "stat_7")].animations[0].name);
    TEST_ASSERT_EQUAL_size_t(4u, catalog->subjects[wolf_asset_catalog_find_subject(catalog, "hypo")].animations[0].pose_count);
    TEST_ASSERT_EQUAL_size_t(77u, catalog->subject_count);
    free(catalog);
}

void test_catalog_assigns_reconstruction_methods(void)
{
    WolfAssetCatalog *catalog = malloc(sizeof(WolfAssetCatalog));

    wolf_asset_catalog_build(catalog);
    TEST_ASSERT_EQUAL_STRING("open green barrel", asset_for(catalog, "SPR_STAT_1")->description);
    TEST_ASSERT_TRUE(asset_for(catalog, "SPR_STAT_1")->single_view.hollow_depth > 0);
    TEST_ASSERT_TRUE(asset_for(catalog, "SPR_STAT_1")->trim_shadow_fringe);
    TEST_ASSERT_TRUE(asset_for(catalog, "SPR_STAT_30")->single_view.hollow_depth > 0);
    TEST_ASSERT_TRUE(asset_for(catalog, "SPR_STAT_11")->single_view.rough_surface);
    TEST_ASSERT_TRUE(asset_for(catalog, "SPR_STAT_3")->remove_light_pool);
    TEST_ASSERT_TRUE(asset_for(catalog, "SPR_STAT_4")->remove_light_pool);
    TEST_ASSERT_TRUE(asset_for(catalog, "SPR_STAT_14")->remove_light_pool);
    TEST_ASSERT_FALSE(asset_for(catalog, "SPR_STAT_7")->remove_light_pool);
    TEST_ASSERT_EQUAL_INT(WOLF_HAND_BUILT_PUDDLE, asset_for(catalog, "SPR_STAT_0")->hand_built);
    TEST_ASSERT_EQUAL_INT(WOLF_HAND_BUILT_PUDDLE, asset_for(catalog, "SPR_STAT_38")->hand_built);
    TEST_ASSERT_EQUAL_INT(WOLF_HAND_BUILT_BONE_PILE, asset_for(catalog, "SPR_STAT_19")->hand_built);
    TEST_ASSERT_EQUAL_INT(WOLF_HAND_BUILT_TREASURE_CHEST, asset_for(catalog, "SPR_STAT_31")->hand_built);
    TEST_ASSERT_EQUAL_INT(WOLF_HAND_BUILT_FLAG, asset_for(catalog, "SPR_STAT_39")->hand_built);
    TEST_ASSERT_EQUAL_INT(WOLF_DEPTH_PROFILE_LATHE, asset_for(catalog, "SPR_STAT_1")->single_view.profile);
    TEST_ASSERT_EQUAL_INT(WOLF_ASSET_HAND_BUILT, asset_for(catalog, "SPR_STAT_2")->method);
    TEST_ASSERT_EQUAL_INT(WOLF_HAND_BUILT_TABLE_AND_CHAIRS, asset_for(catalog, "SPR_STAT_2")->hand_built);
    TEST_ASSERT_EQUAL_INT(WOLF_HAND_BUILT_BED, asset_for(catalog, "SPR_STAT_22")->hand_built);
    TEST_ASSERT_EQUAL_INT(WOLF_DEPTH_PROFILE_EXTRUDE, asset_for(catalog, "SPR_STAT_20")->single_view.profile);
    TEST_ASSERT_EQUAL_INT(WOLF_DEPTH_PROFILE_CAGE, asset_for(catalog, "SPR_STAT_17")->single_view.profile);
    TEST_ASSERT_EQUAL_INT(WOLF_DEPTH_PROFILE_CAGE, asset_for(catalog, "SPR_STAT_18")->single_view.profile);
    TEST_ASSERT_EQUAL_INT(WOLF_HAND_BUILT_FOOD, asset_for(catalog, "SPR_STAT_24")->hand_built);
    TEST_ASSERT_EQUAL_INT(WOLF_HAND_BUILT_FIRST_AID, asset_for(catalog, "SPR_STAT_25")->hand_built);
    TEST_ASSERT_EQUAL_INT(WOLF_HAND_BUILT_GIBS, asset_for(catalog, "SPR_STAT_34")->hand_built);
    TEST_ASSERT_EQUAL_INT(WOLF_HAND_BUILT_WELL, asset_for(catalog, "SPR_STAT_36")->hand_built);
    TEST_ASSERT_EQUAL_INT(WOLF_HAND_BUILT_WELL, asset_for(catalog, "SPR_STAT_37")->hand_built);
    /* Actors drawn only from the front get one face; objects keep their drawing all through. */
    TEST_ASSERT_TRUE(asset_for(catalog, "SPR_BOSS_W1")->single_view.plain_back);
    TEST_ASSERT_TRUE(asset_for(catalog, "SPR_GRD_DIE_1")->single_view.plain_back);
    TEST_ASSERT_TRUE(asset_for(catalog, "SPR_BLINKY_W1")->single_view.plain_back);
    TEST_ASSERT_TRUE(asset_for(catalog, "SPR_BLINKY_W1")->single_view.solid_back);
    TEST_ASSERT_FALSE(asset_for(catalog, "SPR_BOSS_W1")->single_view.solid_back);
    TEST_ASSERT_FALSE(asset_for(catalog, "SPR_STAT_1")->single_view.plain_back);
    TEST_ASSERT_FALSE(asset_for(catalog, "SPR_SMOKE_1")->single_view.plain_back);

    /* Upright camera-facing frames are built on the actor's body; falling ones are inflated. */
    TEST_ASSERT_EQUAL_INT(WOLF_ASSET_POSE_TRANSFER, asset_for(catalog, "SPR_GRD_PAIN_1")->method);
    TEST_ASSERT_EQUAL_INT(WOLF_ASSET_POSE_TRANSFER, asset_for(catalog, "SPR_SS_SHOOT2")->method);
    TEST_ASSERT_EQUAL_INT(WOLF_ASSET_SINGLE_VIEW, asset_for(catalog, "SPR_GRD_DIE_2")->method);
    TEST_ASSERT_TRUE(asset_for(catalog, "SPR_GRD_DIE_2")->depth_from_subject);
    /* Bosses have no rotation frames to borrow from. */
    TEST_ASSERT_EQUAL_INT(WOLF_ASSET_SINGLE_VIEW, asset_for(catalog, "SPR_BOSS_SHOOT1")->method);
    TEST_ASSERT_FALSE(asset_for(catalog, "SPR_BOSS_SHOOT1")->depth_from_subject);

    /* Weapons recede into the screen and keep their dark gun metal. */
    TEST_ASSERT_EQUAL_INT(WOLF_DEPTH_PROFILE_FIRST_PERSON, asset_for(catalog, "SPR_PISTOLATK2")->single_view.profile);
    TEST_ASSERT_FALSE(asset_for(catalog, "SPR_PISTOLATK2")->strip_shadow);
    TEST_ASSERT_TRUE(asset_for(catalog, "SPR_STAT_7")->strip_shadow);
    TEST_ASSERT_FALSE(asset_for(catalog, "SPR_DEMO")->strip_shadow);
    free(catalog);
}

/* Timed sequences follow the original next pointers, tics and loops. */
void test_animation_catalog_rebuilds_original_sequences(void)
{
    WolfAnimation *sequences = malloc(sizeof(WolfAnimation) * WOLF_ANIMATION_CAPACITY);
    size_t count = wolf_animation_catalog_build(sequences, WOLF_ANIMATION_CAPACITY);
    const WolfAnimation *chase = find_sequence(sequences, count, "grdchase");
    const WolfAnimation *die = find_sequence(sequences, count, "grddie");
    const WolfAnimation *shoot = find_sequence(sequences, count, "grdshoot");
    const WolfAnimation *pain = find_sequence(sequences, count, "grdpain");
    const WolfAnimation *pain_variant = find_sequence(sequences, count, "grdpain1");
    const WolfAnimation *pistol = find_sequence(sequences, count, "player_pistol_attack");
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

    TEST_ASSERT_NOT_NULL(pain);
    TEST_ASSERT_NOT_NULL(pain_variant);
    TEST_ASSERT_EQUAL_INT(sprite_index("SPR_GRD_PAIN_2"), pain_variant->frames[0].sprite_index);

    TEST_ASSERT_NOT_NULL(pistol);
    TEST_ASSERT_EQUAL_size_t(4u, pistol->frame_count);
    TEST_ASSERT_EQUAL_INT(6, pistol->frames[0].tics);
    TEST_ASSERT_EQUAL_INT(sprite_index("SPR_PISTOLATK1"), pistol->frames[0].sprite_index);
    TEST_ASSERT_EQUAL_STRING("player_pistol_ready", pistol->next_animation);

    for (size_t first = 0u; first < count; ++first) {
        for (size_t second = first + 1u; second < count; ++second) {
            TEST_ASSERT_TRUE(strcmp(sequences[first].name, sequences[second].name) != 0);
        }
    }
    free(sequences);
}
