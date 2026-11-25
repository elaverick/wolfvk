#ifndef WOLFVK_GAME_WORLD_H
#define WOLFVK_GAME_WORLD_H

#include "wolfvk/level.h"
#include "wolfvk/player_state.h"
#include "wolfvk/vulkan_renderer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WOLF_WORLD_MAX_DOORS 64u
#define WOLF_WORLD_DOOR_OPEN_POSITION 0xffffu
#define WOLF_WORLD_DOOR_TICKS_PER_SECOND 70.0f
#define WOLF_WORLD_DOOR_SPEED_PER_TIC 1024.0f
#define WOLF_WORLD_DOOR_OPEN_TICKS 300.0f
#define WOLF_WORLD_PUSHWALL_TILE_TICKS 128.0f

typedef enum WolfDoorAction {
    WOLF_DOOR_CLOSED = 0,
    WOLF_DOOR_OPENING,
    WOLF_DOOR_OPEN,
    WOLF_DOOR_CLOSING
} WolfDoorAction;

typedef struct WolfDoor {
    uint8_t tile_x;
    uint8_t tile_y;
    bool vertical;
    uint8_t lock_type;
    WolfDoorAction action;
    uint32_t position;
    float open_ticks;
} WolfDoor;

typedef struct WolfPushWall {
    bool active;
    uint8_t source_x;
    uint8_t source_y;
    uint16_t wall_tile;
    int8_t direction_x;
    int8_t direction_y;
    float elapsed_ticks;
    bool second_tile_available;
} WolfPushWall;

typedef struct WolfWorldState {
    WolfLevel level;
    WolfDoor doors[WOLF_WORLD_MAX_DOORS];
    uint8_t door_at[WOLF_LEVEL_PLANE_WORDS];
    uint16_t door_count;
    uint16_t texture_page_count;
    /* Health, ammunition, keys and score; door locks check these keys. */
    WolfPlayerState player;
    bool has_secret_walls;
    WolfPushWall pushwall;
    uint32_t geometry_revision;
} WolfWorldState;

/* Copy a decoded WL6 level and initialise its original door and secret-wall markers. */
bool wolf_world_state_init(
    WolfWorldState *world,
    const WolfLevel *level,
    uint16_t texture_page_count);

/* True if a valid secret-wall marker is in the immediately facing map cell. */
bool wolf_world_state_has_secret_wall_ahead(
    const WolfWorldState *world,
    float player_world_x,
    float player_world_z,
    float player_yaw_radians);

/* Activate the original Use action in the cardinal direction nearest the player's yaw. */
bool wolf_world_state_use(
    WolfWorldState *world,
    float player_world_x,
    float player_world_z,
    float player_yaw_radians);

/* Advance doors and pushwalls using the original 70 Hz timing model. */
bool wolf_world_state_update(
    WolfWorldState *world,
    float delta_seconds,
    float player_world_x,
    float player_world_z,
    bool *geometry_changed);

/* Apply map-cell and moving-wall collision using the player's circular footprint. */
bool wolf_world_state_position_overlaps_solid(
    const WolfWorldState *world,
    float player_world_x,
    float player_world_z,
    float player_radius_tiles);

/* Collect an original red or yellow key when the player enters its map cell. */
bool wolf_world_state_collect_key(
    WolfWorldState *world,
    float player_world_x,
    float player_world_z);

/* Dynamic geometry reserves one two-sided quad per door and four for a map with pushwalls. */
uint32_t wolf_world_state_dynamic_quad_count(const WolfWorldState *world);

/* Rebuild the fixed-size animated door and pushwall section of the scene mesh. */
bool wolf_world_state_build_dynamic_quads(
    const WolfWorldState *world,
    VulkanQuad *quads,
    uint32_t quad_count);

/* True while a door panel or secret wall is visibly moving. */
bool wolf_world_state_has_animated_geometry(const WolfWorldState *world);

/* Incremented when a secret wall leaves or finally occupies a map cell. */
uint32_t wolf_world_state_geometry_revision(const WolfWorldState *world);

#endif
