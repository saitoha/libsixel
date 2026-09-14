/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "assessment/assessment.h"
#include "assessment/color.h"
#include "tests/data/color-numeric/vectors.h"
#include "tests/data/color-numeric/opponents.h"

int
test_qa_0004(int argc, char **argv)
{
    sixel_color_def_t *srgb;
    double clipped[3];
    double xyz[3];
    double a[3];
    double b[3];
    int c;
    int ok;
    (void)argc;
    (void)argv;
    srgb = sixel_cdef_name("srgb");
    /* The independent P3-red linear-sRGB vector leaves the unit cube. */
    for (c = 0; c < 3; ++c) {
        clipped[c] = fmin(1, fmax(0, color_vectors[0][9 + c]));
    }
    ok = sixel_qa_xyz(srgb, SIXEL_COLORSPACE_LINEAR, clipped, xyz);
    sixel_qa_lab(xyz, a);
    sixel_qa_lab(color_vectors[0] + 6, b);
    sixel_cdef_free(srgb);
    return ok && sixel_qa_de00(a, b) > 5.0
        ? EXIT_SUCCESS : EXIT_FAILURE;
}
