#ifndef WOLFKVK_RENDERER_VK_H
#define WOLFKVK_RENDERER_VK_H

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#include <stdbool.h>
#include <stdint.h>

#define WOLFKVK_FRAMES_IN_FLIGHT 2u

typedef struct WolfVkRenderer {
    SDL_Window *window;

    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t graphics_family;
    uint32_t present_family;
    VkQueue graphics_queue;
    VkQueue present_queue;

    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    VkImage *swapchain_images;
    VkImageView *swapchain_views;
    VkFramebuffer *framebuffers;
    VkImage *depth_images;
    VkDeviceMemory *depth_memory;
    VkImageView *depth_views;
    VkFence *images_in_flight;
    uint32_t swapchain_image_count;
    VkFormat depth_format;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;

    VkCommandPool command_pool;
    VkCommandBuffer *command_buffers;

    VkSemaphore image_available[WOLFKVK_FRAMES_IN_FLIGHT];
    VkSemaphore render_finished[WOLFKVK_FRAMES_IN_FLIGHT];
    VkFence frame_fences[WOLFKVK_FRAMES_IN_FLIGHT];
    uint32_t current_frame;

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;
    float animation_time;
} WolfVkRenderer;

/* Initialise Vulkan and connect its presentation surface to the SDL window. */
bool wolfvk_renderer_create(WolfVkRenderer *renderer, SDL_Window *window);

/* Render one animated 3D frame and present it through the SDL window surface. */
bool wolfvk_renderer_draw(WolfVkRenderer *renderer, float delta_seconds);

/* Release Vulkan resources in reverse dependency order. */
void wolfvk_renderer_destroy(WolfVkRenderer *renderer);

#endif
