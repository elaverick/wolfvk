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

/*
 * Build a complete WL6-shaped VSWAP: each rotation set holds the eight true
 * views of a small solid, a few objects are drawn, and the rest are blank.
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

            if (asset->kind == WOLF_ASSET_ROTATION_SET) {
                wolf_voxel_render_view(solid, wolf_rotation_degrees(catalog->rotation_for_sprite[sprite]), 0,
                    &sprites[sprite], NULL);
            } else if (sprite % 9 == 0) {
                test_fill_rectangle(&sprites[sprite], 24, 30, 39, 63, (uint8_t)(sprite % 250));
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

/* The whole tool, end to end, on original-format data: every output written and validated. */
void test_pipeline_writes_and_validates_every_output(void)
{
    WolfPipelineOptions options;
    WolfVoxDocument document;
    char error[128];

    TEST_ASSERT_TRUE(wolf_make_directories(PIPELINE_TEST_DIRECTORY));
    TEST_ASSERT_TRUE(build_synthetic_wl6_vswap(PIPELINE_TEST_DIRECTORY "/VSWAP.WL6"));

    options.vswap_path = PIPELINE_TEST_DIRECTORY "/VSWAP.WL6";
    options.output_directory = PIPELINE_TEST_DIRECTORY "/output";
    options.multi_view = wolf_multi_view_default_options();
    options.write_comparison_sheets = true;
    options.only_asset = NULL;
    TEST_ASSERT_EQUAL_INT(0, wolf_pipeline_run(&options));

    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/output/manifest.json"));
    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/output/sprites/050_grd_s_1.png"));
    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/output/validation/report.html"));
    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/output/validation/metrics.csv"));
    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/output/validation/grd_s.png"));
    TEST_ASSERT_TRUE(file_exists(PIPELINE_TEST_DIRECTORY "/output/vox/models/stat_1.vox"));

    TEST_ASSERT_TRUE(wolf_vox_read_file(PIPELINE_TEST_DIRECTORY "/output/vox/models/grd_s.vox",
        &document, error, sizeof(error)));
    TEST_ASSERT_EQUAL_size_t(1u, document.model_count);
    TEST_ASSERT_TRUE(wolf_voxel_model_count(document.models[0]) > 1000u);
    wolf_vox_document_destroy(&document);

    TEST_ASSERT_TRUE(wolf_vox_read_file(PIPELINE_TEST_DIRECTORY "/output/vox/animations/grdchase.vox",
        &document, error, sizeof(error)));
    TEST_ASSERT_EQUAL_size_t(6u, document.model_count);
    TEST_ASSERT_EQUAL_INT32(5, document.frame_indices[5]);
    wolf_vox_document_destroy(&document);
}

/* Data from another game variant has a different sprite layout and must be refused. */
void test_pipeline_refuses_non_wl6_sprite_counts(void)
{
    WolfSpriteImage sprite;
    const WolfSpriteImage *sprites[1] = {&sprite};
    WolfByteBuffer file = {0};
    WolfPipelineOptions options;

    test_make_patterned_sprite(&sprite, 1u);
    TEST_ASSERT_TRUE(wolf_make_directories(PIPELINE_TEST_DIRECTORY));
    TEST_ASSERT_TRUE(test_build_vswap(sprites, 1u, 2u, &file));
    TEST_ASSERT_TRUE(wolf_write_entire_file(PIPELINE_TEST_DIRECTORY "/SMALL.WL6", file.data, file.size));
    wolf_buffer_free(&file);

    options.vswap_path = PIPELINE_TEST_DIRECTORY "/SMALL.WL6";
    options.output_directory = PIPELINE_TEST_DIRECTORY "/rejected";
    options.multi_view = wolf_multi_view_default_options();
    options.write_comparison_sheets = false;
    options.only_asset = NULL;
    TEST_ASSERT_NOT_EQUAL(0, wolf_pipeline_run(&options));

    options.vswap_path = PIPELINE_TEST_DIRECTORY "/missing.WL6";
    TEST_ASSERT_NOT_EQUAL(0, wolf_pipeline_run(&options));
}
