#include "vox_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOX_VERSION 150u
#define VOX_CHUNK_HEADER_SIZE 12u
#define MAX_SCENE_NODES 256

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
 * Scene graph: root transform 0 -> group 1 -> for each object a named
 * transform (2 + 2i) -> shape (3 + 2i) listing that object's keyframes.
 */
static void append_scene(WolfByteBuffer *buffer, const WolfVoxObject *objects, size_t object_count)
{
    size_t chunk;
    uint32_t next_model = 0u;

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
    wolf_buffer_append_u32_le(buffer, (uint32_t)object_count);
    for (size_t object = 0u; object < object_count; ++object) {
        wolf_buffer_append_u32_le(buffer, (uint32_t)(2u + 2u * object));
    }
    end_chunk(buffer, chunk);

    for (size_t object = 0u; object < object_count; ++object) {
        char translation[48];
        long offset = (long)(((double)object - (double)(object_count - 1u) * 0.5) * WOLF_VOX_OBJECT_SPACING);

        (void)snprintf(translation, sizeof(translation), "%ld 0 %d", offset, WOLF_VOXEL_DIM / 2);
        chunk = begin_chunk(buffer, "nTRN");
        wolf_buffer_append_u32_le(buffer, (uint32_t)(2u + 2u * object));
        wolf_buffer_append_u32_le(buffer, 1u);
        append_string(buffer, "_name");
        append_string(buffer, objects[object].name);
        wolf_buffer_append_u32_le(buffer, (uint32_t)(3u + 2u * object));
        wolf_buffer_append_u32_le(buffer, UINT32_MAX);
        wolf_buffer_append_u32_le(buffer, 0u);
        wolf_buffer_append_u32_le(buffer, 1u);
        wolf_buffer_append_u32_le(buffer, 1u);
        append_string(buffer, "_t");
        append_string(buffer, translation);
        end_chunk(buffer, chunk);

        chunk = begin_chunk(buffer, "nSHP");
        wolf_buffer_append_u32_le(buffer, (uint32_t)(3u + 2u * object));
        wolf_buffer_append_u32_le(buffer, 0u);
        wolf_buffer_append_u32_le(buffer, (uint32_t)objects[object].frame_count);
        for (size_t frame = 0u; frame < objects[object].frame_count; ++frame) {
            char frame_text[16];

            (void)snprintf(frame_text, sizeof(frame_text), "%u", (unsigned)frame);
            wolf_buffer_append_u32_le(buffer, next_model++);
            wolf_buffer_append_u32_le(buffer, 1u);
            append_string(buffer, "_f");
            append_string(buffer, frame_text);
        }
        end_chunk(buffer, chunk);
    }
}

bool wolf_vox_encode_objects(
    const WolfVoxObject *objects,
    size_t object_count,
    const uint8_t *palette_rgba,
    WolfByteBuffer *output)
{
    size_t main_children_offset;
    size_t chunk;

    if (objects == NULL || object_count == 0u || palette_rgba == NULL || output == NULL) {
        return false;
    }
    for (size_t object = 0u; object < object_count; ++object) {
        if (objects[object].name == NULL || objects[object].frames == NULL || objects[object].frame_count == 0u) {
            return false;
        }
    }

    wolf_buffer_append(output, "VOX ", 4u);
    wolf_buffer_append_u32_le(output, VOX_VERSION);
    wolf_buffer_append(output, "MAIN", 4u);
    wolf_buffer_append_u32_le(output, 0u);
    main_children_offset = output->size;
    wolf_buffer_append_u32_le(output, 0u);

    for (size_t object = 0u; object < object_count; ++object) {
        for (size_t frame = 0u; frame < objects[object].frame_count; ++frame) {
            append_model(output, objects[object].frames[frame], palette_rgba);
        }
    }
    append_scene(output, objects, object_count);

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

bool wolf_vox_write_objects_file(
    const char *path,
    const WolfVoxObject *objects,
    size_t object_count,
    const uint8_t *palette_rgba)
{
    WolfByteBuffer buffer = {0};
    bool written = false;

    if (wolf_vox_encode_objects(objects, object_count, palette_rgba, &buffer)) {
        written = wolf_write_entire_file(path, buffer.data, buffer.size);
    }
    wolf_buffer_free(&buffer);
    return written;
}

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

/*
 * Read a dictionary, extracting the "_f" integer and the "_name" string when
 * the caller asks for them.
 */
static bool read_dictionary(const uint8_t *data, size_t end, size_t *cursor, int32_t *frame_value,
    char *name, size_t name_size)
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
        if (frame_value != NULL && key_length == 2u && memcmp(key, "_f", 2u) == 0 && value_length < 16u) {
            char text[16];

            memcpy(text, value, value_length);
            text[value_length] = '\0';
            *frame_value = (int32_t)strtol(text, NULL, 10);
        }
        if (name != NULL && key_length == 5u && memcmp(key, "_name", 5u) == 0) {
            size_t copied = value_length < name_size - 1u ? value_length : name_size - 1u;

            memcpy(name, value, copied);
            name[copied] = '\0';
        }
    }
    return true;
}

/* A transform node's name and the node it points to. */
typedef struct TransformNode {
    uint32_t node_id;
    uint32_t child_id;
    char name[64];
} TransformNode;

static bool read_transform_node(const uint8_t *data, size_t end, TransformNode *node)
{
    size_t cursor = 4u;

    if (end < 4u) {
        return false;
    }
    node->node_id = wolf_read_u32_le(data);
    node->name[0] = '\0';
    if (!read_dictionary(data, end, &cursor, NULL, node->name, sizeof(node->name)) || cursor + 4u > end) {
        return false;
    }
    node->child_id = wolf_read_u32_le(data + cursor);
    return true;
}

/* Record a shape node's models and keyframes as one document object. */
static bool read_shape_node(const uint8_t *data, size_t end, WolfVoxDocumentObject *object, uint32_t *node_id)
{
    size_t cursor = 4u;
    uint32_t model_count;

    if (end < 4u) {
        return false;
    }
    *node_id = wolf_read_u32_le(data);
    if (!read_dictionary(data, end, &cursor, NULL, NULL, 0u) || cursor + 4u > end) {
        return false;
    }
    model_count = wolf_read_u32_le(data + cursor);
    cursor += 4u;
    object->frame_count = 0u;
    for (uint32_t model = 0u; model < model_count; ++model) {
        int32_t frame_value = -1;
        uint32_t model_id;

        if (cursor + 4u > end) {
            return false;
        }
        model_id = wolf_read_u32_le(data + cursor);
        cursor += 4u;
        if (!read_dictionary(data, end, &cursor, &frame_value, NULL, 0u)) {
            return false;
        }
        if (object->frame_count < WOLF_VOX_MAX_OBJECT_FRAMES) {
            object->model_ids[object->frame_count] = model_id;
            object->frame_indices[object->frame_count] = frame_value;
            ++object->frame_count;
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
    TransformNode transforms[MAX_SCENE_NODES];
    size_t transform_count = 0u;
    uint32_t shape_ids[WOLF_VOX_MAX_OBJECTS];

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

                if (models == NULL) {
                    wolf_vox_document_destroy(document);
                    wolf_set_error(error_message, error_message_size, "Out of memory reading models.");
                    return false;
                }
                document->models = models;
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
            document->models[document->model_count++] = model;
            have_size = false;
        } else if (memcmp(identifier, "RGBA", 4u) == 0 && content_size >= 1024u) {
            /* Store chunk entry n at palette slot n so slot = colour index - 1. */
            memcpy(document->palette_rgba, content, 1024u);
        } else if (memcmp(identifier, "nTRN", 4u) == 0 && transform_count < MAX_SCENE_NODES) {
            if (!read_transform_node(content, content_size, &transforms[transform_count])) {
                wolf_vox_document_destroy(document);
                wolf_set_error(error_message, error_message_size, "Transform node is malformed.");
                return false;
            }
            ++transform_count;
        } else if (memcmp(identifier, "nSHP", 4u) == 0 && document->object_count < WOLF_VOX_MAX_OBJECTS) {
            if (!read_shape_node(content, content_size, &document->objects[document->object_count],
                    &shape_ids[document->object_count])) {
                wolf_vox_document_destroy(document);
                wolf_set_error(error_message, error_message_size, "Shape node is malformed.");
                return false;
            }
            ++document->object_count;
        }
        cursor += VOX_CHUNK_HEADER_SIZE + content_size + children_size;
    }

    /* Each object is named by the transform node that points at its shape. */
    for (size_t object = 0u; object < document->object_count; ++object) {
        for (size_t transform = 0u; transform < transform_count; ++transform) {
            if (transforms[transform].child_id == shape_ids[object]) {
                (void)snprintf(document->objects[object].name, sizeof(document->objects[object].name),
                    "%s", transforms[transform].name);
            }
        }
        for (size_t frame = 0u; frame < document->objects[object].frame_count; ++frame) {
            if (document->objects[object].model_ids[frame] >= document->model_count) {
                wolf_vox_document_destroy(document);
                wolf_set_error(error_message, error_message_size, "A shape refers to a missing model.");
                return false;
            }
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
    memset(document, 0, sizeof(*document));
}
