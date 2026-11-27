#include "wolfvk/voxel_scene.h"
#include "wolfvk/actor.h"

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

static bool is_runtime_humanoid(const WolfLevelObject *object)
{
    size_t length;

    if (strcmp(object->subject, "grd") != 0 &&
        strcmp(object->subject, "ofc") != 0 &&
        strcmp(object->subject, "ss") != 0 &&
        strcmp(object->subject, "mut") != 0) {
        return false;
    }
    length = strlen(object->animation);
    return length >= 2u && object->animation[length - 2u] == '_' &&
        (object->animation[length - 1u] == 's' || object->animation[length - 1u] == 'w');
}

static const char *actor_subject(WolfActorFamily family)
{
    switch (family) {
    case WOLF_ACTOR_GUARD:
        return "grd";
    case WOLF_ACTOR_OFFICER:
        return "ofc";
    case WOLF_ACTOR_SS:
        return "ss";
    case WOLF_ACTOR_MUTANT:
        return "mut";
    default:
        return NULL;
    }
}

static const WolfVoxelPoseMesh *find_actor_pose(
    const WolfVoxelScene *scene,
    const WolfActor *actor)
{
    const char *subject = actor_subject(actor->family);

    if (subject == NULL) {
        return NULL;
    }
    for (uint32_t index = 0u; index < scene->pose_count; ++index) {
        if (scene->poses[index].pose == actor->pose &&
            strcmp(scene->poses[index].subject, subject) == 0 &&
            strcmp(scene->poses[index].animation, actor->animation) == 0) {
            return &scene->poses[index];
        }
    }
    return NULL;
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

    /*
     * Each humanoid may need four walking poses in addition to its spawn
     * pose. Reserve five entries per object; duplicates are collapsed below.
     */
    if ((size_t)object_count > SIZE_MAX / 14u / sizeof(*scene->poses)) {
        return false;
    }
    scene->poses = calloc((size_t)object_count * 14u, sizeof(*scene->poses));
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

    /*
     * Preload every pose needed by the complete humanoid state chains.
     * Missing optional poses remain unavailable and are reported once; state
     * selection can therefore never reference an unmeshed animation frame.
     */
    for (uint32_t index = 0u; index < object_count; ++index) {
        const WolfLevelObject *object = &objects[index];
        static const struct {
            const char *suffix;
            uint8_t pose_count;
        } actor_animations[] = {
            {"_w", 4u}, {"_shoot", 3u}, {"_pain", 2u}, {"_die", 3u}, {"_dead", 1u}
        };

        if (is_runtime_humanoid(object)) {
            for (size_t animation_index = 0u;
                 animation_index < sizeof(actor_animations) / sizeof(actor_animations[0]);
                 ++animation_index) {
                char animation_name[WOLF_LEVEL_OBJECT_NAME_LENGTH];

                (void)snprintf(animation_name, sizeof(animation_name), "%s%s",
                    object->subject, actor_animations[animation_index].suffix);
                uint8_t required_pose_count = actor_animations[animation_index].pose_count;

                if ((strcmp(object->subject, "ofc") == 0 ||
                     strcmp(object->subject, "mut") == 0) &&
                    strcmp(actor_animations[animation_index].suffix, "_die") == 0) {
                    required_pose_count = 4u;
                }
                for (uint8_t pose_index = 0u;
                     pose_index < required_pose_count;
                     ++pose_index) {
                    bool already_present = false;

                    for (uint32_t existing = 0u; existing < scene->pose_count; ++existing) {
                        if (scene->poses[existing].pose == pose_index &&
                            strcmp(scene->poses[existing].subject, object->subject) == 0 &&
                            strcmp(scene->poses[existing].animation, animation_name) == 0) {
                            already_present = true;
                            break;
                        }
                    }
                    if (already_present) {
                        continue;
                    }
                    {
                        WolfVoxelPoseMesh *pose = &scene->poses[scene->pose_count++];
                        (void)snprintf(pose->subject, sizeof(pose->subject), "%s", object->subject);
                        (void)snprintf(pose->animation, sizeof(pose->animation), "%s", animation_name);
                        pose->pose = pose_index;
                    }
                }
            }
        }
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
        const WolfVoxelPoseMesh *pose;
        VulkanModelInstance *instance;

        if (is_runtime_humanoid(object)) {
            continue;
        }
        pose = find_pose(scene, object);
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

 
bool wolf_voxel_scene_build_actor_instances(
    const WolfVoxelScene *scene,
    const WolfActor *actors,
    uint32_t actor_count,
    VulkanModelInstance **instances,
    uint32_t *instance_count)
{
    VulkanModelInstance *placed;
    uint32_t placed_count = 0u;

    if (instances != NULL) {
        *instances = NULL;
    }
    if (instance_count != NULL) {
        *instance_count = 0u;
    }
    if (scene == NULL || instances == NULL || instance_count == NULL ||
        (actor_count > 0u && actors == NULL)) {
        return false;
    }
    if (actor_count == 0u) {
        return true;
    }

    placed = malloc((size_t)actor_count * sizeof(*placed));
    if (placed == NULL) {
        return false;
    }
    for (uint32_t index = 0u; index < actor_count; ++index) {
        const WolfActor *actor = &actors[index];
        const WolfVoxelPoseMesh *pose;
        VulkanModelInstance *instance;

        if (!actor->active) {
            continue;
        }
        pose = find_actor_pose(scene, actor);
        if (pose == NULL || !pose->available) {
            continue;
        }
        instance = &placed[placed_count++];
        instance->first_quad = pose->first_quad;
        instance->quad_count = pose->quad_count;
        instance->position[0] = actor->position_x;
        instance->position[1] = 0.0f;
        instance->position[2] = actor->position_z;
        instance->yaw_radians = actor->yaw_radians;
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
