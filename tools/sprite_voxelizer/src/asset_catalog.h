#ifndef WOLF_SPRITE_VOXELIZER_ASSET_CATALOG_H
#define WOLF_SPRITE_VOXELIZER_ASSET_CATALOG_H

#include "reconstruction.h"
#include "wl6_tables.h"

#include <stdbool.h>
#include <stddef.h>

#define WOLF_ASSET_NAME_LENGTH 48
#define WOLF_ROTATION_COUNT 8

typedef enum WolfAssetKind {
    /* Eight frames drawn from eight directions, rebuilt into one solid. */
    WOLF_ASSET_ROTATION_SET,
    /* A frame the original engine always draws facing the camera. */
    WOLF_ASSET_SINGLE_VIEW
} WolfAssetKind;

/* One output model and how it is reconstructed. */
typedef struct WolfVoxelAsset {
    char name[WOLF_ASSET_NAME_LENGTH];
    WolfAssetKind kind;
    /* Frame-1 sprite of a rotation set, or the only sprite of a single view. */
    int first_sprite;
    int sprite_count;
    /* Leading token of the sprite name (e.g. "GRD"), shared by one actor's frames. */
    char subject[WOLF_ASSET_NAME_LENGTH];
    const char *description;
    WolfSingleViewOptions single_view;
    /*
     * When true, single_view.depth_scale is replaced by the depth ratio of the
     * same subject's rotation-set model, so camera-facing frames of an actor
     * share the body thickness recovered from its rotation frames.
     */
    bool depth_from_subject;
} WolfVoxelAsset;

typedef struct WolfAssetCatalog {
    WolfVoxelAsset assets[WOLF_WL6_SPRITE_COUNT];
    size_t asset_count;
    /* Asset that contains each sprite, and the sprite's rotation within it (0 for single views). */
    int asset_for_sprite[WOLF_WL6_SPRITE_COUNT];
    int rotation_for_sprite[WOLF_WL6_SPRITE_COUNT];
} WolfAssetCatalog;

/* Build the WL6 asset list from the original sprite and state tables. */
void wolf_asset_catalog_build(WolfAssetCatalog *catalog);

/* Index of the first rotation-set asset for a subject, or -1 if it has none. */
int wolf_asset_catalog_reference_for_subject(const WolfAssetCatalog *catalog, const char *subject);

/* Rotation angle in degrees for frame k of an eight-way set. */
double wolf_rotation_degrees(int rotation);

#define WOLF_ANIMATION_MAX_FRAMES 16

typedef struct WolfAnimationFrame {
    /* Index into wolf_wl6_states, or -1 for frames not driven by a state. */
    int state_index;
    int sprite_index;
    /* Duration in original 70 Hz tics; 0 means "hold until the game changes state". */
    int tics;
    /* Original rotate flag: 1 uses the rotation-set model of sprite_index. */
    int rotate;
} WolfAnimationFrame;

/* A sequence of frames reconstructed from the original state graph. */
typedef struct WolfAnimation {
    char name[WOLF_ASSET_NAME_LENGTH];
    WolfAnimationFrame frames[WOLF_ANIMATION_MAX_FRAMES];
    size_t frame_count;
    /* Frame index the sequence returns to, or -1 when it does not loop. */
    int loop_frame;
    /* Animation that follows a non-looping sequence, or empty if it simply ends. */
    char next_animation[WOLF_ASSET_NAME_LENGTH];
    /* Original state entered after the last frame, or -1. */
    int next_state_index;
    const char *notes;
} WolfAnimation;

#define WOLF_ANIMATION_CAPACITY 160

/*
 * Group the original states into animations. States whose names differ
 * only by trailing digits and an "s" suffix (s_grdchase1, s_grdchase1s,
 * s_grdchase2 ...) form one sequence, ordered by following each state's
 * next pointer. The player's weapon animations come from attackinfo in
 * WL_AGENT.C because the player has no sprite states.
 */
size_t wolf_animation_catalog_build(WolfAnimation *animations, size_t capacity);

#endif
