#include "vulkan_renderer_internal.h"

#include <stdlib.h>
#include <string.h>

/* Match Vulkan's allowed memory types with the visibility/coherency needed for direct uploads. */
static bool find_memory_type(
    VulkanRenderer *renderer,
    uint32_t type_filter,
    VkMemoryPropertyFlags required_properties,
    uint32_t *memory_type_index)
{
    VkPhysicalDeviceMemoryProperties memory_properties;

    if (renderer == NULL || memory_type_index == NULL) {
        return false;
    }

    vkGetPhysicalDeviceMemoryProperties(renderer->physical_device, &memory_properties);

    for (uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
        bool type_is_available = (type_filter & (1u << index)) != 0u;
        bool has_required_properties =
            (memory_properties.memoryTypes[index].propertyFlags & required_properties) ==
            required_properties;

        if (type_is_available && has_required_properties) {
            *memory_type_index = index;
            return true;
        }
    }

    return false;
}

/* Buffer and backing memory form one owned resource and must be released together. */
static void destroy_buffer(
    VulkanRenderer *renderer,
    VkBuffer *buffer,
    VkDeviceMemory *memory)
{
    if (*buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(renderer->device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
    }

    if (*memory != VK_NULL_HANDLE) {
        vkFreeMemory(renderer->device, *memory, NULL);
        *memory = VK_NULL_HANDLE;
    }
}

/* Static level geometry is uploaded once, so coherent host-visible buffers keep setup simple. */
static bool create_host_buffer(
    VulkanRenderer *renderer,
    const void *data,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkBuffer *buffer,
    VkDeviceMemory *memory)
{
    VkBufferCreateInfo buffer_info = {0};
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo allocation_info = {0};
    VkResult result;
    uint32_t memory_type_index;
    void *mapped_data = NULL;

    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateBuffer(renderer->device, &buffer_info, NULL, buffer);
    if (result != VK_SUCCESS) {
        return false;
    }

    vkGetBufferMemoryRequirements(renderer->device, *buffer, &memory_requirements);
    if (!find_memory_type(
            renderer, memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &memory_type_index)) {
        destroy_buffer(renderer, buffer, memory);
        return false;
    }

    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = memory_type_index;

    result = vkAllocateMemory(renderer->device, &allocation_info, NULL, memory);
    if (result != VK_SUCCESS) {
        destroy_buffer(renderer, buffer, memory);
        return false;
    }

    result = vkBindBufferMemory(renderer->device, *buffer, *memory, 0);
    if (result != VK_SUCCESS) {
        destroy_buffer(renderer, buffer, memory);
        return false;
    }

    result = vkMapMemory(renderer->device, *memory, 0, size, 0, &mapped_data);
    if (result != VK_SUCCESS) {
        destroy_buffer(renderer, buffer, memory);
        return false;
    }

    memcpy(mapped_data, data, (size_t)size);
    vkUnmapMemory(renderer->device, *memory);
    return true;
}

/* Upload a complete replacement mesh before retiring the previous buffers, preserving valid geometry on failure. */
bool vulkan_renderer_geometry_set_quads(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count)
{
    VkBuffer new_vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory new_vertex_memory = VK_NULL_HANDLE;
    VkBuffer new_index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory new_index_memory = VK_NULL_HANDLE;
    uint32_t *indices = NULL;
    uint32_t index_count;
    size_t vertex_data_size;
    size_t index_data_size;

    if (renderer == NULL || renderer->device == VK_NULL_HANDLE ||
        (quad_count > 0u && quads == NULL) ||
        quad_count > UINT32_MAX / 6u ||
        (size_t)quad_count > SIZE_MAX / (4u * sizeof(VulkanVertex))) {
        return false;
    }

    if (quad_count == 0u) {
        if (vkDeviceWaitIdle(renderer->device) != VK_SUCCESS) {
            return false;
        }
        vulkan_renderer_geometry_destroy(renderer);
        return true;
    }

    index_count = quad_count * 6u;
    vertex_data_size = (size_t)quad_count * 4u * sizeof(VulkanVertex);
    index_data_size = (size_t)index_count * sizeof(*indices);
    indices = malloc(index_data_size);
    if (indices == NULL) {
        return false;
    }

    /* Two indexed triangles share each quad's four vertices. */
    for (uint32_t quad_index = 0; quad_index < quad_count; ++quad_index) {
        uint32_t vertex_base = quad_index * 4u;
        uint32_t index_base = quad_index * 6u;

        indices[index_base] = vertex_base;
        indices[index_base + 1u] = vertex_base + 1u;
        indices[index_base + 2u] = vertex_base + 2u;
        indices[index_base + 3u] = vertex_base + 2u;
        indices[index_base + 4u] = vertex_base + 3u;
        indices[index_base + 5u] = vertex_base;
    }

    if (!create_host_buffer(
            renderer, &quads[0].vertices[0], (VkDeviceSize)vertex_data_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            &new_vertex_buffer, &new_vertex_memory) ||
        !create_host_buffer(
            renderer, indices, (VkDeviceSize)index_data_size,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            &new_index_buffer, &new_index_memory)) {
        free(indices);
        destroy_buffer(renderer, &new_vertex_buffer, &new_vertex_memory);
        destroy_buffer(renderer, &new_index_buffer, &new_index_memory);
        return false;
    }

    free(indices);

    if (vkDeviceWaitIdle(renderer->device) != VK_SUCCESS) {
        destroy_buffer(renderer, &new_vertex_buffer, &new_vertex_memory);
        destroy_buffer(renderer, &new_index_buffer, &new_index_memory);
        return false;
    }

    vulkan_renderer_geometry_destroy(renderer);
    renderer->vertex_buffer = new_vertex_buffer;
    renderer->vertex_buffer_memory = new_vertex_memory;
    renderer->index_buffer = new_index_buffer;
    renderer->index_buffer_memory = new_index_memory;
    renderer->index_count = index_count;
    return true;
}

/* Clear both geometry buffers and the draw count so teardown is safe to call more than once. */
void vulkan_renderer_geometry_destroy(VulkanRenderer *renderer)
{
    if (renderer == NULL || renderer->device == VK_NULL_HANDLE) {
        return;
    }

    destroy_buffer(
        renderer, &renderer->vertex_buffer, &renderer->vertex_buffer_memory);
    destroy_buffer(
        renderer, &renderer->index_buffer, &renderer->index_buffer_memory);
    renderer->index_count = 0u;
}
