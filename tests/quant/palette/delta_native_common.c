/* SPDX-License-Identifier: MIT */
/* Native delta compares emitted colors without replacing working floats. */
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
#include "src/palette.h"

typedef struct dn_stream {
    char bytes[65536];
    size_t size;
} dn_stream_t;

static int
dn_write(char *data, int size, void *context)
{
    dn_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size >= sizeof(stream->bytes) - stream->size) {
        return -1;
    }
    memcpy(stream->bytes + stream->size, data, (size_t)size);
    stream->size += (size_t)size;
    stream->bytes[stream->size] = '\0';
    return size;
}

/* A single extended color makes the independent palette oracle exact up to
 * binary32 storage. No image metric can substitute for this observation. */
int
test_delta_native_frames(int mode)
{
    static struct { int flag; char const *value; } const options[] = {
        {'=', "1"}, {'~', "none"}, {'d', "none"}, {'Q', "heckbert"},
        {'F', "none"}, {'a', "off"}, {'_', "none"}, {'0', "none"},
        {'X', "gamma"}, {'W', "gamma"}, {'U', "gamma"}, {'t', "rgb"},
        {'.', "float32"}, {'p', "4"}, {'4', "full-frame"}, {'5', "none"},
        {'Z', "delta"}, {'7', NULL}
    };
    static char const *const methods[] = {
        "auto", "fs", "atkinson", "jajuni", "stucki", "burkes",
        "sierra:variant=1", "sierra:variant=2", "sierra:variant=3", "lso2"
    };
    static float const color[3] = { -.05f, .124f, 1.05f };
    static unsigned char const wire[3] = { 0, 31, 255 };
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    sixel_frame_transparency_t transparency;
    sixel_frame_interface_t *iface;
    dn_stream_t stream;
    float pixels[4 * 25 * 3];
    unsigned char bytes[sizeof(pixels) / sizeof(float)];
    unsigned char *decoded;
    unsigned char *palette;
    unsigned char *mask;
    int width;
    int height;
    int colors;
    int rows;
    int count;
    int pass;
    int i;
    int c;
    int keep;
    int expected;
    double want;
    int ok;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    decoded = NULL;
    palette = NULL;
    mask = NULL;
    pass = 0;
    ok = 0;
    rows = mode == 4 ? 25 : 1;
    count = rows * 4;
    for (i = 0; i < count * 3; ++i) {
        pixels[i] = color[i % 3];
        bytes[i] = wire[i % 3];
    }
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, dn_write, &stream, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if ((mode == 1 && SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
            "8bit"))) || (mode == 4 && SIXEL_FAILED(sixel_encoder_setopt(
            encoder, '=', "4"))) || (mode == 2 && SIXEL_FAILED(
            sixel_encoder_set_6delta_plane_size(encoder, 6, 1)))) {
        goto end;
    }
    if (mode >= 10 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            'd', methods[mode - 10]))) {
        goto end;
    }
    for (pass = 0; pass < 4; ++pass) {
        memset(&stream, 0, sizeof(stream));
        if (pass == 3) {
            /* A changed source must repaint, not freeze on valid history. */
            for (i = 0; i < count; ++i) {
                pixels[i * 3 + 1] = .624f;
                bytes[i * 3 + 1] = 158;
            }
        }
        if (pass == 2) {
            sixel_encoder_invalidate_6delta_plane(encoder);
        }
        if (mode == 2 && SIXEL_FAILED(sixel_encoder_set_6delta_plane_origin(
                encoder, pass ? 2 : 0, 0))) {
            goto end;
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    mode == 1 ? bytes : (unsigned char *)pixels, 4, rows,
                    mode == 1 ? SIXEL_PIXELFORMAT_RGB888 :
                        SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
            goto end;
        }
        if (mode == 3) {
            mask = calloc((size_t)count, 1U);
            if (mask == NULL) {
                goto end;
            }
            mask[3] = 1;
            memset(&transparency, 0, sizeof(transparency));
            transparency.transparent = -1;
            transparency.alpha_zero_is_transparent = 1;
            transparency.transparent_mask = mask;
            transparency.transparent_mask_size = (size_t)count;
            iface = (sixel_frame_interface_t *)frame;
            if (SIXEL_FAILED(iface->vtbl->set_transparency(iface,
                    &transparency))) {
                goto end;
            }
            mask = NULL;
        }
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != (mode != 1) ||
                (mode == 4 && dither->pipeline_last_encode_threads < 2) ||
                !encoder->accumulation_valid ||
                SIXEL_FAILED(sixel_decode_raw((unsigned char *)stream.bytes,
                    (int)stream.size, &decoded, &width, &height, &palette,
                    &colors, NULL)) || width != 4 || height != rows) {
            goto end;
        }
        if (mode != 1) {
            if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) || view.entries == NULL) {
                goto end;
            }
            for (c = 0; c < 3; ++c) {
                /* One centroid: a 2e-7 budget covers binary32 accumulation.
                 * Green is .001 below the next RGB101 boundary; source
                 * byte rounding gives 32, while actual wire RGB is 31. */
                want = pass == 3 && c == 1 ? .624 : color[c];
                if (!isfinite(view.entries[c]) ||
                        fabs(view.entries[c] - want) > 2e-7) {
                    goto end;
                }
            }
        }
        for (i = 0; i < count; ++i) {
            keep = (mode == 3 && i == 3) ||
                (pass == 1 && (mode != 2 || i < 2));
            if ((decoded[i] >= colors) != keep) {
                fprintf(stderr, "pixel %d keep=%d index=%d colors=%d\n",
                    i, keep, decoded[i], colors);
                goto end;
            }
            for (c = 0; c < 3 && !keep; ++c) {
                expected = pass == 3 && c == 1 ? 158 : wire[c];
                if (palette[decoded[i] * 3 + c] != expected) {
                    goto end;
                }
            }
        }
        /* Validate actual retained colors too, including invalid holes. */
        for (i = 0; i < (mode == 2 ? 6 : count); ++i) {
            keep = mode == 3 && i == 3;
            if (mode == 2) {
                keep = pass == 0 ? i >= 4 : pass >= 2 && i < 2;
            }
            if (encoder->accumulation_valid_mask[i] != !keep) {
                goto end;
            }
            for (c = 0; c < 3 && !keep; ++c) {
                expected = pass == 3 && c == 1 ? 158 : wire[c];
                if (encoder->accumulation_pixels[i * 3 + c] != expected) {
                    goto end;
                }
            }
        }
        free(decoded);
        free(palette);
        decoded = NULL;
        palette = NULL;
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "delta frame mode=%d pass=%d: %s\n", mode, pass,
            sixel_helper_get_additional_message());
    }
    free(mask);
    free(decoded);
    free(palette);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Low-level setup supplies both independent storage views, as the high-level
 * native builder does. A byte setter alone deliberately has no float view. */
static int
dn_prepare(sixel_dither_t *dither, unsigned char const *bytes, int count)
{
    sixel_palette_float32_entries_request_t request;
    float entries[9];
    int i;

    for (i = 0; i < count * 3; ++i) {
        entries[i] = (float)bytes[i] / 255.0f;
    }
    request.entries = entries;
    request.colors = (unsigned int)count;
    request.depth = 3 * sizeof(float);
    return SIXEL_SUCCEEDED(dither->palette->vtbl->init_entries_float32(
            dither->palette, &request)) &&
        SIXEL_SUCCEEDED(sixel_palette_prepare_rgb101(dither->palette,
            SIXEL_COLORSPACE_GAMMA, SIXEL_PALETTE_OUTPUT_LEGACY));
}

/* A kept .6 gray at a desired .55 creates a negative residual. All nine
 * kernels have a positive immediate-right tap; .501 must become black in
 * diffuse mode and remain white in skip mode. The next pixel is invalid in
 * retained history, so it must perform a real lookup. */
int
test_delta_native_residual(int method)
{
    static int const methods[] = {
        SIXEL_DIFFUSE_FS, SIXEL_DIFFUSE_ATKINSON, SIXEL_DIFFUSE_JAJUNI,
        SIXEL_DIFFUSE_STUCKI, SIXEL_DIFFUSE_BURKES, SIXEL_DIFFUSE_SIERRA1,
        SIXEL_DIFFUSE_SIERRA2, SIXEL_DIFFUSE_SIERRA3, SIXEL_DIFFUSE_LSO2
    };
    unsigned char palette[9] = { 0, 0, 0, 255, 255, 255, 0, 0, 0 };
    unsigned char history[6] = { 153, 153, 153, 0, 0, 0 };
    unsigned char valid[2] = { 1, 0 };
    float pixels[6];
    sixel_dither_t *dither;
    sixel_index_t *indexes;
    int mode;
    int i;
    int ok;

    dither = NULL;
    indexes = NULL;
    ok = 0;
    mode = 0;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 3, NULL))) {
        goto end;
    }
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGBFLOAT32);
    sixel_dither_set_diffusion_type(dither, methods[method]);
    sixel_dither_set_diffusion_scan(dither, SIXEL_SCAN_RASTER);
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_optimize_palette(dither, 0);
    sixel_dither_set_transparent(dither, 2);
    dither->use_rgb101_palette = 1;
    dither->force_palette = 1;
    if (SIXEL_FAILED(sixel_dither_set_clamp_policy(dither,
            SIXEL_DITHER_CLAMP_NONE, 1.01)) ||
            !dn_prepare(dither, palette, 3)) {
        goto end;
    }
    for (mode = 0; mode < 2; ++mode) {
        for (i = 0; i < 3; ++i) {
            pixels[i] = .55f;
            pixels[3 + i] = .501f;
        }
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            2, 1, 0, 0, 2, 1, 2, 1, 0U,
            mode ? SIXEL_6DELTA_ERROR_DIFFUSE : SIXEL_6DELTA_ERROR_SKIP);
        indexes = sixel_dither_apply_palette(dither,
            (unsigned char *)pixels, 2, 1);
        if (indexes == NULL || dither->clamp_promoted_rgb888 != 0 ||
                indexes[0] != 2 ||
                indexes[1] != (mode ? 0 : 1)) {
            goto end;
        }
        sixel_allocator_free(dither->allocator, indexes);
        indexes = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "native delta residual method=%d mode=%d: %s\n",
            method, mode, sixel_helper_get_additional_message());
        if (indexes != NULL) {
            fprintf(stderr, "indexes=%u,%u\n", indexes[0], indexes[1]);
        }
    }
    if (dither != NULL) {
        sixel_allocator_free(dither->allocator, indexes);
    }
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_native_threshold(int unused)
{
    unsigned char palette[6] = { 0, 0, 0, 255, 255, 255 };
    unsigned char history[3] = { 1, 1, 1 };
    unsigned char valid[1] = { 1 };
    float sample[3] = { -.003f, -.003f, -.003f };
    float chosen[3] = { 0, 0, 0 };
    float retained[3];
    sixel_dither_t *dither;
    unsigned int threshold;
    int keep;
    int ok;

    (void)unused;
    dither = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 2, NULL))) {
        goto end;
    }
    sixel_dither_set_palette(dither, palette);
    dither->use_rgb101_palette = 1;
    if (!dn_prepare(dither, palette, 2)) {
        goto end;
    }
    for (threshold = 1; threshold <= 2; ++threshold) {
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            1, 1, 0, 0, 1, 1, 1, 1, threshold, SIXEL_6DELTA_ERROR_SKIP);
        keep = sixel_dither_6delta_keep_float(dither, 0, 0, 0,
            SIXEL_PIXELFORMAT_RGBFLOAT32, sample, chosen, 0, 0, retained);
        /* Distance to retained is .006921569, between 1/255 and 2/255.
         * Black is nearer. Clamping the source to zero falsely keeps at 1. */
        if (keep != (threshold == 2) ||
                !isfinite(retained[0]) ||
                fabs(retained[0] - 1.0 / 255.0) > 1e-9) {
            goto end;
        }
    }
    ok = 1;
end:
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_delta_native_scope(int unused)
{
    static struct { int flag; char const *value; } const options[] = {
        {'=', "1"}, {'~', "none"}, {'d', "none"}, {'Q', "heckbert"},
        {'F', "none"}, {'a', "off"}, {'_', "none"}, {'X', "gamma"},
        {'W', "gamma"}, {'U', "display-p3"}, {'t', "rgb"},
        {'.', "float32"}, {'p', "4"}, {'Z', "delta"}
    };
    float pixels[3] = { .2f, .4f, .6f };
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    dn_stream_t stream;
    int i;
    int ok;

    (void)unused;
    encoder = NULL;
    frame = NULL;
    output = NULL;
    ok = 0;
    memset(&stream, 0, sizeof(stream));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                (unsigned char *)pixels, 1, 1, SIXEL_PIXELFORMAT_RGBFLOAT32,
                NULL, -1)) ||
            SIXEL_FAILED(sixel_output_new(&output, dn_write, &stream, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    /* An unconnected destination must reject before emitting any bytes.
     * Reuse the same encoder after selecting the connected interpretation. */
    if (sixel_encoder_encode_frame(encoder, frame, output) !=
            SIXEL_BAD_ARGUMENT || stream.size != 0 ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U', "gamma")) ||
            SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
            stream.size == 0 || encoder->diagnostic_dither == NULL ||
            !((sixel_dither_t *)encoder->diagnostic_dither)->use_rgb101_palette) {
        goto end;
    }
    ok = 1;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
