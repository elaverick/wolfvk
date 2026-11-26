#ifndef WOLF_AUDIO_ADLIB_ARCHIVE_H
#define WOLF_AUDIO_ADLIB_ARCHIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WOLF_AUDIO_ADLIB_SOUND_COUNT 87u
#define WOLF_AUDIO_ADLIB_CHUNK_COUNT 288u
#define WOLF_AUDIO_ADLIB_START_CHUNK 87u
#define WOLF_AUDIO_ADLIB_HEADER_SIZE ((WOLF_AUDIO_ADLIB_CHUNK_COUNT + 1u) * 4u)
#define WOLF_AUDIO_ADLIB_COMMON_HEADER_SIZE 6u
#define WOLF_AUDIO_ADLIB_INSTRUMENT_SIZE 16u
#define WOLF_AUDIO_ADLIB_DATA_OFFSET 23u

typedef struct WolfAudioAdlibSound {
    uint8_t instrument[WOLF_AUDIO_ADLIB_INSTRUMENT_SIZE];
    uint8_t block;
    const uint8_t *frequency_data;
    uint32_t frequency_data_length;
} WolfAudioAdlibSound;

typedef struct WolfAudioAdlibArchive {
    uint8_t *audio_data;
    size_t audio_size;
    uint32_t offsets[WOLF_AUDIO_ADLIB_CHUNK_COUNT + 1u];
} WolfAudioAdlibArchive;

/*
 * Decode the retail AUDIOHED.WL6 offset table and copy its AUDIOT.WL6 data.
 * The standard WL6 archive stores 288 chunks and a final end offset.
 */
bool wolf_audio_adlib_archive_decode(
    const uint8_t *header_data,
    size_t header_size,
    const uint8_t *audio_data,
    size_t audio_size,
    WolfAudioAdlibArchive *archive,
    char *error_message,
    size_t error_message_size);

/* Return one of the original 87 AdLib effect chunks (AUDIOHED chunk 87-173). */
bool wolf_audio_adlib_archive_get_sound(
    const WolfAudioAdlibArchive *archive,
    uint16_t sound_index,
    WolfAudioAdlibSound *sound,
    char *error_message,
    size_t error_message_size);

/* Release the copied AUDIOT data and reset the archive. */
void wolf_audio_adlib_archive_destroy(WolfAudioAdlibArchive *archive);

#endif
