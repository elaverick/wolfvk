#ifndef WOLFVK_VGA_GRAPHICS_H
#define WOLFVK_VGA_GRAPHICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The original game keeps its 2D pictures (status bar, faces, digits, menus)
 * in VGAGRAPH.WL6. Each chunk is Huffman-compressed with the tree stored in
 * VGADICT.WL6, and VGAHEAD.WL6 lists the 3-byte file offset of every chunk.
 * Chunk 0 is the picture size table and chunks 1-2 are fonts, so pictures
 * start at chunk 3.
 */
#define WOLF_VGA_HUFFMAN_NODE_COUNT 255u
#define WOLF_VGA_HUFFMAN_HEAD_NODE 254u
#define WOLF_VGA_SIZE_TABLE_CHUNK 0u
#define WOLF_VGA_FIRST_PICTURE_CHUNK 3u
#define WOLF_VGA_SPARSE_CHUNK UINT32_MAX

/* WL6 chunk numbers from the original GFXV_WL6.H that the status bar draws. */
enum {
    WOLF_VGA_STATUSBARPIC = 86,
    WOLF_VGA_KNIFEPIC = 91,
    WOLF_VGA_GUNPIC = 92,
    WOLF_VGA_MACHINEGUNPIC = 93,
    WOLF_VGA_GATLINGGUNPIC = 94,
    WOLF_VGA_NOKEYPIC = 95,
    WOLF_VGA_GOLDKEYPIC = 96,
    WOLF_VGA_SILVERKEYPIC = 97,
    WOLF_VGA_N_BLANKPIC = 98,
    WOLF_VGA_N_0PIC = 99,
    WOLF_VGA_FACE1APIC = 109,
    WOLF_VGA_FACE8APIC = 130,
    WOLF_VGA_GOTGATLINGPIC = 131,
    WOLF_VGA_MUTANTBJPIC = 132
};

typedef struct WolfVgaHuffmanNode {
    /* Values below 256 are output bytes; larger values refer to node (value - 256). */
    uint16_t bit0;
    uint16_t bit1;
} WolfVgaHuffmanNode;

typedef struct WolfVgaGraphics {
    WolfVgaHuffmanNode dictionary[WOLF_VGA_HUFFMAN_NODE_COUNT];
    uint32_t *chunk_offsets;
    uint32_t chunk_count;
    uint8_t *graph_data;
    size_t graph_size;
    uint16_t *picture_widths;
    uint16_t *picture_heights;
    uint32_t picture_count;
} WolfVgaGraphics;

/* One decoded picture as row-major indices into the original VGA palette. */
typedef struct WolfVgaPicture {
    uint16_t width;
    uint16_t height;
    uint8_t *pixels;
} WolfVgaPicture;

/*
 * Expand exactly destination_size bytes using the original id Software
 * Huffman scheme: bits are read least-significant first from each source
 * byte and decoding always restarts at node 254.
 */
bool wolf_vga_huffman_expand(
    const WolfVgaHuffmanNode dictionary[WOLF_VGA_HUFFMAN_NODE_COUNT],
    const uint8_t *source,
    size_t source_size,
    uint8_t *destination,
    size_t destination_size);

/*
 * Reorder a picture from the original Mode X layout, where four planes each
 * hold every fourth column, into row-major pixels. Width must be a multiple of 4.
 */
bool wolf_vga_deplanarize(
    const uint8_t *planar,
    uint16_t width,
    uint16_t height,
    uint8_t *row_major);

/* Parse in-memory VGAHEAD, VGADICT and VGAGRAPH data and read the picture size table. */
bool wolf_vga_graphics_decode(
    const uint8_t *head_data,
    size_t head_size,
    const uint8_t *dictionary_data,
    size_t dictionary_size,
    const uint8_t *graph_data,
    size_t graph_size,
    WolfVgaGraphics *graphics,
    char *error_message,
    size_t error_message_size);

/* Read and parse the three original graphics files. */
bool wolf_vga_graphics_load(
    const char *head_path,
    const char *dictionary_path,
    const char *graph_path,
    WolfVgaGraphics *graphics,
    char *error_message,
    size_t error_message_size);

/* Decompress one picture chunk (3 and above) into row-major palette indices. */
bool wolf_vga_graphics_decode_picture(
    const WolfVgaGraphics *graphics,
    uint32_t chunk,
    WolfVgaPicture *picture);

void wolf_vga_picture_destroy(WolfVgaPicture *picture);
void wolf_vga_graphics_destroy(WolfVgaGraphics *graphics);

#endif
