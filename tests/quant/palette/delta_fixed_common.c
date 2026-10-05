/* SPDX-License-Identifier: MIT */
/* Fixed delta preserves real slots and uses a separate omission mask. */
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
#include "src/sixel_decode_pixels.h"

typedef struct df_stream {
    char data[65536];
    size_t size;
} df_stream_t;

static int
df_write(char *data, int size, void *context)
{
    df_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size >= sizeof(stream->data) - stream->size) {
        return -1;
    }
    memcpy(stream->data + stream->size, data, (size_t)size);
    stream->size += (size_t)size;
    stream->data[stream->size] = '\0';
    return size;
}

/* Full gray ramps and the duplicate four-entry PAL fixture have literal
 * sRGB8 contracts. Work fractions allow 2e-7 float error; RGB101 boundaries
 * are at least 1/510 code unit away from any supplied integer gray. */
static int
df_component(int entry, int channel, int duplicates)
{
    static unsigned char const table[4][3] = {
        {88, 88, 88}, {94, 94, 94}, {88, 88, 88}, {255, 0, 0}
    };

    return duplicates ? table[entry][channel] : entry;
}

static int
df_wire(int value)
{
    return (((value * 100 + 127) / 255) * 255 + 50) / 100;
}

int
test_delta_fixed_frames(int mode, int duplicates, char const *map)
{
    static struct { int flag; char const *value; } const options[] = {
        {'=', "1"}, {'~', "none"}, {'d', "none"}, {'0', "none"},
        {'_', "none"}, {'.', "float32"}, {'W', "gamma"}, {'U', "gamma"},
        {'t', "rgb"}, {'Z', "delta"}, {'7', NULL},
        {'Q', "heckbert"}, {'F', "none"}, {'4', "full-frame"}, {'5', "none"}
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_dither_t *cached;
    sixel_frame_transparency_t transparency;
    sixel_palette_float32_entries_view_t view;
    df_stream_t stream;
    float pixels[256 * 25 * 3];
    unsigned char bytes[256 * 25 * 3];
    unsigned char retained[257 * 25 * 3];
    unsigned char valid[257 * 25];
    unsigned char omitted[256 * 25];
    unsigned char seen[256];
    unsigned char *mask;
    unsigned char *decoded;
    unsigned char *paint;
    unsigned char *palette;
    unsigned int flags;
    int width;
    int height;
    int ncolors;
    int colors;
    int rows;
    int count;
    int plane_width;
    int plane_count;
    int origin;
    int pass;
    int i;
    int c;
    int entry;
    int target;
    int plane;
    int hole;
    int native;
    int keep;
    int any_omitted;
    int ok;
    double want;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    cached = NULL;
    mask = decoded = paint = palette = NULL;
    ok = 0;
    pass = 0;
    colors = duplicates ? 4 : 256;
    rows = mode == 3 ? 25 : 1;
    count = colors * rows;
    plane_width = colors + (mode == 2);
    plane_count = plane_width * rows;
    memset(retained, 0, sizeof(retained));
    memset(valid, 0, sizeof(valid));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, df_write, &stream, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, map ? 'm' : 'b',
            map ? map : "gray8")) ||
            (mode == 3 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '=', "4"))) ||
            (mode == 2 && SIXEL_FAILED(sixel_encoder_set_6delta_plane_size(
                encoder, plane_width, rows)))) {
        goto end;
    }
    /* Keep the animation cache, but do not send cursor controls to stdout. */
    encoder->fstatic = 1;
    for (pass = 0; pass < 6; ++pass) {
        native = mode != 5 && !(mode == 6 && pass == 1);
        origin = mode == 2 && pass > 0;
        if (((pass == 0 || (mode == 6 && pass < 3)) &&
                SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                    native ? "float32" : "8bit"))) ||
                (mode == 2 && SIXEL_FAILED(
                    sixel_encoder_set_6delta_plane_origin(encoder,
                        origin, 0)))) {
            goto end;
        }
        if (pass == 2) {
            sixel_encoder_invalidate_6delta_plane(encoder);
            memset(valid, 0, sizeof(valid));
        }
        if (pass >= 4) {
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Z',
                    pass == 4 ? "full" : "delta"))) {
                goto end;
            }
            memset(valid, 0, sizeof(valid));
        }
        any_omitted = 0;
        for (i = 0; i < count; ++i) {
            entry = (i % colors + origin) % colors;
            target = pass < 3 ? entry : duplicates ?
                (entry == 3 ? 0 : 3) : (entry < 128 ? 255 : 0);
            for (c = 0; c < 3; ++c) {
                bytes[i * 3 + c] =
                    (unsigned char)df_component(target, c, duplicates);
                pixels[i * 3 + c] = bytes[i * 3 + c] / 255.0f;
            }
            plane = (i / colors) * plane_width + origin + i % colors;
            hole = mode == 1 && pass < 3 &&
                i == (pass == 1 ? 0 : 1);
            /* Repeat/shift cases retain identical emitted RGB. Changed
             * frames select different endpoints, well away from ties. */
            keep = native && pass != 4 && valid[plane];
            for (c = 0; c < 3; ++c) {
                keep = keep && retained[plane * 3 + c] ==
                    df_wire(bytes[i * 3 + c]);
            }
            omitted[i] = (unsigned char)(hole || keep);
            any_omitted |= omitted[i];
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    native ? (unsigned char *)pixels : bytes, colors, rows,
                    native ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
                        SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
            goto end;
        }
        if (mode == 4 || mode == 6) {
            sixel_frame_set_multiframe(frame, 1);
        }
        if (mode == 1 && pass < 3) {
            mask = calloc((size_t)count, 1);
            if (mask == NULL) {
                goto end;
            }
            mask[pass == 1 ? 0 : 1] = 1;
            memset(&transparency, 0, sizeof(transparency));
            transparency.transparent = -1;
            transparency.alpha_zero_is_transparent = 1;
            transparency.transparent_mask = mask;
            transparency.transparent_mask_size = (size_t)count;
            if (SIXEL_FAILED(((sixel_frame_interface_t *)frame)->vtbl->
                    set_transparency((sixel_frame_interface_t *)frame,
                        &transparency))) {
                goto end;
            }
            mask = NULL;
        }
        memset(&stream, 0, sizeof(stream));
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
            goto end;
        }
        if (any_omitted && strstr(stream.data, "\033P0;1q") == NULL) {
            fprintf(stderr, "fixed omission requires P2=1\n");
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != native ||
                dither->keycolor != -1 || dither->keycolor_reserved ||
                (mode == 3 && dither->pipeline_last_encode_threads < 2)) {
            fprintf(stderr, "fixed state native=%d key=%d reserved=%d\n",
                dither == NULL ? -1 : dither->use_rgb101_palette,
                dither == NULL ? -2 : dither->keycolor,
                dither == NULL ? -1 : dither->keycolor_reserved);
            goto end;
        }
        if (mode == 4 && pass < 3) {
            if (pass != 0 && (dither != cached ||
                    encoder->fixed_palette_source == NULL)) {
                goto end;
            }
            cached = dither;
        }
        if (native) {
            if (dither->ncolors != colors ||
                    SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                        dither->palette, &view)) || view.entries == NULL ||
                    view.entry_count != (unsigned int)colors) {
                goto end;
            }
            memset(seen, 0, sizeof(seen));
            for (i = 0; i < colors * 3; ++i) {
                if (!isfinite(view.entries[i])) {
                    goto end;
                }
                entry = mode == 7 ? (int)(view.entries[i] * 255.0f + .5f)
                    : i / 3;
                if (entry < 0 || entry >= colors ||
                        (mode == 7 && i % 3 == 0 && seen[entry]++)) {
                    goto end;
                }
                want = df_component(entry, i % 3, duplicates) / 255.0;
                if (!isfinite(view.entries[i]) ||
                        fabs(view.entries[i] - want) > 2e-7) {
                    goto end;
                }
            }
        }
        if (SIXEL_FAILED(sixel_decode_raw_with_options_mask(
                (unsigned char *)stream.data, (int)stream.size, 0,
                &decoded, &paint, &width, &height, &palette, &ncolors,
                &flags, encoder->allocator)) || width != colors ||
                height != rows || paint == NULL) {
            goto end;
        }
        for (i = 0; i < count; ++i) {
            if ((paint[i] == 0) != omitted[i]) {
                fprintf(stderr, "fixed pixel %d paint=%u omitted=%u\n",
                    i, paint[i], omitted[i]);
                goto end;
            }
            plane = (i / colors) * plane_width + origin + i % colors;
            if (omitted[i]) {
                continue;
            }
            for (c = 0; c < 3; ++c) {
                if (palette[decoded[i] * 3 + c] !=
                        df_wire(bytes[i * 3 + c])) {
                    fprintf(stderr, "fixed rgb pixel=%d channel=%d got=%d "
                        "want=%d\n", i, c, palette[decoded[i] * 3 + c],
                        df_wire(bytes[i * 3 + c]));
                    goto end;
                }
                retained[plane * 3 + c] =
                    (unsigned char)df_wire(bytes[i * 3 + c]);
            }
            valid[plane] = 1;
        }
        if (pass != 4) {
            if (!encoder->accumulation_valid ||
                    memcmp(encoder->accumulation_valid_mask, valid,
                        (size_t)plane_count) != 0) {
                fprintf(stderr, "fixed history validity mismatch\n");
                goto end;
            }
            /* Native history records wire RGB; the legacy byte control
             * retains its unsnapped table bytes. Precision-switch cases
             * invalidate before returning to native comparisons. */
            for (i = 0; i < plane_count; ++i) {
                for (c = 0; valid[i] && c < 3; ++c) {
                    want = native ? retained[i * 3 + c] : bytes[i * 3 + c];
                    if (encoder->accumulation_pixels[i * 3 + c] != want) {
                        fprintf(stderr, "fixed history RGB pixel=%d "
                            "channel=%d got=%d want=%.0f\n", i, c,
                            encoder->accumulation_pixels[i * 3 + c], want);
                        goto end;
                    }
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
        fprintf(stderr, "fixed delta mode=%d map=%d pass=%d: %s\n",
            mode, map != NULL, pass, sixel_helper_get_additional_message());
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

/* After gray2 painted 67% gray, gray1 must be able to retain that displayed
 * color even though it is no longer in the palette. Its negative FS residual
 * changes the next .51 sample to black in diffuse mode; skip leaves white. */
int
test_delta_fixed_residual(int skip)
{
    static struct { int flag; char const *value; } const options[] = {
        {'=', "1"}, {'~', "none"}, {'d', "none"}, {'0', "none"},
        {'_', "none"}, {'.', "float32"}, {'W', "gamma"}, {'U', "gamma"},
        {'t', "rgb"}, {'Z', "delta"}, {'b', "gray2"}, {'7', NULL}
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_frame_transparency_t transparency;
    sixel_dither_t *dither;
    df_stream_t stream;
    float pixels[6];
    unsigned char *mask;
    unsigned char *decoded;
    unsigned char *paint;
    unsigned char *palette;
    unsigned int flags;
    int width;
    int height;
    int colors;
    int pass;
    int i;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    mask = decoded = paint = palette = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, df_write, &stream, NULL))) {
        goto end;
    }
    for (i = 0; i < (int)(sizeof(options) / sizeof(options[0])); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[i].flag,
                options[i].value))) {
            goto end;
        }
    }
    for (pass = 0; pass < 2; ++pass) {
        for (i = 0; i < 6; ++i) {
            pixels[i] = pass ? (i < 3 ? .6f : .51f) : 2.0f / 3.0f;
        }
        if (pass && (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'b',
                "gray1")) || SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'd', "fs")) || SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'Z', skip ? "delta:error=skip" : "delta:error=diffuse")))) {
            goto end;
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    (unsigned char *)pixels, 2, 1,
                    SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
            goto end;
        }
        if (!pass) {
            mask = calloc(2, 1);
            if (mask == NULL) {
                goto end;
            }
            mask[1] = 1;
            memset(&transparency, 0, sizeof(transparency));
            transparency.transparent = -1;
            transparency.alpha_zero_is_transparent = 1;
            transparency.transparent_mask = mask;
            transparency.transparent_mask_size = 2;
            if (SIXEL_FAILED(((sixel_frame_interface_t *)frame)->vtbl->
                    set_transparency((sixel_frame_interface_t *)frame,
                        &transparency))) {
                goto end;
            }
            mask = NULL;
        }
        memset(&stream, 0, sizeof(stream));
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output))) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    dither = (sixel_dither_t *)encoder->diagnostic_dither;
    if (dither == NULL || !dither->use_rgb101_palette ||
            dither->keycolor != -1 || dither->ncolors != 2 ||
            strstr(stream.data, "\033P0;1q") == NULL ||
            SIXEL_FAILED(sixel_decode_raw_with_options_mask(
                (unsigned char *)stream.data, (int)stream.size, 0,
                &decoded, &paint, &width, &height, &palette, &colors,
                &flags, encoder->allocator)) || width != 2 || height != 1 ||
            paint == NULL || paint[0] != 0 || paint[1] == 0 ||
            decoded[1] != (skip ? 1 : 0) ||
            encoder->accumulation_pixels[0] != 171 ||
            encoder->accumulation_pixels[3] != (skip ? 255 : 0)) {
        goto end;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "fixed FS residual skip=%d: %s\n", skip,
            sixel_helper_get_additional_message());
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

static int df_fail_mask;
static int df_mask_failures;

static void *
df_realloc(void *pointer, size_t size)
{
    if (df_fail_mask && size == 3) {
        ++df_mask_failures;
        return NULL;
    }
    return realloc(pointer, size);
}

/* The writer must never borrow stale mask contents after failed preparation,
 * disabled capture or a missing retained plane. Recovery reuses storage. */
int
test_delta_fixed_mask_failure(void)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    unsigned char history[9] = {0};
    unsigned char valid[3] = {1, 1, 1};
    unsigned char source_mask[3] = {0, 1, 0};
    unsigned char *saved;
    int ok;

    allocator = NULL;
    dither = NULL;
    ok = 0;
    df_fail_mask = df_mask_failures = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                df_realloc, NULL)) ||
            SIXEL_FAILED(sixel_dither_new(&dither, 2, allocator))) {
        goto end;
    }
    sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
        history, sizeof(history), valid, sizeof(valid),
        3, 1, 0, 0, 3, 1, 0, 1, 0, SIXEL_6DELTA_ERROR_SKIP);
    sixel_dither_set_pipeline_accumulation_result_enabled(dither, 1);
    df_fail_mask = 1;
    if (sixel_dither_prepare_delta_mask(dither, source_mask, 3, 3) !=
            SIXEL_BAD_ALLOCATION || df_mask_failures != 1 ||
            dither->pipeline_accumulation_result_mask_size != 0) {
        goto end;
    }
    df_fail_mask = 0;
    if (SIXEL_FAILED(sixel_dither_prepare_delta_mask(dither, source_mask,
            3, 3)) || dither->pipeline_accumulation_result_mask_size != 3 ||
            memcmp(dither->pipeline_accumulation_result_mask,
                source_mask, 3) != 0) {
        goto end;
    }
    saved = dither->pipeline_accumulation_result_mask;
    if (SIXEL_FAILED(sixel_dither_prepare_delta_mask(dither, NULL, 0, 3)) ||
            dither->pipeline_accumulation_result_mask != saved ||
            memcmp(saved, history, 3) != 0) {
        goto end;
    }
    sixel_dither_set_pipeline_accumulation_result_enabled(dither, 0);
    if (SIXEL_FAILED(sixel_dither_prepare_delta_mask(dither, source_mask,
            3, 3)) || dither->pipeline_accumulation_result_mask_size != 0) {
        goto end;
    }
    sixel_dither_set_pipeline_accumulation_result_enabled(dither, 1);
    sixel_dither_clear_pipeline_accumulation_buffer_hint(dither);
    if (SIXEL_FAILED(sixel_dither_prepare_delta_mask(dither, source_mask,
            3, 3)) || dither->pipeline_accumulation_result_mask_size != 0) {
        goto end;
    }
    ok = 1;
end:
    df_fail_mask = 0;
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
