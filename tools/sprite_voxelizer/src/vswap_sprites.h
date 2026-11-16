#ifndef WOLF_SPRITE_VOXELIZER_VSWAP_SPRITES_H
#define WOLF_SPRITE_VOXELIZER_VSWAP_SPRITES_H

#include "sprite_image.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The decoded sprite range of a VSWAP page archive. */
typedef struct WolfSpriteArchive {
    uint16_t sprite_count;
    uint16_t first_sprite_page;
    WolfSpriteImage *sprites;
} WolfSpriteArchive;

/*
 * Decode one original t_compshape sprite page. The page starts with the
 * leftmost and rightmost drawn columns and a per-column offset table; each
 * column then lists vertical posts as (end*2, pixel-source base, start*2)
 * word triples terminated by a zero word.
 */
bool wolf_sprite_page_decode(
    const uint8_t *page,
    size_t page_size,
    WolfSpriteImage *image,
    char *error_message,
    size_t error_message_size);

/*
 * Encode an image in the original t_compshape layout. The tool never writes
 * game data; the encoder exists so tests can build original-format fixtures
 * without shipping copyrighted pages. Returns false if the output is too small.
 */
bool wolf_sprite_page_encode(
    const WolfSpriteImage *image,
    uint8_t *page,
    size_t page_capacity,
    size_t *page_size);

/* Decode every sprite page between the VSWAP sprite and sound boundaries. */
bool wolf_sprite_archive_decode(
    const uint8_t *file_data,
    size_t file_size,
    WolfSpriteArchive *archive,
    char *error_message,
    size_t error_message_size);

/* Read a VSWAP file from disk and decode its sprite pages. */
bool wolf_sprite_archive_load(
    const char *path,
    WolfSpriteArchive *archive,
    char *error_message,
    size_t error_message_size);

/* Release decoded sprites and leave the archive empty. */
void wolf_sprite_archive_destroy(WolfSpriteArchive *archive);

#endif
