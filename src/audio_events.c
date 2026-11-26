#include "wolfvk/audio_events.h"

#include <string.h>

static WolfAudioVector3 world_tile_centre(
    uint8_t tile_x,
    uint8_t tile_y,
    uint16_t map_width,
    uint16_t map_height)
{
    WolfAudioVector3 position = {0.0f, WOLF_WORLD_EYE_HEIGHT, 0.0f};

    if (map_width == 0u || map_height == 0u) {
        return position;
    }

    position.x = ((float)tile_x + 0.5f - (float)map_width * 0.5f) *
        WOLF_WORLD_TILE_SIZE;
    position.z = ((float)tile_y + 0.5f - (float)map_height * 0.5f) *
        WOLF_WORLD_TILE_SIZE;
    return position;
}

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
    snapshot->map_width = world->level.width;
    snapshot->map_height = world->level.height;
    for (uint16_t index = 0u; index < snapshot->door_count; ++index) {
        snapshot->door_actions[index] = world->doors[index].action;
        snapshot->door_tile_x[index] = world->doors[index].tile_x;
        snapshot->door_tile_y[index] = world->doors[index].tile_y;
    }
    snapshot->pushwall_active = world->pushwall.active;
    snapshot->pushwall_tile_x = world->pushwall.source_x;
    snapshot->pushwall_tile_y = world->pushwall.source_y;
}

size_t wolf_audio_world_snapshot_diff(
    const WolfAudioWorldSnapshot *previous,
    const WolfAudioWorldSnapshot *current,
    WolfAudioWorldEventInfo *events,
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
                events[event_count].type = WOLF_AUDIO_WORLD_DOOR_OPENED;
                events[event_count].source_position = world_tile_centre(
                    current->door_tile_x[index], current->door_tile_y[index],
                    current->map_width, current->map_height);
                ++event_count;
            }
        } else if (current_action == WOLF_DOOR_CLOSING) {
            if (event_count < output_capacity) {
                events[event_count].type = WOLF_AUDIO_WORLD_DOOR_CLOSED;
                events[event_count].source_position = world_tile_centre(
                    current->door_tile_x[index], current->door_tile_y[index],
                    current->map_width, current->map_height);
                ++event_count;
            }
        }
    }

    if (!previous->pushwall_active && current->pushwall_active &&
        event_count < output_capacity) {
        events[event_count].type = WOLF_AUDIO_WORLD_SECRET_WALL_STARTED;
        events[event_count].source_position = world_tile_centre(
            current->pushwall_tile_x, current->pushwall_tile_y,
            current->map_width, current->map_height);
        ++event_count;
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
