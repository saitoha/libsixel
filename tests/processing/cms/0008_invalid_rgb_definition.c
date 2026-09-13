/* SPDX-License-Identifier: MIT */
/* Singular primaries and nonfinite metadata cannot create a definition. */
#include <math.h>
#include <stdlib.h>
#include "src/cms.h"

int test_cdef_0008(int argc, char **argv)
{
    double xy[8] = {.3127, .3290, .68, .32, .68, .32, .15, .06};
    double gamma[3] = {1.0, 1.0, 1.0};
    sixel_color_def_t *definition;

    (void)argc;
    (void)argv;
    definition = sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_POWER, gamma);
    if (definition != NULL) {
        sixel_cdef_free(definition);
        return EXIT_FAILURE;
    }
    xy[4] = .265;
    xy[5] = .69;
    gamma[0] = NAN;
    definition = sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_POWER, gamma);
    if (definition != NULL) {
        sixel_cdef_free(definition);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
