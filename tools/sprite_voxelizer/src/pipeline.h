#ifndef WOLF_SPRITE_VOXELIZER_PIPELINE_H
#define WOLF_SPRITE_VOXELIZER_PIPELINE_H

#include "reconstruction.h"

#include <stdbool.h>

/*
 * Acceptance thresholds for rotation-set models. A voxel solid can only
 * approximate eight hand-drawn frames, so these bound how far a rendered
 * rotation may depart from its original frame. Single-view models must
 * reproduce their frame exactly.
 */
#define WOLF_ROTATION_MIN_MEAN_SILHOUETTE_IOU 0.85
#define WOLF_ROTATION_MIN_VIEW_SILHOUETTE_IOU 0.75
#define WOLF_ROTATION_MIN_MEAN_COLOUR_ACCURACY 0.60

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
