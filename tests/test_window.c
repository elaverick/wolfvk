#include "unity.h"

#include <SDL3/SDL.h>

void setUp(void)
{
    TEST_ASSERT_TRUE_MESSAGE(
        SDL_Init(SDL_INIT_VIDEO),
        SDL_GetError());
}

void tearDown(void)
{
    SDL_Quit();
}

void test_window_creation(void)
{
    SDL_Window *window = SDL_CreateWindow(
        "WolfVK test window", 640, 480, SDL_WINDOW_HIDDEN);

    TEST_ASSERT_NOT_NULL_MESSAGE(window, SDL_GetError());
    SDL_DestroyWindow(window);
}

void test_window_is_resizable(void)
{
    SDL_Window *window = SDL_CreateWindow(
        "WolfVK resize test", 640, 480,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);

    TEST_ASSERT_NOT_NULL_MESSAGE(window, SDL_GetError());
    TEST_ASSERT_TRUE(
        (SDL_GetWindowFlags(window) & SDL_WINDOW_RESIZABLE) != 0u);
    SDL_DestroyWindow(window);
}

void test_window_close_event_can_be_queued(void)
{
    SDL_Event event = {0};
    SDL_Event received_event = {0};

    event.type = SDL_EVENT_QUIT;
    TEST_ASSERT_EQUAL_INT(1, SDL_PushEvent(&event));
    TEST_ASSERT_TRUE(SDL_PollEvent(&received_event));
    TEST_ASSERT_EQUAL_UINT32(SDL_EVENT_QUIT, received_event.type);
}
