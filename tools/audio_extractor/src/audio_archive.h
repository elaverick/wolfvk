#ifndef WOLFVK_AUDIO_ARCHIVE_H
#define WOLFVK_AUDIO_ARCHIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WOLF_AUDIO_DIGI_SAMPLE_RATE 7000u
#define WOLF_AUDIO_DIGI_PAGE_SIZE 4096u

typedef struct WolfAudioPcmSample {
    uint8_t *data;
    uint32_t length;
} WolfAudioPcmSample;

typedef struct WolfAudioArchive {
    WolfAudioPcmSample *samples;
    uint16_t sample_count;
} WolfAudioArchive;

typedef struct WolfAudioDigiSound {
    const char *name;
    uint16_t sample_index;
} WolfAudioDigiSound;

/*
 * Read and concatenate the digitised sample pages in a retail WL6 VSWAP file.
 * Sample bytes are unsigned 8-bit mono PCM at 7000 Hz, as configured by the
 * original Wolf3D SoundBlaster driver.
 */
bool wolf_audio_archive_decode(
    const uint8_t *file_data,
    size_t file_size,
    WolfAudioArchive *archive,
    char *error_message,
    size_t error_message_size);

/* Load a VSWAP file from disk and decode its digitised samples. */
bool wolf_audio_archive_load(
    const char *path,
    WolfAudioArchive *archive,
    char *error_message,
    size_t error_message_size);

/* Release sample memory and leave the archive empty. */
void wolf_audio_archive_destroy(WolfAudioArchive *archive);

/*
 * Return the original WL6 DigiMap aliases. Several named sound events share
 * one digitised sample, so multiple entries can reference the same index.
 */
const WolfAudioDigiSound *wolf_audio_digi_sound_map(size_t *sound_count);

#endif
