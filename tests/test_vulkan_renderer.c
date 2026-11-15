#include "unity.h"

#include "vulkan_renderer_internal.h"

void test_graphics_and_present_on_same_queue_family(void)
{
    const VkQueueFamilyProperties families[] = {
        {.queueFlags = VK_QUEUE_GRAPHICS_BIT, .queueCount = 1}
    };
    const VkBool32 present_support[] = {VK_TRUE};
    VulkanQueueFamilyIndices indices = {0};

    TEST_ASSERT_TRUE(vulkan_renderer_find_queue_families(
        families, present_support, 1, &indices));
    TEST_ASSERT_TRUE(indices.has_graphics_family);
    TEST_ASSERT_TRUE(indices.has_present_family);
    TEST_ASSERT_EQUAL_UINT32(0, indices.graphics_family);
    TEST_ASSERT_EQUAL_UINT32(0, indices.present_family);
}

void test_graphics_and_present_on_separate_queue_families(void)
{
    const VkQueueFamilyProperties families[] = {
        {.queueFlags = VK_QUEUE_GRAPHICS_BIT, .queueCount = 1},
        {.queueFlags = 0, .queueCount = 1}
    };
    const VkBool32 present_support[] = {VK_FALSE, VK_TRUE};
    VulkanQueueFamilyIndices indices = {0};

    TEST_ASSERT_TRUE(vulkan_renderer_find_queue_families(
        families, present_support, 2, &indices));
    TEST_ASSERT_EQUAL_UINT32(0, indices.graphics_family);
    TEST_ASSERT_EQUAL_UINT32(1, indices.present_family);
}

void test_queue_selection_rejects_missing_graphics_queue(void)
{
    const VkQueueFamilyProperties families[] = {
        {.queueFlags = VK_QUEUE_COMPUTE_BIT, .queueCount = 1}
    };
    const VkBool32 present_support[] = {VK_TRUE};
    VulkanQueueFamilyIndices indices = {0};

    TEST_ASSERT_FALSE(vulkan_renderer_find_queue_families(
        families, present_support, 1, &indices));
    TEST_ASSERT_FALSE(indices.has_graphics_family);
}

void test_queue_selection_rejects_missing_present_queue(void)
{
    const VkQueueFamilyProperties families[] = {
        {.queueFlags = VK_QUEUE_GRAPHICS_BIT, .queueCount = 1}
    };
    const VkBool32 present_support[] = {VK_FALSE};
    VulkanQueueFamilyIndices indices = {0};

    TEST_ASSERT_FALSE(vulkan_renderer_find_queue_families(
        families, present_support, 1, &indices));
    TEST_ASSERT_FALSE(indices.has_present_family);
}

void test_queue_selection_rejects_null_arguments(void)
{
    const VkQueueFamilyProperties families[] = {
        {.queueFlags = VK_QUEUE_GRAPHICS_BIT, .queueCount = 1}
    };
    const VkBool32 present_support[] = {VK_TRUE};
    VulkanQueueFamilyIndices indices = {0};

    TEST_ASSERT_FALSE(vulkan_renderer_find_queue_families(
        NULL, present_support, 1, &indices));
    TEST_ASSERT_FALSE(vulkan_renderer_find_queue_families(
        families, NULL, 1, &indices));
    TEST_ASSERT_FALSE(vulkan_renderer_find_queue_families(
        families, present_support, 1, NULL));
}

void test_queue_selection_ignores_empty_queue_families(void)
{
    const VkQueueFamilyProperties families[] = {
        {.queueFlags = VK_QUEUE_GRAPHICS_BIT, .queueCount = 0},
        {.queueFlags = VK_QUEUE_GRAPHICS_BIT, .queueCount = 1}
    };
    const VkBool32 present_support[] = {VK_TRUE, VK_TRUE};
    VulkanQueueFamilyIndices indices = {0};

    TEST_ASSERT_TRUE(vulkan_renderer_find_queue_families(
        families, present_support, 2, &indices));
    TEST_ASSERT_EQUAL_UINT32(1, indices.graphics_family);
    TEST_ASSERT_EQUAL_UINT32(1, indices.present_family);
}

void test_renderer_rejects_window_without_vulkan_support(void)
{
    SDL_Window *window = SDL_CreateWindow(
        "WolfVK non-Vulkan test window", 320, 240, SDL_WINDOW_HIDDEN);
    VulkanRenderer *renderer = NULL;

    TEST_ASSERT_NOT_NULL_MESSAGE(window, SDL_GetError());
    renderer = (VulkanRenderer *)window;

    TEST_ASSERT_FALSE(vulkan_renderer_create(window, &renderer));
    TEST_ASSERT_NULL(renderer);
    SDL_DestroyWindow(window);
}

void test_surface_format_prefers_srgb(void)
{
    const VkSurfaceFormatKHR formats[] = {
        {.format = VK_FORMAT_R8G8B8A8_UNORM, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        {.format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
    };
    VkSurfaceFormatKHR selected = {0};

    TEST_ASSERT_TRUE(vulkan_renderer_choose_surface_format(formats, 2, &selected));
    TEST_ASSERT_EQUAL_INT(VK_FORMAT_B8G8R8A8_SRGB, selected.format);
}

void test_surface_format_falls_back_to_first_supported_format(void)
{
    const VkSurfaceFormatKHR formats[] = {
        {.format = VK_FORMAT_R8G8B8A8_UNORM, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        {.format = VK_FORMAT_R8G8B8A8_UNORM, .colorSpace = VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT}
    };
    VkSurfaceFormatKHR selected = {0};

    TEST_ASSERT_TRUE(vulkan_renderer_choose_surface_format(formats, 2, &selected));
    TEST_ASSERT_EQUAL_INT(formats[0].format, selected.format);
    TEST_ASSERT_EQUAL_INT(formats[0].colorSpace, selected.colorSpace);
}

void test_present_mode_prefers_mailbox(void)
{
    const VkPresentModeKHR modes[] = {
        VK_PRESENT_MODE_FIFO_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_MAILBOX_KHR
    };

    TEST_ASSERT_EQUAL_INT(
        VK_PRESENT_MODE_MAILBOX_KHR,
        vulkan_renderer_choose_present_mode(modes, 3));
}

void test_present_mode_falls_back_to_fifo(void)
{
    const VkPresentModeKHR modes[] = {
        VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_KHR
    };

    TEST_ASSERT_EQUAL_INT(
        VK_PRESENT_MODE_FIFO_KHR,
        vulkan_renderer_choose_present_mode(modes, 2));
}

void test_surface_format_handles_undefined_single_format(void)
{
    const VkSurfaceFormatKHR formats[] = {
        {.format = VK_FORMAT_UNDEFINED, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
    };
    VkSurfaceFormatKHR selected = {0};

    TEST_ASSERT_TRUE(vulkan_renderer_choose_surface_format(formats, 1, &selected));
    TEST_ASSERT_EQUAL_INT(VK_FORMAT_B8G8R8A8_SRGB, selected.format);
}

void test_swapchain_selection_handles_empty_lists(void)
{
    VkSurfaceFormatKHR selected = {0};

    TEST_ASSERT_FALSE(vulkan_renderer_choose_surface_format(NULL, 0, &selected));
    TEST_ASSERT_EQUAL_INT(
        VK_PRESENT_MODE_FIFO_KHR,
        vulkan_renderer_choose_present_mode(NULL, 0));
}

void test_renderer_creates_swapchain_and_presents_triangle(void)
{
    SDL_Window *window = SDL_CreateWindow(
        "WolfVK Vulkan integration test", 640, 480,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN);
    VulkanRenderer *renderer = NULL;
    bool renderer_created;
    bool frame_rendered = false;

    TEST_ASSERT_NOT_NULL_MESSAGE(window, SDL_GetError());

    renderer_created = vulkan_renderer_create(window, &renderer);
    if (renderer_created) {
        frame_rendered = vulkan_renderer_draw_frame(renderer);
    }

    vulkan_renderer_destroy(renderer);
    SDL_DestroyWindow(window);

    TEST_ASSERT_TRUE_MESSAGE(renderer_created, "Vulkan renderer initialisation failed");
    TEST_ASSERT_TRUE_MESSAGE(frame_rendered, "Vulkan frame submission or presentation failed");
}

void test_device_scoring_prefers_discrete_gpus(void)
{
    TEST_ASSERT_TRUE(
        vulkan_renderer_score_device_type(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) >
        vulkan_renderer_score_device_type(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU));
    TEST_ASSERT_TRUE(
        vulkan_renderer_score_device_type(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) >
        vulkan_renderer_score_device_type(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU));
    TEST_ASSERT_TRUE(
        vulkan_renderer_score_device_type(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) >
        vulkan_renderer_score_device_type(VK_PHYSICAL_DEVICE_TYPE_CPU));
}
