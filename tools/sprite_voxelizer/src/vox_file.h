#ifndef WOLF_SPRITE_VOXELIZER_VOX_FILE_H
#define WOLF_SPRITE_VOXELIZER_VOX_FILE_H

#include "file_io.h"
#include "voxel_model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * MagicaVoxel stores up to 255 colours; colour index c refers to RGBA chunk
 * entry c-1 and index 0 means empty. Wolfenstein palette entry n is stored
 * as colour n+1, so the original 256-entry palette maps one-to-one except
 * for entry 255, which is replaced by its nearest visual match.
 */
uint8_t wolf_vox_colour_for_palette_index(const uint8_t *palette_rgba, uint8_t palette_index);

/* One named object in a file: an animation whose keyframes are its models. */
typedef struct WolfVoxObject {
    const char *name;
    const WolfVoxelModel *const *frames;
    size_t frame_count;
} WolfVoxObject;

/* Spacing between objects laid out side by side in the MagicaVoxel scene. */
#define WOLF_VOX_OBJECT_SPACING 80

/*
 * Encode named animated objects as one MagicaVoxel 150 file. Each object
 * becomes a transform node carrying its "_name" and a shape node listing its
 * models with "_f" keyframe indices 0..n-1, which MagicaVoxel plays as an
 * animation. Objects are placed side by side along x so they can all be
 * seen at once; the engine addresses them by name.
 */
bool wolf_vox_encode_objects(
    const WolfVoxObject *objects,
    size_t object_count,
    const uint8_t *palette_rgba,
    WolfByteBuffer *output);

bool wolf_vox_write_objects_file(
    const char *path,
    const WolfVoxObject *objects,
    size_t object_count,
    const uint8_t *palette_rgba);

#define WOLF_VOX_MAX_OBJECTS 32
#define WOLF_VOX_MAX_OBJECT_FRAMES 64

/* An object read back from a file: its name and its models in keyframe order. */
typedef struct WolfVoxDocumentObject {
    char name[64];
    size_t model_ids[WOLF_VOX_MAX_OBJECT_FRAMES];
    int32_t frame_indices[WOLF_VOX_MAX_OBJECT_FRAMES];
    size_t frame_count;
} WolfVoxDocumentObject;

/* Models, palette and scene objects read back from a MagicaVoxel file. */
typedef struct WolfVoxDocument {
    size_t model_count;
    WolfVoxelModel **models;
    uint8_t palette_rgba[256u * 4u];
    WolfVoxDocumentObject objects[WOLF_VOX_MAX_OBJECTS];
    size_t object_count;
} WolfVoxDocument;

/*
 * Decode a MagicaVoxel file into models whose palette_index values are
 * original Wolfenstein palette indices (colour index minus one), plus the
 * named objects of its scene graph. Models larger than the 64-voxel tile
 * volume are rejected.
 */
bool wolf_vox_decode(
    const uint8_t *data,
    size_t size,
    WolfVoxDocument *document,
    char *error_message,
    size_t error_message_size);

bool wolf_vox_read_file(
    const char *path,
    WolfVoxDocument *document,
    char *error_message,
    size_t error_message_size);

void wolf_vox_document_destroy(WolfVoxDocument *document);

#endif
