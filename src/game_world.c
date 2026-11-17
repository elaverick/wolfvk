#include "wolfvk/game_world.h"

#include "wolfvk/level_objects.h"
#include "wolfvk/wall_textures.h"
#include "wolfvk/world.h"

#include <math.h>
#include <string.h>

#define WOLF_DOOR_TEXTURE_PAGE_COUNT 8u
#define WOLF_INFO_KEY1 43u
#define WOLF_INFO_KEY2 44u
#define WOLF_TWO_PI 6.28318530717958647692f
#define WOLF_PI_OVER_FOUR 0.78539816339744830962f
#define WOLF_HALF_PI 1.57079632679489661923f

static size_t tile_index(uint16_t x, uint16_t y)
{
    return (size_t)y * WOLF_LEVEL_PLANE_WIDTH + x;
}

static float map_coordinate(float world_coordinate, uint16_t map_extent)
{
    return world_coordinate / WOLF_WORLD_TILE_SIZE + (float)map_extent * 0.5f;
}

static bool position_overlaps_cell(
    float map_x,
    float map_y,
    float radius,
    int tile_x,
    int tile_y)
{
    float closest_x = fmaxf((float)tile_x, fminf(map_x, (float)tile_x + 1.0f));
    float closest_y = fmaxf((float)tile_y, fminf(map_y, (float)tile_y + 1.0f));
    float distance_x = map_x - closest_x;
    float distance_y = map_y - closest_y;

    return distance_x * distance_x + distance_y * distance_y < radius * radius;
}

static int door_index_at(const WolfWorldState *world, int tile_x, int tile_y)
{
    uint8_t door_index;

    if (tile_x < 0 || tile_y < 0 ||
        tile_x >= world->level.width || tile_y >= world->level.height) {
        return -1;
    }

    door_index = world->door_at[tile_index((uint16_t)tile_x, (uint16_t)tile_y)];
    return door_index == UINT8_MAX ? -1 : (int)door_index;
}

/* Match the original map values: even IDs are vertical, odd IDs horizontal. */
static void decode_door_tile(uint16_t tile, bool *vertical, uint8_t *lock_type)
{
    *vertical = (tile & 1u) == 0u;
    *lock_type = (uint8_t)((tile - (*vertical ? 90u : 91u)) / 2u);
}

/* The original actorat grid marks a door as solid until it has reached its open state. */
static bool map_cell_is_solid(
    const WolfWorldState *world,
    int tile_x,
    int tile_y)
{
    uint16_t tile;
    int door_index;

    if (tile_x < 0 || tile_y < 0 ||
        tile_x >= world->level.width || tile_y >= world->level.height) {
        return true;
    }

    tile = world->level.wall_tiles[tile_index((uint16_t)tile_x, (uint16_t)tile_y)];
    if (tile == 0u) {
        return false;
    }

    if (wolf_level_is_door_tile(tile)) {
        door_index = door_index_at(world, tile_x, tile_y);
        return door_index < 0 ||
            world->doors[door_index].action != WOLF_DOOR_OPEN;
    }

    return true;
}

/* A moving pushwall occupies its current map cell and reserves the next one, as actorat did. */
static bool pushwall_occupies_cell(
    const WolfWorldState *world,
    int tile_x,
    int tile_y)
{
    int current_x;
    int current_y;

    if (!world->pushwall.active) {
        return false;
    }

    current_x = (int)world->pushwall.source_x +
        world->pushwall.direction_x * (int)floorf(
            world->pushwall.elapsed_ticks / WOLF_WORLD_PUSHWALL_TILE_TICKS);
    current_y = (int)world->pushwall.source_y +
        world->pushwall.direction_y * (int)floorf(
            world->pushwall.elapsed_ticks / WOLF_WORLD_PUSHWALL_TILE_TICKS);

    return (tile_x == current_x && tile_y == current_y) ||
        (tile_x == current_x + world->pushwall.direction_x &&
         tile_y == current_y + world->pushwall.direction_y);
}

/* The original four key statics occupy info-plane markers 43 and 44 in WL6. */
static void collect_key_at_tile(WolfWorldState *world, int tile_x, int tile_y)
{
    size_t index;
    uint16_t marker;

    if (tile_x < 0 || tile_y < 0 ||
        tile_x >= world->level.width || tile_y >= world->level.height) {
        return;
    }

    index = tile_index((uint16_t)tile_x, (uint16_t)tile_y);
    marker = world->level.object_tiles[index];
    if (marker == WOLF_INFO_KEY1) {
        world->key_mask |= 1u;
        world->level.object_tiles[index] = 0u;
    } else if (marker == WOLF_INFO_KEY2) {
        world->key_mask |= 2u;
        world->level.object_tiles[index] = 0u;
    }
}

/* Refuse a closure when the player's original collision radius still overlaps the doorway. */
static bool player_blocks_door(
    const WolfWorldState *world,
    const WolfDoor *door,
    float player_world_x,
    float player_world_z)
{
    float map_x = map_coordinate(player_world_x, world->level.width);
    float map_y = map_coordinate(player_world_z, world->level.height);

    return position_overlaps_cell(
        map_x, map_y, 0.34375f, door->tile_x, door->tile_y);
}

/* CloseDoor leaves a doorway open if the player is standing in its opening. */
static bool request_door_close(
    const WolfWorldState *world,
    WolfDoor *door,
    float player_world_x,
    float player_world_z)
{
    if (player_blocks_door(world, door, player_world_x, player_world_z)) {
        return false;
    }

    door->action = WOLF_DOOR_CLOSING;
    return true;
}

/* SpawnDoor's 90..101 mapping and original SetupWalls layout are retained. */
bool wolf_world_state_init(
    WolfWorldState *world,
    const WolfLevel *level,
    uint16_t texture_page_count)
{
    if (world == NULL || level == NULL ||
        level->width == 0u || level->height == 0u ||
        level->width > WOLF_LEVEL_PLANE_WIDTH ||
        level->height > WOLF_LEVEL_PLANE_HEIGHT) {
        return false;
    }

    {
        WolfLevel level_copy = *level;

        memset(world, 0, sizeof(*world));
        world->level = level_copy;
        level = &world->level;
    }
    world->texture_page_count = texture_page_count;
    memset(world->door_at, UINT8_MAX, sizeof(world->door_at));
    world->geometry_revision = 1u;

    for (uint16_t y = 0u; y < level->height; ++y) {
        for (uint16_t x = 0u; x < level->width; ++x) {
            size_t index = tile_index(x, y);
            uint16_t wall_tile = level->wall_tiles[index];

            if (wolf_level_is_door_tile(wall_tile)) {
                WolfDoor *door;

                if (world->door_count >= WOLF_WORLD_MAX_DOORS ||
                    texture_page_count < WOLF_DOOR_TEXTURE_PAGE_COUNT) {
                    return false;
                }

                door = &world->doors[world->door_count];
                door->tile_x = (uint8_t)x;
                door->tile_y = (uint8_t)y;
                decode_door_tile(wall_tile, &door->vertical, &door->lock_type);
                door->action = WOLF_DOOR_CLOSED;
                door->position = 0u;
                door->open_ticks = 0.0f;
                world->door_at[index] = (uint8_t)world->door_count;
                ++world->door_count;
            }

            if (wolf_level_is_secret_door_cell(level, x, y) &&
                wall_tile != 0u && !wolf_level_is_door_tile(wall_tile)) {
                uint16_t horizontal_page;
                uint16_t vertical_page;

                /*
                 * A pushwall uses the same two original wall pages as its
                 * stationary form. Validate them while the info-plane marker
                 * and its source wall cell are still available.
                 */
                if (!wolf_wall_texture_page_for_tile(
                        wall_tile, false, texture_page_count, &horizontal_page) ||
                    !wolf_wall_texture_page_for_tile(
                        wall_tile, true, texture_page_count, &vertical_page)) {
                    return false;
                }
                world->has_secret_walls = true;
            }
        }
    }

    return true;
}

/*
 * Resolve the same four-way interaction cell for the view direction used by
 * both the original Use action and the SDL camera. Camera yaw zero looks north.
 */
static bool world_state_facing_cell(
    const WolfWorldState *world,
    float player_world_x,
    float player_world_z,
    float player_yaw_radians,
    int *target_x,
    int *target_y,
    int *direction_x_out,
    int *direction_y_out)
{
    float map_x;
    float map_y;
    float yaw;
    int direction;
    int direction_x = 0;
    int direction_y = 0;
    int player_tile_x;
    int player_tile_y;

    if (world == NULL || target_x == NULL || target_y == NULL ||
        direction_x_out == NULL || direction_y_out == NULL ||
        !isfinite(player_world_x) || !isfinite(player_world_z) ||
        !isfinite(player_yaw_radians)) {
        return false;
    }

    map_x = map_coordinate(player_world_x, world->level.width);
    map_y = map_coordinate(player_world_z, world->level.height);
    player_tile_x = (int)floorf(map_x);
    player_tile_y = (int)floorf(map_y);

    yaw = fmodf(player_yaw_radians, WOLF_TWO_PI);
    if (yaw < 0.0f) {
        yaw += WOLF_TWO_PI;
    }
    direction = (int)floorf((yaw + WOLF_PI_OVER_FOUR) / WOLF_HALF_PI) & 3;
    switch (direction) {
    case 0:
        direction_y = -1;
        break;
    case 1:
        direction_x = 1;
        break;
    case 2:
        direction_y = 1;
        break;
    default:
        direction_x = -1;
        break;
    }

    *target_x = player_tile_x + direction_x;
    *target_y = player_tile_y + direction_y;
    *direction_x_out = direction_x;
    *direction_y_out = direction_y;
    return *target_x >= 0 && *target_y >= 0 &&
        *target_x < world->level.width && *target_y < world->level.height;
}

/*
 * The original player calls PushWall for as long as Use is held. Expose the
 * adjacent marker query so the main loop can retry pushwall activation while
 * held without repeatedly toggling ordinary doors.
 */
bool wolf_world_state_has_secret_wall_ahead(
    const WolfWorldState *world,
    float player_world_x,
    float player_world_z,
    float player_yaw_radians)
{
    int target_x;
    int target_y;
    int direction_x;
    int direction_y;
    size_t target_index;
    uint16_t wall_tile;
    uint16_t horizontal_page;
    uint16_t vertical_page;

    if (!world_state_facing_cell(
            world, player_world_x, player_world_z, player_yaw_radians,
            &target_x, &target_y, &direction_x, &direction_y) ||
        !wolf_level_is_secret_door_cell(&world->level, target_x, target_y)) {
        return false;
    }

    target_index = tile_index((uint16_t)target_x, (uint16_t)target_y);
    wall_tile = world->level.wall_tiles[target_index];
    return wall_tile != 0u && !wolf_level_is_door_tile(wall_tile) &&
        wolf_wall_texture_page_for_tile(
            wall_tile, false, world->texture_page_count, &horizontal_page) &&
        wolf_wall_texture_page_for_tile(
            wall_tile, true, world->texture_page_count, &vertical_page);
}

/* A single Use press operates the door or pushwall immediately in front of the player. */
bool wolf_world_state_use(
    WolfWorldState *world,
    float player_world_x,
    float player_world_z,
    float player_yaw_radians)
{
    int target_x;
    int target_y;
    int direction_x;
    int direction_y;
    size_t target_index;
    int door_index;

    if (!world_state_facing_cell(
            world, player_world_x, player_world_z, player_yaw_radians,
            &target_x, &target_y, &direction_x, &direction_y)) {
        return false;
    }

    target_index = tile_index((uint16_t)target_x, (uint16_t)target_y);
    if (wolf_level_is_secret_door_cell(&world->level, target_x, target_y)) {
        int destination_x = target_x + direction_x;
        int destination_y = target_y + direction_y;
        uint16_t wall_tile = world->level.wall_tiles[target_index];
        uint16_t horizontal_page;
        uint16_t vertical_page;

        /*
         * The info-plane marker alone does not make a usable wall. Preserve
         * the original PushWall guard: it must point at a real, textured wall
         * and the cell immediately beyond it must be empty.
         */
        if (world->pushwall.active || wall_tile == 0u ||
            wolf_level_is_door_tile(wall_tile) ||
            !wolf_wall_texture_page_for_tile(
                wall_tile, false, world->texture_page_count, &horizontal_page) ||
            !wolf_wall_texture_page_for_tile(
                wall_tile, true, world->texture_page_count, &vertical_page) ||
            map_cell_is_solid(world, destination_x, destination_y)) {
            return false;
        }

        world->pushwall.active = true;
        world->pushwall.source_x = (uint8_t)target_x;
        world->pushwall.source_y = (uint8_t)target_y;
        world->pushwall.wall_tile = wall_tile;
        world->pushwall.direction_x = (int8_t)direction_x;
        world->pushwall.direction_y = (int8_t)direction_y;
        /* The original engine initialises pwallstate to one before the first MovePWalls tick. */
        world->pushwall.elapsed_ticks = 1.0f;
        world->pushwall.second_tile_available = false;

        /* The old cell becomes floor while the moving wall supplies its collision and render shape. */
        world->level.wall_tiles[target_index] = 0u;
        world->level.object_tiles[target_index] = 0u;
        ++world->geometry_revision;
        return true;
    }

    door_index = door_index_at(world, target_x, target_y);
    if (door_index < 0) {
        return false;
    }

    {
        WolfDoor *door = &world->doors[door_index];

        if (door->lock_type >= 1u && door->lock_type <= 4u &&
            (world->key_mask & (uint8_t)(1u << (door->lock_type - 1u))) == 0u) {
            return false;
        }

        switch (door->action) {
        case WOLF_DOOR_CLOSED:
        case WOLF_DOOR_CLOSING:
            door->action = WOLF_DOOR_OPENING;
            return true;

        case WOLF_DOOR_OPEN:
            if (request_door_close(
                    world, door, player_world_x, player_world_z)) {
                door->open_ticks = 0.0f;
                return true;
            }
            return false;

        case WOLF_DOOR_OPENING:
            return request_door_close(
                world, door, player_world_x, player_world_z);

        default:
            return false;
        }
    }
}

/* Advance one door using the original tics<<10 motion and 300-tic open interval. */
static bool update_door(
    const WolfWorldState *world,
    WolfDoor *door,
    float delta_seconds,
    float player_world_x,
    float player_world_z)
{
    const float elapsed_ticks = delta_seconds * WOLF_WORLD_DOOR_TICKS_PER_SECOND;
    const float movement = elapsed_ticks * WOLF_WORLD_DOOR_SPEED_PER_TIC;
    uint32_t previous_position = door->position;
    WolfDoorAction previous_action = door->action;

    switch (door->action) {
    case WOLF_DOOR_OPEN:
        door->open_ticks += elapsed_ticks;
        if (door->open_ticks >= WOLF_WORLD_DOOR_OPEN_TICKS) {
            (void)request_door_close(
                world, door, player_world_x, player_world_z);
        }
        break;

    case WOLF_DOOR_OPENING:
        if (movement >= (float)(WOLF_WORLD_DOOR_OPEN_POSITION - door->position)) {
            door->position = WOLF_WORLD_DOOR_OPEN_POSITION;
            door->open_ticks = 0.0f;
            door->action = WOLF_DOOR_OPEN;
        } else {
            door->position += (uint32_t)movement;
        }
        break;

    case WOLF_DOOR_CLOSING:
        if (player_blocks_door(
                world, door, player_world_x, player_world_z)) {
            door->action = WOLF_DOOR_OPENING;
            break;
        }

        if (movement >= (float)door->position) {
            door->position = 0u;
            door->action = WOLF_DOOR_CLOSED;
            door->open_ticks = 0.0f;
        } else {
            door->position -= (uint32_t)movement;
        }
        break;

    case WOLF_DOOR_CLOSED:
    default:
        break;
    }

    return door->position != previous_position ||
        door->action != previous_action;
}

/* Move a secret wall at one tile per 128 original game tics, for at most two cells. */
static bool update_pushwall(WolfWorldState *world, float delta_seconds)
{
    WolfPushWall *pushwall = &world->pushwall;
    float updated_ticks;
    int destination_x;
    int destination_y;

    if (!pushwall->active || delta_seconds <= 0.0f) {
        return false;
    }

    updated_ticks = pushwall->elapsed_ticks +
        delta_seconds * WOLF_WORLD_DOOR_TICKS_PER_SECOND;

    if (updated_ticks >= WOLF_WORLD_PUSHWALL_TILE_TICKS &&
        !pushwall->second_tile_available) {
        destination_x = (int)pushwall->source_x +
            2 * pushwall->direction_x;
        destination_y = (int)pushwall->source_y +
            2 * pushwall->direction_y;

        if (map_cell_is_solid(world, destination_x, destination_y)) {
            updated_ticks = WOLF_WORLD_PUSHWALL_TILE_TICKS;
            pushwall->elapsed_ticks = updated_ticks;
            destination_x = (int)pushwall->source_x + pushwall->direction_x;
            destination_y = (int)pushwall->source_y + pushwall->direction_y;
            world->level.wall_tiles[tile_index(
                (uint16_t)destination_x, (uint16_t)destination_y)] = pushwall->wall_tile;
            pushwall->active = false;
            ++world->geometry_revision;
            return true;
        }

        pushwall->second_tile_available = true;
    }

    if (updated_ticks >= 2.0f * WOLF_WORLD_PUSHWALL_TILE_TICKS) {
        destination_x = (int)pushwall->source_x +
            2 * pushwall->direction_x;
        destination_y = (int)pushwall->source_y +
            2 * pushwall->direction_y;
        world->level.wall_tiles[tile_index(
            (uint16_t)destination_x, (uint16_t)destination_y)] = pushwall->wall_tile;
        pushwall->elapsed_ticks = 2.0f * WOLF_WORLD_PUSHWALL_TILE_TICKS;
        pushwall->active = false;
        ++world->geometry_revision;
        return true;
    }

    pushwall->elapsed_ticks = updated_ticks;
    return true;
}

/* The caller updates the world before movement so a closing door is solid that frame. */
bool wolf_world_state_update(
    WolfWorldState *world,
    float delta_seconds,
    float player_world_x,
    float player_world_z,
    bool *geometry_changed)
{
    bool changed = false;

    if (geometry_changed != NULL) {
        *geometry_changed = false;
    }

    if (world == NULL || !isfinite(delta_seconds) || delta_seconds < 0.0f ||
        !isfinite(player_world_x) || !isfinite(player_world_z)) {
        return false;
    }

    /* Cap a stalled frame to the same 0.10-second movement step used by the player controller. */
    if (delta_seconds > 0.10f) {
        delta_seconds = 0.10f;
    }

    for (uint16_t index = 0u; index < world->door_count; ++index) {
        changed = update_door(
            world, &world->doors[index], delta_seconds,
            player_world_x, player_world_z) || changed;
    }

    changed = update_pushwall(world, delta_seconds) || changed;

    if (geometry_changed != NULL) {
        *geometry_changed = changed;
    }
    return true;
}

/* Keep collision in map-cell space so doors, walls and moving pushwalls share one boundary rule. */
bool wolf_world_state_position_overlaps_solid(
    const WolfWorldState *world,
    float player_world_x,
    float player_world_z,
    float player_radius_tiles)
{
    float map_x;
    float map_y;
    int minimum_x;
    int maximum_x;
    int minimum_y;
    int maximum_y;

    if (world == NULL || !isfinite(player_world_x) || !isfinite(player_world_z) ||
        !isfinite(player_radius_tiles) || player_radius_tiles < 0.0f) {
        return true;
    }

    map_x = map_coordinate(player_world_x, world->level.width);
    map_y = map_coordinate(player_world_z, world->level.height);
    if (!isfinite(map_x) || !isfinite(map_y) ||
        map_x - player_radius_tiles < 0.0f ||
        map_y - player_radius_tiles < 0.0f ||
        map_x + player_radius_tiles > (float)world->level.width ||
        map_y + player_radius_tiles > (float)world->level.height) {
        return true;
    }

    minimum_x = (int)floorf(map_x - player_radius_tiles);
    maximum_x = (int)floorf(map_x + player_radius_tiles);
    minimum_y = (int)floorf(map_y - player_radius_tiles);
    maximum_y = (int)floorf(map_y + player_radius_tiles);

    for (int y = minimum_y; y <= maximum_y; ++y) {
        for (int x = minimum_x; x <= maximum_x; ++x) {
            uint16_t object_marker =
                world->level.object_tiles[tile_index((uint16_t)x, (uint16_t)y)];

            if (!map_cell_is_solid(world, x, y) &&
                !pushwall_occupies_cell(world, x, y) &&
                !wolf_level_object_marker_blocks_player(object_marker)) {
                continue;
            }

            if (position_overlaps_cell(
                    map_x, map_y, player_radius_tiles, x, y)) {
                return true;
            }
        }
    }

    return false;
}

/* Keys are collected on entry to their original info-plane cell, then removed from the map. */
bool wolf_world_state_collect_key(
    WolfWorldState *world,
    float player_world_x,
    float player_world_z)
{
    uint8_t previous_keys;

    if (world == NULL || !isfinite(player_world_x) || !isfinite(player_world_z)) {
        return false;
    }

    previous_keys = world->key_mask;
    collect_key_at_tile(
        world,
        (int)floorf(map_coordinate(player_world_x, world->level.width)),
        (int)floorf(map_coordinate(player_world_z, world->level.height)));
    return previous_keys != world->key_mask;
}

static void set_vertex(
    VulkanVertex *vertex,
    const float position[3],
    const float texcoord[2],
    uint16_t texture_page,
    float shade)
{
    vertex->position[0] = position[0];
    vertex->position[1] = position[1];
    vertex->position[2] = position[2];
    vertex->colour[0] = shade;
    vertex->colour[1] = shade;
    vertex->colour[2] = shade;
    vertex->texcoord[0] = texcoord[0];
    vertex->texcoord[1] = texcoord[1];
    vertex->texture_layer = texture_page;
    vertex->use_texture = 1u;
}

static void append_quad(
    VulkanQuad *quad,
    const float positions[4][3],
    const float texcoords[4][2],
    uint16_t texture_page,
    float shade)
{
    for (size_t index = 0u; index < 4u; ++index) {
        set_vertex(
            &quad->vertices[index], positions[index],
            texcoords[index], texture_page, shade);
    }
}

static void clear_quad(VulkanQuad *quad)
{
    static const float position[3] = {0.0f, -100000.0f, 0.0f};
    static const float texcoord[2] = {0.0f, 0.0f};

    for (size_t index = 0u; index < 4u; ++index) {
        set_vertex(&quad->vertices[index], position, texcoord, 0u, 0.0f);
        quad->vertices[index].use_texture = 0u;
    }
}

/* Door pages are the final eight uncompressed VSWAP pages before the sprite range. */
static uint16_t door_texture_page(const WolfWorldState *world, const WolfDoor *door)
{
    uint16_t page = (uint16_t)(world->texture_page_count - WOLF_DOOR_TEXTURE_PAGE_COUNT);

    if (door->lock_type >= 1u && door->lock_type <= 4u) {
        page = (uint16_t)(page + 6u);
    } else if (door->lock_type == 5u) {
        page = (uint16_t)(page + 4u);
    }

    if (door->vertical) {
        ++page;
    }

    return page;
}

/*
 * The renderer disables face culling, so one quad is visible from both sides.
 * A second coincident quad with reversed UVs z-fights and appears as a
 * flickering, incorrectly oriented second door texture.
 */
static void build_door_quad(
    const WolfWorldState *world,
    const WolfDoor *door,
    VulkanQuad *quad)
{
    static const float door_uvs[4][2] = {
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}
    };
    float x0 = -((float)world->level.width * WOLF_WORLD_TILE_SIZE) * 0.5f +
        (float)door->tile_x * WOLF_WORLD_TILE_SIZE;
    float z0 = -((float)world->level.height * WOLF_WORLD_TILE_SIZE) * 0.5f +
        (float)door->tile_y * WOLF_WORLD_TILE_SIZE;
    float x1 = x0 + WOLF_WORLD_TILE_SIZE;
    float z1 = z0 + WOLF_WORLD_TILE_SIZE;
    float slide = ((float)door->position /
        (float)WOLF_WORLD_DOOR_OPEN_POSITION) * WOLF_WORLD_TILE_SIZE;
    float plane;
    float positions[4][3];

    if (door->action == WOLF_DOOR_OPEN) {
        clear_quad(quad);
        return;
    }

    if (door->vertical) {
        plane = (x0 + x1) * 0.5f;
        z0 += slide;
        z1 += slide;
        positions[0][0] = plane;
        positions[0][1] = 0.0f;
        positions[0][2] = z0;
        positions[1][0] = plane;
        positions[1][1] = 0.0f;
        positions[1][2] = z1;
        positions[2][0] = plane;
        positions[2][1] = WOLF_WORLD_WALL_HEIGHT;
        positions[2][2] = z1;
        positions[3][0] = plane;
        positions[3][1] = WOLF_WORLD_WALL_HEIGHT;
        positions[3][2] = z0;
    } else {
        plane = (z0 + z1) * 0.5f;
        x0 += slide;
        x1 += slide;
        positions[0][0] = x0;
        positions[0][1] = 0.0f;
        positions[0][2] = plane;
        positions[1][0] = x1;
        positions[1][1] = 0.0f;
        positions[1][2] = plane;
        positions[2][0] = x1;
        positions[2][1] = WOLF_WORLD_WALL_HEIGHT;
        positions[2][2] = plane;
        positions[3][0] = x0;
        positions[3][1] = WOLF_WORLD_WALL_HEIGHT;
        positions[3][2] = plane;
    }

    append_quad(
        quad, (const float (*)[3])positions, door_uvs,
        door_texture_page(world, door), 0.98f);
}

/* A moving pushwall remains a complete textured block while its map-cell origin changes. */
static bool build_pushwall_quads(
    const WolfWorldState *world,
    VulkanQuad *quads)
{
    static const float wall_uvs[4][2] = {
        {1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}
    };
    float progress;
    float map_x;
    float map_y;
    float x0;
    float x1;
    float z0;
    float z1;
    float x_face[4][3];
    float z_face[4][3];
    uint16_t horizontal_page;
    uint16_t vertical_page;

    if (!world->pushwall.active) {
        for (size_t index = 0u; index < 4u; ++index) {
            clear_quad(&quads[index]);
        }
        return true;
    }

    if (!wolf_wall_texture_page_for_tile(
            world->pushwall.wall_tile, false,
            world->texture_page_count, &horizontal_page) ||
        !wolf_wall_texture_page_for_tile(
            world->pushwall.wall_tile, true,
            world->texture_page_count, &vertical_page)) {
        return false;
    }

    {
        float pwall_state = world->pushwall.elapsed_ticks;
        float tile_offset = floorf(pwall_state / WOLF_WORLD_PUSHWALL_TILE_TICKS);
        float texel_offset = fmodf(floorf(pwall_state * 0.5f), 64.0f) / 64.0f;

        /* Match pwallpos = (pwallstate / 2) & 63 and its 128-tic tile transitions. */
        progress = tile_offset + texel_offset;
    }
    map_x = (float)world->pushwall.source_x + 0.5f +
        (float)world->pushwall.direction_x * progress;
    map_y = (float)world->pushwall.source_y + 0.5f +
        (float)world->pushwall.direction_y * progress;

    x0 = -((float)world->level.width * WOLF_WORLD_TILE_SIZE) * 0.5f +
        (map_x - 0.5f) * WOLF_WORLD_TILE_SIZE;
    x1 = x0 + WOLF_WORLD_TILE_SIZE;
    z0 = -((float)world->level.height * WOLF_WORLD_TILE_SIZE) * 0.5f +
        (map_y - 0.5f) * WOLF_WORLD_TILE_SIZE;
    z1 = z0 + WOLF_WORLD_TILE_SIZE;

    {
        const float face[4][3] = {
            {x0, 0.0f, z0}, {x1, 0.0f, z0},
            {x1, WOLF_WORLD_WALL_HEIGHT, z0}, {x0, WOLF_WORLD_WALL_HEIGHT, z0}
        };
        memcpy(x_face, face, sizeof(x_face));
    }
    append_quad(&quads[0], (const float (*)[3])x_face, wall_uvs, horizontal_page, 0.88f);

    {
        const float face[4][3] = {
            {x1, 0.0f, z1}, {x0, 0.0f, z1},
            {x0, WOLF_WORLD_WALL_HEIGHT, z1}, {x1, WOLF_WORLD_WALL_HEIGHT, z1}
        };
        append_quad(&quads[1], face, wall_uvs, horizontal_page, 0.95f);
    }

    {
        const float face[4][3] = {
            {x0, 0.0f, z1}, {x0, 0.0f, z0},
            {x0, WOLF_WORLD_WALL_HEIGHT, z0}, {x0, WOLF_WORLD_WALL_HEIGHT, z1}
        };
        memcpy(z_face, face, sizeof(z_face));
    }
    append_quad(&quads[2], (const float (*)[3])z_face, wall_uvs, vertical_page, 0.82f);

    {
        const float face[4][3] = {
            {x1, 0.0f, z0}, {x1, 0.0f, z1},
            {x1, WOLF_WORLD_WALL_HEIGHT, z1}, {x1, WOLF_WORLD_WALL_HEIGHT, z0}
        };
        append_quad(&quads[3], face, wall_uvs, vertical_page, 0.90f);
    }

    return true;
}

uint32_t wolf_world_state_dynamic_quad_count(const WolfWorldState *world)
{
    if (world == NULL) {
        return 0u;
    }

    return (uint32_t)world->door_count +
        (world->has_secret_walls ? 4u : 0u);
}

/* Keep a constant quad count so the mapped Vulkan vertex buffer can be updated in place. */
bool wolf_world_state_build_dynamic_quads(
    const WolfWorldState *world,
    VulkanQuad *quads,
    uint32_t quad_count)
{
    uint32_t expected_count;
    uint32_t output_index = 0u;

    if (world == NULL || quads == NULL) {
        return false;
    }

    expected_count = wolf_world_state_dynamic_quad_count(world);
    if (quad_count != expected_count) {
        return false;
    }

    for (uint16_t index = 0u; index < world->door_count; ++index) {
        build_door_quad(world, &world->doors[index], &quads[output_index]);
        ++output_index;
    }

    if (world->has_secret_walls) {
        if (!build_pushwall_quads(world, &quads[output_index])) {
            return false;
        }
    }

    return true;
}

bool wolf_world_state_has_animated_geometry(const WolfWorldState *world)
{
    if (world == NULL) {
        return false;
    }

    if (world->pushwall.active) {
        return true;
    }

    for (uint16_t index = 0u; index < world->door_count; ++index) {
        if (world->doors[index].action == WOLF_DOOR_OPENING ||
            world->doors[index].action == WOLF_DOOR_CLOSING) {
            return true;
        }
    }

    return false;
}

uint32_t wolf_world_state_geometry_revision(const WolfWorldState *world)
{
    return world != NULL ? world->geometry_revision : 0u;
}
