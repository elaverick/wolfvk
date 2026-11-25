#include "wolfvk/screen_layout.h"

#include <string.h>

bool wolf_screen_layout_compute(
    uint32_t window_width,
    uint32_t window_height,
    bool show_status_bar,
    WolfScreenLayout *layout)
{
    uint32_t horizontal_scale;
    uint32_t vertical_scale;
    uint32_t scale;
    uint32_t bar_width;
    uint32_t bar_height;

    if (layout == NULL || window_width == 0u || window_height == 0u) {
        return false;
    }

    memset(layout, 0, sizeof(*layout));
    layout->view = (WolfRectangle){0u, 0u, window_width, window_height};
    if (!show_status_bar) {
        return true;
    }

    /* Scale as the whole original screen would, so the bar keeps its original share of the height. */
    horizontal_scale = window_width / WOLF_ORIGINAL_SCREEN_WIDTH;
    vertical_scale = window_height / WOLF_ORIGINAL_SCREEN_HEIGHT;
    scale = horizontal_scale < vertical_scale ? horizontal_scale : vertical_scale;
    if (scale == 0u) {
        scale = 1u;
    }

    bar_width = WOLF_STATUS_BAR_WIDTH * scale;
    bar_height = WOLF_STATUS_BAR_HEIGHT * scale;
    if (bar_height >= window_height) {
        return true;
    }

    layout->status_bar_scale = scale;
    layout->view.height = window_height - bar_height;
    layout->status_strip = (WolfRectangle){0u, layout->view.height, window_width, bar_height};
    /* A window narrower than one bar keeps the bar's left edge, as the original screen did. */
    layout->status_bar = (WolfRectangle){
        bar_width < window_width ? (window_width - bar_width) / 2u : 0u,
        layout->view.height,
        bar_width,
        bar_height
    };
    return true;
}
