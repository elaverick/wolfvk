#ifndef WOLF_SPRITE_VOXELIZER_VALIDATION_H
#define WOLF_SPRITE_VOXELIZER_VALIDATION_H

#include "sprite_image.h"
#include "voxel_model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Agreement between an original sprite frame and the voxel model rendered
 * from the same direction. Silhouette agreement and colour agreement are
 * measured separately because carving errors and colouring errors have
 * different causes.
 */
typedef struct WolfViewMetrics {
    size_t original_pixels;
    size_t rendered_pixels;
    /* Pixels drawn in both images. */
    size_t shared_pixels;
    /* Pixels drawn in either image. */
    size_t union_pixels;
    /* Shared pixels with exactly the original palette index. */
    size_t exact_colour_pixels;
    /* Shared pixels whose colour appears in the original within one pixel. */
    size_t near_colour_pixels;
    /* Shared / union: 1.0 means identical outlines. */
    double silhouette_iou;
    /* Exact / shared: 1.0 means every overlapping pixel has the original colour. */
    double colour_accuracy;
    /*
     * Near / shared: tolerates a detail drawn one pixel away, which is how
     * the hand-drawn rotation frames differ from any single solid.
     */
    double near_colour_accuracy;
    /* Mean absolute RGB difference per channel over shared pixels, 0-255. */
    double mean_colour_error;
} WolfViewMetrics;

void wolf_compare_views(
    const WolfSpriteImage *original,
    const WolfSpriteImage *rendered,
    const uint8_t *palette_rgba,
    WolfViewMetrics *metrics);

/*
 * Upper bound on the mean silhouette IoU that ANY solid can achieve against
 * an eight-way rotation set. Views k and k+4 look at a solid from opposite
 * sides, so a solid's silhouettes in those views are exact mirror images.
 * Jaccard distance (1 - IoU) obeys the triangle inequality, so for a pair
 * whose drawn silhouettes mirror each other with IoU m, the two views'
 * IoUs against any one solid sum to at most 1 + m. Averaging (1 + m) / 2
 * over the four opposite pairs bounds the achievable mean IoU.
 *
 * m is taken at the best relative sideways shift up to max_relative_shift
 * columns, matching the freedom frame registration has. The pair function
 * returns (1 + m) / 2 for one pair: the best mean IoU the two views can
 * reach together, and so the most either can be expected to reach.
 */
double wolf_opposite_pair_ceiling(
    const WolfSpriteImage *view,
    const WolfSpriteImage *opposite_view,
    int max_relative_shift);

double wolf_rotation_set_silhouette_ceiling(
    const WolfSpriteImage *const *views,
    int max_relative_shift);

/* Expand an indexed sprite to RGBA with transparent background. */
void wolf_sprite_to_rgba(const WolfSpriteImage *image, const uint8_t *palette_rgba, uint8_t *rgba);

/* Pixel scale of the per-view cells in a comparison sheet. */
#define WOLF_SHEET_CELL_SCALE 3
#define WOLF_SHEET_CELL_SIZE (WOLF_SPRITE_SIZE * WOLF_SHEET_CELL_SCALE)
#define WOLF_SHEET_MARGIN 6

typedef struct WolfComparisonSheet {
    uint8_t *rgba;
    uint32_t width;
    uint32_t height;
} WolfComparisonSheet;

/*
 * Lay out a visual comparison: one column per original view with the
 * original frame (as drawn) on top, the voxel model rendered from that
 * rotation in the middle and a difference map below, measured against the
 * shadow-free frame: grey = match, yellow = different colour, red = missing
 * from the model, blue = extra in the model, purple = floor shadow removed
 * before voxelising. Two shaded three-quarter previews follow on the right.
 * targets and shadow_masks may be NULL when nothing was removed.
 */
bool wolf_comparison_sheet_build(
    const WolfSpriteImage *const *originals,
    const WolfSpriteImage *const *targets,
    const uint8_t *const *shadow_masks,
    const WolfSpriteImage *rendered,
    size_t view_count,
    const WolfVoxelModel *model,
    const uint8_t *palette_rgba,
    WolfComparisonSheet *sheet);

void wolf_comparison_sheet_destroy(WolfComparisonSheet *sheet);

#endif
