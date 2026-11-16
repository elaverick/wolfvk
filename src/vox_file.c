#include "wolfvk/vox_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOX_CHUNK_HEADER_SIZE 12u
#define VOX_FILE_HEADER_SIZE 8u
#define VOX_MAX_TRANSFORM_NODES 256u

/* Keep failures readable for callers without requiring a diagnostic buffer. */
static void write_error(char *error_message, size_t error_message_size, const char *message)
{
    if (error_message != NULL && error_message_size > 0u) {
        (void)snprintf(error_message, error_message_size, "%s", message);
    }
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8u) |
        ((uint32_t)bytes[2] << 16u) |
        ((uint32_t)bytes[3] << 24u);
}

/* A bounded reader over one chunk's content, so malformed lengths cannot run past it. */
typedef struct ChunkReader {
    const uint8_t *data;
    size_t size;
    size_t cursor;
} ChunkReader;

static bool reader_u32(ChunkReader *reader, uint32_t *value)
{
    if (reader->size - reader->cursor < 4u) {
        return false;
    }
    *value = read_u32_le(reader->data + reader->cursor);
    reader->cursor += 4u;
    return true;
}

static bool reader_string(ChunkReader *reader, const uint8_t **text, uint32_t *length)
{
    if (!reader_u32(reader, length) || *length > reader->size - reader->cursor) {
        return false;
    }
    *text = reader->data + reader->cursor;
    reader->cursor += *length;
    return true;
}

static bool string_equals(const uint8_t *text, uint32_t length, const char *expected)
{
    size_t expected_length = strlen(expected);

    return length == expected_length && memcmp(text, expected, expected_length) == 0;
}

/*
 * Scene-graph dictionaries carry the two attributes the engine needs: a
 * transform's "_name" (the animation name) and a shape model's "_f"
 * (its keyframe, i.e. the pose number). Everything else is skipped.
 */
static bool read_dictionary(
    ChunkReader *reader,
    char *name,
    size_t name_size,
    int32_t *keyframe)
{
    uint32_t pair_count;

    if (!reader_u32(reader, &pair_count)) {
        return false;
    }

    for (uint32_t pair = 0u; pair < pair_count; ++pair) {
        const uint8_t *key;
        const uint8_t *value;
        uint32_t key_length;
        uint32_t value_length;

        if (!reader_string(reader, &key, &key_length) ||
            !reader_string(reader, &value, &value_length)) {
            return false;
        }

        if (name != NULL && string_equals(key, key_length, "_name")) {
            size_t copied = value_length < name_size - 1u ? value_length : name_size - 1u;

            memcpy(name, value, copied);
            name[copied] = '\0';
        } else if (keyframe != NULL && string_equals(key, key_length, "_f") &&
            value_length > 0u && value_length < 12u) {
            char text[12];
            char *end = NULL;
            long parsed;

            memcpy(text, value, value_length);
            text[value_length] = '\0';
            parsed = strtol(text, &end, 10);
            if (end != text && *end == '\0' && parsed >= 0 && parsed < INT32_MAX) {
                *keyframe = (int32_t)parsed;
            }
        }
    }

    return true;
}

/* A transform node names the shape (or group) it points to. */
typedef struct TransformNode {
    uint32_t child_id;
    char name[WOLF_VOX_OBJECT_NAME_LENGTH];
} TransformNode;

typedef struct DecodeState {
    WolfVoxFile *file;
    uint32_t model_capacity;
    uint32_t pending_size[3];
    bool has_pending_size;
    TransformNode transforms[VOX_MAX_TRANSFORM_NODES];
    uint32_t transform_count;
    uint32_t shape_node_ids[WOLF_VOX_MAX_OBJECTS];
} DecodeState;

/* SIZE precedes each XYZI and gives the bounds every voxel coordinate must respect. */
static bool decode_size_chunk(DecodeState *state, ChunkReader *reader)
{
    for (uint32_t axis = 0u; axis < 3u; ++axis) {
        if (!reader_u32(reader, &state->pending_size[axis]) ||
            state->pending_size[axis] == 0u ||
            state->pending_size[axis] > WOLF_VOX_MAX_DIMENSION) {
            return false;
        }
    }
    state->has_pending_size = true;
    return true;
}

static bool decode_voxel_chunk(DecodeState *state, ChunkReader *reader)
{
    WolfVoxFile *file = state->file;
    WolfVoxModel *model;
    uint32_t stored_count;

    if (!state->has_pending_size || !reader_u32(reader, &stored_count) ||
        stored_count > (reader->size - reader->cursor) / 4u) {
        return false;
    }

    if (file->model_count == state->model_capacity) {
        uint32_t new_capacity = state->model_capacity == 0u ? 8u : state->model_capacity * 2u;
        WolfVoxModel *models = realloc(file->models, (size_t)new_capacity * sizeof(*models));

        if (models == NULL) {
            return false;
        }
        file->models = models;
        state->model_capacity = new_capacity;
    }

    model = &file->models[file->model_count];
    memset(model, 0, sizeof(*model));
    memcpy(model->size, state->pending_size, sizeof(model->size));
    if (stored_count > 0u) {
        model->voxels = malloc((size_t)stored_count * sizeof(*model->voxels));
        if (model->voxels == NULL) {
            return false;
        }
    }
    /* Count the model now so it is released even if a later chunk fails. */
    ++file->model_count;

    for (uint32_t entry = 0u; entry < stored_count; ++entry) {
        const uint8_t *bytes = reader->data + reader->cursor + (size_t)entry * 4u;

        /* Colour 0 and out-of-bounds coordinates cannot describe a visible voxel. */
        if (bytes[3] == 0u || bytes[0] >= model->size[0] ||
            bytes[1] >= model->size[1] || bytes[2] >= model->size[2]) {
            continue;
        }
        model->voxels[model->voxel_count].x = bytes[0];
        model->voxels[model->voxel_count].y = bytes[1];
        model->voxels[model->voxel_count].z = bytes[2];
        model->voxels[model->voxel_count].colour_index = bytes[3];
        ++model->voxel_count;
    }

    state->has_pending_size = false;
    return true;
}

static bool decode_transform_chunk(DecodeState *state, ChunkReader *reader)
{
    TransformNode *node;
    uint32_t node_id;

    if (state->transform_count >= VOX_MAX_TRANSFORM_NODES) {
        /* Very large scenes are legal; nodes beyond the limit just stay unnamed. */
        return true;
    }

    node = &state->transforms[state->transform_count];
    node->name[0] = '\0';
    if (!reader_u32(reader, &node_id) ||
        !read_dictionary(reader, node->name, sizeof(node->name), NULL) ||
        !reader_u32(reader, &node->child_id)) {
        return false;
    }
    ++state->transform_count;
    return true;
}

/* A shape node lists an object's models; each model's "_f" says which pose it is. */
static bool decode_shape_chunk(DecodeState *state, ChunkReader *reader)
{
    WolfVoxFile *file = state->file;
    WolfVoxObject *object;
    uint32_t node_id;
    uint32_t model_count;

    if (file->object_count >= WOLF_VOX_MAX_OBJECTS) {
        return true;
    }

    object = &file->objects[file->object_count];
    memset(object, 0, sizeof(*object));
    if (!reader_u32(reader, &node_id) ||
        !read_dictionary(reader, NULL, 0u, NULL) ||
        !reader_u32(reader, &model_count)) {
        return false;
    }

    for (uint32_t entry = 0u; entry < model_count; ++entry) {
        uint32_t model_id;
        int32_t keyframe = -1;
        uint32_t pose;

        if (!reader_u32(reader, &model_id) ||
            !read_dictionary(reader, NULL, 0u, &keyframe)) {
            return false;
        }
        /* A still shape has no keyframes; its models are its poses in listed order. */
        pose = keyframe >= 0 ? (uint32_t)keyframe : entry;
        if (pose >= WOLF_VOX_MAX_OBJECT_POSES) {
            continue;
        }
        object->model_for_pose[pose] = model_id;
        if (pose + 1u > object->pose_count) {
            object->pose_count = pose + 1u;
        }
    }

    state->shape_node_ids[file->object_count] = node_id;
    ++file->object_count;
    return true;
}

/* Name each object after the transform node that points at its shape, then validate model references. */
static bool resolve_objects(DecodeState *state)
{
    WolfVoxFile *file = state->file;

    for (uint32_t object_index = 0u; object_index < file->object_count; ++object_index) {
        WolfVoxObject *object = &file->objects[object_index];

        for (uint32_t transform = 0u; transform < state->transform_count; ++transform) {
            if (state->transforms[transform].child_id == state->shape_node_ids[object_index]) {
                (void)snprintf(object->name, sizeof(object->name), "%s",
                    state->transforms[transform].name);
            }
        }
        for (uint32_t pose = 0u; pose < object->pose_count; ++pose) {
            if (object->model_for_pose[pose] >= file->model_count) {
                return false;
            }
        }
    }
    return true;
}

bool wolf_vox_file_decode(
    const uint8_t *data,
    size_t size,
    WolfVoxFile *file,
    char *error_message,
    size_t error_message_size)
{
    DecodeState *state;
    size_t cursor;
    size_t main_end;
    const char *failure = NULL;

    if (file == NULL) {
        write_error(error_message, error_message_size, "No VOX output was supplied.");
        return false;
    }
    memset(file, 0, sizeof(*file));

    if (data == NULL || size < VOX_FILE_HEADER_SIZE + VOX_CHUNK_HEADER_SIZE ||
        memcmp(data, "VOX ", 4u) != 0 || memcmp(data + VOX_FILE_HEADER_SIZE, "MAIN", 4u) != 0) {
        write_error(error_message, error_message_size, "Not a MagicaVoxel file.");
        return false;
    }

    /* MAIN's children follow its (normally empty) content and fill the rest of the file. */
    cursor = VOX_FILE_HEADER_SIZE + VOX_CHUNK_HEADER_SIZE;
    {
        uint32_t main_content_size = read_u32_le(data + VOX_FILE_HEADER_SIZE + 4u);
        uint32_t main_children_size = read_u32_le(data + VOX_FILE_HEADER_SIZE + 8u);

        if (main_content_size > size - cursor ||
            main_children_size > size - cursor - main_content_size) {
            write_error(error_message, error_message_size, "The MAIN chunk is truncated.");
            return false;
        }
        cursor += main_content_size;
        main_end = cursor + main_children_size;
    }

    state = calloc(1u, sizeof(*state));
    if (state == NULL) {
        write_error(error_message, error_message_size, "Out of memory reading a VOX file.");
        return false;
    }
    state->file = file;

    while (failure == NULL && main_end - cursor >= VOX_CHUNK_HEADER_SIZE) {
        const uint8_t *identifier = data + cursor;
        uint32_t content_size = read_u32_le(data + cursor + 4u);
        uint32_t children_size = read_u32_le(data + cursor + 8u);
        size_t available = main_end - cursor - VOX_CHUNK_HEADER_SIZE;
        ChunkReader reader;

        if (content_size > available || children_size > available - content_size) {
            failure = "A VOX chunk is truncated.";
            break;
        }

        reader.data = data + cursor + VOX_CHUNK_HEADER_SIZE;
        reader.size = content_size;
        reader.cursor = 0u;

        if (memcmp(identifier, "SIZE", 4u) == 0) {
            if (!decode_size_chunk(state, &reader)) {
                failure = "A VOX model is larger than one 64-voxel map tile or has no size.";
            }
        } else if (memcmp(identifier, "XYZI", 4u) == 0) {
            if (!decode_voxel_chunk(state, &reader)) {
                failure = "A VOX model's voxel list is malformed.";
            }
        } else if (memcmp(identifier, "RGBA", 4u) == 0) {
            if (content_size < sizeof(file->palette_rgba)) {
                failure = "The VOX palette is truncated.";
            } else {
                memcpy(file->palette_rgba, reader.data, sizeof(file->palette_rgba));
                file->has_palette = true;
            }
        } else if (memcmp(identifier, "nTRN", 4u) == 0) {
            if (!decode_transform_chunk(state, &reader)) {
                failure = "A VOX transform node is malformed.";
            }
        } else if (memcmp(identifier, "nSHP", 4u) == 0) {
            if (!decode_shape_chunk(state, &reader)) {
                failure = "A VOX shape node is malformed.";
            }
        }
        cursor += VOX_CHUNK_HEADER_SIZE + (size_t)content_size + children_size;
    }

    if (failure == NULL && state->has_pending_size) {
        failure = "A VOX SIZE chunk has no voxel list.";
    }
    if (failure == NULL && file->model_count == 0u) {
        failure = "The VOX file contains no models.";
    }
    if (failure == NULL && !resolve_objects(state)) {
        failure = "A VOX animation refers to a missing model.";
    }
    free(state);

    if (failure != NULL) {
        wolf_vox_file_destroy(file);
        write_error(error_message, error_message_size, failure);
        return false;
    }
    return true;
}

bool wolf_vox_file_load(
    const char *path,
    WolfVoxFile *file,
    char *error_message,
    size_t error_message_size)
{
    FILE *stream;
    uint8_t *data;
    long length;
    bool decoded;

    if (file != NULL) {
        memset(file, 0, sizeof(*file));
    }
    if (path == NULL || file == NULL) {
        write_error(error_message, error_message_size, "Invalid VOX load arguments.");
        return false;
    }

    stream = fopen(path, "rb");
    if (stream == NULL) {
        write_error(error_message, error_message_size, "The VOX file could not be opened.");
        return false;
    }
    if (fseek(stream, 0, SEEK_END) != 0 || (length = ftell(stream)) <= 0 ||
        fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        write_error(error_message, error_message_size, "The VOX file could not be read.");
        return false;
    }

    data = malloc((size_t)length);
    if (data == NULL) {
        fclose(stream);
        write_error(error_message, error_message_size, "Out of memory reading a VOX file.");
        return false;
    }
    if (fread(data, 1u, (size_t)length, stream) != (size_t)length) {
        free(data);
        fclose(stream);
        write_error(error_message, error_message_size, "The VOX file could not be read.");
        return false;
    }
    fclose(stream);

    decoded = wolf_vox_file_decode(data, (size_t)length, file, error_message, error_message_size);
    free(data);
    return decoded;
}

bool wolf_vox_file_find_pose(
    const WolfVoxFile *file,
    const char *object_name,
    uint32_t pose,
    const WolfVoxModel **model)
{
    if (file == NULL || object_name == NULL || model == NULL) {
        return false;
    }

    for (uint32_t object_index = 0u; object_index < file->object_count; ++object_index) {
        const WolfVoxObject *object = &file->objects[object_index];

        if (strcmp(object->name, object_name) == 0) {
            if (pose >= object->pose_count) {
                return false;
            }
            *model = &file->models[object->model_for_pose[pose]];
            return true;
        }
    }
    return false;
}

void wolf_vox_file_destroy(WolfVoxFile *file)
{
    if (file == NULL) {
        return;
    }
    for (uint32_t model = 0u; model < file->model_count; ++model) {
        free(file->models[model].voxels);
    }
    free(file->models);
    memset(file, 0, sizeof(*file));
}
