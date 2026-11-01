/* SPDX-License-Identifier: MIT */
/* Float rounding at each hub must retain all 4913 sampled SDR P3 colors. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "src/cms.h"
#include "src/colorspace.h"

int test_cdef_0010(int argc, char **argv)
{
    static double const xy[8] = {
        .3127, .3290, .64, .33, .30, .60, .15, .06
    };
    static double const gamma[3] = {1.0, 1.0, 1.0};
    static int const spaces[3] = {
        SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_CIELAB,
        SIXEL_COLORSPACE_DIN99D
    };
    sixel_color_def_t *p3;
    sixel_color_def_t *linear;
    sixel_color_xform_t *forward;
    sixel_color_xform_t *inverse;
    double input[3];
    double work[3];
    double opponent[3];
    double output[3];
    double error;
    double maximum;
    int r;
    int g;
    int b;
    int space;
    int channel;
    int ok;

    (void)argc;
    (void)argv;
    maximum = 0.0;
    p3 = sixel_cdef_name("display-p3");
    linear = sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_POWER, gamma);
    forward = sixel_cxform_new(p3, linear, 1);
    inverse = sixel_cxform_new(linear, p3, 1);
    sixel_cdef_free(p3);
    sixel_cdef_free(linear);
    ok = forward != NULL && inverse != NULL;
    for (r = 0; ok && r <= 16; ++r) {
        for (g = 0; ok && g <= 16; ++g) {
            for (b = 0; ok && b <= 16; ++b) {
                input[0] = (double)r / 16.0;
                input[1] = (double)g / 16.0;
                input[2] = (double)b / 16.0;
                for (space = 0; ok && space < 3; ++space) {
                    ok = sixel_cxform_apply(forward, input, work);
                    if (!ok) {
                        break;
                    }
                    for (channel = 0; channel < 3; ++channel) {
                        work[channel] = (double)(float)work[channel];
                    }
                    ok = ok && sixel_color_convert(opponent, work,
                        SIXEL_COLORSPACE_LINEAR, spaces[space]);
                    if (!ok) {
                        break;
                    }
                    for (channel = 0; channel < 3; ++channel) {
                        opponent[channel] = (double)(float)opponent[channel];
                    }
                    ok = ok && sixel_color_convert(work, opponent,
                        spaces[space], SIXEL_COLORSPACE_LINEAR);
                    if (!ok) {
                        break;
                    }
                    for (channel = 0; channel < 3; ++channel) {
                        work[channel] = (double)(float)work[channel];
                    }
                    ok = ok && sixel_cxform_apply(inverse, work, output);
                    for (channel = 0; ok && channel < 3; ++channel) {
                        error = fabs(sixel_color_decode_srgb(output[channel])
                            - sixel_color_decode_srgb(input[channel]));
                        if (!isfinite(error) || error > 1e-5) {
                            ok = 0;
                        }
                        if (error > maximum) {
                            maximum = error;
                        }
                    }
                }
            }
        }
    }
    fprintf(stderr, "P3 float roundtrip max linear error %.12g\n", maximum);
    sixel_cxform_free(inverse);
    sixel_cxform_free(forward);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
