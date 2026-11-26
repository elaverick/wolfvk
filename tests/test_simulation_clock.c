#include "unity.h"

#include "wolfvk/simulation_clock.h"

#include <math.h>

void test_simulation_clock_emits_one_tick_per_seventieth_second(void)
{
    WolfSimulationClock clock = {0.0};

    TEST_ASSERT_EQUAL_UINT32(0u, wolf_simulation_clock_advance(&clock, 0.5 * WOLF_SIMULATION_TIC_SECONDS));
    TEST_ASSERT_EQUAL_UINT32(1u, wolf_simulation_clock_advance(&clock, 0.5 * WOLF_SIMULATION_TIC_SECONDS));
    TEST_ASSERT_EQUAL_UINT32(0u, wolf_simulation_clock_advance(&clock, 0.25 * WOLF_SIMULATION_TIC_SECONDS));
    TEST_ASSERT_EQUAL_UINT32(1u, wolf_simulation_clock_advance(&clock, 0.75 * WOLF_SIMULATION_TIC_SECONDS));
}

void test_simulation_clock_is_independent_of_frame_partitioning(void)
{
    WolfSimulationClock steady = {0.0};
    WolfSimulationClock irregular = {0.0};
    uint32_t steady_tics = 0u;
    uint32_t irregular_tics = 0u;

    for (uint32_t frame = 0u; frame < 70u; ++frame) {
        steady_tics += wolf_simulation_clock_advance(&steady, 1.0 / 70.0);
    }
    irregular_tics += wolf_simulation_clock_advance(&irregular, 0.004);
    irregular_tics += wolf_simulation_clock_advance(&irregular, 0.021);
    irregular_tics += wolf_simulation_clock_advance(&irregular, 0.008);
    irregular_tics += wolf_simulation_clock_advance(&irregular, 0.037);
    irregular_tics += wolf_simulation_clock_advance(&irregular, 0.030);

    TEST_ASSERT_EQUAL_UINT32(70u, steady_tics);
    TEST_ASSERT_EQUAL_UINT32(7u, irregular_tics);
    TEST_ASSERT_FLOAT_WITHIN(1e-8, 0.0, steady.accumulated_seconds);
    TEST_ASSERT_FLOAT_WITHIN(1e-8, 0.0, irregular.accumulated_seconds);
}

void test_simulation_clock_caps_catch_up_after_a_long_frame(void)
{
    WolfSimulationClock clock = {0.0};

    TEST_ASSERT_EQUAL_UINT32(
        WOLF_SIMULATION_MAX_TICS_PER_FRAME,
        wolf_simulation_clock_advance(&clock, 10.0));
    TEST_ASSERT_FLOAT_WITHIN(
        1e-8, 0.0, clock.accumulated_seconds);
}

void test_simulation_clock_retains_fractional_time_between_frames(void)
{
    WolfSimulationClock clock = {0.0};

    TEST_ASSERT_EQUAL_UINT32(2u, wolf_simulation_clock_advance(&clock, 2.5 / 70.0));
    TEST_ASSERT_FLOAT_WITHIN(1e-8, 0.5 / 70.0, clock.accumulated_seconds);
    TEST_ASSERT_EQUAL_UINT32(1u, wolf_simulation_clock_advance(&clock, 0.5 / 70.0));
    TEST_ASSERT_FLOAT_WITHIN(1e-8, 0.0, clock.accumulated_seconds);
}

void test_simulation_clock_rejects_invalid_time_and_arguments(void)
{
    WolfSimulationClock clock = {0.0};

    TEST_ASSERT_EQUAL_UINT32(0u, wolf_simulation_clock_advance(NULL, 1.0));
    TEST_ASSERT_EQUAL_UINT32(0u, wolf_simulation_clock_advance(&clock, -1.0));
    TEST_ASSERT_EQUAL_UINT32(0u, wolf_simulation_clock_advance(&clock, NAN));
    TEST_ASSERT_EQUAL_UINT32(0u, wolf_simulation_clock_advance(&clock, INFINITY));
    TEST_ASSERT_FLOAT_WITHIN(1e-8, 0.0, clock.accumulated_seconds);
}

void test_simulation_clock_reset_discards_fractional_time(void)
{
    WolfSimulationClock clock = {0.0};

    TEST_ASSERT_EQUAL_UINT32(0u, wolf_simulation_clock_advance(&clock, 0.5 / 70.0));
    wolf_simulation_clock_reset(&clock);
    TEST_ASSERT_FLOAT_WITHIN(1e-8, 0.0, clock.accumulated_seconds);
    TEST_ASSERT_EQUAL_UINT32(0u, wolf_simulation_clock_advance(&clock, 0.5 / 70.0));
}
