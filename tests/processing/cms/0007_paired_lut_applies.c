/* SPDX-License-Identifier: MIT */
/* Independent LUT directions need not be mathematical inverses. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/paired-lut.h"

int test_cdef_0007(int argc, char **argv)
{
    static double const input[3] = {.7, .8, .9};
    sixel_color_def_t *definition;
    double xyz[3];
    double rgb[3];
    int channel;
    int ok;

    (void)argc;
    (void)argv;
    definition = sixel_cdef_icc(color_paired_lut, sizeof(color_paired_lut));
    ok = definition != NULL && sixel_cdef_output_ready(definition, 0, 0) &&
        sixel_cdef_to_xyz(definition, input, xyz, 0) &&
        sixel_cdef_from_xyz(definition, xyz, rgb, 0);
    if (ok) {
        for (channel = 0; channel < 3; ++channel) {
            /* The fixture's u1.15 PCS values and constant device outputs. */
            if (fabs(xyz[channel] - .25 * (channel + 1)) > 1e-12 ||
                fabs(rgb[channel] - .2 * (channel + 1)) > 1e-12) {
                ok = 0;
            }
        }
    }
    sixel_cdef_free(definition);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
