#include "wolfvk/audio_spatial.h"

#include <math.h>

bool wolf_audio_relative_position(
    WolfAudioVector3 listener_position,
    float listener_yaw_radians,
    WolfAudioVector3 source_position,
    WolfAudioVector3 *relative_position)
{
    float offset_x;
    float offset_y;
    float offset_z;
    float sine_yaw;
    float cosine_yaw;

    if (relative_position == NULL ||
        !isfinite(listener_position.x) ||
        !isfinite(listener_position.y) ||
        !isfinite(listener_position.z) ||
        !isfinite(listener_yaw_radians) ||
        !isfinite(source_position.x) ||
        !isfinite(source_position.y) ||
        !isfinite(source_position.z)) {
        return false;
    }

    offset_x = source_position.x - listener_position.x;
    offset_y = source_position.y - listener_position.y;
    offset_z = source_position.z - listener_position.z;
    sine_yaw = sinf(listener_yaw_radians);
    cosine_yaw = cosf(listener_yaw_radians);

    /*
     * Camera forward is (sin(yaw), -cos(yaw)) in the X/Z plane.
     * Camera right is (cos(yaw), sin(yaw)).
     */
    relative_position->x = offset_x * cosine_yaw + offset_z * sine_yaw;
    relative_position->y = offset_y;
    relative_position->z = -offset_x * sine_yaw + offset_z * cosine_yaw;
    return true;
}
