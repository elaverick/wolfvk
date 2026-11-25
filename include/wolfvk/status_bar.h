#ifndef WOLFVK_STATUS_BAR_H
#define WOLFVK_STATUS_BAR_H

#include "wolfvk/player_state.h"
#include "wolfvk/screen_layout.h"
#include "wolfvk/vga_graphics.h"

#include <stdbool.h>
#include <stdint.h>

#define WOLF_STATUS_BAR_PIXEL_COUNT (WOLF_STATUS_BAR_WIDTH * WOLF_STATUS_BAR_HEIGHT)
/* The original LoadLatchMem keeps pictures 91-134 ready to draw over the bar; it uses 91-132. */
#define WOLF_STATUS_BAR_FIRST_PICTURE WOLF_VGA_KNIFEPIC
#define WOLF_STATUS_BAR_LAST_PICTURE WOLF_VGA_MUTANTBJPIC
#define WOLF_STATUS_BAR_PICTURE_COUNT \
    (WOLF_STATUS_BAR_LAST_PICTURE - WOLF_STATUS_BAR_FIRST_PICTURE + 1)
/* DrawPlayBorder fills around the view with palette colour 127. */
#define WOLF_STATUS_BAR_BORDER_COLOUR 127u

/* The bar's background and every picture drawn over it, decoded from VGAGRAPH. */
typedef struct WolfStatusBarGraphics {
    WolfVgaPicture background;
    WolfVgaPicture pictures[WOLF_STATUS_BAR_PICTURE_COUNT];
} WolfStatusBarGraphics;

/* Decode STATUSBARPIC and the face, digit, key and weapon pictures. */
bool wolf_status_bar_graphics_load(
    const WolfVgaGraphics *graphics,
    WolfStatusBarGraphics *status_graphics);

void wolf_status_bar_graphics_destroy(WolfStatusBarGraphics *status_graphics);

/* The face picture DrawFace would show: one of seven damage levels and three glances, or the dead face. */
uint32_t wolf_status_bar_face_picture(const WolfPlayerState *player);

/*
 * Draw the 320x40 bar as palette indices, placing the floor, score, lives,
 * face, health, ammunition, keys and weapon where DrawPlayScreen does.
 */
bool wolf_status_bar_compose(
    const WolfStatusBarGraphics *status_graphics,
    const WolfPlayerState *player,
    uint8_t indexed_pixels[WOLF_STATUS_BAR_PIXEL_COUNT]);

/* Expand palette indices to RGBA through the original VGA palette. */
void wolf_status_bar_expand_rgba(
    const uint8_t *indexed_pixels,
    uint32_t pixel_count,
    uint8_t *rgba_pixels);

#endif
