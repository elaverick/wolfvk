#include "wolfvk/weapon.h"

#include "game_palette.h"
#include "wolfvk/vox_file.h"
#include "wolfvk/voxel_mesh.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const weapon_subjects[WOLF_WEAPON_COUNT] = {
    "knife", "pistol", "machinegun", "chain"
};

static const char *const weapon_animations[WOLF_WEAPON_COUNT] = {
    "knife", "pistol", "machinegun", "chain"
};

/* Keep the first missing asset visible in the startup diagnostic without failing the level. */
static void record_problem(WolfWeaponScene *scene, const char *subject, const char *reason)
{
    if (scene->first_problem[0] == '\0') {
        (void)snprintf(scene->first_problem, sizeof(scene->first_problem),
            "%s.vox: %s", subject, reason);
    }
}

/* Append a weapon frame to one shared mesh and retain its indexed draw range. */
static bool append_pose(
    WolfWeaponScene *scene,
    WolfWeaponPose *pose,
    const VulkanQuad *quads,
    uint32_t quad_count)
{
    VulkanQuad *grown;

    if (quad_count == 0u) {
        return true;
    }
    if (quad_count > UINT32_MAX - scene->quad_count) {
        return false;
    }
    grown = realloc(scene->quads, ((size_t)scene->quad_count + quad_count) * sizeof(*grown));
    if (grown == NULL) {
        return false;
    }
    scene->quads = grown;
    memcpy(scene->quads + scene->quad_count, quads, (size_t)quad_count * sizeof(*quads));
    pose->first_quad = scene->quad_count;
    pose->quad_count = quad_count;
    pose->available = true;
    scene->quad_count += quad_count;
    return true;
}

/* Read ready plus the four original attack sprites from a weapon subject's named VOX animations. */
static bool load_weapon_subject(
    WolfWeaponScene *scene,
    WolfWeaponType weapon,
    const char *asset_directory)
{
    char path[512];
    char error_message[160];
    WolfVoxFile file;
    const uint8_t *palette;
    const char *subject = weapon_subjects[weapon];
    const char *animation = weapon_animations[weapon];

    if (snprintf(path, sizeof(path), "%s/%s.vox", asset_directory, subject) < 0 ||
        strlen(path) >= sizeof(path)) {
        record_problem(scene, subject, "asset path is too long");
        scene->unavailable_pose_count += 5u;
        return true;
    }
    if (!wolf_vox_file_load(path, &file, error_message, sizeof(error_message))) {
        record_problem(scene, subject, error_message);
        scene->unavailable_pose_count += 5u;
        return true;
    }

    palette = file.has_palette ? file.palette_rgba : wolf_game_palette_rgba;
    for (uint32_t frame = 0u; frame < 5u; ++frame) {
        const char *animation_name = frame == 0u ? "ready" : "attack";
        char named_animation[WOLF_VOX_OBJECT_NAME_LENGTH];
        const WolfVoxModel *model = NULL;
        VulkanQuad *quads = NULL;
        uint32_t quad_count = 0u;
        WolfWeaponPose *pose = frame == 0u
            ? &scene->ready[weapon]
            : &scene->attack[weapon][frame - 1u];

        (void)snprintf(named_animation, sizeof(named_animation), "%s_%s", animation, animation_name);
        if (!wolf_vox_file_find_pose(&file, named_animation, frame == 0u ? 0u : frame - 1u, &model)) {
            char reason[128];

            (void)snprintf(reason, sizeof(reason), "missing %s pose %u",
                named_animation, frame == 0u ? 0u : frame - 1u);
            record_problem(scene, subject, reason);
            ++scene->unavailable_pose_count;
            continue;
        }
        if (!wolf_voxel_mesh_build(model, palette, &quads, &quad_count)) {
            free(quads);
            wolf_vox_file_destroy(&file);
            return false;
        }
        if (!append_pose(scene, pose, quads, quad_count)) {
            free(quads);
            wolf_vox_file_destroy(&file);
            return false;
        }
        free(quads);
        if (!pose->available) {
            ++scene->unavailable_pose_count;
        }
    }

    wolf_vox_file_destroy(&file);
    return true;
}

bool wolf_weapon_scene_load(WolfWeaponScene *scene, const char *asset_directory)
{
    if (scene != NULL) {
        memset(scene, 0, sizeof(*scene));
    }
    if (scene == NULL || asset_directory == NULL || asset_directory[0] == '\0') {
        return false;
    }

    for (uint32_t weapon = 0u; weapon < WOLF_WEAPON_COUNT; ++weapon) {
        if (!load_weapon_subject(scene, (WolfWeaponType)weapon, asset_directory)) {
            wolf_weapon_scene_destroy(scene);
            return false;
        }
    }
    return true;
}

void wolf_weapon_scene_destroy(WolfWeaponScene *scene)
{
    if (scene == NULL) {
        return;
    }
    free(scene->quads);
    memset(scene, 0, sizeof(*scene));
}

void wolf_weapon_state_initialize(WolfWeaponState *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->selected = WOLF_WEAPON_PISTOL;
}

bool wolf_weapon_state_select(WolfWeaponState *state, WolfWeaponType weapon)
{
    if (state == NULL || weapon < WOLF_WEAPON_KNIFE || weapon >= WOLF_WEAPON_COUNT) {
        return false;
    }
    state->selected = weapon;
    state->attack_frame = 0u;
    state->tic_remainder = 0.0f;
    state->firing = false;
    return true;
}

/*
 * Original WL_AGENT attackinfo advances four six-tic states. Pistol and knife
 * complete once; machinegun repeats attack frame 2, while chaingun repeats
 * attack frame 3. The original gameplay attack callbacks are intentionally
 * not invoked yet: this state only drives the weapon model.
 */
bool wolf_weapon_state_update(WolfWeaponState *state, bool fire_down, float delta_seconds)
{
    float elapsed_tics;

    if (state == NULL || state->selected < WOLF_WEAPON_KNIFE ||
        state->selected >= WOLF_WEAPON_COUNT || !isfinite(delta_seconds) ||
        delta_seconds < 0.0f) {
        return false;
    }

    if (!fire_down && !state->firing) {
        return true;
    }

    if (fire_down && !state->firing) {
        state->firing = true;
        state->attack_frame = 0u;
        state->tic_remainder = 0.0f;
    }

    if (delta_seconds > 0.0f) {
        elapsed_tics = state->tic_remainder + delta_seconds / WOLF_WEAPON_TIC_SECONDS;
        while (elapsed_tics >= (float)WOLF_WEAPON_ATTACK_TICS) {
            elapsed_tics -= (float)WOLF_WEAPON_ATTACK_TICS;
            if (state->attack_frame < 3u) {
                ++state->attack_frame;
            } else if ((state->selected == WOLF_WEAPON_MACHINEGUN ||
                        state->selected == WOLF_WEAPON_CHAINGUN) && fire_down) {
                state->attack_frame = state->selected == WOLF_WEAPON_MACHINEGUN ? 2u : 3u;
            } else {
                state->attack_frame = 0u;
                state->firing = false;
                state->tic_remainder = 0.0f;
                return true;
            }
        }
        state->tic_remainder = elapsed_tics;
    }

    if (!fire_down && state->attack_frame == 0u && state->tic_remainder == 0.0f) {
        state->firing = false;
    }
    return true;
}

const WolfWeaponPose *wolf_weapon_scene_current_pose(
    const WolfWeaponScene *scene,
    const WolfWeaponState *state)
{
    const WolfWeaponPose *pose;

    if (scene == NULL || state == NULL || state->selected < WOLF_WEAPON_KNIFE ||
        state->selected >= WOLF_WEAPON_COUNT) {
        return NULL;
    }
    pose = state->firing
        ? &scene->attack[state->selected][state->attack_frame]
        : &scene->ready[state->selected];
    return pose->available ? pose : NULL;
}

/* Build a small camera-space weapon transform so the model follows yaw, pitch and player movement. */
bool wolf_weapon_build_view_projection(
    const WolfCamera *camera,
    float aspect_ratio,
    float attack_offset,
    float matrix[16])
{
    float camera_projection[16];
    float pitch_cosine;
    float forward[3];
    float right[3];
    float up[3];
    float position[3];
    float model[16] = {0};
    const float scale = 0.30f;
    float product[16] = {0};

    if (camera == NULL || matrix == NULL || !isfinite(aspect_ratio) ||
        aspect_ratio <= 0.0f || !isfinite(attack_offset) ||
        !wolf_camera_build_view_projection(camera, aspect_ratio, camera_projection)) {
        return false;
    }

    pitch_cosine = cosf(camera->pitch_radians);
    forward[0] = sinf(camera->yaw_radians) * pitch_cosine;
    forward[1] = sinf(camera->pitch_radians);
    forward[2] = -cosf(camera->yaw_radians) * pitch_cosine;
    right[0] = cosf(camera->yaw_radians);
    right[1] = 0.0f;
    right[2] = sinf(camera->yaw_radians);
    up[0] = -sinf(camera->yaw_radians) * sinf(camera->pitch_radians);
    up[1] = cosf(camera->pitch_radians);
    up[2] = cosf(camera->yaw_radians) * sinf(camera->pitch_radians);

    /* Offset the model to the player's right and below eye level, with a subtle firing kick. */
    for (int axis = 0; axis < 3; ++axis) {
        position[axis] = camera->position[axis] +
            forward[axis] * 1.0f + right[axis] * 0.38f - up[axis] * (0.48f + attack_offset);
    }

    /* VOX weapon models face -Z, so their depth axis is the negative camera-forward vector. */
    model[0] = right[0] * scale;
    model[1] = right[1] * scale;
    model[2] = right[2] * scale;
    model[4] = up[0] * scale;
    model[5] = up[1] * scale;
    model[6] = up[2] * scale;
    model[8] = -forward[0] * scale;
    model[9] = -forward[1] * scale;
    model[10] = -forward[2] * scale;
    model[12] = position[0];
    model[13] = position[1];
    model[14] = position[2];
    model[15] = 1.0f;

    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            for (int component = 0; component < 4; ++component) {
                product[column * 4 + row] +=
                    camera_projection[component * 4 + row] * model[column * 4 + component];
            }
        }
    }
    memcpy(matrix, product, sizeof(product));
    return true;
}
