#ifndef WOLFVK_VOXEL_MESH_H
#define WOLFVK_VOXEL_MESH_H

#include "wolfvk/vox_file.h"
#include "wolfvk/vulkan_renderer.h"
#include "wolfvk/world.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * The voxelizer builds every model at one voxel per sprite pixel, and a
 * sprite is 64 pixels across one map tile, so 64 voxels span one tile.
 */
#define WOLF_VOXELS_PER_TILE 64u
#define WOLF_VOXEL_SIZE (WOLF_WORLD_TILE_SIZE / (float)WOLF_VOXELS_PER_TILE)

/*
 * Build the visible surface of a voxel model as flat-coloured quads in model
 * space, ready to be placed in the world by a model transform.
 *
 * Model space matches the camera's world axes: x east, y up, z south, with
 * the model's axis at the origin and its floor at y = 0. The model faces
 * negative z (north), which is the direction a yaw of zero looks. VOX files
 * put the actor's axis at the centre of the x/y footprint, make z up and
 * face the actor towards -y; this function performs that conversion.
 *
 * Only faces between a voxel and empty space are emitted, and neighbouring
 * faces of the same colour are merged into larger rectangles. Faces lying
 * on the floor or against the ceiling can never be seen and are left out.
 *
 * palette_rgba holds 256 sRGB entries where MagicaVoxel colour c is entry
 * c - 1. Colours are converted to linear space because the swapchain is
 * sRGB, and each face direction is shaded slightly differently so the
 * shape reads clearly without lighting. Callers own and must free *quads.
 */
bool wolf_voxel_mesh_build(
    const WolfVoxModel *model,
    const uint8_t palette_rgba[256u * 4u],
    VulkanQuad **quads,
    uint32_t *quad_count);

/* Linear-space brightness applied to faces pointing along each VOX axis direction. */
float wolf_voxel_mesh_face_shade(uint32_t axis, bool positive_direction);

#endif
