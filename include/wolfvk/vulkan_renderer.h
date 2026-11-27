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

/*
 * One placed copy of a model. Models are uploaded once in their own model
 * space; an instance draws a range of that mesh turned by a yaw (0 faces
 * north, as the camera does) and moved to a world position. Many instances
 * can share one range, and changing an instance's range later is enough to
 * change its pose.
 */
typedef struct VulkanModelInstance {
    uint32_t first_quad;
    uint32_t quad_count;
    float position[3];
    float yaw_radians;
} VulkanModelInstance;

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

/*
 * Replace every model mesh with one model-space quad list. Existing
 * instances refer to the old mesh, so they are cleared and must be set again.
 */
bool vulkan_renderer_set_model_meshes(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count);

/* Replace the placed model instances drawn each frame; every range must lie inside the model mesh. */
bool vulkan_renderer_set_model_instances(
    VulkanRenderer *renderer,
    const VulkanModelInstance *instances,
    uint32_t instance_count);

/* Upload the first-person weapon poses into a dedicated overlay mesh. */
bool vulkan_renderer_set_weapon_meshes(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count);

/* Select the weapon pose and camera-relative transform drawn at the end of each frame. */
bool vulkan_renderer_set_weapon_pose(
    VulkanRenderer *renderer,
    uint32_t first_quad,
    uint32_t quad_count,
    const float view_projection[16]);

/* Suppress the first-person weapon while the player death animation is active. */
void vulkan_renderer_hide_weapon(VulkanRenderer *renderer);

/* Build the column-major model transform for an instance: turn by its yaw, then move to its position. */
void vulkan_renderer_model_matrix(const VulkanModelInstance *instance, float matrix[16]);

/* Upload 64x64 RGBA wall pages as layers in the renderer's nearest-sampled texture array. */
bool vulkan_renderer_set_wall_textures(
    VulkanRenderer *renderer,
    const uint8_t *rgba_pages,
    uint32_t page_count);

/*
 * Show the 320x40 status bar (RGBA, row-major) below the 3D view at the
 * integer scale wolf_screen_layout_compute chooses, with border_rgba filling
 * the rest of its strip. The first call makes room for the bar; later calls
 * replace its pixels, which reach the screen with the next frame.
 */
bool vulkan_renderer_set_status_bar(
    VulkanRenderer *renderer,
    const uint8_t *rgba_pixels,
    const uint8_t border_rgba[4]);

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
