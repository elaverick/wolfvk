#ifndef WOLF_SPRITE_VOXELIZER_HAND_BUILT_MODELS_H
#define WOLF_SPRITE_VOXELIZER_HAND_BUILT_MODELS_H

#include "voxel_model.h"

/*
 * Some static objects are furniture drawn in a perspective no depth profile
 * can recover: the table's chairs are drawn in three-quarter view and the
 * bed is seen end-on. These are modelled directly in voxels, using the
 * original sprite only as a reference for proportions and palette entries.
 */
typedef enum WolfHandBuiltModel {
    WOLF_HAND_BUILT_NONE,
    /* SPR_STAT_2: round pedestal table with candles and two ladder-back chairs. */
    WOLF_HAND_BUILT_TABLE_AND_CHAIRS,
    /* SPR_STAT_22: metal-framed bed seen from its foot, with mattress and pillow. */
    WOLF_HAND_BUILT_BED
} WolfHandBuiltModel;

/* Build the requested model into an empty volume. Returns false for NONE. */
bool wolf_build_hand_model(WolfHandBuiltModel which, WolfVoxelModel *model);

#endif
