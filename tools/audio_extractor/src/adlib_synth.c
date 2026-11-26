#include "adlib_synth.h"

#include <opal/opal.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define WOLF_AUDIO_ADLIB_FRAMES_PER_TICK (WOLF_AUDIO_ADLIB_SAMPLE_RATE / WOLF_AUDIO_ADLIB_TICK_RATE)
#define WOLF_AUDIO_ADLIB_RELEASE_FRAMES (WOLF_AUDIO_ADLIB_SAMPLE_RATE / 10u)
#define WOLF_AUDIO_ADLIB_MAX_DURATION_SECONDS 30u
#define WOLF_AUDIO_ADLIB_MAX_FREQUENCY_BYTES (WOLF_AUDIO_ADLIB_TICK_RATE * WOLF_AUDIO_ADLIB_MAX_DURATION_SECONDS)

/* Generate one tick of OPL output, averaging the two chip channels to mono. */
static void render_frames(
    Opal *chip,
    int16_t *destination,
    uint32_t frame_count)
{
    for (uint32_t frame = 0u; frame < frame_count; ++frame) {
        int16_t left = 0;
        int16_t right = 0;
        int32_t mono;

        opalSample(chip, &left, &right);
        mono = ((int32_t)left + (int32_t)right) / 4;
        if (mono > INT16_MAX) {
            mono = INT16_MAX;
        } else if (mono < INT16_MIN) {
            mono = INT16_MIN;
        }
        destination[frame] = (int16_t)mono;
    }
}

bool wolf_audio_adlib_render(
    const WolfAudioAdlibSound *sound,
    WolfAudioPcm16 *rendered)
{
    Opal chip;
    uint32_t frame_count;
    size_t total_frames;
    size_t output_cursor = 0u;
    uint8_t block_register;

    if (rendered == NULL) {
        return false;
    }
    memset(rendered, 0, sizeof(*rendered));
    if (sound == NULL || sound->frequency_data == NULL ||
        sound->frequency_data_length == 0u ||
        sound->frequency_data_length > WOLF_AUDIO_ADLIB_MAX_FREQUENCY_BYTES) {
        return false;
    }

    total_frames = (size_t)sound->frequency_data_length *
        (size_t)WOLF_AUDIO_ADLIB_FRAMES_PER_TICK +
        (size_t)WOLF_AUDIO_ADLIB_RELEASE_FRAMES;
    if (total_frames > UINT32_MAX || total_frames > SIZE_MAX / sizeof(int16_t)) {
        return false;
    }
    frame_count = (uint32_t)total_frames;
    rendered->samples = calloc(total_frames, sizeof(*rendered->samples));
    if (rendered->samples == NULL) {
        return false;
    }

    /*
     * Wolf3D uses the first AdLib channel for effects. The operator registers
     * are the original MOD slot 0 and carrier slot 3; nConn is deliberately
     * zeroed by the original SDL_AlSetFXInst implementation.
     */
    opalInit(&chip, (int)WOLF_AUDIO_ADLIB_SAMPLE_RATE);
    opalWriteReg(&chip, 0x20u, sound->instrument[0]);
    opalWriteReg(&chip, 0x23u, sound->instrument[1]);
    opalWriteReg(&chip, 0x40u, sound->instrument[2]);
    opalWriteReg(&chip, 0x43u, sound->instrument[3]);
    opalWriteReg(&chip, 0x60u, sound->instrument[4]);
    opalWriteReg(&chip, 0x63u, sound->instrument[5]);
    opalWriteReg(&chip, 0x80u, sound->instrument[6]);
    opalWriteReg(&chip, 0x83u, sound->instrument[7]);
    opalWriteReg(&chip, 0xe0u, sound->instrument[8]);
    opalWriteReg(&chip, 0xe3u, sound->instrument[9]);
    opalWriteReg(&chip, 0xc0u, 0u);
    block_register = (uint8_t)(((sound->block & 7u) << 2u) | 0x20u);

    for (uint32_t tick = 0u; tick < sound->frequency_data_length; ++tick) {
        uint8_t frequency = sound->frequency_data[tick];

        if (frequency == 0u) {
            opalWriteReg(&chip, 0xb0u, 0u);
        } else {
            opalWriteReg(&chip, 0xa0u, frequency);
            opalWriteReg(&chip, 0xb0u, block_register);
        }

        render_frames(
            &chip,
            rendered->samples + output_cursor,
            WOLF_AUDIO_ADLIB_FRAMES_PER_TICK);
        output_cursor += WOLF_AUDIO_ADLIB_FRAMES_PER_TICK;
    }

    /* Match the original key-off and preserve the OPL envelope's release tail. */
    opalWriteReg(&chip, 0xb0u, 0u);
    render_frames(
        &chip,
        rendered->samples + output_cursor,
        WOLF_AUDIO_ADLIB_RELEASE_FRAMES);

    rendered->frame_count = frame_count;
    rendered->sample_rate = WOLF_AUDIO_ADLIB_SAMPLE_RATE;
    return true;
}

void wolf_audio_pcm16_destroy(WolfAudioPcm16 *rendered)
{
    if (rendered == NULL) {
        return;
    }
    free(rendered->samples);
    memset(rendered, 0, sizeof(*rendered));
}
