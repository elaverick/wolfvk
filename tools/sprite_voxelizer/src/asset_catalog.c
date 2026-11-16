#include "asset_catalog.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Static objects begin at SPR_STAT_0, sprite index 2 in WL6. */
#define FIRST_STATIC_SPRITE 2
#define STATIC_SPRITE_COUNT 48

/* Default thickness, relative to width, for people with no rotation frames (bosses, BJ). */
#define DEFAULT_HUMANOID_DEPTH_SCALE 0.6

typedef struct StaticObjectProfile {
    const char *description;
    WolfDepthProfile profile;
    double depth_scale;
    double maximum_half_depth;
    double extrude_half_depth;
} StaticObjectProfile;

/*
 * Depth choices for the WL6 static objects, in SPR_STAT order. Descriptions
 * follow the comments of statinfo[] in WL_ACT1.C (with the retail WL6 art
 * where the comment is stale). Round objects use the lathe; flat treasure,
 * signs and racks are extruded; irregular shapes are inflated.
 */
static const StaticObjectProfile static_object_profiles[STATIC_SPRITE_COUNT] = {
    {"puddle", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"green barrel", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"table and chairs", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"floor lamp", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"chandelier", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"hanged skeleton", WOLF_DEPTH_PROFILE_INFLATE, 0.6, 32.0, 0.0},
    {"dog food", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"white pillar", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"potted tree", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"skeleton lying flat", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 4.0, 0.0},
    {"sink", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"potted plant", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"blue urn", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"bare table", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"ceiling light", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"kitchen utensils", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 1.5},
    {"suit of armour", WOLF_DEPTH_PROFILE_INFLATE, 0.6, 32.0, 0.0},
    {"hanging cage", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"skeleton in cage", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"pile of bones", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 6.0, 0.0},
    {"gold key", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 1.0},
    {"silver key", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 1.0},
    {"bed", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 16.0},
    {"basket", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"food", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"first aid kit", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 6.0},
    {"ammunition clip", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 2.0},
    {"machine gun", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 2.5, 0.0},
    {"chain gun", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 3.0, 0.0},
    {"cross", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 1.5},
    {"chalice", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"treasure chest", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 5.0},
    {"crown", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"extra life", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 32.0, 0.0},
    {"gibs", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 4.0, 0.0},
    {"wooden barrel", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"well with water", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"empty well", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"pool of blood", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"flag", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 1.0},
    {"junk", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 3.0, 0.0},
    {"junk", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 3.0, 0.0},
    {"junk", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 3.0, 0.0},
    {"junk", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 3.0, 0.0},
    {"hanging pots", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 4.0, 0.0},
    {"stove", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0},
    {"spear rack", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 2.0},
    {"vines", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 0.5},
};

double wolf_rotation_degrees(int rotation)
{
    return 45.0 * (double)rotation;
}

/* "SPR_GRD_W1_1" -> "grd_w1_1"; the original also has one enum without the prefix. */
static void lowercase_asset_name(const char *sprite_name, char *name, size_t name_size)
{
    const char *source = strncmp(sprite_name, "SPR_", 4u) == 0 ? sprite_name + 4 : sprite_name;
    size_t length = 0u;

    while (source[length] != '\0' && length + 1u < name_size) {
        name[length] = (char)tolower((unsigned char)source[length]);
        ++length;
    }
    name[length] = '\0';
}

/* The first name token after "SPR_" identifies the actor or object family. */
static void subject_for_sprite(const char *sprite_name, char *subject, size_t subject_size)
{
    const char *source = strncmp(sprite_name, "SPR_", 4u) == 0 ? sprite_name + 4 : sprite_name;
    size_t length = 0u;

    while (source[length] != '\0' && source[length] != '_' && length + 1u < subject_size) {
        subject[length] = source[length];
        ++length;
    }
    subject[length] = '\0';
}

static bool sprite_starts_with(int sprite, const char *prefix)
{
    return strncmp(wolf_wl6_sprite_names[sprite], prefix, strlen(prefix)) == 0;
}

/* Choose how a camera-facing sprite gains depth when it is not a static object. */
static void configure_dynamic_single_view(WolfVoxelAsset *asset)
{
    int sprite = asset->first_sprite;

    asset->single_view.profile = WOLF_DEPTH_PROFILE_INFLATE;
    asset->single_view.depth_scale = 1.0;
    asset->single_view.maximum_half_depth = 32.0;
    asset->single_view.extrude_half_depth = 0.0;
    asset->depth_from_subject = false;

    if (sprite_starts_with(sprite, "SPR_DEMO") || sprite_starts_with(sprite, "SPR_DEATHCAM")) {
        asset->description = "title lettering";
        asset->single_view.profile = WOLF_DEPTH_PROFILE_EXTRUDE;
        asset->single_view.extrude_half_depth = 1.0;
    } else if (sprite_starts_with(sprite, "SPR_SMOKE") || sprite_starts_with(sprite, "SPR_BOOM") ||
        sprite_starts_with(sprite, "SPR_FIRE") || sprite_starts_with(sprite, "SPR_BLINKY") ||
        sprite_starts_with(sprite, "SPR_PINKY") || sprite_starts_with(sprite, "SPR_CLYDE") ||
        sprite_starts_with(sprite, "SPR_INKY")) {
        /* Puffs, fireballs and the ghost bonus enemies are round. */
        asset->description = "round effect or ghost";
    } else if (sprite_starts_with(sprite, "SPR_HYPO")) {
        asset->description = "thrown syringe";
        asset->single_view.depth_scale = 0.6;
    } else if (sprite >= WOLF_WL6_SPRITE_COUNT - 20) {
        /* The last twenty sprites are the player's first-person weapon frames. */
        asset->description = "first-person weapon";
        asset->single_view.depth_scale = 0.6;
    } else {
        /* Pain, attack, death and boss frames: people and dogs facing the camera. */
        asset->description = "camera-facing actor frame";
        asset->single_view.depth_scale = DEFAULT_HUMANOID_DEPTH_SCALE;
        asset->depth_from_subject = true;
    }
}

void wolf_asset_catalog_build(WolfAssetCatalog *catalog)
{
    bool rotation_base[WOLF_WL6_SPRITE_COUNT] = {false};

    memset(catalog, 0, sizeof(*catalog));
    for (int sprite = 0; sprite < WOLF_WL6_SPRITE_COUNT; ++sprite) {
        catalog->asset_for_sprite[sprite] = -1;
    }

    /*
     * A state with rotate == 1 tells CalcRotate to add 0..7 to its sprite,
     * so the original game itself identifies every eight-way set.
     */
    for (int state = 0; state < WOLF_WL6_STATE_COUNT; ++state) {
        if (wolf_wl6_states[state].rotate == 1 && wolf_wl6_states[state].sprite_index >= 0) {
            rotation_base[wolf_wl6_states[state].sprite_index] = true;
        }
    }

    for (int sprite = 0; sprite < WOLF_WL6_SPRITE_COUNT; ++sprite) {
        WolfVoxelAsset *asset;
        int asset_index;

        if (catalog->asset_for_sprite[sprite] >= 0) {
            continue;
        }
        asset_index = (int)catalog->asset_count++;
        asset = &catalog->assets[asset_index];
        asset->first_sprite = sprite;
        subject_for_sprite(wolf_wl6_sprite_names[sprite], asset->subject, sizeof(asset->subject));

        if (rotation_base[sprite] && sprite + WOLF_ROTATION_COUNT <= WOLF_WL6_SPRITE_COUNT) {
            size_t length;

            asset->kind = WOLF_ASSET_ROTATION_SET;
            asset->sprite_count = WOLF_ROTATION_COUNT;
            asset->description = "eight-way rotation set";
            /* Drop the "_1" frame suffix: SPR_GRD_W1_1 becomes grd_w1. */
            lowercase_asset_name(wolf_wl6_sprite_names[sprite], asset->name, sizeof(asset->name));
            length = strlen(asset->name);
            if (length >= 2u && asset->name[length - 2u] == '_' && asset->name[length - 1u] == '1') {
                asset->name[length - 2u] = '\0';
            }
            for (int rotation = 0; rotation < WOLF_ROTATION_COUNT; ++rotation) {
                catalog->asset_for_sprite[sprite + rotation] = asset_index;
                catalog->rotation_for_sprite[sprite + rotation] = rotation;
            }
            continue;
        }

        asset->kind = WOLF_ASSET_SINGLE_VIEW;
        asset->sprite_count = 1;
        lowercase_asset_name(wolf_wl6_sprite_names[sprite], asset->name, sizeof(asset->name));
        catalog->asset_for_sprite[sprite] = asset_index;
        catalog->rotation_for_sprite[sprite] = 0;

        if (sprite >= FIRST_STATIC_SPRITE && sprite < FIRST_STATIC_SPRITE + STATIC_SPRITE_COUNT) {
            const StaticObjectProfile *profile = &static_object_profiles[sprite - FIRST_STATIC_SPRITE];

            asset->description = profile->description;
            asset->single_view.profile = profile->profile;
            asset->single_view.depth_scale = profile->depth_scale;
            asset->single_view.maximum_half_depth = profile->maximum_half_depth;
            asset->single_view.extrude_half_depth = profile->extrude_half_depth;
            asset->depth_from_subject = false;
        } else {
            configure_dynamic_single_view(asset);
        }
    }
}

int wolf_asset_catalog_reference_for_subject(const WolfAssetCatalog *catalog, const char *subject)
{
    for (size_t asset = 0u; asset < catalog->asset_count; ++asset) {
        if (catalog->assets[asset].kind == WOLF_ASSET_ROTATION_SET &&
            strcmp(catalog->assets[asset].subject, subject) == 0) {
            return (int)asset;
        }
    }
    return -1;
}

/* "s_grdchase1s" -> "grdchase": the family shared by one sequence's states. */
static void animation_key(const char *state_name, char *key, size_t key_size)
{
    const char *source = strncmp(state_name, "s_", 2u) == 0 ? state_name + 2 : state_name;
    size_t length = strlen(source);

    if (length + 1u > key_size) {
        length = key_size - 1u;
    }
    memcpy(key, source, length);
    key[length] = '\0';

    /* The "s" suffix marks the short pause half of a two-part walking step. */
    if (length >= 2u && key[length - 1u] == 's' && isdigit((unsigned char)key[length - 2u])) {
        key[--length] = '\0';
    }
    while (length > 0u && isdigit((unsigned char)key[length - 1u])) {
        key[--length] = '\0';
    }
}

/* Append the player's weapon sequences, which WL_AGENT.C drives from attackinfo. */
static size_t append_weapon_animations(WolfAnimation *animations, size_t count, size_t capacity)
{
    static const char *const weapon_names[4] = {"knife", "pistol", "machinegun", "chaingun"};
    /* SPR_KNIFEREADY is the first of four five-frame weapon groups at the end of the enum. */
    const int first_weapon_sprite = WOLF_WL6_SPRITE_COUNT - 20;

    for (int weapon = 0; weapon < 4 && count < capacity; ++weapon) {
        WolfAnimation *animation = &animations[count++];
        int ready_sprite = first_weapon_sprite + weapon * 5;

        memset(animation, 0, sizeof(*animation));
        (void)snprintf(animation->name, sizeof(animation->name), "player_%s_attack", weapon_names[weapon]);
        /* attackinfo: four frames of six tics each, then back to the ready frame. */
        for (int frame = 1; frame <= 4; ++frame) {
            WolfAnimationFrame *entry = &animation->frames[animation->frame_count++];

            entry->state_index = -1;
            entry->sprite_index = ready_sprite + frame;
            entry->tics = 6;
            entry->rotate = 0;
        }
        animation->loop_frame = -1;
        animation->next_state_index = -1;
        (void)snprintf(animation->next_animation, sizeof(animation->next_animation),
            "player_%s_ready", weapon_names[weapon]);
        animation->notes = weapon >= 2 ?
            "From attackinfo in WL_AGENT.C; frame 3 repeats while fire is held." :
            "From attackinfo in WL_AGENT.C.";

        if (count < capacity) {
            WolfAnimation *ready = &animations[count++];

            memset(ready, 0, sizeof(*ready));
            (void)snprintf(ready->name, sizeof(ready->name), "player_%s_ready", weapon_names[weapon]);
            ready->frames[0].state_index = -1;
            ready->frames[0].sprite_index = ready_sprite;
            ready->frames[0].tics = 0;
            ready->frame_count = 1u;
            ready->loop_frame = 0;
            ready->next_state_index = -1;
            ready->notes = "Idle weapon frame drawn while not attacking.";
        }
    }
    return count;
}

size_t wolf_animation_catalog_build(WolfAnimation *animations, size_t capacity)
{
    char keys[WOLF_WL6_STATE_COUNT][WOLF_ASSET_NAME_LENGTH];
    bool has_family_predecessor[WOLF_WL6_STATE_COUNT] = {false};
    bool family_done[WOLF_WL6_STATE_COUNT] = {false};
    size_t count = 0u;

    for (int state = 0; state < WOLF_WL6_STATE_COUNT; ++state) {
        animation_key(wolf_wl6_states[state].name, keys[state], sizeof(keys[state]));
    }
    for (int state = 0; state < WOLF_WL6_STATE_COUNT; ++state) {
        int next = wolf_wl6_states[state].next_state_index;

        if (next >= 0 && next != state && strcmp(keys[state], keys[next]) == 0) {
            has_family_predecessor[next] = true;
        }
    }

    for (int family_first = 0; family_first < WOLF_WL6_STATE_COUNT; ++family_first) {
        int starts[WOLF_WL6_STATE_COUNT];
        size_t start_count = 0u;

        if (family_done[family_first]) {
            continue;
        }
        /*
         * Entry points are the family's states that no other family state
         * leads to. A family that is a pure loop (walking, standing) has
         * none, so its first state in source order is used instead.
         */
        for (int state = family_first; state < WOLF_WL6_STATE_COUNT; ++state) {
            if (strcmp(keys[state], keys[family_first]) == 0) {
                family_done[state] = true;
                if (!has_family_predecessor[state]) {
                    starts[start_count++] = state;
                }
            }
        }
        if (start_count == 0u) {
            starts[start_count++] = family_first;
        }

        for (size_t start = 0u; start < start_count && count < capacity; ++start) {
            WolfAnimation *animation = &animations[count++];
            int state = starts[start];

            memset(animation, 0, sizeof(*animation));
            animation->loop_frame = -1;
            animation->next_state_index = -1;
            animation->notes = "From the state tables in WL_ACT2.C.";
            /* Families with several entry points are named after each entry state. */
            if (start_count > 1u) {
                const char *name = wolf_wl6_states[state].name;

                (void)snprintf(animation->name, sizeof(animation->name), "%s",
                    strncmp(name, "s_", 2u) == 0 ? name + 2 : name);
            } else {
                (void)snprintf(animation->name, sizeof(animation->name), "%s", keys[state]);
            }

            for (;;) {
                const WolfStateDefinition *definition = &wolf_wl6_states[state];
                WolfAnimationFrame *frame;
                int next = definition->next_state_index;

                if (animation->frame_count >= WOLF_ANIMATION_MAX_FRAMES) {
                    break;
                }
                frame = &animation->frames[animation->frame_count++];
                frame->state_index = state;
                frame->sprite_index = definition->sprite_index;
                frame->tics = definition->tics;
                frame->rotate = definition->rotate;

                if (next < 0) {
                    break;
                }
                if (strcmp(keys[next], keys[state]) != 0) {
                    animation->next_state_index = next;
                    break;
                }
                /* Returning to a state already in this sequence closes the loop. */
                {
                    int revisit = -1;

                    for (size_t earlier = 0u; earlier < animation->frame_count; ++earlier) {
                        if (animation->frames[earlier].state_index == next) {
                            revisit = (int)earlier;
                            break;
                        }
                    }
                    if (revisit >= 0) {
                        animation->loop_frame = revisit;
                        break;
                    }
                }
                state = next;
            }
        }
    }

    /* Name each follow-on sequence by the animation that contains its entry state. */
    for (size_t animation = 0u; animation < count; ++animation) {
        int next = animations[animation].next_state_index;

        for (size_t candidate = 0u; next >= 0 && candidate < count; ++candidate) {
            bool contains = false;

            for (size_t frame = 0u; frame < animations[candidate].frame_count; ++frame) {
                contains = contains || animations[candidate].frames[frame].state_index == next;
            }
            if (contains) {
                (void)snprintf(animations[animation].next_animation,
                    sizeof(animations[animation].next_animation), "%s", animations[candidate].name);
                break;
            }
        }
    }
    return append_weapon_animations(animations, count, capacity);
}
