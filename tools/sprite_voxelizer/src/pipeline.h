#ifndef WOLF_SPRITE_VOXELIZER_PIPELINE_H
#define WOLF_SPRITE_VOXELIZER_PIPELINE_H

#include "reconstruction.h"

#include <stdbool.h>

/*
 * Acceptance thresholds for rotation-set models. The face-on frames (front,
 * sides, back) define the solid and are reproduced closely. The diagonal
 * frames were drawn independently and often contradict the face-on ones
 * (opposite diagonals of a walk cycle mirror each other at only about 0.56
 * IoU, which no solid can reconcile), so they are held to a looser bound
 * that still rejects a model that is wrong from those directions.
 *
 * Colour is judged within one pixel because a voxel on the edge of the
 * body is seen by two frames that drew it differently; with one voxel per
 * pixel the edge columns of a narrow figure can only match one of them.
 */
#define WOLF_ROTATION_MIN_FACE_ON_IOU 0.80
/*
 * Where a face-on frame and its opposite frame disagree so much that no
 * solid can reach the bound above, the bound becomes this share of the
 * pair's own ceiling (wolf_opposite_pair_ceiling).
 */
#define WOLF_ROTATION_MIN_FRACTION_OF_PAIR_CEILING 0.95
#define WOLF_ROTATION_MIN_DIAGONAL_IOU 0.55
#define WOLF_ROTATION_MIN_FACE_ON_NEAR_COLOUR 0.60

/* A hand-built model is checked only loosely against its reference sprite. */
/*
 * Hand-built models reinterpret sprites drawn in perspective (a table's
 * chairs in three-quarter view, food seen from above), so their front view
 * cannot match the sprite pixel for pixel. They must keep the sprite's
 * width within this tolerance and take nearly all colours from its palette.
 */
#define WOLF_HAND_BUILT_WIDTH_TOLERANCE 0.25
#define WOLF_HAND_BUILT_MIN_PALETTE_SHARE 0.90

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
    /*
     * Also copy every verified subject file here, flat, for the game to load
     * (normally <game directory>/assets/voxels); NULL skips the copy.
     */
    const char *assets_directory;
} WolfPipelineOptions;

/*
 * Extract every sprite, remove painted floor shadows, reconstruct every pose
 * model, write one VOX file per subject holding all of its animations, and
 * validate the written files against the original frames.
 * Returns 0 when everything was written and passed validation.
 */
int wolf_pipeline_run(const WolfPipelineOptions *options);

#endif
