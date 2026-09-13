/* SPDX-License-Identifier: MIT */
/* Simple RGB uses a decoding exponent, independent of output selection. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/vectors.h"

int test_cdef_0002(int argc, char **argv)
{
    static double const xy[8] = {
        .3127, .3290, .68, .32, .265, .69, .15, .06
    };
    static double const gamma[3] = {2.0, 2.0, 2.0};
    static double const rgb[3] = {-.5, 1.5, .25};
    sixel_color_def_t *definition;
    double xyz[3];
    double expected;
    int channel;
    int ok;

    (void)argc;
    (void)argv;
    definition = sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_POWER, gamma);
    ok = definition != NULL && sixel_cdef_to_xyz(definition, rgb, xyz, 1);
    if (ok) {
        for (channel = 0; channel < 3; ++channel) {
            /* Squared magnitudes weight independently tabulated primaries. */
            expected = -.25 * color_vectors[0][6 + channel] +
                2.25 * color_vectors[1][6 + channel] +
                .0625 * color_vectors[2][6 + channel];
            if (fabs(xyz[channel] - expected) > 1e-10) {
                ok = 0;
            }
        }
    }
    sixel_cdef_free(definition);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
