cmake_minimum_required(VERSION 3.24)

project(WolfAudioExtractor VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)

add_library(audio_extractor_core STATIC
    src/audio_archive.c
)
target_include_directories(audio_extractor_core PUBLIC src)

add_executable(wolf_audio_extractor
    src/main.c
)
target_link_libraries(wolf_audio_extractor PRIVATE audio_extractor_core)

set(AUDIO_EXTRACTOR_TARGETS audio_extractor_core wolf_audio_extractor)

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

    add_library(unity STATIC \${unity_SOURCE_DIR}/src/unity.c)
    target_include_directories(unity PUBLIC \${unity_SOURCE_DIR}/src)

    add_executable(audio_extractor_tests
        tests/test_main.c
        tests/test_audio_archive.c
    )
    target_link_libraries(audio_extractor_tests PRIVATE audio_extractor_core unity)
    list(APPEND AUDIO_EXTRACTOR_TARGETS audio_extractor_tests)

    add_test(NAME audio_extractor_unit_tests COMMAND audio_extractor_tests)
    set_tests_properties(audio_extractor_unit_tests PROPERTIES TIMEOUT 30)
endif()

foreach(target IN LISTS AUDIO_EXTRACTOR_TARGETS)
    if(MSVC)
        target_compile_options(\${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(\${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()
