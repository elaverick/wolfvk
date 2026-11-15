#include "unity.h"

#include "wolfvk/vulkan_renderer.h"

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
