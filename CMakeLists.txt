cmake_minimum_required(VERSION 3.24)

project(WolfVK VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    # Keep the Windows demo independent of a separately installed MSVC runtime.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
include(CTest)

find_package(SDL3 3.4.0 CONFIG QUIET)
find_package(Vulkan REQUIRED)
find_program(GLSLC_EXECUTABLE NAMES glslc
    HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
    REQUIRED)
if(NOT SDL3_FOUND)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.20
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

# SDL3_mixer provides the engine's positional effect playback. Build it against
# the same SDL3 source dependency when a system package is not available.
find_package(SDL3_mixer CONFIG QUIET)
if(NOT SDL3_mixer_FOUND)
    set(SDLMIXER_VENDORED ON CACHE BOOL "" FORCE)
    set(SDLMIXER_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL3_mixer
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
        GIT_TAG release-3.2.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3_mixer)
endif()

add_executable(wolfvk
    src/main.c
    src/camera.c
    src/player_controller.c
    src/game_world.c
    src/command_line.c
    src/input.c
    src/level.c
    src/level_geometry.c
    src/wall_textures.c
    src/game_palette.c
    src/vulkan_renderer_texture.c
    src/vulkan_renderer_geometry.c
    src/vulkan_renderer.c
    src/vulkan_renderer_frame.c
    src/vulkan_renderer_models.c
    src/vulkan_renderer_status_bar.c
    src/vox_file.c
    src/voxel_mesh.c
    src/level_objects.c
    src/voxel_scene.c
    src/weapon.c
    src/vulkan_renderer_weapon.c
    src/player_state.c
    src/screen_layout.c
    src/vga_graphics.c
    src/status_bar.c
    src/bonus_items.c
    src/audio.c
    src/audio_spatial.c
)
target_include_directories(wolfvk PRIVATE include)
target_link_libraries(wolfvk PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer Vulkan::Vulkan)
if(NOT MSVC)
    target_link_libraries(wolfvk PRIVATE m)
endif()

set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(VERTEX_SHADER_SPIRV "${SHADER_OUTPUT_DIR}/triangle.vert.spv")
set(FRAGMENT_SHADER_SPIRV "${SHADER_OUTPUT_DIR}/triangle.frag.spv")

add_custom_command(
    OUTPUT "${VERTEX_SHADER_SPIRV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND ${GLSLC_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert"
        -o "${VERTEX_SHADER_SPIRV}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FRAGMENT_SHADER_SPIRV}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
    COMMAND ${GLSLC_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag"
        -o "${FRAGMENT_SHADER_SPIRV}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag"
    VERBATIM
)

add_custom_target(wolfvk_shaders
    DEPENDS "${VERTEX_SHADER_SPIRV}" "${FRAGMENT_SHADER_SPIRV}"
)
add_dependencies(wolfvk wolfvk_shaders)
add_custom_command(TARGET wolfvk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:wolfvk>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VERTEX_SHADER_SPIRV}" "$<TARGET_FILE_DIR:wolfvk>/shaders/triangle.vert.spv"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FRAGMENT_SHADER_SPIRV}" "$<TARGET_FILE_DIR:wolfvk>/shaders/triangle.frag.spv"
    VERBATIM
)
if(MSVC)
    target_compile_options(wolfvk PRIVATE /W4 /WX)
else()
    target_compile_options(wolfvk PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING)
    # Unity is a small, established C unit-testing framework. Keeping it as a
    # pinned source dependency makes the test runner reproducible in CI.
    FetchContent_Declare(
        Unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG v2.6.1
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(Unity)
    if(NOT unity_POPULATED)
        FetchContent_Populate(Unity)
    endif()

    add_library(unity STATIC ${unity_SOURCE_DIR}/src/unity.c)
    target_include_directories(unity PUBLIC ${unity_SOURCE_DIR}/src)

    add_executable(wolfvk_tests
        tests/test_main.c
        tests/test_window.c
        tests/test_camera.c
        tests/test_player_controller.c
        tests/test_game_world.c
        tests/test_command_line.c
        tests/test_level.c
        tests/test_level_geometry.c
        tests/test_wall_textures.c
        tests/test_input.c
        tests/test_vulkan_renderer.c
        tests/test_vox_builder.c
        tests/test_vox_file.c
        tests/test_voxel_mesh.c
        tests/test_level_objects.c
        tests/test_voxel_scene.c
        tests/test_weapon.c
        tests/test_vga_graphics.c
        tests/test_player_state.c
        tests/test_status_bar.c
        tests/test_bonus_items.c
        tests/test_audio_spatial.c
        src/camera.c
        src/player_controller.c
        src/game_world.c
        src/command_line.c
        src/input.c
        src/level.c
        src/level_geometry.c
        src/wall_textures.c
        src/game_palette.c
        src/vulkan_renderer_texture.c
        src/vulkan_renderer_geometry.c
        src/vulkan_renderer.c
        src/vulkan_renderer_frame.c
        src/vulkan_renderer_models.c
        src/vulkan_renderer_status_bar.c
        src/vox_file.c
        src/voxel_mesh.c
        src/level_objects.c
        src/voxel_scene.c
        src/weapon.c
        src/vulkan_renderer_weapon.c
        src/player_state.c
        src/screen_layout.c
        src/vga_graphics.c
        src/status_bar.c
        src/bonus_items.c
        src/audio_spatial.c
    )
    add_dependencies(wolfvk_tests wolfvk_shaders)
    target_include_directories(wolfvk_tests PRIVATE include src)
    target_link_libraries(wolfvk_tests PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer Vulkan::Vulkan unity)
    if(NOT MSVC)
        target_link_libraries(wolfvk_tests PRIVATE m)
    endif()
    add_custom_command(TARGET wolfvk_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:wolfvk_tests>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VERTEX_SHADER_SPIRV}" "$<TARGET_FILE_DIR:wolfvk_tests>/shaders/triangle.vert.spv"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FRAGMENT_SHADER_SPIRV}" "$<TARGET_FILE_DIR:wolfvk_tests>/shaders/triangle.frag.spv"
        VERBATIM
    )
    if(MSVC)
        target_compile_options(wolfvk_tests PRIVATE /W4 /WX)
    else()
        target_compile_options(wolfvk_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME wolfvk_unit_tests COMMAND wolfvk_tests)
    set_tests_properties(wolfvk_unit_tests PROPERTIES TIMEOUT 30)
endif()
