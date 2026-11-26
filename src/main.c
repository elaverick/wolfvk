#include "wolfvk/camera.h"
#include "wolfvk/command_line.h"
#include "wolfvk/input.h"
#include "wolfvk/level.h"
#include "wolfvk/game_world.h"
#include "wolfvk/level_geometry.h"
#include "wolfvk/level_objects.h"
#include "wolfvk/player_controller.h"
#include "wolfvk/screen_layout.h"
#include "wolfvk/status_bar.h"
#include "wolfvk/vga_graphics.h"
#include "wolfvk/voxel_scene.h"
#include "wolfvk/vulkan_renderer.h"
#include "wolfvk/world.h"
#include "wolfvk/wall_textures.h"
#include "wolfvk/weapon.h"
#include "wolfvk/audio.h"
#include "wolfvk/audio_events.h"
#include "wolfvk/audio_sound_names.h"

#include "game_palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The status bar as last sent to the renderer, so it is only re-sent when
 * something it shows has changed.
 */
typedef struct StatusBarDisplay {
    bool available;
    WolfStatusBarGraphics graphics;
    uint8_t shown_pixels[WOLF_STATUS_BAR_PIXEL_COUNT];
    bool has_shown_pixels;
} StatusBarDisplay;

/*
 * Load an optional extracted sound from the runtime assets directory. The
 * converter writes these named OGG files; missing assets are non-fatal.
 */
static void load_optional_sound(WolfAudio *audio, const char *sound_name)
{
    char sound_path[512];
    SDL_PathInfo path_info;

    (void)snprintf(sound_path, sizeof(sound_path), "assets/sounds/%s.ogg", sound_name);
    if (SDL_GetPathInfo(sound_path, &path_info) && path_info.type == SDL_PATHTYPE_FILE) {
        (void)wolf_audio_load(audio, sound_name, sound_path);
    }
}

/* Load all 87 original effect IDs; missing assets remain optional for partial installs. */
static void load_audio_assets(WolfAudio *audio)
{
    for (size_t index = 0u; index < WOLF_AUDIO_SOUND_COUNT; ++index) {
        load_optional_sound(audio, wolf_audio_sound_names[index]);
    }
}

/* Play sounds for doors and secret pushwalls when their world states transition. */
static void play_world_audio_events(
    WolfAudio *audio,
    const WolfAudioWorldSnapshot *previous,
    const WolfWorldState *world)
{
    WolfAudioWorldSnapshot current;
    WolfAudioWorldEventInfo events[WOLF_WORLD_MAX_DOORS + 1u];
    size_t event_count;

    if (audio == NULL || previous == NULL || world == NULL) {
        return;
    }

    wolf_audio_world_snapshot_capture(world, &current);
    event_count = wolf_audio_world_snapshot_diff(
        previous, &current, events, sizeof(events) / sizeof(events[0]));

    for (size_t index = 0u; index < event_count; ++index) {
        const char *sound_name = wolf_audio_world_event_sound(events[index].type);

        if (sound_name != NULL) {
            (void)wolf_audio_play(audio, sound_name, events[index].source_position);
        }
    }
}

/*
 * Use the framebuffer pixels of the 3D view for the aspect ratio, so the
 * scene above the status bar is not stretched and high-DPI windows keep a
 * correct projection.
 */
static bool update_camera_projection(
    SDL_Window *window,
    VulkanRenderer *renderer,
    const WolfCamera *camera,
    bool show_status_bar)
{
    int drawable_width = 0;
    int drawable_height = 0;
    WolfScreenLayout layout;
    float view_projection[16];

    SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
    if (drawable_width <= 0 || drawable_height <= 0 ||
        !wolf_screen_layout_compute(
            (uint32_t)drawable_width, (uint32_t)drawable_height, show_status_bar, &layout)) {
        return true;
    }

    if (!wolf_camera_build_view_projection(
            camera, (float)layout.view.width / (float)layout.view.height, view_projection)) {
        return false;
    }

    return vulkan_renderer_set_view_projection(renderer, view_projection);
}

/* Tell users how level selection works and where the original retail assets must be placed. */
static void print_usage(const char *program_name)
{
    printf("Usage: %s [--level NUMBER] [--difficulty 1-4] [--help]\n", program_name);
    printf("Level numbers are one-based; if omitted, the first level is loaded.\n");
    printf("Difficulty follows the original menu: 1 Can I play, Daddy?, 2 Don't hurt me.,\n");
    printf("3 Bring 'em on! (the default), 4 I am Death incarnate!\n");
    printf("Place MAPHEAD.WL6, GAMEMAPS.WL6 and VSWAP.WL6 in the working directory.\n");
    printf("VGAHEAD.WL6, VGADICT.WL6 and VGAGRAPH.WL6 there provide the status bar.\n");
    printf("Voxel sprites are read from %s beside them, or beside the executable.\n",
        WOLF_VOXEL_ASSET_DIRECTORY);
}

static bool is_directory(const char *path)
{
    SDL_PathInfo path_info;

    return SDL_GetPathInfo(path, &path_info) && path_info.type == SDL_PATHTYPE_DIRECTORY;
}

/*
 * Voxel assets are derived from the user's own game data, so they belong
 * with the WL6 files in the working directory. A copy beside the executable
 * is also accepted, which is where the shaders are found.
 */
static bool find_voxel_asset_directory(char *directory, size_t directory_size)
{
    const char *base_path;

    if (is_directory(WOLF_VOXEL_ASSET_DIRECTORY)) {
        (void)snprintf(directory, directory_size, "%s", WOLF_VOXEL_ASSET_DIRECTORY);
        return true;
    }

    base_path = SDL_GetBasePath();
    if (base_path != NULL) {
        (void)snprintf(directory, directory_size, "%s%s", base_path, WOLF_VOXEL_ASSET_DIRECTORY);
        if (is_directory(directory)) {
            return true;
        }
    }
    return false;
}

/*
 * Mesh every actor and object pose the level places. Missing assets are
 * reported but not fatal: the level is still playable without its sprites.
 */
static bool load_voxel_scene(
    const WolfWorldState *world,
    WolfDifficulty difficulty,
    WolfVoxelScene *scene)
{
    char asset_directory[512];
    WolfLevelObject *objects = NULL;
    uint32_t object_count = 0u;
    bool loaded;

    memset(scene, 0, sizeof(*scene));
    if (!find_voxel_asset_directory(asset_directory, sizeof(asset_directory))) {
        fprintf(stderr,
            "No %s directory was found, so actors and objects will not be drawn.\n"
            "Create it with: wolf_sprite_voxelizer --vswap VSWAP.WL6 --assets %s\n",
            WOLF_VOXEL_ASSET_DIRECTORY, WOLF_VOXEL_ASSET_DIRECTORY);
        return true;
    }

    if (!wolf_level_find_objects(&world->level, difficulty, &objects, &object_count)) {
        fprintf(stderr, "Could not read the level's actors and objects.\n");
        return false;
    }

    loaded = wolf_voxel_scene_load(scene, objects, object_count, asset_directory);
    free(objects);
    if (!loaded) {
        fprintf(stderr, "Could not load the voxel sprites from %s.\n", asset_directory);
        return false;
    }

    printf("Loaded %u voxel poses (%u quads) for %u actors and objects from %s.\n",
        (unsigned int)(scene->pose_count - scene->unavailable_pose_count),
        (unsigned int)scene->quad_count, (unsigned int)object_count, asset_directory);
    if (scene->unavailable_pose_count > 0u) {
        fprintf(stderr, "%u voxel poses are unavailable and will not be drawn (first: %s).\n",
            (unsigned int)scene->unavailable_pose_count, scene->first_problem);
    }
    return true;
}

/*
 * Decode the status bar pictures from the original VGAGRAPH files. They are
 * optional: without them the game runs with the 3D view filling the window.
 */
static void load_status_bar_graphics(StatusBarDisplay *display)
{
    WolfVgaGraphics graphics = {0};
    char error_message[256];

    memset(display, 0, sizeof(*display));
    if (!wolf_vga_graphics_load(
            "VGAHEAD.WL6", "VGADICT.WL6", "VGAGRAPH.WL6",
            &graphics, error_message, sizeof(error_message))) {
        fprintf(stderr, "No status bar will be shown: %s\n"
            "Copy VGAHEAD.WL6, VGADICT.WL6 and VGAGRAPH.WL6 beside the map files to show it.\n",
            error_message);
        return;
    }
    display->available = wolf_status_bar_graphics_load(&graphics, &display->graphics);
    if (!display->available) {
        fprintf(stderr, "The status bar pictures in VGAGRAPH.WL6 could not be decoded.\n");
    }
    wolf_vga_graphics_destroy(&graphics);
}

/* Redraw the bar from the player's current values and send it to the renderer if it changed. */
static bool refresh_status_bar(
    VulkanRenderer *renderer,
    StatusBarDisplay *display,
    const WolfPlayerState *player)
{
    uint8_t indexed_pixels[WOLF_STATUS_BAR_PIXEL_COUNT];
    static uint8_t rgba_pixels[WOLF_STATUS_BAR_PIXEL_COUNT * 4u];
    const uint8_t *border_rgba =
        wolf_game_palette_rgba + (size_t)WOLF_STATUS_BAR_BORDER_COLOUR * 4u;

    if (!display->available) {
        return true;
    }
    if (!wolf_status_bar_compose(&display->graphics, player, indexed_pixels)) {
        return false;
    }
    if (display->has_shown_pixels &&
        memcmp(indexed_pixels, display->shown_pixels, sizeof(indexed_pixels)) == 0) {
        return true;
    }

    wolf_status_bar_expand_rgba(indexed_pixels, WOLF_STATUS_BAR_PIXEL_COUNT, rgba_pixels);
    if (!vulkan_renderer_set_status_bar(renderer, rgba_pixels, border_rgba)) {
        return false;
    }
    memcpy(display->shown_pixels, indexed_pixels, sizeof(indexed_pixels));
    display->has_shown_pixels = true;
    return true;
}

/* Place the models from the current info plane, so items the player has collected disappear. */
static bool update_model_instances(
    VulkanRenderer *renderer,
    const WolfWorldState *world,
    WolfDifficulty difficulty,
    const WolfVoxelScene *scene)
{
    WolfLevelObject *objects = NULL;
    uint32_t object_count = 0u;
    VulkanModelInstance *instances = NULL;
    uint32_t instance_count = 0u;
    bool updated;

    if (!wolf_level_find_objects(&world->level, difficulty, &objects, &object_count)) {
        return false;
    }
    updated = wolf_voxel_scene_build_instances(
            scene, &world->level, objects, object_count, &instances, &instance_count) &&
        vulkan_renderer_set_model_instances(renderer, instances, instance_count);
    free(objects);
    free(instances);
    return updated;
}

/* Combine the map's exposed wall faces with the large floor and roof, and resolve its spawn. */
static bool build_scene_geometry(
    const WolfWorldState *world,
    const VulkanQuad environment_quads[2],
    VulkanQuad **scene_quads,
    uint32_t *scene_quad_count,
    uint32_t *base_quad_count,
    uint32_t *dynamic_quad_count,
    WolfPlayerStart *player_start)
{
    VulkanQuad *wall_quads = NULL;
    uint32_t wall_quad_count = 0u;
    VulkanQuad *combined_quads;
    uint32_t combined_quad_count;

    if (world == NULL || scene_quads == NULL || scene_quad_count == NULL ||
        base_quad_count == NULL || dynamic_quad_count == NULL ||
        player_start == NULL) {
        fprintf(stderr, "Invalid level geometry output arguments.\n");
        return false;
    }

    if (!wolf_level_build_wall_quads(
            &world->level, world->texture_page_count, &wall_quads, &wall_quad_count)) {
        fprintf(
            stderr, "Could not generate wall geometry for level %s (%u x %u).\n",
            world->level.name,
            (unsigned int)world->level.width,
            (unsigned int)world->level.height);
        return false;
    }

    /* Custom maps may omit the original marker; a deterministic floor fallback still permits inspection. */
    if (!wolf_level_find_player_start(&world->level, player_start)) {
        if (!wolf_level_find_open_tile(
                &world->level, &player_start->tile_x, &player_start->tile_y)) {
            size_t open_tile_count = 0u;
            size_t solid_tile_count = 0u;

            for (uint16_t tile_y = 0u; tile_y < world->level.height; ++tile_y) {
                for (uint16_t tile_x = 0u; tile_x < world->level.width; ++tile_x) {
                    size_t tile_index =
                        (size_t)tile_y * WOLF_LEVEL_PLANE_WIDTH + tile_x;

                    if (world->level.wall_tiles[tile_index] == 0u ||
                        wolf_level_is_omitted_door_cell(&world->level, tile_x, tile_y)) {
                        ++open_tile_count;
                    } else {
                        ++solid_tile_count;
                    }
                }
            }

            fprintf(
                stderr,
                "No player-start marker or open floor cell exists in level %s (%u x %u). "
                "The decoded wall plane contains %zu solid cells and %zu open cells.\n",
                world->level.name, (unsigned int)world->level.width,
                (unsigned int)world->level.height,
                solid_tile_count, open_tile_count);
            fprintf(stderr, "First wall-plane row:");
            for (uint16_t tile_x = 0u; tile_x < world->level.width && tile_x < 16u; ++tile_x) {
                fprintf(stderr, " %04X", world->level.wall_tiles[tile_x]);
            }
            fputc('\n', stderr);
            free(wall_quads);
            return false;
        }
        player_start->yaw_radians = 0.0f;
    }

    *base_quad_count = wall_quad_count + 2u;
    *dynamic_quad_count = wolf_world_state_dynamic_quad_count(world);
    if (wall_quad_count > UINT32_MAX - 2u - *dynamic_quad_count) {
        fprintf(stderr, "Level generated too many wall and door quads.\n");
        free(wall_quads);
        return false;
    }

    combined_quad_count = *base_quad_count + *dynamic_quad_count;
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
    if (*dynamic_quad_count > 0u &&
        !wolf_world_state_build_dynamic_quads(
            world, combined_quads + *base_quad_count, *dynamic_quad_count)) {
        free(combined_quads);
        return false;
    }

    *scene_quads = combined_quads;
    *scene_quad_count = combined_quad_count;
    return true;
}

/* Own the application lifetime: load WL6 data, establish SDL/Vulkan, run input and frame updates, then unwind. */
int main(int argc, char *argv[])
{
    SDL_Window *window;
    SDL_Event event;
    /*
     * Match the supplied original-game reference capture. The swapchain is sRGB,
     * so these linear values produce display greys of approximately RGB 113 and 56.
     * The planes extend beyond the camera far clip and share the original wall height.
     */
    const VulkanQuad environment_quads[] = {
        {{
            {{-10000.0f, 0.0f, -10000.0f}, {0.165132f, 0.165132f, 0.165132f}},
            {{10000.0f, 0.0f, -10000.0f}, {0.165132f, 0.165132f, 0.165132f}},
            {{10000.0f, 0.0f, 10000.0f}, {0.165132f, 0.165132f, 0.165132f}},
            {{-10000.0f, 0.0f, 10000.0f}, {0.165132f, 0.165132f, 0.165132f}}
        }},
        {{
            {{-10000.0f, WOLF_WORLD_CEILING_HEIGHT, -10000.0f}, {0.039546f, 0.039546f, 0.039546f}},
            {{10000.0f, WOLF_WORLD_CEILING_HEIGHT, -10000.0f}, {0.039546f, 0.039546f, 0.039546f}},
            {{10000.0f, WOLF_WORLD_CEILING_HEIGHT, 10000.0f}, {0.039546f, 0.039546f, 0.039546f}},
            {{-10000.0f, WOLF_WORLD_CEILING_HEIGHT, 10000.0f}, {0.039546f, 0.039546f, 0.039546f}}
        }}
    };
    VulkanRenderer *renderer = NULL;
    WolfInput *input = NULL;
    WolfCamera camera;
    WolfOptions options;
    WolfLevel level;
    WolfWorldState world = {0};
    WolfWallTextures wall_textures = {0};
    VulkanQuad *scene_quads = NULL;
    uint32_t scene_quad_count = 0u;
    uint32_t base_quad_count = 0u;
    uint32_t dynamic_quad_count = 0u;
    uint32_t geometry_revision = 0u;
    WolfPlayerStart player_start = {0};
    WolfVoxelScene voxel_scene = {0};
    WolfWeaponScene weapon_scene = {0};
    WolfWeaponState weapon_state;
    WolfAudio audio = {0};
    bool audio_available = false;
    StatusBarDisplay status_bar = {0};
    char error_message[256];
    char window_title[128];
    Uint64 performance_frequency;
    Uint64 previous_counter;
    int running = 1;
    bool use_was_down = false;
    bool weapon_assets_uploaded = false;
    float drawable_aspect_ratio = 16.0f / 9.0f;

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

    if (!wolf_wall_textures_load_vswap(
            "VSWAP.WL6", &wall_textures, error_message, sizeof(error_message))) {
        fprintf(stderr, "Could not load original wall textures: %s\n", error_message);
        return 1;
    }

    if (!wolf_world_state_init(&world, &level, wall_textures.page_count)) {
        fprintf(stderr, "Could not initialise the original door and secret-wall data.\n");
        wolf_wall_textures_destroy(&wall_textures);
        return 1;
    }
    /* A new game on the chosen level: full health, a pistol with 8 bullets, 3 lives, no keys. */
    wolf_player_state_new_game(&world.player, options.level_index);

    if (!build_scene_geometry(
            &world, environment_quads, &scene_quads, &scene_quad_count,
            &base_quad_count, &dynamic_quad_count, &player_start)) {
        fprintf(stderr, "Level geometry initialisation failed; check the VSWAP wall page range.\n");
        wolf_wall_textures_destroy(&wall_textures);
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
        wolf_wall_textures_destroy(&wall_textures);
        return 1;
    }

    window = SDL_CreateWindow(
        window_title, 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    if (window == NULL) {
        fprintf(stderr, "SDL Vulkan window creation failed: %s\n", SDL_GetError());
        free(scene_quads);
        wolf_wall_textures_destroy(&wall_textures);
        SDL_Quit();
        return 1;
    }

    if (!vulkan_renderer_create(window, &renderer)) {
        fprintf(stderr, "Vulkan renderer initialisation failed.\n");
        free(scene_quads);
        wolf_wall_textures_destroy(&wall_textures);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!vulkan_renderer_set_wall_textures(
            renderer, wall_textures.rgba_pages, wall_textures.page_count)) {
        fprintf(stderr, "Could not upload the original VSWAP wall textures.\n");
        free(scene_quads);
        wolf_wall_textures_destroy(&wall_textures);
        vulkan_renderer_destroy(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    wolf_wall_textures_destroy(&wall_textures);

    load_status_bar_graphics(&status_bar);
    if (!refresh_status_bar(renderer, &status_bar, &world.player)) {
        fprintf(stderr, "Could not show the status bar.\n");
        wolf_status_bar_graphics_destroy(&status_bar.graphics);
        free(scene_quads);
        vulkan_renderer_destroy(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    wolf_camera_initialize(&camera);
    camera.position[0] =
        -((float)level.width * WOLF_WORLD_TILE_SIZE) * 0.5f +
        ((float)player_start.tile_x + 0.5f) * WOLF_WORLD_TILE_SIZE;
    camera.position[2] =
        -((float)level.height * WOLF_WORLD_TILE_SIZE) * 0.5f +
        ((float)player_start.tile_y + 0.5f) * WOLF_WORLD_TILE_SIZE;
    camera.yaw_radians = player_start.yaw_radians;
    if (!update_camera_projection(window, renderer, &camera, status_bar.available)) {
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
    geometry_revision = wolf_world_state_geometry_revision(&world);

    if (!load_voxel_scene(&world, options.difficulty, &voxel_scene) ||
        !vulkan_renderer_set_model_meshes(renderer, voxel_scene.quads, voxel_scene.quad_count) ||
        !update_model_instances(renderer, &world, options.difficulty, &voxel_scene)) {
        fprintf(stderr, "Could not place the level's voxel actors and objects.\n");
        wolf_voxel_scene_destroy(&voxel_scene);
        free(scene_quads);
        vulkan_renderer_destroy(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (find_voxel_asset_directory(error_message, sizeof(error_message))) {
        if (!wolf_weapon_scene_load(&weapon_scene, error_message)) {
            fprintf(stderr, "Could not load first-person weapon meshes from %s.\n", error_message);
        } else if (weapon_scene.quad_count > 0u) {
            if (!vulkan_renderer_set_weapon_meshes(renderer, weapon_scene.quads, weapon_scene.quad_count)) {
                fprintf(stderr, "Could not upload the first-person weapon meshes.\n");
                wolf_weapon_scene_destroy(&weapon_scene);
                wolf_voxel_scene_destroy(&voxel_scene);
                free(scene_quads);
                vulkan_renderer_destroy(renderer);
                SDL_DestroyWindow(window);
                SDL_Quit();
                return 1;
            }
            weapon_assets_uploaded = true;
            printf("Loaded all four voxel weapons (%u quads).\n", (unsigned int)weapon_scene.quad_count);
        } else {
            fprintf(stderr, "First-person weapons are incomplete and will be hidden (%s).\n",
                weapon_scene.first_problem);
        }
    } else {
        fprintf(stderr, "No voxel asset directory was found; first-person weapons will not be drawn.\n");
    }
    wolf_weapon_state_initialize(&weapon_state);

    input = wolf_input_create();
    if (input == NULL) {
        fprintf(stderr, "Input initialisation failed: %s\n", SDL_GetError());
        wolf_weapon_scene_destroy(&weapon_scene);
        wolf_voxel_scene_destroy(&voxel_scene);
        vulkan_renderer_destroy(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        fprintf(stderr, "Could not capture the mouse for relative mouse-look: %s\n",
            SDL_GetError());
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        audio_available = wolf_audio_initialize(&audio);
        if (audio_available) {
            load_audio_assets(&audio);
            printf("Spatial audio initialised; %zu sounds loaded.\n", audio.sound_count);
        } else {
            fprintf(stderr, "Spatial audio is unavailable: %s\n", SDL_GetError());
        }
    } else {
        fprintf(stderr, "SDL audio subsystem is unavailable: %s\n", SDL_GetError());
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
                if (!update_camera_projection(window, renderer, &camera, status_bar.available)) {
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

        {
            bool dynamic_geometry_changed = false;
            bool use_down;
            bool fire_down;
            bool geometry_changed = false;
            WolfAudioWorldSnapshot audio_world_before;

            wolf_audio_world_snapshot_capture(&world, &audio_world_before);
            if (!wolf_world_state_update(
                    &world, delta_seconds, camera.position[0], camera.position[2],
                    &dynamic_geometry_changed) ||
                !wolf_player_update(&camera, input, &world, delta_seconds)) {
                fprintf(stderr, "Player, door or pushwall update failed.\n");
                running = 0;
            }

            {
                WolfBonusType collected_bonuses[9];
                size_t collected_bonus_count = 0u;
                bool collected = wolf_world_state_collect_bonuses_with_events(
                    &world, camera.position[0], camera.position[2], camera.yaw_radians,
                    collected_bonuses, sizeof(collected_bonuses) / sizeof(collected_bonuses[0]),
                    &collected_bonus_count);

                if (collected &&
                    !update_model_instances(renderer, &world, options.difficulty, &voxel_scene)) {
                    fprintf(stderr, "Could not remove a picked-up item's model.\n");
                    running = 0;
                }
                if (collected && audio_available) {
                    for (size_t index = 0u; index < collected_bonus_count; ++index) {
                        const char *pickup_sound =
                            wolf_audio_bonus_sound_name(collected_bonuses[index]);

                        if (pickup_sound != NULL) {
                            (void)wolf_audio_play(&audio, pickup_sound,
                                (WolfAudioVector3){camera.position[0], camera.position[1], camera.position[2]});
                        }
                    }
                }
            }

            if (audio_available) {
                (void)wolf_audio_set_listener(&audio,
                    (WolfAudioVector3){camera.position[0], camera.position[1], camera.position[2]},
                    camera.yaw_radians);
            }

            fire_down = wolf_input_mouse_button_down(input, SDL_BUTTON_LEFT) ||
                wolf_input_key_down(input, SDL_SCANCODE_LCTRL) ||
                wolf_input_gamepad_button_down(input, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) ||
                wolf_input_gamepad_axis(input, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.35f;
            /*
             * Keys 1-4 choose among the weapons picked up so far, as CheckWeaponChange
             * does. As in the original, the weapon cannot change part-way through an attack.
             */
            for (int weapon = WOLF_WEAPON_KNIFE;
                 !weapon_state.firing && weapon <= WOLF_WEAPON_CHAINGUN;
                 ++weapon) {
                if (wolf_input_key_down(input, (SDL_Scancode)(SDL_SCANCODE_1 + weapon))) {
                    (void)wolf_player_state_select_weapon(&world.player, (WolfWeaponType)weapon);
                    break;
                }
            }
            /*
             * The player state owns the weapon in hand; picking up a better gun
             * also switches to it. The view model follows whichever is held.
             */
            if (weapon_state.selected != world.player.weapon) {
                (void)wolf_weapon_state_select(&weapon_state, world.player.weapon);
            }
            /*
             * Firing uses bullets and swaps to the knife when they run out. The
             * shot and knife events are not used until enemies can take damage.
             */
            {
                WolfWeaponAttackEvents attack_events;
                WolfWeaponType attacking_weapon = weapon_state.selected;

                if (!wolf_weapon_state_update(
                        &weapon_state, &world.player, fire_down, delta_seconds, &attack_events)) {
                    fprintf(stderr, "Weapon attack update failed.\n");
                    running = 0;
                }
                if (audio_available && attack_events.gun_shots > 0u) {
                    const char *shot_sound = attacking_weapon == WOLF_WEAPON_PISTOL
                        ? "ATKPISTOLSND"
                        : attacking_weapon == WOLF_WEAPON_MACHINEGUN
                            ? "ATKMACHINEGUNSND"
                            : attacking_weapon == WOLF_WEAPON_CHAINGUN
                                ? "ATKGATLINGSND"
                                : NULL;

                    for (uint32_t shot = 0u; shot < attack_events.gun_shots && shot_sound != NULL; ++shot) {
                        (void)wolf_audio_play(&audio, shot_sound,
                            (WolfAudioVector3){camera.position[0], camera.position[1], camera.position[2]});
                    }
                }
                if (audio_available && attack_events.knife_attacks > 0u) {
                    for (uint32_t attack = 0u; attack < attack_events.knife_attacks; ++attack) {
                        (void)wolf_audio_play(&audio, "ATKKNIFESND",
                            (WolfAudioVector3){camera.position[0], camera.position[1], camera.position[2]});
                    }
                }
            }
            {
                const WolfWeaponPose *pose = weapon_assets_uploaded
                    ? wolf_weapon_scene_current_pose(&weapon_scene, &weapon_state)
                    : NULL;
                int drawable_width = 0;
                int drawable_height = 0;
                WolfScreenLayout layout;
                float weapon_matrix[16];
                float attack_offset = weapon_state.firing ? 0.12f : 0.0f;
                SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
                /* The weapon is drawn in the 3D view above the status bar, so it uses the view's shape. */
                if (drawable_width > 0 && drawable_height > 0 &&
                    wolf_screen_layout_compute(
                        (uint32_t)drawable_width, (uint32_t)drawable_height,
                        status_bar.available, &layout)) {
                    drawable_aspect_ratio = (float)layout.view.width / (float)layout.view.height;
                }
                if (pose != NULL && wolf_weapon_build_view_projection(
                        &camera, drawable_aspect_ratio, attack_offset, weapon_matrix)) {
                    if (!vulkan_renderer_set_weapon_pose(
                            renderer, pose->first_quad, pose->quad_count, weapon_matrix)) {
                        fprintf(stderr, "Could not select the active first-person weapon pose.\n");
                        running = 0;
                    }
                }
            }

            use_down = wolf_input_key_down(input, SDL_SCANCODE_E) ||
                wolf_input_key_down(input, SDL_SCANCODE_SPACE) ||
                wolf_input_gamepad_button_down(input, SDL_GAMEPAD_BUTTON_NORTH);
            if (use_down && (!use_was_down ||
                wolf_world_state_has_secret_wall_ahead(
                    &world, camera.position[0], camera.position[2],
                    camera.yaw_radians))) {
                bool used = wolf_world_state_use(
                    &world, camera.position[0], camera.position[2],
                    camera.yaw_radians);

                dynamic_geometry_changed = used || dynamic_geometry_changed;
                if (!used && !use_was_down && audio_available) {
                    (void)wolf_audio_play(&audio, "DONOTHINGSND",
                        (WolfAudioVector3){camera.position[0], camera.position[1], camera.position[2]});
                }
            }
            use_was_down = use_down;

            if (audio_available) {
                play_world_audio_events(&audio, &audio_world_before, &world);
            }

            (void)wolf_player_state_update_face(&world.player, delta_seconds);
            if (running && !refresh_status_bar(renderer, &status_bar, &world.player)) {
                fprintf(stderr, "Could not update the status bar.\n");
                running = 0;
            }

            if (!update_camera_projection(window, renderer, &camera, status_bar.available)) {
                fprintf(stderr, "Could not update the camera projection.\n");
                running = 0;
            }

            {
                uint32_t current_revision = wolf_world_state_geometry_revision(&world);
                geometry_changed = current_revision != geometry_revision;
                geometry_revision = current_revision;
            }
            if (running && geometry_changed) {
                free(scene_quads);
                scene_quads = NULL;
                if (!build_scene_geometry(
                        &world, environment_quads, &scene_quads, &scene_quad_count,
                        &base_quad_count, &dynamic_quad_count, &player_start) ||
                    !vulkan_renderer_set_quads(renderer, scene_quads, scene_quad_count)) {
                    fprintf(stderr, "Could not rebuild static geometry after a pushwall move.\n");
                    running = 0;
                }
            } else if (running &&
                (dynamic_geometry_changed || wolf_world_state_has_animated_geometry(&world))) {
                if (!wolf_world_state_build_dynamic_quads(
                        &world, scene_quads + base_quad_count, dynamic_quad_count) ||
                    !vulkan_renderer_update_quads(renderer, scene_quads, scene_quad_count)) {
                    fprintf(stderr, "Could not update animated door or pushwall geometry.\n");
                    running = 0;
                }
            }
        }

        if (running && !vulkan_renderer_draw_frame(renderer)) {
            fprintf(stderr, "Vulkan frame rendering failed.\n");
            running = 0;
        }
        SDL_Delay(1);
    }

    (void)SDL_SetWindowRelativeMouseMode(window, false);
    wolf_input_destroy(input);
    wolf_weapon_scene_destroy(&weapon_scene);
    wolf_voxel_scene_destroy(&voxel_scene);
    wolf_status_bar_graphics_destroy(&status_bar.graphics);
    free(scene_quads);
    vulkan_renderer_destroy(renderer);
    SDL_DestroyWindow(window);
    wolf_audio_shutdown(&audio);
    SDL_Quit();
    return 0;
}
