#ifndef WOLFVK_AUDIO_EVENTS_H
#define WOLFVK_AUDIO_EVENTS_H

#include "wolfvk/game_world.h"

#include <stddef.h>
#include <stdint.h>

/* World transitions that have matching original WL6 digitised effects. */
typedef enum WolfAudioWorldEvent {
    WOLF_AUDIO_WORLD_DOOR_OPENED = 0,
    WOLF_AUDIO_WORLD_DOOR_CLOSED,
    WOLF_AUDIO_WORLD_SECRET_WALL_STARTED
} WolfAudioWorldEvent;

/*
 * Small audio-only snapshot: avoid copying the full map and player state every
 * frame just to detect the transitions that should emit world sound effects.
 */
typedef struct WolfAudioWorldSnapshot {
    WolfDoorAction door_actions[WOLF_WORLD_MAX_DOORS];
    uint16_t door_count;
    bool pushwall_active;
} WolfAudioWorldSnapshot;

/* Capture the current door actions and secret-wall movement state. */
void wolf_audio_world_snapshot_capture(
    const WolfWorldState *world,
    WolfAudioWorldSnapshot *snapshot);

/* Return the events introduced between two snapshots, up to output_capacity. */
size_t wolf_audio_world_snapshot_diff(
    const WolfAudioWorldSnapshot *previous,
    const WolfAudioWorldSnapshot *current,
    WolfAudioWorldEvent *events,
    size_t output_capacity);

/* Map an event to the exact named sound exported from a retail WL6 DigiMap. */
const char *wolf_audio_world_event_sound(WolfAudioWorldEvent event);

#endif
