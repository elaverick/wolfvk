#include "validation.h"

#include <stdlib.h>
#include <string.h>

/* Neutral backdrop that keeps both dark and light sprite colours readable. */
static const uint8_t sheet_background[4] = {38u, 40u, 52u, 255u};
static const uint8_t cell_background[4] = {62u, 66u, 82u, 255u};

void wolf_compare_views(
    const WolfSpriteImage *original,
    const WolfSpriteImage *rendered,
    const uint8_t *palette_rgba,
    WolfViewMetrics *metrics)
{
    double colour_error_sum = 0.0;

    memset(metrics, 0, sizeof(*metrics));
    for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        bool in_original = original->opaque[pixel] != 0u;
        bool in_rendered = rendered->opaque[pixel] != 0u;

        metrics->original_pixels += in_original ? 1u : 0u;
        metrics->rendered_pixels += in_rendered ? 1u : 0u;
        metrics->union_pixels += (in_original || in_rendered) ? 1u : 0u;
        if (in_original && in_rendered) {
            const uint8_t *expected = palette_rgba + (size_t)original->palette_index[pixel] * 4u;
            const uint8_t *actual = palette_rgba + (size_t)rendered->palette_index[pixel] * 4u;

            ++metrics->shared_pixels;
            if (original->palette_index[pixel] == rendered->palette_index[pixel]) {
                ++metrics->exact_colour_pixels;
            }
            for (int channel = 0; channel < 3; ++channel) {
                colour_error_sum += (double)abs((int)expected[channel] - (int)actual[channel]);
            }
        }
    }

    /* Two empty images agree perfectly. */
    metrics->silhouette_iou = metrics->union_pixels > 0u ?
        (double)metrics->shared_pixels / (double)metrics->union_pixels : 1.0;
    metrics->colour_accuracy = metrics->shared_pixels > 0u ?
        (double)metrics->exact_colour_pixels / (double)metrics->shared_pixels : 1.0;
    metrics->mean_colour_error = metrics->shared_pixels > 0u ?
        colour_error_sum / (3.0 * (double)metrics->shared_pixels) : 0.0;
}

void wolf_sprite_to_rgba(const WolfSpriteImage *image, const uint8_t *palette_rgba, uint8_t *rgba)
{
    for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        if (image->opaque[pixel]) {
            memcpy(rgba + pixel * 4u, palette_rgba + (size_t)image->palette_index[pixel] * 4u, 3u);
            rgba[pixel * 4u + 3u] = 255u;
        } else {
            memset(rgba + pixel * 4u, 0, 4u);
        }
    }
}

static void fill_rectangle(WolfComparisonSheet *sheet, uint32_t left, uint32_t top,
    uint32_t width, uint32_t height, const uint8_t *colour)
{
    for (uint32_t row = top; row < top + height && row < sheet->height; ++row) {
        for (uint32_t column = left; column < left + width && column < sheet->width; ++column) {
            memcpy(sheet->rgba + ((size_t)row * sheet->width + column) * 4u, colour, 4u);
        }
    }
}

/* Copy an RGBA image into the sheet at an integer scale, skipping transparent pixels. */
static void blit_scaled(WolfComparisonSheet *sheet, uint32_t left, uint32_t top,
    const uint8_t *rgba, uint32_t source_size, uint32_t scale)
{
    for (uint32_t row = 0u; row < source_size * scale; ++row) {
        for (uint32_t column = 0u; column < source_size * scale; ++column) {
            const uint8_t *source = rgba + ((size_t)(row / scale) * source_size + column / scale) * 4u;
            uint32_t sheet_row = top + row;
            uint32_t sheet_column = left + column;

            if (source[3] == 0u || sheet_row >= sheet->height || sheet_column >= sheet->width) {
                continue;
            }
            memcpy(sheet->rgba + ((size_t)sheet_row * sheet->width + sheet_column) * 4u, source, 4u);
        }
    }
}

/* Colour-code where the rendered model departs from the original frame. */
static void build_difference_image(const WolfSpriteImage *original, const WolfSpriteImage *rendered, uint8_t *rgba)
{
    static const uint8_t matching[4] = {150u, 150u, 150u, 255u};
    static const uint8_t wrong_colour[4] = {240u, 200u, 40u, 255u};
    static const uint8_t missing[4] = {230u, 50u, 50u, 255u};
    static const uint8_t extra[4] = {60u, 120u, 255u, 255u};

    for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        const uint8_t *colour = NULL;

        if (original->opaque[pixel] && rendered->opaque[pixel]) {
            colour = original->palette_index[pixel] == rendered->palette_index[pixel] ? matching : wrong_colour;
        } else if (original->opaque[pixel]) {
            colour = missing;
        } else if (rendered->opaque[pixel]) {
            colour = extra;
        }
        if (colour != NULL) {
            memcpy(rgba + pixel * 4u, colour, 4u);
        } else {
            memset(rgba + pixel * 4u, 0, 4u);
        }
    }
}

bool wolf_comparison_sheet_build(
    const WolfSpriteImage *const *originals,
    const WolfSpriteImage *rendered,
    size_t view_count,
    const WolfVoxelModel *model,
    const uint8_t *palette_rgba,
    WolfComparisonSheet *sheet)
{
    const uint32_t step = WOLF_SHEET_CELL_SIZE + WOLF_SHEET_MARGIN;
    const uint32_t preview_count = 2u;
    /* Previews span the full height of the three comparison rows. */
    const uint32_t preview_size = 3u * WOLF_SHEET_CELL_SIZE + 2u * WOLF_SHEET_MARGIN;
    uint8_t cell_rgba[WOLF_SPRITE_PIXELS * 4u];
    uint8_t *preview_rgba;
    uint32_t preview_left;

    memset(sheet, 0, sizeof(*sheet));
    sheet->width = WOLF_SHEET_MARGIN + (uint32_t)view_count * step + preview_count * (preview_size + WOLF_SHEET_MARGIN);
    sheet->height = WOLF_SHEET_MARGIN + 3u * step;
    sheet->rgba = malloc((size_t)sheet->width * sheet->height * 4u);
    preview_rgba = malloc((size_t)preview_size * preview_size * 4u);
    if (sheet->rgba == NULL || preview_rgba == NULL) {
        free(preview_rgba);
        wolf_comparison_sheet_destroy(sheet);
        return false;
    }
    fill_rectangle(sheet, 0u, 0u, sheet->width, sheet->height, sheet_background);

    for (size_t view = 0u; view < view_count; ++view) {
        uint32_t left = WOLF_SHEET_MARGIN + (uint32_t)view * step;

        for (uint32_t row = 0u; row < 3u; ++row) {
            fill_rectangle(sheet, left, WOLF_SHEET_MARGIN + row * step,
                WOLF_SHEET_CELL_SIZE, WOLF_SHEET_CELL_SIZE, cell_background);
        }
        wolf_sprite_to_rgba(originals[view], palette_rgba, cell_rgba);
        blit_scaled(sheet, left, WOLF_SHEET_MARGIN, cell_rgba, WOLF_SPRITE_SIZE, WOLF_SHEET_CELL_SCALE);
        wolf_sprite_to_rgba(&rendered[view], palette_rgba, cell_rgba);
        blit_scaled(sheet, left, WOLF_SHEET_MARGIN + step, cell_rgba, WOLF_SPRITE_SIZE, WOLF_SHEET_CELL_SCALE);
        build_difference_image(originals[view], &rendered[view], cell_rgba);
        blit_scaled(sheet, left, WOLF_SHEET_MARGIN + 2u * step, cell_rgba, WOLF_SPRITE_SIZE, WOLF_SHEET_CELL_SCALE);
    }

    /* Front-left and back-right three-quarter views show the reconstructed depth. */
    preview_left = WOLF_SHEET_MARGIN + (uint32_t)view_count * step;
    for (uint32_t preview = 0u; preview < preview_count; ++preview) {
        double yaw = preview == 0u ? -35.0 : 145.0;

        fill_rectangle(sheet, preview_left, WOLF_SHEET_MARGIN, preview_size, preview_size, cell_background);
        wolf_voxel_render_preview(model, palette_rgba, yaw, 25.0, (int)preview_size, preview_rgba);
        blit_scaled(sheet, preview_left, WOLF_SHEET_MARGIN, preview_rgba, preview_size, 1u);
        preview_left += preview_size + WOLF_SHEET_MARGIN;
    }
    free(preview_rgba);
    return true;
}

void wolf_comparison_sheet_destroy(WolfComparisonSheet *sheet)
{
    free(sheet->rgba);
    memset(sheet, 0, sizeof(*sheet));
}
