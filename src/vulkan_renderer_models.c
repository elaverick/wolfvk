#include "vulkan_renderer_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Column-major 4x4 product, matching the camera's view-projection layout. */
static void multiply_matrices(const float left[16], const float right[16], float result[16])
{
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;

            for (int component = 0; component < 4; ++component) {
                sum += left[component * 4 + row] * right[column * 4 + component];
            }
            result[column * 4 + row] = sum;
        }
    }
}

void vulkan_renderer_model_matrix(const VulkanModelInstance *instance, float matrix[16])
{
    float cosine;
    float sine;

    if (instance == NULL || matrix == NULL) {
        return;
    }

    /*
     * Turn about the vertical axis so a model facing north (-z) faces the
     * instance's yaw, using the camera's convention that forward is
     * (sin yaw, 0, -cos yaw). Model x and z map to world x cos - z sin and
     * x sin + z cos.
     */
    cosine = cosf(instance->yaw_radians);
    sine = sinf(instance->yaw_radians);
    memset(matrix, 0, 16u * sizeof(float));
    matrix[0] = cosine;
    matrix[2] = sine;
    matrix[5] = 1.0f;
    matrix[8] = -sine;
    matrix[10] = cosine;
    matrix[12] = instance->position[0];
    matrix[13] = instance->position[1];
    matrix[14] = instance->position[2];
    matrix[15] = 1.0f;
}

static void destroy_model_buffers(VulkanRenderer *renderer)
{
    vulkan_renderer_geometry_destroy_buffer(
        renderer, &renderer->model_vertex_buffer, &renderer->model_vertex_buffer_memory);
    vulkan_renderer_geometry_destroy_buffer(
        renderer, &renderer->model_index_buffer, &renderer->model_index_buffer_memory);
    renderer->model_quad_count = 0u;
}

/* Upload the replacement mesh first so a failed upload leaves the previous models drawable. */
bool vulkan_renderer_models_set_meshes(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count)
{
    VkBuffer new_vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory new_vertex_memory = VK_NULL_HANDLE;
    VkBuffer new_index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory new_index_memory = VK_NULL_HANDLE;

    if (renderer == NULL || renderer->device == VK_NULL_HANDLE ||
        (quad_count > 0u && quads == NULL)) {
        return false;
    }

    if (quad_count > 0u &&
        !vulkan_renderer_geometry_create_quad_buffers(
            renderer, quads, quad_count,
            &new_vertex_buffer, &new_vertex_memory,
            &new_index_buffer, &new_index_memory)) {
        return false;
    }

    /* A frame still in flight may be reading the old buffers. */
    if (vkDeviceWaitIdle(renderer->device) != VK_SUCCESS) {
        vulkan_renderer_geometry_destroy_buffer(renderer, &new_vertex_buffer, &new_vertex_memory);
        vulkan_renderer_geometry_destroy_buffer(renderer, &new_index_buffer, &new_index_memory);
        return false;
    }

    destroy_model_buffers(renderer);
    renderer->model_vertex_buffer = new_vertex_buffer;
    renderer->model_vertex_buffer_memory = new_vertex_memory;
    renderer->model_index_buffer = new_index_buffer;
    renderer->model_index_buffer_memory = new_index_memory;
    renderer->model_quad_count = quad_count;

    /* Old instance ranges described the previous mesh. */
    free(renderer->model_instances);
    renderer->model_instances = NULL;
    renderer->model_instance_count = 0u;
    return true;
}

/* Instances are read on the CPU while recording each frame, so replacing them needs no GPU wait. */
bool vulkan_renderer_models_set_instances(
    VulkanRenderer *renderer,
    const VulkanModelInstance *instances,
    uint32_t instance_count)
{
    VulkanModelInstance *copy = NULL;

    if (renderer == NULL || (instance_count > 0u && instances == NULL)) {
        return false;
    }

    for (uint32_t index = 0u; index < instance_count; ++index) {
        const VulkanModelInstance *instance = &instances[index];

        if (instance->quad_count == 0u ||
            instance->first_quad > renderer->model_quad_count ||
            instance->quad_count > renderer->model_quad_count - instance->first_quad ||
            !isfinite(instance->position[0]) || !isfinite(instance->position[1]) ||
            !isfinite(instance->position[2]) || !isfinite(instance->yaw_radians)) {
            return false;
        }
    }

    if (instance_count > 0u) {
        copy = malloc((size_t)instance_count * sizeof(*copy));
        if (copy == NULL) {
            return false;
        }
        memcpy(copy, instances, (size_t)instance_count * sizeof(*copy));
    }

    free(renderer->model_instances);
    renderer->model_instances = copy;
    renderer->model_instance_count = instance_count;
    return true;
}

void vulkan_renderer_models_record(VulkanRenderer *renderer, VkCommandBuffer command_buffer)
{
    VkDeviceSize vertex_buffer_offset = 0;

    if (renderer->model_instance_count == 0u || renderer->model_quad_count == 0u) {
        return;
    }

    vkCmdBindVertexBuffers(
        command_buffer, 0, 1, &renderer->model_vertex_buffer, &vertex_buffer_offset);
    vkCmdBindIndexBuffer(
        command_buffer, renderer->model_index_buffer, 0, VK_INDEX_TYPE_UINT32);

    /*
     * The shader takes one matrix, so each instance pushes its own combined
     * model-view-projection; the level mesh pushes the plain view-projection.
     */
    for (uint32_t index = 0u; index < renderer->model_instance_count; ++index) {
        const VulkanModelInstance *instance = &renderer->model_instances[index];
        float model[16];
        float model_view_projection[16];

        vulkan_renderer_model_matrix(instance, model);
        multiply_matrices(renderer->view_projection, model, model_view_projection);
        vkCmdPushConstants(
            command_buffer, renderer->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
            0, (uint32_t)sizeof(model_view_projection), model_view_projection);
        vkCmdDrawIndexed(
            command_buffer, instance->quad_count * 6u, 1, instance->first_quad * 6u, 0, 0);
    }
}

void vulkan_renderer_models_destroy(VulkanRenderer *renderer)
{
    if (renderer == NULL) {
        return;
    }
    if (renderer->device != VK_NULL_HANDLE) {
        destroy_model_buffers(renderer);
    }
    free(renderer->model_instances);
    renderer->model_instances = NULL;
    renderer->model_instance_count = 0u;
}
