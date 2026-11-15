#include "wolfvk/vulkan_renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct VulkanRenderer {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
};

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

static bool select_physical_device(VulkanRenderer *renderer)
{
    uint32_t device_count = 0;
    VkPhysicalDevice *devices = NULL;
    uint32_t device_index;
    bool selected = false;

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

    for (device_index = 0; device_index < device_count && !selected; ++device_index) {
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
                renderer->physical_device = devices[device_index];
                renderer->graphics_queue = VK_NULL_HANDLE;
                renderer->present_queue = VK_NULL_HANDLE;
                selected = true;
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

static bool create_logical_device(VulkanRenderer *renderer)
{
    uint32_t family_count = 0;
    VkQueueFamilyProperties *families = NULL;
    VkBool32 *present_support = NULL;
    VulkanQueueFamilyIndices indices;
    VkDeviceQueueCreateInfo queue_create_infos[2] = {0};
    uint32_t unique_families[2];
    uint32_t unique_family_count = 0;
    float queue_priority = 1.0f;
    VkPhysicalDeviceFeatures features = {0};
    const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo create_info = {0};
    uint32_t family_index;
    VkResult result;

    vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device, &family_count, NULL);
    families = calloc(family_count, sizeof(*families));
    present_support = calloc(family_count, sizeof(*present_support));
    if (families == NULL || present_support == NULL) {
        free(families);
        free(present_support);
        return false;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device, &family_count, families);
    for (family_index = 0; family_index < family_count; ++family_index) {
        if (vkGetPhysicalDeviceSurfaceSupportKHR(
                renderer->physical_device, family_index, renderer->surface,
                &present_support[family_index]) != VK_SUCCESS) {
            present_support[family_index] = VK_FALSE;
        }
    }

    if (!vulkan_renderer_find_queue_families(
            families, present_support, family_count, &indices)) {
        free(families);
        free(present_support);
        return false;
    }

    free(families);
    free(present_support);

    unique_families[unique_family_count++] = indices.graphics_family;
    if (indices.present_family != indices.graphics_family) {
        unique_families[unique_family_count++] = indices.present_family;
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

    vkGetDeviceQueue(renderer->device, indices.graphics_family, 0, &renderer->graphics_queue);
    vkGetDeviceQueue(renderer->device, indices.present_family, 0, &renderer->present_queue);
    return true;
}

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
    renderer = calloc(1, sizeof(*renderer));
    if (renderer == NULL) {
        return false;
    }

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

    if (!select_physical_device(renderer) || !create_logical_device(renderer)) {
        vulkan_renderer_destroy(renderer);
        return false;
    }

    *renderer_out = renderer;
    return true;
}

void vulkan_renderer_destroy(VulkanRenderer *renderer)
{
    if (renderer == NULL) {
        return;
    }

    if (renderer->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(renderer->device);
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
