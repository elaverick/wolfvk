#include "pipeline.h"

#include "asset_catalog.h"
#include "file_io.h"
#include "game_palette.h"
#include "hand_built_models.h"
#include "png_writer.h"
#include "sprite_shadow.h"
#include "validation.h"
#include "vox_file.h"
#include "vswap_sprites.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_LENGTH 1024
/* SPR_STAT_17, the empty hanging cage, whose colours are a cage's bars. */
#define WOLF_EMPTY_CAGE_SPRITE 19

/* Everything learned about one pose model, kept for the manifest and reports. */
typedef struct AssetResult {
    WolfVoxelModel *model;
    bool selected;
    bool built;
    bool passed;
    size_t voxel_count;
    double depth_scale_used;
    int depth_extent;
    WolfViewMetrics views[WOLF_ROTATION_COUNT];
    /* Best IoU this view and its opposite frame allow any solid to reach together. */
    double pair_ceilings[WOLF_ROTATION_COUNT];
    int registration_shifts[WOLF_ROTATION_COUNT];
    size_t view_count;
    double mean_silhouette_iou;
    double minimum_silhouette_iou;
    double mean_colour_accuracy;
    double mean_near_colour_accuracy;
    double mean_colour_error;
    /* Every reason validation failed, separated by semicolons; empty when it passed. */
    char failure[256];
} AssetResult;

typedef struct SubjectResult {
    bool selected;
    bool written;
    bool readback_matches;
} SubjectResult;

typedef struct PipelineState {
    const WolfPipelineOptions *options;
    WolfSpriteArchive archive;
    /* Sprites with their painted floor shadows removed, and what was removed. */
    WolfSpriteImage *shadow_free;
    uint8_t (*shadow_masks)[WOLF_SPRITE_PIXELS];
    size_t shadow_pixels[WOLF_WL6_SPRITE_COUNT];
    WolfAssetCatalog catalog;
    AssetResult results[WOLF_WL6_SPRITE_COUNT];
    SubjectResult subjects[WOLF_SUBJECT_CAPACITY];
    WolfAnimation sequences[WOLF_ANIMATION_CAPACITY];
    size_t sequence_count;
} PipelineState;

/*
 * Join a directory and a file name. An over-long result is truncated to an
 * empty path, so a later open fails cleanly instead of touching a
 * different, cut-short file name.
 */
static void join_path(char *path, const char *directory, const char *leaf)
{
    size_t directory_length = strlen(directory);
    size_t leaf_length = strlen(leaf);

    if (directory_length + 1u + leaf_length + 1u > PATH_LENGTH) {
        path[0] = '\0';
        return;
    }
    memcpy(path, directory, directory_length);
    path[directory_length] = '/';
    memcpy(path + directory_length + 1u, leaf, leaf_length + 1u);
}

static const char *method_name(const WolfVoxelAsset *asset)
{
    switch (asset->method) {
    case WOLF_ASSET_ROTATION_SET:
        return "rotation_set";
    case WOLF_ASSET_POSE_TRANSFER:
        return "pose_transfer";
    case WOLF_ASSET_HAND_BUILT:
        return "hand_built";
    case WOLF_ASSET_AXIAL_REVOLVE:
        return "axial_revolve";
    case WOLF_ASSET_SINGLE_VIEW:
    default:
        switch (asset->single_view.profile) {
        case WOLF_DEPTH_PROFILE_LATHE:
            return "lathe";
        case WOLF_DEPTH_PROFILE_EXTRUDE:
            return "extrude";
        case WOLF_DEPTH_PROFILE_FIRST_PERSON:
            return "first_person";
        case WOLF_DEPTH_PROFILE_CAGE:
            return "cage";
        case WOLF_DEPTH_PROFILE_INFLATE:
        default:
            return "inflate";
        }
    }
}

static bool is_first_person(const WolfVoxelAsset *asset)
{
    return asset->method == WOLF_ASSET_SINGLE_VIEW && asset->single_view.profile == WOLF_DEPTH_PROFILE_FIRST_PERSON;
}

/* PNG name of an extracted original frame, e.g. "050_grd_s_1.png". */
static void sprite_file_name(int sprite, char *leaf, size_t leaf_size)
{
    const char *source = wolf_wl6_sprite_names[sprite];
    char name[WOLF_ASSET_NAME_LENGTH];
    size_t length = 0u;

    if (strncmp(source, "SPR_", 4u) == 0) {
        source += 4;
    }
    while (source[length] != '\0' && length + 1u < sizeof(name)) {
        name[length] = (char)(source[length] >= 'A' && source[length] <= 'Z' ?
            source[length] - 'A' + 'a' : source[length]);
        ++length;
    }
    name[length] = '\0';
    (void)snprintf(leaf, leaf_size, "%03d_%s.png", sprite, name);
}

static bool export_sprite_images(const PipelineState *state, const char *sprite_directory)
{
    uint8_t rgba[WOLF_SPRITE_PIXELS * 4u];

    for (int sprite = 0; sprite < WOLF_WL6_SPRITE_COUNT; ++sprite) {
        char leaf[WOLF_ASSET_NAME_LENGTH + 16];
        char path[PATH_LENGTH];

        sprite_file_name(sprite, leaf, sizeof(leaf));
        join_path(path, sprite_directory, leaf);
        wolf_sprite_to_rgba(&state->archive.sprites[sprite], wolf_game_palette_rgba, rgba);
        if (!wolf_png_write_rgba(path, rgba, WOLF_SPRITE_SIZE, WOLF_SPRITE_SIZE)) {
            fprintf(stderr, "error: could not write %s\n", path);
            return false;
        }
    }
    return true;
}

/* Remove painted floor shadows from the sprites of every family that has them. */
static void strip_shadows(PipelineState *state)
{
    for (int sprite = 0; sprite < WOLF_WL6_SPRITE_COUNT; ++sprite) {
        const WolfVoxelAsset *asset = &state->catalog.assets[state->catalog.asset_for_sprite[sprite]];

        if (asset->strip_shadow) {
            state->shadow_pixels[sprite] = wolf_sprite_remove_floor_shadow(
                &state->archive.sprites[sprite], &state->shadow_free[sprite], state->shadow_masks[sprite]);
        } else {
            memcpy(&state->shadow_free[sprite], &state->archive.sprites[sprite], sizeof(WolfSpriteImage));
            memset(state->shadow_masks[sprite], 0, WOLF_SPRITE_PIXELS);
            state->shadow_pixels[sprite] = 0u;
        }
    }
}

/* Depth scale for a camera-facing frame: the actor's own build when its rotation model exists. */
static double subject_depth_scale(const PipelineState *state, const WolfVoxelAsset *asset)
{
    int reference = state->catalog.subjects[asset->subject].reference_asset;

    if (asset->depth_from_subject && reference >= 0 && state->results[reference].model != NULL) {
        double ratio = state->results[reference].depth_scale_used;

        return ratio < 0.35 ? 0.35 : (ratio > 1.5 ? 1.5 : ratio);
    }
    return asset->single_view.depth_scale;
}

static void reconstruct_asset(PipelineState *state, size_t asset_index)
{
    const WolfVoxelAsset *asset = &state->catalog.assets[asset_index];
    AssetResult *result = &state->results[asset_index];
    const WolfSpriteImage *image = &state->shadow_free[asset->first_sprite];

    result->model = wolf_voxel_model_create();
    if (result->model == NULL) {
        return;
    }

    switch (asset->method) {
    case WOLF_ASSET_ROTATION_SET: {
        const WolfSpriteImage *views[WOLF_ROTATION_COUNT];
        double angles[WOLF_ROTATION_COUNT];

        for (int rotation = 0; rotation < WOLF_ROTATION_COUNT; ++rotation) {
            views[rotation] = &state->shadow_free[asset->first_sprite + rotation];
            angles[rotation] = wolf_rotation_degrees(rotation);
        }
        result->built = wolf_reconstruct_multi_view(views, angles, WOLF_ROTATION_COUNT,
            &state->options->multi_view, result->model, result->registration_shifts);
        result->depth_scale_used = wolf_voxel_model_depth_ratio(result->model);
        break;
    }
    case WOLF_ASSET_POSE_TRANSFER: {
        WolfSingleViewOptions fallback = asset->single_view;
        int reference = state->catalog.subjects[asset->subject].reference_asset;

        fallback.depth_scale = subject_depth_scale(state, asset);
        result->depth_scale_used = fallback.depth_scale;
        if (reference >= 0 && state->results[reference].model != NULL) {
            wolf_reconstruct_pose_transfer(image, state->results[reference].model, &fallback, result->model);
        } else {
            wolf_reconstruct_single_view(image, &fallback, result->model);
        }
        result->built = true;
        break;
    }
    case WOLF_ASSET_HAND_BUILT:
        result->built = wolf_build_hand_model(asset->hand_built, image, result->model);
        result->depth_scale_used = wolf_voxel_model_depth_ratio(result->model);
        break;
    case WOLF_ASSET_AXIAL_REVOLVE:
        /* Rotation 2 is the side view, nose toward screen-left. */
        wolf_reconstruct_axial_revolve(&state->shadow_free[asset->first_sprite + 2], result->model);
        result->depth_scale_used = wolf_voxel_model_depth_ratio(result->model);
        result->built = true;
        break;
    case WOLF_ASSET_SINGLE_VIEW:
    default: {
        WolfSingleViewOptions single_view = asset->single_view;
        uint8_t frame_colours[256] = {0};

        single_view.depth_scale = subject_depth_scale(state, asset);
        /* A cage's bars are the colours of the empty hanging cage (SPR_STAT_17). */
        if (single_view.profile == WOLF_DEPTH_PROFILE_CAGE) {
            const WolfSpriteImage *empty_cage = &state->shadow_free[WOLF_EMPTY_CAGE_SPRITE];

            for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
                if (empty_cage->opaque[pixel]) {
                    frame_colours[empty_cage->palette_index[pixel]] = 1u;
                }
            }
            single_view.frame_colours = frame_colours;
        }
        wolf_reconstruct_single_view(image, &single_view, result->model);
        result->depth_scale_used = single_view.depth_scale;
        result->built = true;
        break;
    }
    }
    result->voxel_count = wolf_voxel_model_count(result->model);
    result->depth_extent = wolf_voxel_model_depth_extent(result->model);
}

/*
 * A hand-built model is checked against its reference sprite for the two
 * things a faithful rebuild must keep: its width across the front view
 * (relative to the sprite's), and its palette (the share of voxels whose
 * colour the sprite itself uses).
 */
static void hand_built_reference_check(const WolfSpriteImage *reference, const WolfVoxelModel *model,
    double *width_ratio, double *palette_share)
{
    uint8_t reference_colours[256] = {0};
    int reference_left = WOLF_SPRITE_SIZE;
    int reference_right = -1;
    int model_left = WOLF_VOXEL_DIM;
    int model_right = -1;
    size_t matching = 0u;
    size_t total = 0u;

    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);

            if (reference->opaque[pixel]) {
                reference_colours[reference->palette_index[pixel]] = 1u;
                reference_left = column < reference_left ? column : reference_left;
                reference_right = column > reference_right ? column : reference_right;
            }
        }
    }
    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        if (model->occupied[voxel]) {
            int x = (int)(voxel % WOLF_VOXEL_DIM);

            model_left = x < model_left ? x : model_left;
            model_right = x > model_right ? x : model_right;
            matching += reference_colours[model->palette_index[voxel]] ? 1u : 0u;
            ++total;
        }
    }
    *width_ratio = reference_right < 0 || model_right < 0 ? 0.0 :
        (double)(model_right - model_left + 1) / (double)(reference_right - reference_left + 1);
    *palette_share = total > 0u ? (double)matching / (double)total : 0.0;
}

/* Record one more reason for failing validation. */
static void add_failure(AssetResult *result, const char *reason)
{
    size_t used = strlen(result->failure);

    result->passed = false;
    (void)snprintf(result->failure + used, sizeof(result->failure) - used, "%s%s", used > 0u ? "; " : "", reason);
}

/* Render the model from each original direction, score it, and decide pass or fail. */
static void validate_asset(PipelineState *state, size_t asset_index, const char *validation_directory)
{
    const WolfVoxelAsset *asset = &state->catalog.assets[asset_index];
    AssetResult *result = &state->results[asset_index];
    const WolfSpriteImage *originals[WOLF_ROTATION_COUNT];
    const WolfSpriteImage *targets[WOLF_ROTATION_COUNT];
    const uint8_t *masks[WOLF_ROTATION_COUNT];
    WolfSpriteImage rendered[WOLF_ROTATION_COUNT];
    double face_on_near_colour = 0.0;
    int face_on_views = 0;
    char reason[128];

    result->view_count = (size_t)asset->sprite_count;
    result->mean_silhouette_iou = 0.0;
    result->minimum_silhouette_iou = 1.0;
    result->mean_colour_accuracy = 0.0;
    result->mean_near_colour_accuracy = 0.0;
    result->mean_colour_error = 0.0;
    result->failure[0] = '\0';

    for (size_t view = 0u; view < result->view_count; ++view) {
        int sprite = asset->first_sprite + (int)view;
        WolfViewMetrics *metrics = &result->views[view];

        originals[view] = &state->archive.sprites[sprite];
        targets[view] = &state->shadow_free[sprite];
        masks[view] = state->shadow_masks[sprite];
        wolf_voxel_render_view(result->model, wolf_rotation_degrees((int)view),
            result->registration_shifts[view], &rendered[view], NULL);
        wolf_compare_views(targets[view], &rendered[view], wolf_game_palette_rgba, metrics);
        result->mean_silhouette_iou += metrics->silhouette_iou;
        result->mean_colour_accuracy += metrics->colour_accuracy;
        result->mean_near_colour_accuracy += metrics->near_colour_accuracy;
        result->mean_colour_error += metrics->mean_colour_error;
        if (metrics->silhouette_iou < result->minimum_silhouette_iou) {
            result->minimum_silhouette_iou = metrics->silhouette_iou;
        }
        if (view % 2u == 0u) {
            face_on_near_colour += metrics->near_colour_accuracy;
            ++face_on_views;
        }
    }
    result->mean_silhouette_iou /= (double)result->view_count;
    result->mean_colour_accuracy /= (double)result->view_count;
    result->mean_near_colour_accuracy /= (double)result->view_count;
    result->mean_colour_error /= (double)result->view_count;
    face_on_near_colour /= (double)(face_on_views > 0 ? face_on_views : 1);

    result->passed = result->built;
    switch (asset->method) {
    case WOLF_ASSET_ROTATION_SET:
        for (size_t view = 0u; view < result->view_count; ++view) {
            double minimum = view % 2u == 0u ? WOLF_ROTATION_MIN_FACE_ON_IOU : WOLF_ROTATION_MIN_DIAGONAL_IOU;

            result->pair_ceilings[view] = wolf_opposite_pair_ceiling(targets[view], targets[(view + 4u) % 8u],
                2 * state->options->multi_view.maximum_registration_shift);
            if (view % 2u == 0u && minimum > WOLF_ROTATION_MIN_FRACTION_OF_PAIR_CEILING * result->pair_ceilings[view]) {
                minimum = WOLF_ROTATION_MIN_FRACTION_OF_PAIR_CEILING * result->pair_ceilings[view];
            }
            if (result->views[view].silhouette_iou < minimum) {
                (void)snprintf(reason, sizeof(reason), "rotation %u silhouette IoU %.3f < %.2f",
                    (unsigned)view, result->views[view].silhouette_iou, minimum);
                add_failure(result, reason);
            }
        }
        if (face_on_near_colour < WOLF_ROTATION_MIN_FACE_ON_NEAR_COLOUR) {
            (void)snprintf(reason, sizeof(reason), "face-on colour within 1px %.3f < %.2f",
                face_on_near_colour, WOLF_ROTATION_MIN_FACE_ON_NEAR_COLOUR);
            add_failure(result, reason);
        }
        break;
    case WOLF_ASSET_HAND_BUILT: {
        double width_ratio;
        double palette_share;

        hand_built_reference_check(targets[0], result->model, &width_ratio, &palette_share);
        if (width_ratio < 1.0 - WOLF_HAND_BUILT_WIDTH_TOLERANCE || width_ratio > 1.0 + WOLF_HAND_BUILT_WIDTH_TOLERANCE) {
            (void)snprintf(reason, sizeof(reason), "width is %.2f times the reference", width_ratio);
            add_failure(result, reason);
        }
        if (palette_share < WOLF_HAND_BUILT_MIN_PALETTE_SHARE) {
            (void)snprintf(reason, sizeof(reason), "only %.0f%% of voxels use the reference's colours",
                palette_share * 100.0);
            add_failure(result, reason);
        }
        break;
    }
    case WOLF_ASSET_AXIAL_REVOLVE:
        /* A body of revolution reproduces the side frames it was turned from. */
        for (size_t view = 2u; view < result->view_count; view += 4u) {
            if (result->views[view].silhouette_iou < WOLF_ROTATION_MIN_FACE_ON_IOU) {
                (void)snprintf(reason, sizeof(reason), "side rotation %u silhouette IoU %.3f < %.2f",
                    (unsigned)view, result->views[view].silhouette_iou, WOLF_ROTATION_MIN_FACE_ON_IOU);
                add_failure(result, reason);
            }
        }
        break;
    case WOLF_ASSET_SINGLE_VIEW:
    case WOLF_ASSET_POSE_TRANSFER:
    default:
        /* A camera-facing frame must reproduce its shadow-free original exactly. */
        if (result->views[0].shared_pixels != result->views[0].union_pixels ||
            result->views[0].exact_colour_pixels != result->views[0].shared_pixels) {
            add_failure(result, "front view differs from the original");
        }
        if (is_first_person(asset) && result->depth_extent < WOLF_FIRST_PERSON_MIN_DEPTH) {
            (void)snprintf(reason, sizeof(reason), "weapon reaches only %d voxels into the screen", result->depth_extent);
            add_failure(result, reason);
        }
        break;
    }

    if (state->options->write_comparison_sheets) {
        WolfComparisonSheet sheet;
        char leaf[WOLF_ASSET_NAME_LENGTH + 8];
        char path[PATH_LENGTH];

        if (wolf_comparison_sheet_build(originals, targets, masks, rendered, result->view_count, result->model,
                wolf_game_palette_rgba, &sheet)) {
            (void)snprintf(leaf, sizeof(leaf), "%s.png", asset->name);
            join_path(path, validation_directory, leaf);
            if (!wolf_png_write_rgba(path, sheet.rgba, sheet.width, sheet.height)) {
                fprintf(stderr, "error: could not write %s\n", path);
            }
            wolf_comparison_sheet_destroy(&sheet);
        }
    }
}

static bool models_identical(const WolfVoxelModel *expected, const WolfVoxelModel *actual)
{
    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        if (expected->occupied[voxel] != actual->occupied[voxel]) {
            return false;
        }
        if (expected->occupied[voxel] && expected->palette_index[voxel] != actual->palette_index[voxel]) {
            return false;
        }
    }
    return true;
}

/*
 * Write one subject's file (every animation as a named object) and read it
 * back, checking names, keyframe order and every voxel. Palette entry 255
 * cannot round-trip, so this also proves no WL6 sprite relies on it.
 */
static void write_subject(PipelineState *state, size_t subject_index, const char *vox_directory)
{
    const WolfSubject *subject = &state->catalog.subjects[subject_index];
    SubjectResult *result = &state->subjects[subject_index];
    WolfVoxObject objects[WOLF_SUBJECT_MAX_ANIMATIONS];
    const WolfVoxelModel *frames[WOLF_SUBJECT_MAX_ANIMATIONS][WOLF_ANIMATION_MAX_POSES];
    char leaf[WOLF_ASSET_NAME_LENGTH + 8];
    char path[PATH_LENGTH];
    WolfVoxDocument document;
    char error[256];

    for (size_t animation = 0u; animation < subject->animation_count; ++animation) {
        const WolfSubjectAnimation *entry = &subject->animations[animation];

        for (size_t pose = 0u; pose < entry->pose_count; ++pose) {
            frames[animation][pose] = state->results[entry->assets[pose]].model;
            if (frames[animation][pose] == NULL) {
                return;
            }
        }
        objects[animation].name = entry->name;
        objects[animation].frames = frames[animation];
        objects[animation].frame_count = entry->pose_count;
    }

    (void)snprintf(leaf, sizeof(leaf), "%s.vox", subject->name);
    join_path(path, vox_directory, leaf);
    result->written = wolf_vox_write_objects_file(path, objects, subject->animation_count, wolf_game_palette_rgba);
    if (!result->written) {
        fprintf(stderr, "error: could not write %s\n", path);
        return;
    }
    if (!wolf_vox_read_file(path, &document, error, sizeof(error))) {
        fprintf(stderr, "error: %s: %s\n", path, error);
        return;
    }
    result->readback_matches = document.object_count == subject->animation_count;
    for (size_t animation = 0u; animation < document.object_count && result->readback_matches; ++animation) {
        const WolfVoxDocumentObject *object = &document.objects[animation];

        result->readback_matches = strcmp(object->name, objects[animation].name) == 0 &&
            object->frame_count == objects[animation].frame_count;
        for (size_t frame = 0u; frame < object->frame_count && result->readback_matches; ++frame) {
            result->readback_matches = object->frame_indices[frame] == (int32_t)frame &&
                models_identical(frames[animation][frame], document.models[object->model_ids[frame]]);
        }
    }
    if (!result->readback_matches) {
        fprintf(stderr, "error: %s did not read back identically\n", path);
    }
    wolf_vox_document_destroy(&document);
}

/* Locate the subject animation and pose that show a sprite. */
static void locate_sprite(const WolfAssetCatalog *catalog, int sprite, int *subject, int *animation, int *pose)
{
    int asset = catalog->asset_for_sprite[sprite];
    const WolfSubjectAnimation *entry;

    *subject = catalog->assets[asset].subject;
    *animation = catalog->assets[asset].animation;
    *pose = 0;
    entry = &catalog->subjects[*subject].animations[*animation];
    for (size_t index = 0u; index < entry->pose_count; ++index) {
        if (entry->assets[index] == asset) {
            *pose = (int)index;
        }
    }
}

static void write_pose_json(FILE *file, const PipelineState *state, int asset_index)
{
    const WolfVoxelAsset *asset = &state->catalog.assets[asset_index];
    const AssetResult *result = &state->results[asset_index];

    fprintf(file, "{\"model\": \"%s\", \"method\": \"%s\", \"description\": \"%s\", \"sprites\": [",
        asset->name, method_name(asset), asset->description != NULL ? asset->description : "");
    for (int sprite = 0; sprite < asset->sprite_count; ++sprite) {
        fprintf(file, "%s{\"name\": \"%s\", \"index\": %d, \"shadow_pixels_removed\": %u}",
            sprite > 0 ? ", " : "", wolf_wl6_sprite_names[asset->first_sprite + sprite],
            asset->first_sprite + sprite, (unsigned)state->shadow_pixels[asset->first_sprite + sprite]);
    }
    fprintf(file, "], \"voxels\": %u, \"depth_extent\": %d, \"depth_scale\": %.3f, ",
        (unsigned)result->voxel_count, result->depth_extent, result->depth_scale_used);
    fprintf(file, "\"validation\": {\"passed\": %s, \"failure\": \"%s\", \"mean_silhouette_iou\": %.4f, "
        "\"minimum_silhouette_iou\": %.4f, \"mean_colour_accuracy\": %.4f, \"mean_near_colour_accuracy\": %.4f, "
        "\"mean_colour_error\": %.2f, \"views\": [",
        result->passed ? "true" : "false", result->failure, result->mean_silhouette_iou,
        result->minimum_silhouette_iou, result->mean_colour_accuracy, result->mean_near_colour_accuracy,
        result->mean_colour_error);
    for (size_t view = 0u; view < result->view_count; ++view) {
        fprintf(file, "%s{\"rotation\": %u, \"registration_offset_columns\": %d, \"silhouette_iou\": %.4f, "
            "\"pair_ceiling\": %.4f, \"colour_accuracy\": %.4f, \"near_colour_accuracy\": %.4f}",
            view > 0u ? ", " : "", (unsigned)view, result->registration_shifts[view],
            result->views[view].silhouette_iou,
            asset->method == WOLF_ASSET_ROTATION_SET ? result->pair_ceilings[view] : 1.0,
            result->views[view].colour_accuracy,
            result->views[view].near_colour_accuracy);
    }
    fprintf(file, "]}}");
}

/* The manifest is the engine-facing index of files, animations, poses and original timing. */
static bool write_manifest(const PipelineState *state, const char *path)
{
    FILE *file = wolf_open_file(path, "w");
    bool first_subject = true;
    bool first_sequence = true;

    if (file == NULL) {
        return false;
    }
    fprintf(file, "{\n  \"source\": \"VSWAP.WL6\",\n");
    fprintf(file, "  \"conventions\": {\"voxels_per_tile\": %d, \"axes\": \"x right, y away from the front viewer, z up\", "
        "\"facing\": \"-y\", \"origin\": \"actor axis at x=32,y=32; floor at z=0\", "
        "\"rotation\": \"rotation k is viewed from 45*k degrees anticlockwise (from above) of the facing direction\", "
        "\"first_person\": \"weapon models extend along +y away from the player's eye; the hand is nearest\", "
        "\"colour\": \"VOX colour n+1 is Wolfenstein palette entry n\", \"tic_rate_hz\": 70},\n", WOLF_VOXEL_DIM);

    fprintf(file, "  \"subjects\": [\n");
    for (size_t subject_index = 0u; subject_index < state->catalog.subject_count; ++subject_index) {
        const WolfSubject *subject = &state->catalog.subjects[subject_index];

        if (!state->subjects[subject_index].written) {
            continue;
        }
        fprintf(file, "%s    {\"name\": \"%s\", \"file\": \"vox/%s.vox\", \"animations\": [\n",
            first_subject ? "" : ",\n", subject->name, subject->name);
        for (size_t animation = 0u; animation < subject->animation_count; ++animation) {
            const WolfSubjectAnimation *entry = &subject->animations[animation];

            fprintf(file, "      {\"name\": \"%s\", \"poses\": [", entry->name);
            for (size_t pose = 0u; pose < entry->pose_count; ++pose) {
                fprintf(file, "%s\n        ", pose > 0u ? "," : "");
                write_pose_json(file, state, entry->assets[pose]);
            }
            fprintf(file, "]}%s\n", animation + 1u < subject->animation_count ? "," : "");
        }
        fprintf(file, "    ]}");
        first_subject = false;
    }

    /* Timed sequences from the original state tables, pointing at the poses they show. */
    fprintf(file, "\n  ],\n  \"sequences\": [\n");
    for (size_t sequence = 0u; sequence < state->sequence_count; ++sequence) {
        const WolfAnimation *entry = &state->sequences[sequence];
        int subject;
        int animation;
        int pose;

        locate_sprite(&state->catalog, entry->frames[0].sprite_index, &subject, &animation, &pose);
        if (!state->subjects[subject].written) {
            continue;
        }
        fprintf(file, "%s    {\"name\": \"%s\", \"subject\": \"%s\", \"loop_frame\": %d, \"next\": \"%s\", "
            "\"notes\": \"%s\", \"frames\": [",
            first_sequence ? "" : ",\n", entry->name, state->catalog.subjects[subject].name, entry->loop_frame,
            entry->next_animation, entry->notes != NULL ? entry->notes : "");
        for (size_t frame = 0u; frame < entry->frame_count; ++frame) {
            const WolfAnimationFrame *step = &entry->frames[frame];

            locate_sprite(&state->catalog, step->sprite_index, &subject, &animation, &pose);
            fprintf(file, "%s{\"state\": \"%s\", \"sprite\": \"%s\", \"animation\": \"%s\", \"pose\": %d, "
                "\"tics\": %d, \"rotates\": %s}",
                frame > 0u ? ", " : "", step->state_index >= 0 ? wolf_wl6_states[step->state_index].name : "",
                wolf_wl6_sprite_names[step->sprite_index],
                state->catalog.subjects[subject].animations[animation].name, pose, step->tics,
                step->rotate == 1 ? "true" : "false");
        }
        fprintf(file, "]}");
        first_sequence = false;
    }
    fprintf(file, "\n  ]\n}\n");
    return fclose(file) == 0;
}

static bool write_metrics_csv(const PipelineState *state, const char *path)
{
    FILE *file = wolf_open_file(path, "w");

    if (file == NULL) {
        return false;
    }
    fprintf(file, "subject,animation,model,method,views,voxels,depth_extent,mean_silhouette_iou,min_silhouette_iou,"
        "mean_colour_accuracy,mean_near_colour_accuracy,mean_colour_error,passed,failure\n");
    for (size_t asset_index = 0u; asset_index < state->catalog.asset_count; ++asset_index) {
        const WolfVoxelAsset *asset = &state->catalog.assets[asset_index];
        const AssetResult *result = &state->results[asset_index];

        if (!result->selected) {
            continue;
        }
        fprintf(file, "%s,%s,%s,%s,%u,%u,%d,%.4f,%.4f,%.4f,%.4f,%.2f,%s,%s\n",
            state->catalog.subjects[asset->subject].name,
            state->catalog.subjects[asset->subject].animations[asset->animation].name,
            asset->name, method_name(asset), (unsigned)result->view_count, (unsigned)result->voxel_count,
            result->depth_extent, result->mean_silhouette_iou, result->minimum_silhouette_iou,
            result->mean_colour_accuracy, result->mean_near_colour_accuracy, result->mean_colour_error,
            result->passed ? "pass" : "FAIL", result->failure);
    }
    return fclose(file) == 0;
}

/* A browsable report: every pose's comparison sheet with its scores, grouped by subject. */
static bool write_html_report(const PipelineState *state, const char *path)
{
    FILE *file = wolf_open_file(path, "w");

    if (file == NULL) {
        return false;
    }
    fprintf(file, "<!doctype html><html><head><meta charset=\"utf-8\"><title>Sprite voxel validation</title>"
        "<style>body{font-family:system-ui,sans-serif;background:#1e2029;color:#e8e8ee;margin:16px}"
        "section{margin:0 0 24px}img{image-rendering:pixelated;max-width:100%%;display:block}"
        "h2{font-size:20px;margin:28px 0 8px;border-bottom:1px solid #444}h3{font-size:15px;margin:4px 0}"
        ".pass{color:#7ddc7d}.fail{color:#ff6b6b}p{margin:2px 0;font-size:13px;color:#b8bccc}</style></head><body>"
        "<h1>Sprite voxel validation</h1><p>Rows per model: original frame as drawn, voxel model rendered from "
        "the same rotation, difference against the shadow-free frame (grey match, yellow colour differs, red "
        "missing, blue extra, purple floor shadow removed before voxelising). Right: shaded three-quarter "
        "previews.</p>");
    for (size_t subject_index = 0u; subject_index < state->catalog.subject_count; ++subject_index) {
        const WolfSubject *subject = &state->catalog.subjects[subject_index];

        if (!state->subjects[subject_index].selected) {
            continue;
        }
        fprintf(file, "<h2>%s.vox</h2>", subject->name);
        for (size_t animation = 0u; animation < subject->animation_count; ++animation) {
            const WolfSubjectAnimation *entry = &subject->animations[animation];

            for (size_t pose = 0u; pose < entry->pose_count; ++pose) {
                const WolfVoxelAsset *asset = &state->catalog.assets[entry->assets[pose]];
                const AssetResult *result = &state->results[entry->assets[pose]];

                fprintf(file, "<section><h3>%s &middot; pose %u of %s <span class=\"%s\">%s</span></h3>"
                    "<p>%s &middot; %s &middot; %u voxels &middot; silhouette IoU mean %.3f (min %.3f) &middot; "
                    "exact colour %.1f%% (within 1px %.1f%%) %s</p><img src=\"%s.png\" alt=\"%s\"></section>",
                    asset->name, (unsigned)pose + 1u, entry->name, result->passed ? "pass" : "fail",
                    result->passed ? "PASS" : "FAIL", asset->description != NULL ? asset->description : "",
                    method_name(asset), (unsigned)result->voxel_count, result->mean_silhouette_iou,
                    result->minimum_silhouette_iou, result->mean_colour_accuracy * 100.0,
                    result->mean_near_colour_accuracy * 100.0, result->failure, asset->name, asset->name);
            }
        }
    }
    fprintf(file, "</body></html>\n");
    return fclose(file) == 0;
}

/* Append a file's bytes to the viewer data as a base64 string literal. */
static bool append_file_base64(WolfByteBuffer *script, const char *path)
{
    uint8_t *data;
    size_t size;

    if (!wolf_read_entire_file(path, &data, &size)) {
        return false;
    }
    wolf_buffer_append_text(script, "\"");
    wolf_buffer_append_base64(script, data, size);
    wolf_buffer_append_text(script, "\"");
    free(data);
    return true;
}

/*
 * The HTML viewer runs straight from disk, where browsers refuse to fetch
 * neighbouring files, so everything it needs is packed into one script:
 * the manifest, every subject's VOX file and every original frame.
 */
static bool write_viewer_data(const PipelineState *state, const char *manifest_path, const char *vox_directory,
    const char *sprite_directory, const char *path)
{
    WolfByteBuffer script = {0};
    uint8_t *manifest;
    size_t manifest_size;
    bool first = true;
    bool complete = true;

    if (!wolf_read_entire_file(manifest_path, &manifest, &manifest_size)) {
        return false;
    }
    wolf_buffer_append_text(&script, "/* Generated by wolf_sprite_voxelizer from your own game data. Do not commit. */\n");
    wolf_buffer_append_text(&script, "window.WOLF_VOXEL_DATA = {\n\"manifest\": ");
    wolf_buffer_append(&script, manifest, manifest_size);
    free(manifest);

    wolf_buffer_append_text(&script, ",\n\"vox\": {");
    for (size_t subject_index = 0u; subject_index < state->catalog.subject_count && complete; ++subject_index) {
        char leaf[WOLF_ASSET_NAME_LENGTH + 8];
        char path_buffer[PATH_LENGTH];

        if (!state->subjects[subject_index].written) {
            continue;
        }
        (void)snprintf(leaf, sizeof(leaf), "%s.vox", state->catalog.subjects[subject_index].name);
        join_path(path_buffer, vox_directory, leaf);
        wolf_buffer_append_text(&script, first ? "\n\"" : ",\n\"");
        wolf_buffer_append_text(&script, state->catalog.subjects[subject_index].name);
        wolf_buffer_append_text(&script, "\": ");
        complete = append_file_base64(&script, path_buffer);
        first = false;
    }
    wolf_buffer_append_text(&script, "},\n\"sprites\": {");
    for (int sprite = 0; sprite < WOLF_WL6_SPRITE_COUNT && complete; ++sprite) {
        char leaf[WOLF_ASSET_NAME_LENGTH + 16];
        char path_buffer[PATH_LENGTH];
        char key[16];

        sprite_file_name(sprite, leaf, sizeof(leaf));
        join_path(path_buffer, sprite_directory, leaf);
        (void)snprintf(key, sizeof(key), "%s\"%d\": ", sprite > 0 ? ",\n" : "\n", sprite);
        wolf_buffer_append_text(&script, key);
        complete = append_file_base64(&script, path_buffer);
    }
    wolf_buffer_append_text(&script, "}\n};\n");
    complete = complete && !script.failed && wolf_write_entire_file(path, script.data, script.size);
    wolf_buffer_free(&script);
    return complete;
}

static void release_state(PipelineState *state)
{
    for (size_t asset = 0u; asset < state->catalog.asset_count; ++asset) {
        wolf_voxel_model_destroy(state->results[asset].model);
    }
    wolf_sprite_archive_destroy(&state->archive);
    free(state->shadow_free);
    free(state->shadow_masks);
    free(state);
}

int wolf_pipeline_run(const WolfPipelineOptions *options)
{
    PipelineState *state = calloc(1u, sizeof(PipelineState));
    char sprite_directory[PATH_LENGTH];
    char vox_directory[PATH_LENGTH];
    char validation_directory[PATH_LENGTH];
    char manifest_path[PATH_LENGTH];
    char path[PATH_LENGTH];
    char error[256];
    size_t poses = 0u;
    size_t poses_passed = 0u;
    size_t subjects_written = 0u;
    size_t subjects_selected = 0u;
    int exit_code = 0;

    if (state == NULL) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }
    state->options = options;
    state->shadow_free = calloc(WOLF_WL6_SPRITE_COUNT, sizeof(WolfSpriteImage));
    state->shadow_masks = calloc(WOLF_WL6_SPRITE_COUNT, sizeof(*state->shadow_masks));
    if (state->shadow_free == NULL || state->shadow_masks == NULL) {
        fprintf(stderr, "error: out of memory\n");
        release_state(state);
        return 1;
    }

    if (!wolf_sprite_archive_load(options->vswap_path, &state->archive, error, sizeof(error))) {
        fprintf(stderr, "error: %s: %s\n", options->vswap_path, error);
        release_state(state);
        return 1;
    }
    if (state->archive.sprite_count != WOLF_WL6_SPRITE_COUNT) {
        fprintf(stderr, "error: expected %d WL6 sprites but found %u; only retail WL6 data is supported\n",
            WOLF_WL6_SPRITE_COUNT, (unsigned)state->archive.sprite_count);
        release_state(state);
        return 1;
    }

    join_path(sprite_directory, options->output_directory, "sprites");
    join_path(vox_directory, options->output_directory, "vox");
    join_path(validation_directory, options->output_directory, "validation");
    join_path(manifest_path, options->output_directory, "manifest.json");
    if (!wolf_make_directories(sprite_directory) || !wolf_make_directories(vox_directory) ||
        !wolf_make_directories(validation_directory)) {
        fprintf(stderr, "error: could not create output directories under %s\n", options->output_directory);
        release_state(state);
        return 1;
    }

    printf("Decoded %u sprites from %s\n", (unsigned)state->archive.sprite_count, options->vswap_path);
    if (!export_sprite_images(state, sprite_directory)) {
        exit_code = 1;
    }

    wolf_asset_catalog_build(&state->catalog);
    state->sequence_count = wolf_animation_catalog_build(state->sequences, WOLF_ANIMATION_CAPACITY);
    strip_shadows(state);

    /* A subject's camera-facing frames only borrow from its own rotation sets, so subjects are independent. */
    for (size_t subject = 0u; subject < state->catalog.subject_count; ++subject) {
        state->subjects[subject].selected = options->only_subject == NULL ||
            strcmp(options->only_subject, state->catalog.subjects[subject].name) == 0;
        subjects_selected += state->subjects[subject].selected ? 1u : 0u;
    }
    if (subjects_selected == 0u) {
        fprintf(stderr, "error: no subject named %s\n", options->only_subject);
        release_state(state);
        return 1;
    }
    for (size_t asset = 0u; asset < state->catalog.asset_count; ++asset) {
        state->results[asset].selected = state->subjects[state->catalog.assets[asset].subject].selected;
    }

    /*
     * Rotation sets first: camera-facing frames of the same actor take their
     * body, or at least its depth, from the rotation-set model.
     */
    for (int pass = 0; pass < 2; ++pass) {
        for (size_t asset = 0u; asset < state->catalog.asset_count; ++asset) {
            const WolfVoxelAsset *entry = &state->catalog.assets[asset];
            AssetResult *result = &state->results[asset];

            if ((pass == 0) != (entry->method == WOLF_ASSET_ROTATION_SET) || !result->selected) {
                continue;
            }
            reconstruct_asset(state, asset);
            if (result->model == NULL) {
                fprintf(stderr, "error: out of memory building %s\n", entry->name);
                exit_code = 1;
                continue;
            }
            validate_asset(state, asset, validation_directory);
            ++poses;
            poses_passed += result->passed ? 1u : 0u;
            printf("  %-16s %-14s voxels %6u  IoU %.3f (min %.3f)  near colour %5.1f%%  %s %s\n",
                entry->name, method_name(entry), (unsigned)result->voxel_count,
                result->mean_silhouette_iou, result->minimum_silhouette_iou,
                result->mean_near_colour_accuracy * 100.0, result->passed ? "pass" : "FAIL", result->failure);
        }
    }

    for (size_t subject = 0u; subject < state->catalog.subject_count; ++subject) {
        if (state->subjects[subject].selected) {
            write_subject(state, subject, vox_directory);
            subjects_written += state->subjects[subject].written && state->subjects[subject].readback_matches ? 1u : 0u;
        }
    }
    if (subjects_written != subjects_selected) {
        exit_code = 1;
    }

    if (!write_manifest(state, manifest_path)) {
        fprintf(stderr, "error: could not write %s\n", manifest_path);
        exit_code = 1;
    }
    join_path(path, validation_directory, "metrics.csv");
    if (!write_metrics_csv(state, path)) {
        fprintf(stderr, "error: could not write %s\n", path);
        exit_code = 1;
    }
    if (options->write_comparison_sheets) {
        join_path(path, validation_directory, "report.html");
        if (!write_html_report(state, path)) {
            fprintf(stderr, "error: could not write %s\n", path);
            exit_code = 1;
        }
    }
    if (options->write_viewer_data) {
        join_path(path, options->output_directory, "viewer_data.js");
        if (!write_viewer_data(state, manifest_path, vox_directory, sprite_directory, path)) {
            fprintf(stderr, "error: could not write %s\n", path);
            exit_code = 1;
        }
    }

    printf("Poses: %u built, %u passed validation. Subject files: %u of %u written and verified.\n",
        (unsigned)poses, (unsigned)poses_passed, (unsigned)subjects_written, (unsigned)subjects_selected);
    if (poses_passed != poses) {
        exit_code = 1;
    }
    release_state(state);
    return exit_code;
}
