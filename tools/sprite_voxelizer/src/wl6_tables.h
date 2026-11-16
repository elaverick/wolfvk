#ifndef WOLF_SPRITE_VOXELIZER_WL6_TABLES_H
#define WOLF_SPRITE_VOXELIZER_WL6_TABLES_H

#include <stdint.h>

/* Number of sprite pages in retail WL6 VSWAP data (the original SPR_ enum). */
#define WOLF_WL6_SPRITE_COUNT 436
/* Number of actor states defined by WL_ACT2.C in the retail WL6 build. */
#define WOLF_WL6_STATE_COUNT 303

/*
 * One entry of the original statetype tables. The original rotate field is
 * 0 for a camera-facing frame, 1 for an eight-way rotation set beginning at
 * the sprite, and 2 for the two-way pain-frame layout.
 */
typedef struct WolfStateDefinition {
    const char *name;
    int32_t rotate;
    int32_t sprite_index;
    int32_t tics;
    int32_t next_state_index;
} WolfStateDefinition;

extern const char *const wolf_wl6_sprite_names[WOLF_WL6_SPRITE_COUNT];
extern const WolfStateDefinition wolf_wl6_states[WOLF_WL6_STATE_COUNT];

#endif
