#include "unity.h"

#include "asset_catalog.h"
#include "file_io.h"
#include "pipeline.h"
#include "test_support.h"
#include "vox_file.h"

#include <stdlib.h>
#include <string.h>

#define PIPELINE_TEST_DIRECTORY "sprite_voxelizer_pipeline_test"

static bool file_exists(const char *path)
{
    FILE *file = wolf_open_file(path, "rb");

    if (file == NULL) {
        return false;
    }
    fclose(file);
    return true;
}

static bool file_contains(const char *path, const char *text)
{
    uint8_t *data;
    size_t size;
    bool found = false;
    size_t length = strlen(text);

    if (!wolf_read_entire_file(path, &data, &size)) {
        return false;
    }
    for (size_t offset = 0u; offset + length <= size && !found; ++offset) {
        found = memcmp(data + offset, text, length) == 0;
    }
    free(data);
    return found;
}

/*
 * Build a complete WL6-shaped VSWAP: each rotation set holds the eight true
 * views of a small solid, a few objects are drawn (with a floor shadow on
 * every object), and the rest are blank.
 */
static bool build_synthetic_wl6_vswap(const char *path)
{
    WolfAssetCatalog *catalog = malloc(sizeof(WolfAssetCatalog));
    WolfSpriteImage *sprites = calloc(WOLF_WL6_SPRITE_COUNT, sizeof(WolfSpriteImage));
    const WolfSpriteImage **pointers = calloc(WOLF_WL6_SPRITE_COUNT, sizeof(WolfSpriteImage *));
    WolfVoxelModel *solid = wolf_voxel_model_create();
    WolfByteBuffer file = {0};
    bool built = false;

    if (catalog != NULL && sprites != NULL && pointers != NULL && solid != NULL) {
        wolf_asset_catalog_build(catalog);
        test_make_box_model(solid, 4, 7, 34);
        for (int sprite = 0; sprite < WOLF_WL6_SPRITE_COUNT; ++sprite) {
            const WolfVoxelAsset *asset = &catalog->assets[catalog->asset_for_sprite[sprite]];

            if (asset->method == WOLF_ASSET_ROTATION_SET || asset->method == WOLF_ASSET_AXIAL_REVOLVE) {
                wolf_voxel_render_view(solid, wolf_rotation_degrees(catalog->rotation_for_sprite[sprite]), 0,
                    &sprites[sprite], NULL);
            } else if (asset->method == WOLF_ASSET_HAND_BUILT && asset->hand_built == WOLF_HAND_BUILT_GIBS) {
                /* Gibs are built from their sprite: a pool of blood with a skull in it. */
                test_fill_rectangle(&sprites[sprite], 4, 58, 60, 63, 40u);
                test_fill_rectangle(&sprites[sprite], 12, 50, 19, 57, 16u);
            } else if (asset->method == WOLF_ASSET_HAND_BUILT && asset->hand_built == WOLF_HAND_BUILT_WELL) {
                /* Wells are built from their sprite: a brick wall under a light rim, dark inside. */
                test_fill_rectangle(&sprites[sprite], 16, 40, 47, 61, 24u);
                test_fill_rectangle(&sprites[sprite], 16, 40, 47, 42, 16u);
                test_fill_rectangle(&sprites[sprite], 20, 44, 43, 45, 8u);
            } else if (asset->method == WOLF_ASSET_HAND_BUILT && asset->hand_built == WOLF_HAND_BUILT_PUDDLE) {
                /* Puddles are laid out from their sprite: a flat smear of two shades. */
                test_fill_rectangle(&sprites[sprite], 8, 59, 55, 63, 148u);
                test_fill_rectangle(&sprites[sprite], 12, 61, 50, 61, 146u);
            } else if (asset->method == WOLF_ASSET_HAND_BUILT && asset->hand_built == WOLF_HAND_BUILT_FLAG) {
                /* The flag is turned from its sprite: pole, cloth and base. */
                test_fill_rectangle(&sprites[sprite], 31, 4, 33, 60, 210u);
                test_fill_rectangle(&sprites[sprite], 26, 14, 37, 50, 39u);
                test_fill_rectangle(&sprites[sprite], 24, 60, 40, 63, 73u);
            } else if (asset->method == WOLF_ASSET_HAND_BUILT) {
                /* The hand-built object's reference is its own front view here. */
                WolfVoxelModel *furniture = wolf_voxel_model_create();

                if (furniture != NULL && wolf_build_hand_model(asset->hand_built, NULL, furniture)) {
                    uint8_t used[256] = {0};
                    int column = 20;

                    wolf_voxel_render_view(furniture, 0.0, 0, &sprites[sprite], NULL);
                    /* A real sprite shows every colour the model uses; add the hidden ones along the top. */
                    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
                        if (furniture->occupied[voxel] && !used[furniture->palette_index[voxel]] && column < 44) {
                            used[furniture->palette_index[voxel]] = 1u;
                            test_fill_rectangle(&sprites[sprite], column, 0, column, 0, furniture->palette_index[voxel]);
                            ++column;
                        }
                    }
                }
                wolf_voxel_model_destroy(furniture);
            } else if (sprite % 9 == 0 || sprite >= WOLF_WL6_SPRITE_COUNT - 20) {
                test_fill_rectangle(&sprites[sprite], 24, 30, 39, 61, (uint8_t)(64 + sprite % 100));
                if (asset->strip_shadow) {
                    test_fill_rectangle(&sprites[sprite], 40, 60, 50, 61, 29u);
                }
            }
            pointers[sprite] = &sprites[sprite];
        }
        /* Exercise the sparse-page path once. */
        pointers[1] = NULL;
        built = test_build_vswap(pointers, WOLF_WL6_SPRITE_COUNT, 8u, &file) &&
            wolf_write_entire_file(path, file.data, file.size);
    }
    wolf_buffer_free(&file);
    wolf_voxel_model_destroy(solid);
    free(pointers);
    free(sprites);
    free(catalog);
    return built;
}

static WolfPipelineOptions test_options(const char *vswap, const char *output)
{
    WolfPipelineOptions options;

    options.vswap_path = vswap;
    options.output_directory = output;
    options.multi_view = wolf_multi_view_default_options();
    options.write_comparison_sheets = true;
    options.write_viewer_data = true;
    options.only_subject = NULL;
    options.assets_directory = NULL;
    return options;
}

/* The whole tool, end to end, on original-format data: every output written and validated. */
void test_pipeline_writes_and_validates_every_output(void)
{
    WolfPipelineOptions options = test_options(PIPELINE_TEST_DIRECTORY "/VSWAP.WL6", PIPELINE_TEST_DIRECTORY "/output");
    WolfVoxDocument document;
    char error[128];

    TEST_ASSERT_TRUE(wolf_make_directories(PIPELINE_TEST_DIRECTORY));
    TEST_ASSERT_TRUE(build_synthetic_wl6_vswap(PIPELINE_TEST_DIRECTORY "/VSWAP.WL6"));
    TEST_ASSERT_EQUAL_INT(0, wolf_pipeline_run(&options));

    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/output/manifest.json"));
    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/output/sprites/050_grd_s_1.png"));
    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/output/validation/report.html"));
    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/output/validation/metrics.csv"));
    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/output/validation/grd_w1.png"));
    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/output/vox/stat_9.vox"));
    TEST_ASSERT_TRUE(file_contains(PIPELINE_TEST_DIRECTORY "/output/viewer_data.js", "window.WOLF_VOXEL_DATA"));
    TEST_ASSERT_TRUE(file_contains(PIPELINE_TEST_DIRECTORY "/output/manifest.json", "\"sequences\""));

    /* One file per actor, every animation a named object with its poses as keyframes. */
    TEST_ASSERT_TRUE(wolf_vox_read_file(PIPELINE_TEST_DIRECTORY "/output/vox/grd.vox", &document, error, sizeof(error)));
    TEST_ASSERT_EQUAL_size_t(6u, document.object_count);
    TEST_ASSERT_EQUAL_STRING("grd_w", document.objects[1].name);
    TEST_ASSERT_EQUAL_size_t(4u, document.objects[1].frame_count);
    TEST_ASSERT_EQUAL_INT32(3, document.objects[1].frame_indices[3]);
    TEST_ASSERT_TRUE(wolf_voxel_model_count(document.models[document.objects[0].model_ids[0]]) > 1000u);
    wolf_vox_document_destroy(&document);
}

/* --only limits the output to one subject's file. */
void test_pipeline_builds_a_single_subject(void)
{
    WolfPipelineOptions options = test_options(PIPELINE_TEST_DIRECTORY "/VSWAP.WL6", PIPELINE_TEST_DIRECTORY "/only");

    options.only_subject = "stat_9";
    options.write_viewer_data = false;
    TEST_ASSERT_TRUE(wolf_make_directories(PIPELINE_TEST_DIRECTORY));
    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/VSWAP.WL6") ||
        build_synthetic_wl6_vswap(PIPELINE_TEST_DIRECTORY "/VSWAP.WL6"));
    TEST_ASSERT_EQUAL_INT(0, wolf_pipeline_run(&options));
    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/only/vox/stat_9.vox"));
    TEST_ASSERT_FALSE(file_exists(PIPELINE_TEST_DIRECTORY "/only/vox/grd.vox"));

    options.only_subject = "no_such_subject";
    TEST_ASSERT_NOT_EQUAL(0, wolf_pipeline_run(&options));
}

/* --assets copies the verified subject files, flat, where the game looks for <name>.vox. */
void test_pipeline_installs_subject_files_for_the_game(void)
{
    WolfPipelineOptions options = test_options(PIPELINE_TEST_DIRECTORY "/VSWAP.WL6", PIPELINE_TEST_DIRECTORY "/installed");
    WolfVoxDocument document;
    char error[128];

    options.only_subject = "stat_9";
    options.write_viewer_data = false;
    options.write_comparison_sheets = false;
    options.assets_directory = PIPELINE_TEST_DIRECTORY "/game/assets/voxels";
    TEST_ASSERT_TRUE(wolf_make_directories(PIPELINE_TEST_DIRECTORY));
    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/VSWAP.WL6") ||
        build_synthetic_wl6_vswap(PIPELINE_TEST_DIRECTORY "/VSWAP.WL6"));
    TEST_ASSERT_EQUAL_INT(0, wolf_pipeline_run(&options));

    TEST_ASSERT_TRUE(wolf_vox_read_file(
        PIPELINE_TEST_DIRECTORY "/game/assets/voxels/stat_9.vox", &document, error, sizeof(error)));
    TEST_ASSERT_EQUAL_size_t(1u, document.object_count);
    TEST_ASSERT_EQUAL_STRING("stat_9", document.objects[0].name);
    wolf_vox_document_destroy(&document);
    TEST_ASSERT_FALSE(file_exists(PIPELINE_TEST_DIRECTORY "/game/assets/voxels/grd.vox"));
}

/* Data from another game variant has a different sprite layout and must be refused. */
void test_pipeline_refuses_non_wl6_sprite_counts(void)
{
    WolfSpriteImage sprite;
    const WolfSpriteImage *sprites[1] = {&sprite};
    WolfByteBuffer file = {0};
    WolfPipelineOptions options = test_options(PIPELINE_TEST_DIRECTORY "/SMALL.WL6", PIPELINE_TEST_DIRECTORY "/rejected");

    test_make_patterned_sprite(&sprite, 1u);
    TEST_ASSERT_TRUE(wolf_make_directories(PIPELINE_TEST_DIRECTORY));
    TEST_ASSERT_TRUE(test_build_vswap(sprites, 1u, 2u, &file));
    TEST_ASSERT_TRUE(wolf_write_entire_file(PIPELINE_TEST_DIRECTORY "/SMALL.WL6", file.data, file.size));
    wolf_buffer_free(&file);

    options.write_comparison_sheets = false;
    TEST_ASSERT_NOT_EQUAL(0, wolf_pipeline_run(&options));
    options.vswap_path = PIPELINE_TEST_DIRECTORY "/missing.WL6";
    TEST_ASSERT_NOT_EQUAL(0, wolf_pipeline_run(&options));
}
