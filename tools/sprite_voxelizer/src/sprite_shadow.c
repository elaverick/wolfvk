#include "sprite_shadow.h"

#include <string.h>

bool wolf_palette_index_is_shadow(uint8_t palette_index)
{
    return palette_index == 0u ||
        (palette_index >= 26u && palette_index <= 31u) ||
        (palette_index >= 234u && palette_index <= 236u);
}

/* Lowest drawn row of the whole sprite, or -1 for an empty sprite. */
static int lowest_drawn_row(const WolfSpriteImage *image)
{
    for (int row = WOLF_SPRITE_SIZE - 1; row >= 0; --row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            if (image->opaque[wolf_sprite_pixel_index(column, row)]) {
                return row;
            }
        }
    }
    return -1;
}

/*
 * Keep only candidate patches that lie flat: a 4-connected patch whose
 * bounding box is taller than it is wide is a dark detail standing up (a
 * robe hem between two legs), not a shadow.
 */
static void keep_flat_patches(uint8_t *candidates)
{
    uint8_t visited[WOLF_SPRITE_PIXELS];
    int stack[WOLF_SPRITE_PIXELS];
    int members[WOLF_SPRITE_PIXELS];

    memset(visited, 0, sizeof(visited));
    for (int seed = 0; seed < WOLF_SPRITE_PIXELS; ++seed) {
        int stack_size = 0;
        int member_count = 0;
        int left = WOLF_SPRITE_SIZE;
        int right = -1;
        int top = WOLF_SPRITE_SIZE;
        int bottom = -1;

        if (!candidates[seed] || visited[seed]) {
            continue;
        }
        visited[seed] = 1u;
        stack[stack_size++] = seed;
        while (stack_size > 0) {
            int pixel = stack[--stack_size];
            int column = pixel % WOLF_SPRITE_SIZE;
            int row = pixel / WOLF_SPRITE_SIZE;
            static const int offsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

            members[member_count++] = pixel;
            left = column < left ? column : left;
            right = column > right ? column : right;
            top = row < top ? row : top;
            bottom = row > bottom ? row : bottom;
            for (int direction = 0; direction < 4; ++direction) {
                int next_column = column + offsets[direction][0];
                int next_row = row + offsets[direction][1];
                int next;

                if (next_column < 0 || next_row < 0 || next_column >= WOLF_SPRITE_SIZE || next_row >= WOLF_SPRITE_SIZE) {
                    continue;
                }
                next = (int)wolf_sprite_pixel_index(next_column, next_row);
                if (candidates[next] && !visited[next]) {
                    visited[next] = 1u;
                    stack[stack_size++] = next;
                }
            }
        }
        if (right - left < bottom - top) {
            for (int member = 0; member < member_count; ++member) {
                candidates[members[member]] = 0u;
            }
        }
    }
}

/*
 * After the shadow is lifted, a few shadow-coloured pixels can remain as
 * crumbs (the dither at a shadow's edge). Any tiny group of shadow colours
 * in the floor band that no longer touches the object is removed too.
 */
static void remove_shadow_crumbs(const WolfSpriteImage *source, int band_top, uint8_t *candidates)
{
    uint8_t visited[WOLF_SPRITE_PIXELS];
    int stack[WOLF_SPRITE_PIXELS];
    int members[WOLF_SPRITE_PIXELS];
    const int largest_crumb = 4;

    memset(visited, 0, sizeof(visited));
    for (int seed = 0; seed < WOLF_SPRITE_PIXELS; ++seed) {
        int stack_size = 0;
        int member_count = 0;
        bool crumb = true;

        if (!source->opaque[seed] || candidates[seed] || visited[seed]) {
            continue;
        }
        visited[seed] = 1u;
        stack[stack_size++] = seed;
        while (stack_size > 0) {
            int pixel = stack[--stack_size];
            int column = pixel % WOLF_SPRITE_SIZE;
            int row = pixel / WOLF_SPRITE_SIZE;

            members[member_count++] = pixel;
            crumb = crumb && row >= band_top && wolf_palette_index_is_shadow(source->palette_index[pixel]);
            /* Eight-connected, so a diagonal touch still counts as attached to the object. */
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int next_column = column + dx;
                    int next_row = row + dy;
                    int next;

                    if (next_column < 0 || next_row < 0 || next_column >= WOLF_SPRITE_SIZE || next_row >= WOLF_SPRITE_SIZE) {
                        continue;
                    }
                    next = (int)wolf_sprite_pixel_index(next_column, next_row);
                    if (source->opaque[next] && !candidates[next] && !visited[next]) {
                        visited[next] = 1u;
                        stack[stack_size++] = next;
                    }
                }
            }
        }
        if (crumb && member_count <= largest_crumb) {
            for (int member = 0; member < member_count; ++member) {
                candidates[members[member]] = 1u;
            }
        }
    }
}

size_t wolf_sprite_remove_floor_shadow(
    const WolfSpriteImage *source,
    WolfSpriteImage *shadow_free,
    uint8_t *removed_mask)
{
    uint8_t candidates[WOLF_SPRITE_PIXELS];
    int bottom = lowest_drawn_row(source);
    int band_top = bottom - WOLF_SHADOW_BAND_ROWS + 1;
    size_t removed = 0u;

    memcpy(shadow_free, source, sizeof(*shadow_free));
    memset(candidates, 0, sizeof(candidates));
    if (removed_mask != NULL) {
        memset(removed_mask, 0, WOLF_SPRITE_PIXELS);
    }
    if (bottom < 0) {
        return 0u;
    }

    for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
        int row = bottom;
        int run_start;

        /* Find this column's lowest drawn pixel; it must sit on the floor. */
        while (row >= band_top && !source->opaque[wolf_sprite_pixel_index(column, row)]) {
            --row;
        }
        if (row < bottom - 2 || row < 0) {
            continue;
        }
        run_start = row;
        while (row >= 0 && source->opaque[wolf_sprite_pixel_index(column, row)] &&
            wolf_palette_index_is_shadow(source->palette_index[wolf_sprite_pixel_index(column, row)])) {
            --row;
        }
        /* A run reaching above the band is the object's own dark shading. */
        if (row == run_start || row + 1 < band_top) {
            continue;
        }
        for (int shadow_row = row + 1; shadow_row <= run_start; ++shadow_row) {
            candidates[wolf_sprite_pixel_index(column, shadow_row)] = 1u;
        }
    }

    keep_flat_patches(candidates);
    /* Crumbs only exist where a shadow was found; an object without one is left untouched. */
    for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        if (candidates[pixel]) {
            remove_shadow_crumbs(source, band_top, candidates);
            break;
        }
    }
    for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        if (candidates[pixel]) {
            shadow_free->opaque[pixel] = 0u;
            shadow_free->palette_index[pixel] = 0u;
            ++removed;
            if (removed_mask != NULL) {
                removed_mask[pixel] = 1u;
            }
        }
    }
    return removed;
}
