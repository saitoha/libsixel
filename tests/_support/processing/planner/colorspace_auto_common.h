/* SPDX-License-Identifier: MIT
 * Observe AUTO at the planner and public encode-frame boundaries. The
 * borrowed samples must stay bit-identical when source and work agree.
 */
#ifndef TEST_COLORSPACE_AUTO_COMMON_H
#define TEST_COLORSPACE_AUTO_COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/planner.h"

static int
auto_sink(char *data, int size, void *context)
{
    (void)data;
    *(int *)context += size;
    return size;
}

static int
colorspace_auto_check(int mode)
{
    int const formats[] = {
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, SIXEL_PIXELFORMAT_RGB888,
        SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_PIXELFORMAT_CIELABFLOAT32, SIXEL_PIXELFORMAT_DIN99DFLOAT32,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
    };
    int const spaces[] = {
        SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_GAMMA,
        SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_GAMMA,
        SIXEL_COLORSPACE_CIELAB, SIXEL_COLORSPACE_DIN99D,
        SIXEL_COLORSPACE_LINEAR
    };
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_encoding_planner_t plan;
    float samples[24];
    float saved[24];
    unsigned char bytes[24];
    unsigned char *pixels;
    SIXELSTATUS status;
    size_t i, j, length;
    int count, expected, format, clustering, failed;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    failed = 1;
    count = 0;
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) goto end;
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADING_POLICY,
                                  "greedy:B1");
    if (SIXEL_FAILED(status)) goto end;
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY, "off");
    if (SIXEL_FAILED(status)) goto end;
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none");
    if (SIXEL_FAILED(status)) goto end;
    status = sixel_output_new(&output, auto_sink, &count, NULL);
    if (SIXEL_FAILED(status)) goto end;
    for (i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
        for (j = 0; j < 24; ++j) {
            samples[j] = spaces[i] == SIXEL_COLORSPACE_LINEAR
                ? (j % 3 == 0 ? -0.125f : j % 3 == 1 ? 0.5f : 1.25f)
                : 0.25f;
            bytes[j] = 64;
        }
        memcpy(saved, samples, sizeof(saved));
        pixels = formats[i] == SIXEL_PIXELFORMAT_RGB888
            ? bytes : (unsigned char *)samples;
        length = formats[i] == SIXEL_PIXELFORMAT_RGB888
            ? sizeof(bytes) : sizeof(samples);
        expected = mode == 4 ? SIXEL_COLORSPACE_LINEAR :
            mode == 3 && i > 0 ? SIXEL_COLORSPACE_GAMMA : spaces[i];
        format = expected == spaces[i] ? formats[i] :
            expected == SIXEL_COLORSPACE_LINEAR
                ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                : SIXEL_PIXELFORMAT_RGBFLOAT32;
        clustering = mode == 2 ? SIXEL_COLORSPACE_GAMMA : expected;
        if (mode == 3 && i > 0) {
            status = sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_PRECISION_POLICY, "8bit");
            if (SIXEL_FAILED(status)) goto end;
        }
        if (mode == 4) {
            status = sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_WORKING_COLORSPACE, "linear");
            if (SIXEL_FAILED(status)) goto end;
            status = sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_CLUSTERING_COLORSPACE, "oklab");
            if (SIXEL_FAILED(status)) goto end;
            status = sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_CLUSTERING_COLORSPACE, "auto");
            if (SIXEL_FAILED(status)) goto end;
        }
        if (mode == 1) {
            /* AUTO must undo both previous explicit choices, in either
             * option order, on an encoder already used for other inputs. */
            status = sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_WORKING_COLORSPACE, "oklab");
            if (SIXEL_FAILED(status)) goto end;
            status = sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_CLUSTERING_COLORSPACE, "gamma");
            if (SIXEL_FAILED(status)) goto end;
            status = sixel_encoder_setopt(encoder,
                i % 2 ? SIXEL_OPTFLAG_CLUSTERING_COLORSPACE :
                        SIXEL_OPTFLAG_WORKING_COLORSPACE, "auto");
            if (SIXEL_FAILED(status)) goto end;
            status = sixel_encoder_setopt(encoder,
                i % 2 ? SIXEL_OPTFLAG_WORKING_COLORSPACE :
                        SIXEL_OPTFLAG_CLUSTERING_COLORSPACE, "auto");
            if (SIXEL_FAILED(status)) goto end;
        } else if (mode == 2) {
            status = sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_CLUSTERING_COLORSPACE, "gamma");
            if (SIXEL_FAILED(status)) goto end;
        }
        status = sixel_frame_new(&frame, NULL);
        if (SIXEL_FAILED(status)) goto end;
        status = sixel_frame_init_borrowed(frame, pixels, 4, 2,
                                           formats[i], NULL, -1);
        if (SIXEL_FAILED(status)) goto end;
        sixel_encoding_planner_init(&plan);
        sixel_encoding_planner_analyze(&plan, encoder, frame);
        /* Verify the absence of a coordinate transform, not just output
         * similarity. Float work can still request a precision filter. */
        if (plan.working_colorspace_effective != expected ||
                plan.working_pixelformat != format ||
                (expected == spaces[i] &&
                 (plan.colorspace_before_scale || plan.colorspace_after_scale)))
            goto end;
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (SIXEL_FAILED(status) || count == 0 ||
                encoder->working_colorspace != expected ||
                encoder->clustering_colorspace != clustering ||
                sixel_frame_get_pixelformat(frame) != format ||
                (expected == spaces[i] &&
                 memcmp(sixel_frame_get_pixels(frame),
                    formats[i] == SIXEL_PIXELFORMAT_RGB888
                        ? (void const *)bytes : (void const *)saved, length)))
            goto end;
        sixel_frame_unref(frame);
        frame = NULL;
    }
    failed = 0;
end:
    if (failed)
        fprintf(stderr, "AUTO mode %d: status=%x %s\n", mode, status,
                sixel_helper_get_additional_message());
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return failed;
}
#endif
