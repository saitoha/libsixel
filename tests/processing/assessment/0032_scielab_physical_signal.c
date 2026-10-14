/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include "assessment/assessment.h"
#include "assessment/scielab.h"
#include "assessment/color.h"

int
test_qa_0032(int argc, char **argv)
{
    float av[6] = {1.125f, -0.125f, 0.5f, .25f, .5f, .75f};
    float bv[6] = {.25f, .5f, .75f, 1.125f, -0.125f, 0.5f};
    double axyz[6];
    double bxyz[6];
    double sample[3];
    double expected;
    double maximum;
    sixel_color_def_t *srgb;
    sixel_frame_t *a;
    sixel_frame_t *b;
    sixel_assessment_t *qa;
    unsigned int mask;
    int index;
    int p;
    int c;
    int ok;

    (void)argc;
    (void)argv;
    a = NULL;
    b = NULL;
    qa = NULL;
    srgb = sixel_cdef_name("srgb");
    ok = 0;
    if (srgb == NULL || SIXEL_FAILED(sixel_frame_new(&a, NULL))
            || SIXEL_FAILED(sixel_frame_new(&b, NULL))
            || SIXEL_FAILED(sixel_assessment_new(&qa, NULL))) {
        goto end;
    }
    for (p = 0; p < 2; ++p) {
        for (c = 0; c < 3; ++c) {
            sample[c] = av[3 * p + c];
        }
        if (!sixel_qa_xyz(srgb, SIXEL_COLORSPACE_LINEAR,
                sample, axyz + 3 * p)) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            sample[c] = bv[3 * p + c];
        }
        if (!sixel_qa_xyz(srgb, SIXEL_COLORSPACE_LINEAR,
                sample, bxyz + 3 * p)) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_qa_scielab(axyz, bxyz, 2, 1, 28.5,
            &expected, &maximum, NULL))
            || SIXEL_FAILED(sixel_frame_init_borrowed(a, av, 2, 1,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, 0))
            || SIXEL_FAILED(sixel_frame_init_borrowed(b, bv, 2, 1,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, 0))) {
        goto end;
    }
    mask = SIXEL_ASSESSMENT_METRIC_MASK(SIXEL_ASSESSMENT_METRIC_SCIELAB);
    index = SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_SCIELAB);
    sixel_assessment_select_metrics(qa, mask);
    qa->scielab_ppd = 28.5;
    /* Luma-only controls must not clip or gray the physical color path. */
    qa->signal_space = SIXEL_COLORSPACE_GAMMA;
    qa->signal_bytes = 1;
    qa->grayscale = 1;
    ok = SIXEL_SUCCEEDED(sixel_assessment_analyze(qa, a, b))
        && qa->results_valid_mask == mask && qa->results_ready
        && isfinite(qa->results[index]) && expected > 0
        && fabs(qa->results[index] - expected) < 1e-7;
end:
    sixel_cdef_free(srgb);
    sixel_assessment_unref(qa);
    sixel_frame_unref(a);
    sixel_frame_unref(b);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
