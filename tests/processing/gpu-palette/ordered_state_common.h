/* SPDX-License-Identifier: MIT */
#ifndef TEST_ORDERED_STATE_COMMON_H
#define TEST_ORDERED_STATE_COMMON_H

#include "src/gpu-palette.h"

enum { ORDERED_STATE_WIDTH = 17, ORDERED_STATE_HEIGHT = 9,
       ORDERED_STATE_PIXELS = ORDERED_STATE_WIDTH * ORDERED_STATE_HEIGHT };

typedef struct ordered_state_fixture {
    sixel_gpu_palette_request_t request;
    unsigned char palette[256 * 3];
    unsigned char pixels[ORDERED_STATE_PIXELS * 3];
    sixel_index_t output[ORDERED_STATE_PIXELS];
} ordered_state_fixture_t;

void ordered_state_init(ordered_state_fixture_t *fixture,
                        int colors, int seed);
int ordered_state_check(ordered_state_fixture_t *fixture);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
