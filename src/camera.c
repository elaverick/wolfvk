#include "wolfvk/camera.h"
#include "wolfvk/world.h"

#include <math.h>
#include <string.h>

#define WOLF_CAMERA_DEFAULT_FOV 1.22173048f
#define WOLF_CAMERA_PI 3.14159265f

/* Project vectors onto the camera basis when constructing the world-to-view transform. */
static float vector_dot(const float left[3], const float right[3])
{
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

/* Construct a perpendicular basis vector so camera roll stays fixed to world up. */
static void vector_cross(
    const float left[3],
    const float right[3],
    float result[3])
{
    result[0] = left[1] * right[2] - left[2] * right[1];
    result[1] = left[2] * right[0] - left[0] * right[2];
    result[2] = left[0] * right[1] - left[1] * right[0];
}

/* Reject degenerate camera directions instead of allowing invalid values into the view matrix. */
static bool vector_normalize(float vector[3])
{
    float length = sqrtf(vector_dot(vector, vector));

    if (!isfinite(length) || length <= 0.0001f) {
        return false;
    }

    vector[0] /= length;
    vector[1] /= length;
    vector[2] /= length;
    return true;
}

/* Combine projection and view transforms into the single matrix consumed by the vertex shader. */
static void matrix_multiply(
    float result[16],
    const float left[16],
    const float right[16])
{
    float product[16] = {0};

    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            for (int component = 0; component < 4; ++component) {
                product[column * 4 + row] +=
                    left[component * 4 + row] * right[column * 4 + component];
            }
        }
    }

    memcpy(result, product, sizeof(product));
}

/* Start at standing eye height with finite projection limits suitable for a room-scale level. */
void wolf_camera_initialize(WolfCamera *camera)
{
    if (camera == NULL) {
        return;
    }

    memset(camera, 0, sizeof(*camera));
    camera->position[1] = WOLF_WORLD_EYE_HEIGHT;
    camera->vertical_fov_radians = WOLF_CAMERA_DEFAULT_FOV;
    camera->near_clip = 0.1f;
    camera->far_clip = 2000.0f;
}

/* Build the camera transform and Vulkan projection from yaw/pitch without storing derived matrices. */
bool wolf_camera_build_view_projection(
    const WolfCamera *camera,
    float aspect_ratio,
    float matrix[16])
{
    const float world_up[3] = {0.0f, 1.0f, 0.0f};
    float forward[3];
    float right[3];
    float up[3];
    float view[16] = {0};
    float projection[16] = {0};
    float pitch_cosine;
    float projection_scale;

    if (camera == NULL || matrix == NULL ||
        !isfinite(aspect_ratio) || aspect_ratio <= 0.0f ||
        !isfinite(camera->vertical_fov_radians) ||
        camera->vertical_fov_radians <= 0.0f ||
        camera->vertical_fov_radians >= WOLF_CAMERA_PI ||
        !isfinite(camera->near_clip) || !isfinite(camera->far_clip) ||
        camera->near_clip <= 0.0f || camera->far_clip <= camera->near_clip ||
        !isfinite(camera->position[0]) || !isfinite(camera->position[1]) ||
        !isfinite(camera->position[2]) || !isfinite(camera->yaw_radians) ||
        !isfinite(camera->pitch_radians)) {
        return false;
    }

    /* Build an orthonormal camera basis; yaw zero faces down negative Z. */
    pitch_cosine = cosf(camera->pitch_radians);
    forward[0] = sinf(camera->yaw_radians) * pitch_cosine;
    forward[1] = sinf(camera->pitch_radians);
    forward[2] = -cosf(camera->yaw_radians) * pitch_cosine;

    vector_cross(forward, world_up, right);
    if (!vector_normalize(right)) {
        return false;
    }
    vector_cross(right, forward, up);

    view[0] = right[0];
    view[4] = right[1];
    view[8] = right[2];
    view[12] = -vector_dot(right, camera->position);

    view[1] = up[0];
    view[5] = up[1];
    view[9] = up[2];
    view[13] = -vector_dot(up, camera->position);

    view[2] = -forward[0];
    view[6] = -forward[1];
    view[10] = -forward[2];
    view[14] = vector_dot(forward, camera->position);
    view[15] = 1.0f;

    /* Vulkan clip space uses a 0..1 depth range and a downward framebuffer Y axis. */
    projection_scale = 1.0f / tanf(camera->vertical_fov_radians * 0.5f);
    projection[0] = projection_scale / aspect_ratio;
    projection[5] = -projection_scale;
    projection[10] = camera->far_clip / (camera->near_clip - camera->far_clip);
    projection[11] = -1.0f;
    projection[14] =
        (camera->far_clip * camera->near_clip) /
        (camera->near_clip - camera->far_clip);

    matrix_multiply(matrix, projection, view);
    return true;
}
