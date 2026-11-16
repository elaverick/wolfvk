#ifndef WOLF_SPRITE_VOXELIZER_PNG_WRITER_H
#define WOLF_SPRITE_VOXELIZER_PNG_WRITER_H

#include "file_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Minimal PNG output for extracted sprites and validation sheets. Images are
 * 8-bit RGBA; pixel data is compressed with a self-contained deflate encoder
 * (LZ77 with the fixed Huffman code) so the tool has no library dependency.
 */

uint32_t wolf_png_crc32(const uint8_t *data, size_t size);
uint32_t wolf_png_adler32(const uint8_t *data, size_t size);

/* Compress bytes into a zlib stream (RFC 1950) using fixed-Huffman deflate blocks. */
bool wolf_zlib_compress(const uint8_t *data, size_t size, WolfByteBuffer *output);

bool wolf_png_encode_rgba(const uint8_t *rgba, uint32_t width, uint32_t height, WolfByteBuffer *output);
bool wolf_png_write_rgba(const char *path, const uint8_t *rgba, uint32_t width, uint32_t height);

#endif
