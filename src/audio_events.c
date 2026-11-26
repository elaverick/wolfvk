#include "wolfvk/audio_events.h"

#include <string.h>

void wolf_audio_world_snapshot_capture(
    const WolfWorldState *world,
    WolfAudioWorldSnapshot *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    if (world == NULL) {
        return;
    }

    snapshot->door_count = world->door_count < WOLF_WORLD_MAX_DOORS
        ? world->door_count
        : WOLF_WORLD_MAX_DOORS;
    for (uint16_t index = 0u; index < snapshot->door_count; ++index) {
        snapshot->door_actions[index] = world->doors[index].action;
    }
    snapshot->pushwall_active = world->pushwall.active;
}

size_t wolf_audio_world_snapshot_diff(
    const WolfAudioWorldSnapshot *previous,
    const WolfAudioWorldSnapshot *current,
    WolfAudioWorldEvent *events,
    size_t output_capacity)
{
    size_t event_count = 0u;
    uint16_t comparable_door_count;

    if (previous == NULL || current == NULL || events == NULL || output_capacity == 0u) {
        return 0u;
    }

    comparable_door_count = previous->door_count < current->door_count
        ? previous->door_count
        : current->door_count;
    if (comparable_door_count > WOLF_WORLD_MAX_DOORS) {
        comparable_door_count = WOLF_WORLD_MAX_DOORS;
    }

    for (uint16_t index = 0u; index < comparable_door_count; ++index) {
        WolfDoorAction previous_action = previous->door_actions[index];
        WolfDoorAction current_action = current->door_actions[index];

        if (previous_action == current_action) {
            continue;
        }

        if (current_action == WOLF_DOOR_OPENING) {
            if (event_count < output_capacity) {
                events[event_count++] = WOLF_AUDIO_WORLD_DOOR_OPENED;
            }
        } else if (current_action == WOLF_DOOR_CLOSING) {
            if (event_count < output_capacity) {
                events[event_count++] = WOLF_AUDIO_WORLD_DOOR_CLOSED;
            }
        }
    }

    if (!previous->pushwall_active && current->pushwall_active &&
        event_count < output_capacity) {
        events[event_count++] = WOLF_AUDIO_WORLD_SECRET_WALL_STARTED;
    }

    return event_count;
}

const char *wolf_audio_world_event_sound(WolfAudioWorldEvent event)
{
    switch (event) {
    case WOLF_AUDIO_WORLD_DOOR_OPENED:
        return "OPENDOORSND";

    case WOLF_AUDIO_WORLD_DOOR_CLOSED:
        return "CLOSEDOORSND";

    case WOLF_AUDIO_WORLD_SECRET_WALL_STARTED:
        return "PUSHWALLSND";

    default:
        return NULL;
    }
}
