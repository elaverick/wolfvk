#include "audio_archive.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define WOLF_VSWAP_PREFIX_SIZE 6u
#define WOLF_VSWAP_PAGE_ENTRY_SIZE 6u
#define WOLF_VSWAP_MAX_DIGI_SAMPLES 256u

static const WolfAudioDigiSound digi_sound_map[] = {
    {"HALTSND", 0u},
    {"DOGBARKSND", 1u},
    {"CLOSEDOORSND", 2u},
    {"OPENDOORSND", 3u},
    {"ATKMACHINEGUNSND", 4u},
    {"ATKPISTOLSND", 5u},
    {"ATKGATLINGSND", 6u},
    {"SCHUTZADSND", 7u},
    {"GUTENTAGSND", 8u},
    {"MUTTISND", 9u},
    {"BOSSFIRESND", 10u},
    {"SSFIRESND", 11u},
    {"DEATHSCREAM1SND", 12u},
    {"DEATHSCREAM2SND", 13u},
    {"DEATHSCREAM3SND", 13u},
    {"TAKEDAMAGESND", 14u},
    {"PUSHWALLSND", 15u},
    {"DOGDEATHSND", 16u},
    {"AHHHGSND", 17u},
    {"DIESND", 18u},
    {"EVASND", 19u},
    {"LEBENSND", 20u},
    {"NAZIFIRESND", 21u},
    {"SLURPIESND", 22u},
    {"TOT_HUNDSND", 23u},
    {"MEINGOTTSND", 24u},
    {"SCHABBSHASND", 25u},
    {"HITLERHASND", 26u},
    {"SPIONSND", 27u},
    {"NEINSOVASSND", 28u},
    {"DOGATTACKSND", 29u},
    {"LEVELDONESND", 30u},
    {"MECHSTEPSND", 31u},
    {"YEAHSND", 32u},
    {"SCHEISTSND", 33u},
    {"DEATHSCREAM4SND", 34u},
    {"DEATHSCREAM5SND", 35u},
    {"DONNERSND", 36u},
    {"EINESND", 37u},
    {"ERLAUBENSND", 38u},
    {"DEATHSCREAM6SND", 39u},
    {"DEATHSCREAM7SND", 40u},
    {"DEATHSCREAM8SND", 41u},
    {"DEATHSCREAM9SND", 42u},
    {"KEINSND", 43u},
    {"MEINSND", 44u},
    {"ROSESND", 45u}
};

/* Decode little-endian VSWAP words without alignment assumptions. */
static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

/* Decode little-endian VSWAP page offsets without alignment assumptions. */
static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

/* Keep parsing failures useful to the command-line caller and automated tests. */
static void set_error(char *error_message, size_t error_message_size, const char *message)
{
    if (error_message != NULL && error_message_size > 0u) {
        (void)snprintf(error_message, error_message_size, "%s", message);
    }
}

/* Read the complete archive before interpreting any offsets into it. */
static bool read_entire_file(
    const char *path,
    uint8_t **file_data,
    size_t *file_size,
    char *error_message,
    size_t error_message_size)
{
    FILE *file;
    long file_length;
    uint8_t *buffer;
    size_t bytes_read;
    int close_result;

#if defined(_MSC_VER)
    file = NULL;
    if (fopen_s(&file, path, "rb") != 0) {
        file = NULL;
    }
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL) {
        set_error(error_message, error_message_size, "Could not open the VSWAP file.");
        return false;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        set_error(error_message, error_message_size, "Could not seek to the end of the VSWAP file.");
        return false;
    }
    file_length = ftell(file);
    if (file_length < 0 || fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        set_error(error_message, error_message_size, "Could not determine the VSWAP file size.");
        return false;
    }

    buffer = malloc(file_length > 0 ? (size_t)file_length : 1u);
    if (buffer == NULL) {
        (void)fclose(file);
        set_error(error_message, error_message_size, "Could not allocate memory for VSWAP data.");
        return false;
    }

    bytes_read = fread(buffer, 1u, (size_t)file_length, file);
    close_result = fclose(file);
    if (bytes_read != (size_t)file_length || close_result != 0) {
        free(buffer);
        set_error(error_message, error_message_size, "Could not read the complete VSWAP file.");
        return false;
    }

    *file_data = buffer;
    *file_size = (size_t)file_length;
    return true;
}

/* Append an owned decoded sample while keeping the archive's count in sync. */
static bool append_sample(
    WolfAudioArchive *archive,
    const uint8_t *file_data,
    size_t file_size,
    const uint8_t *page_offsets,
    const uint8_t *page_lengths,
    uint16_t first_sound_page,
    uint16_t relative_start_page,
    uint16_t sample_length)
{
    WolfAudioPcmSample *expanded_samples;
    WolfAudioPcmSample sample = {0};
    size_t remaining = sample_length;
    size_t destination_offset = 0u;
    size_t page_count;
    uint8_t *sample_data;

    if (sample_length == 0u) {
        return false;
    }

    page_count = ((size_t)sample_length + WOLF_AUDIO_DIGI_PAGE_SIZE - 1u) /
        WOLF_AUDIO_DIGI_PAGE_SIZE;
    sample_data = malloc(sample_length);
    if (sample_data == NULL) {
        return false;
    }

    for (size_t page_index = 0u; page_index < page_count; ++page_index) {
        uint32_t absolute_page = (uint32_t)first_sound_page +
            (uint32_t)relative_start_page + (uint32_t)page_index;
        uint32_t page_offset;
        uint16_t page_length;
        size_t bytes_to_copy = remaining < WOLF_AUDIO_DIGI_PAGE_SIZE
            ? remaining
            : WOLF_AUDIO_DIGI_PAGE_SIZE;

        if (absolute_page > UINT16_MAX) {
            free(sample_data);
            return false;
        }

        page_offset = read_u32_le(page_offsets + (size_t)absolute_page * 4u);
        page_length = read_u16_le(page_lengths + (size_t)absolute_page * 2u);
        if (page_offset == 0u || page_offset > file_size ||
            (size_t)page_length > file_size - (size_t)page_offset ||
            (size_t)page_length < bytes_to_copy) {
            free(sample_data);
            return false;
        }

        memcpy(sample_data + destination_offset, file_data + page_offset, bytes_to_copy);
        destination_offset += bytes_to_copy;
        remaining -= bytes_to_copy;
    }

    sample.data = sample_data;
    sample.length = sample_length;
    expanded_samples = realloc(
        archive->samples,
        ((size_t)archive->sample_count + 1u) * sizeof(*archive->samples));
    if (expanded_samples == NULL) {
        free(sample_data);
        return false;
    }

    archive->samples = expanded_samples;
    archive->samples[archive->sample_count] = sample;
    ++archive->sample_count;
    return true;
}

/* Decode the final VSWAP DigiList and concatenate its sample page runs. */
bool wolf_audio_archive_decode(
    const uint8_t *file_data,
    size_t file_size,
    WolfAudioArchive *archive,
    char *error_message,
    size_t error_message_size)
{
    uint16_t chunk_count;
    uint16_t first_sprite_page;
    uint16_t first_sound_page;
    size_t header_size;
    size_t page_length_table_offset;
    size_t sound_page_count;
    uint16_t list_page_index;
    uint32_t list_page_offset;
    uint16_t list_page_length;
    const uint8_t *page_offsets;
    const uint8_t *page_lengths;
    const uint8_t *digi_list;
    size_t list_cursor = 0u;
    size_t consumed_pages = 0u;

    if (error_message != NULL && error_message_size > 0u) {
        error_message[0] = '\0';
    }
    if (file_data == NULL || archive == NULL ||
        archive->samples != NULL || archive->sample_count != 0u ||
        file_size < WOLF_VSWAP_PREFIX_SIZE) {
        set_error(error_message, error_message_size, "Invalid or non-empty audio archive arguments.");
        return false;
    }

    chunk_count = read_u16_le(file_data);
    first_sprite_page = read_u16_le(file_data + 2u);
    first_sound_page = read_u16_le(file_data + 4u);
    if (chunk_count < 3u || first_sprite_page >= first_sound_page ||
        first_sound_page >= chunk_count - 1u) {
        set_error(error_message, error_message_size, "VSWAP sound page boundaries are invalid.");
        return false;
    }

    header_size = WOLF_VSWAP_PREFIX_SIZE + (size_t)chunk_count * WOLF_VSWAP_PAGE_ENTRY_SIZE;
    if (header_size > file_size) {
        set_error(error_message, error_message_size, "VSWAP page tables are truncated.");
        return false;
    }

    page_length_table_offset = WOLF_VSWAP_PREFIX_SIZE + (size_t)chunk_count * 4u;
    page_offsets = file_data + WOLF_VSWAP_PREFIX_SIZE;
    page_lengths = file_data + page_length_table_offset;
    sound_page_count = (size_t)chunk_count - 1u - first_sound_page;
    list_page_index = (uint16_t)(chunk_count - 1u);

    list_page_offset = read_u32_le(page_offsets + (size_t)list_page_index * 4u);
    list_page_length = read_u16_le(page_lengths + (size_t)list_page_index * 2u);
    if (list_page_offset < header_size || list_page_offset > file_size ||
        (size_t)list_page_length > file_size - (size_t)list_page_offset ||
        list_page_length < 4u) {
        set_error(error_message, error_message_size, "The VSWAP digitised-sound index page is invalid.");
        return false;
    }
    digi_list = file_data + list_page_offset;

    /*
     * The original Page Manager counts sounds by accumulating the number of
     * 4096-byte pages consumed by each length entry. The page number in each
     * record is relative to PMSoundStart (VSWAP's first sound page).
     */
    while (consumed_pages < sound_page_count) {
        uint16_t relative_start_page;
        uint16_t sample_length;
        size_t sample_pages;

        if (list_cursor > (size_t)list_page_length ||
            (size_t)list_page_length - list_cursor < 4u ||
            archive->sample_count >= WOLF_VSWAP_MAX_DIGI_SAMPLES) {
            set_error(error_message, error_message_size, "The VSWAP digitised-sound index is truncated.");
            wolf_audio_archive_destroy(archive);
            return false;
        }

        relative_start_page = read_u16_le(digi_list + list_cursor);
        sample_length = read_u16_le(digi_list + list_cursor + 2u);
        list_cursor += 4u;
        if (sample_length == 0u) {
            set_error(error_message, error_message_size, "The VSWAP digitised-sound index contains an empty sample.");
            wolf_audio_archive_destroy(archive);
            return false;
        }

        sample_pages = ((size_t)sample_length + WOLF_AUDIO_DIGI_PAGE_SIZE - 1u) /
            WOLF_AUDIO_DIGI_PAGE_SIZE;
        if ((size_t)relative_start_page + sample_pages > sound_page_count ||
            sample_pages > sound_page_count - consumed_pages ||
            !append_sample(
                archive, file_data, file_size, page_offsets, page_lengths,
                first_sound_page, relative_start_page, sample_length)) {
            set_error(error_message, error_message_size, "A VSWAP digitised sample points outside its sound pages.");
            wolf_audio_archive_destroy(archive);
            return false;
        }
        consumed_pages += sample_pages;
    }

    if (archive->sample_count == 0u) {
        set_error(error_message, error_message_size, "The VSWAP archive contains no digitised sound samples.");
        return false;
    }
    return true;
}

/* Read an entire VSWAP file and pass its bytes to the bounds-checked decoder. */
bool wolf_audio_archive_load(
    const char *path,
    WolfAudioArchive *archive,
    char *error_message,
    size_t error_message_size)
{
    uint8_t *file_data = NULL;
    size_t file_size = 0u;
    bool decoded;

    if (path == NULL || archive == NULL || archive->samples != NULL ||
        archive->sample_count != 0u) {
        set_error(error_message, error_message_size, "Invalid audio archive load arguments.");
        return false;
    }

    if (!read_entire_file(path, &file_data, &file_size, error_message, error_message_size)) {
        return false;
    }
    decoded = wolf_audio_archive_decode(
        file_data, file_size, archive, error_message, error_message_size);
    free(file_data);
    return decoded;
}

/* Release the decoded samples and reset the archive for safe reuse. */
void wolf_audio_archive_destroy(WolfAudioArchive *archive)
{
    if (archive == NULL) {
        return;
    }
    for (uint16_t index = 0u; index < archive->sample_count; ++index) {
        free(archive->samples[index].data);
    }
    free(archive->samples);
    memset(archive, 0, sizeof(*archive));
}

/* Expose the original WL6 sound names and their digitised VSWAP sample numbers. */
const WolfAudioDigiSound *wolf_audio_digi_sound_map(size_t *sound_count)
{
    if (sound_count != NULL) {
        *sound_count = sizeof(digi_sound_map) / sizeof(digi_sound_map[0]);
    }
    return digi_sound_map;
}
