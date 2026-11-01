/* SPDX-License-Identifier: MIT */
/* Byte delta compares emitted candidates without snapping working entries. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/encoder.h"
#include "src/output.h"
#include "src/palette.h"
#include "src/sixel_decode_pixels.h"

typedef struct db_stream {
    char data[65536];
    size_t size;
} db_stream_t;

static int
db_write(char *data, int size, void *context)
{
    db_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size >= sizeof(stream->data) - stream->size) {
        return -1;
    }
    memcpy(stream->data + stream->size, data, (size_t)size);
    stream->size += (size_t)size;
    stream->data[stream->size] = '\0';
    return size;
}

static int
db_setup(sixel_dither_t **dither, sixel_output_t **output,
         db_stream_t *stream, int method)
{
    unsigned char palette[9] = {32, 32, 32, 200, 200, 200, 0, 0, 0};

    if (SIXEL_FAILED(sixel_dither_new(dither, 3, NULL)) ||
            SIXEL_FAILED(sixel_output_new(output, db_write, stream, NULL))) {
        return 0;
    }
    sixel_dither_set_palette(*dither, palette);
    sixel_dither_set_transparent(*dither, 2);
    (*dither)->keycolor_reserved = 1;
    (*dither)->force_palette = 1;
    sixel_dither_set_pipeline_accumulation_result_enabled(*dither, 1);
    sixel_dither_set_lut_policy(*dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_diffusion_type(*dither, method);
    sixel_output_set_palette_type(*output, SIXEL_PALETTETYPE_RGB);
    return SIXEL_SUCCEEDED(sixel_dither_set_clamp_policy(*dither,
        SIXEL_DITHER_CLAMP_HARD, 1.01));
}

/* The previous color 33 and the chosen byte 32 both mean emitted 13%.
 * All methods must retain that tie, including when diffusion changes later
 * source queries. The positional noise cannot select the distant gray 200. */
int
test_delta_byte_method(int method)
{
    unsigned char pixels[4 * 3 * 3];
    unsigned char history[sizeof(pixels)];
    unsigned char valid[sizeof(pixels) / 3];
    unsigned char const *mask;
    sixel_palette_entries_view_t entries;
    sixel_palette_float32_entries_view_t floats;
    sixel_dither_t *dither;
    sixel_output_t *output;
    db_stream_t stream;
    size_t size;
    size_t i;
    int ok;

    dither = NULL;
    output = NULL;
    ok = 0;
    memset(&stream, 0, sizeof(stream));
    memset(pixels, 32, sizeof(pixels));
    memset(history, 33, sizeof(history));
    memset(valid, 1, sizeof(valid));
    if (!db_setup(&dither, &output, &stream, method)) {
        goto end;
    }
    sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
        history, sizeof(history), valid, sizeof(valid),
        4, 3, 0, 0, 4, 3, 2, 1, 0, SIXEL_6DELTA_ERROR_DIFFUSE);
    if (SIXEL_FAILED(sixel_encode(pixels, 4, 3, 1, dither, output)) ||
            dither->pipeline_6delta_rgb101 ||
            dither->use_rgb101_palette ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries(dither->palette,
                &entries)) || entries.entries[0] != 32 ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &floats)) || floats.entries != NULL) {
        goto end;
    }
    mask = sixel_dither_get_pipeline_accumulation_result_mask(dither, &size);
    if (mask == NULL || size != sizeof(valid)) {
        goto end;
    }
    for (i = 0; i < size; ++i) {
        if (mask[i] != 1) {
            fprintf(stderr, "byte method=%d pixel=%zu keep=%u\n",
                method, i, mask[i]);
            goto end;
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "byte method=%d: %s\n", method,
            sixel_helper_get_additional_message());
    }
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Literal one-channel cases distinguish tie, better repaint, an unrounded
 * query, and standalone application without a wire contract. Distances are
 * exact integers; RGB101 rounding is applied only to the chosen candidate. */
int
test_delta_byte_compare(int unused)
{
    static int const cases[][5] = {
        {32, 33, 32, 1, 1}, {32, 34, 32, 1, 0},
        {34, 35, 32, 1, 1}, {32, 33, 32, 0, 0},
        {0, 0, 1, 1, 1}, {255, 254, 254, 1, 0}
    };
    unsigned char history[3] = {0, 0, 0};
    unsigned char source[3] = {0, 0, 0};
    unsigned char chosen[3] = {0, 0, 0};
    unsigned char valid[1] = {1};
    unsigned char const *retained;
    sixel_dither_t *dither;
    int key;
    int keep;
    int i;
    int ok;

    (void)unused;
    dither = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 3, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); ++i) {
        source[0] = (unsigned char)cases[i][0];
        history[0] = (unsigned char)cases[i][1];
        chosen[0] = (unsigned char)cases[i][2];
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            1, 1, 0, 0, 1, 1, 2, 1, 0, SIXEL_6DELTA_ERROR_SKIP);
        dither->pipeline_6delta_rgb101 = cases[i][3];
        keep = sixel_dither_pipeline_6delta_try_keep_after_lookup(dither,
            0, 0, 0, source, chosen, 0, 0, &retained, &key);
        if (keep != cases[i][4] ||
                (keep && (retained != history || key != 2)) ||
                (!keep && (retained != NULL || key != -1)) ||
                source[0] != cases[i][0] || chosen[0] != cases[i][2]) {
            fprintf(stderr, "byte compare case=%d keep=%d\n", i, keep);
            goto end;
        }
    }
    ok = 1;
end:
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* The output hint belongs to one encode call. Reuse the same dither through
 * a different output contract, then a normalization failure and recovery. */
int
test_delta_byte_scope(int mode)
{
    unsigned char history[3] = {33, 33, 33};
    unsigned char valid[1] = {1};
    unsigned char pixels[3];
    unsigned char const *mask;
    sixel_palette_float32_entries_request_t request;
    float entries[9] = {32.0f / 255, 32.0f / 255, 32.0f / 255,
        200.0f / 255, 200.0f / 255, 200.0f / 255, 0, 0, 0};
    sixel_dither_t *dither;
    sixel_output_t *output;
    db_stream_t stream;
    SIXELSTATUS status;
    size_t size;
    int pass;
    int ok;

    dither = NULL;
    output = NULL;
    ok = 0;
    memset(&stream, 0, sizeof(stream));
    if (!db_setup(&dither, &output, &stream, SIXEL_DIFFUSE_NONE)) {
        goto end;
    }
    for (pass = 0; pass < 4; ++pass) {
        stream.size = 0;
        memset(pixels, 32, sizeof(pixels));
        sixel_output_set_palette_type(output,
            pass == 1 && mode == 0 ? SIXEL_PALETTETYPE_HLS
                : SIXEL_PALETTETYPE_RGB);
        sixel_output_set_ormode(output, pass == 1 && mode == 1);
        sixel_dither_set_body_only(dither, pass == 1 && mode == 2);
        if (SIXEL_FAILED(sixel_output_set_encoder_format(output,
                SIXEL_PIXELFORMAT_RGB888,
                pass == 1 && mode == 3 ? SIXEL_COLORSPACE_LINEAR
                    : SIXEL_COLORSPACE_GAMMA,
                pass == 1 && mode == 4 ? SIXEL_COLORSPACE_LINEAR
                    : SIXEL_COLORSPACE_GAMMA))) {
            goto end;
        }
        memset(&request, 0, sizeof(request));
        if (pass == 1 && mode == 5) {
            request.entries = entries;
            request.colors = 3;
            request.depth = 3 * sizeof(float);
        }
        if (SIXEL_FAILED(dither->palette->vtbl->init_entries_float32(
                dither->palette, &request))) {
            goto end;
        }
        /* 0x7fffffff is outside every supported source pixel format. */
        sixel_dither_set_pixelformat(dither, pass == 2 ? 0x7fffffff
            : SIXEL_PIXELFORMAT_RGB888);
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            1, 1, 0, 0, 1, 1, 2, 1, 0, SIXEL_6DELTA_ERROR_SKIP);
        status = sixel_encode(pixels, 1, 1, 1, dither, output);
        if (dither->pipeline_6delta_rgb101 ||
                dither->pipeline_6delta_hls_bytes != NULL ||
                dither->pipeline_6delta_hls_count != 0U) {
            goto end;
        }
        if (pass == 2) {
            if (SIXEL_SUCCEEDED(status)) {
                goto end;
            }
            continue;
        }
        mask = sixel_dither_get_pipeline_accumulation_result_mask(dither,
            &size);
        if (SIXEL_FAILED(status) || mask == NULL || size != 1 ||
                mask[0] != (pass != 1 || mode == 0)) {
            fprintf(stderr, "byte scope=%d pass=%d status=%x mask=%d\n",
                mode, pass, status, mask == NULL ? -1 : mask[0]);
            goto end;
        }
    }
    ok = 1;
end:
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Real generated byte frames: first paint, identical keep, then black paint.
 * Decoder masks establish the omission actually reached serialized output. */
int
test_delta_byte_repeat(int workers)
{
    static struct { int flag; char const *value; } const options[] = {
        {'~', "none"}, {'d', "none"}, {'0', "hard"}, {'_', "none"},
        {'.', "8bit"}, {'W', "gamma"}, {'U', "gamma"}, {'t', "rgb"},
        {'Z', "delta"}, {'7', NULL}, {'Q', "heckbert"}, {'F', "none"},
        {'4', "full-frame"}, {'5', "none"}, {'p', "4"}, {'a', "off"}
    };
    unsigned char pixels[256 * 25 * 3];
    unsigned char *decoded;
    unsigned char *paint;
    unsigned char *palette;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_dither_t *dither;
    db_stream_t stream;
    unsigned int flags;
    int width;
    int height;
    int colors;
    int rows;
    int pass;
    int i;
    int channel;
    int want;
    int ok;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    decoded = paint = palette = NULL;
    ok = 0;
    pass = -1;
    rows = workers ? 25 : 1;
    memset(&stream, 0, sizeof(stream));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, db_write, &stream, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '=',
        workers ? "greedy:B4" : "greedy:B1"))) {
        goto end;
    }
    for (pass = 0; pass < 3; ++pass) {
        stream.size = 0;
        for (i = 0; i < 256 * rows * 3; ++i) {
            pixels[i] = pass == 2 ? 0 : (unsigned char)(32 + (i / 3 % 3) * 64);
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels,
                    256, rows, SIXEL_PIXELFORMAT_RGB888, NULL, -1)) ||
                SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame,
                    output))) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette ||
                dither->pipeline_6delta_rgb101
                || !encoder->accumulation_valid ||
                (workers && dither->pipeline_last_encode_threads < 2)) {
            goto end;
        }
        if (SIXEL_FAILED(sixel_decode_raw_with_options_mask(
                (unsigned char *)stream.data, (int)stream.size, 0,
                &decoded, &paint, &width, &height, &palette, &colors,
                &flags, encoder->allocator)) || width != 256 ||
                height != rows || paint == NULL) {
            goto end;
        }
        for (i = 0; i < 256 * rows; ++i) {
            want = pass == 2 ? 0 : 33 + (i % 3) * 64;
            /* The decoder mask expresses occupancy as zero/nonzero. */
            if ((paint[i] != 0) != (pass != 1)) {
                fprintf(stderr, "byte repeat worker=%d pass=%d pixel=%d "
                    "paint=%u\n", workers, pass, i, paint[i]);
                goto end;
            }
            for (channel = 0; channel < 3; ++channel) {
                if (encoder->accumulation_pixels[i * 3 + channel] != want ||
                        (paint[i] && (decoded[i] >= colors ||
                         palette[decoded[i] * 3 + channel] != want))) {
                    fprintf(stderr, "byte repeat worker=%d pass=%d "
                        "pixel=%d channel=%d want=%d\n", workers, pass,
                        i, channel, want);
                    goto end;
                }
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
    if (!ok) {
        fprintf(stderr, "byte repeat worker=%d pass=%d: %s\n", workers,
            pass, sixel_helper_get_additional_message());
    }
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


/* Rational HSL references: (330,45,73), (97,47,52), (251,50,48).
 * Their RGB percentage reconstructions are (12,45,78), (71,23,41),
 * (26,74,35). No production transform supplies these expected values. */
static unsigned char const bh_source[3][3] = {
    {31, 116, 201}, {181, 57, 105}, {66, 189, 89}
};
static unsigned char const bh_wire[3][3] = {
    {31, 115, 199}, {181, 59, 105}, {66, 189, 89}
};

/* First-frame capture and subsequent selected-candidate tie must agree.
 * Skip isolates that contract from each method's residual propagation. */
int
test_delta_byte_hls_method(int method)
{
    unsigned char pixels[4 * 3 * 3];
    unsigned char history[sizeof(pixels)];
    unsigned char valid[sizeof(pixels) / 3];
    unsigned char palette[9] = {31, 116, 201, 255, 255, 255, 0, 0, 0};
    unsigned char const *mask;
    unsigned char const *rgb;
    sixel_palette_float32_entries_view_t floats;
    sixel_dither_t *dither;
    sixel_output_t *output;
    db_stream_t stream;
    size_t size;
    size_t rgb_size;
    int pass;
    int i;
    int ok;

    dither = NULL;
    output = NULL;
    ok = 0;
    memset(&stream, 0, sizeof(stream));
    memset(valid, 1, sizeof(valid));
    if (!db_setup(&dither, &output, &stream, method)) {
        goto end;
    }
    sixel_dither_set_palette(dither, palette);
    sixel_output_set_palette_type(output, SIXEL_PALETTETYPE_HLS);
    for (pass = 0; pass < 2; ++pass) {
        stream.size = 0;
        for (i = 0; i < 12; ++i) {
            memcpy(pixels + i * 3, bh_source[0], 3);
            memcpy(history + i * 3, bh_wire[0], 3);
        }
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            4, 3, 0, 0, 4, 3, 2, pass, 0, SIXEL_6DELTA_ERROR_SKIP);
        if (SIXEL_FAILED(sixel_encode(pixels, 4, 3, 1, dither, output)) ||
                dither->pipeline_6delta_hls_bytes != NULL ||
                dither->pipeline_6delta_hls_count != 0U ||
                dither->use_rgb101_palette ||
                SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &floats)) || floats.entries != NULL) {
            goto end;
        }
        mask = sixel_dither_get_pipeline_accumulation_result_mask(dither,
            &size);
        rgb = sixel_dither_get_pipeline_accumulation_result_rgb(dither,
            &rgb_size);
        if (mask == NULL || size != 12 || rgb == NULL || rgb_size != 36 ||
                strstr(stream.data, ";1;330;45;73") == NULL) {
            goto end;
        }
        for (i = 0; i < 12; ++i) {
            if (mask[i] != pass ||
                    (!pass && memcmp(rgb + i * 3, bh_wire[0], 3))) {
                fprintf(stderr, "byte HLS method=%d pass=%d pixel=%d\n",
                    method, pass, i);
                goto end;
            }
        }
    }
    ok = 1;
end:
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Exact integer distances expose both false repaint and false keep. */
int
test_delta_byte_hls_compare(int repaint)
{
    unsigned char valid = 1;
    unsigned char const *retained;
    unsigned char const *query;
    unsigned char const *history;
    sixel_dither_t *dither;
    int key;
    int keep;
    int ok;

    dither = NULL;
    ok = 0;
    query = repaint ? bh_wire[0] : bh_source[0];
    history = repaint ? bh_source[0] : bh_wire[0];
    if (SIXEL_FAILED(sixel_dither_new(&dither, 3, NULL))) {
        goto end;
    }
    sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
        history, 3, &valid, 1, 1, 1, 0, 0, 1, 1, 2, 1, 0,
        SIXEL_6DELTA_ERROR_SKIP);
    dither->pipeline_6delta_hls_bytes = bh_wire[0];
    dither->pipeline_6delta_hls_count = 1;
    keep = sixel_dither_pipeline_6delta_try_keep_after_lookup(dither,
        0, 0, 0, query, bh_source[0], 0, 0, &retained, &key);
    ok = keep == !repaint && (keep ? retained == history && key == 2
        : retained == NULL && key == -1);
end:
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Real generated frames exercise both precision directions without resetting
 * history. An independent composite verifies serialized paints and omissions.
 * Modes: serial, workers, byte->float->byte, float->byte->float, source mask,
 * partial-plane origin, and invalidation. */
int
test_delta_byte_hls_frames(int mode)
{
    static struct { int flag; char const *value; } const options[] = {
        {'~', "none"}, {'d', "none"}, {'0', "hard"}, {'_', "none"},
        {'W', "gamma"}, {'U', "gamma"}, {'t', "hls"}, {'Z', "delta"},
        {'7', NULL}, {'Q', "heckbert"}, {'F', "none"}, {'p', "4"},
        {'4', "full-frame"}, {'5', "none"}, {'a', "off"}
    };
    unsigned char bytes[256 * 25 * 3];
    float pixels[256 * 25 * 3];
    unsigned char retained[257 * 25 * 3];
    unsigned char valid[257 * 25];
    unsigned char *mask;
    unsigned char *decoded;
    unsigned char *paint;
    unsigned char *palette;
    sixel_frame_transparency_t transparency;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_dither_t *dither;
    db_stream_t stream;
    unsigned int flags;
    int width;
    int height;
    int colors;
    int rows;
    int plane_width;
    int origin;
    int plane;
    int hole;
    int entry;
    int native;
    int pass;
    int i;
    int c;
    int keep;
    int ok;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    mask = decoded = paint = palette = NULL;
    ok = 0;
    pass = -1;
    rows = mode == 1 ? 25 : 1;
    plane_width = mode == 5 ? 257 : 256;
    memset(&stream, 0, sizeof(stream));
    memset(retained, 0, sizeof(retained));
    memset(valid, 0, sizeof(valid));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, db_write, &stream, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '=',
            mode == 1 ? "greedy:B4" : "greedy:B1")) ||
            SIXEL_FAILED(sixel_encoder_set_6delta_plane_size(encoder,
                plane_width, rows))) {
        goto end;
    }
    for (pass = 0; pass < 4; ++pass) {
        native = (mode == 2 && pass == 1) || (mode == 3 && pass != 1);
        origin = mode == 5 && pass > 0;
        stream.size = 0;
        if (mode == 6 && pass == 2) {
            sixel_encoder_invalidate_6delta_plane(encoder);
            memset(valid, 0, sizeof(valid));
        }
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                native ? "float32" : "8bit")) ||
                SIXEL_FAILED(sixel_encoder_set_6delta_plane_origin(encoder,
                    origin, 0))) {
            goto end;
        }
        for (i = 0; i < 256 * rows; ++i) {
            entry = ((i % 256 + origin) + (pass == 3)) % 3;
            for (c = 0; c < 3; ++c) {
                bytes[i * 3 + c] = bh_source[entry][c];
                pixels[i * 3 + c] = bytes[i * 3 + c] / 255.0f;
            }
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    native ? (unsigned char *)pixels : bytes,
                    256, rows, native ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
                        SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
            goto end;
        }
        if (mode == 4 && pass < 2) {
            mask = calloc((size_t)(256 * rows), 1);
            if (mask == NULL) {
                goto end;
            }
            mask[pass == 0 ? 1 : 0] = 1;
            memset(&transparency, 0, sizeof(transparency));
            transparency.transparent = -1;
            transparency.alpha_zero_is_transparent = 1;
            transparency.transparent_mask = mask;
            transparency.transparent_mask_size = (size_t)(256 * rows);
            if (SIXEL_FAILED(((sixel_frame_interface_t *)frame)->vtbl->
                    set_transparency((sixel_frame_interface_t *)frame,
                        &transparency))) {
                goto end;
            }
            mask = NULL;
        }
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != native ||
                dither->pipeline_6delta_hls_bytes != NULL ||
                dither->pipeline_6delta_hls_count != 0 ||
                (mode == 1 && dither->pipeline_last_encode_threads < 2)) {
            goto end;
        }
        if (SIXEL_FAILED(sixel_decode_raw_with_options_mask(
                (unsigned char *)stream.data, (int)stream.size, 0,
                &decoded, &paint, &width, &height, &palette, &colors,
                &flags, encoder->allocator)) || width != 256 ||
                height != rows || paint == NULL) {
            goto end;
        }
        for (i = 0; i < 256 * rows; ++i) {
            entry = ((i % 256 + origin) + (pass == 3)) % 3;
            plane = i / 256 * plane_width + i % 256 + origin;
            hole = mode == 4 && pass < 2 && i == (pass == 0 ? 1 : 0);
            keep = valid[plane] &&
                !memcmp(retained + plane * 3, bh_wire[entry], 3);
            if ((paint[i] != 0) != !(hole || keep)) {
                fprintf(stderr, "byte HLS mode=%d pass=%d i=%d paint=%u\n",
                    mode, pass, i, paint[i]);
                goto end;
            }
            if (!hole && !keep) {
                if (decoded[i] >= colors ||
                        memcmp(palette + decoded[i] * 3, bh_wire[entry], 3)) {
                    fprintf(stderr, "byte HLS wire mode=%d pass=%d i=%d\n",
                        mode, pass, i);
                    goto end;
                }
                memcpy(retained + plane * 3, bh_wire[entry], 3);
                valid[plane] = 1;
            }
        }
        if (encoder->accumulation_pixels_size !=
                (size_t)(plane_width * rows * 3) ||
                memcmp(encoder->accumulation_pixels, retained,
                    (size_t)(plane_width * rows * 3)) ||
                memcmp(encoder->accumulation_valid_mask, valid,
                    (size_t)(plane_width * rows))) {
            fprintf(stderr, "byte HLS history mode=%d pass=%d\n", mode, pass);
            goto end;
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
    if (!ok) {
        fprintf(stderr, "byte HLS frames mode=%d pass=%d: %s\n", mode,
            pass, sixel_helper_get_additional_message());
    }
    if (encoder != NULL) {
        sixel_allocator_free(encoder->allocator, decoded);
        sixel_allocator_free(encoder->allocator, paint);
        sixel_allocator_free(encoder->allocator, palette);
    }
    free(mask);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Failure, independent contexts and explicit output snap own separate tests. */
int
test_delta_byte_hls_lifetime(int mode)
{
    sixel_dither_t *dither[2] = {NULL, NULL};
    sixel_output_t *output[2] = {NULL, NULL};
    db_stream_t stream[2];
    unsigned char table[9] = {0, 0, 0, 255, 255, 255, 0, 0, 0};
    unsigned char pixels[3];
    unsigned char valid = 1;
    unsigned char const snapped[3] = {179, 56, 105};
    unsigned char const *history;
    unsigned char const *mask;
    size_t size;
    SIXELSTATUS status;
    int pass;
    int which;
    int color;
    int ok;

    ok = 0;
    memset(stream, 0, sizeof(stream));
    if (!db_setup(&dither[0], &output[0], &stream[0], SIXEL_DIFFUSE_NONE) ||
            !db_setup(&dither[1], &output[1], &stream[1], SIXEL_DIFFUSE_NONE)) {
        goto end;
    }
    for (pass = 0; pass < 4; ++pass) {
        which = mode == 1 ? pass % 2 : 0;
        color = mode == 1 ? (pass / 2 + which) % 2 : mode == 2;
        history = mode == 2 ? snapped : bh_wire[color];
        memcpy(table, bh_source[color], 3);
        memcpy(pixels, bh_source[color], 3);
        sixel_dither_set_palette(dither[which], table);
        dither[which]->palette_snap_exact = mode == 2;
        sixel_output_set_palette_type(output[which], SIXEL_PALETTETYPE_HLS);
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither[which],
            history, 3, &valid, 1, 1, 1, 0, 0, 1, 1, 2, 1, 0,
            SIXEL_6DELTA_ERROR_SKIP);
        sixel_dither_set_pixelformat(dither[which], mode == 0 && pass == 1
            ? 0x7fffffff : SIXEL_PIXELFORMAT_RGB888);
        stream[which].size = 0;
        status = sixel_encode(pixels, 1, 1, 1, dither[which], output[which]);
        if (dither[which]->pipeline_6delta_hls_bytes != NULL ||
                dither[which]->pipeline_6delta_hls_count != 0) {
            goto end;
        }
        if (mode == 0 && pass == 1) {
            if (SIXEL_SUCCEEDED(status)) {
                goto end;
            }
            continue;
        }
        mask = sixel_dither_get_pipeline_accumulation_result_mask(
            dither[which], &size);
        if (SIXEL_FAILED(status) || mask == NULL || size != 1 || !mask[0]) {
            fprintf(stderr, "byte HLS lifetime mode=%d pass=%d\n", mode, pass);
            goto end;
        }
    }
    ok = 1;
end:
    sixel_output_unref(output[0]);
    sixel_output_unref(output[1]);
    sixel_dither_unref(dither[0]);
    sixel_dither_unref(dither[1]);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Legacy indexed output can retain a higher precision palette. Its final
 * writer view, not the byte shadow, determines captured HLS history. */
int
test_delta_byte_hls_float_palette(int unused)
{
    unsigned char table[9] = {31, 116, 201, 255, 255, 255, 0, 0, 0};
    float values[9] = {.123f, .456f, .789f, 1, 1, 1, 0, 0, 0};
    unsigned char const expected[3] = {31, 117, 204};
    unsigned char index = 0;
    unsigned char const *rgb;
    sixel_palette_float32_entries_request_t request;
    sixel_dither_t *dither;
    sixel_output_t *output;
    db_stream_t stream;
    size_t size;
    int ok;

    (void)unused;
    dither = NULL;
    output = NULL;
    ok = 0;
    memset(&stream, 0, sizeof(stream));
    if (!db_setup(&dither, &output, &stream, SIXEL_DIFFUSE_NONE)) {
        goto end;
    }
    sixel_dither_set_palette(dither, table);
    memset(&request, 0, sizeof(request));
    request.entries = values;
    request.colors = 3;
    request.depth = 3 * sizeof(float);
    if (SIXEL_FAILED(dither->palette->vtbl->init_entries_float32(
            dither->palette, &request))) {
        goto end;
    }
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_PAL8);
    sixel_output_set_palette_type(output, SIXEL_PALETTETYPE_HLS);
    if (SIXEL_FAILED(sixel_encode(&index, 1, 1, 1, dither, output))) {
        goto end;
    }
    rgb = sixel_dither_get_pipeline_accumulation_result_rgb(dither, &size);
    ok = rgb != NULL && size == 3 && !memcmp(rgb, expected, 3) &&
        strstr(stream.data, ";1;330;46;73") != NULL;
end:
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set ts=4 sts=4 sw=4 et: */
