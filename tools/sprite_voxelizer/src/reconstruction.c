#include "reconstruction.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_VIEWS 16

WolfMultiViewOptions wolf_multi_view_default_options(void)
{
    WolfMultiViewOptions options;

    options.maximum_registration_shift = 3;
    options.minimum_fragment_voxels = 20u;
    options.prune_excess_voxels = true;
    options.minimum_face_neighbours = 3;
    return options;
}

bool wolf_rotation_is_face_on(double rotation_degrees)
{
    double remainder = fmod(fabs(rotation_degrees), 90.0);

    return remainder < 1e-6 || 90.0 - remainder < 1e-6;
}

/* Canvas column a voxel centre projects to in a registered view. */
static int centre_column(const WolfViewBasis *basis, int x, int y, int shift)
{
    double screen_u = wolf_voxel_center_offset(x) * basis->right_x + wolf_voxel_center_offset(y) * basis->right_y;

    return (int)floor(screen_u + (double)(WOLF_SPRITE_SIZE / 2)) + shift;
}

static bool view_draws(const WolfSpriteImage *view, int column, int row)
{
    return column >= 0 && column < WOLF_SPRITE_SIZE && view->opaque[wolf_sprite_pixel_index(column, row)] != 0u;
}

/*
 * Carve the hull of the listed views. Opposite views look along the same
 * axis and should draw mirror-image outlines, but the artists' front and
 * back (or left and right) frames disagree by a few pixels. Intersecting
 * them would cut away material one of them draws, so for each viewing axis
 * a voxel survives if either view of the pair draws it; the axes are then
 * intersected. Excess this leaves is pruned afterwards.
 */
static void carve_hull(
    const WolfSpriteImage *const *views,
    const WolfViewBasis *bases,
    const int *shifts,
    const size_t *view_list,
    size_t list_count,
    WolfVoxelModel *model)
{
    int axis_of[MAX_VIEWS];
    int axis_count = 0;

    for (size_t entry = 0u; entry < list_count; ++entry) {
        const WolfViewBasis *basis = &bases[view_list[entry]];

        axis_of[entry] = -1;
        for (size_t earlier = 0u; earlier < entry; ++earlier) {
            const WolfViewBasis *other = &bases[view_list[earlier]];

            if (fabs(basis->right_x * other->right_x + basis->right_y * other->right_y) > 0.999) {
                axis_of[entry] = axis_of[earlier];
                break;
            }
        }
        if (axis_of[entry] < 0) {
            axis_of[entry] = axis_count++;
        }
    }

    wolf_voxel_model_clear(model);
    for (int z = 0; z < WOLF_VOXEL_DIM; ++z) {
        int row = wolf_voxel_row_for_layer(z);

        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                bool axis_drawn[MAX_VIEWS] = {false};
                bool inside = true;

                for (size_t entry = 0u; entry < list_count; ++entry) {
                    size_t view = view_list[entry];

                    if (!axis_drawn[axis_of[entry]]) {
                        axis_drawn[axis_of[entry]] =
                            view_draws(views[view], centre_column(&bases[view], x, y, shifts[view]), row);
                    }
                }
                for (int axis = 0; axis < axis_count && inside; ++axis) {
                    inside = axis_drawn[axis];
                }
                model->occupied[wolf_voxel_index(x, y, z)] = inside ? 1u : 0u;
            }
        }
    }
}

/* Silhouette overlap (IoU) between a frame and the model rendered from that frame's direction. */
static double view_agreement(const WolfVoxelModel *model, const WolfSpriteImage *view, double rotation_degrees, int shift)
{
    WolfSpriteImage rendered;
    size_t shared = 0u;
    size_t either = 0u;

    wolf_voxel_render_view(model, rotation_degrees, shift, &rendered, NULL);
    for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        shared += (rendered.opaque[pixel] && view->opaque[pixel]) ? 1u : 0u;
        either += (rendered.opaque[pixel] || view->opaque[pixel]) ? 1u : 0u;
    }
    return either > 0u ? (double)shared / (double)either : 1.0;
}

static double hull_agreement(
    const WolfSpriteImage *const *views,
    const double *rotation_degrees,
    const WolfViewBasis *bases,
    const int *shifts,
    const size_t *view_list,
    size_t list_count,
    WolfVoxelModel *scratch)
{
    double total = 0.0;

    carve_hull(views, bases, shifts, view_list, list_count, scratch);
    for (size_t entry = 0u; entry < list_count; ++entry) {
        size_t view = view_list[entry];

        total += view_agreement(scratch, views[view], rotation_degrees[view], shifts[view]);
    }
    return total / (double)list_count;
}

/*
 * Register the face-on frames to one axis. Opposite views of a real object
 * are mirror images; the hand-drawn frames typically break that by a pixel
 * or two, which would carve away material every frame shows. Coordinate
 * ascent over whole-column shifts maximises the mean silhouette agreement
 * of the hull; the first frame stays fixed so the actor stays centred.
 */
static void register_face_on_views(
    const WolfSpriteImage *const *views,
    const double *rotation_degrees,
    const WolfViewBasis *bases,
    const size_t *view_list,
    size_t list_count,
    int maximum_shift,
    WolfVoxelModel *scratch,
    int *shifts)
{
    double best_score;

    if (maximum_shift <= 0 || list_count < 2u) {
        return;
    }
    best_score = hull_agreement(views, rotation_degrees, bases, shifts, view_list, list_count, scratch);
    for (int pass = 0; pass < 4; ++pass) {
        bool improved = false;

        for (size_t entry = 1u; entry < list_count; ++entry) {
            size_t view = view_list[entry];
            int kept_shift = shifts[view];

            for (int candidate = -maximum_shift; candidate <= maximum_shift; ++candidate) {
                double score;

                if (candidate == kept_shift) {
                    continue;
                }
                shifts[view] = candidate;
                score = hull_agreement(views, rotation_degrees, bases, shifts, view_list, list_count, scratch);
                /* Require a real gain so ties keep the shift already chosen. */
                if (score > best_score + 1e-9) {
                    best_score = score;
                    kept_shift = candidate;
                    improved = true;
                }
            }
            shifts[view] = kept_shift;
        }
        if (!improved) {
            break;
        }
    }
}

/*
 * Remove small disconnected specks left where inconsistent drawings
 * disagree; no frame actually depicts them.
 */
static void remove_small_fragments(WolfVoxelModel *model, size_t minimum_voxels)
{
    uint8_t *visited = calloc(WOLF_VOXEL_COUNT, 1u);
    size_t *queue = malloc(sizeof(size_t) * WOLF_VOXEL_COUNT);
    static const int offsets[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };

    if (visited == NULL || queue == NULL || minimum_voxels <= 1u) {
        free(visited);
        free(queue);
        return;
    }
    for (size_t seed = 0u; seed < WOLF_VOXEL_COUNT; ++seed) {
        size_t head = 0u;
        size_t tail = 0u;

        if (!model->occupied[seed] || visited[seed]) {
            continue;
        }
        visited[seed] = 1u;
        queue[tail++] = seed;
        while (head < tail) {
            size_t voxel = queue[head++];
            int x = (int)(voxel % WOLF_VOXEL_DIM);
            int y = (int)((voxel / WOLF_VOXEL_DIM) % WOLF_VOXEL_DIM);
            int z = (int)(voxel / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM));

            for (int face = 0; face < 6; ++face) {
                int nx = x + offsets[face][0];
                int ny = y + offsets[face][1];
                int nz = z + offsets[face][2];
                size_t neighbour;

                if (!wolf_voxel_in_bounds(nx, ny, nz)) {
                    continue;
                }
                neighbour = wolf_voxel_index(nx, ny, nz);
                if (model->occupied[neighbour] && !visited[neighbour]) {
                    visited[neighbour] = 1u;
                    queue[tail++] = neighbour;
                }
            }
        }
        /* The queue holds exactly this component, so it can be erased in place. */
        if (tail < minimum_voxels) {
            for (size_t member = 0u; member < tail; ++member) {
                model->occupied[queue[member]] = 0u;
            }
        }
    }
    free(visited);
    free(queue);
}

/*
 * Frames that must reject a voxel before pruning may remove it. The phantom
 * limbs of a four-view hull are rejected by several diagonal frames at once;
 * a single dissenting frame (a head drawn a little narrower on one diagonal)
 * is not enough, or the solid is carved into thin plus-shaped shells that
 * read as loose flakes.
 */
#define PRUNE_MINIMUM_REJECTIONS 2

typedef struct PruneCandidate {
    uint32_t voxel;
    /* Number of frames whose silhouette rejects the voxel centre. */
    int rejections;
    /* Squared distance from the actor axis; outer material is pruned first. */
    double radius_squared;
} PruneCandidate;

static int compare_prune_candidates(const void *left, const void *right)
{
    const PruneCandidate *a = left;
    const PruneCandidate *b = right;

    if (a->rejections != b->rejections) {
        return a->rejections > b->rejections ? -1 : 1;
    }
    if (a->radius_squared != b->radius_squared) {
        return a->radius_squared > b->radius_squared ? -1 : 1;
    }
    return a->voxel < b->voxel ? -1 : (a->voxel > b->voxel ? 1 : 0);
}

/*
 * Four orthogonal silhouettes cannot tell which leg is forward, so the
 * face-on hull of a walking figure holds both legs in both positions, and
 * the paired hull also keeps whatever only one of two opposite frames
 * draws. Remove every voxel at least two frames reject, most-rejected and
 * outermost first, as long as each drawn pixel of every face-on frame
 * remains covered by another voxel. The face-on frames are therefore always reproduced in
 * outline, while phantom and excess material is cut away; the diagonal
 * frames decide which of the ambiguous material survives.
 */
static void prune_excess_voxels(
    const WolfSpriteImage *const *views,
    const WolfViewBasis *bases,
    const int *shifts,
    size_t view_count,
    WolfVoxelModel *model)
{
    uint16_t *coverage = calloc(view_count * WOLF_SPRITE_PIXELS, sizeof(uint16_t));
    PruneCandidate *candidates = malloc(sizeof(PruneCandidate) * WOLF_VOXEL_COUNT);
    bool face_on[MAX_VIEWS];
    size_t candidate_count = 0u;

    if (coverage == NULL || candidates == NULL) {
        free(coverage);
        free(candidates);
        return;
    }
    for (size_t view = 0u; view < view_count; ++view) {
        face_on[view] = fabs(bases[view].half_footprint - 0.5) < 1e-6;
    }

    for (int z = 0; z < WOLF_VOXEL_DIM; ++z) {
        int row = wolf_voxel_row_for_layer(z);

        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                size_t voxel = wolf_voxel_index(x, y, z);
                int rejections = 0;

                if (!model->occupied[voxel]) {
                    continue;
                }
                for (size_t view = 0u; view < view_count; ++view) {
                    int first_column;
                    int last_column;
                    bool rejected = !view_draws(views[view], centre_column(&bases[view], x, y, shifts[view]), row);

                    rejections += rejected ? 1 : 0;
                    wolf_voxel_footprint_columns(&bases[view], x, y, shifts[view], &first_column, &last_column);
                    for (int column = first_column; column <= last_column; ++column) {
                        if (column >= 0 && column < WOLF_SPRITE_SIZE) {
                            ++coverage[view * WOLF_SPRITE_PIXELS + wolf_sprite_pixel_index(column, row)];
                        }
                    }
                }
                if (rejections >= PRUNE_MINIMUM_REJECTIONS) {
                    candidates[candidate_count].voxel = (uint32_t)voxel;
                    candidates[candidate_count].rejections = rejections;
                    candidates[candidate_count].radius_squared =
                        wolf_voxel_center_offset(x) * wolf_voxel_center_offset(x) +
                        wolf_voxel_center_offset(y) * wolf_voxel_center_offset(y);
                    ++candidate_count;
                }
            }
        }
    }
    qsort(candidates, candidate_count, sizeof(PruneCandidate), compare_prune_candidates);

    /*
     * Peel from the outside in: a candidate is removed only once it is on
     * the surface. Removing interior voxels never narrows any silhouette, so
     * doing so would only hollow the solid into thin shells that look like
     * loose flakes from other angles. Each pass exposes the next layer.
     */
    for (int pass = 0; pass < WOLF_VOXEL_DIM; ++pass) {
        size_t removed = 0u;

        for (size_t candidate = 0u; candidate < candidate_count; ++candidate) {
            size_t voxel = candidates[candidate].voxel;
            int x = (int)(voxel % WOLF_VOXEL_DIM);
            int y = (int)((voxel / WOLF_VOXEL_DIM) % WOLF_VOXEL_DIM);
            int z = (int)(voxel / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM));
            int row = wolf_voxel_row_for_layer(z);
            bool removable = model->occupied[voxel] && wolf_voxel_is_surface(model, x, y, z);

            for (size_t view = 0u; view < view_count && removable; ++view) {
                int first_column;
                int last_column;

                if (!face_on[view]) {
                    continue;
                }
                wolf_voxel_footprint_columns(&bases[view], x, y, shifts[view], &first_column, &last_column);
                for (int column = first_column; column <= last_column; ++column) {
                    size_t pixel;

                    if (column < 0 || column >= WOLF_SPRITE_SIZE) {
                        continue;
                    }
                    pixel = wolf_sprite_pixel_index(column, row);
                    if (views[view]->opaque[pixel] && coverage[view * WOLF_SPRITE_PIXELS + pixel] < 2u) {
                        removable = false;
                        break;
                    }
                }
            }
            if (!removable) {
                continue;
            }
            model->occupied[voxel] = 0u;
            ++removed;
            for (size_t view = 0u; view < view_count; ++view) {
                int first_column;
                int last_column;

                wolf_voxel_footprint_columns(&bases[view], x, y, shifts[view], &first_column, &last_column);
                for (int column = first_column; column <= last_column; ++column) {
                    if (column >= 0 && column < WOLF_SPRITE_SIZE) {
                        --coverage[view * WOLF_SPRITE_PIXELS + wolf_sprite_pixel_index(column, row)];
                    }
                }
            }
        }
        if (removed == 0u) {
            break;
        }
    }
    free(coverage);
    free(candidates);
}

/* Voxels of the body near one layer, collected once per repaired row. */
typedef struct NearbyBody {
    int x[WOLF_VOXEL_COUNT / 8];
    int y[WOLF_VOXEL_COUNT / 8];
    int z[WOLF_VOXEL_COUNT / 8];
    int count;
} NearbyBody;

/* Layers either side of a repaired row searched for the body it must attach to. */
#define REPAIR_LAYER_REACH 3
/* Squared distance beyond which a restored part would float free, in voxels. */
#define REPAIR_MAXIMUM_DISTANCE_SQUARED 25.0
/* Cost, in squared voxels, of restoring a pixel where another face-on frame draws nothing. */
#define REPAIR_OUTSIDE_OTHER_FRAME_PENALTY 30.0

/*
 * Where the face-on frames disagree about a limb (the side frame's stride
 * foot sits at a height the front frame does not draw), intersecting their
 * silhouettes cuts the limb away. For every face-on drawn pixel left
 * uncovered, add voxels on that pixel's line of sight at the position
 * closest in 3D to the existing body, searching neighbouring layers too, so
 * a foot is restored under its own leg and an ear on the head rather than
 * on the actor's axis. Restored parts are three voxels deep so a foot
 * becomes a block rather than a paper-thin plate.
 */
static void repair_face_on_coverage(
    const WolfSpriteImage *const *views,
    const double *rotation_degrees,
    const WolfViewBasis *bases,
    const int *shifts,
    const size_t *view_list,
    size_t list_count,
    WolfVoxelModel *model)
{
    WolfSpriteImage rendered;
    NearbyBody *nearby = malloc(sizeof(NearbyBody));

    if (nearby == NULL) {
        return;
    }
    for (size_t entry = 0u; entry < list_count; ++entry) {
        size_t view = view_list[entry];

        wolf_voxel_render_view(model, rotation_degrees[view], shifts[view], &rendered, NULL);
        for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
            int z = wolf_voxel_row_for_layer(row);

            /* The body as it stands now, so parts restored in earlier rows count as body. */
            nearby->count = 0;
            for (int layer = z - REPAIR_LAYER_REACH; layer <= z + REPAIR_LAYER_REACH; ++layer) {
                if (layer < 0 || layer >= WOLF_VOXEL_DIM) {
                    continue;
                }
                for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
                    for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                        if (model->occupied[wolf_voxel_index(x, y, layer)] &&
                            nearby->count < (int)(sizeof(nearby->x) / sizeof(nearby->x[0]))) {
                            nearby->x[nearby->count] = x;
                            nearby->y[nearby->count] = y;
                            nearby->z[nearby->count] = layer;
                            ++nearby->count;
                        }
                    }
                }
            }
            for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
                size_t pixel = wolf_sprite_pixel_index(column, row);
                int best_x = -1;
                int best_y = -1;
                double best_distance = DBL_MAX;

                if (!views[view]->opaque[pixel] || rendered.opaque[pixel]) {
                    continue;
                }
                double best_score = DBL_MAX;

                for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
                    for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                        double nearest = DBL_MAX;
                        double score;

                        if (centre_column(&bases[view], x, y, shifts[view]) != column) {
                            continue;
                        }
                        for (int member = 0; member < nearby->count; ++member) {
                            double dx = (double)(x - nearby->x[member]);
                            double dy = (double)(y - nearby->y[member]);
                            double dz = (double)(z - nearby->z[member]);
                            double distance = dx * dx + dy * dy + dz * dz;

                            nearest = distance < nearest ? distance : nearest;
                        }
                        /*
                         * Prefer a position the other face-on frames also draw, so
                         * restoring one frame's outline does not spoil another's.
                         */
                        score = nearest;
                        for (size_t other = 0u; other < list_count; ++other) {
                            size_t other_view = view_list[other];

                            if (other_view != view && !view_draws(views[other_view],
                                    centre_column(&bases[other_view], x, y, shifts[other_view]), row)) {
                                score += REPAIR_OUTSIDE_OTHER_FRAME_PENALTY;
                            }
                        }
                        if (score < best_score) {
                            best_score = score;
                            best_distance = nearest;
                            best_x = x;
                            best_y = y;
                        }
                    }
                }
                /*
                 * Only restore parts that attach to the body; a pixel whose
                 * line of sight passes nowhere near it is a contradiction
                 * between frames, and filling it would leave a floating block.
                 */
                if (best_x >= 0 && best_distance <= REPAIR_MAXIMUM_DISTANCE_SQUARED) {
                    int step_x = (int)lround(bases[view].toward_viewer_x);
                    int step_y = (int)lround(bases[view].toward_viewer_y);

                    for (int offset = -1; offset <= 1; ++offset) {
                        int x = best_x + offset * step_x;
                        int y = best_y + offset * step_y;

                        if (wolf_voxel_in_bounds(x, y, z)) {
                            model->occupied[wolf_voxel_index(x, y, z)] = 1u;
                        }
                    }
                }
            }
        }
    }
    free(nearby);
}

/* Bevel passes applied to the vertical edges of a carved head. */
#define BEVEL_PASSES 3
/* Widest and deepest a figure's top may be, in voxels, to count as a head. */
#define HEAD_MAXIMUM_EXTENT 16

/*
 * Count, for every face-on frame, how many voxels cover each pixel. Used by
 * the clean-up steps to remove material only where a face-on drawn pixel
 * stays covered by something else.
 */
static void count_face_on_coverage(
    const WolfViewBasis *bases,
    const int *shifts,
    const size_t *view_list,
    size_t list_count,
    const WolfVoxelModel *model,
    uint16_t *coverage)
{
    memset(coverage, 0, sizeof(uint16_t) * list_count * WOLF_SPRITE_PIXELS);
    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        int x = (int)(voxel % WOLF_VOXEL_DIM);
        int y = (int)((voxel / WOLF_VOXEL_DIM) % WOLF_VOXEL_DIM);
        int row = wolf_voxel_row_for_layer((int)(voxel / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM)));

        if (!model->occupied[voxel]) {
            continue;
        }
        for (size_t entry = 0u; entry < list_count; ++entry) {
            int first_column;
            int last_column;

            wolf_voxel_footprint_columns(&bases[view_list[entry]], x, y, shifts[view_list[entry]],
                &first_column, &last_column);
            for (int column = first_column; column <= last_column; ++column) {
                if (column >= 0 && column < WOLF_SPRITE_SIZE) {
                    ++coverage[entry * WOLF_SPRITE_PIXELS + wolf_sprite_pixel_index(column, row)];
                }
            }
        }
    }
}

/*
 * Remove one voxel if every face-on drawn pixel it covers is also covered by
 * another voxel, keeping the coverage counts up to date. Returns whether the
 * voxel was removed.
 */
static bool remove_if_covered(
    const WolfSpriteImage *const *views,
    const WolfViewBasis *bases,
    const int *shifts,
    const size_t *view_list,
    size_t list_count,
    uint16_t *coverage,
    WolfVoxelModel *model,
    size_t voxel)
{
    int x = (int)(voxel % WOLF_VOXEL_DIM);
    int y = (int)((voxel / WOLF_VOXEL_DIM) % WOLF_VOXEL_DIM);
    int row = wolf_voxel_row_for_layer((int)(voxel / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM)));

    for (size_t entry = 0u; entry < list_count; ++entry) {
        int first_column;
        int last_column;

        wolf_voxel_footprint_columns(&bases[view_list[entry]], x, y, shifts[view_list[entry]], &first_column, &last_column);
        for (int column = first_column; column <= last_column; ++column) {
            size_t pixel;

            if (column < 0 || column >= WOLF_SPRITE_SIZE) {
                continue;
            }
            pixel = wolf_sprite_pixel_index(column, row);
            if (views[view_list[entry]]->opaque[pixel] && coverage[entry * WOLF_SPRITE_PIXELS + pixel] < 2u) {
                return false;
            }
        }
    }
    model->occupied[voxel] = 0u;
    for (size_t entry = 0u; entry < list_count; ++entry) {
        int first_column;
        int last_column;

        wolf_voxel_footprint_columns(&bases[view_list[entry]], x, y, shifts[view_list[entry]], &first_column, &last_column);
        for (int column = first_column; column <= last_column; ++column) {
            if (column >= 0 && column < WOLF_SPRITE_SIZE) {
                --coverage[entry * WOLF_SPRITE_PIXELS + wolf_sprite_pixel_index(column, row)];
            }
        }
    }
    return true;
}

/*
 * Bevel the vertical edges of the head. Four orthogonal silhouettes carve a
 * square cross-section, so the head comes out boxy: the front frame's outer
 * eye sits on the flat face several voxels in from the corner, and the side
 * frame paints its own view of that eye on the side of the box, giving the
 * figure a third eye. Peeling the head's vertical corners for a few passes
 * rounds it, so the front eye becomes the corner seen from the side and the
 * side frame's eye lands on that same voxel. Only the head (the top fifth of
 * the figure, when that is head-sized) is bevelled; doing the whole body
 * cost too much of its volume.
 * A corner is removed only while every face-on drawn pixel stays covered.
 */
static void bevel_head(
    const WolfSpriteImage *const *views,
    const WolfViewBasis *bases,
    const int *shifts,
    const size_t *view_list,
    size_t list_count,
    WolfVoxelModel *model)
{
    uint16_t *coverage = malloc(sizeof(uint16_t) * list_count * WOLF_SPRITE_PIXELS);
    size_t *corners = malloc(sizeof(size_t) * WOLF_VOXEL_COUNT);
    int lowest = WOLF_VOXEL_DIM;
    int highest = -1;
    int head_bottom;

    if (coverage == NULL || corners == NULL) {
        free(coverage);
        free(corners);
        return;
    }
    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        if (model->occupied[voxel]) {
            int z = (int)(voxel / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM));

            lowest = z < lowest ? z : lowest;
            highest = z > highest ? z : highest;
        }
    }
    head_bottom = highest - (highest - lowest + 1) / 5;

    /*
     * Only a figure whose top is head-sized has a head to round; the top of a
     * dog is its ears and the length of its back.
     */
    {
        int left = WOLF_VOXEL_DIM;
        int right = -1;
        int front = WOLF_VOXEL_DIM;
        int back = -1;

        for (int z = head_bottom; z <= highest && z >= 0; ++z) {
            for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
                for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                    if (model->occupied[wolf_voxel_index(x, y, z)]) {
                        left = x < left ? x : left;
                        right = x > right ? x : right;
                        front = y < front ? y : front;
                        back = y > back ? y : back;
                    }
                }
            }
        }
        if (right < 0 || right - left + 1 > HEAD_MAXIMUM_EXTENT || back - front + 1 > HEAD_MAXIMUM_EXTENT) {
            free(coverage);
            free(corners);
            return;
        }
    }

    count_face_on_coverage(bases, shifts, view_list, list_count, model, coverage);
    for (int pass = 0; pass < BEVEL_PASSES; ++pass) {
        size_t corner_count = 0u;

        /* Corners are collected first so that one pass peels exactly one layer. */
        for (int z = head_bottom; z <= highest && z >= 0; ++z) {
            for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
                for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                    bool open_x;
                    bool open_y;

                    if (!model->occupied[wolf_voxel_index(x, y, z)]) {
                        continue;
                    }
                    open_x = !wolf_voxel_in_bounds(x - 1, y, z) || !model->occupied[wolf_voxel_index(x - 1, y, z)] ||
                        !wolf_voxel_in_bounds(x + 1, y, z) || !model->occupied[wolf_voxel_index(x + 1, y, z)];
                    open_y = !wolf_voxel_in_bounds(x, y - 1, z) || !model->occupied[wolf_voxel_index(x, y - 1, z)] ||
                        !wolf_voxel_in_bounds(x, y + 1, z) || !model->occupied[wolf_voxel_index(x, y + 1, z)];
                    if (open_x && open_y) {
                        corners[corner_count++] = wolf_voxel_index(x, y, z);
                    }
                }
            }
        }
        for (size_t corner = 0u; corner < corner_count; ++corner) {
            (void)remove_if_covered(views, bases, shifts, view_list, list_count, coverage, model, corners[corner]);
        }
    }
    free(coverage);
    free(corners);
}

/*
 * Remove weakly attached voxels: loose flakes, spurs and one-voxel strands
 * left where the frames disagree. A voxel touching fewer than
 * minimum_neighbours occupied face neighbours (the floor counts) is removed,
 * repeatedly, but only while every face-on drawn pixel it covers stays
 * covered by another voxel. Flakes that merely stick out go; a thin limb
 * that is the only thing drawing part of a frame stays, so nothing the
 * original frames show is lost.
 */
static void enforce_minimum_neighbours(
    const WolfSpriteImage *const *views,
    const WolfViewBasis *bases,
    const int *shifts,
    const size_t *view_list,
    size_t list_count,
    WolfVoxelModel *model,
    int minimum_neighbours)
{
    static const int offsets[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };
    uint16_t *coverage = malloc(sizeof(uint16_t) * list_count * WOLF_SPRITE_PIXELS);
    size_t *weak = malloc(sizeof(size_t) * WOLF_VOXEL_COUNT);

    if (coverage == NULL || weak == NULL || minimum_neighbours <= 0) {
        free(coverage);
        free(weak);
        return;
    }
    count_face_on_coverage(bases, shifts, view_list, list_count, model, coverage);
    for (int pass = 0; pass < WOLF_VOXEL_DIM; ++pass) {
        size_t weak_count = 0u;
        size_t removed = 0u;

        for (int z = 0; z < WOLF_VOXEL_DIM; ++z) {
            for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
                for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                    int count = 0;

                    if (!model->occupied[wolf_voxel_index(x, y, z)]) {
                        continue;
                    }
                    for (int face = 0; face < 6; ++face) {
                        int nx = x + offsets[face][0];
                        int ny = y + offsets[face][1];
                        int nz = z + offsets[face][2];

                        /* The floor supports a voxel standing on it, like a neighbour. */
                        if (nz < 0 || (wolf_voxel_in_bounds(nx, ny, nz) && model->occupied[wolf_voxel_index(nx, ny, nz)])) {
                            ++count;
                        }
                    }
                    if (count < minimum_neighbours) {
                        weak[weak_count++] = wolf_voxel_index(x, y, z);
                    }
                }
            }
        }
        for (size_t entry = 0u; entry < weak_count; ++entry) {
            removed += remove_if_covered(views, bases, shifts, view_list, list_count, coverage, model, weak[entry]) ? 1u : 0u;
        }
        if (removed == 0u) {
            break;
        }
    }
    free(coverage);
    free(weak);
}

/* Farthest a fragment may sit from the body and still be joined to it, in voxels. */
#define BRIDGE_MAXIMUM_GAP 6

/*
 * Join every remaining fragment to the main body. Where frames disagree, a
 * limb can be carved with a gap between it and the body (a foot below a
 * shin, a hat brim above a head); a short line of voxels from the
 * fragment's nearest voxel to the body's reattaches it. The line is
 * thickened to two voxels where that stays inside the face-on outlines. A fragment further away than
 * BRIDGE_MAXIMUM_GAP depicts nothing consistent and is removed.
 */
static void bridge_fragments_to_body(
    const WolfSpriteImage *const *views,
    const WolfViewBasis *bases,
    const int *shifts,
    const size_t *view_list,
    size_t list_count,
    WolfVoxelModel *model)
{
    int32_t *label = malloc(sizeof(int32_t) * WOLF_VOXEL_COUNT);
    size_t *queue = malloc(sizeof(size_t) * WOLF_VOXEL_COUNT);
    size_t *members = malloc(sizeof(size_t) * WOLF_VOXEL_COUNT);
    int32_t component_count = 0;
    int32_t largest = -1;
    size_t largest_size = 0u;
    static const int offsets[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };

    if (label == NULL || queue == NULL || members == NULL) {
        free(label);
        free(queue);
        free(members);
        return;
    }
    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        label[voxel] = -1;
    }
    for (size_t seed = 0u; seed < WOLF_VOXEL_COUNT; ++seed) {
        size_t head = 0u;
        size_t tail = 0u;

        if (!model->occupied[seed] || label[seed] >= 0) {
            continue;
        }
        label[seed] = component_count;
        queue[tail++] = seed;
        while (head < tail) {
            size_t voxel = queue[head++];
            int x = (int)(voxel % WOLF_VOXEL_DIM);
            int y = (int)((voxel / WOLF_VOXEL_DIM) % WOLF_VOXEL_DIM);
            int z = (int)(voxel / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM));

            for (int face = 0; face < 6; ++face) {
                int nx = x + offsets[face][0];
                int ny = y + offsets[face][1];
                int nz = z + offsets[face][2];
                size_t neighbour;

                if (!wolf_voxel_in_bounds(nx, ny, nz)) {
                    continue;
                }
                neighbour = wolf_voxel_index(nx, ny, nz);
                if (model->occupied[neighbour] && label[neighbour] < 0) {
                    label[neighbour] = component_count;
                    queue[tail++] = neighbour;
                }
            }
        }
        if (tail > largest_size) {
            largest_size = tail;
            largest = component_count;
        }
        ++component_count;
    }

    for (int32_t fragment = 0; fragment < component_count; ++fragment) {
        size_t member_count = 0u;
        long best_distance = LONG_MAX;
        size_t best_from = 0u;
        size_t best_to = 0u;

        if (fragment == largest) {
            continue;
        }
        for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
            if (label[voxel] == fragment) {
                members[member_count++] = voxel;
            }
        }
        /* Closest pair between the fragment and the body, searched within the allowed gap. */
        for (size_t member = 0u; member < member_count; ++member) {
            int x = (int)(members[member] % WOLF_VOXEL_DIM);
            int y = (int)((members[member] / WOLF_VOXEL_DIM) % WOLF_VOXEL_DIM);
            int z = (int)(members[member] / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM));

            for (int dz = -BRIDGE_MAXIMUM_GAP; dz <= BRIDGE_MAXIMUM_GAP; ++dz) {
                for (int dy = -BRIDGE_MAXIMUM_GAP; dy <= BRIDGE_MAXIMUM_GAP; ++dy) {
                    for (int dx = -BRIDGE_MAXIMUM_GAP; dx <= BRIDGE_MAXIMUM_GAP; ++dx) {
                        long distance = (long)(dx * dx + dy * dy + dz * dz);
                        size_t other;

                        if (distance >= best_distance || !wolf_voxel_in_bounds(x + dx, y + dy, z + dz)) {
                            continue;
                        }
                        other = wolf_voxel_index(x + dx, y + dy, z + dz);
                        if (label[other] == largest) {
                            best_distance = distance;
                            best_from = members[member];
                            best_to = other;
                        }
                    }
                }
            }
        }
        if (best_distance > (long)(BRIDGE_MAXIMUM_GAP * BRIDGE_MAXIMUM_GAP)) {
            for (size_t member = 0u; member < member_count; ++member) {
                model->occupied[members[member]] = 0u;
            }
            continue;
        }
        /* Step from the fragment to the body, filling every voxel on the way. */
        {
            int from[3] = {(int)(best_from % WOLF_VOXEL_DIM), (int)((best_from / WOLF_VOXEL_DIM) % WOLF_VOXEL_DIM),
                (int)(best_from / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM))};
            int to[3] = {(int)(best_to % WOLF_VOXEL_DIM), (int)((best_to / WOLF_VOXEL_DIM) % WOLF_VOXEL_DIM),
                (int)(best_to / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM))};
            int position[3] = {from[0], from[1], from[2]};
            uint8_t colour = model->palette_index[best_from];

            /*
             * Two voxels thick, so the bridge is solid rather than a strand,
             * but the thickening only goes where every face-on frame draws,
             * so a bridge never pushes past an original outline.
             */
            for (;;) {
                for (int dz = 0; dz <= 1; ++dz) {
                    for (int dy = 0; dy <= 1; ++dy) {
                        for (int dx = 0; dx <= 1; ++dx) {
                            int x = position[0] + dx;
                            int y = position[1] + dy;
                            int z = position[2] + dz;
                            bool core = dx == 0 && dy == 0 && dz == 0;
                            bool inside = true;

                            for (size_t entry = 0u; entry < list_count && inside && !core && wolf_voxel_in_bounds(x, y, z); ++entry) {
                                inside = view_draws(views[view_list[entry]],
                                    centre_column(&bases[view_list[entry]], x, y, shifts[view_list[entry]]),
                                    wolf_voxel_row_for_layer(z));
                            }
                            if (wolf_voxel_in_bounds(x, y, z) && inside && !model->occupied[wolf_voxel_index(x, y, z)]) {
                                model->occupied[wolf_voxel_index(x, y, z)] = 1u;
                                model->palette_index[wolf_voxel_index(x, y, z)] = colour;
                            }
                        }
                    }
                }
                if (position[0] == to[0] && position[1] == to[1] && position[2] == to[2]) {
                    break;
                }
                for (int axis = 0; axis < 3; ++axis) {
                    if (position[axis] != to[axis]) {
                        position[axis] += position[axis] < to[axis] ? 1 : -1;
                        break;
                    }
                }
            }
        }
        for (size_t member = 0u; member < member_count; ++member) {
            label[members[member]] = largest;
        }
    }
    free(label);
    free(queue);
    free(members);
}

/*
 * Horizontal outward normal of a surface voxel, estimated from which of its
 * neighbours are empty. Only the horizontal part matters because every
 * original frame is a horizontal view.
 */
static void estimate_horizontal_normal(const WolfVoxelModel *model, int x, int y, int z,
    double *normal_x, double *normal_y)
{
    double sum_x = 0.0;
    double sum_y = 0.0;
    double length;

    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                int nx = x + dx;
                int ny = y + dy;
                int nz = z + dz;

                if (!wolf_voxel_in_bounds(nx, ny, nz) || !model->occupied[wolf_voxel_index(nx, ny, nz)]) {
                    sum_x += (double)dx;
                    sum_y += (double)dy;
                }
            }
        }
    }
    length = sqrt(sum_x * sum_x + sum_y * sum_y);
    if (length < 1e-9) {
        *normal_x = 0.0;
        *normal_y = 0.0;
        return;
    }
    *normal_x = sum_x / length;
    *normal_y = sum_y / length;
}

/*
 * Give every voxel without a colour the colour of its nearest coloured
 * voxel, by a multi-source breadth-first flood through occupied space.
 */
static void propagate_colours(WolfVoxelModel *model, uint8_t *coloured)
{
    size_t *queue = malloc(sizeof(size_t) * WOLF_VOXEL_COUNT);
    size_t head = 0u;
    size_t tail = 0u;
    static const int offsets[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };

    if (queue == NULL) {
        return;
    }
    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        if (model->occupied[voxel] && coloured[voxel]) {
            queue[tail++] = voxel;
        }
    }
    while (head < tail) {
        size_t voxel = queue[head++];
        int x = (int)(voxel % WOLF_VOXEL_DIM);
        int y = (int)((voxel / WOLF_VOXEL_DIM) % WOLF_VOXEL_DIM);
        int z = (int)(voxel / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM));

        for (int face = 0; face < 6; ++face) {
            int nx = x + offsets[face][0];
            int ny = y + offsets[face][1];
            int nz = z + offsets[face][2];
            size_t neighbour;

            if (!wolf_voxel_in_bounds(nx, ny, nz)) {
                continue;
            }
            neighbour = wolf_voxel_index(nx, ny, nz);
            if (model->occupied[neighbour] && !coloured[neighbour]) {
                coloured[neighbour] = 1u;
                model->palette_index[neighbour] = model->palette_index[voxel];
                queue[tail++] = neighbour;
            }
        }
    }
    free(queue);
}

/* True when a drawn pixel touches transparency, i.e. lies on the frame's outline. */
static bool on_silhouette_outline(const WolfSpriteImage *view, size_t pixel)
{
    int column = (int)(pixel % WOLF_SPRITE_SIZE);
    int row = (int)(pixel / WOLF_SPRITE_SIZE);

    return !view_draws(view, column - 1, row) || !view_draws(view, column + 1, row) ||
        row == 0 || row == WOLF_SPRITE_SIZE - 1 ||
        !view->opaque[wolf_sprite_pixel_index(column, row - 1)] ||
        !view->opaque[wolf_sprite_pixel_index(column, row + 1)];
}

/*
 * Colour each visible voxel from the frame that faces it most squarely.
 * Mixing frames voxel by voxel turns the dithered shading into noise; this
 * keeps each side's texture intact. Face-on frames always take precedence
 * over diagonal ones, so the front, sides and back reproduce their own
 * frames and the independently drawn diagonals only colour what no face-on
 * frame shows. The front frame, the one the player sees most, wins ties.
 *
 * Pixel art outlines every silhouette in dark colours. A voxel on a frame's
 * outline is usually the side of the body, which another frame sees face
 * on, so outline pixels lose to any frame that sees the voxel inside its
 * silhouette; otherwise the outlines would become dark blotches on the
 * model's surfaces. The front frame is exempt: what it shows at the edge of
 * the face must stay, or the side and back frames paint their ears and hair
 * onto the cheeks. Where a part is one voxel thick the front and back
 * frames see the same voxel; the front frame's rank wins outright there.
 */
static bool colour_from_facing_views(
    const WolfSpriteImage *const *views,
    const double *rotation_degrees,
    const WolfViewBasis *bases,
    const int *shifts,
    size_t view_count,
    WolfVoxelModel *model)
{
    double *best_facing = malloc(sizeof(double) * WOLF_VOXEL_COUNT);
    uint8_t *coloured = calloc(WOLF_VOXEL_COUNT, 1u);
    int32_t voxel_for_pixel[WOLF_SPRITE_PIXELS];
    WolfSpriteImage rendered;

    if (best_facing == NULL || coloured == NULL) {
        free(best_facing);
        free(coloured);
        return false;
    }
    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        best_facing[voxel] = -DBL_MAX;
    }

    for (size_t view = 0u; view < view_count; ++view) {
        /*
         * Face-on frames outrank diagonals, and among them the front frame
         * outranks the back and the back the sides: a detail a side or back
         * frame draws near the face (an eye in profile, hair behind the ear)
         * must not be painted onto the front, which the front frame draws.
         */
        double preference = fabs(bases[view].half_footprint - 0.5) < 1e-6 ? 10.0 : 0.0;

        /* Rank steps exceed the facing term's range (-1..1), so rank always decides between face-on frames. */
        if (preference > 0.0 && bases[view].toward_viewer_y < -0.5) {
            preference += 6.0;
        } else if (preference > 0.0 && bases[view].toward_viewer_y > 0.5) {
            preference += 3.0;
        }

        wolf_voxel_render_view(model, rotation_degrees[view], shifts[view], &rendered, voxel_for_pixel);
        for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
            int32_t voxel = voxel_for_pixel[pixel];
            double normal_x;
            double normal_y;
            double facing;

            if (voxel < 0 || !views[view]->opaque[pixel]) {
                continue;
            }
            estimate_horizontal_normal(model,
                (int)((size_t)voxel % WOLF_VOXEL_DIM),
                (int)(((size_t)voxel / WOLF_VOXEL_DIM) % WOLF_VOXEL_DIM),
                (int)((size_t)voxel / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM)),
                &normal_x, &normal_y);
            facing = normal_x * bases[view].toward_viewer_x + normal_y * bases[view].toward_viewer_y;
            /* A diagonal frame never paints a surface facing away from it. */
            if (preference == 0.0 && facing < -0.1) {
                continue;
            }
            facing += preference;
            /* Outlines yield to a frame seeing the voxel inside its silhouette, except the front frame's. */
            if (on_silhouette_outline(views[view], pixel) && bases[view].toward_viewer_y > -0.5) {
                facing -= 2.5;
            }
            if (facing > best_facing[voxel]) {
                best_facing[voxel] = facing;
                model->palette_index[voxel] = views[view]->palette_index[pixel];
                coloured[voxel] = 1u;
            }
        }
    }
    propagate_colours(model, coloured);
    free(best_facing);
    free(coloured);
    return true;
}

bool wolf_reconstruct_multi_view(
    const WolfSpriteImage *const *views,
    const double *rotation_degrees,
    size_t view_count,
    const WolfMultiViewOptions *options,
    WolfVoxelModel *model,
    int *registration_shifts)
{
    WolfViewBasis bases[MAX_VIEWS];
    int shifts[MAX_VIEWS] = {0};
    size_t face_on_views[MAX_VIEWS];
    size_t face_on_count = 0u;
    bool any_diagonal = false;

    if (views == NULL || rotation_degrees == NULL || view_count == 0u || view_count > MAX_VIEWS ||
        options == NULL || model == NULL) {
        return false;
    }
    for (size_t view = 0u; view < view_count; ++view) {
        bases[view] = wolf_view_basis(rotation_degrees[view]);
        if (wolf_rotation_is_face_on(rotation_degrees[view])) {
            face_on_views[face_on_count++] = view;
        } else {
            any_diagonal = true;
        }
    }
    /* Without face-on frames, every frame defines the solid. */
    if (face_on_count == 0u) {
        for (size_t view = 0u; view < view_count; ++view) {
            face_on_views[face_on_count++] = view;
        }
        any_diagonal = false;
    }

    register_face_on_views(views, rotation_degrees, bases, face_on_views, face_on_count,
        options->maximum_registration_shift, model, shifts);
    carve_hull(views, bases, shifts, face_on_views, face_on_count, model);

    /* Each diagonal frame is aligned to the face-on solid it will refine. */
    for (size_t view = 0u; view < view_count && any_diagonal; ++view) {
        double best_score = -1.0;
        int best_shift = 0;

        if (wolf_rotation_is_face_on(rotation_degrees[view])) {
            continue;
        }
        for (int candidate = -options->maximum_registration_shift;
            candidate <= options->maximum_registration_shift; ++candidate) {
            double score = view_agreement(model, views[view], rotation_degrees[view], candidate);

            if (score > best_score + 1e-9 || (fabs(score - best_score) <= 1e-9 && abs(candidate) < abs(best_shift))) {
                best_score = score;
                best_shift = candidate;
            }
        }
        shifts[view] = best_shift;
    }

    remove_small_fragments(model, options->minimum_fragment_voxels);
    if (options->prune_excess_voxels) {
        prune_excess_voxels(views, bases, shifts, view_count, model);
        remove_small_fragments(model, options->minimum_fragment_voxels);
    }
    repair_face_on_coverage(views, rotation_degrees, bases, shifts, face_on_views, face_on_count, model);
    bevel_head(views, bases, shifts, face_on_views, face_on_count, model);
    /*
     * Repairs and clean-up can leave parts (an ear tip, a paw) cut off from
     * the body; join them back rather than deleting what a frame draws.
     * Anything too far away to join is removed by the bridging step.
     */
    bridge_fragments_to_body(views, bases, shifts, face_on_views, face_on_count, model);
    enforce_minimum_neighbours(views, bases, shifts, face_on_views, face_on_count, model, options->minimum_face_neighbours);
    bridge_fragments_to_body(views, bases, shifts, face_on_views, face_on_count, model);
    /* A last sweep for any speck the joins could not reach. */
    remove_small_fragments(model, options->minimum_fragment_voxels);
    if (registration_shifts != NULL) {
        memcpy(registration_shifts, shifts, sizeof(int) * view_count);
    }
    return colour_from_facing_views(views, rotation_degrees, bases, shifts, view_count, model);
}

/* Squared distance between two pixel centres. */
static double squared_distance(int column_a, int row_a, int column_b, int row_b)
{
    double dx = (double)(column_a - column_b);
    double dy = (double)(row_a - row_b);

    return dx * dx + dy * dy;
}

/* Is a pixel drawn? Positions off the canvas count as transparent. */
static bool pixel_opaque(const WolfSpriteImage *image, int column, int row)
{
    return column >= 0 && row >= 0 && column < WOLF_SPRITE_SIZE && row < WOLF_SPRITE_SIZE &&
        image->opaque[wolf_sprite_pixel_index(column, row)] != 0u;
}

/*
 * Inflation profile: a pixel's depth follows a circular cross-section whose
 * radius is the local thickness of the shape around it. Thin limbs become
 * thin tubes and broad bodies become broad, rounded volumes.
 */
static void inflate_half_depths(const WolfSpriteImage *image, double *half_depth)
{
    int boundary_columns[(WOLF_SPRITE_SIZE + 2) * (WOLF_SPRITE_SIZE + 2)];
    int boundary_rows[(WOLF_SPRITE_SIZE + 2) * (WOLF_SPRITE_SIZE + 2)];
    size_t boundary_count = 0u;
    double edge_distance[WOLF_SPRITE_PIXELS];
    double local_thickness[WOLF_SPRITE_PIXELS];

    /*
     * The nearest transparent pixel to any drawn pixel is always 4-adjacent
     * to a drawn pixel, so only that boundary set (including the virtual
     * border just off the canvas) needs to be searched.
     */
    for (int row = -1; row <= WOLF_SPRITE_SIZE; ++row) {
        for (int column = -1; column <= WOLF_SPRITE_SIZE; ++column) {
            if (pixel_opaque(image, column, row)) {
                continue;
            }
            if (pixel_opaque(image, column - 1, row) || pixel_opaque(image, column + 1, row) ||
                pixel_opaque(image, column, row - 1) || pixel_opaque(image, column, row + 1)) {
                boundary_columns[boundary_count] = column;
                boundary_rows[boundary_count] = row;
                ++boundary_count;
            }
        }
    }

    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);
            double nearest = 1e30;

            local_thickness[pixel] = 0.0;
            edge_distance[pixel] = 0.0;
            if (!image->opaque[pixel]) {
                continue;
            }
            for (size_t boundary = 0u; boundary < boundary_count; ++boundary) {
                double distance = squared_distance(column, row, boundary_columns[boundary], boundary_rows[boundary]);

                nearest = distance < nearest ? distance : nearest;
            }
            /* Measure to the edge of the transparent pixel, not its centre. */
            edge_distance[pixel] = sqrt(nearest) - 0.5;
        }
    }

    /* Each pixel's local thickness is the largest inscribed disc that contains it. */
    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            double radius = edge_distance[wolf_sprite_pixel_index(column, row)];
            int reach;

            if (radius <= 0.0) {
                continue;
            }
            reach = (int)ceil(radius);
            for (int other_row = row - reach; other_row <= row + reach; ++other_row) {
                for (int other_column = column - reach; other_column <= column + reach; ++other_column) {
                    size_t other;

                    if (!pixel_opaque(image, other_column, other_row) ||
                        squared_distance(column, row, other_column, other_row) > radius * radius) {
                        continue;
                    }
                    other = wolf_sprite_pixel_index(other_column, other_row);
                    if (radius > local_thickness[other]) {
                        local_thickness[other] = radius;
                    }
                }
            }
        }
    }

    for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        double distance = edge_distance[pixel];
        double radius = local_thickness[pixel];

        half_depth[pixel] = image->opaque[pixel] ?
            sqrt(fmax(0.0, distance * (2.0 * radius - distance))) : 0.0;
    }
}

/* Lathe profile: every horizontal run of drawn pixels becomes a disc of its own width. */
static void lathe_half_depths(const WolfSpriteImage *image, double *half_depth)
{
    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        int column = 0;

        while (column < WOLF_SPRITE_SIZE) {
            int run_start;
            double radius;
            double centre;

            if (!image->opaque[wolf_sprite_pixel_index(column, row)]) {
                half_depth[wolf_sprite_pixel_index(column, row)] = 0.0;
                ++column;
                continue;
            }
            run_start = column;
            while (column < WOLF_SPRITE_SIZE && image->opaque[wolf_sprite_pixel_index(column, row)]) {
                ++column;
            }
            radius = (double)(column - run_start) * 0.5;
            centre = (double)(run_start + column) * 0.5;
            for (int member = run_start; member < column; ++member) {
                double offset = (double)member + 0.5 - centre;

                half_depth[wolf_sprite_pixel_index(member, row)] =
                    sqrt(fmax(0.0, radius * radius - offset * offset));
            }
        }
    }
}

void wolf_single_view_half_depths(
    const WolfSpriteImage *image,
    const WolfSingleViewOptions *options,
    double *half_depth)
{
    switch (options->profile) {
    case WOLF_DEPTH_PROFILE_LATHE:
        lathe_half_depths(image, half_depth);
        break;
    case WOLF_DEPTH_PROFILE_EXTRUDE:
        for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
            half_depth[pixel] = image->opaque[pixel] ? options->extrude_half_depth : 0.0;
        }
        return;
    case WOLF_DEPTH_PROFILE_INFLATE:
    case WOLF_DEPTH_PROFILE_FIRST_PERSON:
    default:
        inflate_half_depths(image, half_depth);
        break;
    }

    for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        double scaled = half_depth[pixel] * options->depth_scale;

        half_depth[pixel] = scaled < options->maximum_half_depth ? scaled : options->maximum_half_depth;
    }
}

/*
 * Fill one pixel's line of sight between two depths (inclusive, clamped to
 * the volume), always leaving at least one voxel so no drawn pixel is lost.
 */
static void fill_depth_span(WolfVoxelModel *model, int column, int z, double near_y, double far_y, uint8_t palette_index)
{
    int first = (int)lround(near_y);
    int last = (int)lround(far_y);

    first = first < 0 ? 0 : (first > WOLF_VOXEL_DIM - 1 ? WOLF_VOXEL_DIM - 1 : first);
    last = last < first ? first : (last > WOLF_VOXEL_DIM - 1 ? WOLF_VOXEL_DIM - 1 : last);
    for (int y = first; y <= last; ++y) {
        size_t voxel = wolf_voxel_index(column, y, z);

        model->occupied[voxel] = 1u;
        model->palette_index[voxel] = palette_index;
    }
}

/* Span of voxels centred on the actor axis for a given half-depth. */
static void centred_span(double half_depth, double *near_y, double *far_y)
{
    long thickness = lround(2.0 * half_depth);

    if (thickness < 1) {
        thickness = 1;
    }
    if (thickness > WOLF_VOXEL_DIM) {
        thickness = WOLF_VOXEL_DIM;
    }
    *near_y = (double)(WOLF_VOXEL_DIM / 2 - (int)((thickness + 1) / 2));
    *far_y = *near_y + (double)(thickness - 1);
}

/*
 * First-person placement: rows rise toward the horizon as the weapon recedes,
 * so depth grows steadily from the bottom row (the hand, nearest the eye) to
 * the top row (the muzzle). A row step of k voxels tilts the gun's long axis;
 * each pixel's span is stretched by sqrt(1 + k^2) so that the thickness
 * measured across that axis matches the width the sprite shows.
 */
static void build_first_person(const WolfSpriteImage *image, const double *half_depth, WolfVoxelModel *model)
{
    int top_row = WOLF_SPRITE_SIZE;
    int bottom_row = -1;
    double depth_per_row;
    double stretch;
    const double near_margin = 4.0;

    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            if (image->opaque[wolf_sprite_pixel_index(column, row)]) {
                top_row = row < top_row ? row : top_row;
                bottom_row = row > bottom_row ? row : bottom_row;
            }
        }
    }
    if (bottom_row < 0) {
        return;
    }
    depth_per_row = (double)(WOLF_VOXEL_DIM - 16) / (double)(bottom_row - top_row > 0 ? bottom_row - top_row : 1);
    depth_per_row = depth_per_row < 0.75 ? 0.75 : (depth_per_row > 2.5 ? 2.5 : depth_per_row);
    stretch = sqrt(1.0 + depth_per_row * depth_per_row);

    for (int row = top_row; row <= bottom_row; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);
            double centre;
            double half_span;

            if (!image->opaque[pixel]) {
                continue;
            }
            centre = near_margin + (double)(bottom_row - row) * depth_per_row;
            /* At least one row step, so consecutive rows join into a solid surface. */
            half_span = fmax(0.5 * depth_per_row + 0.5, half_depth[pixel] * stretch);
            half_span = half_span > 12.0 ? 12.0 : half_span;
            fill_depth_span(model, column, wolf_voxel_row_for_layer(row),
                centre - half_span, centre + half_span, image->palette_index[pixel]);
        }
    }
}

/* Most common palette entry among the given pixels (ties go to the lowest entry). */
static uint8_t dominant_colour(const uint8_t *colours, int count)
{
    int tally[256] = {0};
    int best = 0;

    for (int entry = 0; entry < count; ++entry) {
        ++tally[colours[entry]];
    }
    for (int colour = 1; colour < 256; ++colour) {
        if (tally[colour] > tally[best]) {
            best = colour;
        }
    }
    return (uint8_t)best;
}

/*
 * Plain colours for the back of a camera-facing figure. Rows in the top
 * fifth of the figure are the head: their back is the colour at the top of
 * that column (hair, a hat, or skin on a bald head). Below, each pixel takes
 * the dominant colour of its row within five columns either side, which
 * keeps large areas (a uniform, an arm) and drops small front details (eyes,
 * mouth, buttons).
 */
static void plain_back_colours(const WolfSpriteImage *image, uint8_t *back)
{
    int top_row = WOLF_SPRITE_SIZE;
    int bottom_row = -1;
    int head_limit;

    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            if (image->opaque[wolf_sprite_pixel_index(column, row)]) {
                top_row = row < top_row ? row : top_row;
                bottom_row = row;
            }
        }
    }
    head_limit = top_row + (bottom_row - top_row + 1) / 5;

    for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
        uint8_t top_colours[4];
        int top_count = 0;
        int first_row = -1;

        for (int row = 0; row < WOLF_SPRITE_SIZE && top_count < 4; ++row) {
            size_t pixel = wolf_sprite_pixel_index(column, row);

            if (image->opaque[pixel]) {
                first_row = first_row < 0 ? row : first_row;
                top_colours[top_count++] = image->palette_index[pixel];
            }
        }
        for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
            size_t pixel = wolf_sprite_pixel_index(column, row);
            uint8_t window[11];
            int window_count = 0;

            if (!image->opaque[pixel]) {
                continue;
            }
            if (row <= head_limit && first_row >= 0 && first_row <= head_limit) {
                back[pixel] = dominant_colour(top_colours, top_count);
                continue;
            }
            for (int other = column - 5; other <= column + 5; ++other) {
                if (other >= 0 && other < WOLF_SPRITE_SIZE && image->opaque[wolf_sprite_pixel_index(other, row)]) {
                    window[window_count++] = image->palette_index[wolf_sprite_pixel_index(other, row)];
                }
            }
            back[pixel] = dominant_colour(window, window_count);
        }
    }
}

/* Voxel index whose cell contains a point at the given offset from the actor axis. */
static int index_for_offset(double offset)
{
    int index = (int)floor(offset + (double)(WOLF_VOXEL_DIM / 2));

    return index < 0 ? 0 : (index >= WOLF_VOXEL_DIM ? WOLF_VOXEL_DIM - 1 : index);
}

/*
 * Cage profile. Rows are measured by the extent of their bar pixels: narrow
 * rows (the hanging chain) are lathed solid, mostly-filled rows are hoops
 * (capped at the top and bottom of the cage), and sparse rows are bars
 * placed where a cylinder of that radius crosses each bar's column, front
 * and back. Contents are inflated inside, clear of the bars.
 */
static void build_cage(const WolfSpriteImage *image, const WolfSingleViewOptions *options, WolfVoxelModel *model)
{
    WolfSpriteImage contents;
    WolfSpriteImage inner;
    double contents_depth[WOLF_SPRITE_PIXELS];
    WolfSingleViewOptions inflate = {WOLF_DEPTH_PROFILE_INFLATE, 1.0, 32.0, 0.0, NULL, false};
    double radius[WOLF_SPRITE_SIZE];
    double centre[WOLF_SPRITE_SIZE];
    int filled[WOLF_SPRITE_SIZE];
    double largest_radius = 0.0;
    int body_top = -1;
    int body_bottom = -1;

    wolf_sprite_image_clear(&contents);
    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        int first = -1;
        int last = -1;

        filled[row] = 0;
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);

            if (!image->opaque[pixel]) {
                continue;
            }
            if (options->frame_colours != NULL && options->frame_colours[image->palette_index[pixel]]) {
                first = first < 0 ? column : first;
                last = column;
                ++filled[row];
            } else {
                contents.opaque[pixel] = 1u;
                contents.palette_index[pixel] = image->palette_index[pixel];
            }
        }
        radius[row] = first < 0 ? 0.0 : (double)(last - first + 1) * 0.5;
        centre[row] = first < 0 ? 0.0 : (double)(first + last + 1) * 0.5;
        largest_radius = radius[row] > largest_radius ? radius[row] : largest_radius;
    }
    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        if (radius[row] > 0.0 && radius[row] >= 0.5 * largest_radius) {
            body_top = body_top < 0 ? row : body_top;
            body_bottom = row;
        }
    }

    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        int z = wolf_voxel_row_for_layer(row);
        /* A hoop row draws most of its width; a bar row only the bars. */
        bool hoop = radius[row] > 0.0 && (double)filled[row] >= 0.75 * 2.0 * radius[row];
        bool cap = hoop && (row - body_top < 2 || body_bottom - row < 2);
        bool chain = radius[row] < 0.5 * largest_radius;

        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);
            uint8_t colour = image->palette_index[pixel];
            double across;
            double depth;
            int front;
            int back;

            if (!image->opaque[pixel] || contents.opaque[pixel]) {
                continue;
            }
            across = (double)column + 0.5 - centre[row];
            depth = sqrt(fmax(0.0, radius[row] * radius[row] - across * across));
            front = index_for_offset(-depth);
            back = index_for_offset(depth - 1e-6);
            if (chain || cap) {
                fill_depth_span(model, column, z, (double)front, (double)back, colour);
            } else if (hoop) {
                fill_depth_span(model, column, z, (double)front, (double)front + 1.0, colour);
                fill_depth_span(model, column, z, (double)back - 1.0, (double)back, colour);
            } else {
                fill_depth_span(model, column, z, (double)front, (double)front, colour);
                fill_depth_span(model, column, z, (double)back, (double)back, colour);
            }
        }
    }

    /*
     * Bars and hoops cross in front of the prisoner and split it into pieces.
     * Behind a bar pixel with the prisoner on both sides of it (within three
     * pixels, across the row or up the column) the prisoner continues, taking
     * the colour of its neighbour; the bar still hides it from the front.
     */
    inner = contents;
    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);
            int left = -1;
            int right = -1;
            int above = -1;
            int below = -1;

            if (!image->opaque[pixel] || contents.opaque[pixel]) {
                continue;
            }
            for (int step = 1; step <= 3; ++step) {
                if (left < 0 && column - step >= 0 && contents.opaque[wolf_sprite_pixel_index(column - step, row)]) {
                    left = column - step;
                }
                if (right < 0 && column + step < WOLF_SPRITE_SIZE && contents.opaque[wolf_sprite_pixel_index(column + step, row)]) {
                    right = column + step;
                }
                if (above < 0 && row - step >= 0 && contents.opaque[wolf_sprite_pixel_index(column, row - step)]) {
                    above = row - step;
                }
                if (below < 0 && row + step < WOLF_SPRITE_SIZE && contents.opaque[wolf_sprite_pixel_index(column, row + step)]) {
                    below = row + step;
                }
            }
            if (left >= 0 && right >= 0) {
                inner.opaque[pixel] = 1u;
                inner.palette_index[pixel] = contents.palette_index[wolf_sprite_pixel_index(left, row)];
            } else if (above >= 0 && below >= 0) {
                inner.opaque[pixel] = 1u;
                inner.palette_index[pixel] = contents.palette_index[wolf_sprite_pixel_index(column, above)];
            }
        }
    }

    /* The prisoner hangs inside, never reaching the bars. */
    wolf_single_view_half_depths(&inner, &inflate, contents_depth);
    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);
            double limit = radius[row] > 4.0 ? radius[row] - 2.0 : 2.0;
            double near_y;
            double far_y;

            if (!inner.opaque[pixel]) {
                continue;
            }
            centred_span(fmin(contents_depth[pixel], limit), &near_y, &far_y);
            fill_depth_span(model, column, wolf_voxel_row_for_layer(row), near_y, far_y, inner.palette_index[pixel]);
        }
    }
}

void wolf_reconstruct_single_view(
    const WolfSpriteImage *image,
    const WolfSingleViewOptions *options,
    WolfVoxelModel *model)
{
    double half_depth[WOLF_SPRITE_PIXELS];
    uint8_t back[WOLF_SPRITE_PIXELS];

    wolf_voxel_model_clear(model);
    if (options->profile == WOLF_DEPTH_PROFILE_CAGE) {
        build_cage(image, options, model);
        return;
    }
    wolf_single_view_half_depths(image, options, half_depth);
    if (options->profile == WOLF_DEPTH_PROFILE_FIRST_PERSON) {
        build_first_person(image, half_depth, model);
        return;
    }
    if (options->plain_back) {
        plain_back_colours(image, back);
    }

    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);
            int z = wolf_voxel_row_for_layer(row);
            double near_y;
            double far_y;

            if (!image->opaque[pixel]) {
                continue;
            }
            centred_span(half_depth[pixel], &near_y, &far_y);
            fill_depth_span(model, column, z, near_y, far_y, image->palette_index[pixel]);
            /* Behind the middle of the body, the plain back replaces the drawn front. */
            if (options->plain_back) {
                for (int y = (int)lround(near_y); y <= (int)lround(far_y); ++y) {
                    if ((double)y > 0.5 * (near_y + far_y) && wolf_voxel_in_bounds(column, y, z)) {
                        model->palette_index[wolf_voxel_index(column, y, z)] = back[pixel];
                    }
                }
            }
        }
    }
}

void wolf_reconstruct_axial_revolve(const WolfSpriteImage *side_view, WolfVoxelModel *model)
{
    double axis_sum = 0.0;
    size_t drawn = 0u;
    double axis_row;

    wolf_voxel_model_clear(model);
    /* The flight axis runs through the middle of the drawn profile. */
    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            if (side_view->opaque[wolf_sprite_pixel_index(column, row)]) {
                axis_sum += (double)row + 0.5;
                ++drawn;
            }
        }
    }
    if (drawn == 0u) {
        return;
    }
    axis_row = axis_sum / (double)drawn;

    /* From the side (viewer at +x), canvas column c shows depth y = c. */
    for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
        double radius = 0.0;

        for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
            if (side_view->opaque[wolf_sprite_pixel_index(column, row)]) {
                double reach = fabs((double)row + 0.5 - axis_row) + 0.5;

                radius = reach > radius ? reach : radius;
            }
        }
        if (radius <= 0.0) {
            continue;
        }
        for (int z = 0; z < WOLF_VOXEL_DIM; ++z) {
            int layer_row = wolf_voxel_row_for_layer(z);
            double up = (double)layer_row + 0.5 - axis_row;

            for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                double across = wolf_voxel_center_offset(x);
                int nearest_row = -1;

                if (across * across + up * up > radius * radius) {
                    continue;
                }
                /* Colour by the side profile at this height, or its nearest drawn row. */
                for (int distance = 0; distance < WOLF_SPRITE_SIZE && nearest_row < 0; ++distance) {
                    int candidates[2] = {layer_row - distance, layer_row + distance};

                    for (int pick = 0; pick < 2 && nearest_row < 0; ++pick) {
                        int row = candidates[pick];

                        if (row >= 0 && row < WOLF_SPRITE_SIZE &&
                            side_view->opaque[wolf_sprite_pixel_index(column, row)]) {
                            nearest_row = row;
                        }
                    }
                }
                model->occupied[wolf_voxel_index(x, column, z)] = 1u;
                model->palette_index[wolf_voxel_index(x, column, z)] =
                    side_view->palette_index[wolf_sprite_pixel_index(column, nearest_row)];
            }
        }
    }
}

void wolf_reconstruct_pose_transfer(
    const WolfSpriteImage *image,
    const WolfVoxelModel *reference,
    const WolfSingleViewOptions *fallback,
    WolfVoxelModel *model)
{
    int front[WOLF_SPRITE_PIXELS];
    int back[WOLF_SPRITE_PIXELS];
    double row_centre[WOLF_SPRITE_SIZE];
    bool row_has_centre[WOLF_SPRITE_SIZE];
    double half_depth[WOLF_SPRITE_PIXELS];
    double body_centre_sum = 0.0;
    size_t body_centre_count = 0u;
    double body_centre;

    wolf_voxel_model_clear(model);
    wolf_single_view_half_depths(image, fallback, half_depth);

    /* The reference body's front and back surface along each front-view line of sight. */
    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        int z = wolf_voxel_row_for_layer(row);
        double sum = 0.0;
        int count = 0;

        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);

            front[pixel] = -1;
            back[pixel] = -1;
            for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
                if (reference->occupied[wolf_voxel_index(column, y, z)]) {
                    if (front[pixel] < 0) {
                        front[pixel] = y;
                    }
                    back[pixel] = y;
                }
            }
            if (front[pixel] >= 0) {
                sum += 0.5 * (double)(front[pixel] + back[pixel]);
                ++count;
            }
        }
        row_has_centre[row] = count > 0;
        row_centre[row] = count > 0 ? sum / (double)count : 0.0;
        if (count > 0) {
            body_centre_sum += row_centre[row];
            ++body_centre_count;
        }
    }
    body_centre = body_centre_count > 0u ? body_centre_sum / (double)body_centre_count : (double)WOLF_VOXEL_DIM * 0.5 - 0.5;

    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        int z = wolf_voxel_row_for_layer(row);

        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);
            uint8_t colour = image->palette_index[pixel];

            if (!image->opaque[pixel]) {
                continue;
            }
            if (front[pixel] >= 0) {
                /* The frame paints the front face; the body's own sides and back stay underneath. */
                for (int y = front[pixel]; y <= back[pixel]; ++y) {
                    size_t voxel = wolf_voxel_index(column, y, z);

                    model->occupied[voxel] = 1u;
                    model->palette_index[voxel] = (y > front[pixel] && reference->occupied[voxel]) ?
                        reference->palette_index[voxel] : colour;
                }
            } else {
                double centre = row_has_centre[row] ? row_centre[row] : body_centre;
                double half = half_depth[pixel] < 0.5 ? 0.5 : half_depth[pixel];

                fill_depth_span(model, column, z, centre - half + 0.5, centre + half - 0.5, colour);
            }
        }
    }
}

double wolf_voxel_model_depth_ratio(const WolfVoxelModel *model)
{
    size_t total_width = 0u;
    size_t total_depth = 0u;

    for (int z = 0; z < WOLF_VOXEL_DIM; ++z) {
        uint8_t column_used[WOLF_VOXEL_DIM] = {0};
        uint8_t depth_used[WOLF_VOXEL_DIM] = {0};

        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                if (model->occupied[wolf_voxel_index(x, y, z)]) {
                    column_used[x] = 1u;
                    depth_used[y] = 1u;
                }
            }
        }
        for (int position = 0; position < WOLF_VOXEL_DIM; ++position) {
            total_width += column_used[position];
            total_depth += depth_used[position];
        }
    }
    return total_width > 0u ? (double)total_depth / (double)total_width : 1.0;
}

int wolf_voxel_model_depth_extent(const WolfVoxelModel *model)
{
    int nearest = WOLF_VOXEL_DIM;
    int farthest = -1;

    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        if (model->occupied[voxel]) {
            int y = (int)((voxel / WOLF_VOXEL_DIM) % WOLF_VOXEL_DIM);

            nearest = y < nearest ? y : nearest;
            farthest = y > farthest ? y : farthest;
        }
    }
    return farthest < 0 ? 0 : farthest - nearest + 1;
}
