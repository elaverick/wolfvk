#include "unity.h"

#include "wolfvk/voxel_mesh.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint8_t test_palette[256u * 4u];

/* Entry 0 is white, entry 1 black and entry 2 a mid grey; colour c uses entry c - 1. */
static const uint8_t *palette(void)
{
    memset(test_palette, 0, sizeof(test_palette));
    memset(test_palette, 255, 4u);
    test_palette[4u * 2u] = 128u;
    test_palette[4u * 2u + 1u] = 128u;
    test_palette[4u * 2u + 2u] = 128u;
    return test_palette;
}

static WolfVoxModel make_model(WolfVoxVoxel *voxels, uint32_t voxel_count)
{
    WolfVoxModel model;

    model.size[0] = 64u;
    model.size[1] = 64u;
    model.size[2] = 64u;
    model.voxel_count = voxel_count;
    model.voxels = voxels;
    return model;
}

/* Smallest and largest vertex coordinate along one axis, over every quad. */
static void quad_bounds(const VulkanQuad *quads, uint32_t quad_count, int axis, float *minimum, float *maximum)
{
    *minimum = INFINITY;
    *maximum = -INFINITY;
    for (uint32_t quad = 0u; quad < quad_count; ++quad) {
        for (int vertex = 0; vertex < 4; ++vertex) {
            float value = quads[quad].vertices[vertex].position[axis];

            *minimum = value < *minimum ? value : *minimum;
            *maximum = value > *maximum ? value : *maximum;
        }
    }
}

void test_voxel_mesh_omits_the_face_resting_on_the_floor(void)
{
    WolfVoxVoxel voxel = {32u, 32u, 0u, 1u};
    WolfVoxModel model = make_model(&voxel, 1u);
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;
    float minimum;
    float maximum;

    TEST_ASSERT_TRUE(wolf_voxel_mesh_build(&model, palette(), &quads, &quad_count));
    /* Four sides and a top; the bottom face lies on the floor plane and can never be seen. */
    TEST_ASSERT_EQUAL_UINT32(5u, quad_count);

    quad_bounds(quads, quad_count, 1, &minimum, &maximum);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, minimum);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, WOLF_VOXEL_SIZE, maximum);
    free(quads);
}

void test_voxel_mesh_omits_faces_against_the_ceiling(void)
{
    WolfVoxVoxel voxel = {10u, 10u, 63u, 1u};
    WolfVoxModel model = make_model(&voxel, 1u);
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;
    float minimum;
    float maximum;

    TEST_ASSERT_TRUE(wolf_voxel_mesh_build(&model, palette(), &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(5u, quad_count);
    quad_bounds(quads, quad_count, 1, &minimum, &maximum);
    /* A full-height model reaches exactly the original wall height. */
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, WOLF_WORLD_WALL_HEIGHT, maximum);
    free(quads);
}

void test_voxel_mesh_merges_same_coloured_faces_and_keeps_colours_apart(void)
{
    WolfVoxVoxel same_colour[] = {{20u, 20u, 5u, 1u}, {21u, 20u, 5u, 1u}};
    WolfVoxVoxel two_colours[] = {{20u, 20u, 5u, 1u}, {21u, 20u, 5u, 3u}};
    WolfVoxModel model = make_model(same_colour, 2u);
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;

    /* A 2x1x1 bar has six faces once the shared inner faces are removed and the long sides merged. */
    TEST_ASSERT_TRUE(wolf_voxel_mesh_build(&model, palette(), &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(6u, quad_count);
    free(quads);

    /* Different colours cannot share a rectangle: the four long sides split, giving ten faces. */
    model = make_model(two_colours, 2u);
    TEST_ASSERT_TRUE(wolf_voxel_mesh_build(&model, palette(), &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(10u, quad_count);
    free(quads);
}

void test_voxel_mesh_puts_the_axis_at_the_origin_and_faces_north(void)
{
    /*
     * A voxel at the front-right of the footprint, as seen by a viewer the
     * model faces: low y is the front and high x the viewer's right.
     */
    WolfVoxVoxel voxel = {63u, 0u, 0u, 1u};
    WolfVoxModel model = make_model(&voxel, 1u);
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;
    float minimum;
    float maximum;

    TEST_ASSERT_TRUE(wolf_voxel_mesh_build(&model, palette(), &quads, &quad_count));

    /* The model faces north (-z), so its front lies at the most negative z: half a tile from the axis. */
    quad_bounds(quads, quad_count, 2, &minimum, &maximum);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -WOLF_WORLD_TILE_SIZE * 0.5f, minimum);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -WOLF_WORLD_TILE_SIZE * 0.5f + WOLF_VOXEL_SIZE, maximum);

    /* A viewer standing north of it and looking south has west (-x) on their right. */
    quad_bounds(quads, quad_count, 0, &minimum, &maximum);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -WOLF_WORLD_TILE_SIZE * 0.5f, minimum);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -WOLF_WORLD_TILE_SIZE * 0.5f + WOLF_VOXEL_SIZE, maximum);
    free(quads);
}

void test_voxel_mesh_converts_palette_colours_to_shaded_linear_values(void)
{
    WolfVoxVoxel voxel = {32u, 32u, 4u, 3u};
    WolfVoxModel model = make_model(&voxel, 1u);
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 0u;
    bool found_top = false;
    /* sRGB 128 is linear 0.2158. */
    const float linear_grey = 0.21586f;

    TEST_ASSERT_TRUE(wolf_voxel_mesh_build(&model, palette(), &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(6u, quad_count);
    for (uint32_t quad = 0u; quad < quad_count; ++quad) {
        const VulkanVertex *vertex = &quads[quad].vertices[0];
        bool is_top = true;

        TEST_ASSERT_EQUAL_UINT32(0u, vertex->use_texture);
        for (int corner = 0; corner < 4; ++corner) {
            is_top = is_top && fabsf(quads[quad].vertices[corner].position[1] - 5.0f * WOLF_VOXEL_SIZE) < 1e-6f;
        }
        if (is_top) {
            found_top = true;
            TEST_ASSERT_FLOAT_WITHIN(1e-4f, linear_grey * wolf_voxel_mesh_face_shade(2u, true), vertex->colour[0]);
        }
        /* Every face is a shaded copy of the same grey, never brighter than the colour itself. */
        TEST_ASSERT_TRUE(vertex->colour[0] <= linear_grey + 1e-4f);
        TEST_ASSERT_FLOAT_WITHIN(1e-6f, vertex->colour[0], vertex->colour[2]);
    }
    TEST_ASSERT_TRUE(found_top);
    TEST_ASSERT_TRUE(wolf_voxel_mesh_face_shade(2u, true) > wolf_voxel_mesh_face_shade(0u, true));
    TEST_ASSERT_TRUE(wolf_voxel_mesh_face_shade(0u, false) > wolf_voxel_mesh_face_shade(2u, false));
    free(quads);
}

void test_voxel_mesh_handles_empty_and_invalid_models(void)
{
    WolfVoxVoxel voxel = {1u, 1u, 1u, 1u};
    WolfVoxModel model = make_model(NULL, 0u);
    VulkanQuad *quads = NULL;
    uint32_t quad_count = 99u;

    TEST_ASSERT_TRUE(wolf_voxel_mesh_build(&model, palette(), &quads, &quad_count));
    TEST_ASSERT_EQUAL_UINT32(0u, quad_count);
    TEST_ASSERT_NULL(quads);

    model = make_model(&voxel, 1u);
    model.size[1] = 65u;
    TEST_ASSERT_FALSE(wolf_voxel_mesh_build(&model, palette(), &quads, &quad_count));
    model.size[1] = 0u;
    TEST_ASSERT_FALSE(wolf_voxel_mesh_build(&model, palette(), &quads, &quad_count));
    model = make_model(NULL, 1u);
    TEST_ASSERT_FALSE(wolf_voxel_mesh_build(&model, palette(), &quads, &quad_count));
    TEST_ASSERT_FALSE(wolf_voxel_mesh_build(NULL, palette(), &quads, &quad_count));
    TEST_ASSERT_FALSE(wolf_voxel_mesh_build(&model, NULL, &quads, &quad_count));
}
