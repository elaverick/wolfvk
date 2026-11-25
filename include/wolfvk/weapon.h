#ifndef WOLFVK_WEAPON_H
#define WOLFVK_WEAPON_H

#include "wolfvk/camera.h"
#include "wolfvk/player_state.h"
#include "wolfvk/vulkan_renderer.h"

#include <stdbool.h>
#include <stdint.h>

#define WOLF_WEAPON_TIC_SECONDS (1.0f / 70.0f)
#define WOLF_WEAPON_ATTACK_TICS 6u

typedef struct WolfWeaponPose {
    uint32_t first_quad;
    uint32_t quad_count;
    bool available;
} WolfWeaponPose;

typedef struct WolfWeaponScene {
    WolfWeaponPose ready[WOLF_WEAPON_COUNT];
    WolfWeaponPose attack[WOLF_WEAPON_COUNT][4];
    VulkanQuad *quads;
    uint32_t quad_count;
    uint32_t unavailable_pose_count;
    char first_problem[256];
} WolfWeaponScene;

typedef struct WolfWeaponState {
    WolfWeaponType selected;
    uint8_t attack_frame;
    float tic_remainder;
    bool firing;
} WolfWeaponState;

/* Load the four weapon subjects produced by the sprite voxelizer. Missing files are non-fatal. */
bool wolf_weapon_scene_load(WolfWeaponScene *scene, const char *asset_directory);
void wolf_weapon_scene_destroy(WolfWeaponScene *scene);

/* Initialise the pistol as the original starting weapon, ready to fire. */
void wolf_weapon_state_initialize(WolfWeaponState *state);

/* Select a weapon directly; ammunition and weapon pickup gating are intentionally out of scope. */
bool wolf_weapon_state_select(WolfWeaponState *state, WolfWeaponType weapon);

/* Advance the original six-tic attack sequence at 70 tics per second. */
bool wolf_weapon_state_update(WolfWeaponState *state, bool fire_down, float delta_seconds);

/* Resolve the selected weapon's currently displayed pose in the loaded scene. */
const WolfWeaponPose *wolf_weapon_scene_current_pose(
    const WolfWeaponScene *scene,
    const WolfWeaponState *state);

/* Build a view-projection matrix which anchors the model at the player's camera. */
bool wolf_weapon_build_view_projection(
    const WolfCamera *camera,
    float aspect_ratio,
    float attack_offset,
    float matrix[16]);

#endif
