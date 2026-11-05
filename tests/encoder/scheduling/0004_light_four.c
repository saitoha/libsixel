/* SPDX-License-Identifier: MIT */
#include "src/encoder-band-schedule.h"

int
test_schedule_0004_light_four(int argc, char **argv)
{
    sixel_band_schedule_t s = {0, 0, 0};
    int i;

    (void)argc;
    (void)argv;
    (void)sixel_band_schedule_observe;
    (void)sixel_band_schedule_span;
    for (i = 0; i < 4; ++i)
        sixel_band_schedule_observe(&s, 0.00032, 16);
    return s.quantum == 4 ? 0 : 1;
}
