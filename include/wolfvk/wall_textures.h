#ifndef WOLFVK_WALL_TEXTURES_H
#define WOLFVK_WALL_TEXTURES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WOLF_WALL_TEXTURE_WIDTH 64u
#define WOLF_WALL_TEXTURE_HEIGHT 64u
#define WOLF_WALL_TEXTURE_PIXEL_BYTES 4u
#define WOLF_WALL_TEXTURE_RGBA_SIZE \
    (WOLF_WALL_TEXTURE_WIDTH * WOLF_WALL_TEXTURE_HEIGHT * WOLF_WALL_TEXTURE_PIXEL_BYTES)

typedef struct WolfWallTextures {
    uint16_t page_count;
    uint8_t *rgba_pages;
} WolfWallTextures;

/* Decode the uncompressed wall-page section at the start of an original VSWAP file. */
bool wolf_wall_textures_decode_vswap(
    const uint8_t *file_data,
    size_t file_size,
    WolfWallTextures *textures,
    char *error_message,
    size_t error_message_size);

/* Read VSWAP.WL6 and expand its indexed 64x64 wall pages through the original VGA palette. */
bool wolf_wall_textures_load_vswap(
    const char *path,
    WolfWallTextures *textures,
    char *error_message,
    size_t error_message_size);

/* Map a wall tile to its original horizontal/even or vertical/odd VSWAP page. */
bool wolf_wall_texture_page_for_tile(
    uint16_t wall_tile,
    bool vertical_face,
    uint16_t page_count,
    uint16_t *page_index);

/* Release all decoded RGBA wall pages and reset the texture set. */
void wolf_wall_textures_destroy(WolfWallTextures *textures);

#endif
