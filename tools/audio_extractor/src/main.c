#include "audio_archive.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <process.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define WOLF_AUDIO_PATH_CAPACITY 1024u

static void print_usage(const char *program_name)
{
    printf(
        "Usage: %s --vswap <VSWAP.WL6> [--output <directory>] [--ffmpeg <executable>]\n"
        "\n"
        "Extracts the original digitised WL6 sound effects from the VSWAP archive\n"
        "and encodes each named DigiMap sound as an OGG Vorbis file.\n"
        "\n"
        "Options:\n"
        "  --vswap <file>       Original VSWAP.WL6 file (required)\n"
        "  --output <directory> Output directory (default: assets/sounds)\n"
        "  --ffmpeg <program>   FFmpeg executable (default: ffmpeg from PATH)\n"
        "  --help               Show this help text\n"
        "\n"
        "FFmpeg must include its libvorbis encoder. The extracted PCM is 8-bit,\n"
        "mono, 7000 Hz, matching the original Wolf3D SoundBlaster playback rate.\n",
        program_name != NULL ? program_name : "wolf_audio_extractor");
}

/* Create one directory, accepting an existing directory but not an ordinary file. */
static bool make_one_directory(const char *path)
{
    int result;

#if defined(_WIN32)
    result = _mkdir(path);
#else
    result = mkdir(path, 0755);
#endif
    if (result == 0) {
        return true;
    }
    if (errno == EEXIST) {
        struct stat path_status;
#if defined(_WIN32)
        return stat(path, &path_status) == 0 &&
            (path_status.st_mode & _S_IFMT) == _S_IFDIR;
#else
        return stat(path, &path_status) == 0 && S_ISDIR(path_status.st_mode);
#endif
    }
    return false;
}

/* Create a directory hierarchy so the default assets/sounds path works on a clean checkout. */
static bool make_directories(const char *path)
{
    char path_buffer[WOLF_AUDIO_PATH_CAPACITY];
    size_t path_length;

    if (path == NULL || path[0] == '\0') {
        return false;
    }
    path_length = strlen(path);
    if (path_length >= sizeof(path_buffer)) {
        return false;
    }
    memcpy(path_buffer, path, path_length + 1u);

    for (size_t index = 1u; index < path_length; ++index) {
        char separator = path_buffer[index];

        if (separator != '/' && separator != '\\') {
            continue;
        }
#if defined(_WIN32)
        /* A drive prefix such as C: is not a directory to create. */
        if (index == 2u && path_buffer[1] == ':') {
            continue;
        }
#endif
        path_buffer[index] = '\0';
        if (path_buffer[0] != '\0' && !make_one_directory(path_buffer)) {
            return false;
        }
        path_buffer[index] = separator;
    }

    return make_one_directory(path_buffer);
}

/* Write one integer to the RIFF stream in little-endian order. */
static bool write_u16_le(FILE *file, uint16_t value)
{
    uint8_t bytes[2] = {
        (uint8_t)(value & 0xffu),
        (uint8_t)(value >> 8)
    };
    return fwrite(bytes, 1u, sizeof(bytes), file) == sizeof(bytes);
}

/* Write one 32-bit integer to the RIFF stream in little-endian order. */
static bool write_u32_le(FILE *file, uint32_t value)
{
    uint8_t bytes[4] = {
        (uint8_t)(value & 0xffu),
        (uint8_t)((value >> 8) & 0xffu),
        (uint8_t)((value >> 16) & 0xffu),
        (uint8_t)(value >> 24)
    };
    return fwrite(bytes, 1u, sizeof(bytes), file) == sizeof(bytes);
}

/* Save the source PCM as a temporary standards-compliant unsigned 8-bit WAV file. */
static bool write_pcm_wave(const char *path, const WolfAudioPcmSample *sample)
{
    FILE *file;
    bool written;

#if defined(_MSC_VER)
    file = NULL;
    if (fopen_s(&file, path, "wb") != 0) {
        file = NULL;
    }
#else
    file = fopen(path, "wb");
#endif
    if (file == NULL) {
        return false;
    }

    written =
        fwrite("RIFF", 1u, 4u, file) == 4u &&
        write_u32_le(file, 36u + sample->length) &&
        fwrite("WAVEfmt ", 1u, 8u, file) == 8u &&
        write_u32_le(file, 16u) &&
        write_u16_le(file, 1u) &&
        write_u16_le(file, 1u) &&
        write_u32_le(file, WOLF_AUDIO_DIGI_SAMPLE_RATE) &&
        write_u32_le(file, WOLF_AUDIO_DIGI_SAMPLE_RATE) &&
        write_u16_le(file, 1u) &&
        write_u16_le(file, 8u) &&
        fwrite("data", 1u, 4u, file) == 4u &&
        write_u32_le(file, sample->length) &&
        fwrite(sample->data, 1u, sample->length, file) == sample->length;

    if (fclose(file) != 0) {
        written = false;
    }
    return written;
}

/* Run FFmpeg without a shell so file names cannot be interpreted as shell commands. */
static bool encode_ogg(
    const char *ffmpeg,
    const char *wave_path,
    const char *ogg_path)
{
#if defined(_WIN32)
    char *arguments[] = {
        (char *)ffmpeg,
        "-nostdin",
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-i",
        (char *)wave_path,
        "-vn",
        "-ac",
        "1",
        "-ar",
        "7000",
        "-c:a",
        "libvorbis",
        "-q:a",
        "5",
        (char *)ogg_path,
        NULL
    };
    return _spawnvp(_P_WAIT, ffmpeg, (const char * const *)arguments) == 0;
#else
    char *arguments[] = {
        (char *)ffmpeg,
        "-nostdin",
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-i",
        (char *)wave_path,
        "-vn",
        "-ac",
        "1",
        "-ar",
        "7000",
        "-c:a",
        "libvorbis",
        "-q:a",
        "5",
        (char *)ogg_path,
        NULL
    };
    pid_t child = fork();
    int child_status;

    if (child < 0) {
        return false;
    }
    if (child == 0) {
        execvp(ffmpeg, arguments);
        _exit(127);
    }
    if (waitpid(child, &child_status, 0) < 0) {
        return false;
    }
    return WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0;
#endif
}

/* Convert each source sample once, then publish every original DigiMap alias as its own OGG. */
static bool export_sounds(
    const WolfAudioArchive *archive,
    const char *output_directory,
    const char *ffmpeg)
{
    const WolfAudioDigiSound *sound_map;
    size_t sound_count = 0u;
    size_t exported_count = 0u;
    bool success = true;

    sound_map = wolf_audio_digi_sound_map(&sound_count);
    for (uint16_t sample_index = 0u; sample_index < archive->sample_count; ++sample_index) {
        char wave_path[WOLF_AUDIO_PATH_CAPACITY];
        bool sample_has_alias = false;

        if (snprintf(
                wave_path, sizeof(wave_path), "%s/sample_%02u.tmp.wav",
                output_directory, (unsigned int)sample_index) < 0 ||
            strlen(wave_path) >= sizeof(wave_path)) {
            fprintf(stderr, "The temporary WAV path is too long.\n");
            return false;
        }
        if (!write_pcm_wave(wave_path, &archive->samples[sample_index])) {
            fprintf(stderr, "Could not write temporary PCM data for sample %u.\n",
                (unsigned int)sample_index);
            return false;
        }

        for (size_t sound_index = 0u; sound_index < sound_count; ++sound_index) {
            char ogg_path[WOLF_AUDIO_PATH_CAPACITY];

            if (sound_map[sound_index].sample_index != sample_index) {
                continue;
            }
            sample_has_alias = true;
            if (snprintf(
                    ogg_path, sizeof(ogg_path), "%s/%s.ogg",
                    output_directory, sound_map[sound_index].name) < 0 ||
                strlen(ogg_path) >= sizeof(ogg_path)) {
                fprintf(stderr, "The output path for %s is too long.\n",
                    sound_map[sound_index].name);
                success = false;
                continue;
            }
            if (!encode_ogg(ffmpeg, wave_path, ogg_path)) {
                fprintf(stderr,
                    "FFmpeg could not encode %s. Check that FFmpeg and libvorbis are installed.\n",
                    sound_map[sound_index].name);
                success = false;
                continue;
            }
            printf("Wrote %s\n", ogg_path);
            ++exported_count;
        }

        if (!sample_has_alias) {
            fprintf(stderr, "No original sound name refers to digitised sample %u.\n",
                (unsigned int)sample_index);
            success = false;
        }
        (void)remove(wave_path);
    }

    printf("Exported %zu named OGG effects from %u digitised samples.\n",
        exported_count, (unsigned int)archive->sample_count);
    return success;
}

/* Validate options, decode the archive and export only named original sound effects. */
int main(int argc, char *argv[])
{
    const char *vswap_path = NULL;
    const char *output_directory = "assets/sounds";
    const char *ffmpeg = "ffmpeg";
    WolfAudioArchive archive = {0};
    char error_message[256];
    int exit_code = 1;

    for (int argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[argument], "--vswap") == 0 && argument + 1 < argc) {
            vswap_path = argv[++argument];
        } else if (strcmp(argv[argument], "--output") == 0 && argument + 1 < argc) {
            output_directory = argv[++argument];
        } else if (strcmp(argv[argument], "--ffmpeg") == 0 && argument + 1 < argc) {
            ffmpeg = argv[++argument];
        } else {
            fprintf(stderr, "Unknown or incomplete argument: %s\n", argv[argument]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (vswap_path == NULL || !make_directories(output_directory)) {
        if (vswap_path == NULL) {
            fprintf(stderr, "The --vswap option is required.\n");
        } else {
            fprintf(stderr, "Could not create output directory: %s\n", output_directory);
        }
        print_usage(argv[0]);
        return 1;
    }

    if (!wolf_audio_archive_load(
            vswap_path, &archive, error_message, sizeof(error_message))) {
        fprintf(stderr, "Could not extract WL6 digitised sounds: %s\n", error_message);
        goto cleanup;
    }

    printf("Decoded %u original digitised samples from %s.\n",
        (unsigned int)archive.sample_count, vswap_path);
    exit_code = export_sounds(&archive, output_directory, ffmpeg) ? 0 : 1;

cleanup:
    wolf_audio_archive_destroy(&archive);
    return exit_code;
}
