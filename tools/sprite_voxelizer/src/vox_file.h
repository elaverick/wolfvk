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

/*
 * Encode one or more models as a MagicaVoxel 150 file. A single model is
 * written as plain SIZE/XYZI/RGBA chunks for the widest loader support.
 * Several models become one animated object: a scene graph whose shape node
 * lists every model with its "_f" keyframe index, as used by MagicaVoxel's
 * animation timeline.
 */
bool wolf_vox_encode(
    const WolfVoxelModel *const *frames,
    size_t frame_count,
    const uint8_t *palette_rgba,
    WolfByteBuffer *output);

bool wolf_vox_write_file(
    const char *path,
    const WolfVoxelModel *const *frames,
    size_t frame_count,
    const uint8_t *palette_rgba);

/* Models and palette read back from a MagicaVoxel file. */
typedef struct WolfVoxDocument {
    size_t model_count;
    WolfVoxelModel **models;
    /* Keyframe index recorded for each model, or -1 if the file has none. */
    int32_t *frame_indices;
    uint8_t palette_rgba[256u * 4u];
} WolfVoxDocument;

/*
 * Decode a MagicaVoxel file into models whose palette_index values are
 * original Wolfenstein palette indices (colour index minus one). Models
 * larger than the 64-voxel tile volume are rejected.
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
