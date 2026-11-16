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
    /* Shared / union: 1.0 means identical outlines. */
    double silhouette_iou;
    /* Exact / shared: 1.0 means every overlapping pixel has the original colour. */
    double colour_accuracy;
    /* Mean absolute RGB difference per channel over shared pixels, 0-255. */
    double mean_colour_error;
} WolfViewMetrics;

void wolf_compare_views(
    const WolfSpriteImage *original,
    const WolfSpriteImage *rendered,
    const uint8_t *palette_rgba,
    WolfViewMetrics *metrics);

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
 * original frame on top, the voxel model rendered from that rotation in
 * the middle and a difference map below (grey = match, yellow = different
 * colour, red = missing from the model, blue = extra in the model). Two
 * shaded three-quarter previews of the model follow on the right.
 */
bool wolf_comparison_sheet_build(
    const WolfSpriteImage *const *originals,
    const WolfSpriteImage *rendered,
    size_t view_count,
    const WolfVoxelModel *model,
    const uint8_t *palette_rgba,
    WolfComparisonSheet *sheet);

void wolf_comparison_sheet_destroy(WolfComparisonSheet *sheet);

#endif
