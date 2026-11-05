/* SPDX-License-Identifier: MIT */
#include "src/encoder-band-schedule.h"

int
test_schedule_0015_tail_one(int argc, char **argv)
{
    sixel_band_schedule_t s = {0, 0, 0};

    (void)argc;
    (void)argv;
    (void)sixel_band_schedule_observe;
    (void)sixel_band_schedule_span;
    s.quantum = 4;
    return sixel_band_schedule_span(&s, 3, 3, 2, 8) == 1 ? 0 : 1;
}
