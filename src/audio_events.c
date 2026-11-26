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

const char *wolf_audio_bonus_sound_name(WolfBonusType bonus)
{
    switch (bonus) {
    case WOLF_BONUS_GOLD_KEY:
    case WOLF_BONUS_SILVER_KEY:
        return "GETKEYSND";

    case WOLF_BONUS_DOG_FOOD:
    case WOLF_BONUS_FOOD:
        return "HEALTH1SND";

    case WOLF_BONUS_FIRST_AID:
        return "HEALTH2SND";

    case WOLF_BONUS_GIBS:
        return "SLURPIESND";

    case WOLF_BONUS_CLIP:
    case WOLF_BONUS_USED_CLIP:
        return "GETAMMOSND";

    case WOLF_BONUS_MACHINEGUN:
        return "GETMACHINESND";

    case WOLF_BONUS_CHAINGUN:
        return "GETGATLINGSND";

    case WOLF_BONUS_CROSS:
        return "BONUS1SND";

    case WOLF_BONUS_CHALICE:
        return "BONUS2SND";

    case WOLF_BONUS_BIBLE:
        return "BONUS3SND";

    case WOLF_BONUS_CROWN:
        return "BONUS4SND";

    case WOLF_BONUS_EXTRA_LIFE:
        return "BONUS1UPSND";

    case WOLF_BONUS_NONE:
    default:
        return NULL;
    }
}
