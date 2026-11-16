#ifndef WOLF_SPRITE_VOXELIZER_SPRITE_SHADOW_H
#define WOLF_SPRITE_VOXELIZER_SPRITE_SHADOW_H

#include "sprite_image.h"

#include <stddef.h>
#include <stdint.h>

/* Rows above the lowest drawn row in which a drop shadow may lie. */
#define WOLF_SHADOW_BAND_ROWS 8

/* True for the palette entries the artists used to paint floor shadows. */
bool wolf_palette_index_is_shadow(uint8_t palette_index);

/*
 * Remove the drop shadow painted on the floor beneath objects and actors.
 * Shadows are black (0), the darkest greys (26-31) and three dedicated
 * shadow greys (234-236). Objects use those colours too, so a pixel is only
 * shadow when, scanning up its column from the floor, an unbroken run of
 * shadow colours ends within the floor band; a dark run that continues up
 * into the object is the object's own shading. Each shadow patch must also
 * be at least as wide as it is tall, as a shadow lies flat on the floor.
 *
 * Writes the shadow-free image and, optionally, a mask of removed pixels.
 * Returns the number of pixels removed.
 */
size_t wolf_sprite_remove_floor_shadow(
    const WolfSpriteImage *source,
    WolfSpriteImage *shadow_free,
    uint8_t *removed_mask);

#endif
