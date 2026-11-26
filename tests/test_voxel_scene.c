#include "unity.h"

#include "test_vox_builder.h"
#include "wolfvk/voxel_scene.h"
#include "wolfvk/world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_SUBJECT "wolfvk_test_actor"
#define TEST_MISSING_SUBJECT "wolfvk_test_missing"

static const WolfVoxVoxel one_voxel[] = {{32u, 32u, 4u, 2u}};
static const WolfVoxVoxel two_voxels[] = {{10u, 10u, 4u, 2u}, {50u, 50u, 4u, 3u}};

/* A subject with a one-voxel standing pose (one cube: 6 faces) and a two-cube walk pose (12 faces). */
static void write_test_subject(void)
{
    static const WolfVoxVoxel *const standing_poses[] = {one_voxel};
    static const uint32_t standing_counts[] = {1u};
    static const WolfVoxVoxel *const walking_poses[] = {two_voxels};
    static const uint32_t walking_counts[] = {2u};
    const TestVoxAnimation animations[] = {
        {"actor_s", 1u, standing_poses, standing_counts},
        {"actor_w", 1u, walking_poses, walking_counts},
    };

    TEST_ASSERT_TRUE(test_vox_write_file("./" TEST_SUBJECT ".vox", animations, 2u));
}

static WolfLevelObject make_object(
    const char *subject,
    const char *animation,
    uint8_t pose,
    uint16_t tile_x,
    uint16_t tile_y,
    float yaw_radians)
{
    WolfLevelObject object;

    memset(&object, 0, sizeof(object));
    (void)snprintf(object.subject, sizeof(object.subject), "%s", subject);
    (void)snprintf(object.animation, sizeof(object.animation), "%s", animation);
    object.pose = pose;
    object.tile_x = tile_x;
    object.tile_y = tile_y;
    object.yaw_radians = yaw_radians;
    return object;
}

void test_voxel_scene_meshes_each_shared_pose_once(void)
{
    WolfLevelObject objects[4];
    WolfVoxelScene scene;

    write_test_subject();
    objects[0] = make_object(TEST_SUBJECT, "actor_s", 0u, 1u, 1u, 0.0f);
    objects[1] = make_object(TEST_SUBJECT, "actor_w", 0u, 2u, 1u, 0.0f);
    objects[2] = make_object(TEST_SUBJECT, "actor_s", 0u, 3u, 1u, 1.0f);
    objects[3] = make_object(TEST_SUBJECT, "actor_s", 0u, 4u, 1u, 2.0f);

    TEST_ASSERT_TRUE(wolf_voxel_scene_load(&scene, objects, 4u, "."));
    TEST_ASSERT_EQUAL_UINT32(2u, scene.pose_count);
    TEST_ASSERT_EQUAL_UINT32(0u, scene.unavailable_pose_count);
    TEST_ASSERT_TRUE(scene.poses[0].available);
    TEST_ASSERT_TRUE(scene.poses[1].available);
    /* Floor faces are omitted: a cube resting at z = 4 keeps all six, so 6 + 12 quads. */
    TEST_ASSERT_EQUAL_UINT32(6u, scene.poses[0].quad_count);
    TEST_ASSERT_EQUAL_UINT32(12u, scene.poses[1].quad_count);
    TEST_ASSERT_EQUAL_UINT32(6u, scene.poses[1].first_quad);
    TEST_ASSERT_EQUAL_UINT32(18u, scene.quad_count);
    TEST_ASSERT_NOT_NULL(scene.quads);

    wolf_voxel_scene_destroy(&scene);
    TEST_ASSERT_NULL(scene.quads);
    (void)remove("./" TEST_SUBJECT ".vox");
}

void test_voxel_scene_marks_missing_files_and_poses_unavailable(void)
{
    WolfLevelObject objects[3];
    WolfVoxelScene scene;

    write_test_subject();
    objects[0] = make_object(TEST_MISSING_SUBJECT, "missing_s", 0u, 1u, 1u, 0.0f);
    objects[1] = make_object(TEST_SUBJECT, "actor_s", 3u, 2u, 1u, 0.0f);
    objects[2] = make_object(TEST_SUBJECT, "actor_s", 0u, 3u, 1u, 0.0f);

    /* Missing assets never stop the level from loading. */
    TEST_ASSERT_TRUE(wolf_voxel_scene_load(&scene, objects, 3u, "."));
    TEST_ASSERT_EQUAL_UINT32(3u, scene.pose_count);
    TEST_ASSERT_EQUAL_UINT32(2u, scene.unavailable_pose_count);
    TEST_ASSERT_FALSE(scene.poses[0].available);
    TEST_ASSERT_FALSE(scene.poses[1].available);
    TEST_ASSERT_TRUE(scene.poses[2].available);
    TEST_ASSERT_TRUE(strstr(scene.first_problem, TEST_MISSING_SUBJECT ".vox") != NULL);

    wolf_voxel_scene_destroy(&scene);
    (void)remove("./" TEST_SUBJECT ".vox");
}

void test_voxel_scene_places_models_on_their_tiles(void)
{
    WolfLevel *level = calloc(1u, sizeof(*level));
    WolfLevelObject objects[3];
    WolfVoxelScene scene;
    VulkanModelInstance *instances = NULL;
    uint32_t instance_count = 0u;
    const float half_map = (float)WOLF_LEVEL_PLANE_WIDTH * WOLF_WORLD_TILE_SIZE * 0.5f;

    TEST_ASSERT_NOT_NULL(level);
    level->width = WOLF_LEVEL_PLANE_WIDTH;
    level->height = WOLF_LEVEL_PLANE_HEIGHT;
    write_test_subject();
    objects[0] = make_object(TEST_SUBJECT, "actor_s", 0u, 0u, 0u, 0.0f);
    objects[1] = make_object(TEST_MISSING_SUBJECT, "missing_s", 0u, 5u, 5u, 0.0f);
    objects[2] = make_object(TEST_SUBJECT, "actor_w", 0u, 29u, 57u, 1.5f);

    TEST_ASSERT_TRUE(wolf_voxel_scene_load(&scene, objects, 3u, "."));
    TEST_ASSERT_TRUE(wolf_voxel_scene_build_instances(
        &scene, level, objects, 3u, &instances, &instance_count));

    /* The object without a model is skipped; the others stand at their tile centres on the floor. */
    TEST_ASSERT_EQUAL_UINT32(2u, instance_count);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -half_map + 0.5f * WOLF_WORLD_TILE_SIZE, instances[0].position[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, instances[0].position[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -half_map + 0.5f * WOLF_WORLD_TILE_SIZE, instances[0].position[2]);
    TEST_ASSERT_EQUAL_UINT32(scene.poses[0].first_quad, instances[0].first_quad);
    TEST_ASSERT_EQUAL_UINT32(scene.poses[0].quad_count, instances[0].quad_count);

    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -half_map + 29.5f * WOLF_WORLD_TILE_SIZE, instances[1].position[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -half_map + 57.5f * WOLF_WORLD_TILE_SIZE, instances[1].position[2]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.5f, instances[1].yaw_radians);
    TEST_ASSERT_EQUAL_UINT32(scene.poses[2].first_quad, instances[1].first_quad);

    free(instances);
    wolf_voxel_scene_destroy(&scene);
    free(level);
    (void)remove("./" TEST_SUBJECT ".vox");
}


void test_voxel_scene_preloads_and_selects_runtime_actor_walk_poses(void)
{
    static const WolfVoxVoxel pose_a[] = {{32u, 32u, 4u, 2u}};
    static const WolfVoxVoxel pose_b[] = {{10u, 10u, 4u, 2u}, {50u, 50u, 4u, 3u}};
    static const WolfVoxVoxel *const poses[] = {pose_a, pose_a, pose_b, pose_a};
    static const uint32_t pose_counts[] = {1u, 1u, 2u, 1u};
    const TestVoxAnimation animations[] = {
        {"grd_w", 4u, poses, pose_counts},
        {"grd_shoot", 3u, poses, pose_counts},
        {"grd_pain", 2u, poses, pose_counts},
        {"grd_die", 3u, poses, pose_counts},
        {"grd_dead", 1u, poses, pose_counts}
    };
    WolfLevelObject object = make_object("grd", "grd_w", 0u, 4u, 5u, 0.5f);
    WolfLevel *level = calloc(1u, sizeof(*level));
    WolfVoxelScene scene;
    WolfActor actor = {0};
    VulkanModelInstance *actor_instances = NULL;
    VulkanModelInstance *static_instances = NULL;
    uint32_t actor_count = 0u;
    uint32_t static_count = 99u;
    const WolfVoxelPoseMesh *selected_pose = NULL;

    TEST_ASSERT_NOT_NULL(level);
    level->width = WOLF_LEVEL_PLANE_WIDTH;
    level->height = WOLF_LEVEL_PLANE_HEIGHT;
    TEST_ASSERT_TRUE(test_vox_write_file("./grd.vox", animations, 5u));
    TEST_ASSERT_TRUE(wolf_voxel_scene_load(&scene, &object, 1u, "."));
    TEST_ASSERT_EQUAL_UINT32(13u, scene.pose_count);
    TEST_ASSERT_TRUE(wolf_voxel_scene_build_instances(
        &scene, level, &object, 1u, &static_instances, &static_count));
    TEST_ASSERT_EQUAL_UINT32(0u, static_count);
    TEST_ASSERT_NULL(static_instances);

    actor.active = true;
    actor.family = WOLF_ACTOR_GUARD;
    actor.position_x = 12.5f;
    actor.position_z = -8.0f;
    actor.yaw_radians = 1.25f;
    actor.pose = 2u;
    (void)snprintf(actor.animation, sizeof(actor.animation), "%s", "grd_w");
    TEST_ASSERT_TRUE(wolf_voxel_scene_build_actor_instances(
        &scene, &actor, 1u, &actor_instances, &actor_count));
    TEST_ASSERT_EQUAL_UINT32(1u, actor_count);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 12.5f, actor_instances[0].position[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -8.0f, actor_instances[0].position[2]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.25f, actor_instances[0].yaw_radians);
    for (uint32_t index = 0u; index < scene.pose_count; ++index) {
        if (scene.poses[index].pose == 2u &&
            strcmp(scene.poses[index].animation, "grd_w") == 0) {
            selected_pose = &scene.poses[index];
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(selected_pose);
    TEST_ASSERT_EQUAL_UINT32(selected_pose->first_quad, actor_instances[0].first_quad);
    TEST_ASSERT_EQUAL_UINT32(selected_pose->quad_count, actor_instances[0].quad_count);

    free(actor_instances);
    wolf_voxel_scene_destroy(&scene);
    free(level);
    (void)remove("./grd.vox");
}

void test_voxel_scene_rejects_invalid_arguments(void)
{
    WolfLevel *level = calloc(1u, sizeof(*level));
    WolfLevelObject object = make_object(TEST_SUBJECT, "actor_s", 0u, 0u, 0u, 0.0f);
    WolfVoxelScene scene;
    VulkanModelInstance *instances = NULL;
    uint32_t instance_count = 5u;

    TEST_ASSERT_NOT_NULL(level);
    TEST_ASSERT_FALSE(wolf_voxel_scene_load(NULL, &object, 1u, "."));
    TEST_ASSERT_FALSE(wolf_voxel_scene_load(&scene, NULL, 1u, "."));
    TEST_ASSERT_FALSE(wolf_voxel_scene_load(&scene, &object, 1u, NULL));

    /* A level without objects loads nothing and places nothing. */
    TEST_ASSERT_TRUE(wolf_voxel_scene_load(&scene, NULL, 0u, "."));
    TEST_ASSERT_EQUAL_UINT32(0u, scene.pose_count);
    TEST_ASSERT_TRUE(wolf_voxel_scene_build_instances(&scene, level, NULL, 0u, &instances, &instance_count));
    TEST_ASSERT_EQUAL_UINT32(0u, instance_count);
    TEST_ASSERT_NULL(instances);
    TEST_ASSERT_FALSE(wolf_voxel_scene_build_instances(&scene, NULL, NULL, 0u, &instances, &instance_count));
    TEST_ASSERT_FALSE(wolf_voxel_scene_build_instances(&scene, level, &object, 1u, NULL, &instance_count));
    wolf_voxel_scene_destroy(&scene);
    wolf_voxel_scene_destroy(NULL);
    free(level);
}


void test_voxel_scene_preloads_officer_and_ss_combat_poses(void)
{
    static const WolfVoxVoxel *const poses[] = {
        one_voxel, one_voxel, two_voxels, one_voxel
    };
    static const uint32_t pose_counts[] = {1u, 1u, 2u, 1u};
    const TestVoxAnimation officer_animations[] = {
        {"ofc_w", 4u, poses, pose_counts},
        {"ofc_shoot", 3u, poses, pose_counts},
        {"ofc_pain", 2u, poses, pose_counts},
        {"ofc_die", 4u, poses, pose_counts},
        {"ofc_dead", 1u, poses, pose_counts}
    };
    const TestVoxAnimation ss_animations[] = {
        {"ss_w", 4u, poses, pose_counts},
        {"ss_shoot", 3u, poses, pose_counts},
        {"ss_pain", 2u, poses, pose_counts},
        {"ss_die", 3u, poses, pose_counts},
        {"ss_dead", 1u, poses, pose_counts}
    };
    WolfLevelObject objects[2];
    WolfVoxelScene scene;
    const WolfVoxelPoseMesh *officer_last_death_pose = NULL;
    const WolfVoxelPoseMesh *ss_last_shot_pose = NULL;

    objects[0] = make_object("ofc", "ofc_w", 0u, 2u, 2u, 0.0f);
    objects[1] = make_object("ss", "ss_w", 0u, 3u, 2u, 0.0f);
    TEST_ASSERT_TRUE(test_vox_write_file("./ofc.vox", officer_animations, 5u));
    TEST_ASSERT_TRUE(test_vox_write_file("./ss.vox", ss_animations, 5u));

    TEST_ASSERT_TRUE(wolf_voxel_scene_load(&scene, objects, 2u, "."));
    TEST_ASSERT_EQUAL_UINT32(27u, scene.pose_count);
    TEST_ASSERT_EQUAL_UINT32(0u, scene.unavailable_pose_count);
    for (uint32_t index = 0u; index < scene.pose_count; ++index) {
        if (strcmp(scene.poses[index].subject, "ofc") == 0 &&
            strcmp(scene.poses[index].animation, "ofc_die") == 0 &&
            scene.poses[index].pose == 3u) {
            officer_last_death_pose = &scene.poses[index];
        }
        if (strcmp(scene.poses[index].subject, "ss") == 0 &&
            strcmp(scene.poses[index].animation, "ss_shoot") == 0 &&
            scene.poses[index].pose == 2u) {
            ss_last_shot_pose = &scene.poses[index];
        }
    }
    TEST_ASSERT_NOT_NULL(officer_last_death_pose);
    TEST_ASSERT_TRUE(officer_last_death_pose->available);
    TEST_ASSERT_NOT_NULL(ss_last_shot_pose);
    TEST_ASSERT_TRUE(ss_last_shot_pose->available);

    wolf_voxel_scene_destroy(&scene);
    (void)remove("./ofc.vox");
    (void)remove("./ss.vox");
}
