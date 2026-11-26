#ifndef WOLFVK_AUDIO_SPATIAL_H
#define WOLFVK_AUDIO_SPATIAL_H

#include <stdbool.h>

typedef struct WolfAudioVector3 {
    float x;
    float y;
    float z;
} WolfAudioVector3;

/*
 * Convert a world-space emitter position into the listener-relative coordinate
 * system used by spatial audio. Yaw follows WolfVK's camera convention:
 * zero faces north (-Z), increasing clockwise towards east (+X).
 *
 * The result uses +X for listener-right, +Y for up and -Z for forward.
 * Returns false for non-finite inputs or a null output pointer.
 */
bool wolf_audio_relative_position(
    WolfAudioVector3 listener_position,
    float listener_yaw_radians,
    WolfAudioVector3 source_position,
    WolfAudioVector3 *relative_position);

#endif
