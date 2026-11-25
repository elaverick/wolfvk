#include "unity.h"

#include "wolfvk/vga_graphics.h"

#include <string.h>

/*
 * A complete 8-level tree has exactly 255 internal nodes, the size of the
 * original dictionary. Node 254 is the root; following a byte's bits from
 * its most significant end reaches a leaf holding that byte, so every byte
 * costs exactly 8 bits and the encoder below can be very simple.
 */
static void build_identity_dictionary(WolfVgaHuffmanNode dictionary[WOLF_VGA_HUFFMAN_NODE_COUNT])
{
    for (uint32_t heap_index = 1u; heap_index <= 255u; ++heap_index) {
        WolfVgaHuffmanNode *node = &dictionary[255u - heap_index];
        uint32_t children[2] = {heap_index * 2u, heap_index * 2u + 1u};
        uint16_t values[2];

        for (int side = 0; side < 2; ++side) {
            values[side] = children[side] >= 256u
                ? (uint16_t)(children[side] - 256u)
                : (uint16_t)(256u + 255u - children[side]);
        }
        node->bit0 = values[0];
        node->bit1 = values[1];
    }
}

/* Write each byte's bits from the most significant down, packed least-significant-bit first. */
static size_t encode_identity(const uint8_t *data, size_t size, uint8_t *encoded)
{
    size_t bit_position = 0u;

    memset(encoded, 0, size);
    for (size_t index = 0u; index < size; ++index) {
        for (int bit = 7; bit >= 0; --bit) {
            if ((data[index] >> bit) & 1u) {
                encoded[bit_position / 8u] |= (uint8_t)(1u << (bit_position % 8u));
            }
            ++bit_position;
        }
    }
    return size;
}

static void write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static void write_head_entry(uint8_t *head, uint32_t chunk, uint32_t value)
{
    head[chunk * 3u] = (uint8_t)value;
    head[chunk * 3u + 1u] = (uint8_t)(value >> 8);
    head[chunk * 3u + 2u] = (uint8_t)(value >> 16);
}

static void dictionary_bytes(
    const WolfVgaHuffmanNode dictionary[WOLF_VGA_HUFFMAN_NODE_COUNT],
    uint8_t bytes[WOLF_VGA_HUFFMAN_NODE_COUNT * 4u])
{
    for (uint32_t node = 0u; node < WOLF_VGA_HUFFMAN_NODE_COUNT; ++node) {
        bytes[node * 4u] = (uint8_t)dictionary[node].bit0;
        bytes[node * 4u + 1u] = (uint8_t)(dictionary[node].bit0 >> 8);
        bytes[node * 4u + 2u] = (uint8_t)dictionary[node].bit1;
        bytes[node * 4u + 3u] = (uint8_t)(dictionary[node].bit1 >> 8);
    }
}

void test_vga_huffman_reads_bits_least_significant_first_from_node_254(void)
{
    WolfVgaHuffmanNode dictionary[WOLF_VGA_HUFFMAN_NODE_COUNT] = {0};
    /* 'A' is bit 0; 'B' is bits 1,0 and 'C' is bits 1,1 through node 253. */
    const uint8_t encoded[] = {0x1au};
    uint8_t decoded[4] = {0};

    dictionary[254].bit0 = 'A';
    dictionary[254].bit1 = 256u + 253u;
    dictionary[253].bit0 = 'B';
    dictionary[253].bit1 = 'C';

    TEST_ASSERT_TRUE(wolf_vga_huffman_expand(dictionary, encoded, 1u, decoded, 4u));
    TEST_ASSERT_EQUAL_MEMORY("ABCA", decoded, 4u);
}

void test_vga_huffman_rejects_truncated_input_and_invalid_nodes(void)
{
    WolfVgaHuffmanNode dictionary[WOLF_VGA_HUFFMAN_NODE_COUNT] = {0};
    const uint8_t encoded[] = {0xffu};
    uint8_t decoded[16];

    build_identity_dictionary(dictionary);
    TEST_ASSERT_FALSE(wolf_vga_huffman_expand(dictionary, encoded, 1u, decoded, 2u));

    dictionary[254].bit1 = 256u + 300u;
    TEST_ASSERT_FALSE(wolf_vga_huffman_expand(dictionary, encoded, 1u, decoded, 1u));
    TEST_ASSERT_FALSE(wolf_vga_huffman_expand(NULL, encoded, 1u, decoded, 1u));
}

void test_vga_deplanarize_interleaves_the_four_mode_x_planes(void)
{
    /* 8x2: each plane holds two columns of two rows (x = plane, plane + 4). */
    const uint8_t planar[16] = {
        0, 4, 10, 14,
        1, 5, 11, 15,
        2, 6, 12, 16,
        3, 7, 13, 17
    };
    const uint8_t expected[16] = {
        0, 1, 2, 3, 4, 5, 6, 7,
        10, 11, 12, 13, 14, 15, 16, 17
    };
    uint8_t row_major[16] = {0};

    TEST_ASSERT_TRUE(wolf_vga_deplanarize(planar, 8u, 2u, row_major));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, row_major, 16u);
    TEST_ASSERT_FALSE(wolf_vga_deplanarize(planar, 6u, 2u, row_major));
}

void test_vga_graphics_decodes_size_table_and_skips_sparse_chunks(void)
{
    WolfVgaHuffmanNode dictionary[WOLF_VGA_HUFFMAN_NODE_COUNT];
    uint8_t dictionary_data[WOLF_VGA_HUFFMAN_NODE_COUNT * 4u];
    /* Chunks: 0 size table, 1-2 sparse fonts, 3 an 8x2 picture; then the end marker. */
    uint8_t head[5u * 3u];
    uint8_t graph[64];
    const uint8_t size_table[4] = {8u, 0u, 2u, 0u};
    const uint8_t planar_picture[16] = {
        0, 4, 10, 14, 1, 5, 11, 15, 2, 6, 12, 16, 3, 7, 13, 17
    };
    const uint8_t expected[16] = {
        0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14, 15, 16, 17
    };
    size_t graph_size = 0u;
    WolfVgaGraphics graphics = {0};
    WolfVgaPicture picture = {0};
    char error_message[128];

    build_identity_dictionary(dictionary);
    dictionary_bytes(dictionary, dictionary_data);

    write_head_entry(head, 0u, 0u);
    write_u32_le(graph, sizeof(size_table));
    graph_size = 4u + encode_identity(size_table, sizeof(size_table), graph + 4u);
    write_head_entry(head, 1u, 0xffffffu);
    write_head_entry(head, 2u, 0xffffffu);
    write_head_entry(head, 3u, (uint32_t)graph_size);
    write_u32_le(graph + graph_size, sizeof(planar_picture));
    graph_size += 4u + encode_identity(planar_picture, sizeof(planar_picture), graph + graph_size + 4u);
    write_head_entry(head, 4u, (uint32_t)graph_size);

    TEST_ASSERT_TRUE_MESSAGE(
        wolf_vga_graphics_decode(
            head, sizeof(head), dictionary_data, sizeof(dictionary_data), graph, graph_size,
            &graphics, error_message, sizeof(error_message)),
        error_message);
    TEST_ASSERT_EQUAL_UINT32(4u, graphics.chunk_count);
    TEST_ASSERT_EQUAL_UINT32(1u, graphics.picture_count);
    TEST_ASSERT_EQUAL_UINT32(WOLF_VGA_SPARSE_CHUNK, graphics.chunk_offsets[1]);

    TEST_ASSERT_TRUE(wolf_vga_graphics_decode_picture(&graphics, 3u, &picture));
    TEST_ASSERT_EQUAL_UINT16(8u, picture.width);
    TEST_ASSERT_EQUAL_UINT16(2u, picture.height);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, picture.pixels, 16u);

    /* Fonts and the size table are not pictures, and there is no picture past the table. */
    TEST_ASSERT_FALSE(wolf_vga_graphics_decode_picture(&graphics, 1u, &picture));
    TEST_ASSERT_FALSE(wolf_vga_graphics_decode_picture(&graphics, 4u, &picture));

    wolf_vga_picture_destroy(&picture);
    TEST_ASSERT_NULL(picture.pixels);
    wolf_vga_graphics_destroy(&graphics);
    TEST_ASSERT_NULL(graphics.graph_data);
}

void test_vga_graphics_rejects_malformed_files(void)
{
    WolfVgaHuffmanNode dictionary[WOLF_VGA_HUFFMAN_NODE_COUNT];
    uint8_t dictionary_data[WOLF_VGA_HUFFMAN_NODE_COUNT * 4u];
    uint8_t head[5u * 3u] = {0};
    uint8_t graph[8] = {0};
    WolfVgaGraphics graphics = {0};
    char error_message[128];

    build_identity_dictionary(dictionary);
    dictionary_bytes(dictionary, dictionary_data);

    /* A dictionary too short for 255 nodes. */
    TEST_ASSERT_FALSE(wolf_vga_graphics_decode(
        head, sizeof(head), dictionary_data, 100u, graph, sizeof(graph),
        &graphics, error_message, sizeof(error_message)));
    /* A head that is not a whole number of 3-byte entries. */
    TEST_ASSERT_FALSE(wolf_vga_graphics_decode(
        head, sizeof(head) - 1u, dictionary_data, sizeof(dictionary_data), graph, sizeof(graph),
        &graphics, error_message, sizeof(error_message)));
    /* A size table that claims more data than its compressed bytes can hold. */
    write_u32_le(graph, 1000u);
    write_head_entry(head, 1u, sizeof(graph));
    write_head_entry(head, 2u, sizeof(graph));
    write_head_entry(head, 3u, sizeof(graph));
    write_head_entry(head, 4u, sizeof(graph));
    TEST_ASSERT_FALSE(wolf_vga_graphics_decode(
        head, sizeof(head), dictionary_data, sizeof(dictionary_data), graph, sizeof(graph),
        &graphics, error_message, sizeof(error_message)));
    TEST_ASSERT_FALSE(wolf_vga_graphics_load(
        "missing-VGAHEAD.WL6", "missing-VGADICT.WL6", "missing-VGAGRAPH.WL6",
        &graphics, error_message, sizeof(error_message)));
}
