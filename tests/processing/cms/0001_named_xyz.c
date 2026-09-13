/* SPDX-License-Identifier: MIT */
/* P3 primaries, signed samples and D50 adaptation are independent. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/vectors.h"

int test_cdef_0001(int argc, char **argv)
{
    sixel_color_def_t *definition;
    double xyz[3];
    size_t i;
    int channel;
    int ok;

    (void)argc;
    (void)argv;
    definition = sixel_cdef_name("display-p3");
    if (definition == NULL) {
        return EXIT_FAILURE;
    }
    ok = 1;
    for (i = 0; i < sizeof(color_vectors) / sizeof(color_vectors[0]); ++i) {
        if (!sixel_cdef_to_xyz(definition, color_vectors[i], xyz, 1)) {
            ok = 0;
            break;
        }
        for (channel = 0; channel < 3; ++channel) {
            if (!isfinite(xyz[channel]) || fabs(xyz[channel] -
                    color_vectors[i][6 + channel]) > 1e-10) {
                ok = 0;
            }
        }
    }
    sixel_cdef_free(definition);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
