#include "vulkan_renderer_internal.h"

#include <string.h>

/* Upload every weapon frame once; the active attack pose changes only its draw range. */
bool vulkan_renderer_weapon_set_meshes(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count)
{
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory index_memory = VK_NULL_HANDLE;

    if (renderer == NULL || renderer->device == VK_NULL_HANDLE ||
        (quad_count > 0u && quads == NULL)) {
        return false;
    }
    if (quad_count > 0u && !vulkan_renderer_geometry_create_quad_buffers(
            renderer, quads, quad_count,
            &vertex_buffer, &vertex_memory, &index_buffer, &index_memory)) {
        return false;
    }
    if (vkDeviceWaitIdle(renderer->device) != VK_SUCCESS) {
        vulkan_renderer_geometry_destroy_buffer(renderer, &vertex_buffer, &vertex_memory);
        vulkan_renderer_geometry_destroy_buffer(renderer, &index_buffer, &index_memory);
        return false;
    }

    vulkan_renderer_geometry_destroy_buffer(
        renderer, &renderer->weapon_vertex_buffer, &renderer->weapon_vertex_buffer_memory);
    vulkan_renderer_geometry_destroy_buffer(
        renderer, &renderer->weapon_index_buffer, &renderer->weapon_index_buffer_memory);
    renderer->weapon_vertex_buffer = vertex_buffer;
    renderer->weapon_vertex_buffer_memory = vertex_memory;
    renderer->weapon_index_buffer = index_buffer;
    renderer->weapon_index_buffer_memory = index_memory;
    renderer->weapon_quad_count = quad_count;
    renderer->weapon_first_quad = 0u;
    renderer->weapon_draw_quad_count = 0u;
    return true;
}

/* Reject an invalid pose range before recording any indexed draw commands. */
bool vulkan_renderer_weapon_set_pose(
    VulkanRenderer *renderer,
    uint32_t first_quad,
    uint32_t quad_count,
    const float view_projection[16])
{
    if (renderer == NULL || view_projection == NULL || quad_count == 0u ||
        first_quad > renderer->weapon_quad_count ||
        quad_count > renderer->weapon_quad_count - first_quad) {
        return false;
    }
    memcpy(renderer->weapon_view_projection, view_projection,
        sizeof(renderer->weapon_view_projection));
    renderer->weapon_first_quad = first_quad;
    renderer->weapon_draw_quad_count = quad_count;
    return true;
}

/* Draw the currently selected pose over the world so walls never obscure the held weapon. */
void vulkan_renderer_weapon_record(VulkanRenderer *renderer, VkCommandBuffer command_buffer)
{
    VkDeviceSize vertex_buffer_offset = 0u;

    if (renderer == NULL || command_buffer == VK_NULL_HANDLE ||
        renderer->weapon_pipeline == VK_NULL_HANDLE ||
        renderer->weapon_draw_quad_count == 0u ||
        renderer->weapon_vertex_buffer == VK_NULL_HANDLE ||
        renderer->weapon_index_buffer == VK_NULL_HANDLE) {
        return;
    }

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->weapon_pipeline);
    vkCmdBindVertexBuffers(
        command_buffer, 0u, 1u, &renderer->weapon_vertex_buffer, &vertex_buffer_offset);
    vkCmdBindIndexBuffer(
        command_buffer, renderer->weapon_index_buffer, 0u, VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(
        command_buffer, renderer->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
        0u, (uint32_t)sizeof(renderer->weapon_view_projection),
        renderer->weapon_view_projection);
    vkCmdDrawIndexed(
        command_buffer, renderer->weapon_draw_quad_count * 6u, 1u,
        renderer->weapon_first_quad * 6u, 0, 0u);
}

void vulkan_renderer_weapon_destroy(VulkanRenderer *renderer)
{
    if (renderer == NULL || renderer->device == VK_NULL_HANDLE) {
        return;
    }
    vulkan_renderer_geometry_destroy_buffer(
        renderer, &renderer->weapon_vertex_buffer, &renderer->weapon_vertex_buffer_memory);
    vulkan_renderer_geometry_destroy_buffer(
        renderer, &renderer->weapon_index_buffer, &renderer->weapon_index_buffer_memory);
    renderer->weapon_quad_count = 0u;
    renderer->weapon_draw_quad_count = 0u;
}
