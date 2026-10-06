/* SPDX-License-Identifier: MIT */
/* Test final samples, not the final register snapshot. The latter loses
 * earlier paint colors when high-color output redefines a register. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/output.h"
#include "src/sixel_decode_pixels.h"

typedef struct hc_stream {
    unsigned char bytes[131072];
    size_t size;
} hc_stream_t;

static int
hc_write(char *data, int size, void *context)
{
    hc_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size >= sizeof(stream->bytes) - stream->size) {
        return -1;
    }
    memcpy(stream->bytes + stream->size, data, (size_t)size);
    stream->size += (size_t)size;
    stream->bytes[stream->size] = 0;
    return size;
}

static int
hc_decode(hc_stream_t *stream, int width, int height, int const *expected,
          int multi)
{
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char *palette;
    unsigned int flags;
    int w;
    int h;
    int ncolors;
    int i;
    int c;
    int want;
    int ok;

    allocator = NULL;
    pixels = palette = NULL;
    w = h = 0;
    ok = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                NULL, NULL)) ||
            SIXEL_FAILED(sixel_decode_direct_with_options(stream->bytes,
                (int)stream->size, 0, &pixels, &w, &h, &palette, &ncolors,
                &flags, allocator)) || w != width || h != height) {
        fprintf(stderr, "decode dimensions %dx%d expected %dx%d\n",
            w, h, width, height);
        goto end;
    }
    for (i = 0; i < width * height; ++i) {
        for (c = 0; c < 3; ++c) {
            want = expected[multi ? i * 3 + c : c];
            want = (want * 255 + 50) / 100;
            if (pixels[i * 4 + c] != want || pixels[i * 4 + 3] != 255) {
                fprintf(stderr, "pixel %d channel %d: %u expected %d\n",
                    i, c, pixels[i * 4 + c], want);
                goto end;
            }
        }
    }
    if (multi && (flags & SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED)
            == 0U) {
        fprintf(stderr, "multi-pass fixture did not redefine registers\n");
        goto end;
    }
    ok = 1;
end:
    sixel_allocator_free(allocator, pixels);
    sixel_allocator_free(allocator, palette);
    sixel_allocator_unref(allocator);
    return ok;
}

int
test_highcolor_output(int scenario)
{
    /* Independent analytic references: sRGB transfer at .25/.5/.75,
     * inverse transfer at 64/128/192 over 255, and the documented SMPTE-C
     * matrix at (.5, -.9, .5). All finite fixtures are away from percentage
     * ties: transformed samples have > .02 field units of margin, and the
     * multipass samples have > .0098, versus a conservative 1e-4 propagated
     * arithmetic budget. The subbyte witness has .1 field
     * margin and intentionally crosses a bin after an RGB888 store. */
    static int const byte_rgb[3] = { 25, 50, 75 };
    static int const linear_rgb[3] = { 5, 22, 53 };
    static int const subbyte_rgb[3] = { 12, 23, 34 };
    static int const gamma_rgb[3] = { 54, 74, 88 };
    static int const smptec_rgb[3] = { 78, 0, 73 };
    static int const hls_rgb[3] = { 23, 23, 23 };
    static int const diffusion_rgb[3] = { 13, 24, 35 };
    unsigned char bytes[4];
    float samples[32 * 24 * 3];
    float saved[32 * 24 * 3];
    int multi_expected[32 * 24 * 3];
    int const *expected;
    unsigned char *input;
    sixel_dither_t *dither;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    hc_stream_t stream;
    SIXELSTATUS status;
    int format;
    int source;
    int target;
    int width;
    int height;
    int i;
    int c;
    int step;
    int steps;
    int key_index;
    int ok;

    dither = NULL;
    encoder = NULL;
    frame = NULL;
    output = NULL;
    ok = 0;
    bytes[0] = 64; bytes[1] = 128; bytes[2] = 192; bytes[3] = 255;
    samples[0] = .124f; samples[1] = .234f; samples[2] = .344f;
    width = height = 1;
    format = SIXEL_PIXELFORMAT_RGBFLOAT32;
    source = target = SIXEL_COLORSPACE_GAMMA;
    input = (unsigned char *)samples;
    expected = subbyte_rgb;
    if (scenario == 1 || scenario == 2 || scenario == 9) {
        format = scenario == 9 ? SIXEL_PIXELFORMAT_RGBA8888 :
            SIXEL_PIXELFORMAT_RGB888;
        input = bytes;
        target = scenario == 1 ? SIXEL_COLORSPACE_GAMMA :
            SIXEL_COLORSPACE_LINEAR;
        expected = scenario == 1 ? byte_rgb : linear_rgb;
    } else if (scenario == 4) {
        source = SIXEL_COLORSPACE_LINEAR;
        format = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        samples[0] = .25f; samples[1] = .5f; samples[2] = .75f;
        expected = gamma_rgb;
    } else if (scenario == 5 || scenario == 11) {
        source = SIXEL_COLORSPACE_LINEAR;
        target = SIXEL_COLORSPACE_SMPTEC;
        format = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        samples[0] = .5f; samples[1] = -.9f; samples[2] = .5f;
        expected = smptec_rgb;
    } else if (scenario == 6) {
        samples[0] = samples[1] = samples[2] = .234f;
        expected = hls_rgb;
    } else if (scenario == 12) {
        width = 32;
        height = 24;
        for (i = 0; i < width * height; ++i) {
            /* A quiet last band avoids the separately documented legacy
             * pending-pass/final-band defect. This test owns representative
             * lifetime across completed passes, not scheduler repair. */
            key_index = i < 576 ? i : 575;
            samples[i * 3] = (float)((key_index % 32) * 8 + 2) / 255.0f;
            samples[i * 3 + 1] = (float)((key_index / 32) * 8 + 2) / 255.0f;
            samples[i * 3 + 2] = .234f;
            for (c = 0; c < 3; ++c) {
                multi_expected[i * 3 + c] = c == 2 ? 23 :
                    (((c == 0 ? key_index % 32 : key_index / 32) * 8 + 2)
                        * 100 + 127)
                        / 255;
            }
        }
        expected = multi_expected;
    } else if (scenario == 13) {
        expected = diffusion_rgb;
    }
    memcpy(saved, samples, (size_t)width * (size_t)height * 3 * sizeof(float));
    memset(&stream, 0, sizeof(stream));
    if (SIXEL_FAILED(sixel_output_new(&output, hc_write, &stream, NULL))) {
        goto end;
    }
    sixel_output_set_palette_type(output, scenario == 6 ?
        SIXEL_PALETTETYPE_HLS : SIXEL_PALETTETYPE_RGB);
    if (scenario == 10 || scenario == 11) {
        if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'I', NULL)) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', "1")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, '.', "8bit")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                    scenario == 11 ? "linear" : "gamma")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                    scenario == 11 ? "smpte-c" : "gamma")) ||
                SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, input, width,
                    height, format, NULL, -1)) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                    output)) || encoder->diagnostic_dither == NULL ||
                !SIXEL_PIXELFORMAT_IS_FLOAT32(
                    sixel_frame_get_pixelformat(frame))) {
            goto end;
        }
        ok = hc_decode(&stream, width, height, expected, 0);
        goto end;
    }
    if (SIXEL_FAILED(sixel_dither_new(&dither, -1, NULL))) {
        goto end;
    }
    sixel_dither_set_diffusion_type(dither, scenario == 13 ?
        SIXEL_DIFFUSE_FS : SIXEL_DIFFUSE_NONE);
    steps = scenario == 14 ? 3 : 1;
    for (step = 0; step < steps; ++step) {
        if (scenario == 14 && step != 0) {
            input = bytes;
            format = SIXEL_PIXELFORMAT_RGB888;
            target = step == 1 ? SIXEL_COLORSPACE_LINEAR :
                SIXEL_COLORSPACE_GAMMA;
            expected = step == 1 ? linear_rgb : byte_rgb;
        }
        stream.size = 0;
        sixel_dither_set_pixelformat(dither, format);
        if (SIXEL_FAILED(sixel_output_set_encoder_format(output,
                format, source, target))) {
            goto end;
        }
        if (scenario == 7 || scenario == 8) {
            samples[1] = scenario == 7 ? NAN : INFINITY;
            status = sixel_encode(input, width, height, 0, dither, output);
            if (status != SIXEL_BAD_INPUT || stream.size != 0 ||
                    dither->pixelformat != format) {
                goto end;
            }
            samples[1] = saved[1];
        }
        status = sixel_encode(input, width, height, 0, dither, output);
        if (SIXEL_FAILED(status) || dither->pixelformat != format ||
                memcmp(samples, saved, (size_t)width * (size_t)height *
                    3 * sizeof(float)) != 0 ||
                !hc_decode(&stream, width, height, expected, scenario == 12)) {
            goto end;
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "high-color scenario %d: %s\n", scenario,
            sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    sixel_dither_unref(dither);
    sixel_output_unref(output);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
