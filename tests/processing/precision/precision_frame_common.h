/* SPDX-License-Identifier: MIT */
/* Prepared frames expose post-plan samples before palette/wire rounding. */
#ifndef TEST_PRECISION_FRAME_COMMON_H
#define TEST_PRECISION_FRAME_COMMON_H
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "tests/data/color-numeric/frame-byte.h"

static int
precision_sink(char *data, int size, void *context)
{
    int *count;

    (void)data;
    count = context;
    *count += size;
    return size;
}

static int
precision_frame_samples(int mode, int linear)
{
    static unsigned int const witnesses[] = {6, 9, 11, 20, 23, 26};
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    SIXELSTATUS status;
    float pixels[8 * 3];
    float original[3];
    unsigned char const *bytes;
    uint32_t bits;
    unsigned int i;
    unsigned int j;
    unsigned int at;
    unsigned int witness;
    int expected_format;
    int count;
    int failed;
    float const *actual;
    double delta;
    double budget;

    failed = 0;
    /* Constant footprints keep resize geometry independent of interpolation
     * weights. Exact source bits and byte expectations come from Decimal
     * fixtures; gamma resize has its separate float publication budget. */
    for (witness = 0; witness < 6u; ++witness) {
        encoder = NULL;
        frame = NULL;
        output = NULL;
        count = 0;
        at = color_frame_byte_offsets[linear] + witnesses[witness];
        for (i = 0; i < 8 * 3; ++i) {
            bits = color_frame_byte[at].bits[i % 3];
            memcpy(&pixels[i], &bits, sizeof(bits));
        }
        /* Borrowed input may be converted in place before resize allocates
         * its output, so retain the independent input bits separately. */
        memcpy(original, pixels, sizeof(original));
        status = sixel_encoder_new(&encoder, NULL);
        if (SIXEL_FAILED(status)) {
            failed = 1;
            goto cleanup;
        }
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADS, "1");
        if (SIXEL_FAILED(status)) {
            failed = 1;
            goto cleanup;
        }
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY, "off");
        if (SIXEL_FAILED(status)) {
            failed = 1;
            goto cleanup;
        }
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none");
        if (SIXEL_FAILED(status)) {
            failed = 1;
            goto cleanup;
        }
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_PRECISION,
                                      mode == 2 ? "auto" :
                                      mode == 7 ? "float32" : "8bit");
        if (SIXEL_FAILED(status)) {
            failed = 1;
            goto cleanup;
        }
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_WORKING_COLORSPACE,
                                      mode == 3 ? "linear" : "gamma");
        if (SIXEL_FAILED(status)) {
            failed = 1;
            goto cleanup;
        }
        if (mode >= 4) {
            status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_WIDTH, "2");
            if (SIXEL_FAILED(status)) {
                failed = 1;
                goto cleanup;
            }
            status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_HEIGHT, "1");
            if (SIXEL_FAILED(status)) {
                failed = 1;
                goto cleanup;
            }
            status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_RESAMPLING,
                                          "nearest");
            if (SIXEL_FAILED(status)) {
                failed = 1;
                goto cleanup;
            }
            status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_RUNTIME_POLICY,
                mode == 4 ? "scalar:resize_precision=float" :
                mode == 5 ? "scalar:resize_precision=linear" :
                            "scalar:resize_precision=preserve");
            if (SIXEL_FAILED(status)) {
                failed = 1;
                goto cleanup;
            }
        }
        status = sixel_frame_new(&frame, NULL);
        if (SIXEL_FAILED(status)) {
            failed = 1;
            goto cleanup;
        }
        status = sixel_frame_init_borrowed(frame, (unsigned char *)pixels,
            4, 2, linear ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 :
                          SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1);
        if (SIXEL_FAILED(status)) {
            failed = 1;
            goto cleanup;
        }
        status = sixel_output_new(&output, precision_sink, &count, NULL);
        if (SIXEL_FAILED(status)) {
            failed = 1;
            goto cleanup;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        expected_format = mode == 3 ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 :
            mode == 2 || mode == 4 || mode == 7 ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
                                                SIXEL_PIXELFORMAT_RGB888;
        if (SIXEL_FAILED(status) || count == 0 ||
            sixel_frame_get_pixelformat(frame) != expected_format ||
            sixel_frame_get_colorspace(frame) !=
                (mode == 3 ? SIXEL_COLORSPACE_LINEAR :
                             SIXEL_COLORSPACE_GAMMA) ||
            sixel_frame_get_width(frame) != (mode >= 4 ? 2 : 4) ||
            sixel_frame_get_height(frame) != (mode >= 4 ? 1 : 2)) {
            fprintf(stderr, "mode=%d linear=%d status=%x got=%x expected=%x\n",
                mode, linear, status, sixel_frame_get_pixelformat(frame),
                expected_format);
            failed = 1;
        } else if (expected_format == SIXEL_PIXELFORMAT_RGB888) {
            bytes = sixel_frame_get_pixels(frame);
            for (i = 0; i < (mode >= 4 ? 2u : 8u); ++i) {
                for (j = 0; j < 3; ++j) {
                    if (bytes[i * 3 + j] != color_frame_byte[at].bytes[j]) {
                        fprintf(stderr, "byte mismatch mode=%d linear=%d "
                            "witness=%u got=%u expected=%u\n", mode, linear,
                            witness, bytes[i * 3 + j],
                            color_frame_byte[at].bytes[j]);
                        failed = 1;
                    }
                }
            }
        } else {
            actual = (float const *)sixel_frame_get_pixels(frame);
            for (i = 0; i < (mode >= 4 ? 2u : 8u); ++i) {
                for (j = 0; j < 3u; ++j) {
                    /* Float resize publishes a linear intermediate. The
                     * other float cases preserve their original coordinates
                     * exactly; no production transform is an oracle here. */
                    budget = mode == 4
                        ? fabs((double)original[j]) * 64.0 / 16777216.0 : 0.0;
                    delta = fabs((double)actual[i * 3u + j] - original[j]);
                    if (!isfinite(actual[i * 3u + j]) || delta > budget) {
                        fprintf(stderr, "float sample changed mode=%d "
                            "channel=%u delta=%.17g budget=%.17g\n",
                            mode, j, delta, budget);
                        failed = 1;
                    }
                }
            }
        }
cleanup:
        sixel_output_unref(output);
        sixel_frame_unref(frame);
        sixel_encoder_unref(encoder);
    }
    return failed;
}

#endif /* TEST_PRECISION_FRAME_COMMON_H */
