#ifndef WOLF_SPRITE_VOXELIZER_PIPELINE_H
#define WOLF_SPRITE_VOXELIZER_PIPELINE_H

#include "reconstruction.h"

#include <stdbool.h>

/*
 * Acceptance thresholds for rotation-set models. The face-on frames (front,
 * sides, back) define the solid and must be reproduced closely. The
 * diagonal frames were drawn independently of them and often contradict
 * them (opposite diagonals of a walk cycle mirror each other at only about
 * 0.56 IoU), so they are held to a looser bound that still rejects a model
 * that is wrong from those directions.
 */
#define WOLF_ROTATION_MIN_FACE_ON_IOU 0.85
#define WOLF_ROTATION_MIN_DIAGONAL_IOU 0.55
/* Share of face-on pixels whose colour the original draws within one pixel. */
#define WOLF_ROTATION_MIN_FACE_ON_NEAR_COLOUR 0.80

/* A hand-built model is checked only loosely against its reference sprite. */
#define WOLF_HAND_BUILT_MIN_FRONT_IOU 0.50

/* A first-person weapon must reach at least this far into the screen, in voxels. */
#define WOLF_FIRST_PERSON_MIN_DEPTH 24

typedef struct WolfPipelineOptions {
    const char *vswap_path;
    const char *output_directory;
    WolfMultiViewOptions multi_view;
    /* Write PNG comparison sheets and the HTML report. */
    bool write_comparison_sheets;
    /* Write viewer_data.js for the HTML viewer. */
    bool write_viewer_data;
    /* Restrict output to one subject (e.g. "grd"); NULL processes everything. */
    const char *only_subject;
} WolfPipelineOptions;

/*
 * Extract every sprite, remove painted floor shadows, reconstruct every pose
 * model, write one VOX file per subject holding all of its animations, and
 * validate the written files against the original frames.
 * Returns 0 when everything was written and passed validation.
 */
int wolf_pipeline_run(const WolfPipelineOptions *options);

#endif
