/* SPDX-License-Identifier: MIT */
/* Reference values: tests/data/assessment/ms_ssim_origin_reference.py. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assessment/assessment.h"

int
test_qa_0017(int argc, char **argv)
{
    sixel_frame_t *a;
    sixel_frame_t *b;
    sixel_assessment_t *qa;
    float *reference;
    float *target;
    unsigned int mask;
    double expected;
    double actual;
    float av;
    float bv;
    int scenario;
    int width;
    int height;
    int x;
    int y;
    int c;
    int value;
    int index;
    int ok;
    int available;
    size_t offset;
    size_t pixels;

    a = NULL;
    b = NULL;
    qa = NULL;
    reference = NULL;
    target = NULL;
    ok = 0;
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    scenario = atoi(argv[1]);
    if (scenario < 0 || scenario > 8) {
        return EXIT_FAILURE;
    }
    width = scenario == 3 ? 192 : scenario == 4 || scenario == 5 ? 177 : 176;
    height = scenario == 4 || scenario == 5 ? 181 : 176;
    if (scenario == 6) {
        width = 175;
    }
    available = scenario != 6 && scenario != 7;
    pixels = (size_t)width * (size_t)height;
    reference = (float *)malloc(pixels * 3u * sizeof(float));
    target = (float *)malloc(pixels * 3u * sizeof(float));
    if (reference == NULL || target == NULL) {
        goto end;
    }
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            value = (17 * x + 29 * y + (x * y) % 37) % 1024;
            av = (float)value / 1024.0f;
            bv = (float)(3 * value + ((x / 17 + 3 * (y / 13)) % 64) * 8)
                / 4096.0f;
            if (scenario == 0 || scenario == 6 || scenario == 8) {
                bv = av;
            } else if (scenario == 1) {
                av = 0.0f;
                bv = 3.0f / 255.0f;
            } else if (scenario == 2 || scenario == 7) {
                bv = (x + y) % 2 ? 0.75f : 0.25f;
                av = scenario == 7 ? 1.0f - bv : 0.5f;
            }
            offset = ((size_t)y * (size_t)width + (size_t)x) * 3u;
            for (c = 0; c < 3; ++c) {
                reference[offset + (size_t)c] = scenario == 5 ? bv : av;
                target[offset + (size_t)c] = scenario == 5 ? av : bv;
            }
        }
    }
    if (SIXEL_FAILED(sixel_frame_new(&a, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&b, NULL)) ||
            SIXEL_FAILED(sixel_assessment_new(&qa, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(a, reference, width,
                height, SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, 0)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(b, target, width,
                height, SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, 0))) {
        goto end;
    }
    qa->signal_space = SIXEL_COLORSPACE_GAMMA;
    mask = SIXEL_ASSESSMENT_METRIC_MASK(SIXEL_ASSESSMENT_METRIC_MS_SSIM_ORIGIN);
    index = SIXEL_ASSESSMENT_INDEX(SIXEL_ASSESSMENT_METRIC_MS_SSIM_ORIGIN);
    sixel_assessment_select_metrics(qa, mask);
    if (SIXEL_FAILED(sixel_assessment_analyze(qa, a, b))) {
        goto end;
    }
    expected = scenario == 1 ? 0.890641772073393 :
        scenario == 2 ? 0.8264497748127774 :
        scenario == 3 ? 0.9595507147459752 :
        scenario == 4 || scenario == 5 ? 0.95971873668803 : 1.0;
    actual = qa->results[index];
    /* 1.5e-5 covers propagated input/luma error (at most 1.1e-5 for these
     * fixtures), double moments/pooling/pow and float result storage. The
     * independent 2-D generator also derives the input error bound. */
    ok = qa->results_valid_mask == (available ? mask : 0u) &&
        (!available || (isfinite(actual) && fabs(actual - expected) <= 1.5e-5));
    if (scenario == 8 && ok) {
        /* Valid -> unavailable must not publish the preceding score. Both
         * narrow orientations are below the reference admission boundary. */
        for (c = 0; c < 2 && ok; ++c) {
            width = c == 0 ? 176 : 175;
            height = c == 0 ? 175 : 176;
            if (SIXEL_FAILED(sixel_frame_init_borrowed(a, reference, width,
                    height, SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, 0)) ||
                    SIXEL_FAILED(sixel_frame_init_borrowed(b, target, width,
                    height, SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, 0)) ||
                    SIXEL_FAILED(sixel_assessment_analyze(qa, a, b))) {
                ok = 0;
            } else {
                ok = qa->results_valid_mask == 0u && qa->results[index] == 0.0;
            }
        }
    }
    if (!ok) {
        fprintf(stderr, "origin case %d: %.12g, expected %.12g, mask %x\n",
            scenario, actual, expected, qa->results_valid_mask);
    }
end:
    sixel_assessment_unref(qa);
    sixel_frame_unref(a);
    sixel_frame_unref(b);
    free(reference);
    free(target);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
