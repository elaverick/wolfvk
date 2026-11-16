#include "reconstruction.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_VIEWS 16

WolfMultiViewOptions wolf_multi_view_default_options(void)
{
    WolfMultiViewOptions options;

    options.maximum_registration_shift = 3;
    options.minimum_fragment_voxels = 8u;
    options.prune_with_diagonal_views = true;
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

/* Keep exactly the voxels whose centres every listed view draws. */
static void carve_hull(
    const WolfSpriteImage *const *views,
    const WolfViewBasis *bases,
    const int *shifts,
    const size_t *view_list,
    size_t list_count,
    WolfVoxelModel *model)
{
    wolf_voxel_model_clear(model);
    for (int z = 0; z < WOLF_VOXEL_DIM; ++z) {
        int row = wolf_voxel_row_for_layer(z);

        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                bool inside = true;

                for (size_t entry = 0u; entry < list_count && inside; ++entry) {
                    size_t view = view_list[entry];

                    inside = view_draws(views[view], centre_column(&bases[view], x, y, shifts[view]), row);
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
 * face-on hull of a walking figure holds both legs in both positions. The
 * diagonal frames disambiguate: remove every voxel a diagonal frame
 * rejects, as long as each drawn pixel of every face-on frame remains
 * covered by another voxel. The face-on frames are therefore always
 * reproduced in outline, while the phantom material is cut away.
 */
static void prune_with_diagonal_views(
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
                bool diagonal_rejects = false;

                if (!model->occupied[voxel]) {
                    continue;
                }
                for (size_t view = 0u; view < view_count; ++view) {
                    int first_column;
                    int last_column;
                    bool rejected = !view_draws(views[view], centre_column(&bases[view], x, y, shifts[view]), row);

                    rejections += rejected ? 1 : 0;
                    diagonal_rejects = diagonal_rejects || (rejected && !face_on[view]);
                    wolf_voxel_footprint_columns(&bases[view], x, y, shifts[view], &first_column, &last_column);
                    for (int column = first_column; column <= last_column; ++column) {
                        if (column >= 0 && column < WOLF_SPRITE_SIZE) {
                            ++coverage[view * WOLF_SPRITE_PIXELS + wolf_sprite_pixel_index(column, row)];
                        }
                    }
                }
                if (diagonal_rejects) {
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

    for (size_t candidate = 0u; candidate < candidate_count; ++candidate) {
        size_t voxel = candidates[candidate].voxel;
        int x = (int)(voxel % WOLF_VOXEL_DIM);
        int y = (int)((voxel / WOLF_VOXEL_DIM) % WOLF_VOXEL_DIM);
        int row = wolf_voxel_row_for_layer((int)(voxel / (WOLF_VOXEL_DIM * WOLF_VOXEL_DIM)));
        bool removable = true;

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
    free(coverage);
    free(candidates);
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

/*
 * Colour each visible voxel from the frame that faces it most squarely.
 * Mixing frames voxel by voxel turns the dithered shading into noise; this
 * keeps each side's texture intact. Face-on frames get a small preference
 * because they were drawn consistently with one another.
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
        double preference = fabs(bases[view].half_footprint - 0.5) < 1e-6 ? 0.15 : 0.0;

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
            facing = normal_x * bases[view].toward_viewer_x + normal_y * bases[view].toward_viewer_y + preference;
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
    if (options->prune_with_diagonal_views && any_diagonal) {
        prune_with_diagonal_views(views, bases, shifts, view_count, model);
        remove_small_fragments(model, options->minimum_fragment_voxels);
    }
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

void wolf_reconstruct_single_view(
    const WolfSpriteImage *image,
    const WolfSingleViewOptions *options,
    WolfVoxelModel *model)
{
    double half_depth[WOLF_SPRITE_PIXELS];

    wolf_voxel_model_clear(model);
    wolf_single_view_half_depths(image, options, half_depth);
    if (options->profile == WOLF_DEPTH_PROFILE_FIRST_PERSON) {
        build_first_person(image, half_depth, model);
        return;
    }

    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);
            double near_y;
            double far_y;

            if (!image->opaque[pixel]) {
                continue;
            }
            centred_span(half_depth[pixel], &near_y, &far_y);
            fill_depth_span(model, column, wolf_voxel_row_for_layer(row), near_y, far_y, image->palette_index[pixel]);
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
