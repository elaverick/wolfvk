#include "unity.h"

#include "wolfvk/screen_layout.h"
#include "wolfvk/status_bar.h"

#include <stdlib.h>
#include <string.h>

#define TEST_PICTURE_SIZE 8u
#define TEST_BACKGROUND_INDEX 1u

/*
 * Stand-in graphics: a plain background and an 8x8 block for every latched
 * picture, filled with its own chunk number so the composed bar shows which
 * picture landed where.
 */
static uint8_t background_pixels[WOLF_STATUS_BAR_PIXEL_COUNT];
static uint8_t picture_pixels[WOLF_STATUS_BAR_PICTURE_COUNT][TEST_PICTURE_SIZE * TEST_PICTURE_SIZE];

static WolfStatusBarGraphics make_test_graphics(void)
{
    WolfStatusBarGraphics graphics;

    memset(&graphics, 0, sizeof(graphics));
    memset(background_pixels, TEST_BACKGROUND_INDEX, sizeof(background_pixels));
    graphics.background.width = WOLF_STATUS_BAR_WIDTH;
    graphics.background.height = WOLF_STATUS_BAR_HEIGHT;
    graphics.background.pixels = background_pixels;
    for (uint32_t index = 0u; index < WOLF_STATUS_BAR_PICTURE_COUNT; ++index) {
        memset(picture_pixels[index], (int)(WOLF_STATUS_BAR_FIRST_PICTURE + index),
            sizeof(picture_pixels[index]));
        graphics.pictures[index].width = TEST_PICTURE_SIZE;
        graphics.pictures[index].height = TEST_PICTURE_SIZE;
        graphics.pictures[index].pixels = picture_pixels[index];
    }
    return graphics;
}

/* The picture at an original StatusDrawPic position (x in 8-pixel columns). */
static uint8_t picture_at(const uint8_t *bar, uint32_t column, uint32_t y)
{
    return bar[y * WOLF_STATUS_BAR_WIDTH + column * 8u];
}

void test_status_bar_face_tracks_health_and_glance(void)
{
    WolfPlayerState player;

    wolf_player_state_new_game(&player, 0u);
    TEST_ASSERT_EQUAL_UINT32(WOLF_VGA_FACE1APIC, wolf_status_bar_face_picture(&player));
    player.face_frame = 2u;
    TEST_ASSERT_EQUAL_UINT32(WOLF_VGA_FACE1APIC + 2u, wolf_status_bar_face_picture(&player));

    /* Every 16 points of damage moves to the next of the seven bloodied faces. */
    player.face_frame = 1u;
    player.health = 50;
    TEST_ASSERT_EQUAL_UINT32(WOLF_VGA_FACE1APIC + 9u + 1u, wolf_status_bar_face_picture(&player));
    player.health = 1;
    TEST_ASSERT_EQUAL_UINT32(WOLF_VGA_FACE1APIC + 18u + 1u, wolf_status_bar_face_picture(&player));
    player.health = 0;
    TEST_ASSERT_EQUAL_UINT32(WOLF_VGA_FACE8APIC, wolf_status_bar_face_picture(&player));
}

void test_status_bar_places_numbers_keys_face_and_weapon_like_the_original(void)
{
    WolfStatusBarGraphics graphics = make_test_graphics();
    WolfPlayerState player;
    static uint8_t bar[WOLF_STATUS_BAR_PIXEL_COUNT];

    wolf_player_state_new_game(&player, 0u);
    player.score = 1234;
    player.keys = WOLF_PLAYER_GOLD_KEY;
    TEST_ASSERT_TRUE(wolf_status_bar_compose(&graphics, &player, bar));

    /* Floor 1 in a 2-digit field at column 2. */
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_BLANKPIC, picture_at(bar, 2u, 16u));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_0PIC + 1u, picture_at(bar, 3u, 16u));
    /* Score 1234 right-aligned in six digits at column 6. */
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_BLANKPIC, picture_at(bar, 6u, 16u));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_BLANKPIC, picture_at(bar, 7u, 16u));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_0PIC + 1u, picture_at(bar, 8u, 16u));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_0PIC + 4u, picture_at(bar, 11u, 16u));
    /* Three lives at column 14. */
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_0PIC + 3u, picture_at(bar, 14u, 16u));
    /* The healthy face at column 17, row 4. */
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_FACE1APIC, picture_at(bar, 17u, 4u));
    /* Health 100 in three digits at column 21. */
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_0PIC + 1u, picture_at(bar, 21u, 16u));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_0PIC, picture_at(bar, 23u, 16u));
    /* Eight bullets in two digits at column 27. */
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_BLANKPIC, picture_at(bar, 27u, 16u));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_0PIC + 8u, picture_at(bar, 28u, 16u));
    /* Gold key above, empty silver slot below, at column 30. */
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_GOLDKEYPIC, picture_at(bar, 30u, 4u));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_NOKEYPIC, picture_at(bar, 30u, 20u));
    /* The pistol at column 32, row 8. */
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_GUNPIC, picture_at(bar, 32u, 8u));
    /* Untouched areas keep the background. */
    TEST_ASSERT_EQUAL_UINT8(TEST_BACKGROUND_INDEX, bar[0]);
}

void test_status_bar_reflects_changed_player_values(void)
{
    WolfStatusBarGraphics graphics = make_test_graphics();
    WolfPlayerState player;
    static uint8_t bar[WOLF_STATUS_BAR_PIXEL_COUNT];

    wolf_player_state_new_game(&player, 0u);
    wolf_player_state_take_damage(&player, 43);
    wolf_player_state_give_weapon(&player, WOLF_WEAPON_MACHINE_GUN);
    wolf_player_state_give_key(&player, WOLF_PLAYER_SILVER_KEY);
    TEST_ASSERT_TRUE(wolf_status_bar_compose(&graphics, &player, bar));

    /* Health 57: a blank, then 5 and 7. */
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_BLANKPIC, picture_at(bar, 21u, 16u));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_0PIC + 5u, picture_at(bar, 22u, 16u));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_0PIC + 7u, picture_at(bar, 23u, 16u));
    /* 43 damage is the third face. */
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_FACE1APIC + 6u, picture_at(bar, 17u, 4u));
    /* 14 bullets. */
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_0PIC + 1u, picture_at(bar, 27u, 16u));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_0PIC + 4u, picture_at(bar, 28u, 16u));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_NOKEYPIC, picture_at(bar, 30u, 4u));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_SILVERKEYPIC, picture_at(bar, 30u, 20u));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_MACHINEGUNPIC, picture_at(bar, 32u, 8u));

    /* A number wider than its field keeps its lowest digits, as LatchNumber does. */
    player.lives = 12;
    TEST_ASSERT_TRUE(wolf_status_bar_compose(&graphics, &player, bar));
    TEST_ASSERT_EQUAL_UINT8(WOLF_VGA_N_0PIC + 2u, picture_at(bar, 14u, 16u));

    TEST_ASSERT_FALSE(wolf_status_bar_compose(NULL, &player, bar));
    TEST_ASSERT_FALSE(wolf_status_bar_compose(&graphics, NULL, bar));
}

void test_status_bar_expands_through_the_original_palette(void)
{
    const uint8_t indices[2] = {0u, 15u};
    uint8_t rgba[8] = {0};
    const uint8_t expected[8] = {0u, 0u, 0u, 255u, 252u, 252u, 252u, 255u};

    wolf_status_bar_expand_rgba(indices, 2u, rgba);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, rgba, 8u);
}

void test_screen_layout_uses_whole_number_scales_and_keeps_the_view_above_the_bar(void)
{
    WolfScreenLayout layout;

    /* 1280x720: 4x would need 800 lines for a 320x200 screen, so 3x is used. */
    TEST_ASSERT_TRUE(wolf_screen_layout_compute(1280u, 720u, true, &layout));
    TEST_ASSERT_EQUAL_UINT32(3u, layout.status_bar_scale);
    TEST_ASSERT_EQUAL_UINT32(160u, layout.status_bar.x);
    TEST_ASSERT_EQUAL_UINT32(600u, layout.status_bar.y);
    TEST_ASSERT_EQUAL_UINT32(960u, layout.status_bar.width);
    TEST_ASSERT_EQUAL_UINT32(120u, layout.status_bar.height);
    TEST_ASSERT_EQUAL_UINT32(0u, layout.status_strip.x);
    TEST_ASSERT_EQUAL_UINT32(1280u, layout.status_strip.width);
    TEST_ASSERT_EQUAL_UINT32(1280u, layout.view.width);
    TEST_ASSERT_EQUAL_UINT32(600u, layout.view.height);
    TEST_ASSERT_EQUAL_UINT32(layout.view.y + layout.view.height, layout.status_bar.y);

    /* The original screen size gives the original 160-line view. */
    TEST_ASSERT_TRUE(wolf_screen_layout_compute(320u, 200u, true, &layout));
    TEST_ASSERT_EQUAL_UINT32(1u, layout.status_bar_scale);
    TEST_ASSERT_EQUAL_UINT32(160u, layout.view.height);

    /* A wide 1080p window is limited by its height: 5x. */
    TEST_ASSERT_TRUE(wolf_screen_layout_compute(3840u, 1080u, true, &layout));
    TEST_ASSERT_EQUAL_UINT32(5u, layout.status_bar_scale);
    TEST_ASSERT_EQUAL_UINT32(880u, layout.view.height);
    TEST_ASSERT_EQUAL_UINT32((3840u - 1600u) / 2u, layout.status_bar.x);
}

void test_screen_layout_handles_small_windows_and_no_bar(void)
{
    WolfScreenLayout layout;

    /* Smaller than the original screen: still 1x, with the bar's left edge kept. */
    TEST_ASSERT_TRUE(wolf_screen_layout_compute(200u, 100u, true, &layout));
    TEST_ASSERT_EQUAL_UINT32(1u, layout.status_bar_scale);
    TEST_ASSERT_EQUAL_UINT32(0u, layout.status_bar.x);
    TEST_ASSERT_EQUAL_UINT32(60u, layout.view.height);

    /* Too short to keep any view above a bar: the view takes the whole window. */
    TEST_ASSERT_TRUE(wolf_screen_layout_compute(400u, 40u, true, &layout));
    TEST_ASSERT_EQUAL_UINT32(0u, layout.status_bar_scale);
    TEST_ASSERT_EQUAL_UINT32(40u, layout.view.height);

    TEST_ASSERT_TRUE(wolf_screen_layout_compute(1280u, 720u, false, &layout));
    TEST_ASSERT_EQUAL_UINT32(0u, layout.status_bar_scale);
    TEST_ASSERT_EQUAL_UINT32(720u, layout.view.height);

    TEST_ASSERT_FALSE(wolf_screen_layout_compute(0u, 720u, true, &layout));
    TEST_ASSERT_FALSE(wolf_screen_layout_compute(1280u, 720u, true, NULL));
}
