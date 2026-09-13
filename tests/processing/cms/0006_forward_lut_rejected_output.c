/* SPDX-License-Identifier: MIT */
/* A forward-only profile is useful as input but cannot encode output. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "tests/data/color-numeric/forward-lut.h"

int test_cdef_0006(int argc, char **argv)
{
    static double const input[3] = {.7, .8, .9};
    sixel_color_def_t *definition;
    sixel_color_def_t *source;
    sixel_color_xform_t *transform;
    double xyz[3];
    double rgb[3] = {-9.0, -9.0, -9.0};
    int ok;

    (void)argc;
    (void)argv;
    definition = sixel_cdef_icc(color_forward_lut, sizeof(color_forward_lut));
    source = sixel_cdef_name("srgb");
    transform = sixel_cxform_new(source, definition, 0);
    ok = definition != NULL && transform != NULL &&
        sixel_cdef_to_xyz(definition, input, xyz, 0) &&
        !sixel_cdef_output_ready(definition, 0, 0) &&
        !sixel_cxform_apply(transform, input, rgb) && rgb[0] == -9.0 &&
        rgb[1] == -9.0 && rgb[2] == -9.0;
    sixel_cxform_free(transform);
    sixel_cdef_free(source);
    sixel_cdef_free(definition);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
