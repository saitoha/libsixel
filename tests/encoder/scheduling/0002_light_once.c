/* SPDX-License-Identifier: MIT */
#include "src/encoder-band-schedule.h"

int
test_schedule_0002_light_once(int argc, char **argv)
{
    sixel_band_schedule_t s = {0, 0, 0};

    (void)argc;
    (void)argv;
    (void)sixel_band_schedule_observe;
    (void)sixel_band_schedule_span;
    sixel_band_schedule_observe(&s, 0.00032, 16);
    return s.quantum == 1 ? 0 : 1;
}
