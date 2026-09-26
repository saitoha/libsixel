/*
 * SPDX-License-Identifier: MIT
 * Policy: docs/concepts/pixelformat-precision/README.md
 */
/* A noncontributing extreme and cancellation must not erase a small sample. */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/scale.h"

int
test_frame_0098_dynamic(int argc, char **argv)
{
    static float const small[3] = {1.0f, 0.25f, -0.5f};
    float source[21];
    float original[21];
    float output[12];
    float const *result_pixels;
    float magnitude;
    double expected;
    double small_weight;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    SIXELSTATUS status;
    int shape;
    int direct;
    int sw;
    int sh;
    int dw;
    int dh;
    int c;
    int first;
    int last;
    int result;

    if (argc != 4) {
        return EXIT_FAILURE;
    }
    shape = atoi(argv[1]);
    magnitude = atoi(argv[2]) ? 1e20f : FLT_MAX;
    direct = atoi(argv[3]);
    allocator = NULL;
    frame = NULL;
    result = EXIT_FAILURE;
    memset(source, 0, sizeof(source));
    sw = 2;
    sh = 1;
    dw = 4;
    dh = 1;
    small_weight = 1.0;
    if (shape == 1) {
        sw = 1;
        sh = 2;
        dw = 1;
        dh = 4;
    } else if (shape == 2) {
        dw = 2;
        dh = 2;
    } else if (shape >= 3) {
        sw = shape == 3 || shape == 5 ? 3 : 7;
        dw = 1;
        /* Center triangle weights: 3/7 for N=3, 7/37 for N=7. */
        small_weight = sw == 3 ? 3.0 / 7.0 : 7.0 / 37.0;
        if (shape >= 5) {
            sh = sw;
            sw = 1;
        }
    }
    first = shape >= 3 && sw * sh == 7 ? 2 : 0;
    last = sw * sh - 1 - first;
    /* N=7 uses mirrored inner taps (6/7 each), with zeros at the ends. */
    source[first * 3] = magnitude;
    source[first * 3 + 1] = -magnitude;
    source[first * 3 + 2] = magnitude / 2.0f;
    if (shape < 3) {
        memcpy(source + 3, small, sizeof(small));
    } else {
        memcpy(source + (sw * sh / 2) * 3, small, sizeof(small));
        for (c = 0; c < 3; ++c) {
            source[last * 3 + c] = -source[first * 3 + c];
        }
    }
    memcpy(original, source, sizeof(source));
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (direct) {
        status = sixel_scale_image_rgbf32(output, source, sw, sh,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, dw, dh, SIXEL_RES_BILINEAR,
            allocator);
        result_pixels = output;
    } else {
        status = sixel_frame_new(&frame, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_frame_init_borrowed(frame, source, sw, sh,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_frame_resize_float32(frame, dw, dh, SIXEL_RES_BILINEAR);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        iface = sixel_frame_as_interface(frame);
        status = iface->vtbl->get_pixels(iface, &view);
        if (SIXEL_FAILED(status) || view.pixels != NULL
                || view.pixels_float32 == NULL || view.width != dw
                || view.height != dh
                || view.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                || view.colorspace != SIXEL_COLORSPACE_LINEAR) {
            goto end;
        }
        result_pixels = view.pixels_float32;
    }
    if (SIXEL_FAILED(status)
            || memcmp(source, original, sizeof(source)) != 0) {
        goto end;
    }
    for (c = 0; c < 3; ++c) {
        expected = small[c] * small_weight;
        /* The last footprint has one contributor, or symmetric cancellation. */
        if (!isfinite(result_pixels[(dw * dh - 1) * 3 + c])
                || fabs(result_pixels[(dw * dh - 1) * 3 + c] - expected)
                    > (shape < 3 ? 0.0 : 2e-6)) {
            fprintf(stderr, "channel %d: %.9g expected %.17g\n", c,
                result_pixels[(dw * dh - 1) * 3 + c], expected);
            goto end;
        }
    }
    result = EXIT_SUCCESS;
end:
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    return result;
}
