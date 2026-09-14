/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "assessment/assessment.h"
#include "assessment/color.h"
#include "tests/data/color-numeric/vectors.h"
#include "tests/data/color-numeric/opponents.h"

int
test_qa_0003(int argc, char **argv)
{
    sixel_color_def_t *srgb;
    sixel_color_def_t *p3;
    double xyz[3];
    double a[3];
    double b[3];
    double red[3] = {1, 0, 0};
    double delta;
    int ok;
    (void)argc;
    (void)argv;
    srgb = sixel_cdef_name("srgb");
    p3 = sixel_cdef_name("display-p3");
    ok = sixel_cdef_to_xyz(srgb, red, xyz, 1);
    sixel_qa_lab(xyz, a);
    ok &= sixel_cdef_to_xyz(p3, red, xyz, 1);
    sixel_qa_lab(xyz, b);
    delta = sixel_qa_de00(a, b);
    sixel_cdef_free(srgb);
    sixel_cdef_free(p3);
    return ok && delta > 5.0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
