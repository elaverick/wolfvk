#include "unity.h"

#include "wolfvk/camera.h"
#include "wolfvk/world.h"

#include <math.h>

void test_default_camera_builds_vulkan_projection(void)
{
    WolfCamera camera;
    float matrix[16] = {0};

    wolf_camera_initialize(&camera);

    TEST_ASSERT_TRUE(wolf_camera_build_view_projection(&camera, 16.0f / 9.0f, matrix));
    TEST_ASSERT_TRUE(matrix[0] > 0.0f);
    TEST_ASSERT_TRUE(matrix[5] < 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, matrix[11]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, matrix[15]);

    for (int index = 0; index < 16; ++index) {
        TEST_ASSERT_TRUE(isfinite(matrix[index]));
    }
}

void test_camera_eye_height_is_half_the_wall_height(void)
{
    WolfCamera camera;

    wolf_camera_initialize(&camera);

    /* Keep the player's view centred vertically between floor and ceiling. */
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, WOLF_WORLD_EYE_HEIGHT, camera.position[1]);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, WOLF_WORLD_WALL_HEIGHT * 0.5f, camera.position[1]);
}

void test_camera_yaw_changes_view_direction(void)
{
    WolfCamera camera;
    float forward_matrix[16] = {0};
    float turned_matrix[16] = {0};

    wolf_camera_initialize(&camera);
    TEST_ASSERT_TRUE(wolf_camera_build_view_projection(&camera, 1.0f, forward_matrix));

    camera.yaw_radians = 1.57079633f;
    TEST_ASSERT_TRUE(wolf_camera_build_view_projection(&camera, 1.0f, turned_matrix));
    TEST_ASSERT_TRUE(fabsf(forward_matrix[2] - turned_matrix[2]) > 0.1f);
}

void test_camera_rejects_invalid_projection_parameters(void)
{
    WolfCamera camera;
    float matrix[16] = {0};

    wolf_camera_initialize(&camera);

    TEST_ASSERT_FALSE(wolf_camera_build_view_projection(&camera, 0.0f, matrix));
    TEST_ASSERT_FALSE(wolf_camera_build_view_projection(&camera, -1.0f, matrix));
    TEST_ASSERT_FALSE(wolf_camera_build_view_projection(NULL, 1.0f, matrix));
    TEST_ASSERT_FALSE(wolf_camera_build_view_projection(&camera, 1.0f, NULL));

    camera.far_clip = camera.near_clip;
    TEST_ASSERT_FALSE(wolf_camera_build_view_projection(&camera, 1.0f, matrix));
}
