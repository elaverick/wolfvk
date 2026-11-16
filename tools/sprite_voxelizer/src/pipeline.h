#ifndef WOLF_SPRITE_VOXELIZER_PIPELINE_H
#define WOLF_SPRITE_VOXELIZER_PIPELINE_H

#include "reconstruction.h"

#include <stdbool.h>

/*
 * Acceptance thresholds for rotation-set models. The hand-drawn frames are
 * not projections of one solid, so silhouettes are judged against the
 * ceiling the frames themselves allow (see
 * wolf_rotation_set_silhouette_ceiling). The model must reach this fraction
 * of the ceiling; the margin covers the resampling loss of drawing a
 * 64-voxel grid at 45 degrees, which matters most for tiny objects such as
 * the rocket. Single-view models must reproduce their frame exactly.
 */
#define WOLF_ROTATION_MIN_FRACTION_OF_CEILING 0.85
/* No rotation may fall below this, whatever the ceiling, to catch gross failures. */
#define WOLF_ROTATION_MIN_VIEW_SILHOUETTE_IOU 0.60
#define WOLF_ROTATION_MIN_MEAN_NEAR_COLOUR_ACCURACY 0.50
#define WOLF_ROTATION_MAX_MEAN_COLOUR_ERROR 40.0

typedef struct WolfPipelineOptions {
    const char *vswap_path;
    const char *output_directory;
    WolfMultiViewOptions multi_view;
    /* Write PNG comparison sheets and the HTML report. */
    bool write_comparison_sheets;
    /* Restrict model generation to one asset name; NULL processes everything. */
    const char *only_asset;
} WolfPipelineOptions;

/*
 * Extract every sprite, reconstruct and write all voxel models and
 * animations, then validate the written files against the originals.
 * Returns 0 when everything was written and passed validation.
 */
int wolf_pipeline_run(const WolfPipelineOptions *options);

#endif
