#include "vulkan_renderer_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATUS_BAR_RGBA_SIZE ((size_t)WOLF_STATUS_BAR_WIDTH * WOLF_STATUS_BAR_HEIGHT * 4u)

/*
 * One quad covering the whole viewport. The draw sets the viewport to the
 * bar's integer-scaled rectangle, so each texel lands on an exact block of
 * screen pixels and nearest sampling keeps the pixel art sharp.
 */
static const VulkanQuad status_bar_quad = {{
    {{-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, 0u, 1u},
    {{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, 0u, 1u},
    {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, 0u, 1u},
    {{-1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, 0u, 1u}
}};

/* The quad is already in clip space, so the shader's matrix must leave it unchanged. */
static const float identity_matrix[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

/* Clear values are written through the sRGB swapchain, so the border colour must be linear. */
static float srgb_to_linear(uint8_t value)
{
    float normalized = (float)value / 255.0f;

    return normalized <= 0.04045f
        ? normalized / 12.92f
        : powf((normalized + 0.055f) / 1.055f, 2.4f);
}

/*
 * The image is a one-layer array so it can be sampled through the same
 * descriptor layout and shader as the wall pages.
 */
static bool create_status_bar_image(VulkanRenderer *renderer)
{
    VkImageCreateInfo image_info = {0};
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo allocation_info = {0};
    VkImageViewCreateInfo view_info = {0};
    VkSamplerCreateInfo sampler_info = {0};
    uint32_t memory_type_index;

    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    image_info.extent = (VkExtent3D){WOLF_STATUS_BAR_WIDTH, WOLF_STATUS_BAR_HEIGHT, 1u};
    image_info.mipLevels = 1u;
    image_info.arrayLayers = 1u;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(renderer->device, &image_info, NULL, &renderer->status_bar_image) !=
        VK_SUCCESS) {
        return false;
    }

    vkGetImageMemoryRequirements(
        renderer->device, renderer->status_bar_image, &memory_requirements);
    if (!vulkan_renderer_find_memory_type(
            renderer, memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type_index)) {
        return false;
    }
    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = memory_type_index;
    if (vkAllocateMemory(
            renderer->device, &allocation_info, NULL, &renderer->status_bar_image_memory) !=
            VK_SUCCESS ||
        vkBindImageMemory(
            renderer->device, renderer->status_bar_image,
            renderer->status_bar_image_memory, 0u) != VK_SUCCESS) {
        return false;
    }

    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = renderer->status_bar_image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1u;
    view_info.subresourceRange.layerCount = 1u;
    if (vkCreateImageView(
            renderer->device, &view_info, NULL, &renderer->status_bar_image_view) !=
        VK_SUCCESS) {
        return false;
    }

    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    return vkCreateSampler(
        renderer->device, &sampler_info, NULL, &renderer->status_bar_sampler) == VK_SUCCESS;
}

static bool create_status_bar_descriptor(VulkanRenderer *renderer)
{
    VkDescriptorPoolSize pool_size = {0};
    VkDescriptorPoolCreateInfo pool_info = {0};
    VkDescriptorSetAllocateInfo set_info = {0};
    VkDescriptorImageInfo descriptor_image = {0};
    VkWriteDescriptorSet descriptor_write = {0};

    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1u;
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1u;
    pool_info.poolSizeCount = 1u;
    pool_info.pPoolSizes = &pool_size;
    if (vkCreateDescriptorPool(
            renderer->device, &pool_info, NULL, &renderer->status_bar_descriptor_pool) !=
        VK_SUCCESS) {
        return false;
    }

    set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_info.descriptorPool = renderer->status_bar_descriptor_pool;
    set_info.descriptorSetCount = 1u;
    set_info.pSetLayouts = &renderer->wall_texture_set_layout;
    if (vkAllocateDescriptorSets(
            renderer->device, &set_info, &renderer->status_bar_descriptor_set) != VK_SUCCESS) {
        return false;
    }

    descriptor_image.sampler = renderer->status_bar_sampler;
    descriptor_image.imageView = renderer->status_bar_image_view;
    descriptor_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = renderer->status_bar_descriptor_set;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.descriptorCount = 1u;
    descriptor_write.pImageInfo = &descriptor_image;
    vkUpdateDescriptorSets(renderer->device, 1u, &descriptor_write, 0u, NULL);
    return true;
}

/* Build every GPU resource the bar needs the first time a picture is supplied. */
static bool create_status_bar_resources(VulkanRenderer *renderer, const uint8_t *rgba_pixels)
{
    renderer->status_bar_pixels = malloc(STATUS_BAR_RGBA_SIZE);
    if (renderer->status_bar_pixels == NULL ||
        !vulkan_renderer_geometry_create_host_buffer(
            renderer, rgba_pixels, (VkDeviceSize)STATUS_BAR_RGBA_SIZE,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            &renderer->status_bar_staging_buffer, &renderer->status_bar_staging_memory) ||
        !vulkan_renderer_geometry_create_quad_buffers(
            renderer, &status_bar_quad, 1u,
            &renderer->status_bar_vertex_buffer, &renderer->status_bar_vertex_memory,
            &renderer->status_bar_index_buffer, &renderer->status_bar_index_memory) ||
        !create_status_bar_image(renderer) ||
        !create_status_bar_descriptor(renderer)) {
        vulkan_renderer_status_bar_destroy(renderer);
        return false;
    }
    return true;
}

bool vulkan_renderer_status_bar_set(
    VulkanRenderer *renderer,
    const uint8_t *rgba_pixels,
    const uint8_t border_rgba[4])
{
    if (renderer == NULL || renderer->device == VK_NULL_HANDLE ||
        renderer->wall_texture_set_layout == VK_NULL_HANDLE ||
        rgba_pixels == NULL || border_rgba == NULL) {
        return false;
    }
    if (renderer->status_bar_pixels == NULL &&
        !create_status_bar_resources(renderer, rgba_pixels)) {
        fprintf(stderr, "Could not create the Vulkan status bar resources.\n");
        return false;
    }

    memcpy(renderer->status_bar_pixels, rgba_pixels, STATUS_BAR_RGBA_SIZE);
    for (int channel = 0; channel < 3; ++channel) {
        renderer->status_bar_border_colour[channel] = srgb_to_linear(border_rgba[channel]);
    }
    renderer->status_bar_border_colour[3] = 1.0f;
    renderer->status_bar_needs_upload = true;
    return true;
}

bool vulkan_renderer_status_bar_is_shown(const VulkanRenderer *renderer)
{
    return renderer != NULL && renderer->status_bar_descriptor_set != VK_NULL_HANDLE;
}

void vulkan_renderer_status_bar_record_upload(
    VulkanRenderer *renderer,
    VkCommandBuffer command_buffer)
{
    VkImageMemoryBarrier barrier = {0};
    VkBufferImageCopy copy_region = {0};
    void *mapped = NULL;

    if (!renderer->status_bar_needs_upload || renderer->status_bar_pixels == NULL) {
        return;
    }

    /* The frame fence has been waited on, so no earlier frame is still reading the staging buffer. */
    if (vkMapMemory(
            renderer->device, renderer->status_bar_staging_memory, 0u,
            (VkDeviceSize)STATUS_BAR_RGBA_SIZE, 0u, &mapped) != VK_SUCCESS) {
        return;
    }
    memcpy(mapped, renderer->status_bar_pixels, STATUS_BAR_RGBA_SIZE);
    vkUnmapMemory(renderer->device, renderer->status_bar_staging_memory);

    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = renderer->status_bar_uploaded_once ? VK_ACCESS_SHADER_READ_BIT : 0u;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = renderer->status_bar_uploaded_once
        ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        : VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = renderer->status_bar_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1u;
    barrier.subresourceRange.layerCount = 1u;
    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0u, 0u, NULL, 0u, NULL, 1u, &barrier);

    copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.layerCount = 1u;
    copy_region.imageExtent = (VkExtent3D){WOLF_STATUS_BAR_WIDTH, WOLF_STATUS_BAR_HEIGHT, 1u};
    vkCmdCopyBufferToImage(
        command_buffer, renderer->status_bar_staging_buffer, renderer->status_bar_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &copy_region);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0u, 0u, NULL, 0u, NULL, 1u, &barrier);

    renderer->status_bar_needs_upload = false;
    renderer->status_bar_uploaded_once = true;
}

void vulkan_renderer_status_bar_record_draw(
    VulkanRenderer *renderer,
    VkCommandBuffer command_buffer,
    const WolfScreenLayout *layout)
{
    VkClearAttachment clear_attachment = {0};
    VkClearRect clear_rect = {0};
    VkViewport viewport = {0};
    VkRect2D scissor = {0};
    VkDeviceSize vertex_offset = 0u;
    uint32_t visible_width;

    if (!renderer->status_bar_uploaded_once || layout->status_bar_scale == 0u) {
        return;
    }

    /* The original bar is 320 pixels wide; a wider window shows the play-screen border either side. */
    clear_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_attachment.colorAttachment = 0u;
    memcpy(clear_attachment.clearValue.color.float32, renderer->status_bar_border_colour,
        sizeof(renderer->status_bar_border_colour));
    clear_rect.rect.offset = (VkOffset2D){
        (int32_t)layout->status_strip.x, (int32_t)layout->status_strip.y
    };
    clear_rect.rect.extent = (VkExtent2D){
        layout->status_strip.width, layout->status_strip.height
    };
    clear_rect.layerCount = 1u;
    vkCmdClearAttachments(command_buffer, 1u, &clear_attachment, 1u, &clear_rect);

    viewport.x = (float)layout->status_bar.x;
    viewport.y = (float)layout->status_bar.y;
    viewport.width = (float)layout->status_bar.width;
    viewport.height = (float)layout->status_bar.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0u, 1u, &viewport);

    /* A window narrower than the bar crops its right-hand side rather than drawing outside the image. */
    visible_width = layout->status_bar.width < layout->status_strip.width
        ? layout->status_bar.width
        : layout->status_strip.width;
    scissor.offset = (VkOffset2D){(int32_t)layout->status_bar.x, (int32_t)layout->status_bar.y};
    scissor.extent = (VkExtent2D){visible_width, layout->status_bar.height};
    vkCmdSetScissor(command_buffer, 0u, 1u, &scissor);

    /* The weapon overlay may have left its own pipeline bound; the bar uses the ordinary one. */
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->graphics_pipeline);
    vkCmdBindDescriptorSets(
        command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->pipeline_layout,
        0u, 1u, &renderer->status_bar_descriptor_set, 0u, NULL);
    vkCmdPushConstants(
        command_buffer, renderer->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
        0u, (uint32_t)sizeof(identity_matrix), identity_matrix);
    vkCmdBindVertexBuffers(
        command_buffer, 0u, 1u, &renderer->status_bar_vertex_buffer, &vertex_offset);
    vkCmdBindIndexBuffer(
        command_buffer, renderer->status_bar_index_buffer, 0u, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(command_buffer, 6u, 1u, 0u, 0, 0u);
}

void vulkan_renderer_status_bar_destroy(VulkanRenderer *renderer)
{
    if (renderer == NULL || renderer->device == VK_NULL_HANDLE) {
        return;
    }
    if (renderer->status_bar_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(renderer->device, renderer->status_bar_descriptor_pool, NULL);
    }
    if (renderer->status_bar_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(renderer->device, renderer->status_bar_sampler, NULL);
    }
    if (renderer->status_bar_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(renderer->device, renderer->status_bar_image_view, NULL);
    }
    if (renderer->status_bar_image != VK_NULL_HANDLE) {
        vkDestroyImage(renderer->device, renderer->status_bar_image, NULL);
    }
    if (renderer->status_bar_image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(renderer->device, renderer->status_bar_image_memory, NULL);
    }
    vulkan_renderer_geometry_destroy_buffer(
        renderer, &renderer->status_bar_staging_buffer, &renderer->status_bar_staging_memory);
    vulkan_renderer_geometry_destroy_buffer(
        renderer, &renderer->status_bar_vertex_buffer, &renderer->status_bar_vertex_memory);
    vulkan_renderer_geometry_destroy_buffer(
        renderer, &renderer->status_bar_index_buffer, &renderer->status_bar_index_memory);
    free(renderer->status_bar_pixels);

    renderer->status_bar_descriptor_pool = VK_NULL_HANDLE;
    renderer->status_bar_descriptor_set = VK_NULL_HANDLE;
    renderer->status_bar_sampler = VK_NULL_HANDLE;
    renderer->status_bar_image_view = VK_NULL_HANDLE;
    renderer->status_bar_image = VK_NULL_HANDLE;
    renderer->status_bar_image_memory = VK_NULL_HANDLE;
    renderer->status_bar_pixels = NULL;
    renderer->status_bar_needs_upload = false;
    renderer->status_bar_uploaded_once = false;
}
