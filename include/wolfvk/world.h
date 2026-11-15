#ifndef WOLFVK_WORLD_H
#define WOLFVK_WORLD_H

/*
 * A map cell and its wall panel share one world-space dimension. Keeping the
 * scale here prevents the renderer, camera and collision code from drifting
 * into incompatible interpretations of the original square wall tiles.
 */
#define WOLF_WORLD_TILE_SIZE 3.2f
#define WOLF_WORLD_WALL_HEIGHT WOLF_WORLD_TILE_SIZE
#define WOLF_WORLD_CEILING_HEIGHT WOLF_WORLD_WALL_HEIGHT
#define WOLF_WORLD_EYE_HEIGHT (WOLF_WORLD_WALL_HEIGHT * 0.5f)

#endif
