#include "vulkan_renderer_internal.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Choose a swapchain format suitable for predictable 8-bit colour output on desktop displays. */
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

/* Create the presentation images at the drawable size and honour the surface's queue-sharing rules. */
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
    VkPipelineDepthStencilStateCreateInfo overlay_depth_stencil = {0};

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

/* Give each swapchain image a view so the render pass can use it as a colour attachment. */
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

/* Depth attachments need device-local memory; keep that requirement separate from host upload buffers. */
static bool find_depth_memory_type(
    VulkanRenderer *renderer,
    uint32_t type_filter,
    uint32_t *memory_type_index)
{
    VkPhysicalDeviceMemoryProperties memory_properties;

    vkGetPhysicalDeviceMemoryProperties(renderer->physical_device, &memory_properties);
    for (uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
        bool type_is_available = (type_filter & (1u << index)) != 0u;
        bool is_device_local =
            (memory_properties.memoryTypes[index].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0u;

        if (type_is_available && is_device_local) {
            *memory_type_index = index;
            return true;
        }
    }

    return false;
}

/* One shared depth image is safe while the renderer limits itself to one frame in flight. */
/* Select a depth format supported by the adapter and allocate one depth target for the swapchain. */
static bool create_depth_resources(VulkanRenderer *renderer)
{
    static const VkFormat depth_candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    VkFormatProperties format_properties;
    VkImageCreateInfo image_info = {0};
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo allocation_info = {0};
    VkImageViewCreateInfo view_info = {0};
    uint32_t memory_type_index = 0;
    VkResult result;
    bool depth_format_found = false;

    for (size_t index = 0; index < sizeof(depth_candidates) / sizeof(depth_candidates[0]); ++index) {
        vkGetPhysicalDeviceFormatProperties(
            renderer->physical_device, depth_candidates[index], &format_properties);
        if ((format_properties.optimalTilingFeatures &
             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u) {
            renderer->depth_format = depth_candidates[index];
            depth_format_found = true;
            break;
        }
    }

    if (!depth_format_found) {
        fprintf(stderr, "No supported Vulkan depth attachment format was found.\n");
        return false;
    }

    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = renderer->swapchain_extent.width;
    image_info.extent.height = renderer->swapchain_extent.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = renderer->depth_format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = vkCreateImage(renderer->device, &image_info, NULL, &renderer->depth_image);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan depth image creation failed (%d).\n", result);
        return false;
    }

    vkGetImageMemoryRequirements(
        renderer->device, renderer->depth_image, &memory_requirements);
    if (!find_depth_memory_type(
            renderer, memory_requirements.memoryTypeBits, &memory_type_index)) {
        fprintf(stderr, "No suitable Vulkan memory type for the depth image was found.\n");
        return false;
    }

    allocation_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation_info.allocationSize = memory_requirements.size;
    allocation_info.memoryTypeIndex = memory_type_index;
    result = vkAllocateMemory(
        renderer->device, &allocation_info, NULL, &renderer->depth_image_memory);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan depth image memory allocation failed (%d).\n", result);
        return false;
    }

    result = vkBindImageMemory(
        renderer->device, renderer->depth_image, renderer->depth_image_memory, 0);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan depth image memory binding failed (%d).\n", result);
        return false;
    }

    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = renderer->depth_image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = renderer->depth_format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    result = vkCreateImageView(
        renderer->device, &view_info, NULL, &renderer->depth_image_view);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan depth image view creation failed (%d).\n", result);
        return false;
    }

    return true;
}

/* Define the colour/depth attachment lifetime and the layout transitions required by each frame. */
static bool create_render_pass(VulkanRenderer *renderer)
{
    VkAttachmentDescription attachments[2] = {0};
    VkAttachmentReference color_attachment_reference = {0};
    VkAttachmentReference depth_attachment_reference = {0};
    VkSubpassDescription subpass = {0};
    VkSubpassDependency dependency = {0};
    VkRenderPassCreateInfo create_info = {0};
    VkResult result;

    attachments[0].format = renderer->swapchain_format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].format = renderer->depth_format;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    color_attachment_reference.attachment = 0;
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    depth_attachment_reference.attachment = 1;
    depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_reference;
    subpass.pDepthStencilAttachment = &depth_attachment_reference;

    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = 2;
    create_info.pAttachments = attachments;
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

/* Load the packaged SPIR-V relative to the executable so the demo is independent of its launch directory. */
static bool load_shader_module(
    VulkanRenderer *renderer,
    const char *shader_filename,
    VkShaderModule *shader_module)
{
    const char *base_path = SDL_GetBasePath();
    char shader_path[1024];
    FILE *shader_file = NULL;
    long shader_size;
    int shader_path_length;
    uint32_t *shader_code = NULL;
    VkShaderModuleCreateInfo create_info = {0};
    VkResult result;

    if (base_path == NULL) {
        fprintf(stderr, "Could not determine the application directory: %s\n", SDL_GetError());
        return false;
    }

    shader_path_length = snprintf(
        shader_path, sizeof(shader_path), "%sshaders/%s", base_path, shader_filename);
    if (shader_path_length < 0 || (size_t)shader_path_length >= sizeof(shader_path)) {
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

/* Bind the quad vertex layout, depth testing and shaders into the pipeline reused by every level frame. */
static bool create_graphics_pipelines(VulkanRenderer *renderer)
{
    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo shader_stages[2] = {0};
    VkPipelineVertexInputStateCreateInfo vertex_input = {0};
    VkVertexInputBindingDescription vertex_binding = {0};
    VkVertexInputAttributeDescription vertex_attributes[5] = {0};
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {0};
    VkPushConstantRange view_projection_range = {0};
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

    vertex_binding.binding = 0;
    vertex_binding.stride = (uint32_t)sizeof(VulkanVertex);
    vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_attributes[0].location = 0;
    vertex_attributes[0].binding = 0;
    vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[0].offset = (uint32_t)offsetof(VulkanVertex, position);

    vertex_attributes[1].location = 1;
    vertex_attributes[1].binding = 0;
    vertex_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[1].offset = (uint32_t)offsetof(VulkanVertex, colour);

    vertex_attributes[2].location = 2;
    vertex_attributes[2].binding = 0;
    vertex_attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attributes[2].offset = (uint32_t)offsetof(VulkanVertex, texcoord);

    vertex_attributes[3].location = 3;
    vertex_attributes[3].binding = 0;
    vertex_attributes[3].format = VK_FORMAT_R32_UINT;
    vertex_attributes[3].offset = (uint32_t)offsetof(VulkanVertex, texture_layer);

    vertex_attributes[4].location = 4;
    vertex_attributes[4].binding = 0;
    vertex_attributes[4].format = VK_FORMAT_R32_UINT;
    vertex_attributes[4].offset = (uint32_t)offsetof(VulkanVertex, use_texture);

    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &vertex_binding;
    vertex_input.vertexAttributeDescriptionCount = 5;
    vertex_input.pVertexAttributeDescriptions = vertex_attributes;

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

    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    view_projection_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    view_projection_range.offset = 0;
    view_projection_range.size = (uint32_t)sizeof(renderer->view_projection);

    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1u;
    layout_info.pSetLayouts = &renderer->wall_texture_set_layout;
    layout_info.pushConstantRangeCount = 1u;
    layout_info.pPushConstantRanges = &view_projection_range;
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
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = renderer->pipeline_layout;
    pipeline_info.renderPass = renderer->render_pass;
    pipeline_info.subpass = 0;

    result = vkCreateGraphicsPipelines(
        renderer->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
        &renderer->graphics_pipeline);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan graphics pipeline creation failed (%d).\n", result);
        vkDestroyShaderModule(renderer->device, fragment_shader, NULL);
        vkDestroyShaderModule(renderer->device, vertex_shader, NULL);
        return false;
    }

    /* Weapon geometry is a first-person overlay and must not inherit map depth. */
    overlay_depth_stencil = depth_stencil;
    overlay_depth_stencil.depthTestEnable = VK_FALSE;
    overlay_depth_stencil.depthWriteEnable = VK_FALSE;
    pipeline_info.pDepthStencilState = &overlay_depth_stencil;
    result = vkCreateGraphicsPipelines(
        renderer->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
        &renderer->weapon_pipeline);

    vkDestroyShaderModule(renderer->device, fragment_shader, NULL);
    vkDestroyShaderModule(renderer->device, vertex_shader, NULL);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan weapon overlay pipeline creation failed (%d).\n", result);
        return false;
    }
    return true;
}
/* Pair each presentation image with the depth attachment used while that image is rendered. */
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
        VkImageView attachments[2] = {
            renderer->swapchain_image_views[image_index],
            renderer->depth_image_view
        };
        VkResult result;

        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = renderer->render_pass;
        create_info.attachmentCount = 2;
        create_info.pAttachments = attachments;
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

/* Allocate command storage from the graphics family that owns the rendering queue. */
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

/* Keep one primary command buffer per framebuffer so each acquired image has matching recorded work. */
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

/* Synchronise CPU reuse of frame resources with GPU completion and image acquisition. */
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

/* Signal presentation per swapchain image, avoiding reuse while the presentation engine still owns a wait. */
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

/* Release all resources whose dimensions or image ownership are tied to the current swapchain. */
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

    if (renderer->depth_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(renderer->device, renderer->depth_image_view, NULL);
        renderer->depth_image_view = VK_NULL_HANDLE;
    }
    if (renderer->depth_image != VK_NULL_HANDLE) {
        vkDestroyImage(renderer->device, renderer->depth_image, NULL);
        renderer->depth_image = VK_NULL_HANDLE;
    }
    if (renderer->depth_image_memory != VK_NULL_HANDLE) {
        vkFreeMemory(renderer->device, renderer->depth_image_memory, NULL);
        renderer->depth_image_memory = VK_NULL_HANDLE;
    }

    if (renderer->weapon_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(renderer->device, renderer->weapon_pipeline, NULL);
        renderer->weapon_pipeline = VK_NULL_HANDLE;
    }
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

/* Construct frame resources in dependency order so failure can use the same partial teardown path. */
bool vulkan_renderer_frame_resources_create(VulkanRenderer *renderer)
{
    if (!create_swapchain(renderer) ||
        !create_image_views(renderer) ||
        !create_depth_resources(renderer) ||
        !create_render_pass(renderer) ||
        !create_graphics_pipelines(renderer) ||
        !create_framebuffers(renderer) ||
        !create_command_pool(renderer) ||
        !allocate_command_buffers(renderer) ||
        !create_sync_objects(renderer) ||
        !create_render_finished_semaphores(renderer)) {
        return false;
    }

    return true;
}

/* Destroy frame resources in reverse dependency order after the renderer has stopped GPU work. */
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

/* Rebuild size-dependent resources only when the window has a non-zero drawable extent. */
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
        !create_depth_resources(renderer) ||
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

/* Record one complete scene pass against the framebuffer belonging to the acquired image. */
static bool record_command_buffer(
    VulkanRenderer *renderer,
    VkCommandBuffer command_buffer,
    uint32_t image_index)
{
    VkCommandBufferBeginInfo begin_info = {0};
    VkClearValue clear_values[2] = {0};
    VkRenderPassBeginInfo render_pass_info = {0};
    VkDeviceSize vertex_buffer_offset = 0;
    VkResult result;

    if (renderer->wall_texture_descriptor_set == VK_NULL_HANDLE) {
        fprintf(stderr, "Wall textures must be uploaded before drawing the level.\n");
        return false;
    }

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
    clear_values[0].color.float32[0] = 0.025f;
    clear_values[0].color.float32[1] = 0.035f;
    clear_values[0].color.float32[2] = 0.065f;
    clear_values[0].color.float32[3] = 1.0f;
    clear_values[1].depthStencil.depth = 1.0f;
    clear_values[1].depthStencil.stencil = 0;

    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->graphics_pipeline);
    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        renderer->pipeline_layout,
        0u,
        1u,
        &renderer->wall_texture_descriptor_set,
        0u,
        NULL);
    vkCmdPushConstants(
        command_buffer, renderer->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
        0, (uint32_t)sizeof(renderer->view_projection), renderer->view_projection);

    if (renderer->index_count > 0u) {
        vkCmdBindVertexBuffers(
            command_buffer, 0, 1, &renderer->vertex_buffer, &vertex_buffer_offset);
        vkCmdBindIndexBuffer(
            command_buffer, renderer->index_buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(command_buffer, renderer->index_count, 1, 0, 0, 0);
    }
    vulkan_renderer_models_record(renderer, command_buffer);
    vulkan_renderer_weapon_record(renderer, command_buffer);
    vkCmdEndRenderPass(command_buffer);

    result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Vulkan command buffer recording failed (%d).\n", result);
        return false;
    }

    return true;
}

/* Acquire, submit and present one frame while handling out-of-date swapchains and resize requests. */
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
