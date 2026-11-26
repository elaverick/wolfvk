#include "wolfvk/simulation_clock.h"

#include <math.h>

#define WOLF_SIMULATION_MAX_BACKLOG_SECONDS \
    (WOLF_SIMULATION_TIC_SECONDS * (double)WOLF_SIMULATION_MAX_TICS_PER_FRAME)

/* Keep the clock independent of SDL so fixed-tic behaviour is easy to test. */
void wolf_simulation_clock_reset(WolfSimulationClock *clock)
{
    if (clock != NULL) {
        clock->accumulated_seconds = 0.0;
    }
}

uint32_t wolf_simulation_clock_advance(WolfSimulationClock *clock, double delta_seconds)
{
    uint32_t available_tics;

    if (clock == NULL || !isfinite(delta_seconds) || delta_seconds < 0.0 ||
        !isfinite(clock->accumulated_seconds) || clock->accumulated_seconds < 0.0) {
        return 0u;
    }

    clock->accumulated_seconds += delta_seconds;
    if (clock->accumulated_seconds > WOLF_SIMULATION_MAX_BACKLOG_SECONDS) {
        clock->accumulated_seconds = WOLF_SIMULATION_MAX_BACKLOG_SECONDS;
    }

    available_tics = (uint32_t)floor(
        (clock->accumulated_seconds + WOLF_SIMULATION_TIC_SECONDS * 1e-9) /
        WOLF_SIMULATION_TIC_SECONDS);
    if (available_tics > WOLF_SIMULATION_MAX_TICS_PER_FRAME) {
        available_tics = WOLF_SIMULATION_MAX_TICS_PER_FRAME;
    }

    clock->accumulated_seconds -= (double)available_tics * WOLF_SIMULATION_TIC_SECONDS;
    if (clock->accumulated_seconds < 0.0) {
        clock->accumulated_seconds = 0.0;
    }

    return available_tics;
}
