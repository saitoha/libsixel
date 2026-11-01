/* SPDX-License-Identifier: MIT */
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "ordered_state_common.h"

void
ordered_state_init(ordered_state_fixture_t *fixture, int colors, int seed)
{
    sixel_gpu_palette_request_t *request;
    int i;
    int c;

    memset(fixture, 0, sizeof(*fixture));
    for (i = 0; i < colors; ++i) {
        for (c = 0; c < 3; ++c) {
            fixture->palette[i * 3 + c] = (unsigned char)
                (i * (37 + c * 12) + seed * (19 + c * 6));
        }
    }
    for (i = 0; i < ORDERED_STATE_PIXELS; ++i) {
        for (c = 0; c < 3; ++c) {
            fixture->pixels[i * 3 + c] = i % 2
                ? fixture->palette[(i % colors) * 3 + c]
                : (unsigned char)(i * (13 + c * 8) + seed);
        }
    }
    request = &fixture->request;
    request->policy = SIXEL_GPU_POLICY_FORCE;
    request->dest = fixture->output;
    request->pixels = fixture->pixels;
    request->pixel_count = ORDERED_STATE_PIXELS;
    request->width = ORDERED_STATE_WIDTH;
    request->height = ORDERED_STATE_HEIGHT;
    request->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request->palette = fixture->palette;
    request->palette_size = (size_t)colors * 3U;
    request->palette_depth = 3;
    request->ncolors = colors;
    request->lut_policy = SIXEL_LUT_POLICY_ORDERED;
    request->method_for_diffuse = SIXEL_DIFFUSE_NONE;
    request->method_for_scan = SIXEL_SCAN_RASTER;
}

/* A local integer scan is independent of every production lookup backend.
 * Original palette order makes the tie expectation explicit. */
int
ordered_state_check(ordered_state_fixture_t *fixture)
{
    int q;
    int i;
    int c;
    int delta;
    int distance;
    int minimum;
    int expected;

    if (sixel_gpu_palette_apply(&fixture->request) != SIXEL_OK) {
        fprintf(stderr, "ordered state GPU dispatch failed\n");
        return 0;
    }
    for (q = 0; q < ORDERED_STATE_PIXELS; ++q) {
        minimum = INT_MAX;
        expected = 0;
        for (i = 0; i < fixture->request.ncolors; ++i) {
            distance = 0;
            for (c = 0; c < 3; ++c) {
                delta = (int)fixture->pixels[q * 3 + c]
                    - fixture->palette[i * 3 + c];
                distance += delta * delta;
            }
            if (distance < minimum) {
                minimum = distance;
                expected = i;
            }
        }
        if (fixture->output[q] != expected) {
            fprintf(stderr, "ordered state pixel=%d got=%d expected=%d\n",
                    q, (int)fixture->output[q], expected);
            return 0;
        }
    }
    return 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
