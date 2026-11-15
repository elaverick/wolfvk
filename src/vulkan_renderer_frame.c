#include "vulkan_renderer_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool vulkan_renderer_choose_surface_format(
    const VkSurfaceFormatKHR *available_formats,
    uint32_t format_count,
    VkSurfaceFormatKHR *chosen_format)
{
    uint32_t format_index;

    if (available_formats == NULL || chosen_format == NULL || format_count == 0u) {
        return false;
    }

    if (format_count == 1u && available_formats[0].format == VK_FORMAT_UNDEFINED) {
        chosen_format->format = VK_FORMAT_B8G8R8A8_SRGB;
        chosen_format->colorSpace = available_formats[0].colorSpace;
        return true;
    }

    for (format_index = 0; format_index < format_count; ++format_index) {
        if (available_formats[format_index].format == VK_FORMAT_B8G8R8A8_SRGB &&
            available_formats[format_index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            *chosen_format = available_formats[format_index];
            return true;
        }
    }

    *chosen_format = available_formats[0];
    return true;
}

VkPresentModeKHR vulkan_renderer_choose_present_mode(
    const VkPresentModeKHR *available_modes,
    uint32_t mode_count)
{
    uint32_t mode_index;

    if (available_modes == NULL || mode_count == 0u) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    for (mode_index = 0; mode_index < mode_count; ++mode_index) {
        if (available_modes[mode_index] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_swapchain_extent(
    const VkSurfaceCapabilitiesKHR *capabilities,
    SDL_Window *window)
{
    VkExtent2D extent;
    int drawable_width = 0;
    int drawable_height = 0;

    if (capabilities->currentExtent.width != UINT32_MAX) {
        return capabilities->currentExtent;
    }

    SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);

    extent.width = (uint32_t)drawable_width;
    extent.height = (uint32_t)drawable_height;

    if (extent.width < capabilities->minImageExtent.width) {
        extent.width = capabilities->minImageExtent.width;
    } else if (extent.width > capabilities->maxImageExtent.width) {
        extent.width = capabilities->maxImageExtent.width;
    }

    if (extent.height < capabilities->minImageExtent.height) {
        extent.height = capabilities->minImageExtent.height;
    } else if (extent.height > capabilities->maxImageExtent.height) {
        extent.height = capabilities->maxImageExtent.height;
    }

    return extent;
}

static VkCompositeAlphaFlagBitsKHR choose_composite_alpha(VkCompositeAlphaFlagsKHR supported)
{
    static const VkCompositeAlphaFlagBitsKHR preferred_modes[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    };
    size_t mode_index;

    for (mode_index = 0; mode_index < sizeof(preferred_modes) / sizeof(preferred_modes[0]);
         ++mode_index) {
        if ((supported & preferred_modes[mode_index]) != 0u) {
            return preferred_modes[mode_index];
        }
    }

    return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

static bool create_swapchain(VulkanRenderer *renderer)
{
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *available_formats = NULL;
    VkPresentModeKHR *available_modes = NULL;
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
    uint32_t format_count = 0;
    uint32_t mode_count = 0;
    uint32_t image_count;
    uint32_t queue_family_indices[2];
    VkSwapchainCreateInfoKHR create_info = {0};
    VkResult result;

    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        renderer->physical_device, renderer->surface, &capabilities);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Could not query Vulkan surface capabilities (%d).\n", result);
        return false;
    }

    if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0u) {
        fprintf(stderr, "Vulkan surface images cannot be used as color attachments.\n");
        return false;
    }

    if (vkGetPhysicalDeviceSurfaceFormatsKHR(
            renderer->physical_device, renderer->surface, &format_count, NULL) != VK_SUCCESS ||
        format_count == 0u) {
        fprintf(stderr, "The Vulkan surface does not expose any image formats.\n");
        return false;
    }

    available_formats = calloc(format_count, sizeof(*available_formats));
    if (available_formats == NULL) {
        return false;
    }

    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        renderer->physical_device, renderer->surface, &format_count, available_formats);
    if (result != VK_SUCCESS) {
        free(available_formats);
        return false;
    }

    if (vkGetPhysicalDeviceSurfacePresentModesKHR(
            renderer->physical_device, renderer->surface, &mode_count, NULL) != VK_SUCCESS ||
        mode_count == 0u) {
        free(available_formats);
        fprintf(stderr, "The Vulkan surface does not expose any presentation modes.\n");
        return false;
    }

    available_modes = calloc(mode_count, sizeof(*available_modes));
    if (available_modes == NULL) {
        free(available_formats);
        return false;
    }

    result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        renderer->physical_device, renderer->surface, &mode_count, available_modes);
    if (result != VK_SUCCESS ||
        !vulkan_renderer_choose_surface_format(available_formats, format_count, &surface_format)) {
        free(available_formats);
        free(available_modes);
        return false;
    }

    present_mode = vulkan_renderer_choose_present_mode(available_modes, mode_count);
    extent = choose_swapchain_extent(&capabilities, renderer->window);
    free(available_formats);
    free(available_modes);

    if (extent.width == 0u || extent.height == 0u) {
        return false;
    }

    image_count = capabilities.minImageCount + 1u;
    if (capabilities.maxImageCount > 0u && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    queue_family_indices[0] = renderer->graphics_queue_family;
    queue_family_indices[1] = renderer->present_queue_family;

    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = renderer->surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (renderer->graphics_queue_family != renderer->present_queue_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = choose_composite_alpha(capabilities.supportedCompositeAlpha);
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    result = vkCreateSwapchainKHR(renderer->device, &create_info, NULL, &renderer->swapchain);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan swapchain creation failed (%d).\n", result);
        return false;
    }

    renderer->swapchain_format = surface_format.format;
    renderer->swapchain_extent = extent;

    if (vkGetSwapchainImagesKHR(
            renderer->device, renderer->swapchain, &renderer->swapchain_image_count, NULL) != VK_SUCCESS ||
        renderer->swapchain_image_count == 0u) {
        return false;
    }

    renderer->swapchain_images = calloc(
        renderer->swapchain_image_count, sizeof(*renderer->swapchain_images));
    if (renderer->swapchain_images == NULL) {
        return false;
    }

    result = vkGetSwapchainImagesKHR(
        renderer->device, renderer->swapchain,
        &renderer->swapchain_image_count, renderer->swapchain_images);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Could not retrieve Vulkan swapchain images (%d).\n", result);
        return false;
    }

    return true;
}

static bool create_image_views(VulkanRenderer *renderer)
{
    uint32_t image_index;

    renderer->swapchain_image_views = calloc(
        renderer->swapchain_image_count, sizeof(*renderer->swapchain_image_views));
    if (renderer->swapchain_image_views == NULL) {
        return false;
    }

    for (image_index = 0; image_index < renderer->swapchain_image_count; ++image_index) {
        VkImageViewCreateInfo create_info = {0};
        VkResult result;

        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = renderer->swapchain_images[image_index];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = renderer->swapchain_format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        result = vkCreateImageView(
            renderer->device, &create_info, NULL,
            &renderer->swapchain_image_views[image_index]);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "Vulkan swapchain image view creation failed (%d).\n", result);
            return false;
        }
    }

    return true;
}

static bool create_render_pass(VulkanRenderer *renderer)
{
    VkAttachmentDescription color_attachment = {0};
    VkAttachmentReference color_attachment_reference = {0};
    VkSubpassDescription subpass = {0};
    VkSubpassDependency dependency = {0};
    VkRenderPassCreateInfo create_info = {0};
    VkResult result;

    color_attachment.format = renderer->swapchain_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    color_attachment_reference.attachment = 0;
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_reference;

    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &color_attachment;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;

    result = vkCreateRenderPass(renderer->device, &create_info, NULL, &renderer->render_pass);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan render pass creation failed (%d).\n", result);
        return false;
    }

    return true;
}

static FILE *open_shader_file(const char *path)
{
    FILE *shader_file = NULL;

#if defined(_MSC_VER)
    if (fopen_s(&shader_file, path, "rb") != 0) {
        return NULL;
    }
#else
    shader_file = fopen(path, "rb");
#endif

    return shader_file;
}

static bool load_shader_module(
    VulkanRenderer *renderer,
    const char *shader_filename,
    VkShaderModule *shader_module)
{
    const char *base_path = SDL_GetBasePath();
    char shader_path[1024];
    FILE *shader_file = NULL;
    long shader_size;
    uint32_t *shader_code = NULL;
    VkShaderModuleCreateInfo create_info = {0};
    VkResult result;

    if (base_path == NULL) {
        fprintf(stderr, "Could not determine the application directory: %s\n", SDL_GetError());
        return false;
    }

    if (snprintf(shader_path, sizeof(shader_path), "%sshaders/%s", base_path, shader_filename) < 0 ||
        strlen(shader_path) >= sizeof(shader_path)) {
        return false;
    }

    shader_file = open_shader_file(shader_path);
    if (shader_file == NULL) {
        fprintf(stderr, "Could not open compiled shader: %s\n", shader_path);
        return false;
    }

    if (fseek(shader_file, 0, SEEK_END) != 0) {
        fclose(shader_file);
        return false;
    }

    shader_size = ftell(shader_file);
    if (shader_size <= 0 || (shader_size % (long)sizeof(uint32_t)) != 0 ||
        fseek(shader_file, 0, SEEK_SET) != 0) {
        fclose(shader_file);
        return false;
    }

    shader_code = malloc((size_t)shader_size);
    if (shader_code == NULL) {
        fclose(shader_file);
        return false;
    }

    if (fread(shader_code, 1, (size_t)shader_size, shader_file) != (size_t)shader_size) {
        free(shader_code);
        fclose(shader_file);
        return false;
    }
    fclose(shader_file);

    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = (size_t)shader_size;
    create_info.pCode = shader_code;

    result = vkCreateShaderModule(renderer->device, &create_info, NULL, shader_module);
    free(shader_code);

    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan shader module creation failed (%d).\n", result);
        return false;
    }

    return true;
}

static bool create_graphics_pipeline(VulkanRenderer *renderer)
{
    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo shader_stages[2] = {0};
    VkPipelineVertexInputStateCreateInfo vertex_input = {0};
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
    VkViewport viewport = {0};
    VkRect2D scissor = {0};
    VkPipelineViewportStateCreateInfo viewport_state = {0};
    VkPipelineRasterizationStateCreateInfo rasterizer = {0};
    VkPipelineMultisampleStateCreateInfo multisampling = {0};
    VkPipelineColorBlendAttachmentState color_blend_attachment = {0};
    VkPipelineColorBlendStateCreateInfo color_blending = {0};
    VkPipelineLayoutCreateInfo layout_info = {0};
    VkGraphicsPipelineCreateInfo pipeline_info = {0};
    VkResult result;

    if (!load_shader_module(renderer, "triangle.vert.spv", &vertex_shader) ||
        !load_shader_module(renderer, "triangle.frag.spv", &fragment_shader)) {
        if (vertex_shader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(renderer->device, vertex_shader, NULL);
        }
        if (fragment_shader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(renderer->device, fragment_shader, NULL);
        }
        return false;
    }

    shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = vertex_shader;
    shader_stages[0].pName = "main";

    shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = fragment_shader;
    shader_stages[1].pName = "main";

    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)renderer->swapchain_extent.width;
    viewport.height = (float)renderer->swapchain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = renderer->swapchain_extent;

    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.lineWidth = 1.0f;

    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    result = vkCreatePipelineLayout(
        renderer->device, &layout_info, NULL, &renderer->pipeline_layout);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan pipeline layout creation failed (%d).\n", result);
        vkDestroyShaderModule(renderer->device, fragment_shader, NULL);
        vkDestroyShaderModule(renderer->device, vertex_shader, NULL);
        return false;
    }

    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = renderer->pipeline_layout;
    pipeline_info.renderPass = renderer->render_pass;
    pipeline_info.subpass = 0;

    result = vkCreateGraphicsPipelines(
        renderer->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
        &renderer->graphics_pipeline);

    vkDestroyShaderModule(renderer->device, fragment_shader, NULL);
    vkDestroyShaderModule(renderer->device, vertex_shader, NULL);

    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan graphics pipeline creation failed (%d).\n", result);
        return false;
    }

    return true;
}

static bool create_framebuffers(VulkanRenderer *renderer)
{
    uint32_t image_index;

    renderer->framebuffers = calloc(
        renderer->swapchain_image_count, sizeof(*renderer->framebuffers));
    if (renderer->framebuffers == NULL) {
        return false;
    }

    for (image_index = 0; image_index < renderer->swapchain_image_count; ++image_index) {
        VkFramebufferCreateInfo create_info = {0};
        VkResult result;

        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = renderer->render_pass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = &renderer->swapchain_image_views[image_index];
        create_info.width = renderer->swapchain_extent.width;
        create_info.height = renderer->swapchain_extent.height;
        create_info.layers = 1;

        result = vkCreateFramebuffer(
            renderer->device, &create_info, NULL, &renderer->framebuffers[image_index]);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "Vulkan framebuffer creation failed (%d).\n", result);
            return false;
        }
    }

    return true;
}

static bool create_command_pool(VulkanRenderer *renderer)
{
    VkCommandPoolCreateInfo create_info = {0};
    VkResult result;

    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_info.queueFamilyIndex = renderer->graphics_queue_family;

    result = vkCreateCommandPool(renderer->device, &create_info, NULL, &renderer->command_pool);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan command pool creation failed (%d).\n", result);
        return false;
    }

    return true;
}

static bool allocate_command_buffers(VulkanRenderer *renderer)
{
    VkCommandBufferAllocateInfo allocate_info = {0};
    VkResult result;

    renderer->command_buffers = calloc(
        renderer->swapchain_image_count, sizeof(*renderer->command_buffers));
    if (renderer->command_buffers == NULL) {
        return false;
    }

    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = renderer->command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = renderer->swapchain_image_count;

    result = vkAllocateCommandBuffers(
        renderer->device, &allocate_info, renderer->command_buffers);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan command buffer allocation failed (%d).\n", result);
        free(renderer->command_buffers);
        renderer->command_buffers = NULL;
        return false;
    }

    return true;
}

static bool create_sync_objects(VulkanRenderer *renderer)
{
    VkSemaphoreCreateInfo semaphore_info = {0};
    VkFenceCreateInfo fence_info = {0};
    VkResult result;

    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    result = vkCreateSemaphore(
        renderer->device, &semaphore_info, NULL, &renderer->image_available_semaphore);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan image-available semaphore creation failed (%d).\n", result);
        return false;
    }

    result = vkCreateFence(renderer->device, &fence_info, NULL, &renderer->in_flight_fence);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan frame fence creation failed (%d).\n", result);
        return false;
    }

    return true;
}

static bool create_render_finished_semaphores(VulkanRenderer *renderer)
{
    uint32_t image_index;
    VkSemaphoreCreateInfo semaphore_info = {0};

    renderer->render_finished_semaphores = calloc(
        renderer->swapchain_image_count, sizeof(*renderer->render_finished_semaphores));
    if (renderer->render_finished_semaphores == NULL) {
        return false;
    }

    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (image_index = 0; image_index < renderer->swapchain_image_count; ++image_index) {
        VkResult result = vkCreateSemaphore(
            renderer->device, &semaphore_info, NULL,
            &renderer->render_finished_semaphores[image_index]);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "Vulkan render-finished semaphore creation failed (%d).\n", result);
            return false;
        }
    }

    return true;
}

static void destroy_swapchain_resources(VulkanRenderer *renderer)
{
    uint32_t image_index;

    if (renderer->render_finished_semaphores != NULL) {
        for (image_index = 0; image_index < renderer->swapchain_image_count; ++image_index) {
            if (renderer->render_finished_semaphores[image_index] != VK_NULL_HANDLE) {
                vkDestroySemaphore(
                    renderer->device, renderer->render_finished_semaphores[image_index], NULL);
            }
        }
    }
    free(renderer->render_finished_semaphores);
    renderer->render_finished_semaphores = NULL;

    if (renderer->command_buffers != NULL && renderer->command_pool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(
            renderer->device, renderer->command_pool,
            renderer->swapchain_image_count, renderer->command_buffers);
    }
    free(renderer->command_buffers);
    renderer->command_buffers = NULL;

    if (renderer->framebuffers != NULL) {
        for (image_index = 0; image_index < renderer->swapchain_image_count; ++image_index) {
            if (renderer->framebuffers[image_index] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(renderer->device, renderer->framebuffers[image_index], NULL);
            }
        }
    }
    free(renderer->framebuffers);
    renderer->framebuffers = NULL;

    if (renderer->graphics_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(renderer->device, renderer->graphics_pipeline, NULL);
        renderer->graphics_pipeline = VK_NULL_HANDLE;
    }
    if (renderer->pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(renderer->device, renderer->pipeline_layout, NULL);
        renderer->pipeline_layout = VK_NULL_HANDLE;
    }
    if (renderer->render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(renderer->device, renderer->render_pass, NULL);
        renderer->render_pass = VK_NULL_HANDLE;
    }

    if (renderer->swapchain_image_views != NULL) {
        for (image_index = 0; image_index < renderer->swapchain_image_count; ++image_index) {
            if (renderer->swapchain_image_views[image_index] != VK_NULL_HANDLE) {
                vkDestroyImageView(
                    renderer->device, renderer->swapchain_image_views[image_index], NULL);
            }
        }
    }
    free(renderer->swapchain_image_views);
    renderer->swapchain_image_views = NULL;

    if (renderer->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
        renderer->swapchain = VK_NULL_HANDLE;
    }
    free(renderer->swapchain_images);
    renderer->swapchain_images = NULL;
    renderer->swapchain_image_count = 0;
}

bool vulkan_renderer_frame_resources_create(VulkanRenderer *renderer)
{
    if (!create_swapchain(renderer) ||
        !create_image_views(renderer) ||
        !create_render_pass(renderer) ||
        !create_graphics_pipeline(renderer) ||
        !create_framebuffers(renderer) ||
        !create_command_pool(renderer) ||
        !allocate_command_buffers(renderer) ||
        !create_sync_objects(renderer) ||
        !create_render_finished_semaphores(renderer)) {
        return false;
    }

    return true;
}

void vulkan_renderer_frame_resources_destroy(VulkanRenderer *renderer)
{
    if (renderer->device == VK_NULL_HANDLE) {
        return;
    }

    destroy_swapchain_resources(renderer);

    if (renderer->command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(renderer->device, renderer->command_pool, NULL);
        renderer->command_pool = VK_NULL_HANDLE;
    }
    if (renderer->image_available_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(renderer->device, renderer->image_available_semaphore, NULL);
        renderer->image_available_semaphore = VK_NULL_HANDLE;
    }
    if (renderer->in_flight_fence != VK_NULL_HANDLE) {
        vkDestroyFence(renderer->device, renderer->in_flight_fence, NULL);
        renderer->in_flight_fence = VK_NULL_HANDLE;
    }
}

bool vulkan_renderer_frame_resources_recreate(VulkanRenderer *renderer)
{
    int drawable_width = 0;
    int drawable_height = 0;

    SDL_GetWindowSizeInPixels(renderer->window, &drawable_width, &drawable_height);
    if (drawable_width == 0 || drawable_height == 0) {
        return true;
    }

    if (vkDeviceWaitIdle(renderer->device) != VK_SUCCESS) {
        return false;
    }

    destroy_swapchain_resources(renderer);

    if (!create_swapchain(renderer) ||
        !create_image_views(renderer) ||
        !create_render_pass(renderer) ||
        !create_graphics_pipeline(renderer) ||
        !create_framebuffers(renderer) ||
        !allocate_command_buffers(renderer) ||
        !create_render_finished_semaphores(renderer)) {
        return false;
    }

    renderer->framebuffer_resized = false;
    return true;
}

static bool record_command_buffer(
    VulkanRenderer *renderer,
    VkCommandBuffer command_buffer,
    uint32_t image_index)
{
    VkCommandBufferBeginInfo begin_info = {0};
    VkClearValue clear_color = {{{0.025f, 0.035f, 0.065f, 1.0f}}};
    VkRenderPassBeginInfo render_pass_info = {0};
    VkResult result;

    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan command buffer begin failed (%d).\n", result);
        return false;
    }

    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = renderer->render_pass;
    render_pass_info.framebuffer = renderer->framebuffers[image_index];
    render_pass_info.renderArea.offset.x = 0;
    render_pass_info.renderArea.offset.y = 0;
    render_pass_info.renderArea.extent = renderer->swapchain_extent;
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->graphics_pipeline);
    vkCmdDraw(command_buffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(command_buffer);

    result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan command buffer recording failed (%d).\n", result);
        return false;
    }

    return true;
}

bool vulkan_renderer_frame_resources_draw(VulkanRenderer *renderer)
{
    VkResult result;
    uint32_t image_index = 0;
    bool acquired_suboptimal = false;
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {0};
    VkPresentInfoKHR present_info = {0};
    VkCommandBuffer command_buffer;

    if (renderer->swapchain == VK_NULL_HANDLE) {
        return false;
    }

    if (renderer->framebuffer_resized) {
        if (!vulkan_renderer_frame_resources_recreate(renderer)) {
            return false;
        }
        if (renderer->framebuffer_resized) {
            return true;
        }
    }

    result = vkWaitForFences(
        renderer->device, 1, &renderer->in_flight_fence, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Waiting for the Vulkan frame fence failed (%d).\n", result);
        return false;
    }

    result = vkAcquireNextImageKHR(
        renderer->device, renderer->swapchain, UINT64_MAX,
        renderer->image_available_semaphore, VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return vulkan_renderer_frame_resources_recreate(renderer);
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "Acquiring a Vulkan swapchain image failed (%d).\n", result);
        return false;
    }
    acquired_suboptimal = result == VK_SUBOPTIMAL_KHR;

    command_buffer = renderer->command_buffers[image_index];
    result = vkResetFences(renderer->device, 1, &renderer->in_flight_fence);
    if (result != VK_SUCCESS || vkResetCommandBuffer(command_buffer, 0) != VK_SUCCESS ||
        !record_command_buffer(renderer, command_buffer, image_index)) {
        fprintf(stderr, "Could not prepare the Vulkan frame command buffer.\n");
        return false;
    }

    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &renderer->image_available_semaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &renderer->render_finished_semaphores[image_index];

    result = vkQueueSubmit(
        renderer->graphics_queue, 1, &submit_info, renderer->in_flight_fence);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Submitting the Vulkan frame failed (%d).\n", result);
        return false;
    }

    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &renderer->render_finished_semaphores[image_index];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &renderer->swapchain;
    present_info.pImageIndices = &image_index;

    result = vkQueuePresentKHR(renderer->present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        renderer->framebuffer_resized || acquired_suboptimal) {
        return vulkan_renderer_frame_resources_recreate(renderer);
    }
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Presenting the Vulkan frame failed (%d).\n", result);
        return false;
    }

    return true;
}
