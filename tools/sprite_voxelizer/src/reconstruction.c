#include "reconstruction.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

WolfMultiViewOptions wolf_multi_view_default_options(void)
{
    WolfMultiViewOptions options;

    options.allowed_silhouette_misses = 1;
    options.diagonal_tolerance_columns = 0;
    options.minimum_fragment_voxels = 8u;
    options.maximum_registration_shift = 3;
    options.prune_excess_voxels = true;
    return options;
}

/* Is the canvas column/row drawn in this view, allowing a sideways tolerance? */
static bool view_covers(const WolfSpriteImage *view, int column, int row, int tolerance)
{
    for (int candidate = column - tolerance; candidate <= column + tolerance; ++candidate) {
        if (candidate >= 0 && candidate < WOLF_SPRITE_SIZE &&
            view->opaque[wolf_sprite_pixel_index(candidate, row)]) {
            return true;
        }
    }
    return false;
}

/* Carve away every voxel whose centre projects outside too many registered silhouettes. */
static void carve_visual_hull(
    const WolfSpriteImage *const *views,
    const double *rotation_degrees,
    const int *shifts,
    size_t view_count,
    const WolfMultiViewOptions *options,
    WolfVoxelModel *model)
{
    WolfViewBasis bases[16];

    for (size_t view = 0u; view < view_count; ++view) {
        bases[view] = wolf_view_basis(rotation_degrees[view]);
    }

    for (int z = 0; z < WOLF_VOXEL_DIM; ++z) {
        int row = wolf_voxel_row_for_layer(z);

        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                double offset_x = wolf_voxel_center_offset(x);
                double offset_y = wolf_voxel_center_offset(y);
                int misses = 0;

                for (size_t view = 0u; view < view_count; ++view) {
                    const WolfViewBasis *basis = &bases[view];
                    double screen_u = offset_x * basis->right_x + offset_y * basis->right_y;
                    int column = (int)floor(screen_u + (double)(WOLF_SPRITE_SIZE / 2)) + shifts[view];
                    /* Face-on views are pixel exact; only rotated views are given slack. */
                    bool diagonal = basis->half_footprint > 0.5 + 1e-6;
                    int tolerance = diagonal ? options->diagonal_tolerance_columns : 0;

                    if (!view_covers(views[view], column, row, tolerance)) {
                        ++misses;
                        if (misses > options->allowed_silhouette_misses) {
                            break;
                        }
                    }
                }
                model->occupied[wolf_voxel_index(x, y, z)] =
                    misses <= options->allowed_silhouette_misses ? 1u : 0u;
            }
        }
    }
}

/*
 * Remove small disconnected specks. Carving inconsistent drawings leaves
 * isolated islands that no single frame actually depicts.
 */
static void remove_small_fragments(WolfVoxelModel *model, size_t minimum_voxels)
{
    int32_t *component = malloc(sizeof(int32_t) * WOLF_VOXEL_COUNT);
    size_t *queue = malloc(sizeof(size_t) * WOLF_VOXEL_COUNT);
    static const int offsets[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };

    if (component == NULL || queue == NULL || minimum_voxels <= 1u) {
        free(component);
        free(queue);
        return;
    }
    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        component[voxel] = -1;
    }

    for (size_t seed = 0u; seed < WOLF_VOXEL_COUNT; ++seed) {
        size_t head = 0u;
        size_t tail = 0u;

        if (!model->occupied[seed] || component[seed] >= 0) {
            continue;
        }
        component[seed] = 1;
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
                if (model->occupied[neighbour] && component[neighbour] < 0) {
                    component[neighbour] = 1;
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
    free(component);
    free(queue);
}

typedef struct PruneCandidate {
    uint32_t voxel;
    /* Number of frames whose silhouette rejects the voxel centre. */
    int misses;
    /* Squared distance from the actor axis; outer material is pruned first. */
    double radius_squared;
} PruneCandidate;

static int compare_prune_candidates(const void *left, const void *right)
{
    const PruneCandidate *a = left;
    const PruneCandidate *b = right;

    if (a->misses != b->misses) {
        return a->misses > b->misses ? -1 : 1;
    }
    if (a->radius_squared != b->radius_squared) {
        return a->radius_squared > b->radius_squared ? -1 : 1;
    }
    return a->voxel < b->voxel ? -1 : (a->voxel > b->voxel ? 1 : 0);
}

/*
 * Tolerant carving keeps material that a minority of frames reject, and that
 * material shows up as excess outline in those frames. Remove any such voxel
 * that covers a transparent pixel in some frame, provided every drawn pixel it
 * covers in every frame remains covered by another voxel. Silhouette
 * coverage of the drawn pixels therefore never shrinks, while excess
 * outline is cut back. Voxels most frames reject, and those furthest from
 * the axis, are considered first.
 */
static void prune_excess_voxels(
    const WolfSpriteImage *const *views,
    const double *rotation_degrees,
    const int *shifts,
    size_t view_count,
    WolfVoxelModel *model)
{
    WolfViewBasis bases[16];
    uint16_t *coverage = calloc(view_count * WOLF_SPRITE_PIXELS, sizeof(uint16_t));
    PruneCandidate *candidates = malloc(sizeof(PruneCandidate) * WOLF_VOXEL_COUNT);
    size_t candidate_count = 0u;

    if (coverage == NULL || candidates == NULL) {
        free(coverage);
        free(candidates);
        return;
    }
    for (size_t view = 0u; view < view_count; ++view) {
        bases[view] = wolf_view_basis(rotation_degrees[view]);
    }

    /* Count how many voxels cover each pixel of each frame. */
    for (int z = 0; z < WOLF_VOXEL_DIM; ++z) {
        int row = wolf_voxel_row_for_layer(z);

        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                size_t voxel = wolf_voxel_index(x, y, z);
                bool covers_transparent = false;
                int misses = 0;

                if (!model->occupied[voxel]) {
                    continue;
                }
                for (size_t view = 0u; view < view_count; ++view) {
                    int first_column;
                    int last_column;
                    double screen_u = wolf_voxel_center_offset(x) * bases[view].right_x +
                        wolf_voxel_center_offset(y) * bases[view].right_y;
                    int centre_column = (int)floor(screen_u + (double)(WOLF_SPRITE_SIZE / 2)) + shifts[view];

                    if (centre_column < 0 || centre_column >= WOLF_SPRITE_SIZE ||
                        !views[view]->opaque[wolf_sprite_pixel_index(centre_column, row)]) {
                        ++misses;
                    }
                    wolf_voxel_footprint_columns(&bases[view], x, y, shifts[view], &first_column, &last_column);
                    for (int column = first_column; column <= last_column; ++column) {
                        size_t pixel;

                        if (column < 0 || column >= WOLF_SPRITE_SIZE) {
                            continue;
                        }
                        pixel = wolf_sprite_pixel_index(column, row);
                        ++coverage[view * WOLF_SPRITE_PIXELS + pixel];
                        covers_transparent = covers_transparent || !views[view]->opaque[pixel];
                    }
                }
                /* Only material kept by the carving tolerance is eligible for pruning. */
                if (covers_transparent && misses > 0) {
                    candidates[candidate_count].voxel = (uint32_t)voxel;
                    candidates[candidate_count].misses = misses;
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
 * 26 neighbours are empty. Only the horizontal part matters because every
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

/* Mean silhouette overlap between each frame and the carved solid seen from that frame. */
static double silhouette_agreement(
    const WolfSpriteImage *const *views,
    const double *rotation_degrees,
    const int *shifts,
    size_t view_count,
    const WolfMultiViewOptions *options,
    WolfVoxelModel *scratch)
{
    WolfSpriteImage rendered;
    double total = 0.0;

    carve_visual_hull(views, rotation_degrees, shifts, view_count, options, scratch);
    for (size_t view = 0u; view < view_count; ++view) {
        size_t shared = 0u;
        size_t either = 0u;

        wolf_voxel_render_view(scratch, rotation_degrees[view], shifts[view], &rendered, NULL);
        for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
            shared += (rendered.opaque[pixel] && views[view]->opaque[pixel]) ? 1u : 0u;
            either += (rendered.opaque[pixel] || views[view]->opaque[pixel]) ? 1u : 0u;
        }
        total += either > 0u ? (double)shared / (double)either : 1.0;
    }
    return total / (double)view_count;
}

/*
 * Find the column offsets that make the frames describe one consistent
 * solid. Opposite views of a real object are mirror images, and the hand-
 * drawn frames typically break that by a pixel or two, which carves away
 * material every frame actually shows. Coordinate ascent over whole-column
 * shifts maximises the mean silhouette agreement; the first frame stays
 * fixed so the actor remains centred on the original front view.
 */
static void register_views(
    const WolfSpriteImage *const *views,
    const double *rotation_degrees,
    size_t view_count,
    const WolfMultiViewOptions *options,
    WolfVoxelModel *scratch,
    int *shifts)
{
    double best_score;

    for (size_t view = 0u; view < view_count; ++view) {
        shifts[view] = 0;
    }
    if (options->maximum_registration_shift <= 0 || view_count < 2u) {
        return;
    }
    best_score = silhouette_agreement(views, rotation_degrees, shifts, view_count, options, scratch);

    for (int pass = 0; pass < 4; ++pass) {
        bool improved = false;

        for (size_t view = 1u; view < view_count; ++view) {
            int kept_shift = shifts[view];

            for (int candidate = -options->maximum_registration_shift;
                candidate <= options->maximum_registration_shift; ++candidate) {
                double score;

                if (candidate == kept_shift) {
                    continue;
                }
                shifts[view] = candidate;
                score = silhouette_agreement(views, rotation_degrees, shifts, view_count, options, scratch);
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

typedef struct ColourVote {
    uint32_t voxel;
    uint8_t palette_index;
    double weight;
} ColourVote;

static int compare_votes(const void *left, const void *right)
{
    const ColourVote *a = left;
    const ColourVote *b = right;

    if (a->voxel != b->voxel) {
        return a->voxel < b->voxel ? -1 : 1;
    }
    return (int)a->palette_index - (int)b->palette_index;
}

/*
 * Give every voxel without a vote the colour of its nearest coloured voxel,
 * by a multi-source breadth-first flood through occupied space.
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

bool wolf_reconstruct_multi_view(
    const WolfSpriteImage *const *views,
    const double *rotation_degrees,
    size_t view_count,
    const WolfMultiViewOptions *options,
    WolfVoxelModel *model,
    int *registration_shifts)
{
    int shifts[16];
    ColourVote *votes;
    size_t vote_count = 0u;
    uint8_t *coloured;
    int32_t voxel_for_pixel[WOLF_SPRITE_PIXELS];
    WolfSpriteImage rendered;

    if (views == NULL || rotation_degrees == NULL || view_count == 0u || view_count > 16u ||
        options == NULL || model == NULL) {
        return false;
    }

    wolf_voxel_model_clear(model);
    register_views(views, rotation_degrees, view_count, options, model, shifts);
    carve_visual_hull(views, rotation_degrees, shifts, view_count, options, model);
    /* Fragments go first so that pruning never relies on a speck for coverage. */
    remove_small_fragments(model, options->minimum_fragment_voxels);
    if (options->prune_excess_voxels) {
        prune_excess_voxels(views, rotation_degrees, shifts, view_count, model);
    }
    if (registration_shifts != NULL) {
        memcpy(registration_shifts, shifts, sizeof(int) * view_count);
    }

    /* Each view casts at most one vote per drawn pixel. */
    votes = malloc(sizeof(ColourVote) * view_count * WOLF_SPRITE_PIXELS);
    coloured = calloc(WOLF_VOXEL_COUNT, 1u);
    if (votes == NULL || coloured == NULL) {
        free(votes);
        free(coloured);
        return false;
    }

    /*
     * Each drawn pixel votes for the colour of the voxel the model shows
     * there. A voxel's colour affects only the pixels it supplies, so a plain
     * majority maximises the number of reproduced pixels across all frames.
     * The facing term is deliberately tiny: it only breaks ties in favour of
     * the frame that looks straight at the surface.
     */
    for (size_t view = 0u; view < view_count; ++view) {
        WolfViewBasis basis = wolf_view_basis(rotation_degrees[view]);

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
            facing = normal_x * basis.toward_viewer_x + normal_y * basis.toward_viewer_y;
            votes[vote_count].voxel = (uint32_t)voxel;
            votes[vote_count].palette_index = views[view]->palette_index[pixel];
            votes[vote_count].weight = 1.0 + 0.01 * (facing > 0.0 ? facing : 0.0);
            ++vote_count;
        }
    }

    /* Tally votes per voxel and keep the colour with the greatest total weight. */
    qsort(votes, vote_count, sizeof(ColourVote), compare_votes);
    for (size_t first = 0u; first < vote_count;) {
        size_t last = first;
        double best_weight = -1.0;
        uint8_t best_index = 0u;

        while (last < vote_count && votes[last].voxel == votes[first].voxel) {
            size_t run_end = last;
            double run_weight = 0.0;

            while (run_end < vote_count && votes[run_end].voxel == votes[first].voxel &&
                votes[run_end].palette_index == votes[last].palette_index) {
                run_weight += votes[run_end].weight;
                ++run_end;
            }
            if (run_weight > best_weight) {
                best_weight = run_weight;
                best_index = votes[last].palette_index;
            }
            last = run_end;
        }
        model->palette_index[votes[first].voxel] = best_index;
        coloured[votes[first].voxel] = 1u;
        first = last;
    }

    propagate_colours(model, coloured);
    free(votes);
    free(coloured);
    return true;
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
    default:
        inflate_half_depths(image, half_depth);
        break;
    }

    for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        double scaled = half_depth[pixel] * options->depth_scale;

        half_depth[pixel] = scaled < options->maximum_half_depth ? scaled : options->maximum_half_depth;
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

    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);
            int z = wolf_voxel_row_for_layer(row);
            long thickness;
            int first_y;

            if (!image->opaque[pixel]) {
                continue;
            }
            /* Centre the column on the actor's axis; every pixel is at least one voxel thick. */
            thickness = lround(2.0 * half_depth[pixel]);
            if (thickness < 1) {
                thickness = 1;
            }
            if (thickness > WOLF_VOXEL_DIM) {
                thickness = WOLF_VOXEL_DIM;
            }
            first_y = WOLF_VOXEL_DIM / 2 - (int)((thickness + 1) / 2);
            for (int y = first_y; y < first_y + (int)thickness; ++y) {
                size_t voxel = wolf_voxel_index(column, y, z);

                model->occupied[voxel] = 1u;
                model->palette_index[voxel] = image->palette_index[pixel];
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
