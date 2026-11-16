#ifndef WOLF_SPRITE_VOXELIZER_SPRITE_IMAGE_H
#define WOLF_SPRITE_VOXELIZER_SPRITE_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Every Wolfenstein 3D sprite occupies a 64x64 virtual canvas. The actor's
 * position is the vertical line between columns 31 and 32, and the floor is
 * the bottom edge of row 63, so canvas coordinates map directly onto a
 * one-tile voxel volume.
 */
#define WOLF_SPRITE_SIZE 64
#define WOLF_SPRITE_PIXELS (WOLF_SPRITE_SIZE * WOLF_SPRITE_SIZE)

/*
 * A decoded sprite in its original indexed form. Transparency is explicit
 * because the original format omits transparent pixels instead of reserving
 * a palette index for them.
 */
typedef struct WolfSpriteImage {
    uint8_t palette_index[WOLF_SPRITE_PIXELS];
    uint8_t opaque[WOLF_SPRITE_PIXELS];
} WolfSpriteImage;

/* Row-major index for a sprite pixel; row 0 is the top of the canvas. */
static inline size_t wolf_sprite_pixel_index(int column, int row)
{
    return (size_t)row * WOLF_SPRITE_SIZE + (size_t)column;
}

/* Reset an image to the fully transparent state. */
void wolf_sprite_image_clear(WolfSpriteImage *image);

/* Count drawn pixels; an empty sprite has nothing to reconstruct. */
size_t wolf_sprite_image_opaque_count(const WolfSpriteImage *image);

/* Mirror an image horizontally, used to compare views that differ only by handedness. */
void wolf_sprite_image_mirror(const WolfSpriteImage *source, WolfSpriteImage *mirrored);

#endif
