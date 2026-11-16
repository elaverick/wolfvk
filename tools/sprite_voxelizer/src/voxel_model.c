#include "voxel_model.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define WOLF_PI 3.14159265358979323846

WolfVoxelModel *wolf_voxel_model_create(void)
{
    return calloc(1u, sizeof(WolfVoxelModel));
}

void wolf_voxel_model_destroy(WolfVoxelModel *model)
{
    free(model);
}

void wolf_voxel_model_clear(WolfVoxelModel *model)
{
    memset(model, 0, sizeof(*model));
}

size_t wolf_voxel_model_count(const WolfVoxelModel *model)
{
    size_t count = 0u;

    for (size_t voxel = 0u; voxel < WOLF_VOXEL_COUNT; ++voxel) {
        count += model->occupied[voxel] ? 1u : 0u;
    }
    return count;
}

bool wolf_voxel_is_surface(const WolfVoxelModel *model, int x, int y, int z)
{
    static const int neighbour_offsets[6][3] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    };

    if (!model->occupied[wolf_voxel_index(x, y, z)]) {
        return false;
    }
    for (int face = 0; face < 6; ++face) {
        int nx = x + neighbour_offsets[face][0];
        int ny = y + neighbour_offsets[face][1];
        int nz = z + neighbour_offsets[face][2];

        if (!wolf_voxel_in_bounds(nx, ny, nz) || !model->occupied[wolf_voxel_index(nx, ny, nz)]) {
            return true;
        }
    }
    return false;
}

WolfViewBasis wolf_view_basis(double rotation_degrees)
{
    double radians = rotation_degrees * WOLF_PI / 180.0;
    WolfViewBasis basis;

    /*
     * The front viewer stands at -y looking along +y with +x to the right.
     * Rotating that viewer anticlockwise about the vertical axis rotates
     * both the screen-right vector and the direction back toward the viewer.
     */
    basis.right_x = cos(radians);
    basis.right_y = sin(radians);
    basis.toward_viewer_x = sin(radians);
    basis.toward_viewer_y = -cos(radians);
    basis.half_footprint = (fabs(basis.right_x) + fabs(basis.right_y)) * 0.5;
    return basis;
}

void wolf_voxel_render_view(
    const WolfVoxelModel *model,
    double rotation_degrees,
    WolfSpriteImage *image,
    int32_t *voxel_for_pixel)
{
    WolfViewBasis basis = wolf_view_basis(rotation_degrees);
    double depth[WOLF_SPRITE_PIXELS];
    double centre_distance[WOLF_SPRITE_PIXELS];
    /* Shrink the footprint slightly so a face-on cube never bleeds into a neighbour column. */
    double coverage = basis.half_footprint - 1e-6;

    wolf_sprite_image_clear(image);
    for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        depth[pixel] = -DBL_MAX;
        centre_distance[pixel] = DBL_MAX;
        if (voxel_for_pixel != NULL) {
            voxel_for_pixel[pixel] = -1;
        }
    }

    for (int z = 0; z < WOLF_VOXEL_DIM; ++z) {
        int row = wolf_voxel_row_for_layer(z);

        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                size_t voxel = wolf_voxel_index(x, y, z);
                double offset_x;
                double offset_y;
                double screen_u;
                double voxel_depth;
                int first_column;
                int last_column;

                if (!model->occupied[voxel]) {
                    continue;
                }
                offset_x = wolf_voxel_center_offset(x);
                offset_y = wolf_voxel_center_offset(y);
                screen_u = offset_x * basis.right_x + offset_y * basis.right_y;
                voxel_depth = offset_x * basis.toward_viewer_x + offset_y * basis.toward_viewer_y;

                /* Pixel centres sit at column + 0.5 - 32 in actor-relative units. */
                first_column = (int)ceil(screen_u - coverage + 31.5);
                last_column = (int)floor(screen_u + coverage + 31.5);
                for (int column = first_column; column <= last_column; ++column) {
                    size_t pixel;
                    double distance;

                    if (column < 0 || column >= WOLF_SPRITE_SIZE) {
                        continue;
                    }
                    pixel = wolf_sprite_pixel_index(column, row);
                    distance = fabs(screen_u - ((double)column - 31.5));

                    /*
                     * The nearest cube wins. Cubes at equal depth (common on
                     * diagonals) are resolved in favour of the one centred
                     * closest to the pixel so the result is deterministic.
                     */
                    if (voxel_depth > depth[pixel] + 1e-9 ||
                        (fabs(voxel_depth - depth[pixel]) <= 1e-9 && distance < centre_distance[pixel])) {
                        depth[pixel] = voxel_depth;
                        centre_distance[pixel] = distance;
                        image->palette_index[pixel] = model->palette_index[voxel];
                        image->opaque[pixel] = 1u;
                        if (voxel_for_pixel != NULL) {
                            voxel_for_pixel[pixel] = (int32_t)voxel;
                        }
                    }
                }
            }
        }
    }
}

/* Intersect a ray with the model's bounding box, returning the entry distance. */
static bool ray_enter_volume(const double origin[3], const double direction[3], double *entry)
{
    double near_distance = -DBL_MAX;
    double far_distance = DBL_MAX;

    for (int axis = 0; axis < 3; ++axis) {
        if (fabs(direction[axis]) < 1e-12) {
            if (origin[axis] < 0.0 || origin[axis] > (double)WOLF_VOXEL_DIM) {
                return false;
            }
            continue;
        }
        {
            double first = (0.0 - origin[axis]) / direction[axis];
            double second = ((double)WOLF_VOXEL_DIM - origin[axis]) / direction[axis];

            if (first > second) {
                double swap = first;
                first = second;
                second = swap;
            }
            near_distance = first > near_distance ? first : near_distance;
            far_distance = second < far_distance ? second : far_distance;
        }
    }
    if (near_distance > far_distance || far_distance < 0.0) {
        return false;
    }
    *entry = near_distance > 0.0 ? near_distance : 0.0;
    return true;
}

void wolf_voxel_render_preview(
    const WolfVoxelModel *model,
    const uint8_t *palette_rgba,
    double yaw_degrees,
    double pitch_degrees,
    int size,
    uint8_t *rgba)
{
    WolfViewBasis basis = wolf_view_basis(yaw_degrees);
    double pitch = pitch_degrees * WOLF_PI / 180.0;
    /* Unit vector from the model toward the camera, raised by the pitch angle. */
    double toward_camera[3] = {
        basis.toward_viewer_x * cos(pitch), basis.toward_viewer_y * cos(pitch), sin(pitch)
    };
    double right[3] = {basis.right_x, basis.right_y, 0.0};
    double up[3] = {
        toward_camera[1] * right[2] - toward_camera[2] * right[1],
        toward_camera[2] * right[0] - toward_camera[0] * right[2],
        toward_camera[0] * right[1] - toward_camera[1] * right[0]
    };
    /* Fixed key light from the upper front-left so faces read clearly. */
    double light[3] = {-0.45, -0.55, 0.70};
    double light_length = sqrt(light[0] * light[0] + light[1] * light[1] + light[2] * light[2]);
    /* The view must enclose the whole cube from any yaw and pitch. */
    double view_extent = (double)WOLF_VOXEL_DIM * 0.9;

    for (int axis = 0; axis < 3; ++axis) {
        light[axis] /= light_length;
    }

    for (int pixel_row = 0; pixel_row < size; ++pixel_row) {
        for (int pixel_column = 0; pixel_column < size; ++pixel_column) {
            double screen_x = (((double)pixel_column + 0.5) / (double)size * 2.0 - 1.0) * view_extent;
            double screen_y = (1.0 - ((double)pixel_row + 0.5) / (double)size * 2.0) * view_extent;
            double origin[3];
            double direction[3];
            double entry;
            uint8_t *out = rgba + ((size_t)pixel_row * (size_t)size + (size_t)pixel_column) * 4u;

            out[0] = 0u;
            out[1] = 0u;
            out[2] = 0u;
            out[3] = 0u;
            /* Start well outside the volume, on the camera side of the model centre. */
            for (int axis = 0; axis < 3; ++axis) {
                origin[axis] = (double)WOLF_VOXEL_DIM * 0.5 + right[axis] * screen_x + up[axis] * screen_y +
                    toward_camera[axis] * (double)WOLF_VOXEL_DIM * 2.0;
                direction[axis] = -toward_camera[axis];
            }
            if (!ray_enter_volume(origin, direction, &entry)) {
                continue;
            }

            /*
             * Amanatides-Woo traversal: step cell by cell along the ray and
             * remember which axis was crossed last, which gives the face
             * normal of the first occupied voxel for flat shading.
             */
            {
                double position[3];
                int cell[3];
                int step[3];
                double next_boundary[3];
                double boundary_spacing[3];
                int last_axis = 0;

                for (int axis = 0; axis < 3; ++axis) {
                    position[axis] = origin[axis] + direction[axis] * (entry + 1e-6);
                    cell[axis] = (int)floor(position[axis]);
                    if (cell[axis] < 0) {
                        cell[axis] = 0;
                    }
                    if (cell[axis] >= WOLF_VOXEL_DIM) {
                        cell[axis] = WOLF_VOXEL_DIM - 1;
                    }
                    step[axis] = direction[axis] >= 0.0 ? 1 : -1;
                    if (fabs(direction[axis]) < 1e-12) {
                        next_boundary[axis] = DBL_MAX;
                        boundary_spacing[axis] = DBL_MAX;
                    } else {
                        double boundary = step[axis] > 0 ? (double)(cell[axis] + 1) : (double)cell[axis];

                        next_boundary[axis] = (boundary - position[axis]) / direction[axis];
                        boundary_spacing[axis] = fabs(1.0 / direction[axis]);
                    }
                }
                /* The entry face is the axis whose slab was entered last. */
                {
                    double largest_entry = -DBL_MAX;

                    for (int axis = 0; axis < 3; ++axis) {
                        if (fabs(direction[axis]) >= 1e-12) {
                            double slab_entry = (step[axis] > 0 ? 0.0 - origin[axis] :
                                (double)WOLF_VOXEL_DIM - origin[axis]) / direction[axis];

                            if (slab_entry > largest_entry) {
                                largest_entry = slab_entry;
                                last_axis = axis;
                            }
                        }
                    }
                }

                while (wolf_voxel_in_bounds(cell[0], cell[1], cell[2])) {
                    size_t voxel = wolf_voxel_index(cell[0], cell[1], cell[2]);

                    if (model->occupied[voxel]) {
                        double normal[3] = {0.0, 0.0, 0.0};
                        double lambert;
                        double shade;
                        const uint8_t *colour = palette_rgba + (size_t)model->palette_index[voxel] * 4u;

                        normal[last_axis] = (double)-step[last_axis];
                        lambert = normal[0] * light[0] + normal[1] * light[1] + normal[2] * light[2];
                        shade = 0.55 + 0.45 * (lambert > 0.0 ? lambert : 0.0);
                        out[0] = (uint8_t)lround((double)colour[0] * shade);
                        out[1] = (uint8_t)lround((double)colour[1] * shade);
                        out[2] = (uint8_t)lround((double)colour[2] * shade);
                        out[3] = 255u;
                        break;
                    }
                    if (next_boundary[0] <= next_boundary[1] && next_boundary[0] <= next_boundary[2]) {
                        last_axis = 0;
                    } else if (next_boundary[1] <= next_boundary[2]) {
                        last_axis = 1;
                    } else {
                        last_axis = 2;
                    }
                    cell[last_axis] += step[last_axis];
                    next_boundary[last_axis] += boundary_spacing[last_axis];
                }
            }
        }
    }
}
