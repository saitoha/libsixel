/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Solid-region detection has to find a thin element in EVERY row phase.
 *
 * The palette sample is a grid pick, so whether a thin element is in it
 * depends on where the grid falls.  Measured on 1472x760 at -p 64, where the
 * stride is 17, a 3-pixel bar landed in the sample in 3 of 17 phases -- and in
 * motion the phase changes every frame, so the palette gained and lost the
 * color from frame to frame.  That is the flicker.  Anything less than every
 * phase here is still flicker, so the assertion is all-or-nothing rather than
 * a rate.
 *
 * The other half is the discrimination a mass threshold could not make: a
 * 3-pixel bar covers little more area than a cluster of noise, so counting
 * pixels cannot separate them.  Shape can -- the bar is flat along its length.
 * The test therefore also requires that a noisy patch and a hot pixel are
 * never nominated, since a rule that accepts everything would pass the phase
 * assertion trivially.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <6cells.h>
#include <sixel.h>

#include "src/filter-sample.h"

#define SOLID_WIDTH 320
#define SOLID_HEIGHT 240

/* thin horizontal element: a scrub bar */
#define SOLID_HBAR_R 235
#define SOLID_HBAR_G 25
#define SOLID_HBAR_B 30

/* thin vertical element: a scrollbar */
#define SOLID_VBAR_R 40
#define SOLID_VBAR_G 180
#define SOLID_VBAR_B 90

/* a single hot pixel, as a sensor or a compression artifact leaves */
#define SOLID_HOT_R 250
#define SOLID_HOT_G 8
#define SOLID_HOT_B 240

static void
solid_fill(unsigned char *pixels, int bar_y, int bar_x)
{
    int x;
    int y;
    size_t offset;

    for (y = 0; y < SOLID_HEIGHT; ++y) {
        for (x = 0; x < SOLID_WIDTH; ++x) {
            int value;

            offset = ((size_t)y * SOLID_WIDTH + (size_t)x) * 3u;
            /* A gently varying background: never flat, never noisy. */
            value = 30 + ((x / 17 + y / 13) % 11);
            pixels[offset] = (unsigned char)value;
            pixels[offset + 1u] = (unsigned char)(value + 2);
            pixels[offset + 2u] = (unsigned char)(value + 6);
        }
    }
    /* A noisy patch: high gradient on both axes, must never be nominated. */
    for (y = 40; y < 90; ++y) {
        for (x = 40; x < 140; ++x) {
            int n;

            offset = ((size_t)y * SOLID_WIDTH + (size_t)x) * 3u;
            n = (x * 37 + y * 61) % 97;
            pixels[offset] = (unsigned char)(120 + n);
            pixels[offset + 1u] = (unsigned char)(40 + n / 2);
            pixels[offset + 2u] = (unsigned char)(200 - n / 3);
        }
    }
    offset = ((size_t)(SOLID_HEIGHT / 2) * SOLID_WIDTH
              + (size_t)(SOLID_WIDTH / 2)) * 3u;
    pixels[offset] = SOLID_HOT_R;
    pixels[offset + 1u] = SOLID_HOT_G;
    pixels[offset + 2u] = SOLID_HOT_B;

    for (y = bar_y; y < bar_y + 3; ++y) {
        for (x = 20; x < SOLID_WIDTH - 20; ++x) {
            offset = ((size_t)y * SOLID_WIDTH + (size_t)x) * 3u;
            pixels[offset] = SOLID_HBAR_R;
            pixels[offset + 1u] = SOLID_HBAR_G;
            pixels[offset + 2u] = SOLID_HBAR_B;
        }
    }
    for (y = 20; y < SOLID_HEIGHT - 20; ++y) {
        for (x = bar_x; x < bar_x + 3; ++x) {
            offset = ((size_t)y * SOLID_WIDTH + (size_t)x) * 3u;
            pixels[offset] = SOLID_VBAR_R;
            pixels[offset + 1u] = SOLID_VBAR_G;
            pixels[offset + 2u] = SOLID_VBAR_B;
        }
    }
}

static int
solid_contains(unsigned char const *colors,
               unsigned int count,
               int r,
               int g,
               int b)
{
    unsigned int index;

    for (index = 0u; index < count; ++index) {
        int dr;
        int dg;
        int db;

        dr = (int)colors[index * 3u] - r;
        dg = (int)colors[index * 3u + 1u] - g;
        db = (int)colors[index * 3u + 2u] - b;
        if (dr * dr + dg * dg + db * db <= 8 * 8 * 3) {
            return 1;
        }
    }

    return 0;
}

int
test_filter_0014_filter_sample_solid(int argc, char **argv)
{
    unsigned char *pixels;
    unsigned char colors[SIXEL_SAMPLE_SOLID_MAX * 3u];
    int phase;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    pixels = (unsigned char *)malloc((size_t)SOLID_WIDTH * SOLID_HEIGHT * 3u);
    if (pixels == NULL) {
        return EXIT_FAILURE;
    }

    /*
     * Every phase, not most of them.  The scan stride must not exceed the
     * thinnest element worth protecting: a 3-pixel bar is guaranteed to hold
     * a scanned row at stride 3, and at stride 4 detection measured 13 of 17.
     */
    for (phase = 0; phase < 17; ++phase) {
        unsigned int count;

        solid_fill(pixels, 150 + phase, 250 + (phase % 5));
        count = sixel_filter_sample_solid_colors(pixels,
                                                 SOLID_WIDTH,
                                                 SOLID_HEIGHT,
                                                 3,
                                                 NULL,
                                                 0,
                                                 0,
                                                 SOLID_WIDTH,
                                                 SOLID_HEIGHT,
                                                 colors,
                                                 SIXEL_SAMPLE_SOLID_MAX);
        if (!solid_contains(colors, count,
                            SOLID_HBAR_R, SOLID_HBAR_G, SOLID_HBAR_B)) {
            fprintf(stderr,
                    "phase %d: the horizontal bar was not found; a thin "
                    "element that is only found in some phases is exactly "
                    "the frame-to-frame flicker this exists to remove\n",
                    phase);
            goto end;
        }
        if (!solid_contains(colors, count,
                            SOLID_VBAR_R, SOLID_VBAR_G, SOLID_VBAR_B)) {
            fprintf(stderr,
                    "phase %d: the vertical bar was not found; the flatness "
                    "test has to accept either axis, or every scrollbar and "
                    "window edge is discarded\n",
                    phase);
            goto end;
        }
        /*
         * The negative control.  A line kernel is what makes the bar visible,
         * and it would be worthless if it also accepted noise -- a square
         * kernel rejects both, which is why it cannot be used here.
         */
        if (solid_contains(colors, count,
                           SOLID_HOT_R, SOLID_HOT_G, SOLID_HOT_B)) {
            fprintf(stderr,
                    "phase %d: a single hot pixel was nominated as solid\n",
                    phase);
            goto end;
        }
        if (count == 0u) {
            fprintf(stderr, "phase %d: nothing found at all\n", phase);
            goto end;
        }
    }

    /* Nothing from the noisy patch, whose colors all lie above 120 in red. */
    {
        unsigned int count;
        unsigned int index;

        solid_fill(pixels, 150, 250);
        count = sixel_filter_sample_solid_colors(pixels,
                                                 SOLID_WIDTH,
                                                 SOLID_HEIGHT,
                                                 3,
                                                 NULL,
                                                 0,
                                                 0,
                                                 SOLID_WIDTH,
                                                 SOLID_HEIGHT,
                                                 colors,
                                                 SIXEL_SAMPLE_SOLID_MAX);
        for (index = 0u; index < count; ++index) {
            unsigned char const *c;

            c = colors + (size_t)index * 3u;
            if (c[0] >= 120u && c[0] <= 217u && c[2] >= 168u) {
                fprintf(stderr,
                        "a colour from the noisy patch was nominated: "
                        "(%u,%u,%u)\n",
                        (unsigned int)c[0],
                        (unsigned int)c[1],
                        (unsigned int)c[2]);
                goto end;
            }
        }
    }

    /* Formats the reader cannot handle must decline rather than guess. */
    {
        if (sixel_filter_sample_solid_colors(pixels, SOLID_WIDTH,
                                             SOLID_HEIGHT, 1, NULL,
                                             0, 0, SOLID_WIDTH, SOLID_HEIGHT,
                                             colors,
                                             SIXEL_SAMPLE_SOLID_MAX) != 0u) {
            fprintf(stderr, "a depth of 1 should collect nothing\n");
            goto end;
        }
    }
    ok = 1;

end:
    free(pixels);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
