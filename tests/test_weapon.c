#include "unity.h"

#include "wolfvk/weapon.h"

#include <math.h>
#include <string.h>

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

    wolf_camera_initialize(&camera);
    camera.position[0] = 4.0f;
    camera.position[1] = 1.5f;
    camera.position[2] = -8.0f;
    camera.yaw_radians = 0.0f;
    TEST_ASSERT_TRUE(wolf_weapon_build_view_projection(
        &camera, 16.0f / 9.0f, 0.0f, first_matrix));

    camera.yaw_radians = 0.75f;
    TEST_ASSERT_TRUE(wolf_weapon_build_view_projection(
        &camera, 16.0f / 9.0f, 0.0f, second_matrix));
    TEST_ASSERT_FALSE(memcmp(first_matrix, second_matrix, sizeof(first_matrix)) == 0);

    TEST_ASSERT_TRUE(wolf_weapon_build_view_projection(
        &camera, 16.0f / 9.0f, 0.12f, attack_matrix));
    TEST_ASSERT_FALSE(memcmp(second_matrix, attack_matrix, sizeof(second_matrix)) == 0);

    TEST_ASSERT_FALSE(wolf_weapon_build_view_projection(&camera, 0.0f, 0.0f, first_matrix));
    TEST_ASSERT_FALSE(wolf_weapon_build_view_projection(&camera, 1.0f, NAN, first_matrix));
    TEST_ASSERT_FALSE(wolf_weapon_build_view_projection(NULL, 1.0f, 0.0f, first_matrix));
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
