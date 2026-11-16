#ifndef WOLFVK_TEST_VOX_BUILDER_H
#define WOLFVK_TEST_VOX_BUILDER_H

#include "wolfvk/vox_file.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* One named animation for a synthetic file; pose p is drawn by pose_voxels[p]. */
typedef struct TestVoxAnimation {
    const char *name;
    uint32_t pose_count;
    const WolfVoxVoxel *const *pose_voxels;
    const uint32_t *pose_voxel_counts;
} TestVoxAnimation;

typedef struct TestVoxBytes {
    uint8_t *data;
    size_t size;
    size_t capacity;
} TestVoxBytes;

/*
 * Build a MagicaVoxel file laid out as tools/sprite_voxelizer writes one:
 * a SIZE/XYZI pair per pose, then a scene graph in which every animation is
 * a named transform over a shape whose models carry "_f" keyframes, then
 * the palette. Each test palette entry n is (n, 255 - n, 7, 255).
 */
bool test_vox_build(
    const TestVoxAnimation *animations,
    uint32_t animation_count,
    uint32_t model_size,
    bool include_palette,
    TestVoxBytes *bytes);

bool test_vox_write_file(
    const char *path,
    const TestVoxAnimation *animations,
    uint32_t animation_count);

void test_vox_bytes_free(TestVoxBytes *bytes);

#endif
