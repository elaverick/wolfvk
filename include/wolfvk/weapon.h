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
    /* The original gamestate.attackframe: which of the four attackinfo entries is showing. */
    uint8_t attack_frame;
    float tic_remainder;
    bool firing;
    /* Fire was down last update; like buttonheld, a new attack needs a fresh press. */
    bool fire_was_down;
} WolfWeaponState;

/*
 * What an update's attack frames did. Each gun shot has already used its
 * bullet; damage is applied by the enemy code, which reads these counts.
 */
typedef struct WolfWeaponAttackEvents {
    uint32_t gun_shots;
    uint32_t knife_attacks;
} WolfWeaponAttackEvents;

/* Load the four weapon subjects produced by the sprite voxelizer. Missing files are non-fatal. */
bool wolf_weapon_scene_load(WolfWeaponScene *scene, const char *asset_directory);
void wolf_weapon_scene_destroy(WolfWeaponScene *scene);

/* Initialise the pistol as the original starting weapon, ready to fire. */
void wolf_weapon_state_initialize(WolfWeaponState *state);

/* Select a weapon directly; ammunition and weapon pickup gating are intentionally out of scope. */
bool wolf_weapon_state_select(WolfWeaponState *state, WolfWeaponType weapon);

/*
 * Run the original attack (Cmd_Fire and T_Attack) at 70 tics per second.
 * A press of fire starts an attack; holding it does not start another, but
 * keeps the machine gun and chaingun firing. Each gun shot uses one of the
 * player's bullets. When the attack ends with no bullets left the knife comes
 * out, otherwise the chosen weapon returns. `player` and `events` may be NULL
 * (the animation then runs without ammunition rules); events are cleared first.
 */
bool wolf_weapon_state_update(
    WolfWeaponState *state,
    WolfPlayerState *player,
    bool fire_down,
    float delta_seconds,
    WolfWeaponAttackEvents *events);

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
