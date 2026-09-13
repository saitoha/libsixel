/* SPDX-License-Identifier: MIT */
/* Source and destination transforms own independent immutable copies. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/colorspace.h"
#include "tests/data/color-numeric/vectors.h"

int test_cdef_0004(int argc, char **argv)
{
    sixel_color_def_t *source;
    sixel_color_def_t *destination;
    sixel_color_xform_t *transform;
    double rgb[3];
    int channel;
    int ok;

    (void)argc;
    (void)argv;
    source = sixel_cdef_name("display-p3");
    destination = sixel_cdef_name("srgb");
    transform = sixel_cxform_new(source, destination, 1);
    sixel_cdef_free(source);
    sixel_cdef_free(destination);
    ok = transform != NULL &&
        sixel_cxform_apply(transform, color_vectors[0], rgb);
    if (ok) {
        for (channel = 0; channel < 3; ++channel) {
            if (fabs(sixel_color_decode_srgb(rgb[channel]) -
                     color_vectors[0][9 + channel]) > 1e-10) {
                ok = 0;
            }
        }
    }
    sixel_cxform_free(transform);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
