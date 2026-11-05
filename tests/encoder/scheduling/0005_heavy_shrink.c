/* SPDX-License-Identifier: MIT */
#include "src/encoder-band-schedule.h"

int
test_schedule_0005_heavy_shrink(int argc, char **argv)
{
    sixel_band_schedule_t s = {0, 0, 0};

    (void)argc;
    (void)argv;
    (void)sixel_band_schedule_observe;
    (void)sixel_band_schedule_span;
    s.quantum = 4;
    s.seconds_per_band = 0.00002;
    sixel_band_schedule_observe(&s, 0.0048, 16);
    return s.quantum == 1 ? 0 : 1;
}
