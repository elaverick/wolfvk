cmake_minimum_required(VERSION 3.24)

project(WolfVK VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)
include(CTest)

find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.10
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()

add_executable(wolfvk
    src/main.c
)
target_link_libraries(wolfvk PRIVATE SDL3::SDL3)
if(MSVC)
    target_compile_options(wolfvk PRIVATE /W4 /WX)
else()
    target_compile_options(wolfvk PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING)
    add_executable(wolfvk_window_smoke
        tests/window_smoke.c
    )
    target_link_libraries(wolfvk_window_smoke PRIVATE SDL3::SDL3)
    if(MSVC)
        target_compile_options(wolfvk_window_smoke PRIVATE /W4 /WX)
    else()
        target_compile_options(wolfvk_window_smoke PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME sdl3_window_creation COMMAND wolfvk_window_smoke)
    set_tests_properties(sdl3_window_creation PROPERTIES TIMEOUT 30)
endif()
