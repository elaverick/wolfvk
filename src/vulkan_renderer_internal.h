#ifndef WOLFVK_VULKAN_RENDERER_INTERNAL_H
#define WOLFVK_VULKAN_RENDERER_INTERNAL_H

#include "wolfvk/vulkan_renderer.h"

struct VulkanRenderer {
    SDL_Window *window;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
    uint32_t graphics_queue_family;
    uint32_t present_queue_family;

    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    uint32_t swapchain_image_count;
    VkImage *swapchain_images;
    VkImageView *swapchain_image_views;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;
    VkFramebuffer *framebuffers;

    VkImage depth_image;
    VkDeviceMemory depth_image_memory;
    VkImageView depth_image_view;
    VkFormat depth_format;

    VkDescriptorSetLayout wall_texture_set_layout;
    VkDescriptorPool wall_texture_descriptor_pool;
    VkDescriptorSet wall_texture_descriptor_set;
    VkImage wall_texture_image;
    VkDeviceMemory wall_texture_image_memory;
    VkImageView wall_texture_image_view;
    VkSampler wall_texture_sampler;
    uint32_t wall_texture_page_count;

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkBuffer index_buffer;
    VkDeviceMemory index_buffer_memory;
    uint32_t index_count;
    float view_projection[16];

    VkCommandPool command_pool;
    VkCommandBuffer *command_buffers;

    VkSemaphore image_available_semaphore;
    VkSemaphore *render_finished_semaphores;
    VkFence in_flight_fence;
    bool framebuffer_resized;
};

uint32_t vulkan_renderer_score_device_type(VkPhysicalDeviceType device_type);

bool vulkan_renderer_choose_surface_format(
    const VkSurfaceFormatKHR *available_formats,
    uint32_t format_count,
    VkSurfaceFormatKHR *chosen_format);
VkPresentModeKHR vulkan_renderer_choose_present_mode(
    const VkPresentModeKHR *available_modes,
    uint32_t mode_count);

bool vulkan_renderer_geometry_set_quads(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count);
void vulkan_renderer_geometry_destroy(VulkanRenderer *renderer);

bool vulkan_renderer_texture_layout_create(VulkanRenderer *renderer);
bool vulkan_renderer_texture_set(
    VulkanRenderer *renderer,
    const uint8_t *rgba_pages,
    uint32_t page_count);
void vulkan_renderer_texture_destroy(VulkanRenderer *renderer);

bool vulkan_renderer_frame_resources_create(VulkanRenderer *renderer);
void vulkan_renderer_frame_resources_destroy(VulkanRenderer *renderer);
bool vulkan_renderer_frame_resources_recreate(VulkanRenderer *renderer);
bool vulkan_renderer_frame_resources_draw(VulkanRenderer *renderer);

#endif
