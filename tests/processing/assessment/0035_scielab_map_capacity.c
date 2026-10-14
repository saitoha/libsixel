/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include "assessment/assessment.h"

int
test_qa_0035(int argc, char **argv)
{
    float pixels[6] = {0, 0, 0, 1, 1, 1};
    double map[2] = {-7, -9};
    sixel_frame_t *frame;
    sixel_assessment_t *qa;
    int ok;

    (void)argc;
    (void)argv;
    frame = NULL;
    qa = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL))
            || SIXEL_FAILED(sixel_assessment_new(&qa, NULL))
            || SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, 0))) {
        goto end;
    }
    sixel_assessment_select_metrics(qa,
        SIXEL_ASSESSMENT_METRIC_MASK(SIXEL_ASSESSMENT_METRIC_SCIELAB));
    qa->scielab_map = map;
    qa->scielab_map_capacity = 1;
    ok = sixel_assessment_analyze(qa, frame, frame) == SIXEL_BAD_ARGUMENT
        && !qa->results_ready && !qa->results_valid_mask
        && map[0] == -7 && map[1] == -9;
end:
    sixel_assessment_unref(qa);
    sixel_frame_unref(frame);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
