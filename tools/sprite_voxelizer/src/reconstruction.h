#ifndef WOLF_SPRITE_VOXELIZER_RECONSTRUCTION_H
#define WOLF_SPRITE_VOXELIZER_RECONSTRUCTION_H

#include "sprite_image.h"
#include "voxel_model.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * The hand-drawn rotation frames are not perfectly consistent projections of
 * one solid. These options control how strictly each frame's silhouette is
 * allowed to remove material from the shared model.
 */
typedef struct WolfMultiViewOptions {
    /* Number of views that may disagree before a voxel is carved away. */
    int allowed_silhouette_misses;
    /* Extra columns either side of a diagonal-view projection still counted as inside. */
    int diagonal_tolerance_columns;
    /* Connected fragments smaller than this many voxels are discarded as carving noise. */
    size_t minimum_fragment_voxels;
} WolfMultiViewOptions;

WolfMultiViewOptions wolf_multi_view_default_options(void);

/*
 * Rebuild one solid from several rotation frames by space carving (the
 * visual hull), then colour every visible surface voxel by a vote among the
 * frames that see it, weighted toward the frame that faces that surface.
 * Voxels no frame can see inherit the colour of the nearest seen voxel.
 */
bool wolf_reconstruct_multi_view(
    const WolfSpriteImage *const *views,
    const double *rotation_degrees,
    size_t view_count,
    const WolfMultiViewOptions *options,
    WolfVoxelModel *model);

/* How depth is invented for a sprite that was only ever drawn from the front. */
typedef enum WolfDepthProfile {
    /* Round every limb and blob by its own width, like inflating the outline. */
    WOLF_DEPTH_PROFILE_INFLATE,
    /* Turn each horizontal run into a disc, suiting barrels, lamps and pillars. */
    WOLF_DEPTH_PROFILE_LATHE,
    /* A constant thickness, suiting signs, flat treasure and floor clutter. */
    WOLF_DEPTH_PROFILE_EXTRUDE
} WolfDepthProfile;

typedef struct WolfSingleViewOptions {
    WolfDepthProfile profile;
    /* Multiplier from the profile's natural half-depth to voxels. */
    double depth_scale;
    /* Upper bound on half-depth in voxels, keeping flat items flat. */
    double maximum_half_depth;
    /* Half-depth used by the extrude profile. */
    double extrude_half_depth;
} WolfSingleViewOptions;

/*
 * Build a model from one camera-facing frame. Every voxel in a pixel's depth
 * column takes that pixel's colour, so the front view reproduces the sprite
 * exactly while the invented depth gives the object volume from other angles.
 */
void wolf_reconstruct_single_view(
    const WolfSpriteImage *image,
    const WolfSingleViewOptions *options,
    WolfVoxelModel *model);

/*
 * Compute each pixel's half-depth for a single-view profile. Exposed for
 * testing; transparent pixels receive zero.
 */
void wolf_single_view_half_depths(
    const WolfSpriteImage *image,
    const WolfSingleViewOptions *options,
    double *half_depth);

/*
 * Ratio of a model's front-to-back thickness to its side-to-side width,
 * measured layer by layer. Used to give single-view frames of an actor the
 * same body depth as that actor's multi-view reconstruction.
 */
double wolf_voxel_model_depth_ratio(const WolfVoxelModel *model);

#endif
