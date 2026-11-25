#ifndef WOLFVK_SCREEN_LAYOUT_H
#define WOLFVK_SCREEN_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * The original game drew on a 320x200 screen with a 40-line status bar
 * along the bottom. Here the bar keeps its pixel-art look by being drawn
 * at a whole-number multiple of its size, chosen so that a 320x200 screen
 * at the same scale would still fit the window. The 3D view fills the full
 * window width above the bar, so the bar never covers any of the scene.
 */
#define WOLF_ORIGINAL_SCREEN_WIDTH 320u
#define WOLF_ORIGINAL_SCREEN_HEIGHT 200u
#define WOLF_STATUS_BAR_WIDTH 320u
#define WOLF_STATUS_BAR_HEIGHT 40u

typedef struct WolfRectangle {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} WolfRectangle;

typedef struct WolfScreenLayout {
    /* Physical pixels per original pixel; zero when no status bar is shown. */
    uint32_t status_bar_scale;
    /* The full-width strip the bar sits in, and the bar itself centred within it. */
    WolfRectangle status_strip;
    WolfRectangle status_bar;
    WolfRectangle view;
} WolfScreenLayout;

/*
 * Divide a window between the 3D view and the status bar. Without a bar, or
 * when the window is too short to keep any view above one, the view fills
 * the whole window. Returns false for an empty window.
 */
bool wolf_screen_layout_compute(
    uint32_t window_width,
    uint32_t window_height,
    bool show_status_bar,
    WolfScreenLayout *layout);

#endif
