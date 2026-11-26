#include "adlib_archive.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

static void set_error(char *error_message, size_t error_message_size, const char *message)
{
    if (error_message != NULL && error_message_size > 0u) {
        (void)snprintf(error_message, error_message_size, "%s", message);
    }
}

bool wolf_audio_adlib_archive_decode(
    const uint8_t *header_data,
    size_t header_size,
    const uint8_t *audio_data,
    size_t audio_size,
    WolfAudioAdlibArchive *archive,
    char *error_message,
    size_t error_message_size)
{
    uint8_t *audio_copy;

    if (error_message != NULL && error_message_size > 0u) {
        error_message[0] = '\0';
    }
    if (header_data == NULL || audio_data == NULL || archive == NULL ||
        archive->audio_data != NULL || archive->audio_size != 0u ||
        header_size < WOLF_AUDIO_ADLIB_HEADER_SIZE || audio_size == 0u) {
        set_error(error_message, error_message_size, "Invalid or incomplete AUDIOHED/AUDIOT arguments.");
        return false;
    }

    for (size_t index = 0u; index <= WOLF_AUDIO_ADLIB_CHUNK_COUNT; ++index) {
        uint32_t offset = read_u32_le(header_data + index * 4u);

        if ((size_t)offset > audio_size ||
            (index > 0u && offset < archive->offsets[index - 1u])) {
            set_error(error_message, error_message_size, "AUDIOHED contains a non-monotonic or out-of-range chunk offset.");
            return false;
        }
        archive->offsets[index] = offset;
    }

    /* Trailing bytes after the final indexed chunk are harmless and unused. */

    audio_copy = malloc(audio_size);
    if (audio_copy == NULL) {
        memset(archive->offsets, 0, sizeof(archive->offsets));
        set_error(error_message, error_message_size, "Could not allocate memory for AUDIOT data.");
        return false;
    }

    memcpy(audio_copy, audio_data, audio_size);
    archive->audio_data = audio_copy;
    archive->audio_size = audio_size;
    return true;
}

bool wolf_audio_adlib_archive_get_sound(
    const WolfAudioAdlibArchive *archive,
    uint16_t sound_index,
    WolfAudioAdlibSound *sound,
    char *error_message,
    size_t error_message_size)
{
    uint16_t chunk_index;
    uint32_t chunk_start;
    uint32_t chunk_end;
    size_t chunk_size;
    const uint8_t *chunk;
    uint32_t data_length;

    if (error_message != NULL && error_message_size > 0u) {
        error_message[0] = '\0';
    }
    if (archive == NULL || archive->audio_data == NULL || sound == NULL ||
        sound_index >= WOLF_AUDIO_ADLIB_SOUND_COUNT) {
        set_error(error_message, error_message_size, "Invalid AdLib sound archive or sound index.");
        return false;
    }

    chunk_index = (uint16_t)(WOLF_AUDIO_ADLIB_START_CHUNK + sound_index);
    chunk_start = archive->offsets[chunk_index];
    chunk_end = archive->offsets[chunk_index + 1u];
    if (chunk_end < chunk_start) {
        set_error(error_message, error_message_size, "The selected AdLib chunk has invalid offsets.");
        return false;
    }
    chunk_size = (size_t)(chunk_end - chunk_start);
    if (chunk_size < WOLF_AUDIO_ADLIB_DATA_OFFSET) {
        set_error(error_message, error_message_size, "The selected AdLib chunk is shorter than its sound header.");
        return false;
    }

    chunk = archive->audio_data + chunk_start;
    data_length = read_u32_le(chunk);
    if (data_length == 0u || (size_t)data_length > chunk_size - WOLF_AUDIO_ADLIB_DATA_OFFSET) {
        set_error(error_message, error_message_size, "The selected AdLib chunk has an invalid frequency-data length.");
        return false;
    }

    memcpy(sound->instrument, chunk + WOLF_AUDIO_ADLIB_COMMON_HEADER_SIZE,
        WOLF_AUDIO_ADLIB_INSTRUMENT_SIZE);
    sound->block = chunk[WOLF_AUDIO_ADLIB_COMMON_HEADER_SIZE + WOLF_AUDIO_ADLIB_INSTRUMENT_SIZE];
    sound->frequency_data = chunk + WOLF_AUDIO_ADLIB_DATA_OFFSET;
    sound->frequency_data_length = data_length;
    return true;
}

void wolf_audio_adlib_archive_destroy(WolfAudioAdlibArchive *archive)
{
    if (archive == NULL) {
        return;
    }
    free(archive->audio_data);
    memset(archive, 0, sizeof(*archive));
}
