#include "pipeline.h"

#include "asset_catalog.h"
#include "file_io.h"
#include "game_palette.h"
#include "png_writer.h"
#include "validation.h"
#include "vox_file.h"
#include "vswap_sprites.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_LENGTH 1024

/* Everything learned about one asset, kept for the manifest and reports. */
typedef struct AssetResult {
    WolfVoxelModel *model;
    bool processed;
    bool written;
    bool readback_matches;
    bool passed;
    size_t voxel_count;
    double depth_scale_used;
    WolfViewMetrics views[WOLF_ROTATION_COUNT];
    size_t view_count;
    double mean_silhouette_iou;
    double minimum_silhouette_iou;
    double mean_colour_accuracy;
    double mean_colour_error;
} AssetResult;

typedef struct PipelineState {
    const WolfPipelineOptions *options;
    WolfSpriteArchive archive;
    WolfAssetCatalog catalog;
    AssetResult results[WOLF_WL6_SPRITE_COUNT];
    WolfAnimation animations[WOLF_ANIMATION_CAPACITY];
    size_t animation_count;
    bool animation_written[WOLF_ANIMATION_CAPACITY];
} PipelineState;

static void join_path(char *path, const char *directory, const char *leaf)
{
    (void)snprintf(path, PATH_LENGTH, "%s/%s", directory, leaf);
}

static const char *profile_name(WolfDepthProfile profile)
{
    switch (profile) {
    case WOLF_DEPTH_PROFILE_LATHE:
        return "lathe";
    case WOLF_DEPTH_PROFILE_EXTRUDE:
        return "extrude";
    case WOLF_DEPTH_PROFILE_INFLATE:
    default:
        return "inflate";
    }
}

static bool asset_selected(const PipelineState *state, size_t asset)
{
    return state->options->only_asset == NULL ||
        strcmp(state->options->only_asset, state->catalog.assets[asset].name) == 0;
}

/* Save every original frame as a PNG so the extracted art can be inspected directly. */
static bool export_sprite_images(const PipelineState *state, const char *sprite_directory)
{
    uint8_t rgba[WOLF_SPRITE_PIXELS * 4u];

    for (int sprite = 0; sprite < WOLF_WL6_SPRITE_COUNT; ++sprite) {
        char name[WOLF_ASSET_NAME_LENGTH];
        char leaf[WOLF_ASSET_NAME_LENGTH + 16];
        char path[PATH_LENGTH];
        const char *source = wolf_wl6_sprite_names[sprite];
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
        (void)snprintf(leaf, sizeof(leaf), "%03d_%s.png", sprite, name);
        join_path(path, sprite_directory, leaf);
        wolf_sprite_to_rgba(&state->archive.sprites[sprite], wolf_game_palette_rgba, rgba);
        if (!wolf_png_write_rgba(path, rgba, WOLF_SPRITE_SIZE, WOLF_SPRITE_SIZE)) {
            fprintf(stderr, "error: could not write %s\n", path);
            return false;
        }
    }
    return true;
}

static void reconstruct_asset(PipelineState *state, size_t asset_index)
{
    const WolfVoxelAsset *asset = &state->catalog.assets[asset_index];
    AssetResult *result = &state->results[asset_index];

    result->model = wolf_voxel_model_create();
    if (result->model == NULL) {
        return;
    }

    if (asset->kind == WOLF_ASSET_ROTATION_SET) {
        const WolfSpriteImage *views[WOLF_ROTATION_COUNT];
        double angles[WOLF_ROTATION_COUNT];

        for (int rotation = 0; rotation < WOLF_ROTATION_COUNT; ++rotation) {
            views[rotation] = &state->archive.sprites[asset->first_sprite + rotation];
            angles[rotation] = wolf_rotation_degrees(rotation);
        }
        result->processed = wolf_reconstruct_multi_view(
            views, angles, WOLF_ROTATION_COUNT, &state->options->multi_view, result->model);
        result->depth_scale_used = wolf_voxel_model_depth_ratio(result->model);
    } else {
        WolfSingleViewOptions single_view = asset->single_view;

        /* Borrow the body thickness the actor's own rotation frames revealed. */
        if (asset->depth_from_subject) {
            int reference = wolf_asset_catalog_reference_for_subject(&state->catalog, asset->subject);

            if (reference >= 0 && state->results[reference].model != NULL) {
                double ratio = state->results[reference].depth_scale_used;

                single_view.depth_scale = ratio < 0.35 ? 0.35 : (ratio > 1.5 ? 1.5 : ratio);
            }
        }
        wolf_reconstruct_single_view(&state->archive.sprites[asset->first_sprite], &single_view, result->model);
        result->depth_scale_used = single_view.depth_scale;
        result->processed = true;
    }
    result->voxel_count = wolf_voxel_model_count(result->model);
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
 * Validate from the file on disk rather than the in-memory model, so the
 * check also covers the VOX encoding: read it back, render each original
 * direction and compare with the original frame.
 */
static void write_and_validate_asset(PipelineState *state, size_t asset_index,
    const char *model_directory, const char *validation_directory)
{
    const WolfVoxelAsset *asset = &state->catalog.assets[asset_index];
    AssetResult *result = &state->results[asset_index];
    const WolfVoxelModel *frames[1] = {result->model};
    char leaf[WOLF_ASSET_NAME_LENGTH + 8];
    char path[PATH_LENGTH];
    WolfVoxDocument document;
    char error[256];
    const WolfVoxelModel *validated_model;
    const WolfSpriteImage *originals[WOLF_ROTATION_COUNT];
    WolfSpriteImage rendered[WOLF_ROTATION_COUNT];

    (void)snprintf(leaf, sizeof(leaf), "%s.vox", asset->name);
    join_path(path, model_directory, leaf);
    result->written = wolf_vox_write_file(path, frames, 1u, wolf_game_palette_rgba);
    if (!result->written) {
        fprintf(stderr, "error: could not write %s\n", path);
        return;
    }

    if (!wolf_vox_read_file(path, &document, error, sizeof(error))) {
        fprintf(stderr, "error: %s: %s\n", path, error);
        return;
    }
    /* Palette entry 255 cannot round-trip; every WL6 sprite is checked for it here. */
    result->readback_matches = document.model_count == 1u &&
        models_identical(result->model, document.models[0]);
    validated_model = document.models[0];

    result->view_count = (size_t)asset->sprite_count;
    result->mean_silhouette_iou = 0.0;
    result->minimum_silhouette_iou = 1.0;
    result->mean_colour_accuracy = 0.0;
    result->mean_colour_error = 0.0;
    for (size_t view = 0u; view < result->view_count; ++view) {
        WolfViewMetrics *metrics = &result->views[view];

        originals[view] = &state->archive.sprites[asset->first_sprite + (int)view];
        wolf_voxel_render_view(validated_model, wolf_rotation_degrees((int)view), &rendered[view], NULL);
        wolf_compare_views(originals[view], &rendered[view], wolf_game_palette_rgba, metrics);
        result->mean_silhouette_iou += metrics->silhouette_iou;
        result->mean_colour_accuracy += metrics->colour_accuracy;
        result->mean_colour_error += metrics->mean_colour_error;
        if (metrics->silhouette_iou < result->minimum_silhouette_iou) {
            result->minimum_silhouette_iou = metrics->silhouette_iou;
        }
    }
    result->mean_silhouette_iou /= (double)result->view_count;
    result->mean_colour_accuracy /= (double)result->view_count;
    result->mean_colour_error /= (double)result->view_count;

    if (asset->kind == WOLF_ASSET_SINGLE_VIEW) {
        /* A camera-facing frame is reconstructed to reproduce the original exactly. */
        result->passed = result->readback_matches &&
            result->views[0].shared_pixels == result->views[0].union_pixels &&
            result->views[0].exact_colour_pixels == result->views[0].shared_pixels;
    } else {
        result->passed = result->readback_matches &&
            result->mean_silhouette_iou >= WOLF_ROTATION_MIN_MEAN_SILHOUETTE_IOU &&
            result->minimum_silhouette_iou >= WOLF_ROTATION_MIN_VIEW_SILHOUETTE_IOU &&
            result->mean_colour_accuracy >= WOLF_ROTATION_MIN_MEAN_COLOUR_ACCURACY;
    }

    if (state->options->write_comparison_sheets) {
        WolfComparisonSheet sheet;

        if (wolf_comparison_sheet_build(originals, rendered, result->view_count, validated_model,
                wolf_game_palette_rgba, &sheet)) {
            (void)snprintf(leaf, sizeof(leaf), "%s.png", asset->name);
            join_path(path, validation_directory, leaf);
            if (!wolf_png_write_rgba(path, sheet.rgba, sheet.width, sheet.height)) {
                fprintf(stderr, "error: could not write %s\n", path);
            }
            wolf_comparison_sheet_destroy(&sheet);
        }
    }
    wolf_vox_document_destroy(&document);
}

/* Write each animation as one multi-frame VOX, re-reading it to confirm the frames. */
static void write_animations(PipelineState *state, const char *animation_directory)
{
    for (size_t animation_index = 0u; animation_index < state->animation_count; ++animation_index) {
        const WolfAnimation *animation = &state->animations[animation_index];
        const WolfVoxelModel *frames[WOLF_ANIMATION_MAX_FRAMES];
        char leaf[WOLF_ASSET_NAME_LENGTH + 8];
        char path[PATH_LENGTH];
        WolfVoxDocument document;
        char error[256];
        bool complete = true;

        for (size_t frame = 0u; frame < animation->frame_count; ++frame) {
            int asset = state->catalog.asset_for_sprite[animation->frames[frame].sprite_index];

            frames[frame] = asset >= 0 ? state->results[asset].model : NULL;
            complete = complete && frames[frame] != NULL;
        }
        if (!complete) {
            continue;
        }
        (void)snprintf(leaf, sizeof(leaf), "%s.vox", animation->name);
        join_path(path, animation_directory, leaf);
        if (!wolf_vox_write_file(path, frames, animation->frame_count, wolf_game_palette_rgba)) {
            fprintf(stderr, "error: could not write %s\n", path);
            continue;
        }
        if (!wolf_vox_read_file(path, &document, error, sizeof(error))) {
            fprintf(stderr, "error: %s: %s\n", path, error);
            continue;
        }
        state->animation_written[animation_index] = document.model_count == animation->frame_count;
        for (size_t frame = 0u; frame < document.model_count && state->animation_written[animation_index]; ++frame) {
            state->animation_written[animation_index] = models_identical(frames[frame], document.models[frame]) &&
                (animation->frame_count == 1u || document.frame_indices[frame] == (int32_t)frame);
        }
        wolf_vox_document_destroy(&document);
    }
}

static void write_metrics_json(FILE *file, const AssetResult *result)
{
    fprintf(file, "\"validation\": {\"passed\": %s, \"readback_matches\": %s, "
        "\"mean_silhouette_iou\": %.4f, \"minimum_silhouette_iou\": %.4f, "
        "\"mean_colour_accuracy\": %.4f, \"mean_colour_error\": %.2f, \"views\": [",
        result->passed ? "true" : "false", result->readback_matches ? "true" : "false",
        result->mean_silhouette_iou, result->minimum_silhouette_iou,
        result->mean_colour_accuracy, result->mean_colour_error);
    for (size_t view = 0u; view < result->view_count; ++view) {
        fprintf(file, "%s{\"rotation\": %u, \"silhouette_iou\": %.4f, \"colour_accuracy\": %.4f}",
            view > 0u ? ", " : "", (unsigned)view,
            result->views[view].silhouette_iou, result->views[view].colour_accuracy);
    }
    fprintf(file, "]}");
}

/* The manifest is the engine-facing index: which file holds which sprite, and every animation. */
static bool write_manifest(const PipelineState *state, const char *path)
{
    FILE *file = wolf_open_file(path, "w");
    bool first = true;

    if (file == NULL) {
        return false;
    }
    fprintf(file, "{\n  \"source\": \"VSWAP.WL6\",\n");
    fprintf(file, "  \"conventions\": {\"voxels_per_tile\": %d, \"axes\": \"x right, y away from the front viewer, z up\", "
        "\"facing\": \"-y\", \"origin\": \"actor axis at x=32,y=32; floor at z=0\", "
        "\"rotation\": \"rotation k is viewed from 45*k degrees anticlockwise (from above) of the facing direction\", "
        "\"colour\": \"VOX colour n+1 is Wolfenstein palette entry n\", \"tic_rate_hz\": 70},\n", WOLF_VOXEL_DIM);

    fprintf(file, "  \"models\": [\n");
    for (size_t asset_index = 0u; asset_index < state->catalog.asset_count; ++asset_index) {
        const WolfVoxelAsset *asset = &state->catalog.assets[asset_index];
        const AssetResult *result = &state->results[asset_index];

        if (!result->written) {
            continue;
        }
        fprintf(file, "%s    {\"name\": \"%s\", \"file\": \"vox/models/%s.vox\", \"kind\": \"%s\", "
            "\"description\": \"%s\", \"sprites\": [",
            first ? "" : ",\n", asset->name, asset->name,
            asset->kind == WOLF_ASSET_ROTATION_SET ? "rotation_set" : "single_view",
            asset->description != NULL ? asset->description : "");
        for (int sprite = 0; sprite < asset->sprite_count; ++sprite) {
            fprintf(file, "%s\"%s\"", sprite > 0 ? ", " : "", wolf_wl6_sprite_names[asset->first_sprite + sprite]);
        }
        fprintf(file, "], \"first_sprite_index\": %d, ", asset->first_sprite);
        if (asset->kind == WOLF_ASSET_SINGLE_VIEW) {
            fprintf(file, "\"depth_profile\": \"%s\", \"depth_scale\": %.3f, ",
                profile_name(asset->single_view.profile), result->depth_scale_used);
        } else {
            fprintf(file, "\"depth_ratio\": %.3f, ", result->depth_scale_used);
        }
        fprintf(file, "\"voxels\": %u, ", (unsigned)result->voxel_count);
        write_metrics_json(file, result);
        fprintf(file, "}");
        first = false;
    }
    fprintf(file, "\n  ],\n  \"animations\": [\n");

    first = true;
    for (size_t animation_index = 0u; animation_index < state->animation_count; ++animation_index) {
        const WolfAnimation *animation = &state->animations[animation_index];

        if (!state->animation_written[animation_index]) {
            continue;
        }
        fprintf(file, "%s    {\"name\": \"%s\", \"file\": \"vox/animations/%s.vox\", \"loop_frame\": %d, "
            "\"next\": \"%s\", \"notes\": \"%s\", \"frames\": [",
            first ? "" : ",\n", animation->name, animation->name, animation->loop_frame,
            animation->next_animation, animation->notes != NULL ? animation->notes : "");
        for (size_t frame = 0u; frame < animation->frame_count; ++frame) {
            const WolfAnimationFrame *entry = &animation->frames[frame];
            int asset = state->catalog.asset_for_sprite[entry->sprite_index];

            fprintf(file, "%s{\"state\": \"%s\", \"sprite\": \"%s\", \"model\": \"%s\", \"tics\": %d, \"rotates\": %s}",
                frame > 0u ? ", " : "",
                entry->state_index >= 0 ? wolf_wl6_states[entry->state_index].name : "",
                wolf_wl6_sprite_names[entry->sprite_index],
                state->catalog.assets[asset].name, entry->tics,
                entry->rotate == 1 ? "true" : "false");
        }
        fprintf(file, "]}");
        first = false;
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
    fprintf(file, "model,kind,views,voxels,mean_silhouette_iou,min_silhouette_iou,mean_colour_accuracy,mean_colour_error,readback,passed\n");
    for (size_t asset_index = 0u; asset_index < state->catalog.asset_count; ++asset_index) {
        const AssetResult *result = &state->results[asset_index];

        if (!result->written) {
            continue;
        }
        fprintf(file, "%s,%s,%u,%u,%.4f,%.4f,%.4f,%.2f,%s,%s\n",
            state->catalog.assets[asset_index].name,
            state->catalog.assets[asset_index].kind == WOLF_ASSET_ROTATION_SET ? "rotation_set" : "single_view",
            (unsigned)result->view_count, (unsigned)result->voxel_count,
            result->mean_silhouette_iou, result->minimum_silhouette_iou,
            result->mean_colour_accuracy, result->mean_colour_error,
            result->readback_matches ? "match" : "MISMATCH", result->passed ? "pass" : "FAIL");
    }
    return fclose(file) == 0;
}

/* A browsable report: every model's comparison sheet with its scores. */
static bool write_html_report(const PipelineState *state, const char *path)
{
    FILE *file = wolf_open_file(path, "w");

    if (file == NULL) {
        return false;
    }
    fprintf(file, "<!doctype html><html><head><meta charset=\"utf-8\"><title>Sprite voxel validation</title>"
        "<style>body{font-family:system-ui,sans-serif;background:#1e2029;color:#e8e8ee;margin:16px}"
        "section{margin:0 0 28px}img{image-rendering:pixelated;max-width:100%%;display:block}"
        "h2{font-size:16px;margin:4px 0}.pass{color:#7ddc7d}.fail{color:#ff6b6b}"
        "p{margin:2px 0;font-size:13px;color:#b8bccc}</style></head><body>"
        "<h1>Sprite voxel validation</h1><p>Rows per model: original frame, voxel model rendered from the same "
        "rotation, difference (grey match, yellow colour differs, red missing, blue extra). Right: shaded "
        "three-quarter previews.</p>");
    for (size_t asset_index = 0u; asset_index < state->catalog.asset_count; ++asset_index) {
        const WolfVoxelAsset *asset = &state->catalog.assets[asset_index];
        const AssetResult *result = &state->results[asset_index];

        if (!result->written) {
            continue;
        }
        fprintf(file, "<section><h2>%s <span class=\"%s\">%s</span></h2>"
            "<p>%s &middot; %s &middot; %u voxels &middot; silhouette IoU mean %.3f (min %.3f) "
            "&middot; exact colour %.1f%% &middot; mean colour error %.1f</p>"
            "<img src=\"%s.png\" alt=\"%s comparison\"></section>",
            asset->name, result->passed ? "pass" : "fail", result->passed ? "PASS" : "FAIL",
            asset->description != NULL ? asset->description : "",
            asset->kind == WOLF_ASSET_ROTATION_SET ? "8 rotations" : profile_name(asset->single_view.profile),
            (unsigned)result->voxel_count, result->mean_silhouette_iou, result->minimum_silhouette_iou,
            result->mean_colour_accuracy * 100.0, result->mean_colour_error, asset->name, asset->name);
    }
    fprintf(file, "</body></html>\n");
    return fclose(file) == 0;
}

int wolf_pipeline_run(const WolfPipelineOptions *options)
{
    PipelineState *state = calloc(1u, sizeof(PipelineState));
    char sprite_directory[PATH_LENGTH];
    char model_directory[PATH_LENGTH];
    char animation_directory[PATH_LENGTH];
    char validation_directory[PATH_LENGTH];
    char path[PATH_LENGTH];
    char error[256];
    size_t processed = 0u;
    size_t passed = 0u;
    size_t animations_written = 0u;
    int exit_code = 0;

    if (state == NULL) {
        fprintf(stderr, "error: out of memory\n");
        return 1;
    }
    state->options = options;

    if (!wolf_sprite_archive_load(options->vswap_path, &state->archive, error, sizeof(error))) {
        fprintf(stderr, "error: %s: %s\n", options->vswap_path, error);
        free(state);
        return 1;
    }
    if (state->archive.sprite_count != WOLF_WL6_SPRITE_COUNT) {
        fprintf(stderr, "error: expected %d WL6 sprites but found %u; only retail WL6 data is supported\n",
            WOLF_WL6_SPRITE_COUNT, (unsigned)state->archive.sprite_count);
        wolf_sprite_archive_destroy(&state->archive);
        free(state);
        return 1;
    }

    join_path(sprite_directory, options->output_directory, "sprites");
    join_path(model_directory, options->output_directory, "vox/models");
    join_path(animation_directory, options->output_directory, "vox/animations");
    join_path(validation_directory, options->output_directory, "validation");
    if (!wolf_make_directories(sprite_directory) || !wolf_make_directories(model_directory) ||
        !wolf_make_directories(animation_directory) || !wolf_make_directories(validation_directory)) {
        fprintf(stderr, "error: could not create output directories under %s\n", options->output_directory);
        wolf_sprite_archive_destroy(&state->archive);
        free(state);
        return 1;
    }

    printf("Decoded %u sprites from %s\n", (unsigned)state->archive.sprite_count, options->vswap_path);
    if (!export_sprite_images(state, sprite_directory)) {
        exit_code = 1;
    }

    wolf_asset_catalog_build(&state->catalog);
    state->animation_count = wolf_animation_catalog_build(state->animations, WOLF_ANIMATION_CAPACITY);

    /*
     * Rotation sets go first: camera-facing frames of the same actor take
     * their body depth from the rotation-set model.
     */
    for (int pass = 0; pass < 2; ++pass) {
        WolfAssetKind kind = pass == 0 ? WOLF_ASSET_ROTATION_SET : WOLF_ASSET_SINGLE_VIEW;

        for (size_t asset = 0u; asset < state->catalog.asset_count; ++asset) {
            bool needed_as_reference = kind == WOLF_ASSET_ROTATION_SET && options->only_asset != NULL;

            if (state->catalog.assets[asset].kind != kind ||
                (!asset_selected(state, asset) && !needed_as_reference)) {
                continue;
            }
            reconstruct_asset(state, asset);
            if (!asset_selected(state, asset)) {
                continue;
            }
            if (!state->results[asset].processed) {
                fprintf(stderr, "error: could not reconstruct %s\n", state->catalog.assets[asset].name);
                exit_code = 1;
                continue;
            }
            write_and_validate_asset(state, asset, model_directory, validation_directory);
            ++processed;
            passed += state->results[asset].passed ? 1u : 0u;
            printf("  %-24s %-12s voxels %6u  IoU %.3f (min %.3f)  colour %5.1f%%  %s\n",
                state->catalog.assets[asset].name,
                kind == WOLF_ASSET_ROTATION_SET ? "8 rotations" : profile_name(state->catalog.assets[asset].single_view.profile),
                (unsigned)state->results[asset].voxel_count,
                state->results[asset].mean_silhouette_iou, state->results[asset].minimum_silhouette_iou,
                state->results[asset].mean_colour_accuracy * 100.0,
                state->results[asset].passed ? "pass" : "FAIL");
        }
    }

    if (options->only_asset == NULL) {
        write_animations(state, animation_directory);
        for (size_t animation = 0u; animation < state->animation_count; ++animation) {
            animations_written += state->animation_written[animation] ? 1u : 0u;
        }
        if (animations_written != state->animation_count) {
            fprintf(stderr, "error: only %u of %u animations were written and verified\n",
                (unsigned)animations_written, (unsigned)state->animation_count);
            exit_code = 1;
        }
    }

    join_path(path, options->output_directory, "manifest.json");
    if (!write_manifest(state, path)) {
        fprintf(stderr, "error: could not write %s\n", path);
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

    printf("Models: %u written, %u passed validation. Animations: %u written.\n",
        (unsigned)processed, (unsigned)passed, (unsigned)animations_written);
    if (passed != processed) {
        exit_code = 1;
    }

    for (size_t asset = 0u; asset < state->catalog.asset_count; ++asset) {
        wolf_voxel_model_destroy(state->results[asset].model);
    }
    wolf_sprite_archive_destroy(&state->archive);
    free(state);
    return exit_code;
}
