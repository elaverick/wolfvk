#include "vulkan_renderer_internal.h"

#include "wolfvk/wall_textures.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TemporaryWallTexture {
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    VkImage image;
    VkDeviceMemory image_memory;
    VkImageView image_view;
    VkSampler sampler;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
} TemporaryWallTexture;

/* Find a memory type that satisfies both the buffer/image requirements and access pattern. */
static bool find_memory_type(
    VulkanRenderer *renderer,
    uint32_t type_filter,
    VkMemoryPropertyFlags required_properties,
    uint32_t *memory_type_index)
{
    VkPhysicalDeviceMemoryProperties properties;

    vkGetPhysicalDeviceMemoryProperties(renderer->physical_device, &properties);
    for (uint32_t index = 0u; index < properties.memoryTypeCount; ++index) {
        bool type_is_available = (type_filter & (1u << index)) != 0u;
        bool has_required_properties =
            (properties.memoryTypes[index].propertyFlags & required_properties) ==
            required_properties;

        if (type_is_available && has_required_properties) {
            *memory_type_index = index;
            return true;
        }
    }

    return false;
}

/* Destroy temporary upload resources in reverse dependency order, including partial creations. */
static void destroy_temporary_wall_texture(
    VulkanRenderer *renderer,
    TemporaryWallTexture *texture)
{
    if (texture->descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(renderer->device, texture->descriptor_pool, NULL);
        texture->descriptor_pool = VK_NULL_HANDLE;
        texture->descriptor_set = VK_NULL_HANDLE;
    }
    if (texture->sampler != VK_NULL_HANDLE) {
        vkDestroySampler(renderer->device, texture->sampler, NULL);
        texture->sampler = VK_NULL_HANDLE;
    }
    if (texture->image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(renderer->device, texture->image_view, NULL);
        texture->image_view = VK_NULL_HANDLE;
    }
    if (texture->image != VK_NULL_HANDLE) {
        vkDestroyImage(renderer->device, texture->image, NULL);
        texture->image = VK_NULL_HANDLE;
    }
    if (texture->image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(renderer->device, texture->image_memory, NULL);
        texture->image_memory = VK_NULL_HANDLE;
    }
    if (texture->staging_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(renderer->device, texture->staging_buffer, NULL);
        texture->staging_buffer = VK_NULL_HANDLE;
    }
    if (texture->staging_memory != VK_NULL_HANDLE) {
        vkFreeMemory(renderer->device, texture->staging_memory, NULL);
        texture->staging_memory = VK_NULL_HANDLE;
    }
}

/* Allocate one descriptor set layout shared by every material page in the wall texture array. */
bool vulkan_renderer_texture_layout_create(VulkanRenderer *renderer)
{
    VkDescriptorSetLayoutBinding texture_binding = {0};
    VkDescriptorSetLayoutCreateInfo layout_info = {0};
    VkResult result;

    if (renderer == NULL || renderer->device == VK_NULL_HANDLE ||
        renderer->wall_texture_set_layout != VK_NULL_HANDLE) {
        return false;
    }

    texture_binding.binding = 0u;
    texture_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    texture_binding.descriptorCount = 1u;
    texture_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1u;
    layout_info.pBindings = &texture_binding;

    result = vkCreateDescriptorSetLayout(
        renderer->device, &layout_info, NULL, &renderer->wall_texture_set_layout);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture descriptor layout creation failed (%d).\n", result);
        return false;
    }

    return true;
}

/* Create a one-shot transfer command that makes all page layers visible to fragment sampling. */
static bool upload_wall_texture_layers(
    VulkanRenderer *renderer,
    TemporaryWallTexture *texture,
    uint32_t page_count)
{
    VkCommandBufferAllocateInfo allocation_info = {0};
    VkCommandBufferBeginInfo begin_info = {0};
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkImageMemoryBarrier upload_barrier = {0};
    VkBufferImageCopy copy_region = {0};
    VkImageMemoryBarrier sample_barrier = {0};
    VkSubmitInfo submit_info = {0};
    VkResult result;
    bool uploaded = false;

    allocation_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation_info.commandPool = renderer->command_pool;
    allocation_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation_info.commandBufferCount = 1u;

    result = vkAllocateCommandBuffers(
        renderer->device, &allocation_info, &command_buffer);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture command buffer allocation failed (%d).\n", result);
        return false;
    }

    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture command buffer begin failed (%d).\n", result);
        goto cleanup;
    }

    upload_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    upload_barrier.srcAccessMask = 0u;
    upload_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    upload_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    upload_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    upload_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    upload_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    upload_barrier.image = texture->image;
    upload_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    upload_barrier.subresourceRange.baseMipLevel = 0u;
    upload_barrier.subresourceRange.levelCount = 1u;
    upload_barrier.subresourceRange.baseArrayLayer = 0u;
    upload_barrier.subresourceRange.layerCount = page_count;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0u,
        0u, NULL,
        0u, NULL,
        1u, &upload_barrier);

    copy_region.bufferOffset = 0u;
    copy_region.bufferRowLength = 0u;
    copy_region.bufferImageHeight = 0u;
    copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.mipLevel = 0u;
    copy_region.imageSubresource.baseArrayLayer = 0u;
    copy_region.imageSubresource.layerCount = page_count;
    copy_region.imageOffset = (VkOffset3D){0, 0, 0};
    copy_region.imageExtent = (VkExtent3D){
        WOLF_WALL_TEXTURE_WIDTH, WOLF_WALL_TEXTURE_HEIGHT, 1u
    };

    vkCmdCopyBufferToImage(
        command_buffer,
        texture->staging_buffer,
        texture->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1u,
        &copy_region);

    sample_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    sample_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    sample_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sample_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    sample_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    sample_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    sample_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    sample_barrier.image = texture->image;
    sample_barrier.subresourceRange = upload_barrier.subresourceRange;

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0u,
        0u, NULL,
        0u, NULL,
        1u, &sample_barrier);

    result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture command buffer end failed (%d).\n", result);
        goto cleanup;
    }

    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1u;
    submit_info.pCommandBuffers = &command_buffer;

    result = vkQueueSubmit(renderer->graphics_queue, 1u, &submit_info, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture upload submission failed (%d).\n", result);
        goto cleanup;
    }

    result = vkQueueWaitIdle(renderer->graphics_queue);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Waiting for the wall texture upload failed (%d).\n", result);
        goto cleanup;
    }

    uploaded = true;

cleanup:
    vkFreeCommandBuffers(renderer->device, renderer->command_pool, 1u, &command_buffer);
    return uploaded;
}

/* Build a complete replacement texture array before retiring the currently sampled one. */
bool vulkan_renderer_texture_set(
    VulkanRenderer *renderer,
    const uint8_t *rgba_pages,
    uint32_t page_count)
{
    TemporaryWallTexture replacement = {0};
    VkPhysicalDeviceProperties device_properties;
    VkBufferCreateInfo buffer_info = {0};
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo allocation_info = {0};
    VkImageCreateInfo image_info = {0};
    VkImageViewCreateInfo view_info = {0};
    VkSamplerCreateInfo sampler_info = {0};
    VkDescriptorPoolSize pool_size = {0};
    VkDescriptorPoolCreateInfo pool_info = {0};
    VkDescriptorSetAllocateInfo set_info = {0};
    VkDescriptorImageInfo descriptor_image = {0};
    VkWriteDescriptorSet descriptor_write = {0};
    VkDeviceSize upload_size;
    size_t upload_byte_count;
    uint32_t memory_type_index;
    void *mapped_data = NULL;
    VkResult result;

    if (renderer == NULL || renderer->device == VK_NULL_HANDLE ||
        renderer->command_pool == VK_NULL_HANDLE ||
        renderer->wall_texture_set_layout == VK_NULL_HANDLE ||
        rgba_pages == NULL || page_count == 0u) {
        return false;
    }

    vkGetPhysicalDeviceProperties(renderer->physical_device, &device_properties);
    if (page_count > device_properties.limits.maxImageArrayLayers) {
        fprintf(stderr, "The graphics device cannot hold all VSWAP wall texture pages.\n");
        return false;
    }

    upload_byte_count = (size_t)page_count * WOLF_WALL_TEXTURE_RGBA_SIZE;
    if (upload_byte_count / WOLF_WALL_TEXTURE_RGBA_SIZE != (size_t)page_count) {
        fprintf(stderr, "VSWAP wall texture data size exceeds the host address space.\n");
        return false;
    }
    upload_size = (VkDeviceSize)upload_byte_count;

    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = upload_size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateBuffer(
        renderer->device, &buffer_info, NULL, &replacement.staging_buffer);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture staging buffer creation failed (%d).\n", result);
        goto failure;
    }

    vkGetBufferMemoryRequirements(
        renderer->device, replacement.staging_buffer, &memory_requirements);
    if (!find_memory_type(
            renderer, memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &memory_type_index)) {
        fprintf(stderr, "No host-visible coherent memory is available for wall texture upload.\n");
        goto failure;
    }

    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = memory_type_index;

    result = vkAllocateMemory(
        renderer->device, &allocation_info, NULL, &replacement.staging_memory);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture staging memory allocation failed (%d).\n", result);
        goto failure;
    }

    result = vkBindBufferMemory(
        renderer->device, replacement.staging_buffer, replacement.staging_memory, 0u);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture staging memory binding failed (%d).\n", result);
        goto failure;
    }

    result = vkMapMemory(
        renderer->device, replacement.staging_memory, 0u, upload_size, 0u, &mapped_data);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture staging memory mapping failed (%d).\n", result);
        goto failure;
    }
    memcpy(mapped_data, rgba_pages, (size_t)upload_size);
    vkUnmapMemory(renderer->device, replacement.staging_memory);
    mapped_data = NULL;

    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    image_info.extent = (VkExtent3D){
        WOLF_WALL_TEXTURE_WIDTH, WOLF_WALL_TEXTURE_HEIGHT, 1u
    };
    image_info.mipLevels = 1u;
    image_info.arrayLayers = page_count;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    result = vkCreateImage(
        renderer->device, &image_info, NULL, &replacement.image);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture image creation failed (%d).\n", result);
        goto failure;
    }

    vkGetImageMemoryRequirements(renderer->device, replacement.image, &memory_requirements);
    if (!find_memory_type(
            renderer, memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_index)) {
        fprintf(stderr, "No device-local memory is available for wall textures.\n");
        goto failure;
    }

    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = memory_type_index;
    result = vkAllocateMemory(
        renderer->device, &allocation_info, NULL, &replacement.image_memory);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture image memory allocation failed (%d).\n", result);
        goto failure;
    }

    result = vkBindImageMemory(
        renderer->device, replacement.image, replacement.image_memory, 0u);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture image memory binding failed (%d).\n", result);
        goto failure;
    }

    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = replacement.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    view_info.components = (VkComponentMapping){
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
    };
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0u;
    view_info.subresourceRange.levelCount = 1u;
    view_info.subresourceRange.baseArrayLayer = 0u;
    view_info.subresourceRange.layerCount = page_count;

    result = vkCreateImageView(
        renderer->device, &view_info, NULL, &replacement.image_view);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture image view creation failed (%d).\n", result);
        goto failure;
    }

    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;

    result = vkCreateSampler(
        renderer->device, &sampler_info, NULL, &replacement.sampler);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture sampler creation failed (%d).\n", result);
        goto failure;
    }

    if (!upload_wall_texture_layers(renderer, &replacement, page_count)) {
        goto failure;
    }

    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1u;
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1u;
    pool_info.poolSizeCount = 1u;
    pool_info.pPoolSizes = &pool_size;

    result = vkCreateDescriptorPool(
        renderer->device, &pool_info, NULL, &replacement.descriptor_pool);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture descriptor pool creation failed (%d).\n", result);
        goto failure;
    }

    set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_info.descriptorPool = replacement.descriptor_pool;
    set_info.descriptorSetCount = 1u;
    set_info.pSetLayouts = &renderer->wall_texture_set_layout;
    result = vkAllocateDescriptorSets(
        renderer->device, &set_info, &replacement.descriptor_set);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan wall texture descriptor set allocation failed (%d).\n", result);
        goto failure;
    }

    descriptor_image.sampler = replacement.sampler;
    descriptor_image.imageView = replacement.image_view;
    descriptor_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = replacement.descriptor_set;
    descriptor_write.dstBinding = 0u;
    descriptor_write.dstArrayElement = 0u;
    descriptor_write.descriptorCount = 1u;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.pImageInfo = &descriptor_image;
    vkUpdateDescriptorSets(renderer->device, 1u, &descriptor_write, 0u, NULL);

    /*
     * A previous texture set may be in flight. Wait before replacing the
     * descriptor or image that its recorded frames can still reference.
     */
    if (vkDeviceWaitIdle(renderer->device) != VK_SUCCESS) {
        fprintf(stderr, "Waiting for Vulkan before replacing wall textures failed.\n");
        goto failure;
    }

    if (renderer->wall_texture_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(
            renderer->device, renderer->wall_texture_descriptor_pool, NULL);
    }
    if (renderer->wall_texture_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(renderer->device, renderer->wall_texture_sampler, NULL);
    }
    if (renderer->wall_texture_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(renderer->device, renderer->wall_texture_image_view, NULL);
    }
    if (renderer->wall_texture_image != VK_NULL_HANDLE) {
        vkDestroyImage(renderer->device, renderer->wall_texture_image, NULL);
    }
    if (renderer->wall_texture_image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(renderer->device, renderer->wall_texture_image_memory, NULL);
    }

    renderer->wall_texture_descriptor_pool = replacement.descriptor_pool;
    renderer->wall_texture_descriptor_set = replacement.descriptor_set;
    renderer->wall_texture_sampler = replacement.sampler;
    renderer->wall_texture_image_view = replacement.image_view;
    renderer->wall_texture_image = replacement.image;
    renderer->wall_texture_image_memory = replacement.image_memory;
    renderer->wall_texture_page_count = page_count;

    replacement.descriptor_pool = VK_NULL_HANDLE;
    replacement.descriptor_set = VK_NULL_HANDLE;
    replacement.sampler = VK_NULL_HANDLE;
    replacement.image_view = VK_NULL_HANDLE;
    replacement.image = VK_NULL_HANDLE;
    replacement.image_memory = VK_NULL_HANDLE;

    destroy_temporary_wall_texture(renderer, &replacement);
    return true;

failure:
    if (mapped_data != NULL) {
        vkUnmapMemory(renderer->device, replacement.staging_memory);
    }
    destroy_temporary_wall_texture(renderer, &replacement);
    return false;
}

/* Release the descriptor pool and sampled image before destroying their shared layout. */
void vulkan_renderer_texture_destroy(VulkanRenderer *renderer)
{
    if (renderer == NULL || renderer->device == VK_NULL_HANDLE) {
        return;
    }

    if (renderer->wall_texture_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(
            renderer->device, renderer->wall_texture_descriptor_pool, NULL);
        renderer->wall_texture_descriptor_pool = VK_NULL_HANDLE;
        renderer->wall_texture_descriptor_set = VK_NULL_HANDLE;
    }
    if (renderer->wall_texture_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(renderer->device, renderer->wall_texture_sampler, NULL);
        renderer->wall_texture_sampler = VK_NULL_HANDLE;
    }
    if (renderer->wall_texture_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(renderer->device, renderer->wall_texture_image_view, NULL);
        renderer->wall_texture_image_view = VK_NULL_HANDLE;
    }
    if (renderer->wall_texture_image != VK_NULL_HANDLE) {
        vkDestroyImage(renderer->device, renderer->wall_texture_image, NULL);
        renderer->wall_texture_image = VK_NULL_HANDLE;
    }
    if (renderer->wall_texture_image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(renderer->device, renderer->wall_texture_image_memory, NULL);
        renderer->wall_texture_image_memory = VK_NULL_HANDLE;
    }
    renderer->wall_texture_page_count = 0u;

    if (renderer->wall_texture_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(
            renderer->device, renderer->wall_texture_set_layout, NULL);
        renderer->wall_texture_set_layout = VK_NULL_HANDLE;
    }
}
