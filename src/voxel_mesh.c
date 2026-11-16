#include "wolfvk/voxel_mesh.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* A growable quad list; meshing cannot know its output size in advance. */
typedef struct QuadList {
    VulkanQuad *quads;
    uint32_t count;
    uint32_t capacity;
} QuadList;

static bool quad_list_push(QuadList *list, const VulkanQuad *quad)
{
    if (list->count == list->capacity) {
        uint32_t new_capacity = list->capacity == 0u ? 256u : list->capacity * 2u;
        VulkanQuad *grown;

        if (new_capacity < list->capacity) {
            return false;
        }
        grown = realloc(list->quads, (size_t)new_capacity * sizeof(*grown));
        if (grown == NULL) {
            return false;
        }
        list->quads = grown;
        list->capacity = new_capacity;
    }
    list->quads[list->count++] = *quad;
    return true;
}

/* The original palette is sRGB; vertex colours are interpolated and written in linear space. */
static float srgb_to_linear(uint8_t value)
{
    float normalized = (float)value / 255.0f;

    if (normalized <= 0.04045f) {
        return normalized / 12.92f;
    }
    return powf((normalized + 0.055f) / 1.055f, 2.4f);
}

float wolf_voxel_mesh_face_shade(uint32_t axis, bool positive_direction)
{
    /*
     * A fixed light from above and slightly in front keeps the original
     * colours on top and front faces while separating the sides, much as
     * the original engine darkens one wall orientation.
     */
    switch (axis) {
    case 0u:
        return 0.78f;
    case 1u:
        return 0.9f;
    default:
        return positive_direction ? 1.0f : 0.55f;
    }
}

/*
 * Convert a VOX lattice corner into model space. VOX x runs to the viewer's
 * right when the actor faces them; an actor facing north is seen from the
 * north, where the viewer's right is west, so x is negated.
 */
static void vox_corner_to_model(
    const WolfVoxModel *model,
    const uint32_t corner[3],
    float position[3])
{
    float centre_x = (float)model->size[0] * 0.5f;
    float centre_y = (float)model->size[1] * 0.5f;

    position[0] = -((float)corner[0] - centre_x) * WOLF_VOXEL_SIZE;
    position[1] = (float)corner[2] * WOLF_VOXEL_SIZE;
    position[2] = ((float)corner[1] - centre_y) * WOLF_VOXEL_SIZE;
}

/* Emit one merged rectangle lying in the plane `plane` of `axis`, spanning [u0,u1) x [v0,v1). */
static bool emit_face(
    QuadList *list,
    const WolfVoxModel *model,
    uint32_t axis,
    uint32_t plane,
    uint32_t u0,
    uint32_t u1,
    uint32_t v0,
    uint32_t v1,
    const float colour[3])
{
    const uint32_t u_axis = (axis + 1u) % 3u;
    const uint32_t v_axis = (axis + 2u) % 3u;
    const uint32_t corner_u[4] = {u0, u1, u1, u0};
    const uint32_t corner_v[4] = {v0, v0, v1, v1};
    VulkanQuad quad;

    memset(&quad, 0, sizeof(quad));
    for (uint32_t corner_index = 0u; corner_index < 4u; ++corner_index) {
        uint32_t corner[3];

        corner[axis] = plane;
        corner[u_axis] = corner_u[corner_index];
        corner[v_axis] = corner_v[corner_index];
        vox_corner_to_model(model, corner, quad.vertices[corner_index].position);
        memcpy(quad.vertices[corner_index].colour, colour, sizeof(quad.vertices[0].colour));
    }
    return quad_list_push(list, &quad);
}

static size_t grid_index(const WolfVoxModel *model, uint32_t x, uint32_t y, uint32_t z)
{
    return ((size_t)z * model->size[1] + y) * model->size[0] + x;
}

/* Colour of a lattice cell, or 0 when the cell is empty or outside the model. */
static uint8_t colour_at(
    const WolfVoxModel *model,
    const uint8_t *grid,
    const int32_t cell[3])
{
    for (uint32_t axis = 0u; axis < 3u; ++axis) {
        if (cell[axis] < 0 || (uint32_t)cell[axis] >= model->size[axis]) {
            return 0u;
        }
    }
    return grid[grid_index(model, (uint32_t)cell[0], (uint32_t)cell[1], (uint32_t)cell[2])];
}

/*
 * Faces on the floor (z = 0, facing down) and against the ceiling (the top
 * of a full-height model, facing up) are hidden by the floor and roof planes.
 */
static bool face_is_hidden_by_environment(uint32_t axis, bool positive, uint32_t layer)
{
    if (axis != 2u) {
        return false;
    }
    return positive ? layer + 1u >= WOLF_VOXELS_PER_TILE : layer == 0u;
}

/*
 * Mesh every layer of one face direction. For each layer a mask records the
 * colour of each exposed face; the mask is then covered by maximal
 * same-coloured rectangles, scanning row by row.
 */
static bool mesh_direction(
    QuadList *list,
    const WolfVoxModel *model,
    const uint8_t *grid,
    const uint8_t palette_rgba[256u * 4u],
    uint32_t axis,
    bool positive,
    uint8_t *mask)
{
    const uint32_t u_axis = (axis + 1u) % 3u;
    const uint32_t v_axis = (axis + 2u) % 3u;
    const uint32_t u_size = model->size[u_axis];
    const uint32_t v_size = model->size[v_axis];
    const float shade = wolf_voxel_mesh_face_shade(axis, positive);

    for (uint32_t layer = 0u; layer < model->size[axis]; ++layer) {
        if (face_is_hidden_by_environment(axis, positive, layer)) {
            continue;
        }

        for (uint32_t v = 0u; v < v_size; ++v) {
            for (uint32_t u = 0u; u < u_size; ++u) {
                int32_t cell[3];
                int32_t neighbour[3];
                uint8_t colour;

                cell[axis] = (int32_t)layer;
                cell[u_axis] = (int32_t)u;
                cell[v_axis] = (int32_t)v;
                memcpy(neighbour, cell, sizeof(neighbour));
                neighbour[axis] += positive ? 1 : -1;

                colour = colour_at(model, grid, cell);
                mask[(size_t)v * u_size + u] =
                    colour != 0u && colour_at(model, grid, neighbour) == 0u ? colour : 0u;
            }
        }

        for (uint32_t v = 0u; v < v_size; ++v) {
            for (uint32_t u = 0u; u < u_size;) {
                uint8_t colour_index = mask[(size_t)v * u_size + u];
                uint32_t width = 1u;
                uint32_t height = 1u;
                bool row_matches = true;
                float colour[3];
                const uint8_t *rgb;

                if (colour_index == 0u) {
                    ++u;
                    continue;
                }

                while (u + width < u_size && mask[(size_t)v * u_size + u + width] == colour_index) {
                    ++width;
                }
                while (v + height < v_size && row_matches) {
                    for (uint32_t step = 0u; step < width; ++step) {
                        if (mask[(size_t)(v + height) * u_size + u + step] != colour_index) {
                            row_matches = false;
                            break;
                        }
                    }
                    if (row_matches) {
                        ++height;
                    }
                }
                for (uint32_t row = v; row < v + height; ++row) {
                    memset(mask + (size_t)row * u_size + u, 0, width);
                }

                rgb = palette_rgba + (size_t)(colour_index - 1u) * 4u;
                colour[0] = srgb_to_linear(rgb[0]) * shade;
                colour[1] = srgb_to_linear(rgb[1]) * shade;
                colour[2] = srgb_to_linear(rgb[2]) * shade;

                if (!emit_face(
                        list, model, axis, positive ? layer + 1u : layer,
                        u, u + width, v, v + height, colour)) {
                    return false;
                }
                u += width;
            }
        }
    }
    return true;
}

bool wolf_voxel_mesh_build(
    const WolfVoxModel *model,
    const uint8_t palette_rgba[256u * 4u],
    VulkanQuad **quads,
    uint32_t *quad_count)
{
    QuadList list = {0};
    uint8_t *grid;
    uint8_t *mask;
    size_t cell_count;
    bool built = true;

    if (quads != NULL) {
        *quads = NULL;
    }
    if (quad_count != NULL) {
        *quad_count = 0u;
    }
    if (model == NULL || palette_rgba == NULL || quads == NULL || quad_count == NULL ||
        (model->voxel_count > 0u && model->voxels == NULL)) {
        return false;
    }
    for (uint32_t axis = 0u; axis < 3u; ++axis) {
        if (model->size[axis] == 0u || model->size[axis] > WOLF_VOX_MAX_DIMENSION) {
            return false;
        }
    }

    cell_count = (size_t)model->size[0] * model->size[1] * model->size[2];
    grid = calloc(cell_count, 1u);
    mask = calloc((size_t)WOLF_VOX_MAX_DIMENSION * WOLF_VOX_MAX_DIMENSION, 1u);
    if (grid == NULL || mask == NULL) {
        free(grid);
        free(mask);
        return false;
    }

    for (uint32_t voxel = 0u; voxel < model->voxel_count; ++voxel) {
        const WolfVoxVoxel *entry = &model->voxels[voxel];

        if (entry->x < model->size[0] && entry->y < model->size[1] && entry->z < model->size[2]) {
            grid[grid_index(model, entry->x, entry->y, entry->z)] = entry->colour_index;
        }
    }

    for (uint32_t axis = 0u; axis < 3u && built; ++axis) {
        built = mesh_direction(&list, model, grid, palette_rgba, axis, false, mask) &&
            mesh_direction(&list, model, grid, palette_rgba, axis, true, mask);
    }

    free(grid);
    free(mask);
    if (!built) {
        free(list.quads);
        return false;
    }

    *quads = list.quads;
    *quad_count = list.count;
    return true;
}
