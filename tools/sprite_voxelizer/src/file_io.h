#ifndef WOLF_SPRITE_VOXELIZER_FILE_IO_H
#define WOLF_SPRITE_VOXELIZER_FILE_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Original Wolfenstein data and MagicaVoxel files are both little-endian. */
static inline uint16_t wolf_read_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static inline uint32_t wolf_read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static inline void wolf_write_u16_le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xffu);
    data[1] = (uint8_t)(value >> 8);
}

static inline void wolf_write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value & 0xffu);
    data[1] = (uint8_t)((value >> 8) & 0xffu);
    data[2] = (uint8_t)((value >> 16) & 0xffu);
    data[3] = (uint8_t)(value >> 24);
}

/* Open a file in binary mode using the secure CRT where MSVC requires it. */
FILE *wolf_open_file(const char *path, const char *mode);

/* Read a whole file into a heap buffer owned by the caller. */
bool wolf_read_entire_file(const char *path, uint8_t **data, size_t *size);

/* Write a complete buffer, reporting partial writes as failure. */
bool wolf_write_entire_file(const char *path, const void *data, size_t size);

/* Create a directory and any missing parents; existing directories are accepted. */
bool wolf_make_directories(const char *path);

/* Bounded error formatting shared by the decoders. */
void wolf_set_error(char *error_message, size_t error_message_size, const char *message);

/*
 * A growable byte buffer used to assemble binary file formats in memory
 * before writing them in one operation.
 */
typedef struct WolfByteBuffer {
    uint8_t *data;
    size_t size;
    size_t capacity;
    bool failed;
} WolfByteBuffer;

void wolf_buffer_append(WolfByteBuffer *buffer, const void *data, size_t size);
void wolf_buffer_append_u8(WolfByteBuffer *buffer, uint8_t value);
void wolf_buffer_append_u32_le(WolfByteBuffer *buffer, uint32_t value);
void wolf_buffer_append_u32_be(WolfByteBuffer *buffer, uint32_t value);
/* Overwrite four bytes already in the buffer, used to back-fill chunk sizes. */
void wolf_buffer_patch_u32_le(WolfByteBuffer *buffer, size_t offset, uint32_t value);
void wolf_buffer_free(WolfByteBuffer *buffer);

#endif
