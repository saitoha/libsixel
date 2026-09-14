/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "assessment/assessment.h"
#include "assessment/color.h"
#include "tests/data/color-numeric/vectors.h"
#include "tests/data/color-numeric/opponents.h"

int
test_qa_0008(int argc, char **argv)
{
    sixel_frame_t *a;
    sixel_frame_t *b;
    sixel_assessment_t *qa;
    float av[3];
    float bv[3];
    int c;
    int ok;
    (void)argc;
    (void)argv;
    a = NULL;
    b = NULL;
    qa = NULL;
    for (c = 0; c < 3; ++c) {
        av[c] = (float)color_opponents[0][c];
        bv[c] = (float)color_opponents[0][6 + c];
    }
    ok = SIXEL_SUCCEEDED(sixel_frame_new(&a, NULL))
        && SIXEL_SUCCEEDED(sixel_frame_new(&b, NULL))
        && SIXEL_SUCCEEDED(sixel_assessment_new(&qa, NULL));
    if (ok) {
        ok = SIXEL_SUCCEEDED(sixel_frame_init_borrowed(a, av, 1, 1,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, 0))
            && SIXEL_SUCCEEDED(sixel_frame_init_borrowed(b, bv, 1, 1,
            SIXEL_PIXELFORMAT_CIELABFLOAT32, NULL, 0));
    }
    if (ok) {
        sixel_assessment_select_metrics(qa,
            SIXEL_ASSESSMENT_METRIC_MASK(SIXEL_ASSESSMENT_METRIC_DELTA_E00));
        ok = SIXEL_SUCCEEDED(sixel_assessment_analyze(qa, a, b))
            && qa->results[22] < 0.00002
            && av[0] > 1.2f && av[1] < 0;
    }
    sixel_assessment_unref(qa);
    sixel_frame_unref(a);
    sixel_frame_unref(b);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
