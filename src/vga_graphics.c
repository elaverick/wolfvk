#include "wolfvk/vga_graphics.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* VGAHEAD stores each chunk's file position as a 3-byte little-endian value. */
#define VGA_HEAD_ENTRY_SIZE 3u
#define VGA_HEAD_SPARSE_VALUE 0xffffffu
/* Every non-tile chunk starts with its expanded length as a 32-bit value. */
#define VGA_CHUNK_LENGTH_PREFIX 4u
/* The size table holds a 16-bit width and height for each picture. */
#define VGA_SIZE_TABLE_ENTRY_SIZE 4u

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)(data[0] | (data[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void write_error(char *error_message, size_t error_message_size, const char *format, ...)
{
    va_list arguments;

    if (error_message == NULL || error_message_size == 0u) {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(error_message, error_message_size, format, arguments);
    va_end(arguments);
}

bool wolf_vga_huffman_expand(
    const WolfVgaHuffmanNode dictionary[WOLF_VGA_HUFFMAN_NODE_COUNT],
    const uint8_t *source,
    size_t source_size,
    uint8_t *destination,
    size_t destination_size)
{
    const WolfVgaHuffmanNode *node;
    size_t source_index = 0u;
    size_t written = 0u;

    if (dictionary == NULL || destination == NULL || (source == NULL && source_size > 0u)) {
        return false;
    }

    node = &dictionary[WOLF_VGA_HUFFMAN_HEAD_NODE];
    while (written < destination_size) {
        uint8_t source_byte;

        if (source_index >= source_size) {
            return false;
        }
        source_byte = source[source_index++];
        for (unsigned int bit = 0u; bit < 8u && written < destination_size; ++bit) {
            uint16_t value = (source_byte & (1u << bit)) != 0u ? node->bit1 : node->bit0;

            if (value < 256u) {
                destination[written++] = (uint8_t)value;
                node = &dictionary[WOLF_VGA_HUFFMAN_HEAD_NODE];
            } else if ((uint32_t)value - 256u < WOLF_VGA_HUFFMAN_NODE_COUNT) {
                node = &dictionary[value - 256u];
            } else {
                return false;
            }
        }
    }
    return true;
}

bool wolf_vga_deplanarize(
    const uint8_t *planar,
    uint16_t width,
    uint16_t height,
    uint8_t *row_major)
{
    size_t plane_width;
    size_t plane_size;

    if (planar == NULL || row_major == NULL || width == 0u || height == 0u ||
        width % 4u != 0u) {
        return false;
    }

    plane_width = width / 4u;
    plane_size = plane_width * height;
    for (size_t y = 0u; y < height; ++y) {
        for (size_t x = 0u; x < width; ++x) {
            row_major[y * width + x] =
                planar[(x % 4u) * plane_size + y * plane_width + x / 4u];
        }
    }
    return true;
}

/*
 * Return the compressed span of a chunk. Sparse chunks have no data, and the
 * span of a present chunk runs to the next chunk that does exist.
 */
static bool find_chunk_span(
    const WolfVgaGraphics *graphics,
    uint32_t chunk,
    size_t *offset,
    size_t *size)
{
    uint32_t next;

    if (chunk >= graphics->chunk_count ||
        graphics->chunk_offsets[chunk] == WOLF_VGA_SPARSE_CHUNK) {
        return false;
    }
    next = chunk + 1u;
    while (next < graphics->chunk_count &&
        graphics->chunk_offsets[next] == WOLF_VGA_SPARSE_CHUNK) {
        ++next;
    }

    *offset = graphics->chunk_offsets[chunk];
    if (next < graphics->chunk_count) {
        if (graphics->chunk_offsets[next] < *offset) {
            return false;
        }
        *size = graphics->chunk_offsets[next] - *offset;
    } else {
        *size = graphics->graph_size - *offset;
    }
    return *offset <= graphics->graph_size && *size <= graphics->graph_size - *offset;
}

/* Expand a chunk that begins with its own 32-bit expanded length into a new allocation. */
static bool expand_chunk(
    const WolfVgaGraphics *graphics,
    uint32_t chunk,
    uint8_t **expanded,
    size_t *expanded_size)
{
    size_t offset;
    size_t size;
    uint32_t declared_size;
    uint8_t *buffer;

    if (!find_chunk_span(graphics, chunk, &offset, &size) || size < VGA_CHUNK_LENGTH_PREFIX) {
        return false;
    }
    declared_size = read_u32_le(graphics->graph_data + offset);
    if (declared_size == 0u || declared_size > 16u * 1024u * 1024u) {
        return false;
    }

    buffer = malloc(declared_size);
    if (buffer == NULL) {
        return false;
    }
    if (!wolf_vga_huffman_expand(
            graphics->dictionary,
            graphics->graph_data + offset + VGA_CHUNK_LENGTH_PREFIX,
            size - VGA_CHUNK_LENGTH_PREFIX,
            buffer,
            declared_size)) {
        free(buffer);
        return false;
    }
    *expanded = buffer;
    *expanded_size = declared_size;
    return true;
}

bool wolf_vga_graphics_decode(
    const uint8_t *head_data,
    size_t head_size,
    const uint8_t *dictionary_data,
    size_t dictionary_size,
    const uint8_t *graph_data,
    size_t graph_size,
    WolfVgaGraphics *graphics,
    char *error_message,
    size_t error_message_size)
{
    WolfVgaGraphics decoded = {0};
    uint8_t *size_table = NULL;
    size_t size_table_size = 0u;
    uint32_t offset_count;

    if (head_data == NULL || dictionary_data == NULL || graph_data == NULL || graphics == NULL) {
        write_error(error_message, error_message_size, "Invalid VGA graphics arguments.");
        return false;
    }
    if (dictionary_size < WOLF_VGA_HUFFMAN_NODE_COUNT * 4u) {
        write_error(error_message, error_message_size,
            "VGADICT holds %zu bytes; the Huffman tree needs %u.",
            dictionary_size, WOLF_VGA_HUFFMAN_NODE_COUNT * 4u);
        return false;
    }
    if (head_size % VGA_HEAD_ENTRY_SIZE != 0u ||
        head_size / VGA_HEAD_ENTRY_SIZE < WOLF_VGA_FIRST_PICTURE_CHUNK + 1u) {
        write_error(error_message, error_message_size,
            "VGAHEAD holds %zu bytes, which is not a list of 3-byte chunk offsets.", head_size);
        return false;
    }

    for (uint32_t node = 0u; node < WOLF_VGA_HUFFMAN_NODE_COUNT; ++node) {
        decoded.dictionary[node].bit0 = read_u16_le(dictionary_data + node * 4u);
        decoded.dictionary[node].bit1 = read_u16_le(dictionary_data + node * 4u + 2u);
    }

    /* The final head entry marks the end of the last chunk rather than a chunk of its own. */
    offset_count = (uint32_t)(head_size / VGA_HEAD_ENTRY_SIZE);
    decoded.chunk_count = offset_count - 1u;
    decoded.chunk_offsets = malloc((size_t)offset_count * sizeof(uint32_t));
    decoded.graph_data = malloc(graph_size > 0u ? graph_size : 1u);
    if (decoded.chunk_offsets == NULL || decoded.graph_data == NULL) {
        write_error(error_message, error_message_size, "Out of memory reading VGA graphics.");
        wolf_vga_graphics_destroy(&decoded);
        return false;
    }
    for (uint32_t chunk = 0u; chunk < offset_count; ++chunk) {
        const uint8_t *entry = head_data + chunk * VGA_HEAD_ENTRY_SIZE;
        uint32_t value = (uint32_t)entry[0] | ((uint32_t)entry[1] << 8) |
            ((uint32_t)entry[2] << 16);

        decoded.chunk_offsets[chunk] =
            value == VGA_HEAD_SPARSE_VALUE ? WOLF_VGA_SPARSE_CHUNK : value;
    }
    memcpy(decoded.graph_data, graph_data, graph_size);
    decoded.graph_size = graph_size;

    if (!expand_chunk(&decoded, WOLF_VGA_SIZE_TABLE_CHUNK, &size_table, &size_table_size) ||
        size_table_size < VGA_SIZE_TABLE_ENTRY_SIZE) {
        write_error(error_message, error_message_size,
            "The VGAGRAPH picture size table could not be decompressed.");
        wolf_vga_graphics_destroy(&decoded);
        return false;
    }

    decoded.picture_count = (uint32_t)(size_table_size / VGA_SIZE_TABLE_ENTRY_SIZE);
    decoded.picture_widths = malloc((size_t)decoded.picture_count * sizeof(uint16_t));
    decoded.picture_heights = malloc((size_t)decoded.picture_count * sizeof(uint16_t));
    if (decoded.picture_widths == NULL || decoded.picture_heights == NULL) {
        free(size_table);
        write_error(error_message, error_message_size, "Out of memory reading VGA graphics.");
        wolf_vga_graphics_destroy(&decoded);
        return false;
    }
    for (uint32_t picture = 0u; picture < decoded.picture_count; ++picture) {
        decoded.picture_widths[picture] =
            read_u16_le(size_table + picture * VGA_SIZE_TABLE_ENTRY_SIZE);
        decoded.picture_heights[picture] =
            read_u16_le(size_table + picture * VGA_SIZE_TABLE_ENTRY_SIZE + 2u);
    }
    free(size_table);

    *graphics = decoded;
    return true;
}

/* Use the secure CRT on Windows while preserving normal binary fopen elsewhere. */
static FILE *open_binary_file(const char *path)
{
    FILE *file = NULL;

#ifdef _MSC_VER
    if (fopen_s(&file, path, "rb") != 0) {
        return NULL;
    }
#else
    file = fopen(path, "rb");
#endif
    return file;
}

static bool read_whole_file(const char *path, uint8_t **data, size_t *size)
{
    FILE *file = open_binary_file(path);
    long length;
    uint8_t *buffer;

    if (file == NULL) {
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    buffer = malloc(length > 0 ? (size_t)length : 1u);
    if (buffer == NULL ||
        (length > 0 && fread(buffer, 1u, (size_t)length, file) != (size_t)length)) {
        free(buffer);
        fclose(file);
        return false;
    }
    fclose(file);
    *data = buffer;
    *size = (size_t)length;
    return true;
}

bool wolf_vga_graphics_load(
    const char *head_path,
    const char *dictionary_path,
    const char *graph_path,
    WolfVgaGraphics *graphics,
    char *error_message,
    size_t error_message_size)
{
    const char *paths[3] = {head_path, dictionary_path, graph_path};
    uint8_t *contents[3] = {NULL, NULL, NULL};
    size_t sizes[3] = {0u, 0u, 0u};
    bool decoded = false;

    if (head_path == NULL || dictionary_path == NULL || graph_path == NULL) {
        write_error(error_message, error_message_size, "Invalid VGA graphics file paths.");
        return false;
    }
    for (int file = 0; file < 3; ++file) {
        if (!read_whole_file(paths[file], &contents[file], &sizes[file])) {
            write_error(error_message, error_message_size, "Could not read %s.", paths[file]);
            goto cleanup;
        }
    }
    decoded = wolf_vga_graphics_decode(
        contents[0], sizes[0], contents[1], sizes[1], contents[2], sizes[2],
        graphics, error_message, error_message_size);

cleanup:
    for (int file = 0; file < 3; ++file) {
        free(contents[file]);
    }
    return decoded;
}

bool wolf_vga_graphics_decode_picture(
    const WolfVgaGraphics *graphics,
    uint32_t chunk,
    WolfVgaPicture *picture)
{
    uint32_t picture_index;
    uint16_t width;
    uint16_t height;
    size_t pixel_count;
    uint8_t *planar = NULL;
    size_t planar_size = 0u;
    uint8_t *pixels;

    if (graphics == NULL || picture == NULL || chunk < WOLF_VGA_FIRST_PICTURE_CHUNK) {
        return false;
    }
    picture_index = chunk - WOLF_VGA_FIRST_PICTURE_CHUNK;
    if (picture_index >= graphics->picture_count) {
        return false;
    }
    width = graphics->picture_widths[picture_index];
    height = graphics->picture_heights[picture_index];
    pixel_count = (size_t)width * height;
    if (pixel_count == 0u || width % 4u != 0u) {
        return false;
    }

    if (!expand_chunk(graphics, chunk, &planar, &planar_size)) {
        return false;
    }
    if (planar_size < pixel_count) {
        free(planar);
        return false;
    }
    pixels = malloc(pixel_count);
    if (pixels == NULL) {
        free(planar);
        return false;
    }
    (void)wolf_vga_deplanarize(planar, width, height, pixels);
    free(planar);

    picture->width = width;
    picture->height = height;
    picture->pixels = pixels;
    return true;
}

void wolf_vga_picture_destroy(WolfVgaPicture *picture)
{
    if (picture != NULL) {
        free(picture->pixels);
        memset(picture, 0, sizeof(*picture));
    }
}

void wolf_vga_graphics_destroy(WolfVgaGraphics *graphics)
{
    if (graphics != NULL) {
        free(graphics->chunk_offsets);
        free(graphics->graph_data);
        free(graphics->picture_widths);
        free(graphics->picture_heights);
        memset(graphics, 0, sizeof(*graphics));
    }
}
