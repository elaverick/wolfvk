#include "wolfvk/status_bar.h"

#include "game_palette.h"

#include <stdio.h>
#include <string.h>

/* The original status-bar routines position pictures in 8-pixel columns. */
#define STATUS_COLUMN_WIDTH 8u
/* Each face damage level has three glances: ahead, left and right. */
#define FACE_FRAMES_PER_LEVEL 3u
#define FACE_HEALTH_PER_LEVEL 16

/* Copy one picture onto the bar, clipped to the bar's bounds. */
static void draw_picture(
    const WolfVgaPicture *picture,
    uint32_t x,
    uint32_t y,
    uint8_t *indexed_pixels)
{
    if (picture->pixels == NULL) {
        return;
    }
    for (uint32_t row = 0u; row < picture->height && y + row < WOLF_STATUS_BAR_HEIGHT; ++row) {
        for (uint32_t column = 0u;
             column < picture->width && x + column < WOLF_STATUS_BAR_WIDTH;
             ++column) {
            indexed_pixels[(y + row) * WOLF_STATUS_BAR_WIDTH + x + column] =
                picture->pixels[(size_t)row * picture->width + column];
        }
    }
}

/* Draw a latched picture at the original StatusDrawPic position: x in 8-pixel columns, y in pixels. */
static void status_draw_picture(
    const WolfStatusBarGraphics *status_graphics,
    uint32_t column,
    uint32_t y,
    uint32_t picture_chunk,
    uint8_t *indexed_pixels)
{
    if (picture_chunk < WOLF_STATUS_BAR_FIRST_PICTURE ||
        picture_chunk > WOLF_STATUS_BAR_LAST_PICTURE) {
        return;
    }
    draw_picture(
        &status_graphics->pictures[picture_chunk - WOLF_STATUS_BAR_FIRST_PICTURE],
        column * STATUS_COLUMN_WIDTH, y, indexed_pixels);
}

/*
 * LatchNumber: right-justify a number in a field of digit pictures, padding
 * with blanks on the left and keeping only the lowest digits if it is too wide.
 */
static void latch_number(
    const WolfStatusBarGraphics *status_graphics,
    uint32_t column,
    uint32_t y,
    uint32_t width,
    int32_t number,
    uint8_t *indexed_pixels)
{
    char digits[16];
    size_t length;
    size_t first_digit;

    (void)snprintf(digits, sizeof(digits), "%ld", (long)(number < 0 ? 0 : number));
    length = strlen(digits);

    while (length < width) {
        status_draw_picture(status_graphics, column, y, WOLF_VGA_N_BLANKPIC, indexed_pixels);
        ++column;
        --width;
    }

    first_digit = length <= width ? 0u : length - width;
    for (size_t digit = first_digit; digit < length; ++digit) {
        status_draw_picture(
            status_graphics, column, y,
            WOLF_VGA_N_0PIC + (uint32_t)(digits[digit] - '0'), indexed_pixels);
        ++column;
    }
}

bool wolf_status_bar_graphics_load(
    const WolfVgaGraphics *graphics,
    WolfStatusBarGraphics *status_graphics)
{
    WolfStatusBarGraphics decoded = {0};

    if (graphics == NULL || status_graphics == NULL) {
        return false;
    }
    if (!wolf_vga_graphics_decode_picture(graphics, WOLF_VGA_STATUSBARPIC, &decoded.background) ||
        decoded.background.width != WOLF_STATUS_BAR_WIDTH ||
        decoded.background.height != WOLF_STATUS_BAR_HEIGHT) {
        wolf_status_bar_graphics_destroy(&decoded);
        return false;
    }
    for (uint32_t index = 0u; index < WOLF_STATUS_BAR_PICTURE_COUNT; ++index) {
        if (!wolf_vga_graphics_decode_picture(
                graphics, WOLF_STATUS_BAR_FIRST_PICTURE + index, &decoded.pictures[index])) {
            wolf_status_bar_graphics_destroy(&decoded);
            return false;
        }
    }

    *status_graphics = decoded;
    return true;
}

void wolf_status_bar_graphics_destroy(WolfStatusBarGraphics *status_graphics)
{
    if (status_graphics == NULL) {
        return;
    }
    wolf_vga_picture_destroy(&status_graphics->background);
    for (uint32_t index = 0u; index < WOLF_STATUS_BAR_PICTURE_COUNT; ++index) {
        wolf_vga_picture_destroy(&status_graphics->pictures[index]);
    }
}

uint32_t wolf_status_bar_face_picture(const WolfPlayerState *player)
{
    int32_t health;
    uint32_t frame;

    if (player == NULL || player->health <= 0) {
        return WOLF_VGA_FACE8APIC;
    }
    health = player->health > WOLF_PLAYER_MAX_HEALTH ? WOLF_PLAYER_MAX_HEALTH : player->health;
    frame = player->face_frame < FACE_FRAMES_PER_LEVEL ? player->face_frame : 0u;
    return WOLF_VGA_FACE1APIC +
        FACE_FRAMES_PER_LEVEL * (uint32_t)((WOLF_PLAYER_MAX_HEALTH - health) / FACE_HEALTH_PER_LEVEL) +
        frame;
}

bool wolf_status_bar_compose(
    const WolfStatusBarGraphics *status_graphics,
    const WolfPlayerState *player,
    uint8_t indexed_pixels[WOLF_STATUS_BAR_PIXEL_COUNT])
{
    if (status_graphics == NULL || player == NULL || indexed_pixels == NULL ||
        status_graphics->background.pixels == NULL) {
        return false;
    }

    /* Same order and positions as DrawPlayScreen and the Draw* routines in WL_AGENT.C. */
    draw_picture(&status_graphics->background, 0u, 0u, indexed_pixels);
    status_draw_picture(
        status_graphics, 17u, 4u, wolf_status_bar_face_picture(player), indexed_pixels);
    latch_number(status_graphics, 21u, 16u, 3u, player->health, indexed_pixels);
    latch_number(status_graphics, 14u, 16u, 1u, player->lives, indexed_pixels);
    latch_number(status_graphics, 2u, 16u, 2u, (int32_t)player->map_number + 1, indexed_pixels);
    latch_number(status_graphics, 27u, 16u, 2u, player->ammo, indexed_pixels);
    status_draw_picture(
        status_graphics, 30u, 4u,
        (player->keys & WOLF_PLAYER_GOLD_KEY) != 0u ? WOLF_VGA_GOLDKEYPIC : WOLF_VGA_NOKEYPIC,
        indexed_pixels);
    status_draw_picture(
        status_graphics, 30u, 20u,
        (player->keys & WOLF_PLAYER_SILVER_KEY) != 0u ? WOLF_VGA_SILVERKEYPIC : WOLF_VGA_NOKEYPIC,
        indexed_pixels);
    status_draw_picture(
        status_graphics, 32u, 8u, WOLF_VGA_KNIFEPIC + (uint32_t)player->weapon, indexed_pixels);
    latch_number(status_graphics, 6u, 16u, 6u, player->score, indexed_pixels);
    return true;
}

void wolf_status_bar_expand_rgba(
    const uint8_t *indexed_pixels,
    uint32_t pixel_count,
    uint8_t *rgba_pixels)
{
    if (indexed_pixels == NULL || rgba_pixels == NULL) {
        return;
    }
    for (uint32_t pixel = 0u; pixel < pixel_count; ++pixel) {
        memcpy(rgba_pixels + (size_t)pixel * 4u,
            wolf_game_palette_rgba + (size_t)indexed_pixels[pixel] * 4u, 4u);
    }
}
