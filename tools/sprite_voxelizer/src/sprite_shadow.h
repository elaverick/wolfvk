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

/* Rows above the lowest drawn row in which a lamp's pool of light may lie. */
#define WOLF_LIGHT_POOL_BAND_ROWS 10

/*
 * Remove the pool of light painted on the floor under a lamp (the floor
 * lamp, chandelier and ceiling light). The pool is drawn in neutral greys,
 * touches the floor and is far wider than it is tall: at least three times.
 * The lamp's own coloured base is kept. Works in place and adds the removed
 * pixels to removed_mask (optional). Returns the number removed.
 */
size_t wolf_sprite_remove_light_pool(WolfSpriteImage *image, uint8_t *removed_mask);

/*
 * After wolf_sprite_remove_floor_shadow, remove the shadow-coloured pixels
 * left at the end of a floor-band row right beside the removed shadow (two
 * passes). Shadow removal works column by column, so where the shadow was
 * painted over the object's lowest pixels a dark fringe can remain on its
 * edge. Only for objects whose own outline isn't drawn in shadow colours
 * (the green barrel): the pillar shades its edge in those greys. Works in
 * place and needs the mask from the shadow removal. Returns the number
 * removed.
 */
size_t wolf_sprite_trim_shadow_fringe(WolfSpriteImage *image, uint8_t *removed_mask);

#endif
