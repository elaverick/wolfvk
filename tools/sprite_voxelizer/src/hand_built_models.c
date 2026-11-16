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
    KIT_TRIM_DARK = 153,
    /* SPR_STAT_19: bone greys, lightest to darkest. */
    BONE_WHITE = 15,
    BONE_LIGHT = 16,
    BONE_PALE = 17,
    BONE_TOP = 18,
    BONE_MID = 19,
    BONE_GREY = 20,
    BONE_SHADE = 22,
    BONE_DARK = 24,
    BONE_DEEP = 25,
    BONE_HOLLOW = 30,
    /* SPR_STAT_31: treasure chest. */
    CHEST_GOLD_BRIGHT = 69,
    CHEST_GOLD_LIGHT = 72,
    CHEST_GOLD = 73,
    CHEST_GOLD_MID = 75,
    CHEST_GOLD_DARK = 77,
    CHEST_GOLD_SHADE = 78,
    CHEST_RED = 39,
    CHEST_RED_DARK = 45,
    GEM_GREEN = 102,
    GEM_GREEN_DARK = 105,
    GEM_PALE = 137,
    GEM_BLUE = 140,
    GEM_BLUE_DARK = 146
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

/* Perceived brightness of a palette entry. */
static int colour_brightness(int palette_index)
{
    const uint8_t *colour = wolf_game_palette_rgba + (size_t)palette_index * 4u;

    return 3 * colour[0] + 6 * colour[1] + colour[2];
}

/* Small deterministic hash, so scattered parts land in the same place on every run. */
static uint32_t scatter_hash(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t value = a * 73856093u ^ b * 19349663u ^ c * 83492791u;

    value ^= value >> 13;
    value *= 0x5bd1e995u;
    value ^= value >> 15;
    return value;
}

/* Uniform value in [0, 1) from the hash. */
static double scatter_unit(uint32_t a, uint32_t b, uint32_t c)
{
    return (double)(scatter_hash(a, b, c) % 10000u) / 10000.0;
}

/* Bounding box of the sprite's drawn pixels; false for an empty sprite. */
static bool drawn_bounds(const WolfSpriteImage *image, int *left, int *top, int *right, int *bottom)
{
    *left = WOLF_SPRITE_SIZE;
    *top = WOLF_SPRITE_SIZE;
    *right = -1;
    *bottom = -1;
    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            if (image->opaque[wolf_sprite_pixel_index(column, row)]) {
                *left = column < *left ? column : *left;
                *right = column > *right ? column : *right;
                *top = row < *top ? row : *top;
                *bottom = row > *bottom ? row : *bottom;
            }
        }
    }
    return *right >= 0;
}

/*
 * A puddle one voxel thick. The sprite's rows are the puddle seen in
 * perspective, nearest row at the bottom, so each row is spread over its
 * share of a floor area 0.55 times as deep as the puddle is wide. Every
 * voxel samples a slightly jittered row and column, which keeps the drawn
 * ripples and shades as the puddle's surface colour and frays the edge
 * instead of leaving straight steps between rows.
 */
static void build_puddle(const WolfSpriteImage *reference, WolfVoxelModel *model)
{
    int left;
    int top;
    int right;
    int bottom;
    int depth;
    int front;
    double depth_per_row;

    if (reference == NULL || !drawn_bounds(reference, &left, &top, &right, &bottom)) {
        return;
    }
    depth = (int)lround(0.55 * (double)(right - left + 1));
    if (depth < bottom - top + 1) {
        depth = bottom - top + 1;
    }
    depth_per_row = (double)depth / (double)(bottom - top + 1);
    front = WOLF_VOXEL_DIM / 2 - depth / 2;

    /* The puddle's shades, darkest first, for rippling the surface. */
    {
        int shades[256] = {0};
        int shade_count = 0;
        int rank_of[256] = {0};
        bool used[256] = {false};

        for (size_t pixel = 0u; pixel < WOLF_SPRITE_PIXELS; ++pixel) {
            if (reference->opaque[pixel]) {
                used[reference->palette_index[pixel]] = true;
            }
        }
        for (int entry = 0; entry < 256; ++entry) {
            if (used[entry]) {
                int slot = shade_count++;

                while (slot > 0 && colour_brightness(shades[slot - 1]) > colour_brightness(entry)) {
                    shades[slot] = shades[slot - 1];
                    --slot;
                }
                shades[slot] = entry;
            }
        }
        for (int rank = 0; rank < shade_count; ++rank) {
            rank_of[shades[rank]] = rank;
        }

        for (int y = front; y < front + depth; ++y) {
            for (int x = left; x <= right; ++x) {
                double along = ((double)(y - front) + 0.5) / depth_per_row;
                int row = bottom - (int)floor(along + 0.9 * (scatter_unit((uint32_t)x, (uint32_t)y, 1u) - 0.5));
                int column = x + (int)(scatter_hash((uint32_t)x, (uint32_t)y, 2u) % 3u) - 1;
                /* A wobbly oval outline, so the puddle isn't a rectangle. */
                double ox = ((double)x + 0.5 - 0.5 * (double)(left + right + 1)) / (0.5 * (double)(right - left + 1));
                double oy = ((double)y + 0.5 - (double)front - 0.5 * (double)depth) / (0.5 * (double)depth);
                double angle = atan2(oy, ox);
                double outline = 1.0 + 0.10 * sin(3.0 * angle + 0.7) + 0.07 * sin(5.0 * angle + 2.1) +
                    0.05 * sin(8.0 * angle + 4.0);
                double ripple;
                int rank;
                size_t pixel;

                if (row < top || row > bottom || column < 0 || column >= WOLF_SPRITE_SIZE ||
                    sqrt(ox * ox + oy * oy) > outline + 0.06 * (scatter_unit((uint32_t)x, (uint32_t)y, 3u) - 0.5)) {
                    continue;
                }
                pixel = wolf_sprite_pixel_index(column, row);
                if (!reference->opaque[pixel]) {
                    continue;
                }
                /* Ripples: shift each voxel a shade or two lighter or darker in soft waves. */
                ripple = 1.1 * sin(0.45 * x + 0.30 * y) + 0.8 * sin(0.62 * y - 0.21 * x + 1.3) +
                    0.9 * (scatter_unit((uint32_t)x, (uint32_t)y, 4u) - 0.5);
                rank = rank_of[reference->palette_index[pixel]] + (int)lround(ripple);
                rank = rank < 0 ? 0 : (rank >= shade_count ? shade_count - 1 : rank);
                set_voxel(model, x, y, 0, shades[rank]);
            }
        }
    }
}

/* One part of the bone pile, in world-aligned offsets from its own origin. */
#define PILE_PART_EXTENT 16
#define PILE_PART_SIZE (2 * PILE_PART_EXTENT + 1)

typedef struct PilePart {
    /* 0 = empty, otherwise palette entry + 1, indexed [z][y][x] with z from 0 up. */
    uint16_t cells[PILE_PART_SIZE][PILE_PART_SIZE][PILE_PART_SIZE];
} PilePart;

static void part_set(PilePart *part, int x, int y, int z, int colour)
{
    if (x >= -PILE_PART_EXTENT && x <= PILE_PART_EXTENT && y >= -PILE_PART_EXTENT && y <= PILE_PART_EXTENT &&
        z >= 0 && z < PILE_PART_SIZE) {
        part->cells[z][y + PILE_PART_EXTENT][x + PILE_PART_EXTENT] = (uint16_t)(colour + 1);
    }
}

/* Squared distance from a point to the segment a-b. */
static double segment_distance_squared(double px, double py, double pz,
    double ax, double ay, double az, double bx, double by, double bz)
{
    double dx = bx - ax;
    double dy = by - ay;
    double dz = bz - az;
    double length_squared = dx * dx + dy * dy + dz * dz;
    double t = length_squared > 0.0 ? ((px - ax) * dx + (py - ay) * dy + (pz - az) * dz) / length_squared : 0.0;
    double cx;
    double cy;
    double cz;

    t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
    cx = ax + t * dx - px;
    cy = ay + t * dy - py;
    cz = az + t * dz - pz;
    return cx * cx + cy * cy + cz * cz;
}

/*
 * A long bone lying almost flat: a round shaft with a pair of knuckles at
 * each end, lit from above (white on top, grey underneath). shade picks a
 * fresher or an older bone.
 */
static void make_bone(PilePart *part, double length, double heading, double tilt, int shade)
{
    /* Bones are a shade greyer than the skulls, so the skulls stand out. */
    static const int tops[2] = {BONE_PALE, BONE_TOP};
    static const int middles[2] = {BONE_MID, BONE_GREY};
    static const int bottoms[2] = {BONE_SHADE, BONE_DEEP};
    double half = 0.5 * length;
    double ux = cos(heading);
    double uy = sin(heading);
    const double base = 1.6;
    double ends[2][3] = {
        {-ux * half, -uy * half, base - tilt * half},
        {ux * half, uy * half, base + tilt * half}
    };

    memset(part, 0, sizeof(*part));
    for (int z = 0; z < 8; ++z) {
        for (int y = -PILE_PART_EXTENT; y <= PILE_PART_EXTENT; ++y) {
            for (int x = -PILE_PART_EXTENT; x <= PILE_PART_EXTENT; ++x) {
                double px = (double)x;
                double py = (double)y;
                double pz = (double)z + 0.5;
                bool inside = segment_distance_squared(px, py, pz, ends[0][0], ends[0][1], ends[0][2],
                    ends[1][0], ends[1][1], ends[1][2]) <= 0.85 * 0.85;
                bool knuckle = false;
                double height;
                int colour;

                /* Two knuckles side by side at each end. */
                for (int end = 0; end < 2; ++end) {
                    for (int side = -1; side <= 1; side += 2) {
                        double kx = ends[end][0] - uy * 0.9 * side - px;
                        double ky = ends[end][1] + ux * 0.9 * side - py;
                        double kz = ends[end][2] - pz;

                        if (kx * kx + ky * ky + kz * kz <= 1.35 * 1.35) {
                            inside = true;
                            knuckle = true;
                        }
                    }
                }
                if (!inside) {
                    continue;
                }
                height = pz - base;
                colour = height > 0.4 ? tops[shade] : (height < -0.6 ? bottoms[shade] : middles[shade]);
                part_set(part, x, y, z, knuckle && colour == middles[shade] ? tops[shade] : colour);
            }
        }
    }
}

/*
 * A skull facing (face_x, face_y): a rounded cranium over a narrower face,
 * dark eye sockets and nose, and a row of teeth along the jaw.
 */
static void make_skull(PilePart *part, double face_x, double face_y)
{
    double length = sqrt(face_x * face_x + face_y * face_y);
    double dx = face_x / length;
    double dy = face_y / length;

    memset(part, 0, sizeof(*part));
    for (int z = 0; z < 10; ++z) {
        for (int y = -6; y <= 6; ++y) {
            for (int x = -6; x <= 6; ++x) {
                /* Local frame: lx across the face, ly from the face toward the back of the head. */
                double lx = (double)x * -dy + (double)y * dx;
                double ly = -((double)x * dx + (double)y * dy);
                double lz = (double)z + 0.5;
                double cranium = (lx / 3.3) * (lx / 3.3) + ((ly - 0.8) / 3.7) * ((ly - 0.8) / 3.7) +
                    ((lz - 4.0) / 3.3) * ((lz - 4.0) / 3.3);
                double face = (lx / 2.5) * (lx / 2.5) + ((ly + 1.6) / 2.0) * ((ly + 1.6) / 2.0) +
                    ((lz - 1.8) / 1.8) * ((lz - 1.8) / 1.8);
                int colour;

                if (cranium > 1.0 && face > 1.0) {
                    continue;
                }
                colour = lz > 5.2 ? BONE_WHITE : (lz > 3.0 ? BONE_LIGHT : BONE_PALE);
                /* Eye sockets and nose on the front of the face. */
                for (int side = -1; side <= 1; side += 2) {
                    double ex = lx - 1.35 * side;
                    double ey = ly + 3.0;
                    double ez = lz - 3.6;

                    if (ex * ex + ey * ey + ez * ez <= 1.3 * 1.3) {
                        colour = BONE_HOLLOW;
                    }
                }
                if (fabs(lx) < 0.7 && ly < -2.2 && lz > 1.6 && lz < 3.0) {
                    colour = BONE_HOLLOW;
                }
                /* Teeth: alternating light and dark along the bottom of the face. */
                if (ly < -2.6 && lz < 1.5) {
                    colour = ((int)floor(lx + 10.0) % 2 == 0) ? BONE_WHITE : BONE_DARK;
                }
                part_set(part, x, y, z, colour);
            }
        }
    }
}

/*
 * Drop a part at (centre_x, centre_y) until it rests on the floor or on
 * the parts already in the pile, then let it settle sink voxels further
 * in among them. Returns false, placing nothing, if it would rest higher
 * than max_height or overhang the tile.
 */
static bool drop_part(WolfVoxelModel *model, int heights[WOLF_VOXEL_DIM][WOLF_VOXEL_DIM], const PilePart *part,
    int centre_x, int centre_y, int max_height, int sink)
{
    int rest = 0;

    for (int y = -PILE_PART_EXTENT; y <= PILE_PART_EXTENT; ++y) {
        for (int x = -PILE_PART_EXTENT; x <= PILE_PART_EXTENT; ++x) {
            int wx = centre_x + x;
            int wy = centre_y + y;

            for (int z = 0; z < PILE_PART_SIZE; ++z) {
                if (part->cells[z][y + PILE_PART_EXTENT][x + PILE_PART_EXTENT] != 0u) {
                    if (wx < 0 || wy < 0 || wx >= WOLF_VOXEL_DIM || wy >= WOLF_VOXEL_DIM) {
                        return false;
                    }
                    /* The lowest cell of each column must sit on top of what is already there. */
                    rest = heights[wy][wx] - z > rest ? heights[wy][wx] - z : rest;
                    break;
                }
            }
        }
    }
    if (rest > max_height) {
        return false;
    }
    /* Settle down into the bones below by up to sink voxels, never through the floor. */
    rest = rest - sink > 0 ? rest - sink : 0;
    for (int z = 0; z < PILE_PART_SIZE; ++z) {
        for (int y = -PILE_PART_EXTENT; y <= PILE_PART_EXTENT; ++y) {
            for (int x = -PILE_PART_EXTENT; x <= PILE_PART_EXTENT; ++x) {
                uint16_t cell = part->cells[z][y + PILE_PART_EXTENT][x + PILE_PART_EXTENT];
                int wx = centre_x + x;
                int wy = centre_y + y;

                if (cell != 0u && rest + z < WOLF_VOXEL_DIM) {
                    set_voxel(model, wx, wy, rest + z, cell - 1);
                    if (rest + z + 1 > heights[wy][wx]) {
                        heights[wy][wx] = rest + z + 1;
                    }
                }
            }
        }
    }
    return true;
}

/*
 * A low, jumbled pile of bones with a skull. Bones of random length are
 * dropped one at a time at random headings and tilts, spread over an oval
 * (the sprite's width, made rounder) and a little more often near the
 * middle, so they come to rest across one another. The pile is kept low:
 * no bone may come to rest more than a third of the sprite's pile
 * height off the floor in the middle, less toward the edge. A single skull
 * then settles in among the bones near the front, turned a little aside.
 */
static void build_bone_pile(const WolfSpriteImage *reference, WolfVoxelModel *model)
{
    static int heights[WOLF_VOXEL_DIM][WOLF_VOXEL_DIM];
    static PilePart part;
    int left = 2;
    int top = 49;
    int right = 62;
    int bottom = 63;
    double radius_x;
    double radius_y;
    double centre_x;
    const double centre_y = 31.5;
    int max_rest;
    int placed = 0;

    if (reference != NULL) {
        (void)drawn_bounds(reference, &left, &top, &right, &bottom);
    }
    radius_x = 0.42 * (double)(right - left + 1);
    radius_y = 0.8 * radius_x;
    centre_x = 0.5 * (double)(left + right);
    max_rest = (bottom - top + 1) / 3;
    memset(heights, 0, sizeof(heights));

    for (int attempt = 0; attempt < 500 && placed < 100; ++attempt) {
        uint32_t seed = (uint32_t)attempt;
        double angle = 6.28318530717958647692 * scatter_unit(seed, 11u, 0u);
        /* The square root spreads bones evenly over the area; every third one is pulled toward the middle. */
        double reach = sqrt(scatter_unit(seed, 12u, 0u)) * (attempt % 3 == 0 ? 0.55 : 0.95);
        int x = (int)lround(centre_x + cos(angle) * reach * (radius_x - 3.0));
        int y = (int)lround(centre_y + sin(angle) * reach * (radius_y - 3.0));
        int limit = (int)lround((double)max_rest * (1.0 - reach * reach * 0.7));

        make_bone(&part, 6.0 + 8.0 * scatter_unit(seed, 13u, 0u), 3.14159265358979 * scatter_unit(seed, 14u, 0u),
            0.5 * (scatter_unit(seed, 15u, 0u) - 0.5), (int)(scatter_hash(seed, 16u, 0u) % 2u));
        if (drop_part(model, heights, &part, x, y, limit, 0)) {
            ++placed;
        }
    }
    /* One skull, settled in among the bones near the front, looking out and a little to the left. */
    make_skull(&part, -0.35, -1.0);
    (void)drop_part(model, heights, &part, (int)lround(centre_x) - 4, (int)lround(centre_y - 0.35 * radius_y),
        max_rest + 4, 1);
}

/*
 * A small open treasure chest standing on the floor. Its front is the
 * sprite's own drawing of the chest's face; the sides and back are
 * gold-framed red panels with a gold band; the lid stands open, tipped
 * back; inside, a heap of gold coins with gems among them rises above the
 * rim, and a few coins have spilled on the floor, as the sprite shows.
 */
static void build_treasure_chest(const WolfSpriteImage *reference, WolfVoxelModel *model)
{
    /* The chest's face in SPR_STAT_31 spans columns 23-38 and rows 54-61. */
    const int x0 = 23;
    const int x1 = 38;
    const int face_top_row = 54;
    const int y0 = 26;
    const int y1 = 36;
    const int height = 8;
    static const int coins[5] = {CHEST_GOLD_BRIGHT, CHEST_GOLD_LIGHT, CHEST_GOLD, CHEST_GOLD_MID, CHEST_GOLD_LIGHT};
    static const int gems[5] = {GEM_GREEN, GEM_GREEN_DARK, GEM_PALE, GEM_BLUE, GEM_BLUE_DARK};
    static const int spilled[][2] = {{18, 22}, {20, 24}, {17, 25}, {21, 21}, {41, 23}, {40, 21}, {19, 19}, {26, 22}};

    for (int z = 0; z < height; ++z) {
        int row = face_top_row + (height - 1 - z);

        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                bool wall = x == x0 || x == x1 || y == y0 || y == y1;
                bool corner = (x == x0 || x == x1) && (y == y0 || y == y1);
                bool frame = corner || (y == y1 && (x == x0 + 1 || x == x1 - 1)) ||
                    ((x == x0 || x == x1) && (y == y0 + 1 || y == y1 - 1));
                int colour;

                if (!wall && z > 0) {
                    continue;
                }
                if (y == y0 && reference != NULL && reference->opaque[wolf_sprite_pixel_index(x, row)]) {
                    colour = reference->palette_index[wolf_sprite_pixel_index(x, row)];
                } else if (z == 0 || z == height - 1 || frame) {
                    colour = z == height - 1 ? CHEST_GOLD_LIGHT : CHEST_GOLD_MID;
                } else if (z == 3) {
                    colour = CHEST_GOLD;
                } else {
                    colour = z < 3 ? CHEST_RED_DARK : CHEST_RED;
                }
                set_voxel(model, x, y, z, colour);
            }
        }
    }

    /* Treasure heaped inside, highest in the middle: coins, with gems near the top. */
    for (int y = y0 + 1; y < y1; ++y) {
        for (int x = x0 + 1; x < x1; ++x) {
            double dx = ((double)x - 0.5 * (double)(x0 + x1)) / (0.5 * (double)(x1 - x0));
            double dy = ((double)y - 0.5 * (double)(y0 + y1)) / (0.5 * (double)(y1 - y0));
            int heap = height - 2 + (int)lround(2.6 * (1.0 - 0.6 * (dx * dx + dy * dy)) +
                0.8 * scatter_unit((uint32_t)x, (uint32_t)y, 21u));

            for (int z = 1; z <= heap; ++z) {
                uint32_t pick = scatter_hash((uint32_t)x, (uint32_t)y, (uint32_t)z + 30u);
                int colour = coins[pick % 5u];

                if (z >= height - 1 && pick % 100u < 14u) {
                    colour = gems[(pick / 100u) % 5u];
                }
                set_voxel(model, x, y, z, colour);
            }
        }
    }

    /* The lid, open and tipped back 20 degrees from upright, hinged on the back top edge. */
    for (int step = 0; step <= y1 - y0; ++step) {
        int z = height + (int)lround((double)step * 0.94);
        int y = y1 + (int)lround((double)step * 0.34);

        for (int x = x0; x <= x1; ++x) {
            bool rim = x == x0 || x == x1 || step == y1 - y0 || step == 0;

            set_voxel(model, x, y, z, rim ? CHEST_GOLD_MID : CHEST_RED);
            set_voxel(model, x, y + 1, z, rim ? CHEST_GOLD_DARK : CHEST_GOLD_SHADE);
        }
    }

    for (size_t coin = 0u; coin < sizeof(spilled) / sizeof(spilled[0]); ++coin) {
        set_voxel(model, spilled[coin][0], spilled[coin][1], 0, coin % 2u == 0u ? CHEST_GOLD_LIGHT : CHEST_GOLD);
    }
}

/* Strongly red, for picking out the flag's cloth. */
static bool is_cloth_red(uint8_t palette_index)
{
    const uint8_t *colour = wolf_game_palette_rgba + (size_t)palette_index * 4u;

    return colour[0] > 120 && colour[1] < 60 && colour[2] < 60;
}

/*
 * A flag hanging on its pole. The whole sprite is turned on the lathe,
 * which gives the finial, pole and base and wraps the cloth round the
 * pole. Then the cloth rows (any row with red in it, wider than the pole)
 * are pleated: voxels further out than a radius that rises and falls
 * seven times around the pole are cut away, leaving vertical folds that
 * drift a little from top to bottom.
 */
static void build_flag(const WolfSpriteImage *reference, WolfVoxelModel *model)
{
    WolfSingleViewOptions lathe = {WOLF_DEPTH_PROFILE_LATHE, 1.0, 32.0, 0.0, NULL, false, false, 0, 0, false};

    if (reference == NULL) {
        return;
    }
    wolf_reconstruct_single_view(reference, &lathe, model);

    for (int row = 0; row < WOLF_SPRITE_SIZE; ++row) {
        int left = -1;
        int right = -1;
        bool red = false;
        double centre;
        double radius;
        int z = wolf_voxel_row_for_layer(row);

        for (int column = 0; column < WOLF_SPRITE_SIZE; ++column) {
            size_t pixel = wolf_sprite_pixel_index(column, row);

            if (reference->opaque[pixel]) {
                left = left < 0 ? column : left;
                right = column;
                red = red || is_cloth_red(reference->palette_index[pixel]);
            }
        }
        if (!red || right - left + 1 < 5) {
            continue;
        }
        centre = 0.5 * (double)(left + right + 1);
        radius = 0.5 * (double)(right - left + 1);
        for (int y = 0; y < WOLF_VOXEL_DIM; ++y) {
            for (int x = left; x <= right; ++x) {
                double dx = (double)x + 0.5 - centre;
                double dy = (double)y + 0.5 - (double)(WOLF_VOXEL_DIM / 2);
                double distance = sqrt(dx * dx + dy * dy);
                double fold = radius * (0.80 + 0.20 * cos(7.0 * atan2(dx, -dy) + 0.06 * (double)row));

                if (distance > 2.0 && distance > fold) {
                    model->occupied[wolf_voxel_index(x, y, z)] = 0u;
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
    case WOLF_HAND_BUILT_PUDDLE:
        build_puddle(reference, model);
        return true;
    case WOLF_HAND_BUILT_BONE_PILE:
        build_bone_pile(reference, model);
        return true;
    case WOLF_HAND_BUILT_TREASURE_CHEST:
        build_treasure_chest(reference, model);
        return true;
    case WOLF_HAND_BUILT_FLAG:
        build_flag(reference, model);
        return true;
    case WOLF_HAND_BUILT_NONE:
    default:
        return false;
    }
}
