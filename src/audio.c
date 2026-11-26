#include "wolfvk/audio.h"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <math.h>
#include <string.h>

static WolfAudioSound *find_sound(WolfAudio *audio, const char *name)
{
    size_t index;

    for (index = 0; index < audio->sound_count; ++index) {
        if (strcmp(audio->sounds[index].name, name) == 0) {
            return &audio->sounds[index];
        }
    }

    return NULL;
}

bool wolf_audio_initialize(WolfAudio *audio)
{
    size_t index;

    if (audio == NULL) {
        return false;
    }

    memset(audio, 0, sizeof(*audio));
    if (!MIX_Init()) {
        SDL_Log("Could not initialise SDL3_mixer: %s", SDL_GetError());
        return false;
    }

    audio->mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (audio->mixer == NULL) {
        SDL_Log("Could not open the audio mixer: %s", SDL_GetError());
        MIX_Quit();
        return false;
    }

    for (index = 0; index < WOLF_AUDIO_MAX_VOICES; ++index) {
        audio->voices[index].track = MIX_CreateTrack(audio->mixer);
        if (audio->voices[index].track == NULL) {
            SDL_Log("Could not create audio voice %zu: %s", index, SDL_GetError());
            wolf_audio_shutdown(audio);
            return false;
        }
    }

    audio->initialized = true;
    return true;
}

bool wolf_audio_load(WolfAudio *audio, const char *name, const char *path)
{
    WolfAudioSound *sound;
    MIX_Audio *loaded;

    if (audio == NULL || !audio->initialized || name == NULL || path == NULL ||
        name[0] == '\0' || path[0] == '\0') {
        return false;
    }

    sound = find_sound(audio, name);
    if (sound != NULL) {
        return true;
    }
    if (audio->sound_count >= WOLF_AUDIO_MAX_SOUNDS ||
        strlen(name) >= WOLF_AUDIO_NAME_CAPACITY) {
        return false;
    }

    loaded = MIX_LoadAudio(audio->mixer, path, true);
    if (loaded == NULL) {
        SDL_Log("Could not load sound '%s' from '%s': %s", name, path, SDL_GetError());
        return false;
    }

    sound = &audio->sounds[audio->sound_count++];
    (void)SDL_strlcpy(sound->name, name, sizeof(sound->name));
    sound->audio = loaded;
    return true;
}

bool wolf_audio_set_listener(
    WolfAudio *audio,
    WolfAudioVector3 position,
    float yaw_radians)
{
    if (audio == NULL || !audio->initialized ||
        !isfinite(position.x) || !isfinite(position.y) ||
        !isfinite(position.z) || !isfinite(yaw_radians)) {
        return false;
    }

    audio->listener_position = position;
    audio->listener_yaw_radians = yaw_radians;
    return true;
}

bool wolf_audio_play(
    WolfAudio *audio,
    const char *name,
    WolfAudioVector3 source_position)
{
    WolfAudioSound *sound;
    WolfAudioVoice *voice = NULL;
    WolfAudioVector3 relative;
    MIX_Point3D position;
    size_t index;

    if (audio == NULL || !audio->initialized || name == NULL ||
        !wolf_audio_relative_position(
            audio->listener_position, audio->listener_yaw_radians,
            source_position, &relative)) {
        return false;
    }

    sound = find_sound(audio, name);
    if (sound == NULL) {
        return false;
    }

    for (index = 0; index < WOLF_AUDIO_MAX_VOICES; ++index) {
        if (!MIX_TrackPlaying(audio->voices[index].track)) {
            voice = &audio->voices[index];
            break;
        }
    }
    if (voice == NULL) {
        return false;
    }

    position.x = relative.x;
    position.y = relative.y;
    position.z = relative.z;
    if (!MIX_SetTrackAudio(voice->track, sound->audio) ||
        !MIX_SetTrack3DPosition(voice->track, &position) ||
        !MIX_PlayTrack(voice->track, 0)) {
        SDL_Log("Could not play spatial sound '%s': %s", name, SDL_GetError());
        return false;
    }

    return true;
}

void wolf_audio_shutdown(WolfAudio *audio)
{
    size_t index;

    if (audio == NULL) {
        return;
    }

    if (audio->mixer != NULL) {
        (void)MIX_StopAllTracks(audio->mixer, 0);
    }
    for (index = 0; index < WOLF_AUDIO_MAX_VOICES; ++index) {
        if (audio->voices[index].track != NULL) {
            MIX_DestroyTrack(audio->voices[index].track);
            audio->voices[index].track = NULL;
        }
    }
    for (index = 0; index < audio->sound_count; ++index) {
        if (audio->sounds[index].audio != NULL) {
            MIX_DestroyAudio(audio->sounds[index].audio);
            audio->sounds[index].audio = NULL;
        }
    }
    if (audio->mixer != NULL) {
        MIX_DestroyMixer(audio->mixer);
        audio->mixer = NULL;
        MIX_Quit();
    }
    audio->sound_count = 0;
    audio->initialized = false;
}
