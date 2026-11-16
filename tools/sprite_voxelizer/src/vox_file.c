#include "vox_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOX_VERSION 150u
#define VOX_CHUNK_HEADER_SIZE 12u

uint8_t wolf_vox_colour_for_palette_index(const uint8_t *palette_rgba, uint8_t palette_index)
{
    const uint8_t *target;
    uint32_t best_distance = UINT32_MAX;
    uint8_t best_index = 0u;

    if (palette_index < 255u) {
        return (uint8_t)(palette_index + 1u);
    }

    /* Entry 255 has no MagicaVoxel slot; substitute the closest remaining colour. */
    target = palette_rgba + 255u * 4u;
    for (uint32_t candidate = 0u; candidate < 255u; ++candidate) {
        const uint8_t *colour = palette_rgba + candidate * 4u;
        int32_t red = (int32_t)colour[0] - (int32_t)target[0];
        int32_t green = (int32_t)colour[1] - (int32_t)target[1];
        int32_t blue = (int32_t)colour[2] - (int32_t)target[2];
        uint32_t distance = (uint32_t)(red * red + green * green + blue * blue);

        if (distance < best_distance) {
            best_distance = distance;
            best_index = (uint8_t)candidate;
        }
    }
    return (uint8_t)(best_index + 1u);
}

/* Start a chunk and return the offset of its content-size field for back-filling. */
static size_t begin_chunk(WolfByteBuffer *buffer, const char *identifier)
{
    size_t size_offset;

    wolf_buffer_append(buffer, identifier, 4u);
    size_offset = buffer->size;
    wolf_buffer_append_u32_le(buffer, 0u);
    wolf_buffer_append_u32_le(buffer, 0u);
    return size_offset;
}

/* Every chunk except MAIN has no children, so only its content size is patched. */
static void end_chunk(WolfByteBuffer *buffer, size_t size_offset)
{
    wolf_buffer_patch_u32_le(buffer, size_offset, (uint32_t)(buffer->size - size_offset - 8u));
}

static void append_string(WolfByteBuffer *buffer, const char *text)
{
    size_t length = strlen(text);

    wolf_buffer_append_u32_le(buffer, (uint32_t)length);
    wolf_buffer_append(buffer, text, length);
}

static void append_model(WolfByteBuffer *buffer, const WolfVoxelModel *model, const uint8_t *palette_rgba)
{
    size_t chunk;
    size_t count_offset;
    uint32_t voxel_count = 0u;

    chunk = begin_chunk(buffer, "SIZE");
    wolf_buffer_append_u32_le(buffer, WOLF_VOXEL_DIM);
    wolf_buffer_append_u32_le(buffer, WOLF_VOXEL_DIM);
    wolf_buffer_append_u32_le(buffer, WOLF_VOXEL_DIM);
    end_chunk(buffer, chunk);

    chunk = begin_chunk(buffer, "XYZI");
    count_offset = buffer->size;
    wolf_buffer_append_u32_le(buffer, 0u);
    for (int z = 0; z < WOLF_VOXEL_DIM; ++z) {
        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                size_t voxel = wolf_voxel_index(x, y, z);
                uint8_t entry[4];

                if (!model->occupied[voxel]) {
                    continue;
                }
                entry[0] = (uint8_t)x;
                entry[1] = (uint8_t)y;
                entry[2] = (uint8_t)z;
                entry[3] = wolf_vox_colour_for_palette_index(palette_rgba, model->palette_index[voxel]);
                wolf_buffer_append(buffer, entry, sizeof(entry));
                ++voxel_count;
            }
        }
    }
    wolf_buffer_patch_u32_le(buffer, count_offset, voxel_count);
    end_chunk(buffer, chunk);
}

/*
 * Scene graph for an animated object: root transform -> group -> transform
 * -> one shape node whose models are the keyframes 0..N-1.
 */
static void append_animation_scene(WolfByteBuffer *buffer, size_t frame_count)
{
    size_t chunk;

    chunk = begin_chunk(buffer, "nTRN");
    wolf_buffer_append_u32_le(buffer, 0u);
    wolf_buffer_append_u32_le(buffer, 0u);
    wolf_buffer_append_u32_le(buffer, 1u);
    wolf_buffer_append_u32_le(buffer, UINT32_MAX);
    wolf_buffer_append_u32_le(buffer, UINT32_MAX);
    wolf_buffer_append_u32_le(buffer, 1u);
    wolf_buffer_append_u32_le(buffer, 0u);
    end_chunk(buffer, chunk);

    chunk = begin_chunk(buffer, "nGRP");
    wolf_buffer_append_u32_le(buffer, 1u);
    wolf_buffer_append_u32_le(buffer, 0u);
    wolf_buffer_append_u32_le(buffer, 1u);
    wolf_buffer_append_u32_le(buffer, 2u);
    end_chunk(buffer, chunk);

    chunk = begin_chunk(buffer, "nTRN");
    wolf_buffer_append_u32_le(buffer, 2u);
    wolf_buffer_append_u32_le(buffer, 0u);
    wolf_buffer_append_u32_le(buffer, 3u);
    wolf_buffer_append_u32_le(buffer, UINT32_MAX);
    wolf_buffer_append_u32_le(buffer, 0u);
    wolf_buffer_append_u32_le(buffer, 1u);
    wolf_buffer_append_u32_le(buffer, 0u);
    end_chunk(buffer, chunk);

    chunk = begin_chunk(buffer, "nSHP");
    wolf_buffer_append_u32_le(buffer, 3u);
    wolf_buffer_append_u32_le(buffer, 0u);
    wolf_buffer_append_u32_le(buffer, (uint32_t)frame_count);
    for (size_t frame = 0u; frame < frame_count; ++frame) {
        char frame_text[16];

        (void)snprintf(frame_text, sizeof(frame_text), "%u", (unsigned)frame);
        wolf_buffer_append_u32_le(buffer, (uint32_t)frame);
        wolf_buffer_append_u32_le(buffer, 1u);
        append_string(buffer, "_f");
        append_string(buffer, frame_text);
    }
    end_chunk(buffer, chunk);
}

bool wolf_vox_encode(
    const WolfVoxelModel *const *frames,
    size_t frame_count,
    const uint8_t *palette_rgba,
    WolfByteBuffer *output)
{
    size_t main_children_offset;
    size_t chunk;

    if (frames == NULL || frame_count == 0u || palette_rgba == NULL || output == NULL) {
        return false;
    }

    wolf_buffer_append(output, "VOX ", 4u);
    wolf_buffer_append_u32_le(output, VOX_VERSION);
    wolf_buffer_append(output, "MAIN", 4u);
    wolf_buffer_append_u32_le(output, 0u);
    main_children_offset = output->size;
    wolf_buffer_append_u32_le(output, 0u);

    for (size_t frame = 0u; frame < frame_count; ++frame) {
        append_model(output, frames[frame], palette_rgba);
    }
    if (frame_count > 1u) {
        append_animation_scene(output, frame_count);
    }

    chunk = begin_chunk(output, "RGBA");
    for (uint32_t entry = 0u; entry < 256u; ++entry) {
        /* Entry n describes colour n+1, which holds Wolfenstein palette entry n. */
        uint8_t colour[4] = {0u, 0u, 0u, 255u};

        if (entry < 255u) {
            memcpy(colour, palette_rgba + entry * 4u, 3u);
        }
        wolf_buffer_append(output, colour, sizeof(colour));
    }
    end_chunk(output, chunk);

    wolf_buffer_patch_u32_le(output, main_children_offset,
        (uint32_t)(output->size - main_children_offset - 4u));
    return !output->failed;
}

bool wolf_vox_write_file(
    const char *path,
    const WolfVoxelModel *const *frames,
    size_t frame_count,
    const uint8_t *palette_rgba)
{
    WolfByteBuffer buffer = {0};
    bool written = false;

    if (wolf_vox_encode(frames, frame_count, palette_rgba, &buffer)) {
        written = wolf_write_entire_file(path, buffer.data, buffer.size);
    }
    wolf_buffer_free(&buffer);
    return written;
}

/* Read a length-prefixed string and report whether it equals the expected key. */
static bool read_string(const uint8_t *data, size_t end, size_t *cursor, const uint8_t **text, uint32_t *length)
{
    if (*cursor + 4u > end) {
        return false;
    }
    *length = wolf_read_u32_le(data + *cursor);
    *cursor += 4u;
    if (*length > end - *cursor) {
        return false;
    }
    *text = data + *cursor;
    *cursor += *length;
    return true;
}

/* Skip a dictionary, extracting the integer "_f" value if one is present. */
static bool read_dictionary(const uint8_t *data, size_t end, size_t *cursor, int32_t *frame_value)
{
    uint32_t pair_count;

    if (*cursor + 4u > end) {
        return false;
    }
    pair_count = wolf_read_u32_le(data + *cursor);
    *cursor += 4u;
    for (uint32_t pair = 0u; pair < pair_count; ++pair) {
        const uint8_t *key;
        const uint8_t *value;
        uint32_t key_length;
        uint32_t value_length;

        if (!read_string(data, end, cursor, &key, &key_length) ||
            !read_string(data, end, cursor, &value, &value_length)) {
            return false;
        }
        if (frame_value != NULL && key_length == 2u && key[0] == '_' && key[1] == 'f' && value_length < 16u) {
            char text[16];

            memcpy(text, value, value_length);
            text[value_length] = '\0';
            *frame_value = (int32_t)strtol(text, NULL, 10);
        }
    }
    return true;
}

/* Record keyframe indices from a shape node so animated files can be checked. */
static bool read_shape_node(const uint8_t *data, size_t end, WolfVoxDocument *document)
{
    size_t cursor = 0u;
    uint32_t model_count;

    cursor += 4u;
    if (!read_dictionary(data, end, &cursor, NULL) || cursor + 4u > end) {
        return false;
    }
    model_count = wolf_read_u32_le(data + cursor);
    cursor += 4u;
    for (uint32_t model = 0u; model < model_count; ++model) {
        uint32_t model_id;
        int32_t frame_value = -1;

        if (cursor + 4u > end) {
            return false;
        }
        model_id = wolf_read_u32_le(data + cursor);
        cursor += 4u;
        if (!read_dictionary(data, end, &cursor, &frame_value)) {
            return false;
        }
        if (model_id < document->model_count) {
            document->frame_indices[model_id] = frame_value;
        }
    }
    return true;
}

bool wolf_vox_decode(
    const uint8_t *data,
    size_t size,
    WolfVoxDocument *document,
    char *error_message,
    size_t error_message_size)
{
    size_t cursor;
    size_t main_end;
    size_t model_capacity = 0u;
    uint32_t pending_size[3] = {0u, 0u, 0u};
    bool have_size = false;
    /* Shape nodes may precede nothing useful until models are known, so remember them. */
    const uint8_t *shape_nodes[64];
    size_t shape_node_sizes[64];
    size_t shape_node_count = 0u;

    memset(document, 0, sizeof(*document));
    if (data == NULL || size < 20u || memcmp(data, "VOX ", 4u) != 0 ||
        memcmp(data + 8u, "MAIN", 4u) != 0) {
        wolf_set_error(error_message, error_message_size, "Not a MagicaVoxel file.");
        return false;
    }
    cursor = 20u + wolf_read_u32_le(data + 12u);
    main_end = cursor + wolf_read_u32_le(data + 16u);
    if (cursor > size || main_end > size || main_end < cursor) {
        wolf_set_error(error_message, error_message_size, "MAIN chunk is truncated.");
        return false;
    }

    while (cursor + VOX_CHUNK_HEADER_SIZE <= main_end) {
        const uint8_t *identifier = data + cursor;
        uint32_t content_size = wolf_read_u32_le(data + cursor + 4u);
        uint32_t children_size = wolf_read_u32_le(data + cursor + 8u);
        const uint8_t *content = data + cursor + VOX_CHUNK_HEADER_SIZE;

        if (content_size > main_end - cursor - VOX_CHUNK_HEADER_SIZE ||
            children_size > main_end - cursor - VOX_CHUNK_HEADER_SIZE - content_size) {
            wolf_vox_document_destroy(document);
            wolf_set_error(error_message, error_message_size, "A chunk is truncated.");
            return false;
        }

        if (memcmp(identifier, "SIZE", 4u) == 0 && content_size >= 12u) {
            pending_size[0] = wolf_read_u32_le(content);
            pending_size[1] = wolf_read_u32_le(content + 4u);
            pending_size[2] = wolf_read_u32_le(content + 8u);
            have_size = true;
        } else if (memcmp(identifier, "XYZI", 4u) == 0 && content_size >= 4u) {
            uint32_t voxel_count = wolf_read_u32_le(content);
            WolfVoxelModel *model;

            if (!have_size || pending_size[0] > WOLF_VOXEL_DIM || pending_size[1] > WOLF_VOXEL_DIM ||
                pending_size[2] > WOLF_VOXEL_DIM || voxel_count > (content_size - 4u) / 4u) {
                wolf_vox_document_destroy(document);
                wolf_set_error(error_message, error_message_size, "Model is too large or malformed.");
                return false;
            }
            if (document->model_count == model_capacity) {
                size_t new_capacity = model_capacity == 0u ? 8u : model_capacity * 2u;
                WolfVoxelModel **models = realloc(document->models, new_capacity * sizeof(*models));
                int32_t *frames = realloc(document->frame_indices, new_capacity * sizeof(*frames));

                if (models != NULL) {
                    document->models = models;
                }
                if (frames != NULL) {
                    document->frame_indices = frames;
                }
                if (models == NULL || frames == NULL) {
                    wolf_vox_document_destroy(document);
                    wolf_set_error(error_message, error_message_size, "Out of memory reading models.");
                    return false;
                }
                model_capacity = new_capacity;
            }
            model = wolf_voxel_model_create();
            if (model == NULL) {
                wolf_vox_document_destroy(document);
                wolf_set_error(error_message, error_message_size, "Out of memory reading a model.");
                return false;
            }
            for (uint32_t entry = 0u; entry < voxel_count; ++entry) {
                const uint8_t *voxel = content + 4u + (size_t)entry * 4u;

                if (voxel[0] >= pending_size[0] || voxel[1] >= pending_size[1] ||
                    voxel[2] >= pending_size[2] || voxel[3] == 0u) {
                    continue;
                }
                model->occupied[wolf_voxel_index(voxel[0], voxel[1], voxel[2])] = 1u;
                model->palette_index[wolf_voxel_index(voxel[0], voxel[1], voxel[2])] = (uint8_t)(voxel[3] - 1u);
            }
            document->frame_indices[document->model_count] = -1;
            document->models[document->model_count++] = model;
            have_size = false;
        } else if (memcmp(identifier, "RGBA", 4u) == 0 && content_size >= 1024u) {
            /* Store chunk entry n at palette slot n so slot = colour index - 1. */
            memcpy(document->palette_rgba, content, 1024u);
        } else if (memcmp(identifier, "nSHP", 4u) == 0 && shape_node_count < 64u) {
            shape_nodes[shape_node_count] = content;
            shape_node_sizes[shape_node_count] = content_size;
            ++shape_node_count;
        }
        cursor += VOX_CHUNK_HEADER_SIZE + content_size + children_size;
    }

    for (size_t node = 0u; node < shape_node_count; ++node) {
        if (!read_shape_node(shape_nodes[node], shape_node_sizes[node], document)) {
            wolf_vox_document_destroy(document);
            wolf_set_error(error_message, error_message_size, "Shape node is malformed.");
            return false;
        }
    }
    if (document->model_count == 0u) {
        wolf_vox_document_destroy(document);
        wolf_set_error(error_message, error_message_size, "The file contains no models.");
        return false;
    }
    return true;
}

bool wolf_vox_read_file(
    const char *path,
    WolfVoxDocument *document,
    char *error_message,
    size_t error_message_size)
{
    uint8_t *data;
    size_t size;
    bool decoded;

    memset(document, 0, sizeof(*document));
    if (!wolf_read_entire_file(path, &data, &size)) {
        wolf_set_error(error_message, error_message_size, "Could not read the VOX file.");
        return false;
    }
    decoded = wolf_vox_decode(data, size, document, error_message, error_message_size);
    free(data);
    return decoded;
}

void wolf_vox_document_destroy(WolfVoxDocument *document)
{
    for (size_t model = 0u; model < document->model_count; ++model) {
        wolf_voxel_model_destroy(document->models[model]);
    }
    free(document->models);
    free(document->frame_indices);
    memset(document, 0, sizeof(*document));
}
