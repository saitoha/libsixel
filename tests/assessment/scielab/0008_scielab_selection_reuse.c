/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include "assessment/assessment.h"

int
test_qa_0033(int argc, char **argv)
{
    float av[3] = {0, 0, 0};
    float bv[3] = {1, 1, 1};
    sixel_frame_t *a;
    sixel_frame_t *b;
    sixel_assessment_t *qa;
    unsigned int mask;
    int index;
    int ok;

    (void)argc;
    (void)argv;
    a = NULL;
    b = NULL;
    qa = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_frame_new(&a, NULL))
            || SIXEL_FAILED(sixel_frame_new(&b, NULL))
            || SIXEL_FAILED(sixel_assessment_new(&qa, NULL))
            || SIXEL_FAILED(sixel_frame_init_borrowed(a, av, 1, 1,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, 0))
            || SIXEL_FAILED(sixel_frame_init_borrowed(b, bv, 1, 1,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, 0))) {
        goto end;
    }
    mask = SIXEL_ASSESSMENT_METRIC_MASK(SIXEL_ASSESSMENT_METRIC_SCIELAB_MAX);
    index = SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_SCIELAB_MAX);
    sixel_assessment_select_metrics(qa, mask);
    if (SIXEL_FAILED(sixel_assessment_analyze(qa, a, b))
            || qa->results_valid_mask != mask
            || !isfinite(qa->results[index]) || qa->results[index] < 99) {
        goto end;
    }
    qa->scielab_ppd = NAN;
    if (sixel_assessment_analyze(qa, a, b) != SIXEL_BAD_ARGUMENT
            || qa->results_ready || qa->results_valid_mask != 0) {
        goto end;
    }
    qa->scielab_ppd = 60;
    ok = SIXEL_SUCCEEDED(sixel_assessment_analyze(qa, a, a))
        && qa->results_ready && qa->results_valid_mask == mask
        && qa->results[index] == 0;
end:
    sixel_assessment_unref(qa);
    sixel_frame_unref(a);
    sixel_frame_unref(b);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
