/* SPDX-License-Identifier: MIT */
/* Delta decisions use emitted gamma RGB; residuals use working coordinates. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/encoder.h"
#include "src/palette.h"
#include "src/sixel_decode_pixels.h"

static int const dw_formats[] = {
    SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, SIXEL_PIXELFORMAT_OKLABFLOAT32,
    SIXEL_PIXELFORMAT_CIELABFLOAT32, SIXEL_PIXELFORMAT_DIN99DFLOAT32
};

/* Independent neutral-axis equations. RGB matrix rounding produces tiny
 * chroma, covered by 3e-6 in normalized working units, not copied as truth. */
static void
dw_gray(float out[3], double gamma, int workspace)
{
    double linear;
    double lightness;

    linear = gamma <= .04045 ? gamma / 12.92
        : pow((gamma + .055) / 1.055, 2.4);
    out[1] = out[2] = 0.0f;
    if (workspace == 0) {
        out[0] = out[1] = out[2] = (float)linear;
    } else if (workspace == 1) {
        out[0] = (float)cbrt(linear);
    } else {
        lightness = linear > 216.0 / 24389.0
            ? 116 * cbrt(linear) - 16 : (24389.0 / 27.0) * linear;
        out[0] = (float)(workspace == 2 ? lightness / 100
            : 3.2522 * log1p(.0036 * lightness));
    }
}

static int
dw_palette(sixel_dither_t **dither)
{
    unsigned char bytes[9] = {0, 0, 0, 255, 255, 255, 0, 0, 0};
    float values[9] = {0, 0, 0, 1, 1, 1, 0, 0, 0};
    sixel_palette_float32_entries_request_t request;

    if (SIXEL_FAILED(sixel_dither_new(dither, 3, NULL))) {
        return 0;
    }
    sixel_dither_set_palette(*dither, bytes);
    memset(&request, 0, sizeof(request));
    request.entries = values;
    request.colors = 3;
    request.depth = 3 * sizeof(float);
    (*dither)->use_rgb101_palette = 1;
    (*dither)->force_palette = 1;
    sixel_dither_set_transparent(*dither, 2);
    sixel_dither_set_lut_policy(*dither, SIXEL_LUT_POLICY_NONE);
    return SIXEL_SUCCEEDED((*dither)->palette->vtbl->init_entries_float32(
        (*dither)->palette, &request)) &&
        SIXEL_SUCCEEDED(sixel_palette_prepare_rgb101((*dither)->palette,
            SIXEL_COLORSPACE_GAMMA, SIXEL_PALETTE_OUTPUT_LEGACY));
}

int
test_delta_work_compare(int workspace)
{
    /* Desired gamma, retained byte, chosen black/white, threshold, keep.
     * First two cases distinguish gamma from linear/perceptual distances.
     * Threshold decisions have >.011 gamma margin; distance >.19 margin. */
    static double const cases[][5] = {
        {.6, 0, 1, 0, 0}, {.4, 255, 0, 0, 0}, {.6, 171, 0, 0, 1},
        {.6, 0, 1, 160, 1}, {.6, 0, 1, 150, 0}
    };
    unsigned char history[3];
    unsigned char valid[1] = {1};
    float query[3];
    float original[3];
    float chosen[3] = {0, 0, 0};
    float retained[3];
    float expected[3];
    sixel_dither_t *dither;
    int row;
    int channel;
    int keep;
    int ok;

    dither = NULL;
    ok = 0;
    row = -1;
    if (!dw_palette(&dither)) {
        goto end;
    }
    for (row = 0; row < 5; ++row) {
        memset(history, (int)cases[row][1], sizeof(history));
        dw_gray(query, cases[row][0], workspace);
        memcpy(original, query, sizeof(query));
        dw_gray(expected, cases[row][1] / 255, workspace);
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            1, 1, 0, 0, 1, 1, 2, 1, (unsigned int)cases[row][3],
            SIXEL_6DELTA_ERROR_DIFFUSE);
        keep = sixel_dither_6delta_keep_float(dither, 0, 0, 0,
            dw_formats[workspace], query, chosen, (int)cases[row][2],
            0, retained);
        if (keep != (int)cases[row][4] ||
                memcmp(query, original, sizeof(query)) != 0) {
            goto end;
        }
        for (channel = 0; keep && channel < 3; ++channel) {
            if (!isfinite(retained[channel]) ||
                    fabs(retained[channel] - expected[channel]) > 3e-6) {
                goto end;
            }
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "delta workspace=%d compare row=%d\n", workspace,
            row);
    }
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_work_extended(int mode)
{
    unsigned char history[3] = {1, 1, 1};
    unsigned char valid[1] = {1};
    unsigned char const *mask;
    float query[3] = {-.005f / 12.92f, -.005f / 12.92f, -.005f / 12.92f};
    float chosen[3] = {0, 0, 0};
    float retained[3];
    sixel_dither_t *dither;
    size_t size;
    int threshold;
    int keep;
    int format;
    int ok;

    dither = NULL;
    ok = 0;
    if (!dw_palette(&dither)) {
        goto end;
    }
    format = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    if (mode == 1) {
        format = SIXEL_PIXELFORMAT_DIN99DFLOAT32;
        query[0] = 1e30f;
        query[1] = query[2] = 0.0f;
    } else if (mode == 2) {
        dither->use_rgb101_palette = 0;
        dither->clamp_promoted_rgb888 = 1;
    }
    sixel_dither_set_pipeline_accumulation_result_enabled(dither, 1);
    for (threshold = 2; threshold <= 3; ++threshold) {
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            1, 1, 0, 0, 1, 1, 2, 1, (unsigned int)threshold,
            SIXEL_6DELTA_ERROR_DIFFUSE);
        if (SIXEL_FAILED(sixel_dither_prepare_delta_mask(dither,
                NULL, 0, 1))) {
            goto end;
        }
        keep = sixel_dither_6delta_keep_float(dither, 0, 0, 0,
            format, query, chosen, 0, 1, retained);
        mask = sixel_dither_get_pipeline_accumulation_result_mask(dither,
            &size);
        /* Gamma distance .008921569 lies between 2/255 and 3/255.
         * A premature gamut clamp would incorrectly keep at threshold 2. */
        if (keep != (mode == 0 && threshold == 3) || mask == NULL ||
                size != 1 || mask[0] != keep) {
            goto end;
        }
    }
    ok = 1;
end:
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_work_residual(int diffuse)
{
    unsigned char history[6] = {171, 171, 171, 0, 0, 0};
    unsigned char valid[2] = {1, 0};
    float pixels[6];
    sixel_index_t *indexes;
    sixel_dither_t *dither;
    int channel;
    int probe;
    int expected;
    int ok;

    dither = NULL;
    indexes = NULL;
    ok = 0;
    if (!dw_palette(&dither)) {
        goto end;
    }
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
    sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
    if (SIXEL_FAILED(sixel_dither_set_clamp_policy(dither,
            SIXEL_DITHER_CLAMP_NONE, 1.01))) {
        goto end;
    }
    for (probe = 0; probe < 2; ++probe) {
        for (channel = 0; channel < 3; ++channel) {
            pixels[channel] = .32f;
            pixels[3 + channel] = probe ? .56f : .51f;
        }
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            2, 1, 0, 0, 2, 1, 2, 1, 0,
            diffuse ? SIXEL_6DELTA_ERROR_DIFFUSE : SIXEL_6DELTA_ERROR_SKIP);
        indexes = sixel_dither_apply_palette(dither,
            (unsigned char *)pixels, 2, 1);
        /* decode_sRGB(171/255)=.40724021; the 7/16 tap is -.03816759.
         * .51 becomes black and .56 remains white. Using gamma history
         * directly would also turn .56 black. All margins exceed .01. */
        expected = diffuse && !probe ? 0 : 1;
        if (indexes == NULL || indexes[0] != 2 || indexes[1] != expected) {
            goto end;
        }
        sixel_allocator_free(dither->allocator, indexes);
        indexes = NULL;
    }
    ok = 1;
end:
    if (dither != NULL) {
        sixel_allocator_free(dither->allocator, indexes);
    }
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct dw_stream {
    unsigned char data[65536];
    size_t size;
} dw_stream_t;

static int
dw_write(char *data, int size, void *context)
{
    dw_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size > sizeof(stream->data) - stream->size) {
        return -1;
    }
    memcpy(stream->data + stream->size, data, (size_t)size);
    stream->size += (size_t)size;
    return size;
}

int
test_delta_work_fixed(int workers)
{
    static struct { int flag; char const *value; } const options[] = {
        {'~', "none"}, {'d', "none"}, {'0', "none"}, {'_', "none"},
        {'.', "float32"}, {'U', "gamma"}, {'t', "rgb"},
        {'Z', "delta:threshold=0"}, {'7', NULL}, {'b', "gray1"}
    };
    static char const *const spaces[] = {
        "linear", "oklab", "cielab", "din99d", "gamma", "linear"
    };
    float pixels[256 * 25 * 3];
    unsigned char *decoded;
    unsigned char *paint;
    unsigned char *palette;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    dw_stream_t stream;
    unsigned int flags;
    int width;
    int height;
    int colors;
    int rows;
    int pass;
    int i;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    decoded = paint = palette = NULL;
    ok = 0;
    rows = workers ? 25 : 1;
    memset(&stream, 0, sizeof(stream));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, dw_write, &stream, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', workers ? "4" : "1"))) {
        goto end;
    }
    for (pass = 0; pass < 6; ++pass) {
        stream.size = 0;
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', spaces[pass]))) {
            goto end;
        }
        for (i = 0; i < 256 * rows * 3; ++i) {
            pixels[i] = pass == 0 && (i / 3 % 2) ? 1.0f : 0.0f;
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    (unsigned char *)pixels, 256, rows,
                    SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                    output))) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || !dither->use_rgb101_palette ||
                (workers && dither->pipeline_last_encode_threads < 2) ||
                SIXEL_FAILED(sixel_decode_raw_with_options_mask(stream.data,
                    (int)stream.size, 0, &decoded, &paint, &width, &height,
                    &palette, &colors, &flags, encoder->allocator)) ||
                width != 256 || height != rows) {
            goto end;
        }
        for (i = 0; i < width * height; ++i) {
            /* Changed white becomes painted black index zero beside an
             * omitted black index zero; later workspace changes keep all. */
            if ((paint[i] != 0) != (pass == 0 || (pass == 1 && i % 2)) ||
                    (paint[i] && (decoded[i] >= colors ||
                     palette[decoded[i] * 3] !=
                         (pass == 0 && i % 2 ? 255 : 0)))) {
                fprintf(stderr, "delta fixed pass=%d pixel=%d\n", pass, i);
                goto end;
            }
        }
        sixel_allocator_free(encoder->allocator, decoded);
        sixel_allocator_free(encoder->allocator, paint);
        sixel_allocator_free(encoder->allocator, palette);
        decoded = paint = palette = NULL;
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (encoder != NULL) {
        sixel_allocator_free(encoder->allocator, decoded);
        sixel_allocator_free(encoder->allocator, paint);
        sixel_allocator_free(encoder->allocator, palette);
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}


/* DIN99d (0,1,-1) is virtual. Its inverse is approximately (-2,-28,73)
 * gamma RGB, where bright history beats black despite worse working error.
 * Black's working squared error is exactly 2; retained pale yellow exceeds
 * 3. The guard is deliberately independent of the gamma threshold gate. */
int
test_delta_work_virtual(int apply)
{
    unsigned char history[3] = {247, 245, 209};
    unsigned char valid[1] = {1};
    float query[3] = {0, 1, -1};
    float chosen[3] = {0, 0, 0};
    float retained[3];
    float values[9] = {0, 0, 0, 1, 0, 0, 0, 0, 0};
    sixel_palette_float32_entries_request_t request;
    sixel_dither_t *dither;
    sixel_index_t *indexes;
    int ok;

    dither = NULL;
    indexes = NULL;
    ok = 0;
    if (!dw_palette(&dither)) {
        goto end;
    }
    sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
        history, sizeof(history), valid, sizeof(valid),
        1, 1, 0, 0, 1, 1, 2, 1, 0, SIXEL_6DELTA_ERROR_DIFFUSE);
    if (!apply) {
        ok = !sixel_dither_6delta_keep_float(dither, 0, 0, 0,
            SIXEL_PIXELFORMAT_DIN99DFLOAT32, query, chosen, 0, 0, retained);
        if (query[0] != 0 || query[1] != 1 || query[2] != -1) {
            ok = 0;
        }
        goto end;
    }
    memset(&request, 0, sizeof(request));
    request.entries = values;
    request.colors = 3;
    request.depth = 3 * sizeof(float);
    if (SIXEL_FAILED(dither->palette->vtbl->init_entries_float32(
            dither->palette, &request)) ||
            SIXEL_FAILED(sixel_palette_prepare_rgb101(dither->palette,
                SIXEL_COLORSPACE_DIN99D, SIXEL_PALETTE_OUTPUT_LEGACY)) ||
            SIXEL_FAILED(sixel_dither_set_clamp_policy(dither,
                SIXEL_DITHER_CLAMP_NONE, 1.01))) {
        goto end;
    }
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_DIN99DFLOAT32);
    sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_FS);
    indexes = sixel_dither_apply_palette(dither,
        (unsigned char *)query, 1, 1);
    ok = indexes != NULL && indexes[0] == 0;
end:
    if (dither != NULL) {
        sixel_allocator_free(dither->allocator, indexes);
    }
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set ts=4 sts=4 sw=4 et: */
