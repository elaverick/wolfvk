#ifndef WOLFVK_VOX_FILE_H
#define WOLFVK_VOX_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * The engine reads the MagicaVoxel files written by tools/sprite_voxelizer.
 * Each file holds one actor or object. Every animation is a named scene
 * object whose keyframes ("_f" 0, 1, 2 ...) are its poses, so a pose is
 * addressed by object name and keyframe number, e.g. "grd_s" pose 0.
 */

/* One map tile is 64 voxels wide, deep and tall; larger models are rejected. */
#define WOLF_VOX_MAX_DIMENSION 64u
#define WOLF_VOX_MAX_OBJECTS 32u
#define WOLF_VOX_MAX_OBJECT_POSES 64u
#define WOLF_VOX_OBJECT_NAME_LENGTH 64u

/* A model is stored as its occupied voxels; colour 0 never appears because it means "empty". */
typedef struct WolfVoxVoxel {
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint8_t colour_index;
} WolfVoxVoxel;

typedef struct WolfVoxModel {
    uint32_t size[3];
    uint32_t voxel_count;
    WolfVoxVoxel *voxels;
} WolfVoxModel;

/* A named animation and the model used for each of its poses, in keyframe order. */
typedef struct WolfVoxObject {
    char name[WOLF_VOX_OBJECT_NAME_LENGTH];
    uint32_t pose_count;
    uint32_t model_for_pose[WOLF_VOX_MAX_OBJECT_POSES];
} WolfVoxObject;

typedef struct WolfVoxFile {
    uint32_t model_count;
    WolfVoxModel *models;
    /*
     * MagicaVoxel colour c (1..255) is palette_rgba entry c - 1. The voxelizer
     * stores Wolfenstein palette entry n as colour n + 1, so the original
     * palette survives unchanged.
     */
    uint8_t palette_rgba[256u * 4u];
    bool has_palette;
    uint32_t object_count;
    WolfVoxObject objects[WOLF_VOX_MAX_OBJECTS];
} WolfVoxFile;

/* Decode a complete MagicaVoxel file held in memory. */
bool wolf_vox_file_decode(
    const uint8_t *data,
    size_t size,
    WolfVoxFile *file,
    char *error_message,
    size_t error_message_size);

/* Read and decode a MagicaVoxel file from disk. */
bool wolf_vox_file_load(
    const char *path,
    WolfVoxFile *file,
    char *error_message,
    size_t error_message_size);

/* Find the model drawn for one pose of a named animation. */
bool wolf_vox_file_find_pose(
    const WolfVoxFile *file,
    const char *object_name,
    uint32_t pose,
    const WolfVoxModel **model);

/* Release every model and reset the file so it can be destroyed twice safely. */
void wolf_vox_file_destroy(WolfVoxFile *file);

#endif
