#ifndef WOLFVK_VULKAN_RENDERER_INTERNAL_H
#define WOLFVK_VULKAN_RENDERER_INTERNAL_H

#include "wolfvk/screen_layout.h"
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
    VkPipeline weapon_pipeline;
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
    uint32_t quad_count;
    VkDeviceSize vertex_buffer_size;
    float view_projection[16];
    float damage_flash_intensity;

    /* Voxel models live in their own buffers so animating doors never re-copies them. */
    VkBuffer model_vertex_buffer;
    VkDeviceMemory model_vertex_buffer_memory;
    VkBuffer model_index_buffer;
    VkDeviceMemory model_index_buffer_memory;
    uint32_t model_quad_count;
    VulkanModelInstance *model_instances;
    uint32_t model_instance_count;

    /* First-person weapon meshes use a depth-independent pass after the world models. */
    VkBuffer weapon_vertex_buffer;
    VkDeviceMemory weapon_vertex_buffer_memory;
    VkBuffer weapon_index_buffer;
    VkDeviceMemory weapon_index_buffer_memory;
    uint32_t weapon_quad_count;
    uint32_t weapon_first_quad;
    uint32_t weapon_draw_quad_count;
    float weapon_view_projection[16];
    /*
     * The status bar is a 320x40 picture drawn at an integer scale below the
     * 3D view. Its pixels are kept on the CPU and copied to the GPU at the
     * start of the next frame, once the previous frame has finished with them.
     */
    VkImage status_bar_image;
    VkDeviceMemory status_bar_image_memory;
    VkImageView status_bar_image_view;
    VkSampler status_bar_sampler;
    VkDescriptorPool status_bar_descriptor_pool;
    VkDescriptorSet status_bar_descriptor_set;
    VkBuffer status_bar_staging_buffer;
    VkDeviceMemory status_bar_staging_memory;
    VkBuffer status_bar_vertex_buffer;
    VkDeviceMemory status_bar_vertex_memory;
    VkBuffer status_bar_index_buffer;
    VkDeviceMemory status_bar_index_memory;
    uint8_t *status_bar_pixels;
    float status_bar_border_colour[4];
    bool status_bar_needs_upload;
    bool status_bar_uploaded_once;

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
bool vulkan_renderer_geometry_update_quads(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count);
void vulkan_renderer_geometry_destroy(VulkanRenderer *renderer);

/* Create host-visible vertex and index buffers holding indexed quads (two triangles each). */
bool vulkan_renderer_geometry_create_quad_buffers(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count,
    VkBuffer *vertex_buffer,
    VkDeviceMemory *vertex_memory,
    VkBuffer *index_buffer,
    VkDeviceMemory *index_memory);
void vulkan_renderer_geometry_destroy_buffer(
    VulkanRenderer *renderer,
    VkBuffer *buffer,
    VkDeviceMemory *memory);

bool vulkan_renderer_models_set_meshes(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count);
bool vulkan_renderer_models_set_instances(
    VulkanRenderer *renderer,
    const VulkanModelInstance *instances,
    uint32_t instance_count);
/* Record one indexed draw per instance, each with its own model-view-projection push constant. */
void vulkan_renderer_models_record(VulkanRenderer *renderer, VkCommandBuffer command_buffer);
void vulkan_renderer_models_destroy(VulkanRenderer *renderer);

bool vulkan_renderer_weapon_set_meshes(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count);
bool vulkan_renderer_weapon_set_pose(
    VulkanRenderer *renderer,
    uint32_t first_quad,
    uint32_t quad_count,
    const float view_projection[16]);
void vulkan_renderer_weapon_record(VulkanRenderer *renderer, VkCommandBuffer command_buffer);
/* Hide the view model without releasing its uploaded geometry. */
void vulkan_renderer_weapon_hide(VulkanRenderer *renderer);
void vulkan_renderer_weapon_destroy(VulkanRenderer *renderer);
bool vulkan_renderer_geometry_create_host_buffer(
    VulkanRenderer *renderer,
    const void *data,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkBuffer *buffer,
    VkDeviceMemory *memory);
bool vulkan_renderer_find_memory_type(
    VulkanRenderer *renderer,
    uint32_t type_filter,
    VkMemoryPropertyFlags required_properties,
    uint32_t *memory_type_index);

bool vulkan_renderer_status_bar_set(
    VulkanRenderer *renderer,
    const uint8_t *rgba_pixels,
    const uint8_t border_rgba[4]);
/* True once a status bar picture has been supplied, so the view must leave room for it. */
bool vulkan_renderer_status_bar_is_shown(const VulkanRenderer *renderer);
/* Copy changed status bar pixels to the GPU; recorded before the render pass begins. */
void vulkan_renderer_status_bar_record_upload(VulkanRenderer *renderer, VkCommandBuffer command_buffer);
/* Fill the strip below the view with the border colour and draw the bar over it at its integer scale. */
void vulkan_renderer_status_bar_record_draw(
    VulkanRenderer *renderer,
    VkCommandBuffer command_buffer,
    const WolfScreenLayout *layout);
void vulkan_renderer_status_bar_destroy(VulkanRenderer *renderer);

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
