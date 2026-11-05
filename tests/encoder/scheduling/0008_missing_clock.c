/* SPDX-License-Identifier: MIT */
#include "src/encoder-band-schedule.h"

int
test_schedule_0008_missing_clock(int argc, char **argv)
{
    sixel_band_schedule_t s = {0, 0, 0};

    (void)argc;
    (void)argv;
    (void)sixel_band_schedule_observe;
    (void)sixel_band_schedule_span;
    s.quantum = 4;
    sixel_band_schedule_observe(&s, 0.0, 16);
    return s.quantum == 1 && s.seconds_per_band == 0.0 ? 0 : 1;
}
