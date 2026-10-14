/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include "assessment/assessment.h"

int
test_qa_0034(int argc, char **argv)
{
    float a[6] = {0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f};
    float b[6] = {1, 1, 1, 1, 1, 1};
    double map[2] = {-1, -1};
    sixel_frame_t *ref;
    sixel_frame_t *out;
    sixel_assessment_t *qa;
    int ok;

    (void)argc;
    (void)argv;
    ref = NULL;
    out = NULL;
    qa = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_frame_new(&ref, NULL))
            || SIXEL_FAILED(sixel_frame_new(&out, NULL))
            || SIXEL_FAILED(sixel_assessment_new(&qa, NULL))
            || SIXEL_FAILED(sixel_frame_init_borrowed(ref, a, 2, 1,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, 0))
            || SIXEL_FAILED(sixel_frame_init_borrowed(out, b, 2, 1,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, 0))) {
        goto end;
    }
    sixel_assessment_select_metrics(qa,
        SIXEL_ASSESSMENT_METRIC_MASK(SIXEL_ASSESSMENT_METRIC_MS_SSIM));
    qa->scielab_map = map;
    qa->scielab_map_capacity = 2;
    /* Uniform neutral Y=1/8 and Y=1 have L*=42 and 100. Float32
     * frame conversion and the sRGB matrix justify 0.01 CIE76 tolerance. */
    ok = SIXEL_SUCCEEDED(sixel_assessment_analyze(qa, ref, out))
        && qa->results_ready && isfinite(map[0]) && isfinite(map[1])
        && fabs(map[0] - 58) < 0.01 && fabs(map[1] - 58) < 0.01;
end:
    sixel_assessment_unref(qa);
    sixel_frame_unref(ref);
    sixel_frame_unref(out);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
