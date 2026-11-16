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
    WOLF_HAND_BUILT_GIBS
} WolfHandBuiltModel;

/*
 * Build the requested model into the volume. reference is the shadow-free
 * original sprite, needed by models built from its pixels (gibs) and
 * ignored by the others. Returns false for NONE.
 */
bool wolf_build_hand_model(WolfHandBuiltModel which, const WolfSpriteImage *reference, WolfVoxelModel *model);

#endif
