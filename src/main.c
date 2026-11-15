#include "wolfvk/input.h"
#include "wolfvk/vulkan_renderer.h"

#include <stdio.h>

int main(void)
{
    SDL_Window *window;
    SDL_Event event;
    const VulkanQuad demo_quad = {{
        {{-0.65f, -0.45f, 0.5f}, {0.95f, 0.28f, 0.16f}},
        {{0.65f, -0.45f, 0.5f}, {0.20f, 0.75f, 0.92f}},
        {{0.65f, 0.45f, 0.5f}, {0.95f, 0.78f, 0.22f}},
        {{-0.65f, 0.45f, 0.5f}, {0.20f, 0.80f, 0.35f}}
    }};
    VulkanRenderer *renderer = NULL;
    WolfInput *input = NULL;
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

    if (!vulkan_renderer_set_quads(renderer, &demo_quad, 1)) {
        fprintf(stderr, "Could not upload demo quad geometry.\n");
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

    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        fprintf(stderr, "Could not enable relative mouse mode: %s\n", SDL_GetError());
    }

    while (running) {
        wolf_input_begin_frame(input);
        while (SDL_PollEvent(&event)) {
            wolf_input_handle_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                running = 0;
            } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                vulkan_renderer_notify_resize(renderer);
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
