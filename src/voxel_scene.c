#include "wolfvk/voxel_scene.h"

#include "game_palette.h"
#include "wolfvk/vox_file.h"
#include "wolfvk/voxel_mesh.h"
#include "wolfvk/world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void record_problem(WolfVoxelScene *scene, const char *subject, const char *reason)
{
    if (scene->first_problem[0] == '\0') {
        (void)snprintf(scene->first_problem, sizeof(scene->first_problem),
            "%s.vox: %s", subject, reason);
    }
}

static bool pose_matches(
    const WolfVoxelPoseMesh *pose,
    const char *subject,
    const char *animation,
    uint8_t pose_index)
{
    return pose->pose == pose_index &&
        strcmp(pose->subject, subject) == 0 &&
        strcmp(pose->animation, animation) == 0;
}

static const WolfVoxelPoseMesh *find_pose(
    const WolfVoxelScene *scene,
    const WolfLevelObject *object)
{
    for (uint32_t index = 0u; index < scene->pose_count; ++index) {
        if (pose_matches(&scene->poses[index], object->subject, object->animation, object->pose)) {
            return &scene->poses[index];
        }
    }
    return NULL;
}

/* Append a mesh to the scene's shared quad list and record where it starts. */
static bool append_mesh(
    WolfVoxelScene *scene,
    WolfVoxelPoseMesh *pose,
    const VulkanQuad *quads,
    uint32_t quad_count)
{
    VulkanQuad *grown;

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
    scene->quad_count += quad_count;
    return true;
}

/* Mesh every pose belonging to one subject from a single read of its file. */
static bool load_subject(WolfVoxelScene *scene, const char *subject, const char *asset_directory)
{
    char path[512];
    char error_message[160];
    WolfVoxFile file;
    const uint8_t *palette;

    (void)snprintf(path, sizeof(path), "%s/%s.vox", asset_directory, subject);
    if (!wolf_vox_file_load(path, &file, error_message, sizeof(error_message))) {
        record_problem(scene, subject, error_message);
        return true;
    }
    /* Files written by the voxelizer always carry the original palette; others fall back to it. */
    palette = file.has_palette ? file.palette_rgba : wolf_game_palette_rgba;

    for (uint32_t index = 0u; index < scene->pose_count; ++index) {
        WolfVoxelPoseMesh *pose = &scene->poses[index];
        const WolfVoxModel *model;
        VulkanQuad *quads = NULL;
        uint32_t quad_count = 0u;
        bool appended;

        if (strcmp(pose->subject, subject) != 0) {
            continue;
        }
        if (!wolf_vox_file_find_pose(&file, pose->animation, pose->pose, &model)) {
            (void)snprintf(error_message, sizeof(error_message),
                "no pose %u in animation %s", (unsigned int)pose->pose, pose->animation);
            record_problem(scene, subject, error_message);
            continue;
        }
        if (!wolf_voxel_mesh_build(model, palette, &quads, &quad_count)) {
            wolf_vox_file_destroy(&file);
            return false;
        }
        if (quad_count == 0u) {
            /* An empty model has nothing to draw, which is not an error. */
            free(quads);
            continue;
        }
        appended = append_mesh(scene, pose, quads, quad_count);
        free(quads);
        if (!appended) {
            wolf_vox_file_destroy(&file);
            return false;
        }
        pose->available = true;
    }

    wolf_vox_file_destroy(&file);
    return true;
}

bool wolf_voxel_scene_load(
    WolfVoxelScene *scene,
    const WolfLevelObject *objects,
    uint32_t object_count,
    const char *asset_directory)
{
    if (scene != NULL) {
        memset(scene, 0, sizeof(*scene));
    }
    if (scene == NULL || asset_directory == NULL || (object_count > 0u && objects == NULL)) {
        return false;
    }
    if (object_count == 0u) {
        return true;
    }

    /* There can be no more distinct poses than objects. */
    scene->poses = calloc(object_count, sizeof(*scene->poses));
    if (scene->poses == NULL) {
        return false;
    }
    for (uint32_t index = 0u; index < object_count; ++index) {
        const WolfLevelObject *object = &objects[index];
        WolfVoxelPoseMesh *pose;

        if (find_pose(scene, object) != NULL) {
            continue;
        }
        pose = &scene->poses[scene->pose_count++];
        (void)snprintf(pose->subject, sizeof(pose->subject), "%s", object->subject);
        (void)snprintf(pose->animation, sizeof(pose->animation), "%s", object->animation);
        pose->pose = object->pose;
    }

    /* Load each subject the first time one of its poses appears. */
    for (uint32_t index = 0u; index < scene->pose_count; ++index) {
        bool subject_seen = false;

        for (uint32_t earlier = 0u; earlier < index; ++earlier) {
            if (strcmp(scene->poses[earlier].subject, scene->poses[index].subject) == 0) {
                subject_seen = true;
                break;
            }
        }
        if (!subject_seen && !load_subject(scene, scene->poses[index].subject, asset_directory)) {
            wolf_voxel_scene_destroy(scene);
            return false;
        }
    }

    for (uint32_t index = 0u; index < scene->pose_count; ++index) {
        if (!scene->poses[index].available) {
            ++scene->unavailable_pose_count;
        }
    }
    return true;
}

bool wolf_voxel_scene_build_instances(
    const WolfVoxelScene *scene,
    const WolfLevel *level,
    const WolfLevelObject *objects,
    uint32_t object_count,
    VulkanModelInstance **instances,
    uint32_t *instance_count)
{
    VulkanModelInstance *placed;
    uint32_t placed_count = 0u;
    float level_origin_x;
    float level_origin_z;

    if (instances != NULL) {
        *instances = NULL;
    }
    if (instance_count != NULL) {
        *instance_count = 0u;
    }
    if (scene == NULL || level == NULL || instances == NULL || instance_count == NULL ||
        (object_count > 0u && objects == NULL)) {
        return false;
    }
    if (object_count == 0u) {
        return true;
    }

    placed = malloc((size_t)object_count * sizeof(*placed));
    if (placed == NULL) {
        return false;
    }

    /* The level mesh is centred on the world origin, so tile (0, 0) starts half a map away. */
    level_origin_x = -((float)level->width * WOLF_WORLD_TILE_SIZE) * 0.5f;
    level_origin_z = -((float)level->height * WOLF_WORLD_TILE_SIZE) * 0.5f;

    for (uint32_t index = 0u; index < object_count; ++index) {
        const WolfLevelObject *object = &objects[index];
        const WolfVoxelPoseMesh *pose = find_pose(scene, object);
        VulkanModelInstance *instance;

        if (pose == NULL || !pose->available) {
            continue;
        }
        instance = &placed[placed_count++];
        instance->first_quad = pose->first_quad;
        instance->quad_count = pose->quad_count;
        instance->position[0] =
            level_origin_x + ((float)object->tile_x + 0.5f) * WOLF_WORLD_TILE_SIZE;
        instance->position[1] = 0.0f;
        instance->position[2] =
            level_origin_z + ((float)object->tile_y + 0.5f) * WOLF_WORLD_TILE_SIZE;
        instance->yaw_radians = object->yaw_radians;
    }

    if (placed_count == 0u) {
        free(placed);
        return true;
    }
    *instances = placed;
    *instance_count = placed_count;
    return true;
}

void wolf_voxel_scene_destroy(WolfVoxelScene *scene)
{
    if (scene == NULL) {
        return;
    }
    free(scene->poses);
    free(scene->quads);
    memset(scene, 0, sizeof(*scene));
}
