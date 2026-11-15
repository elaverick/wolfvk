#include "wolfvk/vulkan_renderer.h"

#include <stdio.h>

int main(void)
{
    SDL_Window *window;
    SDL_Event event;
    VulkanRenderer *renderer = NULL;
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

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = 0;
            }
        }
        if (!vulkan_renderer_draw_frame(renderer)) {
            fprintf(stderr, "Vulkan frame rendering failed.\n");
            running = 0;
        }
        SDL_Delay(1);
    }

    vulkan_renderer_destroy(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
