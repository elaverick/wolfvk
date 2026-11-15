#ifndef WOLFVK_VULKAN_RENDERER_H
#define WOLFVK_VULKAN_RENDERER_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct VulkanRenderer VulkanRenderer;

typedef struct VulkanVertex {
    float position[3];
    float colour[3];
    float texcoord[2];
    uint32_t texture_layer;
    uint32_t use_texture;
} VulkanVertex;

typedef struct VulkanQuad {
    VulkanVertex vertices[4];
} VulkanQuad;

typedef struct VulkanQueueFamilyIndices {
    uint32_t graphics_family;
    uint32_t present_family;
    bool has_graphics_family;
    bool has_present_family;
} VulkanQueueFamilyIndices;

/* Select the first queue families that can render and present to the window. */
bool vulkan_renderer_find_queue_families(
    const VkQueueFamilyProperties *queue_families,
    const VkBool32 *present_support,
    uint32_t queue_family_count,
    VulkanQueueFamilyIndices *indices);

/* Create the Vulkan instance, window surface, physical device and logical device. */
bool vulkan_renderer_create(SDL_Window *window, VulkanRenderer **renderer);

/* Upload static quad geometry. Each quad supplies four world-space vertices. */
bool vulkan_renderer_set_quads(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count);

/* Update an existing scene mesh without reallocating its fixed-size vertex and index buffers. */
bool vulkan_renderer_update_quads(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count);

/* Upload 64x64 RGBA wall pages as layers in the renderer's nearest-sampled texture array. */
bool vulkan_renderer_set_wall_textures(
    VulkanRenderer *renderer,
    const uint8_t *rgba_pages,
    uint32_t page_count);

/* Set a column-major 4x4 view-projection matrix used by the vertex shader. */
bool vulkan_renderer_set_view_projection(
    VulkanRenderer *renderer,
    const float view_projection[16]);

/* Submit and present one frame. Returns false on an unrecoverable Vulkan error. */
bool vulkan_renderer_draw_frame(VulkanRenderer *renderer);

/* Defer swapchain recreation until the next frame can safely rebuild size-dependent resources. */
void vulkan_renderer_notify_resize(VulkanRenderer *renderer);

/* Release Vulkan objects in reverse dependency order. */
void vulkan_renderer_destroy(VulkanRenderer *renderer);

#endif
