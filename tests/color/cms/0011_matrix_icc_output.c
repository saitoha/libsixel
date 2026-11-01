/* SPDX-License-Identifier: MIT */
/* A matrix/TRC ICC output must actually encode and evaluate RGB. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/icc.h"
#include "tests/data/color-numeric/vectors.h"

int test_cdef_0005(int argc, char **argv)
{
    sixel_color_def_t *definition;
    double xyz[3];
    double rgb[3];
    size_t i;
    int channel;
    int ok;

    (void)argc;
    (void)argv;
    definition = sixel_cdef_icc(color_icc, sizeof(color_icc));
    ok = definition != NULL && sixel_cdef_output_ready(definition, 1, 1);
    for (i = 0; ok && i < 7; ++i) {
        ok = sixel_cdef_to_xyz(definition, color_vectors[i], xyz, 1) &&
            sixel_cdef_from_xyz(definition, xyz, rgb, 1);
        for (channel = 0; ok && channel < 3; ++channel) {
            if (fabs(rgb[channel] - color_vectors[i][channel]) > 1e-5) {
                ok = 0;
            }
        }
    }
    sixel_cdef_free(definition);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
