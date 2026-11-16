#include "unity.h"

#include "test_vox_builder.h"
#include "wolfvk/vox_file.h"

#include <stdio.h>
#include <string.h>

static const WolfVoxVoxel standing_pose[] = {{32u, 32u, 0u, 5u}, {32u, 32u, 1u, 6u}};
static const WolfVoxVoxel walking_pose_one[] = {{10u, 11u, 12u, 20u}};
static const WolfVoxVoxel walking_pose_two[] = {{1u, 2u, 3u, 21u}, {4u, 5u, 6u, 22u}, {7u, 8u, 9u, 23u}};

/* A guard-like file: a one-pose standing animation and a two-pose walk. */
static bool build_guard_file(TestVoxBytes *bytes, uint32_t model_size, bool include_palette)
{
    static const WolfVoxVoxel *const standing_poses[] = {standing_pose};
    static const uint32_t standing_counts[] = {2u};
    static const WolfVoxVoxel *const walking_poses[] = {walking_pose_one, walking_pose_two};
    static const uint32_t walking_counts[] = {1u, 3u};
    const TestVoxAnimation animations[] = {
        {"grd_s", 1u, standing_poses, standing_counts},
        {"grd_w", 2u, walking_poses, walking_counts},
    };

    return test_vox_build(animations, 2u, model_size, include_palette, bytes);
}

void test_vox_file_reads_named_animations_and_their_poses(void)
{
    TestVoxBytes bytes;
    WolfVoxFile file;
    const WolfVoxModel *model = NULL;
    char error_message[128];

    TEST_ASSERT_TRUE(build_guard_file(&bytes, 64u, true));
    TEST_ASSERT_TRUE_MESSAGE(
        wolf_vox_file_decode(bytes.data, bytes.size, &file, error_message, sizeof(error_message)),
        error_message);
    test_vox_bytes_free(&bytes);

    TEST_ASSERT_EQUAL_UINT32(3u, file.model_count);
    TEST_ASSERT_EQUAL_UINT32(2u, file.object_count);
    TEST_ASSERT_EQUAL_STRING("grd_s", file.objects[0].name);
    TEST_ASSERT_EQUAL_STRING("grd_w", file.objects[1].name);
    TEST_ASSERT_EQUAL_UINT32(2u, file.objects[1].pose_count);

    TEST_ASSERT_TRUE(wolf_vox_file_find_pose(&file, "grd_s", 0u, &model));
    TEST_ASSERT_EQUAL_UINT32(64u, model->size[0]);
    TEST_ASSERT_EQUAL_UINT32(2u, model->voxel_count);
    TEST_ASSERT_EQUAL_UINT8(32u, model->voxels[1].x);
    TEST_ASSERT_EQUAL_UINT8(1u, model->voxels[1].z);
    TEST_ASSERT_EQUAL_UINT8(6u, model->voxels[1].colour_index);

    /* Pose 1 of the walk is the file's third model, with its own voxels. */
    TEST_ASSERT_TRUE(wolf_vox_file_find_pose(&file, "grd_w", 1u, &model));
    TEST_ASSERT_EQUAL_UINT32(3u, model->voxel_count);
    TEST_ASSERT_EQUAL_UINT8(23u, model->voxels[2].colour_index);

    wolf_vox_file_destroy(&file);
}

void test_vox_file_keeps_the_stored_palette(void)
{
    TestVoxBytes bytes;
    WolfVoxFile file;

    TEST_ASSERT_TRUE(build_guard_file(&bytes, 64u, true));
    TEST_ASSERT_TRUE(wolf_vox_file_decode(bytes.data, bytes.size, &file, NULL, 0u));
    TEST_ASSERT_TRUE(file.has_palette);
    /* Colour c is palette entry c - 1; the builder stores entry n as (n, 255 - n, 7). */
    TEST_ASSERT_EQUAL_UINT8(4u, file.palette_rgba[4u * 4u]);
    TEST_ASSERT_EQUAL_UINT8(251u, file.palette_rgba[4u * 4u + 1u]);
    TEST_ASSERT_EQUAL_UINT8(7u, file.palette_rgba[4u * 4u + 2u]);
    wolf_vox_file_destroy(&file);
    test_vox_bytes_free(&bytes);

    TEST_ASSERT_TRUE(build_guard_file(&bytes, 64u, false));
    TEST_ASSERT_TRUE(wolf_vox_file_decode(bytes.data, bytes.size, &file, NULL, 0u));
    TEST_ASSERT_FALSE(file.has_palette);
    wolf_vox_file_destroy(&file);
    test_vox_bytes_free(&bytes);
}

void test_vox_file_reports_missing_animations_and_poses(void)
{
    TestVoxBytes bytes;
    WolfVoxFile file;
    const WolfVoxModel *model = NULL;

    TEST_ASSERT_TRUE(build_guard_file(&bytes, 64u, true));
    TEST_ASSERT_TRUE(wolf_vox_file_decode(bytes.data, bytes.size, &file, NULL, 0u));
    test_vox_bytes_free(&bytes);

    TEST_ASSERT_FALSE(wolf_vox_file_find_pose(&file, "grd_die", 0u, &model));
    TEST_ASSERT_FALSE(wolf_vox_file_find_pose(&file, "grd_w", 2u, &model));
    TEST_ASSERT_FALSE(wolf_vox_file_find_pose(&file, NULL, 0u, &model));
    TEST_ASSERT_FALSE(wolf_vox_file_find_pose(NULL, "grd_s", 0u, &model));
    wolf_vox_file_destroy(&file);
    /* Destroying twice must be harmless. */
    wolf_vox_file_destroy(&file);
}

void test_vox_file_rejects_models_larger_than_a_map_tile(void)
{
    TestVoxBytes bytes;
    WolfVoxFile file;
    char error_message[128] = {0};

    TEST_ASSERT_TRUE(build_guard_file(&bytes, 65u, true));
    TEST_ASSERT_FALSE(wolf_vox_file_decode(
        bytes.data, bytes.size, &file, error_message, sizeof(error_message)));
    TEST_ASSERT_TRUE(strstr(error_message, "64-voxel") != NULL);
    TEST_ASSERT_EQUAL_UINT32(0u, file.model_count);
    test_vox_bytes_free(&bytes);
}

void test_vox_file_rejects_truncated_and_foreign_data(void)
{
    static const uint8_t not_vox[] = "PNG not a voxel file at all";
    TestVoxBytes bytes;
    WolfVoxFile file;
    char error_message[128];

    TEST_ASSERT_FALSE(wolf_vox_file_decode(not_vox, sizeof(not_vox), &file, error_message, sizeof(error_message)));
    TEST_ASSERT_FALSE(wolf_vox_file_decode(NULL, 0u, &file, error_message, sizeof(error_message)));
    TEST_ASSERT_FALSE(wolf_vox_file_decode(not_vox, sizeof(not_vox), NULL, error_message, sizeof(error_message)));

    TEST_ASSERT_TRUE(build_guard_file(&bytes, 64u, true));
    /* Every shortened copy must be rejected or decoded safely, never read out of bounds. */
    for (size_t length = 0u; length < bytes.size; length += 7u) {
        if (wolf_vox_file_decode(bytes.data, length, &file, error_message, sizeof(error_message))) {
            wolf_vox_file_destroy(&file);
        }
    }
    /* Cutting the file inside its first voxel list removes every model. */
    TEST_ASSERT_FALSE(wolf_vox_file_decode(bytes.data, 60u, &file, error_message, sizeof(error_message)));
    test_vox_bytes_free(&bytes);
}

void test_vox_file_rejects_animations_that_name_missing_models(void)
{
    TestVoxBytes bytes;
    WolfVoxFile file;
    char error_message[128] = {0};
    size_t shape_offset = 0u;

    TEST_ASSERT_TRUE(build_guard_file(&bytes, 64u, true));
    for (size_t offset = 0u; offset + 4u <= bytes.size; ++offset) {
        if (memcmp(bytes.data + offset, "nSHP", 4u) == 0) {
            shape_offset = offset;
            break;
        }
    }
    TEST_ASSERT_TRUE(shape_offset > 0u);
    /* Header (12) + node id (4) + empty dictionary (4) + model count (4) reaches the first model id. */
    bytes.data[shape_offset + 24u] = 99u;
    TEST_ASSERT_FALSE(wolf_vox_file_decode(bytes.data, bytes.size, &file, error_message, sizeof(error_message)));
    TEST_ASSERT_TRUE(strstr(error_message, "missing model") != NULL);
    test_vox_bytes_free(&bytes);
}

void test_vox_file_loads_from_disk(void)
{
    static const WolfVoxVoxel *const poses[] = {standing_pose};
    static const uint32_t counts[] = {2u};
    const TestVoxAnimation animation = {"stat_3", 1u, poses, counts};
    const char *path = "wolfvk_test_stat_3.vox";
    WolfVoxFile file;
    char error_message[128];

    TEST_ASSERT_TRUE(test_vox_write_file(path, &animation, 1u));
    TEST_ASSERT_TRUE_MESSAGE(
        wolf_vox_file_load(path, &file, error_message, sizeof(error_message)), error_message);
    TEST_ASSERT_EQUAL_STRING("stat_3", file.objects[0].name);
    wolf_vox_file_destroy(&file);
    (void)remove(path);

    TEST_ASSERT_FALSE(wolf_vox_file_load("wolfvk_no_such_file.vox", &file, error_message, sizeof(error_message)));
    TEST_ASSERT_TRUE(strstr(error_message, "opened") != NULL);
}
