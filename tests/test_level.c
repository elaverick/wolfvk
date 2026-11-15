#include "unity.h"

#include "wolfvk/level_geometry.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#define TEST_RLEW_TAG 0xabcd
#define TEST_PLANE_CHUNK_SIZE 10u
#define TEST_MAP_HEADER_SIZE 38u
#define TEST_MAPHEAD_SIZE (2u + WOLF_LEVEL_MAPHEAD_OFFSET_COUNT * 4u)
#define TEST_GAMEMAPS_SIZE (TEST_MAP_HEADER_SIZE + 2u * TEST_PLANE_CHUNK_SIZE)

static void write_u16_le(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)(value >> 8);
}

static void write_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)((value >> 8) & 0xffu);
    destination[2] = (uint8_t)((value >> 16) & 0xffu);
    destination[3] = (uint8_t)(value >> 24);
}

static void write_uniform_plane(uint8_t *destination, uint16_t value)
{
    write_u16_le(destination, 8u);
    write_u16_le(destination + 2u, (uint16_t)(WOLF_LEVEL_PLANE_WORDS * sizeof(uint16_t)));
    write_u16_le(destination + 4u, (uint16_t)TEST_RLEW_TAG);
    write_u16_le(destination + 6u, (uint16_t)WOLF_LEVEL_PLANE_WORDS);
    write_u16_le(destination + 8u, value);
}

static void write_zero_plane(uint8_t *destination)
{
    write_uniform_plane(destination, 0u);
}

static bool write_test_file(const char *path, const uint8_t *data, size_t data_size)
{
    FILE *file = NULL;
    size_t bytes_written;
    int close_result;

#if defined(_MSC_VER)
    if (fopen_s(&file, path, "wb") != 0) {
        return false;
    }
#else
    file = fopen(path, "wb");
#endif

    if (file == NULL) {
        return false;
    }

    bytes_written = fwrite(data, 1u, data_size, file);
    close_result = fclose(file);
    return bytes_written == data_size && close_result == 0;
}

static bool create_test_map_files(char *maphead_path, size_t maphead_path_size,
                                  char *gamemaps_path, size_t gamemaps_path_size,
                                  uint16_t wall_plane_value)
{
    const char *base_path = SDL_GetBasePath();
    uint8_t maphead[TEST_MAPHEAD_SIZE];
    uint8_t gamemaps[TEST_GAMEMAPS_SIZE] = {0};
    int maphead_length;
    int gamemaps_length;

    if (base_path == NULL) {
        return false;
    }

    maphead_length = snprintf(
        maphead_path, maphead_path_size, "%swolfvk_test_maphead.bin", base_path);
    gamemaps_length = snprintf(
        gamemaps_path, gamemaps_path_size, "%swolfvk_test_gamemaps.bin", base_path);
    if (maphead_length < 0 || (size_t)maphead_length >= maphead_path_size ||
        gamemaps_length < 0 || (size_t)gamemaps_length >= gamemaps_path_size) {
        return false;
    }

    memset(maphead, 0xff, sizeof(maphead));
    write_u16_le(maphead, (uint16_t)TEST_RLEW_TAG);
    write_u32_le(maphead + 2u, 0u);

    write_u32_le(gamemaps, TEST_MAP_HEADER_SIZE);
    write_u32_le(gamemaps + 4u, TEST_MAP_HEADER_SIZE + TEST_PLANE_CHUNK_SIZE);
    write_u16_le(gamemaps + 12u, (uint16_t)TEST_PLANE_CHUNK_SIZE);
    write_u16_le(gamemaps + 14u, (uint16_t)TEST_PLANE_CHUNK_SIZE);
    write_u16_le(gamemaps + 18u, (uint16_t)WOLF_LEVEL_PLANE_WIDTH);
    write_u16_le(gamemaps + 20u, (uint16_t)WOLF_LEVEL_PLANE_HEIGHT);
    memcpy(gamemaps + 22u, "TEST LEVEL", 10u);

    write_uniform_plane(gamemaps + TEST_MAP_HEADER_SIZE, wall_plane_value);
    write_zero_plane(gamemaps + TEST_MAP_HEADER_SIZE + TEST_PLANE_CHUNK_SIZE);

    return write_test_file(maphead_path, maphead, sizeof(maphead)) &&
        write_test_file(gamemaps_path, gamemaps, sizeof(gamemaps));
}

void test_carmack_decoder_expands_literal_words(void)
{
    const uint8_t source[] = {4u, 0u, 0x34u, 0x12u, 0x78u, 0x56u};
    uint8_t destination[4] = {0};

    TEST_ASSERT_TRUE(wolf_level_decode_carmack(
        source, sizeof(source), destination, sizeof(destination)));
    TEST_ASSERT_EQUAL_HEX8(0x34u, destination[0]);
    TEST_ASSERT_EQUAL_HEX8(0x12u, destination[1]);
    TEST_ASSERT_EQUAL_HEX8(0x78u, destination[2]);
    TEST_ASSERT_EQUAL_HEX8(0x56u, destination[3]);
}

void test_carmack_decoder_expands_near_and_far_references(void)
{
    const uint8_t near_source[] = {
        6u, 0u, 0x34u, 0x12u, 0x02u, 0xa7u, 0x01u
    };
    const uint8_t far_source[] = {
        6u, 0u, 0x34u, 0x12u, 0x02u, 0xa8u, 0x00u, 0x00u
    };
    uint8_t destination[6] = {0};

    TEST_ASSERT_TRUE(wolf_level_decode_carmack(
        near_source, sizeof(near_source), destination, sizeof(destination)));
    for (size_t index = 0; index < sizeof(destination); index += 2u) {
        TEST_ASSERT_EQUAL_HEX8(0x34u, destination[index]);
        TEST_ASSERT_EQUAL_HEX8(0x12u, destination[index + 1u]);
    }

    memset(destination, 0, sizeof(destination));
    TEST_ASSERT_TRUE(wolf_level_decode_carmack(
        far_source, sizeof(far_source), destination, sizeof(destination)));
    for (size_t index = 0; index < sizeof(destination); index += 2u) {
        TEST_ASSERT_EQUAL_HEX8(0x34u, destination[index]);
        TEST_ASSERT_EQUAL_HEX8(0x12u, destination[index + 1u]);
    }
}

void test_carmack_decoder_preserves_tagged_literal_words(void)
{
    const uint8_t source[] = {2u, 0u, 0x00u, 0xa7u, 0x5cu};
    uint8_t destination[2] = {0};

    TEST_ASSERT_TRUE(wolf_level_decode_carmack(
        source, sizeof(source), destination, sizeof(destination)));
    TEST_ASSERT_EQUAL_HEX8(0x5cu, destination[0]);
    TEST_ASSERT_EQUAL_HEX8(0xa7u, destination[1]);
}

void test_carmack_decoder_rejects_invalid_back_references(void)
{
    const uint8_t source[] = {4u, 0u, 0x01u, 0xa7u, 0x01u};
    uint8_t destination[4] = {0};

    TEST_ASSERT_FALSE(wolf_level_decode_carmack(
        source, sizeof(source), destination, sizeof(destination)));
}

void test_rlew_decoder_expands_literals_and_runs(void)
{
    const uint8_t literal_source[] = {4u, 0u, 1u, 0u, 2u, 0u};
    const uint8_t run_source[] = {
        6u, 0u, 0xcdu, 0xabu, 3u, 0u, 0x34u, 0x12u
    };
    uint16_t literal_destination[2] = {0};
    uint16_t run_destination[3] = {0};

    TEST_ASSERT_TRUE(wolf_level_decode_rlew(
        literal_source, sizeof(literal_source), TEST_RLEW_TAG,
        literal_destination, 2u));
    TEST_ASSERT_EQUAL_UINT16(1u, literal_destination[0]);
    TEST_ASSERT_EQUAL_UINT16(2u, literal_destination[1]);

    TEST_ASSERT_TRUE(wolf_level_decode_rlew(
        run_source, sizeof(run_source), TEST_RLEW_TAG,
        run_destination, 3u));
    TEST_ASSERT_EQUAL_UINT16(0x1234u, run_destination[0]);
    TEST_ASSERT_EQUAL_UINT16(0x1234u, run_destination[1]);
    TEST_ASSERT_EQUAL_UINT16(0x1234u, run_destination[2]);
}

void test_rlew_decoder_rejects_truncated_and_overflowing_runs(void)
{
    const uint8_t truncated_source[] = {6u, 0u, 0xcdu, 0xabu, 3u, 0u};
    const uint8_t overflow_source[] = {
        4u, 0u, 0xcdu, 0xabu, 3u, 0u, 0x34u, 0x12u
    };
    uint16_t destination[2] = {0};

    TEST_ASSERT_FALSE(wolf_level_decode_rlew(
        truncated_source, sizeof(truncated_source), TEST_RLEW_TAG,
        destination, 3u));
    TEST_ASSERT_FALSE(wolf_level_decode_rlew(
        overflow_source, sizeof(overflow_source), TEST_RLEW_TAG,
        destination, 2u));
}

void test_level_loader_reads_original_map_header_and_planes(void)
{
    char maphead_path[1024];
    char gamemaps_path[1024];
    WolfLevel level = {0};
    char error_message[256];

    TEST_ASSERT_TRUE(create_test_map_files(
        maphead_path, sizeof(maphead_path), gamemaps_path, sizeof(gamemaps_path), 5u));

    TEST_ASSERT_TRUE_MESSAGE(
        wolf_level_load(
            maphead_path, gamemaps_path, 0u,
            &level, error_message, sizeof(error_message)),
        error_message);

    TEST_ASSERT_EQUAL_UINT16(WOLF_LEVEL_PLANE_WIDTH, level.width);
    TEST_ASSERT_EQUAL_UINT16(WOLF_LEVEL_PLANE_HEIGHT, level.height);
    TEST_ASSERT_EQUAL_STRING("TEST LEVEL", level.name);
    TEST_ASSERT_EQUAL_UINT16(5u, level.wall_tiles[0]);
    TEST_ASSERT_EQUAL_UINT16(5u, level.wall_tiles[WOLF_LEVEL_PLANE_WORDS - 1u]);
    TEST_ASSERT_EQUAL_UINT16(0u, level.object_tiles[0]);
    TEST_ASSERT_EQUAL_UINT16(0u, level.object_tiles[WOLF_LEVEL_PLANE_WORDS - 1u]);

    remove(maphead_path);
    remove(gamemaps_path);
}

void test_level_loader_converts_area_markers_to_open_cells(void)
{
    char maphead_path[1024];
    char gamemaps_path[1024];
    WolfLevel level = {0};
    char error_message[256];

    TEST_ASSERT_TRUE(create_test_map_files(
        maphead_path, sizeof(maphead_path), gamemaps_path, sizeof(gamemaps_path),
        (uint16_t)WOLF_LEVEL_AREA_TILE_FIRST));

    TEST_ASSERT_TRUE_MESSAGE(
        wolf_level_load(
            maphead_path, gamemaps_path, 0u,
            &level, error_message, sizeof(error_message)),
        error_message);

    for (size_t tile_index = 0u;
         tile_index < WOLF_LEVEL_PLANE_WORDS;
         ++tile_index) {
        TEST_ASSERT_EQUAL_UINT16(0u, level.wall_tiles[tile_index]);
    }

    remove(maphead_path);
    remove(gamemaps_path);
}

void test_level_loader_rejects_sparse_map_entries(void)
{
    char maphead_path[1024];
    char gamemaps_path[1024];
    WolfLevel level = {0};
    char error_message[256];

    TEST_ASSERT_TRUE(create_test_map_files(
        maphead_path, sizeof(maphead_path), gamemaps_path, sizeof(gamemaps_path), 0u));

    TEST_ASSERT_FALSE(wolf_level_load(
        maphead_path, gamemaps_path, 1u,
        &level, error_message, sizeof(error_message)));
    TEST_ASSERT_TRUE(strstr(error_message, "sparse") != NULL);

    remove(maphead_path);
    remove(gamemaps_path);
}

void test_level_loader_preserves_original_player_start_marker_in_info_plane(void)
{
    char maphead_path[1024];
    char gamemaps_path[1024];
    WolfLevel level = {0};
    WolfPlayerStart player_start = {0};
    char error_message[256];
    FILE *file = NULL;
    uint8_t info_plane_stream[4u + WOLF_LEVEL_PLANE_WORDS * 2u];
    size_t stream_size = 0u;

    TEST_ASSERT_TRUE(create_test_map_files(
        maphead_path, sizeof(maphead_path),
        gamemaps_path, sizeof(gamemaps_path), 0u));

    /* Build a literal Carmack stream containing an RLEW info plane and marker 19 at (4, 0). */
    write_u16_le(info_plane_stream,
        (uint16_t)(WOLF_LEVEL_PLANE_WORDS * 2u + 2u));
    write_u16_le(info_plane_stream + 2u,
        (uint16_t)(WOLF_LEVEL_PLANE_WORDS * 2u));
    stream_size = 4u;
    for (size_t index = 0u; index < WOLF_LEVEL_PLANE_WORDS; ++index) {
        write_u16_le(info_plane_stream + stream_size, index == 4u ? 19u : 0u);
        stream_size += 2u;
    }

#if defined(_MSC_VER)
    TEST_ASSERT_EQUAL_INT(0, fopen_s(&file, gamemaps_path, "r+b"));
#else
    file = fopen(gamemaps_path, "r+b");
    TEST_ASSERT_NOT_NULL(file);
#endif
    TEST_ASSERT_NOT_NULL(file);
    /* Plane 1 is stored after the map header and compressed wall plane. */
    TEST_ASSERT_EQUAL_INT(0, fseek(file,
        (long)TEST_MAP_HEADER_SIZE + (long)TEST_PLANE_CHUNK_SIZE, SEEK_SET));
    TEST_ASSERT_EQUAL_UINT(stream_size, fwrite(info_plane_stream, 1u, stream_size, file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));

    /* Update the second plane's compressed byte length in the map header. */
#if defined(_MSC_VER)
    TEST_ASSERT_EQUAL_INT(0, fopen_s(&file, gamemaps_path, "r+b"));
#else
    file = fopen(gamemaps_path, "r+b");
    TEST_ASSERT_NOT_NULL(file);
#endif
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 14L, SEEK_SET));
    {
        uint8_t length_bytes[2];
        write_u16_le(length_bytes, (uint16_t)stream_size);
        TEST_ASSERT_EQUAL_UINT(sizeof(length_bytes),
            fwrite(length_bytes, 1u, sizeof(length_bytes), file));
    }
    TEST_ASSERT_EQUAL_INT(0, fclose(file));

    TEST_ASSERT_TRUE_MESSAGE(wolf_level_load(
        maphead_path, gamemaps_path, 0u,
        &level, error_message, sizeof(error_message)), error_message);
    TEST_ASSERT_EQUAL_UINT16(19u, level.object_tiles[4u]);
    TEST_ASSERT_TRUE(wolf_level_find_player_start(&level, &player_start));
    TEST_ASSERT_EQUAL_UINT16(4u, player_start.tile_x);
    TEST_ASSERT_EQUAL_UINT16(0u, player_start.tile_y);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, player_start.yaw_radians);

    remove(maphead_path);
    remove(gamemaps_path);
}


void test_level_loader_converts_ambush_markers_to_open_cells(void)
{
    char maphead_path[1024];
    char gamemaps_path[1024];
    WolfLevel level = {0};
    char error_message[256];

    TEST_ASSERT_TRUE(create_test_map_files(
        maphead_path, sizeof(maphead_path), gamemaps_path, sizeof(gamemaps_path),
        (uint16_t)WOLF_LEVEL_AMBUSH_TILE));

    TEST_ASSERT_TRUE_MESSAGE(
        wolf_level_load(
            maphead_path, gamemaps_path, 0u,
            &level, error_message, sizeof(error_message)),
        error_message);

    for (size_t tile_index = 0u;
         tile_index < WOLF_LEVEL_PLANE_WORDS;
         ++tile_index) {
        TEST_ASSERT_EQUAL_UINT16(0u, level.wall_tiles[tile_index]);
    }

    remove(maphead_path);
    remove(gamemaps_path);
}
