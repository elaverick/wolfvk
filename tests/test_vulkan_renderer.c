#include "unity.h"

#include "vulkan_renderer_internal.h"
#include "wolfvk/wall_textures.h"

#include <string.h>

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

void test_renderer_creates_swapchain_and_presents_quads(void)
{
    const VulkanQuad quads[] = {
        {{
            {{-0.8f, -0.7f, 0.7f}, {0.95f, 0.28f, 0.16f}, {0.0f, 1.0f}, 0u, 1u},
            {{0.8f, -0.7f, 0.7f}, {0.20f, 0.75f, 0.92f}, {1.0f, 1.0f}, 0u, 1u},
            {{0.8f, 0.7f, 0.7f}, {0.95f, 0.78f, 0.22f}, {1.0f, 0.0f}, 0u, 1u},
            {{-0.8f, 0.7f, 0.7f}, {0.20f, 0.80f, 0.35f}, {0.0f, 0.0f}, 0u, 1u}
        }},
        {{
            {{-0.3f, -0.3f, 0.3f}, {0.95f, 0.20f, 0.65f}},
            {{0.3f, -0.3f, 0.3f}, {0.95f, 0.20f, 0.65f}},
            {{0.3f, 0.3f, 0.3f}, {0.95f, 0.20f, 0.65f}},
            {{-0.3f, 0.3f, 0.3f}, {0.95f, 0.20f, 0.65f}}
        }}
    };
    SDL_Window *window = SDL_CreateWindow(
        "WolfVK Vulkan quad integration test", 640, 480,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN);
    uint8_t white_wall_page[WOLF_WALL_TEXTURE_RGBA_SIZE];
    VulkanRenderer *renderer = NULL;
    bool renderer_created;
    bool textures_uploaded = false;
    bool geometry_uploaded = false;
    bool frame_rendered = false;

    TEST_ASSERT_NOT_NULL_MESSAGE(window, SDL_GetError());
    memset(white_wall_page, 255, sizeof(white_wall_page));

    renderer_created = vulkan_renderer_create(window, &renderer);
    if (renderer_created) {
        textures_uploaded = vulkan_renderer_set_wall_textures(
            renderer, white_wall_page, 1u);
        if (textures_uploaded) {
            geometry_uploaded = vulkan_renderer_set_quads(
                renderer, quads, (uint32_t)(sizeof(quads) / sizeof(quads[0])));
        }
        if (geometry_uploaded) {
            frame_rendered = vulkan_renderer_draw_frame(renderer);
        }
    }

    vulkan_renderer_destroy(renderer);
    SDL_DestroyWindow(window);

    TEST_ASSERT_TRUE_MESSAGE(renderer_created, "Vulkan renderer initialisation failed");
    TEST_ASSERT_TRUE_MESSAGE(textures_uploaded, "Vulkan texture array upload failed");
    TEST_ASSERT_TRUE_MESSAGE(geometry_uploaded, "Vulkan quad buffer upload failed");
    TEST_ASSERT_TRUE_MESSAGE(frame_rendered, "Vulkan quad rendering or presentation failed");
}

/* Apply a column-major matrix to a point. */
static void transform_point(const float matrix[16], const float point[3], float result[3])
{
    for (int row = 0; row < 3; ++row) {
        result[row] = matrix[row] * point[0] + matrix[4 + row] * point[1] +
            matrix[8 + row] * point[2] + matrix[12 + row];
    }
}

void test_model_matrix_turns_north_facing_models_to_their_yaw(void)
{
    const float model_front[3] = {0.0f, 1.0f, -1.0f};
    const float model_right[3] = {1.0f, 0.0f, 0.0f};
    VulkanModelInstance instance = {0u, 1u, {10.0f, 0.5f, -4.0f}, 0.0f};
    float matrix[16];
    float point[3];

    /* Yaw zero leaves the model facing north and only moves it. */
    vulkan_renderer_model_matrix(&instance, matrix);
    transform_point(matrix, model_front, point);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 10.0f, point[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.5f, point[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -5.0f, point[2]);

    /* A quarter turn faces east, matching the camera's yaw: the front moves to +x, the right to +z (south). */
    instance.yaw_radians = 1.57079632679f;
    vulkan_renderer_model_matrix(&instance, matrix);
    transform_point(matrix, model_front, point);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 11.0f, point[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -4.0f, point[2]);
    transform_point(matrix, model_right, point);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 10.0f, point[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -3.0f, point[2]);
}

void test_renderer_draws_placed_model_instances(void)
{
    const VulkanQuad environment[] = {
        {{
            {{-0.8f, -0.7f, 0.7f}, {0.4f, 0.4f, 0.4f}},
            {{0.8f, -0.7f, 0.7f}, {0.4f, 0.4f, 0.4f}},
            {{0.8f, 0.7f, 0.7f}, {0.4f, 0.4f, 0.4f}},
            {{-0.8f, 0.7f, 0.7f}, {0.4f, 0.4f, 0.4f}}
        }}
    };
    const VulkanQuad model_quads[] = {
        {{
            {{-0.1f, 0.0f, 0.0f}, {0.9f, 0.1f, 0.1f}},
            {{0.1f, 0.0f, 0.0f}, {0.9f, 0.1f, 0.1f}},
            {{0.1f, 0.2f, 0.0f}, {0.9f, 0.1f, 0.1f}},
            {{-0.1f, 0.2f, 0.0f}, {0.9f, 0.1f, 0.1f}}
        }},
        {{
            {{0.0f, 0.0f, -0.1f}, {0.1f, 0.9f, 0.1f}},
            {{0.0f, 0.0f, 0.1f}, {0.1f, 0.9f, 0.1f}},
            {{0.0f, 0.2f, 0.1f}, {0.1f, 0.9f, 0.1f}},
            {{0.0f, 0.2f, -0.1f}, {0.1f, 0.9f, 0.1f}}
        }}
    };
    const VulkanModelInstance instances[] = {
        {0u, 2u, {-0.3f, -0.1f, 0.3f}, 0.0f},
        {1u, 1u, {0.3f, -0.1f, 0.3f}, 0.8f}
    };
    const VulkanModelInstance out_of_range = {1u, 2u, {0.0f, 0.0f, 0.0f}, 0.0f};
    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };
    SDL_Window *window = SDL_CreateWindow(
        "WolfVK Vulkan model integration test", 320, 240,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN);
    uint8_t white_wall_page[WOLF_WALL_TEXTURE_RGBA_SIZE];
    VulkanRenderer *renderer = NULL;
    bool renderer_created;
    bool scene_ready = false;
    bool models_uploaded = false;
    bool out_of_range_rejected = false;
    bool instances_placed = false;
    bool frame_rendered = false;
    bool meshes_replaced = false;
    bool instances_cleared = false;

    TEST_ASSERT_NOT_NULL_MESSAGE(window, SDL_GetError());
    memset(white_wall_page, 255, sizeof(white_wall_page));

    renderer_created = vulkan_renderer_create(window, &renderer);
    if (renderer_created) {
        scene_ready = vulkan_renderer_set_wall_textures(renderer, white_wall_page, 1u) &&
            vulkan_renderer_set_quads(renderer, environment, 1u) &&
            vulkan_renderer_set_view_projection(renderer, identity);
    }
    if (scene_ready) {
        models_uploaded = vulkan_renderer_set_model_meshes(renderer, model_quads, 2u);
    }
    if (models_uploaded) {
        out_of_range_rejected = !vulkan_renderer_set_model_instances(renderer, &out_of_range, 1u);
        instances_placed = vulkan_renderer_set_model_instances(renderer, instances, 2u);
    }
    if (instances_placed) {
        frame_rendered = vulkan_renderer_draw_frame(renderer) && vulkan_renderer_draw_frame(renderer);
    }
    if (frame_rendered) {
        /* Replacing the meshes invalidates every instance range drawn from the old mesh. */
        meshes_replaced = vulkan_renderer_set_model_meshes(renderer, model_quads, 1u);
        instances_cleared = renderer->model_instance_count == 0u &&
            vulkan_renderer_draw_frame(renderer);
    }

    vulkan_renderer_destroy(renderer);
    SDL_DestroyWindow(window);

    TEST_ASSERT_TRUE_MESSAGE(renderer_created, "Vulkan renderer initialisation failed");
    TEST_ASSERT_TRUE_MESSAGE(scene_ready, "Vulkan level scene setup failed");
    TEST_ASSERT_TRUE_MESSAGE(models_uploaded, "Vulkan model mesh upload failed");
    TEST_ASSERT_TRUE_MESSAGE(out_of_range_rejected, "An instance outside the model mesh was accepted");
    TEST_ASSERT_TRUE_MESSAGE(instances_placed, "Vulkan model instances were rejected");
    TEST_ASSERT_TRUE_MESSAGE(frame_rendered, "Vulkan model rendering or presentation failed");
    TEST_ASSERT_TRUE_MESSAGE(meshes_replaced, "Vulkan model mesh replacement failed");
    TEST_ASSERT_TRUE_MESSAGE(instances_cleared, "Old model instances survived a mesh replacement");
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
