#include "unity.h"

#include "game_palette.h"
#include "test_support.h"
#include "vox_file.h"

#include <string.h>

static void assert_models_equal(const WolfVoxelModel *expected, const WolfVoxelModel *actual)
{
    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        TEST_ASSERT_EQUAL_UINT8(expected->occupied[voxel], actual->occupied[voxel]);
        if (expected->occupied[voxel]) {
            TEST_ASSERT_EQUAL_UINT8(expected->palette_index[voxel], actual->palette_index[voxel]);
        }
    }
}

/* Find a chunk identifier anywhere in the encoded file. */
static bool contains_chunk(const WolfByteBuffer *buffer, const char *identifier)
{
    for (size_t offset = 0u; offset + 4u <= buffer->size; ++offset) {
        if (memcmp(buffer->data + offset, identifier, 4u) == 0) {
            return true;
        }
    }
    return false;
}

void test_vox_single_model_round_trips_with_original_palette(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    const WolfVoxelModel *frames[1] = {model};
    WolfByteBuffer buffer = {0};
    WolfVoxDocument document;
    char error[128];

    test_make_cylinder_model(model, 7.0, 30);
    model->occupied[wolf_voxel_index(0, 63, 63)] = 1u;
    model->palette_index[wolf_voxel_index(0, 63, 63)] = 0u; /* palette entry 0 must survive as colour 1 */
    TEST_ASSERT_TRUE(wolf_vox_encode(frames, 1u, wolf_game_palette_rgba, &buffer));

    TEST_ASSERT_EQUAL_MEMORY("VOX ", buffer.data, 4u);
    TEST_ASSERT_EQUAL_UINT32(150u, wolf_read_u32_le(buffer.data + 4u));
    TEST_ASSERT_EQUAL_MEMORY("MAIN", buffer.data + 8u, 4u);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)buffer.size - 20u, wolf_read_u32_le(buffer.data + 16u));
    TEST_ASSERT_FALSE_MESSAGE(contains_chunk(&buffer, "nSHP"), "single models need no scene graph");

    TEST_ASSERT_TRUE(wolf_vox_decode(buffer.data, buffer.size, &document, error, sizeof(error)));
    TEST_ASSERT_EQUAL_size_t(1u, document.model_count);
    assert_models_equal(model, document.models[0]);
    /* RGBA entry n holds Wolfenstein palette entry n (displayed as colour n+1). */
    TEST_ASSERT_EQUAL_MEMORY(wolf_game_palette_rgba + 40u * 4u, document.palette_rgba + 40u * 4u, 3u);

    wolf_vox_document_destroy(&document);
    wolf_buffer_free(&buffer);
    wolf_voxel_model_destroy(model);
}

/* Several models become one animated object whose shape node lists keyframes in order. */
void test_vox_animation_records_keyframes(void)
{
    WolfVoxelModel *first = wolf_voxel_model_create();
    WolfVoxelModel *second = wolf_voxel_model_create();
    const WolfVoxelModel *frames[3] = {first, second, first};
    WolfByteBuffer buffer = {0};
    WolfVoxDocument document;
    char error[128];

    test_make_box_model(first, 3, 3, 10);
    test_make_cylinder_model(second, 5.0, 20);
    TEST_ASSERT_TRUE(wolf_vox_encode(frames, 3u, wolf_game_palette_rgba, &buffer));
    TEST_ASSERT_TRUE(contains_chunk(&buffer, "nTRN"));
    TEST_ASSERT_TRUE(contains_chunk(&buffer, "nGRP"));
    TEST_ASSERT_TRUE(contains_chunk(&buffer, "nSHP"));

    TEST_ASSERT_TRUE(wolf_vox_decode(buffer.data, buffer.size, &document, error, sizeof(error)));
    TEST_ASSERT_EQUAL_size_t(3u, document.model_count);
    for (int32_t frame = 0; frame < 3; ++frame) {
        TEST_ASSERT_EQUAL_INT32(frame, document.frame_indices[frame]);
    }
    assert_models_equal(second, document.models[1]);
    assert_models_equal(first, document.models[2]);
    wolf_vox_document_destroy(&document);
    wolf_buffer_free(&buffer);
    wolf_voxel_model_destroy(first);
    wolf_voxel_model_destroy(second);
}

/* Palette entry 255 has no MagicaVoxel slot and is stored as its nearest colour. */
void test_vox_colour_mapping_handles_the_last_palette_entry(void)
{
    uint8_t substitute;
    const uint8_t *original = wolf_game_palette_rgba + 255u * 4u;
    const uint8_t *chosen;

    TEST_ASSERT_EQUAL_UINT8(1u, wolf_vox_colour_for_palette_index(wolf_game_palette_rgba, 0u));
    TEST_ASSERT_EQUAL_UINT8(255u, wolf_vox_colour_for_palette_index(wolf_game_palette_rgba, 254u));
    substitute = wolf_vox_colour_for_palette_index(wolf_game_palette_rgba, 255u);
    TEST_ASSERT_TRUE(substitute >= 1u && substitute <= 255u);
    chosen = wolf_game_palette_rgba + (size_t)(substitute - 1u) * 4u;
    TEST_ASSERT_INT_WITHIN(64, original[0], chosen[0]);
    TEST_ASSERT_INT_WITHIN(64, original[2], chosen[2]);
}

void test_vox_decoder_rejects_malformed_files(void)
{
    WolfVoxelModel *model = wolf_voxel_model_create();
    const WolfVoxelModel *frames[1] = {model};
    WolfByteBuffer buffer = {0};
    WolfVoxDocument document;
    char error[128];
    uint8_t copy[64];

    test_make_box_model(model, 2, 2, 2);
    TEST_ASSERT_TRUE(wolf_vox_encode(frames, 1u, wolf_game_palette_rgba, &buffer));

    /* Wrong magic. */
    memcpy(copy, buffer.data, sizeof(copy));
    copy[0] = 'X';
    TEST_ASSERT_FALSE(wolf_vox_decode(copy, sizeof(copy), &document, error, sizeof(error)));

    /* MAIN claims more children than the file holds. */
    TEST_ASSERT_FALSE(wolf_vox_decode(buffer.data, buffer.size - 10u, &document, error, sizeof(error)));

    /* A model larger than the 64-voxel tile volume. */
    wolf_write_u32_le(buffer.data + 20u + 12u, 65u);
    TEST_ASSERT_FALSE(wolf_vox_decode(buffer.data, buffer.size, &document, error, sizeof(error)));

    TEST_ASSERT_FALSE(wolf_vox_encode(frames, 0u, wolf_game_palette_rgba, &buffer));
    TEST_ASSERT_FALSE(wolf_vox_read_file("missing-file.vox", &document, error, sizeof(error)));
    wolf_buffer_free(&buffer);
    wolf_voxel_model_destroy(model);
}
