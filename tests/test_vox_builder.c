#include "test_vox_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool append(TestVoxBytes *bytes, const void *data, size_t size)
{
    if (bytes->size + size > bytes->capacity) {
        size_t capacity = bytes->capacity == 0u ? 1024u : bytes->capacity;
        uint8_t *grown;

        while (capacity < bytes->size + size) {
            capacity *= 2u;
        }
        grown = realloc(bytes->data, capacity);
        if (grown == NULL) {
            return false;
        }
        bytes->data = grown;
        bytes->capacity = capacity;
    }
    memcpy(bytes->data + bytes->size, data, size);
    bytes->size += size;
    return true;
}

static bool append_u32(TestVoxBytes *bytes, uint32_t value)
{
    const uint8_t encoded[4] = {
        (uint8_t)value, (uint8_t)(value >> 8u), (uint8_t)(value >> 16u), (uint8_t)(value >> 24u)
    };

    return append(bytes, encoded, sizeof(encoded));
}

static void patch_u32(TestVoxBytes *bytes, size_t offset, uint32_t value)
{
    bytes->data[offset] = (uint8_t)value;
    bytes->data[offset + 1u] = (uint8_t)(value >> 8u);
    bytes->data[offset + 2u] = (uint8_t)(value >> 16u);
    bytes->data[offset + 3u] = (uint8_t)(value >> 24u);
}

static bool append_string(TestVoxBytes *bytes, const char *text)
{
    return append_u32(bytes, (uint32_t)strlen(text)) && append(bytes, text, strlen(text));
}

/* Write a chunk header now and return where its content size must be patched. */
static bool begin_chunk(TestVoxBytes *bytes, const char *identifier, size_t *size_offset)
{
    if (!append(bytes, identifier, 4u)) {
        return false;
    }
    *size_offset = bytes->size;
    return append_u32(bytes, 0u) && append_u32(bytes, 0u);
}

static void end_chunk(TestVoxBytes *bytes, size_t size_offset)
{
    patch_u32(bytes, size_offset, (uint32_t)(bytes->size - size_offset - 8u));
}

static bool append_models(TestVoxBytes *bytes, const TestVoxAnimation *animation, uint32_t model_size)
{
    for (uint32_t pose = 0u; pose < animation->pose_count; ++pose) {
        size_t chunk;
        bool written = begin_chunk(bytes, "SIZE", &chunk) &&
            append_u32(bytes, model_size) && append_u32(bytes, model_size) && append_u32(bytes, model_size);

        if (!written) {
            return false;
        }
        end_chunk(bytes, chunk);
        if (!begin_chunk(bytes, "XYZI", &chunk) ||
            !append_u32(bytes, animation->pose_voxel_counts[pose])) {
            return false;
        }
        for (uint32_t voxel = 0u; voxel < animation->pose_voxel_counts[pose]; ++voxel) {
            const WolfVoxVoxel *entry = &animation->pose_voxels[pose][voxel];
            const uint8_t encoded[4] = {entry->x, entry->y, entry->z, entry->colour_index};

            if (!append(bytes, encoded, sizeof(encoded))) {
                return false;
            }
        }
        end_chunk(bytes, chunk);
    }
    return true;
}

static bool append_scene(TestVoxBytes *bytes, const TestVoxAnimation *animations, uint32_t animation_count)
{
    size_t chunk;
    uint32_t next_model = 0u;

    /* Root transform 0 -> group 1 -> (named transform 2 + 2i -> shape 3 + 2i) per animation. */
    if (!begin_chunk(bytes, "nTRN", &chunk) || !append_u32(bytes, 0u) || !append_u32(bytes, 0u) ||
        !append_u32(bytes, 1u) || !append_u32(bytes, UINT32_MAX) || !append_u32(bytes, UINT32_MAX) ||
        !append_u32(bytes, 1u) || !append_u32(bytes, 0u)) {
        return false;
    }
    end_chunk(bytes, chunk);

    if (!begin_chunk(bytes, "nGRP", &chunk) || !append_u32(bytes, 1u) || !append_u32(bytes, 0u) ||
        !append_u32(bytes, animation_count)) {
        return false;
    }
    for (uint32_t animation = 0u; animation < animation_count; ++animation) {
        if (!append_u32(bytes, 2u + 2u * animation)) {
            return false;
        }
    }
    end_chunk(bytes, chunk);

    for (uint32_t animation = 0u; animation < animation_count; ++animation) {
        if (!begin_chunk(bytes, "nTRN", &chunk) || !append_u32(bytes, 2u + 2u * animation) ||
            !append_u32(bytes, 1u) || !append_string(bytes, "_name") ||
            !append_string(bytes, animations[animation].name) ||
            !append_u32(bytes, 3u + 2u * animation) || !append_u32(bytes, UINT32_MAX) ||
            !append_u32(bytes, 0u) || !append_u32(bytes, 1u) || !append_u32(bytes, 0u)) {
            return false;
        }
        end_chunk(bytes, chunk);

        if (!begin_chunk(bytes, "nSHP", &chunk) || !append_u32(bytes, 3u + 2u * animation) ||
            !append_u32(bytes, 0u) || !append_u32(bytes, animations[animation].pose_count)) {
            return false;
        }
        for (uint32_t pose = 0u; pose < animations[animation].pose_count; ++pose) {
            char keyframe[16];

            (void)snprintf(keyframe, sizeof(keyframe), "%u", (unsigned int)pose);
            if (!append_u32(bytes, next_model++) || !append_u32(bytes, 1u) ||
                !append_string(bytes, "_f") || !append_string(bytes, keyframe)) {
                return false;
            }
        }
        end_chunk(bytes, chunk);
    }
    return true;
}

bool test_vox_build(
    const TestVoxAnimation *animations,
    uint32_t animation_count,
    uint32_t model_size,
    bool include_palette,
    TestVoxBytes *bytes)
{
    size_t children_offset;

    memset(bytes, 0, sizeof(*bytes));
    if (!append(bytes, "VOX ", 4u) || !append_u32(bytes, 150u) || !append(bytes, "MAIN", 4u) ||
        !append_u32(bytes, 0u)) {
        return false;
    }
    children_offset = bytes->size;
    if (!append_u32(bytes, 0u)) {
        return false;
    }

    for (uint32_t animation = 0u; animation < animation_count; ++animation) {
        if (!append_models(bytes, &animations[animation], model_size)) {
            return false;
        }
    }
    if (!append_scene(bytes, animations, animation_count)) {
        return false;
    }
    if (include_palette) {
        size_t chunk;

        if (!begin_chunk(bytes, "RGBA", &chunk)) {
            return false;
        }
        for (uint32_t entry = 0u; entry < 256u; ++entry) {
            const uint8_t colour[4] = {(uint8_t)entry, (uint8_t)(255u - entry), 7u, 255u};

            if (!append(bytes, colour, sizeof(colour))) {
                return false;
            }
        }
        end_chunk(bytes, chunk);
    }

    patch_u32(bytes, children_offset, (uint32_t)(bytes->size - children_offset - 4u));
    return true;
}

bool test_vox_write_file(
    const char *path,
    const TestVoxAnimation *animations,
    uint32_t animation_count)
{
    TestVoxBytes bytes;
    FILE *file = NULL;
    bool written = false;

    if (!test_vox_build(animations, animation_count, 64u, true, &bytes)) {
        test_vox_bytes_free(&bytes);
        return false;
    }
#if defined(_MSC_VER)
    if (fopen_s(&file, path, "wb") != 0) {
        file = NULL;
    }
#else
    file = fopen(path, "wb");
#endif
    if (file != NULL) {
        written = fwrite(bytes.data, 1u, bytes.size, file) == bytes.size;
        written = fclose(file) == 0 && written;
    }
    test_vox_bytes_free(&bytes);
    return written;
}

void test_vox_bytes_free(TestVoxBytes *bytes)
{
    free(bytes->data);
    memset(bytes, 0, sizeof(*bytes));
}
