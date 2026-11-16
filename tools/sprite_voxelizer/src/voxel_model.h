#ifndef WOLF_SPRITE_VOXELIZER_VOXEL_MODEL_H
#define WOLF_SPRITE_VOXELIZER_VOXEL_MODEL_H

#include "sprite_image.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * A sprite becomes a 64x64x64 volume: one voxel per original sprite pixel,
 * so a model occupies exactly one map tile and keeps the original pixel
 * scale. Axes follow MagicaVoxel: x runs to the right of the front view,
 * y runs away from the front viewer, and z is up from the floor.
 *
 * The actor's facing direction is -y, so the original rotation frame 1 is
 * the view from -y. Each voxel stores an index into the original
 * Wolfenstein VGA palette so no colour is ever approximated.
 */
#define WOLF_VOXEL_DIM 64
#define WOLF_VOXEL_COUNT (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM * WOLF_VOXEL_DIM)

typedef struct WolfVoxelModel {
    uint8_t occupied[WOLF_VOXEL_COUNT];
    uint8_t palette_index[WOLF_VOXEL_COUNT];
} WolfVoxelModel;

static inline size_t wolf_voxel_index(int x, int y, int z)
{
    return ((size_t)z * WOLF_VOXEL_DIM + (size_t)y) * WOLF_VOXEL_DIM + (size_t)x;
}

static inline bool wolf_voxel_in_bounds(int x, int y, int z)
{
    return x >= 0 && y >= 0 && z >= 0 &&
        x < WOLF_VOXEL_DIM && y < WOLF_VOXEL_DIM && z < WOLF_VOXEL_DIM;
}

/* Allocate an empty model; the structure is large, so it always lives on the heap. */
WolfVoxelModel *wolf_voxel_model_create(void);
void wolf_voxel_model_destroy(WolfVoxelModel *model);
void wolf_voxel_model_clear(WolfVoxelModel *model);

size_t wolf_voxel_model_count(const WolfVoxelModel *model);

/* True when the voxel is occupied and at least one face touches empty space. */
bool wolf_voxel_is_surface(const WolfVoxelModel *model, int x, int y, int z);

/* Sprite canvas row that a voxel layer appears on; the floor is row 63. */
static inline int wolf_voxel_row_for_layer(int z)
{
    return WOLF_SPRITE_SIZE - 1 - z;
}

/*
 * Orthographic camera for an original rotation. A rotation of 45*k degrees
 * places the viewer k eighths of a turn anticlockwise (seen from above)
 * from the actor's facing direction, which is exactly how CalcRotate in
 * WL_DRAW.C selects frame k+1 of an eight-way set.
 */
typedef struct WolfViewBasis {
    double right_x;
    double right_y;
    double toward_viewer_x;
    double toward_viewer_y;
    /* Half the horizontal width of a unit cube's projection. */
    double half_footprint;
} WolfViewBasis;

WolfViewBasis wolf_view_basis(double rotation_degrees);

/* Horizontal voxel-centre coordinates relative to the actor's vertical axis. */
static inline double wolf_voxel_center_offset(int coordinate)
{
    return (double)coordinate - (double)(WOLF_VOXEL_DIM / 2) + 0.5;
}

/*
 * Render the model as the original engine would draw a sprite: orthographic,
 * one canvas pixel per voxel, viewed horizontally from the given rotation.
 * Each voxel is treated as a cube, so at diagonal angles it covers its true
 * projected width. Optionally records which voxel supplied each pixel.
 */
void wolf_voxel_render_view(
    const WolfVoxelModel *model,
    double rotation_degrees,
    WolfSpriteImage *image,
    int32_t *voxel_for_pixel);

/*
 * Render a shaded three-quarter preview for human inspection. The output is
 * RGBA, size x size pixels; yaw uses the same convention as the views and
 * pitch is the camera elevation in degrees.
 */
void wolf_voxel_render_preview(
    const WolfVoxelModel *model,
    const uint8_t *palette_rgba,
    double yaw_degrees,
    double pitch_degrees,
    int size,
    uint8_t *rgba);

#endif
