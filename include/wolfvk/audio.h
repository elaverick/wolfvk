#ifndef WOLFVK_AUDIO_H
#define WOLFVK_AUDIO_H

#include "wolfvk/audio_spatial.h"

#include <stdbool.h>
#include <stddef.h>

#define WOLF_AUDIO_MAX_SOUNDS 96
#define WOLF_AUDIO_MAX_VOICES 32
#define WOLF_AUDIO_NAME_CAPACITY 48

typedef struct MIX_Mixer MIX_Mixer;
typedef struct MIX_Audio MIX_Audio;
typedef struct MIX_Track MIX_Track;

typedef struct WolfAudioSound {
    char name[WOLF_AUDIO_NAME_CAPACITY];
    MIX_Audio *audio;
} WolfAudioSound;

typedef struct WolfAudioVoice {
    MIX_Track *track;
} WolfAudioVoice;

typedef struct WolfAudio {
    MIX_Mixer *mixer;
    WolfAudioSound sounds[WOLF_AUDIO_MAX_SOUNDS];
    WolfAudioVoice voices[WOLF_AUDIO_MAX_VOICES];
    WolfAudioVector3 listener_position;
    float listener_yaw_radians;
    size_t sound_count;
    bool initialized;
} WolfAudio;

/* Open SDL3_mixer and prepare a fixed pool of independently positioned voices. */
bool wolf_audio_initialize(WolfAudio *audio);

/* Load one OGG/WAV effect once; sounds are cached until audio shutdown. */
bool wolf_audio_load(WolfAudio *audio, const char *name, const char *path);

/* Set the listener pose in the same world-space coordinates as emitters. */
bool wolf_audio_set_listener(
    WolfAudio *audio,
    WolfAudioVector3 position,
    float yaw_radians);

/* Play a cached effect at a world-space source position. */
bool wolf_audio_play(
    WolfAudio *audio,
    const char *name,
    WolfAudioVector3 source_position);

/* Stop voices and release all mixer resources. Safe on a zero-initialized object. */
void wolf_audio_shutdown(WolfAudio *audio);

#endif
