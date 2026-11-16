#ifndef WOLF_SPRITE_VOXELIZER_TEST_SUPPORT_H
#define WOLF_SPRITE_VOXELIZER_TEST_SUPPORT_H

#include "file_io.h"
#include "sprite_image.h"
#include "voxel_model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Synthetic fixtures. The original game data is copyrighted and never
 * present in CI, so every test builds its own original-format input.
 */

/* Fill an inclusive canvas rectangle with one palette index. */
void test_fill_rectangle(WolfSpriteImage *image, int left, int top, int right, int bottom, uint8_t palette_index);

/* A deterministic pattern with gaps, multiple posts per column and varied colours. */
void test_make_patterned_sprite(WolfSpriteImage *image, unsigned seed);

/*
 * Solid upright box centred on the actor axis, standing on the floor.
 * Layer z is coloured palette index 16 + z / 4 so every frame agrees on colour.
 */
void test_make_box_model(WolfVoxelModel *model, int half_width, int half_depth, int height);

/* Upright cylinder of the given radius with the same height-banded colouring. */
void test_make_cylinder_model(WolfVoxelModel *model, double radius, int height);

/*
 * Build an original-layout VSWAP file holding the given sprites after
 * wall_page_count blank wall pages and before one sound page. A NULL
 * entry becomes a sparse page (offset 0), as the original archive allows.
 */
bool test_build_vswap(
    const WolfSpriteImage *const *sprites,
    size_t sprite_count,
    uint16_t wall_page_count,
    WolfByteBuffer *output);

/* Test-side zlib decoder (stored and fixed-Huffman blocks) used to verify the PNG encoder. */
bool test_zlib_decompress(const uint8_t *data, size_t size, uint8_t *output, size_t capacity, size_t *output_size);

#endif
