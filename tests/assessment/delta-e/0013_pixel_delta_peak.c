/* SPDX-License-Identifier: MIT */
/* A sparse black/white error has an analytic DE00 of 100 at the fault. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "assessment/assessment.h"

int
test_qa_0013(int argc, char **argv)
{
    unsigned char reference[64 * 64 * 3];
    unsigned char target[64 * 64 * 3];
    sixel_frame_t *a;
    sixel_frame_t *b;
    sixel_assessment_t *qa;
    unsigned int mask;
    int max_index;
    int mean_index;
    int ok;
    int i;
    int reset;
    int tails;
    unsigned char const grays[5] = {4, 16, 32, 64, 128};

    a = NULL;
    b = NULL;
    qa = NULL;
    ok = 0;
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    reset = strcmp(argv[1], "reset") == 0;
    tails = strcmp(argv[1], "tails") == 0;
    memset(reference, 0, sizeof(reference));
    memset(target, 0, sizeof(target));
    memset(target + (9 * 64 + 17) * 3, 255, 3);
    if (tails) {
        /* Against black, these encoded sRGB grays have DE00 in disjoint
         * intervals (0,1), (1,5), (5,10), (10,20), (20,50), respectively.
         * They follow the sRGB TRC and the neutral CIEDE2000 lightness term;
         * no production color helper generates the expected tail counts. */
        for (i = 0; i < 5; ++i) {
            memset(target + i * 3, grays[i], 3);
        }
    }
    if (reset) {
        /* Equal maxima select the first row-major pixel, not the last. */
        memset(target + (20 * 64 + 2) * 3, 255, 3);
    }
    if (SIXEL_FAILED(sixel_frame_new(&a, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&b, NULL)) ||
            SIXEL_FAILED(sixel_assessment_new(&qa, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(a, reference, 64, 64,
                SIXEL_PIXELFORMAT_RGB888, NULL, 0)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(b, target, 64, 64,
                SIXEL_PIXELFORMAT_RGB888, NULL, 0))) {
        goto end;
    }
    mask = SIXEL_ASSESSMENT_METRIC_MASK(
        SIXEL_ASSESSMENT_METRIC_DELTA_E00_MAX);
    if (strcmp(argv[1], "mean") == 0) {
        mask |= SIXEL_ASSESSMENT_METRIC_MASK(
            SIXEL_ASSESSMENT_METRIC_DELTA_E00);
    }
    sixel_assessment_select_metrics(qa, mask);
    max_index = SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_DELTA_E00_MAX);
    mean_index = SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_DELTA_E00);
    if (SIXEL_FAILED(sixel_assessment_analyze(qa, a, b)) ||
            qa->results_valid_mask != mask ||
            fabs(qa->results[max_index] - 100.0) > 0.00001 ||
            qa->delta_e00_max_x != 17 || qa->delta_e00_max_y != 9) {
        goto end;
    }
    for (i = 0; i < 5; ++i) {
        if (qa->delta_e00_above[i] !=
                (size_t)(tails ? 5 - i : reset ? 2 : 1)) {
            goto end;
        }
    }
    if (strcmp(argv[1], "mean") == 0 &&
            fabs(qa->results[mean_index] - 100.0 / 4096.0) > 0.000001) {
        goto end;
    }
    if (reset) {
        if (SIXEL_FAILED(sixel_assessment_analyze(qa, a, a)) ||
                qa->results[max_index] != 0.0 ||
                qa->delta_e00_max_x != 0 || qa->delta_e00_max_y != 0) {
            goto end;
        }
        for (i = 0; i < 5; ++i) {
            if (qa->delta_e00_above[i] != 0) {
                goto end;
            }
        }
    }
    ok = 1;
end:
    sixel_assessment_unref(qa);
    sixel_frame_unref(a);
    sixel_frame_unref(b);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
