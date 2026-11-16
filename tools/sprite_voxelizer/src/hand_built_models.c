#include "hand_built_models.h"

#include <math.h>

/*
 * Palette entries sampled from the original sprites. Naming them keeps the
 * model code readable and records where each colour came from.
 */
enum {
    /* SPR_STAT_2: orange table and chairs. */
    TABLE_TOP_LIGHT = 57,
    TABLE_TOP_RIM = 59,
    TABLE_EDGE = 61,
    TABLE_UNDERSIDE = 63,
    CHAIR_OUTLINE = 60,
    CHAIR_FACE = 62,
    CHAIR_SHADE = 63,
    WOOD_DARK = 214,
    WOOD_MID = 217,
    WOOD_LIGHT = 220,
    PEDESTAL = 218,
    CANDLE_RED = 41,
    CANDLE_TIP = 32,
    HOLDER_LIGHT = 17,
    HOLDER_SHADE = 23,
    /* SPR_STAT_22: grey bed. */
    MATTRESS_TOP = 19,
    MATTRESS_CROWN = 20,
    MATTRESS_SIDE = 23,
    MATTRESS_SEAM = 24,
    PILLOW_TOP = 17,
    PILLOW_SIDE = 21,
    FRAME_TOP = 28,
    FRAME_SIDE = 29,
    FRAME_DARK = 30,
    SPRING_HOOK = 0
};

static void set_voxel(WolfVoxelModel *model, int x, int y, int z, int palette_index)
{
    if (wolf_voxel_in_bounds(x, y, z)) {
        model->occupied[wolf_voxel_index(x, y, z)] = 1u;
        model->palette_index[wolf_voxel_index(x, y, z)] = (uint8_t)palette_index;
    }
}

/* Inclusive box; its top layer takes top_colour so lit surfaces read as in the sprite. */
static void paint_box(WolfVoxelModel *model, int x0, int x1, int y0, int y1, int z0, int z1,
    int side_colour, int top_colour)
{
    for (int z = z0; z <= z1; ++z) {
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                set_voxel(model, x, y, z, z == z1 ? top_colour : side_colour);
            }
        }
    }
}

/* Upright cylinder around (centre_x, centre_y) in voxel-centre coordinates. */
static void paint_cylinder(WolfVoxelModel *model, double centre_x, double centre_y, double radius,
    int z0, int z1, int side_colour, int top_colour)
{
    for (int z = z0; z <= z1; ++z) {
        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                double dx = (double)x - centre_x;
                double dy = (double)y - centre_y;

                if (dx * dx + dy * dy <= radius * radius) {
                    set_voxel(model, x, y, z, z == z1 ? top_colour : side_colour);
                }
            }
        }
    }
}

/*
 * One ladder-back chair. facing is +1 when the seat faces +x (the chair on
 * the left of the table) and -1 for the mirrored chair on the right.
 */
static void paint_chair(WolfVoxelModel *model, int back_x, int facing)
{
    const int seat_depth = 11;
    const int seat_front_x = back_x + facing * (seat_depth - 1);
    const int seat_x0 = facing > 0 ? back_x : seat_front_x;
    const int seat_x1 = facing > 0 ? seat_front_x : back_x;
    const int seat_y0 = 26;
    const int seat_y1 = 37;
    const int leg_xs[2] = {back_x, seat_front_x - facing};
    const int leg_ys[2] = {seat_y0, seat_y1 - 1};

    /* Four legs from the floor to just under the seat. */
    for (int leg_x = 0; leg_x < 2; ++leg_x) {
        for (int leg_y = 0; leg_y < 2; ++leg_y) {
            int x0 = leg_xs[leg_x] < leg_xs[leg_x] + facing ? leg_xs[leg_x] : leg_xs[leg_x] + facing;

            paint_box(model, x0, x0 + 1, leg_ys[leg_y], leg_ys[leg_y] + 1, 0, 12, WOOD_MID, WOOD_MID);
        }
    }
    /* Stretchers between the legs, as drawn low on the sprite's chairs. */
    paint_box(model, seat_x0, seat_x1, seat_y0, seat_y0, 4, 4, WOOD_DARK, WOOD_DARK);
    paint_box(model, seat_x0, seat_x1, seat_y1, seat_y1, 4, 4, WOOD_DARK, WOOD_DARK);
    /* Seat. */
    paint_box(model, seat_x0, seat_x1, seat_y0, seat_y1, 13, 15, CHAIR_FACE, CHAIR_OUTLINE);
    /* Ladder back: two posts and two rails rising from the outer edge of the seat. */
    {
        int post_x0 = facing > 0 ? back_x : back_x - 1;

        paint_box(model, post_x0, post_x0 + 1, seat_y0, seat_y0 + 1, 16, 30, CHAIR_SHADE, CHAIR_OUTLINE);
        paint_box(model, post_x0, post_x0 + 1, seat_y1 - 1, seat_y1, 16, 30, CHAIR_SHADE, CHAIR_OUTLINE);
        paint_box(model, post_x0, post_x0 + 1, seat_y0 + 2, seat_y1 - 2, 27, 28, CHAIR_FACE, CHAIR_SHADE);
        paint_box(model, post_x0, post_x0 + 1, seat_y0 + 2, seat_y1 - 2, 19, 20, CHAIR_FACE, CHAIR_SHADE);
    }
}

static void build_table_and_chairs(WolfVoxelModel *model)
{
    /* The table in the sprite spans columns 12-54 and is centred on column 33. */
    const double centre_x = 33.0;
    const double centre_y = 31.5;

    /* Splayed four-footed base, sloping down from a hub to the floor. */
    paint_cylinder(model, centre_x, centre_y, 3.0, 0, 5, WOOD_MID, WOOD_LIGHT);
    for (int foot = 0; foot < 4; ++foot) {
        double direction_x = (foot & 1) ? 1.0 : -1.0;
        double direction_y = (foot & 2) ? 1.0 : -1.0;

        for (int step = 2; step <= 9; ++step) {
            int x = (int)lround(centre_x + direction_x * step * 0.75);
            int y = (int)lround(centre_y + direction_y * step * 0.75);
            int height = step < 5 ? 2 : (step < 8 ? 1 : 0);

            paint_box(model, x, x + 1, y, y + 1, 0, height, WOOD_DARK, WOOD_MID);
        }
    }
    /* Turned pedestal column. */
    paint_cylinder(model, centre_x, centre_y, 1.6, 6, 20, PEDESTAL, PEDESTAL);
    /* Round top: dark underside and edge, bright upper face with a rim. */
    paint_cylinder(model, centre_x, centre_y, 21.0, 21, 22, TABLE_UNDERSIDE, TABLE_EDGE);
    paint_cylinder(model, centre_x, centre_y, 21.0, 23, 23, TABLE_EDGE, TABLE_EDGE);
    paint_cylinder(model, centre_x, centre_y, 21.0, 24, 24, TABLE_TOP_RIM, TABLE_TOP_RIM);
    paint_cylinder(model, centre_x, centre_y, 19.0, 24, 24, TABLE_TOP_LIGHT, TABLE_TOP_LIGHT);
    /* White candle holder with two red candles, as on the original table. */
    paint_box(model, 30, 36, 29, 34, 25, 26, HOLDER_SHADE, HOLDER_LIGHT);
    paint_box(model, 31, 31, 31, 32, 27, 32, CANDLE_RED, CANDLE_TIP);
    paint_box(model, 35, 35, 31, 32, 27, 32, CANDLE_RED, CANDLE_TIP);

    /* Chairs either side, seats tucked toward the table as drawn. */
    paint_chair(model, 8, 1);
    paint_chair(model, 57, -1);
}

static void build_bed(WolfVoxelModel *model)
{
    /* The sprite shows the bed end-on: 41 columns wide (11-51); it is made 60 long. */
    const int x0 = 11;
    const int x1 = 51;
    const int y0 = 2;
    const int y1 = 61;

    /* Corner posts of the metal frame. */
    paint_box(model, x0, x0 + 2, y0, y0 + 2, 0, 11, FRAME_SIDE, FRAME_TOP);
    paint_box(model, x1 - 2, x1, y0, y0 + 2, 0, 11, FRAME_SIDE, FRAME_TOP);
    paint_box(model, x0, x0 + 2, y1 - 2, y1, 0, 11, FRAME_SIDE, FRAME_TOP);
    paint_box(model, x1 - 2, x1, y1 - 2, y1, 0, 11, FRAME_SIDE, FRAME_TOP);
    /* Frame rails and the sprung base they carry. */
    paint_box(model, x0, x1, y0, y1, 9, 11, FRAME_DARK, FRAME_TOP);
    paint_box(model, x0, x1, y0, y0 + 1, 9, 11, FRAME_SIDE, FRAME_TOP);
    paint_box(model, x0, x1, y1 - 1, y1, 9, 11, FRAME_SIDE, FRAME_TOP);
    /* Spring hangers under the rail, the black marks under the sprite's frame. */
    for (int hanger = 0; hanger < 4; ++hanger) {
        int x = hanger < 2 ? 17 + hanger : 44 + (hanger - 2);

        set_voxel(model, x, y0 + 2, 8, SPRING_HOOK);
        set_voxel(model, x, y0 + 2, 7, SPRING_HOOK);
        set_voxel(model, x, y1 - 2, 8, SPRING_HOOK);
        set_voxel(model, x, y1 - 2, 7, SPRING_HOOK);
    }
    /* Mattress with a seam along its lower edge and a slightly domed top. */
    paint_box(model, x0 + 1, x1 - 1, y0 + 1, y1 - 1, 12, 12, MATTRESS_SEAM, MATTRESS_SEAM);
    paint_box(model, x0 + 1, x1 - 1, y0 + 1, y1 - 1, 13, 17, MATTRESS_SIDE, MATTRESS_TOP);
    paint_box(model, x0 + 3, x1 - 3, y0 + 3, y1 - 3, 18, 18, MATTRESS_CROWN, MATTRESS_CROWN);
    /* Pillow at the head end, the raised shape seen above the mattress in the sprite. */
    paint_box(model, 19, 43, y1 - 13, y1 - 4, 19, 20, PILLOW_SIDE, PILLOW_TOP);
    paint_box(model, 22, 40, y1 - 11, y1 - 6, 21, 21, PILLOW_TOP, PILLOW_TOP);
}

bool wolf_build_hand_model(WolfHandBuiltModel which, WolfVoxelModel *model)
{
    wolf_voxel_model_clear(model);
    switch (which) {
    case WOLF_HAND_BUILT_TABLE_AND_CHAIRS:
        build_table_and_chairs(model);
        return true;
    case WOLF_HAND_BUILT_BED:
        build_bed(model);
        return true;
    case WOLF_HAND_BUILT_NONE:
    default:
        return false;
    }
}
