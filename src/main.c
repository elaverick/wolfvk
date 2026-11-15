#include "wolfvk/camera.h"
#include "wolfvk/command_line.h"
#include "wolfvk/input.h"
#include "wolfvk/level.h"
#include "wolfvk/level_geometry.h"
#include "wolfvk/player_controller.h"
#include "wolfvk/vulkan_renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void print_usage(const char *program_name)
{
    printf("Usage: %s [--level NUMBER] [--help]\n", program_name);
    printf("Level numbers are one-based; if omitted, the first level is loaded.\n");
    printf("Place MAPHEAD.WL6 and GAMEMAPS.WL6 in the working directory.\n");
}

static bool build_scene_geometry(
    const WolfLevel *level,
    const VulkanQuad environment_quads[2],
    VulkanQuad **scene_quads,
    uint32_t *scene_quad_count,
    uint16_t *spawn_x,
    uint16_t *spawn_y)
{
    VulkanQuad *wall_quads = NULL;
    uint32_t wall_quad_count = 0u;
    VulkanQuad *combined_quads;
    uint32_t combined_quad_count;

    if (scene_quads == NULL || scene_quad_count == NULL ||
        spawn_x == NULL || spawn_y == NULL ||
        !wolf_level_build_wall_quads(level, &wall_quads, &wall_quad_count)) {
        return false;
    }

    if (!wolf_level_find_open_tile(level, spawn_x, spawn_y) ||
        wall_quad_count > UINT32_MAX - 2u) {
        free(wall_quads);
        return false;
    }

    combined_quad_count = wall_quad_count + 2u;
    combined_quads = malloc((size_t)combined_quad_count * sizeof(VulkanQuad));
    if (combined_quads == NULL) {
        free(wall_quads);
        return false;
    }

    combined_quads[0] = environment_quads[0];
    combined_quads[1] = environment_quads[1];
    if (wall_quad_count > 0u) {
        memcpy(
            combined_quads + 2u, wall_quads,
            (size_t)wall_quad_count * sizeof(VulkanQuad));
    }

    free(wall_quads);
    *scene_quads = combined_quads;
    *scene_quad_count = combined_quad_count;
    return true;
}

int main(int argc, char *argv[])
{
    SDL_Window *window;
    SDL_Event event;
    /* The planes extend beyond the camera far clip so their edges remain unseen. */
    const VulkanQuad environment_quads[] = {
        {{
            {{-10000.0f, 0.0f, -10000.0f}, {0.16f, 0.20f, 0.24f}},
            {{10000.0f, 0.0f, -10000.0f}, {0.16f, 0.20f, 0.24f}},
            {{10000.0f, 0.0f, 10000.0f}, {0.16f, 0.20f, 0.24f}},
            {{-10000.0f, 0.0f, 10000.0f}, {0.16f, 0.20f, 0.24f}}
        }},
        {{
            {{-10000.0f, 3.2f, -10000.0f}, {0.11f, 0.14f, 0.20f}},
            {{10000.0f, 3.2f, -10000.0f}, {0.11f, 0.14f, 0.20f}},
            {{10000.0f, 3.2f, 10000.0f}, {0.11f, 0.14f, 0.20f}},
            {{-10000.0f, 3.2f, 10000.0f}, {0.11f, 0.14f, 0.20f}}
        }}
    };
    VulkanRenderer *renderer = NULL;
    WolfInput *input = NULL;
    WolfCamera camera;
    WolfOptions options;
    WolfLevel level;
    VulkanQuad *scene_quads = NULL;
    uint32_t scene_quad_count = 0u;
    uint16_t spawn_x = 0u;
    uint16_t spawn_y = 0u;
    char error_message[256];
    char window_title[128];
    Uint64 performance_frequency;
    Uint64 previous_counter;
    int running = 1;

    if (!wolf_parse_command_line(
            argc, argv, &options, error_message, sizeof(error_message))) {
        fprintf(stderr, "Command line error: %s\n", error_message);
        print_usage(argv != NULL && argv[0] != NULL ? argv[0] : "wolfvk");
        return 1;
    }

    if (options.show_help) {
        print_usage(argv != NULL && argv[0] != NULL ? argv[0] : "wolfvk");
        return 0;
    }

    if (!wolf_level_load(
            "MAPHEAD.WL6", "GAMEMAPS.WL6", options.level_index,
            &level, error_message, sizeof(error_message))) {
        fprintf(stderr, "Could not load level %u: %s\n",
            options.level_index + 1u, error_message);
        return 1;
    }

    if (!build_scene_geometry(
            &level, environment_quads, &scene_quads, &scene_quad_count,
            &spawn_x, &spawn_y)) {
        fprintf(stderr, "Could not build level geometry or find an empty spawn tile.\n");
        return 1;
    }

    (void)snprintf(
        window_title, sizeof(window_title), "WolfVK - Level %u: %.16s",
        options.level_index + 1u, level.name);
    printf("Loaded level %u: %s (%u x %u).\n",
        (unsigned int)(options.level_index + 1u), level.name,
        (unsigned int)level.width, (unsigned int)level.height);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL video initialisation failed: %s\n", SDL_GetError());
        free(scene_quads);
        return 1;
    }

    window = SDL_CreateWindow(
        window_title, 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    if (window == NULL) {
        fprintf(stderr, "SDL Vulkan window creation failed: %s\n", SDL_GetError());
        free(scene_quads);
        SDL_Quit();
        return 1;
    }

    if (!vulkan_renderer_create(window, &renderer)) {
        fprintf(stderr, "Vulkan renderer initialisation failed.\n");
        free(scene_quads);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    wolf_camera_initialize(&camera);
    camera.position[0] =
        -(float)level.width * 0.5f + (float)spawn_x + 0.5f;
    camera.position[2] =
        -(float)level.height * 0.5f + (float)spawn_y + 0.5f;
    if (!update_camera_projection(window, renderer, &camera)) {
        fprintf(stderr, "Could not initialise the camera projection.\n");
        free(scene_quads);
        vulkan_renderer_destroy(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!vulkan_renderer_set_quads(renderer, scene_quads, scene_quad_count)) {
        fprintf(stderr, "Could not upload level geometry.\n");
        free(scene_quads);
        vulkan_renderer_destroy(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    free(scene_quads);
    scene_quads = NULL;

    input = wolf_input_create();
    if (input == NULL) {
        fprintf(stderr, "Input initialisation failed: %s\n", SDL_GetError());
        vulkan_renderer_destroy(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        fprintf(stderr, "Could not capture the mouse for relative mouse-look: %s\\n",
            SDL_GetError());
    }

    performance_frequency = SDL_GetPerformanceFrequency();
    previous_counter = SDL_GetPerformanceCounter();

    while (running) {
        Uint64 current_counter;
        float delta_seconds;

        wolf_input_begin_frame(input);
        while (SDL_PollEvent(&event)) {
            wolf_input_handle_event(input, &event);
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN &&
                 event.key.scancode == SDL_SCANCODE_ESCAPE)) {
                running = 0;
            } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                (void)SDL_SetWindowRelativeMouseMode(window, false);
            } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                (void)SDL_SetWindowRelativeMouseMode(window, true);
            } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                vulkan_renderer_notify_resize(renderer);
                if (!update_camera_projection(window, renderer, &camera)) {
                    fprintf(stderr, "Could not update the camera projection after resize.\n");
                    running = 0;
                }
            }
        }
        current_counter = SDL_GetPerformanceCounter();
        delta_seconds = performance_frequency > 0u
            ? (float)((double)(current_counter - previous_counter) /
                (double)performance_frequency)
            : 0.0f;
        previous_counter = current_counter;

        if (!wolf_player_update(&camera, input, &level, delta_seconds) ||
            !update_camera_projection(window, renderer, &camera)) {
            fprintf(stderr, "Player update or camera projection failed.\\n");
            running = 0;
        }

        if (running && !vulkan_renderer_draw_frame(renderer)) {
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
