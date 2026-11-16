cmake_minimum_required(VERSION 3.24)

# A standalone offline tool: it is built separately from the game so the
# engine never depends on asset-conversion code.
project(WolfSpriteVoxelizer VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(CTest)

# The original VGA palette is shared with the game rather than duplicated.
set(WOLFVK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

add_library(sprite_voxelizer_core STATIC
    src/asset_catalog.c
    src/file_io.c
    src/hand_built_models.c
    src/pipeline.c
    src/png_writer.c
    src/reconstruction.c
    src/sprite_image.c
    src/sprite_shadow.c
    src/validation.c
    src/vox_file.c
    src/voxel_model.c
    src/vswap_sprites.c
    src/wl6_tables.c
    "${WOLFVK_SOURCE_DIR}/game_palette.c"
)
target_include_directories(sprite_voxelizer_core PUBLIC src "${WOLFVK_SOURCE_DIR}")
if(NOT MSVC)
    target_link_libraries(sprite_voxelizer_core PUBLIC m)
endif()

add_executable(wolf_sprite_voxelizer src/main.c)
target_link_libraries(wolf_sprite_voxelizer PRIVATE sprite_voxelizer_core)

set(SPRITE_VOXELIZER_TARGETS sprite_voxelizer_core wolf_sprite_voxelizer)

if(BUILD_TESTING)
    include(FetchContent)
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
    # Geometry and metric checks compare doubles, which Unity omits by default.
    target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)

    add_executable(sprite_voxelizer_tests
        tests/test_main.c
        tests/test_support.c
        tests/test_vswap_sprites.c
        tests/test_voxel_model.c
        tests/test_reconstruction.c
        tests/test_vox_file.c
        tests/test_png_writer.c
        tests/test_asset_catalog.c
        tests/test_validation.c
        tests/test_pipeline.c
    )
    target_include_directories(sprite_voxelizer_tests PRIVATE tests)
    target_link_libraries(sprite_voxelizer_tests PRIVATE sprite_voxelizer_core unity)
    list(APPEND SPRITE_VOXELIZER_TARGETS sprite_voxelizer_tests)

    add_test(NAME sprite_voxelizer_unit_tests COMMAND sprite_voxelizer_tests)
    set_tests_properties(sprite_voxelizer_unit_tests PROPERTIES TIMEOUT 120)
endif()

foreach(target IN LISTS SPRITE_VOXELIZER_TARGETS)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()
