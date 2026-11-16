#include "wolfvk/level_objects.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WOLF_PI 3.14159265358979f

/*
 * An enemy type with eight-way sprites. Each has four standing and four
 * patrolling markers (one per compass direction) for the easy tier; the
 * medium and hard tiers repeat them tier_step markers further on.
 */
typedef struct ActorFamily {
    const char *subject;
    uint16_t standing_marker;
    uint16_t patrolling_marker;
    uint16_t tier_step;
    /* Dogs have no standing frames; the original spawns them walking. */
    bool has_standing_pose;
} ActorFamily;

static const ActorFamily actor_families[] = {
    {"grd", 108u, 112u, 36u, true},
    {"ofc", 116u, 120u, 36u, true},
    {"ss", 126u, 130u, 36u, true},
    {"dog", 134u, 138u, 36u, false},
    {"mut", 216u, 220u, 18u, true},
};

/* Bosses and ghosts are single sprites; each stands in the first frame of its walk. */
typedef struct SingleActor {
    uint16_t marker;
    const char *subject;
} SingleActor;

static const SingleActor single_actors[] = {
    {214u, "boss"},   /* Hans Grosse */
    {197u, "gretel"}, /* Gretel Grosse */
    {215u, "gift"},   /* Otto Giftmacher */
    {179u, "fat"},    /* General Fettgesicht */
    {196u, "schabb"}, /* Dr. Schabbs */
    {160u, "fake"},   /* Fake Hitler */
    {178u, "mecha"},  /* Hitler starts inside his mech suit */
    {224u, "blinky"},
    {225u, "clyde"},
    {226u, "pinky"},
    {227u, "inky"},
};

/*
 * The original stores an actor's facing as dir * 2 in its eight-way dirtype
 * (east, northeast, north ...), so markers count east, north, west, south.
 */
static float yaw_for_marker_direction(uint16_t direction)
{
    static const float yaws[4] = {
        WOLF_PI * 0.5f, /* east */
        0.0f,           /* north */
        WOLF_PI * 1.5f, /* west */
        WOLF_PI         /* south */
    };

    return yaws[direction & 3u];
}

static bool cell_is_open(const WolfLevel *level, int tile_x, int tile_y)
{
    return tile_x >= 0 && tile_y >= 0 &&
        tile_x < (int)level->width && tile_y < (int)level->height &&
        level->wall_tiles[(size_t)tile_y * WOLF_LEVEL_PLANE_WIDTH + (size_t)tile_x] == 0u;
}

/*
 * Pick the compass direction with the most open floor in a straight line.
 * Ties go to the earlier direction in a fixed order so placement is stable.
 */
static float open_space_yaw(const WolfLevel *level, uint16_t tile_x, uint16_t tile_y)
{
    static const int step_x[4] = {0, 1, 0, -1};
    static const int step_y[4] = {1, 0, -1, 0};
    static const float yaws[4] = {WOLF_PI, WOLF_PI * 0.5f, 0.0f, WOLF_PI * 1.5f};
    uint32_t best_direction = 0u;
    uint32_t best_run = 0u;

    for (uint32_t direction = 0u; direction < 4u; ++direction) {
        uint32_t run = 0u;
        int x = (int)tile_x + step_x[direction];
        int y = (int)tile_y + step_y[direction];

        while (cell_is_open(level, x, y)) {
            ++run;
            x += step_x[direction];
            y += step_y[direction];
        }
        if (run > best_run) {
            best_run = run;
            best_direction = direction;
        }
    }
    return yaws[best_direction];
}

static void set_names(WolfLevelObject *object, const char *subject, const char *animation)
{
    (void)snprintf(object->subject, sizeof(object->subject), "%s", subject);
    (void)snprintf(object->animation, sizeof(object->animation), "%s", animation);
}

/*
 * Decide whether an eight-way enemy family owns this marker and, if the
 * difficulty spawns it, describe the object. Returns true when the marker
 * belongs to the family, whether or not it spawns.
 */
static bool classify_family_marker(
    const ActorFamily *family,
    uint16_t marker,
    WolfDifficulty difficulty,
    WolfLevelObject *object,
    bool *spawns)
{
    for (uint16_t tier = 0u; tier < 3u; ++tier) {
        uint16_t offset = (uint16_t)(tier * family->tier_step);
        bool standing = marker >= family->standing_marker + offset &&
            marker < family->standing_marker + offset + 4u;
        bool patrolling = marker >= family->patrolling_marker + offset &&
            marker < family->patrolling_marker + offset + 4u;
        uint16_t direction;
        char animation[WOLF_LEVEL_OBJECT_NAME_LENGTH];

        if (!standing && !patrolling) {
            continue;
        }

        /* Tier 1 needs medium difficulty and tier 2 needs hard, exactly as ScanInfoPlane falls through. */
        *spawns = (tier == 0u) ||
            (tier == 1u && difficulty >= WOLF_DIFFICULTY_MEDIUM) ||
            (tier == 2u && difficulty >= WOLF_DIFFICULTY_HARD);
        direction = (uint16_t)(marker -
            (standing ? family->standing_marker : family->patrolling_marker) - offset);

        (void)snprintf(animation, sizeof(animation), "%s_%s", family->subject,
            standing && family->has_standing_pose ? "s" : "w");
        set_names(object, family->subject, animation);
        object->pose = 0u;
        object->yaw_radians = yaw_for_marker_direction(direction);
        return true;
    }
    return false;
}

/* Describe the object a marker spawns at this difficulty; false when it spawns nothing drawable. */
static bool classify_marker(
    const WolfLevel *level,
    uint16_t marker,
    uint16_t tile_x,
    uint16_t tile_y,
    WolfDifficulty difficulty,
    WolfLevelObject *object)
{
    char name[WOLF_LEVEL_OBJECT_NAME_LENGTH];
    bool spawns = false;

    memset(object, 0, sizeof(*object));
    object->tile_x = tile_x;
    object->tile_y = tile_y;
    object->info_marker = marker;

    if ((marker >= WOLF_LEVEL_STATIC_FIRST && marker <= WOLF_LEVEL_STATIC_LAST) ||
        marker == WOLF_LEVEL_STATIC_EXTRA_CLIP) {
        unsigned int sprite = marker == WOLF_LEVEL_STATIC_EXTRA_CLIP
            ? WOLF_LEVEL_STATIC_EXTRA_CLIP_SPRITE
            : (unsigned int)(marker - WOLF_LEVEL_STATIC_FIRST);

        (void)snprintf(name, sizeof(name), "stat_%u", sprite);
        set_names(object, name, name);
        object->yaw_radians = open_space_yaw(level, tile_x, tile_y);
        return true;
    }

    if (marker == WOLF_LEVEL_DEAD_GUARD) {
        set_names(object, "grd", "grd_dead");
        object->yaw_radians = open_space_yaw(level, tile_x, tile_y);
        return true;
    }

    for (size_t family = 0u; family < sizeof(actor_families) / sizeof(actor_families[0]); ++family) {
        if (classify_family_marker(&actor_families[family], marker, difficulty, object, &spawns)) {
            return spawns;
        }
    }

    for (size_t actor = 0u; actor < sizeof(single_actors) / sizeof(single_actors[0]); ++actor) {
        if (single_actors[actor].marker == marker) {
            (void)snprintf(name, sizeof(name), "%s_w", single_actors[actor].subject);
            set_names(object, single_actors[actor].subject, name);
            object->yaw_radians = open_space_yaw(level, tile_x, tile_y);
            return true;
        }
    }

    return false;
}

bool wolf_level_find_objects(
    const WolfLevel *level,
    WolfDifficulty difficulty,
    WolfLevelObject **objects,
    uint32_t *object_count)
{
    WolfLevelObject *found;
    uint32_t count = 0u;

    if (objects != NULL) {
        *objects = NULL;
    }
    if (object_count != NULL) {
        *object_count = 0u;
    }
    if (level == NULL || objects == NULL || object_count == NULL ||
        level->width > WOLF_LEVEL_PLANE_WIDTH || level->height > WOLF_LEVEL_PLANE_HEIGHT ||
        (int)difficulty < (int)WOLF_DIFFICULTY_BABY || (int)difficulty > (int)WOLF_DIFFICULTY_HARD) {
        return false;
    }
    if (level->width == 0u || level->height == 0u) {
        return true;
    }

    /* At most one object can stand on each map cell. */
    found = malloc((size_t)level->width * level->height * sizeof(*found));
    if (found == NULL) {
        return false;
    }

    for (uint16_t tile_y = 0u; tile_y < level->height; ++tile_y) {
        for (uint16_t tile_x = 0u; tile_x < level->width; ++tile_x) {
            uint16_t marker = level->object_tiles[(size_t)tile_y * WOLF_LEVEL_PLANE_WIDTH + tile_x];

            if (marker != 0u &&
                classify_marker(level, marker, tile_x, tile_y, difficulty, &found[count])) {
                ++count;
            }
        }
    }

    if (count == 0u) {
        free(found);
        return true;
    }
    *objects = found;
    *object_count = count;
    return true;
}
