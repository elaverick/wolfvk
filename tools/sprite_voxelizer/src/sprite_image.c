#include "sprite_image.h"

#include <string.h>

void wolf_sprite_image_clear(WolfSpriteImage *image)
{
    memset(image->palette_index, 0, sizeof(image->palette_index));
    memset(image->opaque, 0, sizeof(image->opaque));
}

size_t wolf_sprite_image_opaque_count(const WolfSpriteImage *image)
{
    size_t count = 0u;

    for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        count += image->opaque[pixel] ? 1u : 0u;
    }
    return count;
}

void wolf_sprite_image_mirror(const WolfSpriteImage *source, WolfSpriteImage *mirrored)
{
    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t from = wolf_sprite_pixel_index(column, row);
            size_t to = wolf_sprite_pixel_index(WOLF_SPRITE_SIZE - 1 - column, row);

            mirrored->palette_index[to] = source->palette_index[from];
            mirrored->opaque[to] = source->opaque[from];
        }
    }
}
