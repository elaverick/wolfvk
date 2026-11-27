#include "vulkan_renderer_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Prefer dedicated graphics hardware while retaining integrated and software devices as fallbacks. */
uint32_t vulkan_renderer_score_device_type(VkPhysicalDeviceType device_type)
{
    switch (device_type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return 4;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return 3;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return 2;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return 1;
    default:
        return 0;
    }
}

/* Rendering and window presentation may use different queue families on the selected adapter. */
bool vulkan_renderer_find_queue_families(
    const VkQueueFamilyProperties *queue_families,
    const VkBool32 *present_support,
    uint32_t queue_family_count,
    VulkanQueueFamilyIndices *indices)
{
    uint32_t family_index;

    if (queue_families == NULL || present_support == NULL || indices == NULL) {
        return false;
    }

    memset(indices, 0, sizeof(*indices));

    for (family_index = 0; family_index < queue_family_count; ++family_index) {
        if (!indices->has_graphics_family &&
            queue_families[family_index].queueCount > 0u &&
            (queue_families[family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
            indices->graphics_family = family_index;
            indices->has_graphics_family = true;
        }

        if (!indices->has_present_family && queue_families[family_index].queueCount > 0u &&
            present_support[family_index] == VK_TRUE) {
            indices->present_family = family_index;
            indices->has_present_family = true;
        }

        if (indices->has_graphics_family && indices->has_present_family) {
            return true;
        }
    }

    return false;
}

/* A graphics-capable device is insufficient unless it also exposes the presentation extension. */
static bool device_supports_swapchain(VkPhysicalDevice physical_device)
{
    uint32_t extension_count = 0;
    VkExtensionProperties *extensions = NULL;
    uint32_t extension_index;
    bool found_swapchain = false;

    if (vkEnumerateDeviceExtensionProperties(
            physical_device, NULL, &extension_count, NULL) != VK_SUCCESS ||
        extension_count == 0u) {
        return false;
    }

    extensions = calloc(extension_count, sizeof(*extensions));
    if (extensions == NULL) {
        return false;
    }

    if (vkEnumerateDeviceExtensionProperties(
            physical_device, NULL, &extension_count, extensions) != VK_SUCCESS) {
        free(extensions);
        return false;
    }

    for (extension_index = 0; extension_index < extension_count; ++extension_index) {
        if (strcmp(extensions[extension_index].extensionName,
                   VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            found_swapchain = true;
            break;
        }
    }

    free(extensions);
    return found_swapchain;
}

/* Choose the highest-ranked adapter that can render and present to this SDL window surface. */
static bool select_physical_device(VulkanRenderer *renderer)
{
    uint32_t device_count = 0;
    VkPhysicalDevice *devices = NULL;
    uint32_t device_index;
    bool selected = false;
    uint32_t best_device_score = 0;

    if (vkEnumeratePhysicalDevices(renderer->instance, &device_count, NULL) != VK_SUCCESS ||
        device_count == 0u) {
        fprintf(stderr, "No Vulkan physical devices are available.\n");
        return false;
    }

    devices = calloc(device_count, sizeof(*devices));
    if (devices == NULL) {
        return false;
    }

    if (vkEnumeratePhysicalDevices(renderer->instance, &device_count, devices) != VK_SUCCESS) {
        free(devices);
        return false;
    }

    for (device_index = 0; device_index < device_count; ++device_index) {
        uint32_t family_count = 0;
        VkQueueFamilyProperties *families = NULL;
        VkBool32 *present_support = NULL;
        VulkanQueueFamilyIndices indices;
        uint32_t family_index;

        vkGetPhysicalDeviceQueueFamilyProperties(devices[device_index], &family_count, NULL);
        if (family_count == 0u) {
            continue;
        }

        families = calloc(family_count, sizeof(*families));
        present_support = calloc(family_count, sizeof(*present_support));
        if (families == NULL || present_support == NULL) {
            free(families);
            free(present_support);
            break;
        }

        vkGetPhysicalDeviceQueueFamilyProperties(devices[device_index], &family_count, families);
        for (family_index = 0; family_index < family_count; ++family_index) {
            if (vkGetPhysicalDeviceSurfaceSupportKHR(
                    devices[device_index], family_index, renderer->surface,
                    &present_support[family_index]) != VK_SUCCESS) {
                present_support[family_index] = VK_FALSE;
            }
        }

        if (vulkan_renderer_find_queue_families(
                families, present_support, family_count, &indices) &&
            device_supports_swapchain(devices[device_index])) {
            VkSurfaceCapabilitiesKHR capabilities;
            uint32_t format_count = 0;
            uint32_t present_mode_count = 0;

            if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                    devices[device_index], renderer->surface, &capabilities) == VK_SUCCESS &&
                vkGetPhysicalDeviceSurfaceFormatsKHR(
                    devices[device_index], renderer->surface, &format_count, NULL) == VK_SUCCESS &&
                vkGetPhysicalDeviceSurfacePresentModesKHR(
                    devices[device_index], renderer->surface, &present_mode_count, NULL) == VK_SUCCESS &&
                format_count > 0u && present_mode_count > 0u) {
                VkPhysicalDeviceProperties properties;
                uint32_t device_score;

                vkGetPhysicalDeviceProperties(devices[device_index], &properties);
                device_score = vulkan_renderer_score_device_type(properties.deviceType);
                if (!selected || device_score > best_device_score) {
                    renderer->physical_device = devices[device_index];
                    renderer->graphics_queue_family = indices.graphics_family;
                    renderer->present_queue_family = indices.present_family;
                    best_device_score = device_score;
                    selected = true;
                }
            }
        }

        free(families);
        free(present_support);
    }

    free(devices);

    if (!selected) {
        fprintf(stderr, "No Vulkan device supports graphics and window presentation.\n");
    }
    return selected;
}

/* Request only the queue families WolfVK uses, avoiding duplicate requests when they are shared. */
static bool create_logical_device(VulkanRenderer *renderer)
{
    VkDeviceQueueCreateInfo queue_create_infos[2] = {0};
    uint32_t unique_families[2];
    uint32_t unique_family_count = 0;
    float queue_priority = 1.0f;
    VkPhysicalDeviceFeatures features = {0};
    const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo create_info = {0};
    uint32_t family_index;
    VkResult result;

    unique_families[unique_family_count++] = renderer->graphics_queue_family;
    if (renderer->present_queue_family != renderer->graphics_queue_family) {
        unique_families[unique_family_count++] = renderer->present_queue_family;
    }

    for (family_index = 0; family_index < unique_family_count; ++family_index) {
        queue_create_infos[family_index].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[family_index].queueFamilyIndex = unique_families[family_index];
        queue_create_infos[family_index].queueCount = 1;
        queue_create_infos[family_index].pQueuePriorities = &queue_priority;
    }

    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = unique_family_count;
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.pEnabledFeatures = &features;
    create_info.enabledExtensionCount = 1;
    create_info.ppEnabledExtensionNames = device_extensions;

    result = vkCreateDevice(renderer->physical_device, &create_info, NULL, &renderer->device);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan logical device creation failed (%d).\n", result);
        return false;
    }

    vkGetDeviceQueue(
        renderer->device, renderer->graphics_queue_family, 0, &renderer->graphics_queue);
    vkGetDeviceQueue(
        renderer->device, renderer->present_queue_family, 0, &renderer->present_queue);
    return true;
}

/* Build Vulkan objects in dependency order; the shared destroy path also handles partial failures. */
bool vulkan_renderer_create(SDL_Window *window, VulkanRenderer **renderer_out)
{
    VulkanRenderer *renderer;
    const char * const *required_extensions;
    uint32_t extension_count = 0;
    VkApplicationInfo application_info = {0};
    VkInstanceCreateInfo instance_info = {0};
    VkResult result;

    if (window == NULL || renderer_out == NULL) {
        return false;
    }

    *renderer_out = NULL;
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_VULKAN) == 0u) {
        fprintf(stderr, "The SDL window was not created with SDL_WINDOW_VULKAN.\n");
        return false;
    }

    renderer = calloc(1, sizeof(*renderer));
    if (renderer == NULL) {
        return false;
    }

    renderer->window = window;
    renderer->view_projection[0] = 1.0f;
    renderer->view_projection[5] = 1.0f;
    renderer->view_projection[10] = 1.0f;
    renderer->view_projection[15] = 1.0f;
    required_extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
    if (required_extensions == NULL || extension_count == 0u) {
        fprintf(stderr, "SDL could not provide Vulkan instance extensions: %s\n", SDL_GetError());
        vulkan_renderer_destroy(renderer);
        return false;
    }

    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = "WolfVK";
    application_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    application_info.pEngineName = "WolfVK";
    application_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    application_info.apiVersion = VK_API_VERSION_1_0;

    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &application_info;
    instance_info.enabledExtensionCount = extension_count;
    instance_info.ppEnabledExtensionNames = required_extensions;

    result = vkCreateInstance(&instance_info, NULL, &renderer->instance);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan instance creation failed (%d).\n", result);
        vulkan_renderer_destroy(renderer);
        return false;
    }

    if (!SDL_Vulkan_CreateSurface(window, renderer->instance, NULL, &renderer->surface)) {
        fprintf(stderr, "Vulkan window surface creation failed: %s\n", SDL_GetError());
        vulkan_renderer_destroy(renderer);
        return false;
    }

    if (!select_physical_device(renderer) || !create_logical_device(renderer) ||
        !vulkan_renderer_texture_layout_create(renderer) ||
        !vulkan_renderer_frame_resources_create(renderer)) {
        vulkan_renderer_destroy(renderer);
        return false;
    }

    *renderer_out = renderer;
    return true;
}

/* Wait for GPU work to finish, then release dependent resources before their parent device/instance. */
void vulkan_renderer_destroy(VulkanRenderer *renderer)
{
    if (renderer == NULL) {
        return;
    }

    if (renderer->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(renderer->device);
        vulkan_renderer_frame_resources_destroy(renderer);
        vulkan_renderer_geometry_destroy(renderer);
        vulkan_renderer_weapon_destroy(renderer);
        vulkan_renderer_models_destroy(renderer);
        vulkan_renderer_status_bar_destroy(renderer);
        vulkan_renderer_texture_destroy(renderer);
        vkDestroyDevice(renderer->device, NULL);
    }
    if (renderer->surface != VK_NULL_HANDLE && renderer->instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
    }
    if (renderer->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(renderer->instance, NULL);
    }
    free(renderer);
}

/* Replace the static level mesh through the geometry owner, which manages its GPU buffers. */
bool vulkan_renderer_set_quads(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count)
{
    return vulkan_renderer_geometry_set_quads(renderer, quads, quad_count);
}

/* Refresh moving doors and pushwalls in the existing fixed-size mesh allocation. */
bool vulkan_renderer_update_quads(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count)
{
    return vulkan_renderer_geometry_update_quads(renderer, quads, quad_count);
}

/* Replace the voxel model mesh through the model owner, which keeps it apart from level geometry. */
bool vulkan_renderer_set_model_meshes(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count)
{
    return vulkan_renderer_models_set_meshes(renderer, quads, quad_count);
}

/* Replace the placed models drawn by the next recorded frame. */
bool vulkan_renderer_set_model_instances(
    VulkanRenderer *renderer,
    const VulkanModelInstance *instances,
    uint32_t instance_count)
{
    return vulkan_renderer_models_set_instances(renderer, instances, instance_count);
}


/* Replace weapon frame geometry without changing the actor model buffers. */
bool vulkan_renderer_set_weapon_meshes(
    VulkanRenderer *renderer,
    const VulkanQuad *quads,
    uint32_t quad_count)
{
    return vulkan_renderer_weapon_set_meshes(renderer, quads, quad_count);
}

/* Select a view-model pose and the camera-relative matrix used by the overlay pass. */
bool vulkan_renderer_set_weapon_pose(
    VulkanRenderer *renderer,
    uint32_t first_quad,
    uint32_t quad_count,
    const float view_projection[16])
{
    return vulkan_renderer_weapon_set_pose(renderer, first_quad, quad_count, view_projection);
}

/* Upload the original indexed wall pages before any frame references textured geometry. */
/* Hide the view model during the player's death animation. */
void vulkan_renderer_hide_weapon(VulkanRenderer *renderer)
{
    vulkan_renderer_weapon_hide(renderer);
}

bool vulkan_renderer_set_wall_textures(
    VulkanRenderer *renderer,
    const uint8_t *rgba_pages,
    uint32_t page_count)
{
    return vulkan_renderer_texture_set(renderer, rgba_pages, page_count);
}

bool vulkan_renderer_set_status_bar(
    VulkanRenderer *renderer,
    const uint8_t *rgba_pixels,
    const uint8_t border_rgba[4])
{
    return vulkan_renderer_status_bar_set(renderer, rgba_pixels, border_rgba);
}

/* Clamp the damage palette effect before the next frame records its fragment push constant. */
void vulkan_renderer_set_damage_flash(VulkanRenderer *renderer, float intensity)
{
    if (renderer == NULL) {
        return;
    }
    if (intensity < 0.0f) {
        intensity = 0.0f;
    } else if (intensity > 1.0f) {
        intensity = 1.0f;
    }
    renderer->damage_flash_intensity = intensity;
}

/* Store the latest camera matrix for the next submitted frame without rebuilding the pipeline. */
bool vulkan_renderer_set_view_projection(
    VulkanRenderer *renderer,
    const float view_projection[16])
{
    if (renderer == NULL || view_projection == NULL) {
        return false;
    }

    memcpy(renderer->view_projection, view_projection, sizeof(renderer->view_projection));
    return true;
}

/* Keep swapchain acquisition, command submission and presentation in the frame-resource module. */
bool vulkan_renderer_draw_frame(VulkanRenderer *renderer)
{
    if (renderer == NULL) {
        return false;
    }
    return vulkan_renderer_frame_resources_draw(renderer);
}

/* Defer swapchain replacement until frame code can safely rebuild all size-dependent resources. */
void vulkan_renderer_notify_resize(VulkanRenderer *renderer)
{
    if (renderer != NULL) {
        renderer->framebuffer_resized = true;
    }
}
