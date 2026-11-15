#include "wolfvk/camera.h"
#include "wolfvk/input.h"
#include "wolfvk/vulkan_renderer.h"

#include <stdio.h>

static bool update_camera_projection(
    SDL_Window *window,
    VulkanRenderer *renderer,
    const WolfCamera *camera)
{
    int drawable_width = 0;
    int drawable_height = 0;
    float view_projection[16];

    SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
    if (drawable_width <= 0 || drawable_height <= 0) {
        return true;
    }

    if (!wolf_camera_build_view_projection(
            camera, (float)drawable_width / (float)drawable_height, view_projection)) {
        return false;
    }

    return vulkan_renderer_set_view_projection(renderer, view_projection);
}

int main(void)
{
    SDL_Window *window;
    SDL_Event event;
    /* The planes extend beyond the camera far clip so their edges remain unseen. */
    const VulkanQuad environment_quads[] = {
        {{
            {{-10000.0f, 0.0f, -10000.0f}, {0.16f, 0.20f, 0.24f}},
            {{1000.0f, 0.0f, -1000.0f}, {0.16f, 0.20f, 0.24f}},
            {{1000.0f, 0.0f, 1000.0f}, {0.16f, 0.20f, 0.24f}},
            {{-1000.0f, 0.0f, 1000.0f}, {0.16f, 0.20f, 0.24f}}
        }},
        {{
            {{-1000.0f, 3.2f, -1000.0f}, {0.11f, 0.14f, 0.20f}},
            {{1000.0f, 3.2f, -1000.0f}, {0.11f, 0.14f, 0.20f}},
            {{1000.0f, 3.2f, 1000.0f}, {0.11f, 0.14f, 0.20f}},
            {{-1000.0f, 3.2f, 1000.0f}, {0.11f, 0.14f, 0.20f}}
        }}
    };
    VulkanRenderer *renderer = NULL;
    WolfInput *input = NULL;
    WolfCamera camera;
    int running = 1;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL video initialisation failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow(
        "WolfVK", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    if (window == NULL) {
        fprintf(stderr, "SDL Vulkan window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (!vulkan_renderer_create(window, &renderer)) {
        fprintf(stderr, "Vulkan renderer initialisation failed.\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    wolf_camera_initialize(&camera);
    if (!update_camera_projection(window, renderer, &camera)) {
        fprintf(stderr, "Could not initialise the camera projection.\n");
        vulkan_renderer_destroy(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!vulkan_renderer_set_quads(
            renderer, environment_quads,
            (uint32_t)(sizeof(environment_quads) / sizeof(environment_quads[0])))) {
        fprintf(stderr, "Could not upload floor and roof geometry.\n");
        vulkan_renderer_destroy(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    input = wolf_input_create();
    if (input == NULL) {
        fprintf(stderr, "Input initialisation failed: %s\n", SDL_GetError());
        vulkan_renderer_destroy(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    while (running) {
        wolf_input_begin_frame(input);
        while (SDL_PollEvent(&event)) {
            wolf_input_handle_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                running = 0;
            } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                vulkan_renderer_notify_resize(renderer);
                if (!update_camera_projection(window, renderer, &camera)) {
                    fprintf(stderr, "Could not update the camera projection after resize.\n");
                    running = 0;
                }
            }
        }
        if (!vulkan_renderer_draw_frame(renderer)) {
            fprintf(stderr, "Vulkan frame rendering failed.\n");
            running = 0;
        }
        SDL_Delay(1);
    }

    wolf_input_destroy(input);
    vulkan_renderer_destroy(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
