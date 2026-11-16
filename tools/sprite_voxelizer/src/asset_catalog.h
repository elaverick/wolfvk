#ifndef WOLF_SPRITE_VOXELIZER_ASSET_CATALOG_H
#define WOLF_SPRITE_VOXELIZER_ASSET_CATALOG_H

#include "hand_built_models.h"
#include "reconstruction.h"
#include "wl6_tables.h"

#include <stdbool.h>
#include <stddef.h>

#define WOLF_ASSET_NAME_LENGTH 48
#define WOLF_ROTATION_COUNT 8

/* How one pose model is produced. */
typedef enum WolfAssetMethod {
    /* Eight frames drawn from eight directions, rebuilt into one solid. */
    WOLF_ASSET_ROTATION_SET,
    /* A camera-facing frame given depth by a profile (inflate, lathe, extrude, first person). */
    WOLF_ASSET_SINGLE_VIEW,
    /* A camera-facing actor frame built on the body of the actor's rotation-set model. */
    WOLF_ASSET_POSE_TRANSFER,
    /* Objects modelled directly, using the sprite only as a reference. */
    WOLF_ASSET_HAND_BUILT,
    /* A projectile revolved about its flight axis from its side frame. */
    WOLF_ASSET_AXIAL_REVOLVE
} WolfAssetMethod;

/* One pose model: a rotation set or a single frame. */
typedef struct WolfVoxelAsset {
    char name[WOLF_ASSET_NAME_LENGTH];
    WolfAssetMethod method;
    /* Frame-1 sprite of a rotation set, or the only sprite of a single view. */
    int first_sprite;
    int sprite_count;
    /* Index into the catalogue's subjects, and the animation within that subject. */
    int subject;
    int animation;
    const char *description;
    WolfSingleViewOptions single_view;
    /*
     * When true, single_view.depth_scale is replaced by the depth ratio of the
     * subject's rotation-set model so all of an actor's frames share its build.
     */
    bool depth_from_subject;
    /* Remove the floor shadow painted beneath the sprite before reconstruction. */
    bool strip_shadow;
    WolfHandBuiltModel hand_built;
} WolfVoxelAsset;

#define WOLF_SUBJECT_MAX_ANIMATIONS 12
#define WOLF_ANIMATION_MAX_POSES 16

/* A named animation inside a subject's file: its poses in playback order. */
typedef struct WolfSubjectAnimation {
    char name[WOLF_ASSET_NAME_LENGTH];
    int assets[WOLF_ANIMATION_MAX_POSES];
    size_t pose_count;
} WolfSubjectAnimation;

/*
 * One output file: an actor, a static object, a projectile or a weapon,
 * holding every one of its animations.
 */
typedef struct WolfSubject {
    char name[WOLF_ASSET_NAME_LENGTH];
    WolfSubjectAnimation animations[WOLF_SUBJECT_MAX_ANIMATIONS];
    size_t animation_count;
    /* Rotation-set asset whose body depth this subject's single frames use, or -1. */
    int reference_asset;
} WolfSubject;

#define WOLF_SUBJECT_CAPACITY 128

typedef struct WolfAssetCatalog {
    WolfVoxelAsset assets[WOLF_WL6_SPRITE_COUNT];
    size_t asset_count;
    WolfSubject subjects[WOLF_SUBJECT_CAPACITY];
    size_t subject_count;
    /* Asset that contains each sprite, and the sprite's rotation within it (0 for single views). */
    int asset_for_sprite[WOLF_WL6_SPRITE_COUNT];
    int rotation_for_sprite[WOLF_WL6_SPRITE_COUNT];
} WolfAssetCatalog;

/*
 * Build the WL6 catalogue from the original sprite and state tables. Sprite
 * names give the grouping: SPR_GRD_W1_1 belongs to subject "grd",
 * animation "w"; SPR_GRD_DIE_2 to animation "die". Poses of an animation
 * are in sprite order, which is the original drawing order.
 */
void wolf_asset_catalog_build(WolfAssetCatalog *catalog);

/* Index of a subject by name, or -1. */
int wolf_asset_catalog_find_subject(const WolfAssetCatalog *catalog, const char *name);

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

/* A timed sequence reconstructed from the original state graph. */
typedef struct WolfAnimation {
    char name[WOLF_ASSET_NAME_LENGTH];
    WolfAnimationFrame frames[WOLF_ANIMATION_MAX_FRAMES];
    size_t frame_count;
    /* Frame index the sequence returns to, or -1 when it does not loop. */
    int loop_frame;
    /* Sequence that follows a non-looping sequence, or empty if it simply ends. */
    char next_animation[WOLF_ASSET_NAME_LENGTH];
    /* Original state entered after the last frame, or -1. */
    int next_state_index;
    const char *notes;
} WolfAnimation;

#define WOLF_ANIMATION_CAPACITY 160

/*
 * Group the original states into timed sequences. States whose names differ
 * only by trailing digits and an "s" suffix (s_grdchase1, s_grdchase1s,
 * s_grdchase2 ...) form one sequence, ordered by following each state's
 * next pointer. The player's weapon sequences come from attackinfo in
 * WL_AGENT.C because the player has no sprite states. These sequences give
 * the original timing for playing a subject's animations in the engine.
 */
size_t wolf_animation_catalog_build(WolfAnimation *animations, size_t capacity);

#endif
