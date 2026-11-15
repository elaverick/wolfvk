#include <SDL3/SDL.h>

#include <stdio.h>

int main(void)
{
    SDL_Window *window;
    SDL_WindowFlags flags;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL initialisation failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("WolfVK window smoke test", 640, 480,
                              SDL_WINDOW_HIDDEN);
    if (window == NULL) {
        fprintf(stderr, "SDL window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    flags = SDL_GetWindowFlags(window);
    if ((flags & SDL_WINDOW_HIDDEN) == 0u) {
        fprintf(stderr, "The smoke-test window was not created hidden.\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("SDL3 hidden-window creation passed.");
    return 0;
}
