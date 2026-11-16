#include "unity.h"

#include "wolfvk/weapon.h"
#include "test_vox_builder.h"

#include <math.h>
#include <stdio.h>
#include <string.h>


static const WolfVoxVoxel weapon_voxels[] = {{32u, 32u, 8u, 5u}};

/* Build four weapon subjects with the named ready pose and four attack keyframes. */
static bool write_test_weapon_assets(void)
{
    static const WolfVoxVoxel *const ready_poses[] = {weapon_voxels};
    static const uint32_t ready_counts[] = {1u};
    static const WolfVoxVoxel *const attack_poses[] = {
        weapon_voxels, weapon_voxels, weapon_voxels, weapon_voxels
    };
    static const uint32_t attack_counts[] = {1u, 1u, 1u, 1u};
    static const char *const subjects[] = {"knife", "pistol", "machinegun", "chain"};
    static const char *const animations[] = {
        "player_knife", "player_pistol", "player_machinegun", "player_chaingun"
    };

    (void)SDL_CreateDirectory("./wolfvk_test");
    for (size_t weapon = 0u; weapon < 4u; ++weapon) {
        TestVoxAnimation sequences[] = {
            {NULL, 1u, ready_poses, ready_counts},
            {NULL, 4u, attack_poses, attack_counts}
        };
        char ready_name[WOLF_VOX_OBJECT_NAME_LENGTH];
        char attack_name[WOLF_VOX_OBJECT_NAME_LENGTH];
        char path[128];

        (void)snprintf(ready_name, sizeof(ready_name), "%s_ready", animations[weapon]);
        (void)snprintf(attack_name, sizeof(attack_name), "%s_attack", animations[weapon]);
        (void)snprintf(path, sizeof(path), "./wolfvk_test/%s.vox", subjects[weapon]);
        sequences[0].name = ready_name;
        sequences[1].name = attack_name;
        if (!test_vox_write_file(path, sequences, 2u)) {
            return false;
        }
    }
    return true;
}

static void remove_test_weapon_assets(void)
{
    static const char *const subjects[] = {"knife", "pistol", "machinegun", "chain"};
    char path[128];

    for (size_t weapon = 0u; weapon < 4u; ++weapon) {
        (void)snprintf(path, sizeof(path), "./wolfvk_test/%s.vox", subjects[weapon]);
        (void)remove(path);
    }
    (void)SDL_RemovePath("./wolfvk_test");
}

void test_weapon_starts_with_ready_pistol(void)
{
    WolfWeaponState state;

    wolf_weapon_state_initialize(&state);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_PISTOL, state.selected);
    TEST_ASSERT_FALSE(state.firing);
    TEST_ASSERT_EQUAL_UINT8(0u, state.attack_frame);
    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, false, 0.25f));
    TEST_ASSERT_FALSE(state.firing);
}

void test_weapon_pistol_uses_four_six_tic_attack_frames(void)
{
    WolfWeaponState state;
    const float six_tics = 6.0f / 70.0f;

    wolf_weapon_state_initialize(&state);
    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, true, 0.0f));
    TEST_ASSERT_TRUE(state.firing);
    TEST_ASSERT_EQUAL_UINT8(0u, state.attack_frame);

    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, true, six_tics));
    TEST_ASSERT_EQUAL_UINT8(1u, state.attack_frame);
    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, true, six_tics));
    TEST_ASSERT_EQUAL_UINT8(2u, state.attack_frame);
    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, true, six_tics));
    TEST_ASSERT_EQUAL_UINT8(3u, state.attack_frame);
    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, true, six_tics));
    TEST_ASSERT_FALSE(state.firing);
    TEST_ASSERT_EQUAL_UINT8(0u, state.attack_frame);
}

void test_weapon_knife_finishes_the_attack_after_four_frames(void)
{
    WolfWeaponState state;

    wolf_weapon_state_initialize(&state);
    TEST_ASSERT_TRUE(wolf_weapon_state_select(&state, WOLF_WEAPON_KNIFE));
    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, true, 0.0f));
    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, false, 24.0f / 70.0f));
    TEST_ASSERT_FALSE(state.firing);
    TEST_ASSERT_EQUAL_UINT8(0u, state.attack_frame);
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_KNIFE, state.selected);
}

void test_weapon_machinegun_repeats_original_attack_frames_while_held(void)
{
    WolfWeaponState state;
    const float six_tics = 6.0f / 70.0f;

    wolf_weapon_state_initialize(&state);
    TEST_ASSERT_TRUE(wolf_weapon_state_select(&state, WOLF_WEAPON_MACHINEGUN));
    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, true, 18.0f / 70.0f));
    TEST_ASSERT_TRUE(state.firing);
    TEST_ASSERT_EQUAL_UINT8(1u, state.attack_frame);

    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, true, six_tics));
    TEST_ASSERT_EQUAL_UINT8(2u, state.attack_frame);
    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, true, six_tics));
    TEST_ASSERT_EQUAL_UINT8(1u, state.attack_frame);
}

void test_weapon_chaingun_repeats_and_returns_to_ready_when_released(void)
{
    WolfWeaponState state;
    const float six_tics = 6.0f / 70.0f;

    wolf_weapon_state_initialize(&state);
    TEST_ASSERT_TRUE(wolf_weapon_state_select(&state, WOLF_WEAPON_CHAINGUN));
    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, true, 18.0f / 70.0f));
    TEST_ASSERT_TRUE(state.firing);
    TEST_ASSERT_EQUAL_UINT8(1u, state.attack_frame);

    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, false, six_tics));
    TEST_ASSERT_EQUAL_UINT8(2u, state.attack_frame);
    TEST_ASSERT_TRUE(state.firing);
    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, false, 12.0f / 70.0f));
    TEST_ASSERT_FALSE(state.firing);
    TEST_ASSERT_EQUAL_UINT8(0u, state.attack_frame);
}

void test_weapon_selection_resets_animation_and_rejects_invalid_values(void)
{
    WolfWeaponState state;

    wolf_weapon_state_initialize(&state);
    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, true, 12.0f / 70.0f));
    TEST_ASSERT_TRUE(state.firing);
    TEST_ASSERT_TRUE(wolf_weapon_state_select(&state, WOLF_WEAPON_CHAINGUN));
    TEST_ASSERT_EQUAL_INT(WOLF_WEAPON_CHAINGUN, state.selected);
    TEST_ASSERT_FALSE(state.firing);
    TEST_ASSERT_EQUAL_UINT8(0u, state.attack_frame);
    TEST_ASSERT_FALSE(wolf_weapon_state_select(&state, WOLF_WEAPON_COUNT));
    TEST_ASSERT_FALSE(wolf_weapon_state_select(NULL, WOLF_WEAPON_PISTOL));
    TEST_ASSERT_FALSE(wolf_weapon_state_update(NULL, false, 0.0f));
    TEST_ASSERT_FALSE(wolf_weapon_state_update(&state, false, -0.1f));
    TEST_ASSERT_FALSE(wolf_weapon_state_update(&state, false, NAN));
}

void test_weapon_scene_selects_ready_and_attack_poses(void)
{
    WolfWeaponScene scene = {0};
    WolfWeaponState state;
    const WolfWeaponPose *pose;

    wolf_weapon_state_initialize(&state);
    scene.ready[WOLF_WEAPON_PISTOL].available = true;
    scene.ready[WOLF_WEAPON_PISTOL].first_quad = 10u;
    scene.ready[WOLF_WEAPON_PISTOL].quad_count = 2u;
    scene.attack[WOLF_WEAPON_PISTOL][0].available = true;
    scene.attack[WOLF_WEAPON_PISTOL][0].first_quad = 12u;
    scene.attack[WOLF_WEAPON_PISTOL][0].quad_count = 3u;

    pose = wolf_weapon_scene_current_pose(&scene, &state);
    TEST_ASSERT_NOT_NULL(pose);
    TEST_ASSERT_EQUAL_UINT32(10u, pose->first_quad);
    TEST_ASSERT_EQUAL_UINT32(2u, pose->quad_count);

    TEST_ASSERT_TRUE(wolf_weapon_state_update(&state, true, 0.0f));
    pose = wolf_weapon_scene_current_pose(&scene, &state);
    TEST_ASSERT_NOT_NULL(pose);
    TEST_ASSERT_EQUAL_UINT32(12u, pose->first_quad);
    TEST_ASSERT_EQUAL_UINT32(3u, pose->quad_count);

    TEST_ASSERT_NULL(wolf_weapon_scene_current_pose(NULL, &state));
    TEST_ASSERT_NULL(wolf_weapon_scene_current_pose(&scene, NULL));
    wolf_weapon_scene_destroy(&scene);
}

void test_weapon_transform_tracks_camera_and_validates_projection(void)
{
    WolfCamera camera;
    float first_matrix[16];
    float second_matrix[16];
    float attack_matrix[16];
    float pitch_matrix[16];

    wolf_camera_initialize(&camera);
    camera.position[0] = 4.0f;
    camera.position[1] = 1.5f;
    camera.position[2] = -8.0f;
    camera.yaw_radians = 0.0f;
    TEST_ASSERT_TRUE(wolf_weapon_build_view_projection(
        &camera, 16.0f / 9.0f, 0.0f, first_matrix));

    camera.yaw_radians = 0.75f;
    camera.pitch_radians = 0.2f;
    TEST_ASSERT_TRUE(wolf_weapon_build_view_projection(
        &camera, 16.0f / 9.0f, 0.0f, second_matrix));
    TEST_ASSERT_FALSE(memcmp(first_matrix, second_matrix, sizeof(first_matrix)) == 0);

    TEST_ASSERT_TRUE(wolf_weapon_build_view_projection(
        &camera, 16.0f / 9.0f, 0.12f, attack_matrix));
    TEST_ASSERT_TRUE(wolf_weapon_build_view_projection(
        &camera, 16.0f / 9.0f, 0.0f, pitch_matrix));
    TEST_ASSERT_FALSE(memcmp(first_matrix, pitch_matrix, sizeof(first_matrix)) == 0);
    TEST_ASSERT_FALSE(memcmp(second_matrix, attack_matrix, sizeof(second_matrix)) == 0);

    TEST_ASSERT_FALSE(wolf_weapon_build_view_projection(&camera, 0.0f, 0.0f, first_matrix));
    TEST_ASSERT_FALSE(wolf_weapon_build_view_projection(&camera, 1.0f, NAN, first_matrix));
    TEST_ASSERT_FALSE(wolf_weapon_build_view_projection(NULL, 1.0f, 0.0f, first_matrix));
}


void test_weapon_scene_loads_all_twenty_original_named_poses(void)
{
    WolfWeaponScene scene;

    TEST_ASSERT_TRUE(write_test_weapon_assets());
    TEST_ASSERT_TRUE(wolf_weapon_scene_load(&scene, "./wolfvk_test"));
    TEST_ASSERT_EQUAL_UINT32(0u, scene.unavailable_pose_count);
    TEST_ASSERT_EQUAL_UINT32(20u, WOLF_WEAPON_COUNT * 5u);
    TEST_ASSERT_TRUE(scene.quad_count > 0u);
    for (uint32_t weapon = 0u; weapon < WOLF_WEAPON_COUNT; ++weapon) {
        TEST_ASSERT_TRUE(scene.ready[weapon].available);
        TEST_ASSERT_EQUAL_UINT32(6u, scene.ready[weapon].quad_count);
        for (uint32_t frame = 0u; frame < 4u; ++frame) {
            TEST_ASSERT_TRUE(scene.attack[weapon][frame].available);
            TEST_ASSERT_EQUAL_UINT32(6u, scene.attack[weapon][frame].quad_count);
        }
    }
    wolf_weapon_scene_destroy(&scene);
    remove_test_weapon_assets();
}

void test_weapon_scene_keeps_available_subjects_when_one_file_is_missing(void)
{
    WolfWeaponScene scene;

    TEST_ASSERT_TRUE(write_test_weapon_assets());
    TEST_ASSERT_TRUE(remove("./wolfvk_test/chain.vox") == 0);
    TEST_ASSERT_TRUE(wolf_weapon_scene_load(&scene, "./wolfvk_test"));
    TEST_ASSERT_EQUAL_UINT32(5u, scene.unavailable_pose_count);
    TEST_ASSERT_TRUE(scene.ready[WOLF_WEAPON_PISTOL].available);
    TEST_ASSERT_FALSE(scene.ready[WOLF_WEAPON_CHAINGUN].available);
    TEST_ASSERT_TRUE(scene.quad_count > 0u);
    wolf_weapon_scene_destroy(&scene);
    remove_test_weapon_assets();
}

void test_weapon_scene_loader_rejects_invalid_arguments(void)
{
    WolfWeaponScene scene = {0};

    TEST_ASSERT_FALSE(wolf_weapon_scene_load(NULL, "assets/voxels"));
    TEST_ASSERT_FALSE(wolf_weapon_scene_load(&scene, NULL));
    TEST_ASSERT_FALSE(wolf_weapon_scene_load(&scene, ""));
    wolf_weapon_scene_destroy(&scene);
    wolf_weapon_scene_destroy(NULL);
}
