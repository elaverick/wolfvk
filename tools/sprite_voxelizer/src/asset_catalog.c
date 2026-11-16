#include "asset_catalog.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Static objects begin at SPR_STAT_0, sprite index 2 in WL6. */
#define FIRST_STATIC_SPRITE 2
#define STATIC_SPRITE_COUNT 48

/* Thickness relative to width for people with no rotation frames (the bosses, BJ). */
#define DEFAULT_HUMANOID_DEPTH_SCALE 0.8

typedef struct StaticObjectProfile {
    const char *description;
    WolfDepthProfile profile;
    double depth_scale;
    double maximum_half_depth;
    double extrude_half_depth;
    WolfHandBuiltModel hand_built;
} StaticObjectProfile;

/*
 * Depth choices for the WL6 static objects, in SPR_STAT order. Descriptions
 * follow the comments of statinfo[] in WL_ACT1.C (with the retail WL6 art
 * where the comment is stale). Round objects use the lathe; flat treasure,
 * signs and racks are extruded; irregular shapes are inflated; furniture no
 * profile can represent is modelled by hand.
 */
static const StaticObjectProfile static_object_profiles[STATIC_SPRITE_COUNT] = {
    {"puddle", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"green barrel", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"table and chairs", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_TABLE_AND_CHAIRS},
    {"floor lamp", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"chandelier", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"hanged skeleton", WOLF_DEPTH_PROFILE_INFLATE, 0.6, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"dog food", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"white pillar", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"potted tree", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"skeleton lying flat", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 4.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"sink", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"potted plant", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"blue urn", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"bare table", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"ceiling light", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"kitchen utensils", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 1.5, WOLF_HAND_BUILT_NONE},
    {"suit of armour", WOLF_DEPTH_PROFILE_INFLATE, 0.6, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"hanging cage", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"skeleton in cage", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"pile of bones", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 6.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"gold key", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 1.0, WOLF_HAND_BUILT_NONE},
    {"silver key", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 1.0, WOLF_HAND_BUILT_NONE},
    {"bed", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_BED},
    {"basket", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"food", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"first aid kit", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 6.0, WOLF_HAND_BUILT_NONE},
    {"ammunition clip", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 2.0, WOLF_HAND_BUILT_NONE},
    {"machine gun", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 2.5, 0.0, WOLF_HAND_BUILT_NONE},
    {"chain gun", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 3.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"cross", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 1.5, WOLF_HAND_BUILT_NONE},
    {"chalice", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"treasure chest", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 5.0, WOLF_HAND_BUILT_NONE},
    {"crown", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"extra life", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"gibs", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 4.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"wooden barrel", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"well with water", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"empty well", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"pool of blood", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"flag", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 1.0, WOLF_HAND_BUILT_NONE},
    {"junk", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 3.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"junk", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 3.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"junk", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 3.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"junk", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 3.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"hanging pots", WOLF_DEPTH_PROFILE_INFLATE, 1.0, 4.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"stove", WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, WOLF_HAND_BUILT_NONE},
    {"spear rack", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 2.0, WOLF_HAND_BUILT_NONE},
    {"vines", WOLF_DEPTH_PROFILE_EXTRUDE, 1.0, 32.0, 0.5, WOLF_HAND_BUILT_NONE},
};

/* First-person weapons: subject prefixes in the order of the sprite enum. */
static const char *const weapon_prefixes[4] = {"KNIFE", "PISTOL", "MACHINEGUN", "CHAIN"};

/*
 * Families whose sprites have a painted floor shadow. Effects, projectiles,
 * the title lettering and the first-person weapons have none, and their dark
 * pixels must not be mistaken for one.
 */
static const char *const shadowed_actor_subjects[] = {
    "GRD", "DOG", "SS", "MUT", "OFC", "BLINKY", "PINKY", "CLYDE", "INKY", "BOSS", "SCHABB",
    "FAKE", "MECHA", "HITLER", "GIFT", "GRETEL", "FAT", "BJ"
};

/* Animation names that show an upright actor and can borrow the rotation-set body. */
static const char *const upright_animation_keys[] = {"PAIN", "SHOOT"};

double wolf_rotation_degrees(int rotation)
{
    return 45.0 * (double)rotation;
}

static void copy_lowercase(const char *source, size_t length, char *destination, size_t destination_size)
{
    size_t written = 0u;

    while (written < length && source[written] != '\0' && written + 1u < destination_size) {
        destination[written] = (char)tolower((unsigned char)source[written]);
        ++written;
    }
    destination[written] = '\0';
}

/*
 * Split a sprite name into its subject and animation keys (upper case, as in
 * the enum). "SPR_GRD_W1_1" -> GRD / W, "SPR_HYPO3" -> HYPO / "",
 * "SPR_PISTOLATK2" -> PISTOL / ATK, "SPR_STAT_7" -> STAT_7 / "".
 */
static void split_sprite_name(const char *sprite_name, char *subject, size_t subject_size,
    char *animation, size_t animation_size)
{
    const char *source = strncmp(sprite_name, "SPR_", 4u) == 0 ? sprite_name + 4 : sprite_name;
    size_t source_length = strlen(source);
    size_t subject_length = 0u;
    size_t animation_length;
    const char *remainder;

    for (int weapon = 0; weapon < 4 && subject_length == 0u; ++weapon) {
        size_t prefix_length = strlen(weapon_prefixes[weapon]);

        if (strncmp(source, weapon_prefixes[weapon], prefix_length) == 0) {
            subject_length = prefix_length;
        }
    }
    if (subject_length == 0u && strncmp(source, "STAT_", 5u) == 0) {
        /* Every static object is its own subject. */
        subject_length = source_length;
    }
    if (subject_length == 0u) {
        while (subject_length < source_length && source[subject_length] != '_' &&
            !isdigit((unsigned char)source[subject_length])) {
            ++subject_length;
        }
    }
    (void)snprintf(subject, subject_size, "%.*s", (int)subject_length, source);

    remainder = source + subject_length;
    while (*remainder == '_') {
        ++remainder;
    }
    animation_length = strlen(remainder);
    /* Trailing frame and rotation numbers ("W1_1", "DIE_3", "SHOOT2") do not name the animation. */
    while (animation_length > 0u &&
        (isdigit((unsigned char)remainder[animation_length - 1u]) || remainder[animation_length - 1u] == '_')) {
        --animation_length;
    }
    (void)snprintf(animation, animation_size, "%.*s", (int)animation_length, remainder);
}

static bool name_in_list(const char *name, const char *const *list, size_t count)
{
    for (size_t entry = 0u; entry < count; ++entry) {
        if (strcmp(name, list[entry]) == 0) {
            return true;
        }
    }
    return false;
}

int wolf_asset_catalog_find_subject(const WolfAssetCatalog *catalog, const char *name)
{
    for (size_t subject = 0u; subject < catalog->subject_count; ++subject) {
        if (strcmp(catalog->subjects[subject].name, name) == 0) {
            return (int)subject;
        }
    }
    return -1;
}

/* Find or add the subject and animation an asset belongs to, and append it as the next pose. */
static void place_asset(WolfAssetCatalog *catalog, int asset_index, const char *subject_key, const char *animation_key)
{
    char subject_name[WOLF_ASSET_NAME_LENGTH];
    char animation_name[WOLF_ASSET_NAME_LENGTH];
    int subject_index;
    WolfSubject *subject;
    WolfSubjectAnimation *animation = NULL;

    copy_lowercase(subject_key, strlen(subject_key), subject_name, sizeof(subject_name));
    subject_index = wolf_asset_catalog_find_subject(catalog, subject_name);
    if (subject_index < 0) {
        subject_index = (int)catalog->subject_count++;
        subject = &catalog->subjects[subject_index];
        memset(subject, 0, sizeof(*subject));
        (void)snprintf(subject->name, sizeof(subject->name), "%s", subject_name);
        subject->reference_asset = -1;
    }
    subject = &catalog->subjects[subject_index];

    /* A subject drawn as one animation (a static object, the rocket) names it after itself. */
    if (animation_key[0] == '\0') {
        (void)snprintf(animation_name, sizeof(animation_name), "%s", subject_name);
    } else {
        char lowered[WOLF_ASSET_NAME_LENGTH];

        copy_lowercase(animation_key, strlen(animation_key), lowered, sizeof(lowered));
        /* Bounded so the joined name always fits; WL6 names are far shorter. */
        (void)snprintf(animation_name, sizeof(animation_name), "%.24s_%.22s", subject_name, lowered);
    }
    for (size_t existing = 0u; existing < subject->animation_count; ++existing) {
        if (strcmp(subject->animations[existing].name, animation_name) == 0) {
            animation = &subject->animations[existing];
        }
    }
    if (animation == NULL && subject->animation_count < WOLF_SUBJECT_MAX_ANIMATIONS) {
        animation = &subject->animations[subject->animation_count++];
        memset(animation, 0, sizeof(*animation));
        (void)snprintf(animation->name, sizeof(animation->name), "%s", animation_name);
    }
    if (animation != NULL && animation->pose_count < WOLF_ANIMATION_MAX_POSES) {
        animation->assets[animation->pose_count++] = asset_index;
        catalog->assets[asset_index].subject = subject_index;
        catalog->assets[asset_index].animation = (int)(animation - subject->animations);
    }
    if (catalog->assets[asset_index].method == WOLF_ASSET_ROTATION_SET && subject->reference_asset < 0) {
        subject->reference_asset = asset_index;
    }
}

/* Choose how a camera-facing sprite gains depth when it is not a static object. */
static void configure_dynamic_single_view(WolfAssetCatalog *catalog, WolfVoxelAsset *asset,
    const char *subject_key, const char *animation_key)
{
    int sprite = asset->first_sprite;
    char subject_name[WOLF_ASSET_NAME_LENGTH];

    asset->method = WOLF_ASSET_SINGLE_VIEW;
    asset->single_view.profile = WOLF_DEPTH_PROFILE_INFLATE;
    asset->single_view.depth_scale = 1.0;
    asset->single_view.maximum_half_depth = 32.0;
    asset->single_view.extrude_half_depth = 0.0;
    asset->depth_from_subject = false;
    asset->strip_shadow = name_in_list(subject_key, shadowed_actor_subjects,
        sizeof(shadowed_actor_subjects) / sizeof(shadowed_actor_subjects[0]));

    copy_lowercase(subject_key, strlen(subject_key), subject_name, sizeof(subject_name));
    if (strcmp(subject_key, "DEMO") == 0 || strcmp(subject_key, "DEATHCAM") == 0) {
        asset->description = "title lettering";
        asset->single_view.profile = WOLF_DEPTH_PROFILE_EXTRUDE;
        asset->single_view.extrude_half_depth = 1.0;
    } else if (strcmp(subject_key, "SMOKE") == 0 || strcmp(subject_key, "BOOM") == 0 ||
        strcmp(subject_key, "FIRE") == 0 || strcmp(subject_key, "BLINKY") == 0 ||
        strcmp(subject_key, "PINKY") == 0 || strcmp(subject_key, "CLYDE") == 0 ||
        strcmp(subject_key, "INKY") == 0) {
        /* Puffs, fireballs and the ghost bonus enemies are round. */
        asset->description = "round effect or ghost";
    } else if (strcmp(subject_key, "HYPO") == 0) {
        asset->description = "thrown syringe";
        asset->single_view.depth_scale = 0.6;
    } else if (sprite >= WOLF_WL6_SPRITE_COUNT - 20) {
        /* The last twenty sprites are the player's first-person weapon frames. */
        asset->description = "first-person weapon";
        asset->single_view.profile = WOLF_DEPTH_PROFILE_FIRST_PERSON;
        asset->single_view.depth_scale = 1.0;
        asset->single_view.maximum_half_depth = 6.0;
    } else {
        /* Pain, attack, death and boss frames: people and dogs facing the camera. */
        int subject_index = wolf_asset_catalog_find_subject(catalog, subject_name);
        bool has_reference = subject_index >= 0 && catalog->subjects[subject_index].reference_asset >= 0;

        asset->description = has_reference ? "camera-facing actor frame" : "camera-facing boss or character frame";
        asset->single_view.depth_scale = DEFAULT_HUMANOID_DEPTH_SCALE;
        asset->depth_from_subject = has_reference;
        if (has_reference && name_in_list(animation_key, upright_animation_keys,
                sizeof(upright_animation_keys) / sizeof(upright_animation_keys[0]))) {
            asset->method = WOLF_ASSET_POSE_TRANSFER;
            asset->description = "camera-facing actor frame on the actor's own body";
        }
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

    /*
     * Rotation sets are placed first so every subject knows its reference
     * body before its camera-facing frames are configured.
     */
    for (int pass = 0; pass < 2; ++pass) {
        for (int sprite = 0; sprite < WOLF_WL6_SPRITE_COUNT; ++sprite) {
            char subject_key[WOLF_ASSET_NAME_LENGTH];
            char animation_key[WOLF_ASSET_NAME_LENGTH];
            WolfVoxelAsset *asset;
            int asset_index;
            bool is_rotation = rotation_base[sprite] && sprite + WOLF_ROTATION_COUNT <= WOLF_WL6_SPRITE_COUNT;

            if (catalog->asset_for_sprite[sprite] >= 0 || (pass == 0) != is_rotation) {
                continue;
            }
            split_sprite_name(wolf_wl6_sprite_names[sprite], subject_key, sizeof(subject_key),
                animation_key, sizeof(animation_key));
            asset_index = (int)catalog->asset_count++;
            asset = &catalog->assets[asset_index];
            asset->first_sprite = sprite;
            asset->hand_built = WOLF_HAND_BUILT_NONE;

            if (is_rotation) {
                size_t length;

                asset->method = WOLF_ASSET_ROTATION_SET;
                asset->sprite_count = WOLF_ROTATION_COUNT;
                asset->description = "eight-way rotation set";
                asset->strip_shadow = name_in_list(subject_key, shadowed_actor_subjects,
                    sizeof(shadowed_actor_subjects) / sizeof(shadowed_actor_subjects[0]));
                /* Drop the "_1" frame suffix: SPR_GRD_W1_1 becomes grd_w1. */
                copy_lowercase(wolf_wl6_sprite_names[sprite] + 4, 64u, asset->name, sizeof(asset->name));
                length = strlen(asset->name);
                if (length >= 2u && asset->name[length - 2u] == '_' && asset->name[length - 1u] == '1') {
                    asset->name[length - 2u] = '\0';
                }
                for (int rotation = 0; rotation < WOLF_ROTATION_COUNT; ++rotation) {
                    catalog->asset_for_sprite[sprite + rotation] = asset_index;
                    catalog->rotation_for_sprite[sprite + rotation] = rotation;
                }
                place_asset(catalog, asset_index, subject_key, animation_key);
                continue;
            }

            asset->sprite_count = 1;
            {
                const char *source = wolf_wl6_sprite_names[sprite];

                copy_lowercase(strncmp(source, "SPR_", 4u) == 0 ? source + 4 : source, 64u,
                    asset->name, sizeof(asset->name));
            }
            catalog->asset_for_sprite[sprite] = asset_index;
            catalog->rotation_for_sprite[sprite] = 0;

            if (sprite >= FIRST_STATIC_SPRITE && sprite < FIRST_STATIC_SPRITE + STATIC_SPRITE_COUNT) {
                const StaticObjectProfile *profile = &static_object_profiles[sprite - FIRST_STATIC_SPRITE];

                asset->method = profile->hand_built != WOLF_HAND_BUILT_NONE ? WOLF_ASSET_HAND_BUILT : WOLF_ASSET_SINGLE_VIEW;
                asset->hand_built = profile->hand_built;
                asset->description = profile->description;
                asset->single_view.profile = profile->profile;
                asset->single_view.depth_scale = profile->depth_scale;
                asset->single_view.maximum_half_depth = profile->maximum_half_depth;
                asset->single_view.extrude_half_depth = profile->extrude_half_depth;
                asset->depth_from_subject = false;
                asset->strip_shadow = true;
            } else {
                configure_dynamic_single_view(catalog, asset, subject_key, animation_key);
            }
            place_asset(catalog, asset_index, subject_key, animation_key);
        }
    }

    /* Poses were placed rotation sets first; restore original drawing order within each animation. */
    for (size_t subject = 0u; subject < catalog->subject_count; ++subject) {
        for (size_t animation = 0u; animation < catalog->subjects[subject].animation_count; ++animation) {
            WolfSubjectAnimation *entry = &catalog->subjects[subject].animations[animation];

            for (size_t pose = 1u; pose < entry->pose_count; ++pose) {
                int moving = entry->assets[pose];
                size_t slot = pose;

                while (slot > 0u && catalog->assets[entry->assets[slot - 1u]].first_sprite > catalog->assets[moving].first_sprite) {
                    entry->assets[slot] = entry->assets[slot - 1u];
                    --slot;
                }
                entry->assets[slot] = moving;
            }
        }
    }
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
