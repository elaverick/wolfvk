#ifndef WOLFVK_VOXEL_SCENE_H
#define WOLFVK_VOXEL_SCENE_H

#include "wolfvk/level.h"
#include "wolfvk/level_objects.h"
#include "wolfvk/vulkan_renderer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Voxel assets live in their own directory, one file per actor or object,
 * named after the subject (grd.vox, stat_12.vox ...). The engine looks for
 * this directory beside the original WL6 files, then beside the executable.
 */
#define WOLF_VOXEL_ASSET_DIRECTORY "assets/voxels"

/* One distinct pose a level needs, and where its mesh sits in the shared quad list. */
typedef struct WolfVoxelPoseMesh {
    char subject[WOLF_LEVEL_OBJECT_NAME_LENGTH];
    char animation[WOLF_LEVEL_OBJECT_NAME_LENGTH];
    uint8_t pose;
    /* False when the asset was missing or did not contain this pose; such objects are not drawn. */
    bool available;
    uint32_t first_quad;
    uint32_t quad_count;
} WolfVoxelPoseMesh;

/*
 * Meshes for every pose a level uses. Each pose is meshed once however many
 * objects share it, and all meshes are packed into one model-space quad list
 * for the renderer.
 */
typedef struct WolfVoxelScene {
    WolfVoxelPoseMesh *poses;
    uint32_t pose_count;
    VulkanQuad *quads;
    uint32_t quad_count;
    /* Poses that could not be loaded, with the first reason, so the caller can report them once. */
    uint32_t unavailable_pose_count;
    char first_problem[256];
} WolfVoxelScene;

/*
 * Load and mesh every distinct pose the objects need, reading each subject's
 * file from asset_directory once. Missing or damaged files do not fail the
 * load; their poses are marked unavailable so the level still runs.
 * Returns false only for invalid arguments or exhausted memory.
 */
bool wolf_voxel_scene_load(
    WolfVoxelScene *scene,
    const WolfLevelObject *objects,
    uint32_t object_count,
    const char *asset_directory);

/*
 * Place each object whose pose is available at the centre of its map tile,
 * on the floor, turned to its facing. Callers own and must free *instances.
 */
bool wolf_voxel_scene_build_instances(
    const WolfVoxelScene *scene,
    const WolfLevel *level,
    const WolfLevelObject *objects,
    uint32_t object_count,
    VulkanModelInstance **instances,
    uint32_t *instance_count);

void wolf_voxel_scene_destroy(WolfVoxelScene *scene);

#endif
