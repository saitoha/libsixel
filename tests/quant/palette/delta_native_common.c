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
#include "src/dither-policy-a-dither.h"
#include "src/dither-policy-x-dither.h"
#include "src/dither-policy-bluenoise.h"

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
static int
dn_frames(int mode, char const *positional, int workspace)
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
    static char const *const spaces[] = {
        "gamma", "linear", "oklab", "cielab", "din99d"
    };
    /* Independently evaluated extended sRGB equations, not CMS output.
     * Normalized Lab/DIN units match the documented working convention. */
    static double const expected_work[4][2][3] = {
        { { -0.00393593950409, 0.01415928656, 1.11754125416 },
          { -0.00393593950409, 0.347281425222, 1.11754125416 } },
        { { 0.481133016718, -0.0360300948491, -0.314968626732 },
          { 0.689632218883, -0.0691481091903, -0.180477139597 } },
        { { 0.359741883657, 0.592721069355, -0.850261220688 },
          { 0.64013315703, 0.0348948871053, -0.491635614787 } },
        { { 0.396057220284, 0.0944152028064, -0.461506164481 },
          { 0.674435639385, -0.154742585819, -0.339409067107 } },
    };
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
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', spaces[workspace]))) {
        goto end;
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
    if (positional != NULL && SIXEL_FAILED(
            sixel_encoder_setopt(encoder, 'd', positional))) {
        goto end;
    }
    for (pass = 0; pass < 4; ++pass) {
        memset(&stream, 0, sizeof(stream));
        /* Borrowed frames permit in-place workspace conversion. Each new
         * frame starts with original gamma input, not the previous scratch. */
        for (i = 0; i < count * 3; ++i) {
            pixels[i] = color[i % 3];
            bytes[i] = wire[i % 3];
        }
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
                if (workspace != 0) {
                    want = expected_work[workspace - 1][pass == 3][c];
                }
                /* Inverse matrix and nonlinear transforms add rounding;
                 * 3e-6 normalized units still exclude byte projection. */
                if (!isfinite(view.entries[c]) ||
                        fabs(view.entries[c] - want) >
                            (workspace != 0 ? 3e-6 : 2e-7)) {
                    fprintf(stderr, "workspace=%d channel=%d actual=%.9g "
                        "expected=%.9g\n", workspace, c, view.entries[c],
                        want);
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

int
test_delta_native_frames(int mode)
{
    return dn_frames(mode, NULL, 0);
}

int
test_delta_positional_frames(int method, int mode)
{
    static char const *const names[] = {
        "a_dither", "x_dither", "bluenoise"
    };

    return dn_frames(mode, names[method], 0);
}

int
test_delta_work_frames(int workspace, int mode, int positional)
{
    return dn_frames(mode, positional ? "x_dither" : NULL, workspace);
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
        if (indexes == NULL ||
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
    float sample[3] = { -.005f, -.005f, -.005f };
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
    for (threshold = 2; threshold <= 3; ++threshold) {
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            1, 1, 0, 0, 1, 1, 1, 1, threshold, SIXEL_6DELTA_ERROR_SKIP);
        keep = sixel_dither_6delta_keep_float(dither, 0, 0, 0,
            SIXEL_PIXELFORMAT_RGBFLOAT32, sample, chosen, 0, 0, retained);
        /* Distance to retained is .008921569, between 2/255 and 3/255.
         * Black is nearer. Clamping the source to zero falsely keeps at 2. */
        if (keep != (threshold == 3) ||
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
    /* OR composition has no connected delta contract. Reject before output,
     * then reuse the same encoder with ordinary RGB serialization. */
    encoder->ormode = 1;
    if (sixel_encoder_encode_frame(encoder, frame, output) !=
            SIXEL_BAD_ARGUMENT || stream.size != 0) {
        goto end;
    }
    encoder->ormode = 0;
    if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
            stream.size == 0 || encoder->diagnostic_dither == NULL ||
            !((sixel_dither_t *)encoder->diagnostic_dither)->
                use_rgb101_palette) {
        goto end;
    }
    ok = 1;
end:
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* The selected candidate is controlled independently of the keep decision.
 * A/X at (0,0) shift red by -32/255; blue's frozen red tap sum is 52/255.
 * The source and the biased query lie on opposite sides of the midpoint
 * between red 0 and .2. A 2e-6 query budget is far below the .01 decision
 * margin. Other channels cancel because the competing colors agree there. */
typedef struct dn_observer {
    sixel_lookup_policy_interface_t base;
    int candidate;
    int calls;
    int check_query;
    int failed;
    double expected_red;
} dn_observer_t;

static sixel_lookup_policy_result_t
dn_observe(sixel_lookup_policy_interface_t const *base,
           unsigned char const *pixel)
{
    dn_observer_t *observer;
    float const *values;

    observer = (dn_observer_t *)(void *)base;
    values = (float const *)(void const *)pixel;
    ++observer->calls;
    if (observer->check_query && (!isfinite(values[0]) ||
            fabs(values[0] - observer->expected_red) > 2e-6)) {
        observer->failed = 1;
    }
    return observer->candidate;
}

static int
dn_observe_hint(sixel_lookup_policy_interface_t const *self,
                unsigned char const *pixel,
                int hint)
{
    (void)hint;
    return dn_observe(self, pixel);
}

static sixel_lookup_policy_vtbl_t const dn_observer_vtbl = {
    NULL, NULL, NULL, dn_observe, dn_observe_hint, NULL
};

int
test_delta_positional_query(int method, int band)
{
    static SIXELSTATUS (*const create[])(sixel_allocator_t *, void **) = {
        sixel_dither_policy_a_dither_float32_new,
        sixel_dither_policy_x_dither_float32_new,
        sixel_dither_policy_bluenoise_float32_new
    };
    unsigned char palette[9] = { 0, 0, 0, 51, 0, 0, 0, 0, 0 };
    unsigned char history[4 * 10 * 3];
    unsigned char valid[4 * 10];
    unsigned char mask[2 * 9];
    unsigned char *record;
    float source[2 * 3 * 3];
    float original[2 * 3 * 3];
    sixel_index_t result[2 * 3];
    sixel_dither_t *dither;
    sixel_dither_policy_interface_t *policy;
    sixel_dither_policy_prepare_request_t prepare;
    sixel_dither_policy_apply_request_t request;
    dn_observer_t observer;
    void *object;
    int pass;
    int i;
    int expected;
    int ok;

    dither = NULL;
    policy = NULL;
    object = NULL;
    record = NULL;
    pass = 0;
    ok = 0;
    memset(&prepare, 0, sizeof(prepare));
    memset(&request, 0, sizeof(request));
    memset(&observer, 0, sizeof(observer));
    memset(history, 0, sizeof(history));
    memset(valid, 1, sizeof(valid));
    memset(mask, 0, sizeof(mask));
    memset(source, 0, sizeof(source));
    if (SIXEL_FAILED(sixel_dither_new(&dither, 3, NULL)) ||
            SIXEL_FAILED(create[method](dither->allocator, &object))) {
        goto end;
    }
    policy = object;
    sixel_dither_set_palette(dither, palette);
    dither->use_rgb101_palette = 1;
    if (!dn_prepare(dither, palette, 3)) {
        goto end;
    }
    record = sixel_allocator_malloc(dither->allocator, sizeof(mask));
    if (record == NULL) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_dither_set_pipeline_accumulation_result_mask(
            dither, record, sizeof(mask)))) {
        sixel_allocator_free(dither->allocator, record);
        record = NULL;
        goto end;
    }
    /* The dither owns record from here. All other buffers are borrowed. */
    dither->a_dither_strength_override = 1;
    dither->a_dither_strength = 1.0f;
    dither->x_dither_strength_override = 1;
    dither->x_dither_strength = 1.0f;
    dither->bluenoise_strength_override = 1;
    dither->bluenoise_strength = 1.0f;
    dither->bluenoise_phase_override = 1;
    dither->bluenoise_phase_x = dither->bluenoise_phase_y = 0;
    dither->bluenoise_channel_override = 1;
    dither->bluenoise_channel_rgb = 1;
    dither->bluenoise_gradient_factor_override = 1;
    dither->bluenoise_gradient_factor = 0.0f;
    observer.base.vtbl = &dn_observer_vtbl;
    observer.check_query = !band;
    observer.candidate = !band && method == 2 ? 1 : 0;
    request.dither = prepare.dither = dither;
    request.depth = prepare.depth = 3;
    prepare.pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    request.width = band ? 2 : 1;
    request.height = band ? 3 : 1;
    request.band_origin = band ? 6 : 0;
    request.output_start = band ? 7 : 0;
    request.data = (unsigned char *)(void *)source;
    request.palette = palette;
    request.result = result;
    request.lookup_policy = &observer.base;
    if (band) {
        /* Only the offset plane positions have the candidate's black.
         * A wrong origin therefore loses the keep contest. */
        for (i = 0; i < 40; ++i) {
            history[i * 3] = 255;
        }
        for (i = 0; i < 6; ++i) {
            history[((i / 2 + 7) * 4 + i % 2 + 1) * 3] = 0;
            source[i * 3] = -.1f;
            source[i * 3 + 1] = .1f;
            source[i * 3 + 2] = 1.1f;
        }
        valid[8 * 4 + 2] = 0;
        mask[8 * 2] = 1;
        sixel_dither_set_pipeline_transparent_mask_hint(dither,
            mask, sizeof(mask), 2);
    } else {
        history[0] = method == 2 ? 0 : 51;
    }
    for (pass = 0; pass < (band ? 2 : 4); ++pass) {
        prepare.method_for_scan = pass & 1
            ? SIXEL_SCAN_SERPENTINE : SIXEL_SCAN_RASTER;
        observer.calls = observer.failed = 0;
        memset(record, 0, sizeof(mask));
        for (i = 0; i < 6; ++i) {
            result[i] = 123;
        }
        if (!band) {
            source[0] = method == 2
                ? (pass == 3 ? .18f : .09f)
                : (pass == 3 ? .02f : .11f);
            valid[0] = pass != 1;
            observer.expected_red = source[0] +
                (method == 2 ? 52.0 / 255.0 : -1.0) * (32.0 / 255.0);
        }
        memcpy(original, source, sizeof(source));
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            band ? 4 : 1, band ? 10 : 1, band, band,
            request.width, band ? 9 : 1, 2, band || pass != 2,
            !band && pass == 3 ? 50U : 0U, SIXEL_6DELTA_ERROR_DIFFUSE);
        if (SIXEL_FAILED(policy->vtbl->prepare(policy, &prepare)) ||
                SIXEL_FAILED(policy->vtbl->apply(policy, &request)) ||
                observer.failed || observer.calls != (band ? 5 : 1) ||
                memcmp(source, original, sizeof(source)) != 0) {
            goto end;
        }
        if (band) {
            /* Warmup is untouched, invalid history paints, source mask
             * omits without recording a delta keep; both scans agree. */
            for (i = 0; i < 6; ++i) {
                expected = i < 2 ? 123 : i == 3 ? 0 : 2;
                if (result[i] != expected) {
                    goto end;
                }
            }
            for (i = 0; i < 18; ++i) {
                if (record[i] != (i == 14 || i == 17)) {
                    goto end;
                }
            }
        } else {
            expected = pass == 0 || pass == 3;
            /* Pass 3 is an approximate early keep; pass 0 is post-lookup. */
            if (result[0] != (expected ? 2 : observer.candidate) ||
                    record[0] != (pass == 3 ? 3 : expected)) {
                goto end;
            }
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "positional delta method=%d band=%d pass=%d: %s\n",
            method, band, pass, sixel_helper_get_additional_message());
    }
    if (policy != NULL) {
        policy->vtbl->unref(policy);
    }
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
