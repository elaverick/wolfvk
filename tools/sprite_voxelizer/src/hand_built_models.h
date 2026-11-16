#ifndef WOLF_SPRITE_VOXELIZER_HAND_BUILT_MODELS_H
#define WOLF_SPRITE_VOXELIZER_HAND_BUILT_MODELS_H

#include "sprite_image.h"
#include "voxel_model.h"

/*
 * Some static objects are drawn in a perspective no depth profile can
 * recover: the table's chairs are in three-quarter view, the bed is seen
 * end-on, and the food, first aid kit and gibs lie on the floor seen from
 * above. These are modelled directly in voxels, using the original sprite
 * only as a reference for proportions and palette entries.
 */
typedef enum WolfHandBuiltModel {
    WOLF_HAND_BUILT_NONE,
    /* SPR_STAT_2: round pedestal table with candles and two ladder-back chairs. */
    WOLF_HAND_BUILT_TABLE_AND_CHAIRS,
    /* SPR_STAT_22: metal-framed bed seen from its foot, with mattress and pillow. */
    WOLF_HAND_BUILT_BED,
    /* SPR_STAT_24: a turkey leg with potato and greens on a round plate. */
    WOLF_HAND_BUILT_FOOD,
    /* SPR_STAT_25: flat white box, blue bottom trim, red cross on the lid. */
    WOLF_HAND_BUILT_FIRST_AID,
    /*
     * SPR_STAT_34: the sprite's skull standing in a puddle of blood with its
     * bones lying around it. Built from the sprite's own pixels: the floor
     * is laid out in perspective (rows higher up the sprite lie further
     * away) so the puddle and bones lie flat.
     */
    WOLF_HAND_BUILT_GIBS,
    /*
     * SPR_STAT_36 and SPR_STAT_37: a round well built from its sprite. The
     * sprite's narrowing bottom is only perspective, so the wall is a hollow
     * cylinder of one radius, four voxels thick, its bricks taken from the
     * sprite and laid evenly around the circumference, a light rim on top,
     * and inside either a dark floor or the sprite's water.
     */
    WOLF_HAND_BUILT_WELL,
    /*
     * SPR_STAT_0 and SPR_STAT_38: a puddle of water or blood one voxel
     * thick. The sprite's rows are laid out on the floor in perspective
     * (rows higher up the sprite lie further away), so its drawn colours
     * become the puddle's surface texture instead of a heap.
     */
    WOLF_HAND_BUILT_PUDDLE,
    /*
     * SPR_STAT_19: a low, jumbled pile of individual bones (shafts with
     * knobbed ends) with a skull (eye sockets, nose and teeth), the bones
     * dropped one by one at random so they rest across each other, in the
     * sprite's own bone greys.
     */
    WOLF_HAND_BUILT_BONE_PILE,
    /*
     * SPR_STAT_31: a small gold-trimmed chest standing on the floor, its
     * front as drawn, open with the lid tipped back and heaped with coins
     * and gems, and a few coins spilled in front.
     */
    WOLF_HAND_BUILT_TREASURE_CHEST,
    /*
     * SPR_STAT_39: a flag hanging limp from its pole. The sprite is turned
     * on the lathe (finial, pole, base and the furled cloth), then the
     * cloth is pleated into vertical folds so it reads as fabric.
     */
    WOLF_HAND_BUILT_FLAG
} WolfHandBuiltModel;

/*
 * Build the requested model into the volume. reference is the shadow-free
 * original sprite, needed by models built from its pixels (gibs) and
 * ignored by the others. Returns false for NONE.
 */
bool wolf_build_hand_model(WolfHandBuiltModel which, const WolfSpriteImage *reference, WolfVoxelModel *model);

#endif
