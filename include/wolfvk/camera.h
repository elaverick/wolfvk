#ifndef WOLFVK_CAMERA_H
#define WOLFVK_CAMERA_H

#include <stdbool.h>

typedef struct WolfCamera {
    float position[3];
    float yaw_radians;
    float pitch_radians;
    float vertical_fov_radians;
    float near_clip;
    float far_clip;
} WolfCamera;

/* Initialise a first-person camera looking along the negative Z axis. */
void wolf_camera_initialize(WolfCamera *camera);

/* Build a column-major Vulkan-compatible perspective view-projection matrix. */
bool wolf_camera_build_view_projection(
    const WolfCamera *camera,
    float aspect_ratio,
    float matrix[16]);

#endif
