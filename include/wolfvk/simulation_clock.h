#ifndef WOLFVK_SIMULATION_CLOCK_H
#define WOLFVK_SIMULATION_CLOCK_H

#include <stdint.h>

/* Wolfenstein's actor and world simulation advances at 70 game tics per second. */
#define WOLF_SIMULATION_TICS_PER_SECOND 70u
#define WOLF_SIMULATION_TIC_SECONDS (1.0 / (double)WOLF_SIMULATION_TICS_PER_SECOND)

/* Bound catch-up work after a long frame or debugger pause. */
#define WOLF_SIMULATION_MAX_TICS_PER_FRAME 8u

typedef struct WolfSimulationClock {
    double accumulated_seconds;
} WolfSimulationClock;

/* Reset accumulated time; call when starting or deliberately restarting simulation. */
void wolf_simulation_clock_reset(WolfSimulationClock *clock);

/*
 * Add elapsed wall time and return the number of complete fixed simulation
 * tics available this frame. Excess backlog is discarded rather than causing
 * an unbounded catch-up spiral. Rendering remains free to run at any rate.
 */
uint32_t wolf_simulation_clock_advance(WolfSimulationClock *clock, double delta_seconds);

#endif
