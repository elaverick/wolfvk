#ifndef WOLFVK_VULKAN_RENDERER_H
#define WOLfVK_VULKAN_RENDERER_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct VulkanRenderer VulkanRenderer;

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

/* Release Vulkan objects in reverse dependency order. */
void vulkan_renderer_destroy(VulkanRenderer *renderer);

#endif
