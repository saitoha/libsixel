/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "assessment/assessment.h"
#include "assessment/color.h"
#include "tests/data/color-numeric/vectors.h"
#include "tests/data/color-numeric/opponents.h"

int
test_qa_0002(int argc, char **argv)
{
    sixel_color_def_t *srgb;
    sixel_color_def_t *p3;
    double xyz[3];
    double gray[3] = {0.5, 0.5, 0.5};
    double gray_xyz[3];
    /* Independent IEC sRGB decode of 0.5, computed to double precision. */
    double gray_linear[3] = {0.21404114048223255, 0.21404114048223255,
                             0.21404114048223255};
    size_t i;
    int c;
    int space;
    int ok;
    static int const spaces[] = {SIXEL_COLORSPACE_LINEAR,
        SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_CIELAB,
        SIXEL_COLORSPACE_DIN99D};
    (void)argc;
    (void)argv;
    srgb = sixel_cdef_name("srgb");
    p3 = sixel_cdef_name("display-p3");
    ok = srgb != NULL && p3 != NULL;
    for (i = 0; ok && i < sizeof(color_vectors) / sizeof(color_vectors[0]);
            ++i) {
        ok = sixel_cdef_to_xyz(p3, color_vectors[i], xyz, 1);
        for (c = 0; c < 3; ++c) {
            ok &= fabs(xyz[c] - color_vectors[i][6 + c]) < 1e-9;
        }
        for (space = 0; ok && space < 4; ++space) {
            ok = sixel_qa_xyz(srgb, spaces[space],
                               color_opponents[i] + space * 3, xyz);
            for (c = 0; c < 3; ++c) {
                ok &= fabs(xyz[c] - color_vectors[i][6 + c]) < 1e-6;
            }
        }
    }
    ok &= sixel_cdef_to_xyz(p3, gray, gray_xyz, 1);
    ok &= sixel_qa_xyz(srgb, SIXEL_COLORSPACE_GAMMA, gray, xyz);
    for (c = 0; c < 3; ++c) {
        ok &= fabs(xyz[c] - gray_xyz[c]) < 1e-9;
    }
    ok &= sixel_qa_xyz(srgb, SIXEL_COLORSPACE_LINEAR, gray_linear, xyz);
    for (c = 0; c < 3; ++c) {
        ok &= fabs(xyz[c] - gray_xyz[c]) < 1e-9;
    }
    sixel_cdef_free(srgb);
    sixel_cdef_free(p3);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
