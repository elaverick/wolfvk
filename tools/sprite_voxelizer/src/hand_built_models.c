#include "hand_built_models.h"

#include "game_palette.h"
#include "reconstruction.h"

#include <math.h>
#include <string.h>

/*
 * Palette entries sampled from the original sprites. Naming them keeps the
 * model code readable and records where each colour came from; every one
 * appears in its object's own sprite.
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
    MATTRESS_TOP = 20,
    MATTRESS_CROWN = 19,
    MATTRESS_SIDE = 23,
    MATTRESS_SEAM = 24,
    PILLOW_TOP = 19,
    PILLOW_SIDE = 21,
    FRAME_TOP = 28,
    FRAME_SIDE = 29,
    FRAME_DARK = 30,
    SPRING_HOOK = 0,
    /* SPR_STAT_24: food. */
    PLATE_UNDERSIDE = 126,
    PLATE_RIM = 121,
    PLATE_RIM_LIGHT = 119,
    PLATE_SURFACE = 123,
    POTATO_LIGHT = 16,
    POTATO_SHADE = 19,
    GREENS_LIGHT = 99,
    GREENS_MID = 104,
    GREENS_DARK = 105,
    MEAT_SKIN = 61,
    MEAT_SKIN_DARK = 58,
    MEAT_SIDE = 212,
    MEAT_UNDERSIDE = 216,
    BONE = 18,
    BONE_KNUCKLE = 17,
    /* SPR_STAT_25: first aid kit. */
    KIT_LID = 16,
    KIT_SIDE_UPPER = 17,
    KIT_SIDE = 20,
    KIT_SIDE_LOWER = 22,
    KIT_CROSS = 52,
    KIT_CROSS_EDGE = 50,
    KIT_LATCH = 31,
    KIT_LATCH_SHADE = 28,
    KIT_TRIM = 151,
    KIT_TRIM_DARK = 153
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

/* Upright cylinder around (centre_x, centre_y) in voxel-index coordinates. */
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
 * Ellipsoid with three bands of colour by height, so a rounded shape keeps
 * the lit top, mid tone and shaded underside the artist painted.
 */
static void paint_ellipsoid(WolfVoxelModel *model, double centre_x, double centre_y, double centre_z,
    double radius_x, double radius_y, double radius_z, int top_colour, int middle_colour, int bottom_colour)
{
    for (int z = 0; z < WOLF_VOXEL_DIM; ++z) {
        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                double dx = ((double)x - centre_x) / radius_x;
                double dy = ((double)y - centre_y) / radius_y;
                double dz = ((double)z - centre_z) / radius_z;

                if (dx * dx + dy * dy + dz * dz <= 1.0) {
                    int colour = dz > 0.35 ? top_colour : (dz < -0.35 ? bottom_colour : middle_colour);

                    set_voxel(model, x, y, z, colour);
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

/*
 * A turkey leg on a plate, as the sprite shows: a cyan-rimmed plate (columns
 * 16-43), a white mound of potato on the left, greens on the right and a
 * roast drumstick in the middle with its white bone pointing right.
 */
static void build_food(WolfVoxelModel *model)
{
    const double plate_x = 30.0;
    const double plate_y = 31.5;

    /* Shallow plate: dark underside, raised rim, flat eating surface. */
    paint_cylinder(model, plate_x, plate_y, 13.5, 0, 0, PLATE_UNDERSIDE, PLATE_UNDERSIDE);
    paint_cylinder(model, plate_x, plate_y, 14.0, 1, 1, PLATE_RIM, PLATE_RIM_LIGHT);
    paint_cylinder(model, plate_x, plate_y, 11.5, 1, 1, PLATE_SURFACE, PLATE_SURFACE);
    /* Potato on the left and a few greens to the right of the meat. */
    paint_ellipsoid(model, 22.5, 33.0, 2.0, 3.2, 3.2, 2.0, POTATO_LIGHT, POTATO_SHADE, POTATO_SHADE);
    for (int y = 26; y <= 36; ++y) {
        for (int x = 34; x <= 39; ++x) {
            int pattern = (x * 3 + y * 5) % 7;

            if (pattern < 4 && (x - 36.5) * (x - 36.5) + (y - 31.5) * (y - 31.5) <= 16.0) {
                set_voxel(model, x, y, 2, pattern == 0 ? GREENS_LIGHT : (pattern < 3 ? GREENS_MID : GREENS_DARK));
            }
        }
    }
    /* Drumstick: a roast, rounded body with crisp skin on top. */
    paint_ellipsoid(model, 29.0, 31.5, 4.5, 7.0, 4.2, 3.2, MEAT_SKIN, MEAT_SIDE, MEAT_UNDERSIDE);
    paint_ellipsoid(model, 27.0, 31.0, 6.8, 3.0, 2.2, 1.0, MEAT_SKIN_DARK, MEAT_SKIN_DARK, MEAT_SKIN_DARK);
    /* Bone out to the right, ending in a knuckle. */
    for (int x = 35; x <= 40; ++x) {
        for (int y = 30; y <= 33; ++y) {
            for (int z = 3; z <= 6; ++z) {
                double dy = (double)y - 31.5;
                double dz = (double)z - 4.5;

                if (dy * dy + dz * dz <= 1.6 * 1.6) {
                    set_voxel(model, x, y, z, BONE);
                }
            }
        }
    }
    paint_ellipsoid(model, 41.5, 31.5, 4.5, 1.6, 2.4, 2.0, BONE_KNUCKLE, BONE_KNUCKLE, BONE);
}

/*
 * A flat white first aid kit: blue trim around the base, white sides with a
 * dark latch at the front, and a red cross on the white lid. The sprite's
 * box is 25-27 columns wide; it is made 20 deep and 6 high.
 */
static void build_first_aid(WolfVoxelModel *model)
{
    const int x0 = 19;
    const int x1 = 43;
    const int y0 = 22;
    const int y1 = 41;

    paint_box(model, x0, x1, y0, y1, 0, 0, KIT_TRIM_DARK, KIT_TRIM_DARK);
    paint_box(model, x0, x1, y0, y1, 1, 1, KIT_TRIM, KIT_TRIM);
    paint_box(model, x0, x1, y0, y1, 2, 2, KIT_SIDE_LOWER, KIT_SIDE_LOWER);
    paint_box(model, x0, x1, y0, y1, 3, 3, KIT_SIDE, KIT_SIDE);
    paint_box(model, x0, x1, y0, y1, 4, 4, KIT_SIDE_UPPER, KIT_SIDE_UPPER);
    paint_box(model, x0, x1, y0, y1, 5, 5, KIT_LID, KIT_LID);
    /* Red cross on the lid, outlined in the sprite's darker red. */
    paint_box(model, 25, 37, 29, 34, 5, 5, KIT_CROSS_EDGE, KIT_CROSS_EDGE);
    paint_box(model, 28, 34, 25, 38, 5, 5, KIT_CROSS_EDGE, KIT_CROSS_EDGE);
    paint_box(model, 26, 36, 30, 33, 5, 5, KIT_CROSS, KIT_CROSS);
    paint_box(model, 29, 33, 26, 37, 5, 5, KIT_CROSS, KIT_CROSS);
    /* Latch on the front face. */
    paint_box(model, 29, 32, y0, y0, 3, 3, KIT_LATCH, KIT_LATCH);
    paint_box(model, 29, 32, y0, y0, 2, 2, KIT_LATCH_SHADE, KIT_LATCH_SHADE);
}

/* Blood is any strongly red palette entry. */
static bool is_blood(uint8_t palette_index)
{
    const uint8_t *colour = wolf_game_palette_rgba + (size_t)palette_index * 4u;

    return colour[0] > 2 * colour[1] + 10 && colour[0] > 2 * colour[2] + 10;
}

/* Floor depth for a sprite row: rows lower on screen are nearer the viewer. */
static int floor_depth_for_row(int row, int nearest_row, double depth_per_row, double nearest_depth)
{
    return (int)lround(nearest_depth + (double)(nearest_row - row) * depth_per_row);
}

/*
 * Gibs: the sprite's skull is kept as drawn (inflated, sitting on the
 * floor) and everything else is laid flat. The sprite draws the floor in
 * perspective, so each row maps to a depth: blood becomes a puddle one
 * voxel deep and the bones lie on top of it.
 */
static void build_gibs(const WolfSpriteImage *reference, WolfVoxelModel *model)
{
    WolfSpriteImage skull;
    uint8_t component[WOLF_SPRITE_PIXELS];
    int stack[WOLF_SPRITE_PIXELS];
    int best_component = -1;
    int best_size = 0;
    int component_count = 0;
    int top_row = WOLF_SPRITE_SIZE;
    int bottom_row = -1;
    int skull_bottom = -1;
    const double depth_per_row = 3.0;
    double nearest_depth;

    if (reference == NULL) {
        return;
    }
    memset(component, 0, sizeof(component));
    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            if (reference->opaque[wolf_sprite_pixel_index(column, row)]) {
                top_row = row < top_row ? row : top_row;
                bottom_row = row;
            }
        }
    }
    if (bottom_row < 0) {
        return;
    }

    /* The skull is the largest connected group of non-blood pixels. */
    for (int seed = 0; seed < WOLF_SPRITE_PIXELS; ++seed) {
        int stack_size = 0;
        int size = 0;

        if (!reference->opaque[seed] || is_blood(reference->palette_index[seed]) || component[seed]) {
            continue;
        }
        ++component_count;
        component[seed] = (uint8_t)component_count;
        stack[stack_size++] = seed;
        while (stack_size > 0) {
            int pixel = stack[--stack_size];
            int column = pixel % WOLF_SPRITE_SIZE;
            int row = pixel / WOLF_SPRITE_SIZE;

            ++size;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int next_column = column + dx;
                    int next_row = row + dy;
                    int next;

                    if (next_column < 0 || next_row < 0 || next_column >= WOLF_SPRITE_SIZE || next_row >= WOLF_SPRITE_SIZE) {
                        continue;
                    }
                    next = (int)wolf_sprite_pixel_index(next_column, next_row);
                    if (reference->opaque[next] && !is_blood(reference->palette_index[next]) && !component[next]) {
                        component[next] = (uint8_t)component_count;
                        stack[stack_size++] = next;
                    }
                }
            }
        }
        if (size > best_size) {
            best_size = size;
            best_component = component_count;
        }
    }

    wolf_sprite_image_clear(&skull);
    for (int pixel = 0; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
        if (best_component > 0 && component[pixel] == best_component) {
            skull.opaque[pixel] = 1u;
            skull.palette_index[pixel] = reference->palette_index[pixel];
            skull_bottom = pixel / WOLF_SPRITE_SIZE > skull_bottom ? pixel / WOLF_SPRITE_SIZE : skull_bottom;
        }
    }

    /* Centre the floor's depth range on the tile. */
    nearest_depth = 31.5 - 0.5 * (double)(bottom_row - top_row) * depth_per_row;

    for (int row = top_row; row <= bottom_row; ++row) {
        int depth = floor_depth_for_row(row, bottom_row, depth_per_row, nearest_depth);

        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);
            uint8_t colour = reference->palette_index[pixel];

            if (!reference->opaque[pixel] || skull.opaque[pixel]) {
                continue;
            }
            /* Each row covers its share of the floor, so the puddle has no gaps. */
            for (int y = depth; y < depth + (int)depth_per_row; ++y) {
                if (is_blood(colour)) {
                    set_voxel(model, column, y, 0, colour);
                } else {
                    set_voxel(model, column, y, 1, colour);
                    set_voxel(model, column, y, 0, colour);
                }
            }
        }
    }

    /* The skull keeps its drawn, rounded shape, resting on the puddle at its base row's depth. */
    if (best_component > 0) {
        WolfVoxelModel *rounded = wolf_voxel_model_create();
        WolfSingleViewOptions inflate = {WOLF_DEPTH_PROFILE_INFLATE, 1.0, 6.0, 0.0, NULL, false};
        int drop = wolf_voxel_row_for_layer(skull_bottom) - 1;
        int shift = floor_depth_for_row(skull_bottom, bottom_row, depth_per_row, nearest_depth) - WOLF_VOXEL_DIM / 2;

        if (rounded == NULL) {
            return;
        }
        wolf_reconstruct_single_view(&skull, &inflate, rounded);
        for (int z = 0; z < WOLF_VOXEL_DIM; ++z) {
            for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
                for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                    size_t voxel = wolf_voxel_index(x, y, z);

                    if (rounded->occupied[voxel]) {
                        set_voxel(model, x, y + shift, z - drop, rounded->palette_index[voxel]);
                    }
                }
            }
        }
        wolf_voxel_model_destroy(rounded);
    }
}

/* Most common opaque palette entry within a rectangle of the sprite (inclusive), or -1. */
static int dominant_colour_in(const WolfSpriteImage *image, int left, int top, int right, int bottom)
{
    int tally[256] = {0};
    int best = -1;

    for (int row = top; row <= bottom; ++row) {
        for (int column = left; column <= right; ++column) {
            size_t pixel;

            if (row < 0 || column < 0 || row >= WOLF_SPRITE_SIZE || column >= WOLF_SPRITE_SIZE) {
                continue;
            }
            pixel = wolf_sprite_pixel_index(column, row);
            if (image->opaque[pixel]) {
                ++tally[image->palette_index[pixel]];
                if (best < 0 || tally[image->palette_index[pixel]] > tally[best]) {
                    best = image->palette_index[pixel];
                }
            }
        }
    }
    return best;
}

/* The sprite's colour at a column of a row, or the nearest drawn pixel along that row. */
static int colour_along_row(const WolfSpriteImage *image, int column, int row)
{
    for (int distance = 0; distance < WOLF_SPRITE_SIZE; ++distance) {
        int candidates[2] = {column - distance, column + distance};

        for (int pick = 0; pick < 2; ++pick) {
            int candidate = candidates[pick];

            if (candidate >= 0 && candidate < WOLF_SPRITE_SIZE && image->opaque[wolf_sprite_pixel_index(candidate, row)]) {
                return image->palette_index[wolf_sprite_pixel_index(candidate, row)];
            }
        }
    }
    return 0;
}

static bool is_water(int palette_index)
{
    const uint8_t *colour = wolf_game_palette_rgba + (size_t)palette_index * 4u;

    return colour[2] > colour[0] + 40 && colour[2] > colour[1] + 40;
}

/* Well wall height in voxels: sixteen courses of brick, a rim band and the rim top. */
#define WELL_BRICK_LAYERS 16
#define WELL_WALL_THICKNESS 4.0
#define WELL_WATER_LAYER 12

/*
 * A round brick well. The widest row of the sprite gives the radius and
 * centre; the brick rows (below the rim and the opening) are wrapped around
 * the wall by arc length, so courses run evenly all the way round and the
 * back mirrors the front. The top of the wall takes the sprite's light rim.
 * The sprite's opening shows what is inside: water fills the shaft to near
 * the top, otherwise the shaft is open down to a dark floor.
 */
static void build_well(const WolfSpriteImage *reference, WolfVoxelModel *model)
{
    int top_row = -1;
    int bottom_row = -1;
    int widest = 0;
    int widest_left = 0;
    int widest_right = 0;
    double centre;
    double radius;
    int interior;
    int water_row;

    if (reference == NULL) {
        return;
    }
    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        int left = -1;
        int right = -1;

        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            if (reference->opaque[wolf_sprite_pixel_index(column, row)]) {
                left = left < 0 ? column : left;
                right = column;
            }
        }
        if (left < 0) {
            continue;
        }
        top_row = top_row < 0 ? row : top_row;
        bottom_row = row;
        if (right - left + 1 > widest) {
            widest = right - left + 1;
            widest_left = left;
            widest_right = right;
        }
    }
    if (top_row < 0 || bottom_row - top_row < 10) {
        return;
    }
    centre = (double)(widest_left + widest_right + 1) * 0.5;
    radius = (double)widest * 0.5;
    /* The opening is drawn a few rows below the top edge of the rim. */
    water_row = top_row + 5;
    interior = dominant_colour_in(reference, (int)(centre - radius * 0.5), top_row + 4, (int)(centre + radius * 0.5), top_row + 5);
    if (interior < 0) {
        interior = 0;
    }

    for (int z = 0; z <= WELL_BRICK_LAYERS + 1; ++z) {
        /* Bricks from the sprite's wall rows; the rim band and the light rim top above them. */
        int row = z < WELL_BRICK_LAYERS ? bottom_row - 2 - z : (z == WELL_BRICK_LAYERS ? top_row + 2 : top_row + 1);

        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = 0; x < WOLF_VOXEL_DIM; ++x) {
                double dx = (double)x + 0.5 - centre;
                double dy = (double)y + 0.5 - (double)(WOLF_VOXEL_DIM / 2);
                double distance = sqrt(dx * dx + dy * dy);

                if (distance <= radius && distance >= radius - WELL_WALL_THICKNESS) {
                    /* Arc length around the wall: 0 at the front, +-1 at the sides, +-2 at the back. */
                    double around = atan2(dx, -dy) / (3.14159265358979323846 * 0.5);
                    double unfolded = fabs(around) <= 1.0 ? around : (around > 0.0 ? 2.0 - around : -2.0 - around);
                    int column = (int)floor(centre + unfolded * (radius - 0.5));

                    if (column < widest_left) {
                        column = widest_left;
                    }
                    if (column > widest_right) {
                        column = widest_right;
                    }
                    set_voxel(model, x, y, z, colour_along_row(reference, column, row));
                } else if (distance < radius - WELL_WALL_THICKNESS) {
                    if (z == 0) {
                        set_voxel(model, x, y, z, is_water(interior) ? 0 : interior);
                    } else if (is_water(interior) && z <= WELL_WATER_LAYER) {
                        int column = (int)floor((double)x + 0.5);
                        int water = column >= 0 && column < WOLF_SPRITE_SIZE &&
                            reference->opaque[wolf_sprite_pixel_index(column, water_row)] &&
                            is_water(reference->palette_index[wolf_sprite_pixel_index(column, water_row)]) ?
                            reference->palette_index[wolf_sprite_pixel_index(column, water_row)] : interior;

                        set_voxel(model, x, y, z, water);
                    }
                }
            }
        }
    }
}

bool wolf_build_hand_model(WolfHandBuiltModel which, const WolfSpriteImage *reference, WolfVoxelModel *model)
{
    wolf_voxel_model_clear(model);
    switch (which) {
    case WOLF_HAND_BUILT_TABLE_AND_CHAIRS:
        build_table_and_chairs(model);
        return true;
    case WOLF_HAND_BUILT_BED:
        build_bed(model);
        return true;
    case WOLF_HAND_BUILT_FOOD:
        build_food(model);
        return true;
    case WOLF_HAND_BUILT_FIRST_AID:
        build_first_aid(model);
        return true;
    case WOLF_HAND_BUILT_GIBS:
        build_gibs(reference, model);
        return true;
    case WOLF_HAND_BUILT_WELL:
        build_well(reference, model);
        return true;
    case WOLF_HAND_BUILT_NONE:
    default:
        return false;
    }
}
