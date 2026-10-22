/*
 * SPDX-License-Identifier: MIT
 * Compare the entire ordered sample, including repeated solid weights and
 * transparency, across 1, 2, 4 and 8 threads. At each encoder budget,
 * compare serial and parallel extraction with identical downstream work.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/compat_stub.h"
#include "src/filter-factory.h"
#include "src/filter-sample.h"
#include "src/frame.h"
#include "src/options.h"
#include <stdint.h>
#include "src/threading.h"

#define SAMPLE_WIDTH 257
#define SAMPLE_HEIGHT 193
#define SAMPLE_PIXELS (SAMPLE_WIDTH * SAMPLE_HEIGHT)

typedef struct sample_output {
    unsigned char *data;
    size_t size;
} sample_output_t;

static int
sample_write(char *data, int size, void *priv)
{
    sample_output_t *out;
    unsigned char *grown;

    out = (sample_output_t *)priv;
    grown = (unsigned char *)realloc(out->data, out->size + (size_t)size);
    if (grown == NULL) {
        return -1;
    }
    out->data = grown;
    memcpy(out->data + out->size, data, (size_t)size);
    out->size += (size_t)size;
    return size;
}

static int
sample_case(int format, int clipped, int mode)
{
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_filter_t *filter;
    sixel_sample_stream_t samples;
    sixel_frame_interface_t *iface;
    sixel_frame_transparency_t transparency;
    sixel_filter_sample_config_t config;
    sixel_thread_budget_scope_t scope;
    sixel_runtime_policy_options_t runtime;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sample_output_t encoded;
    sample_output_t reference;
    unsigned char *expected;
    unsigned char *expected_mask;
    unsigned char *pixels;
    unsigned char *working_pixels;
    unsigned char palette[768];
    unsigned char working_palette[768];
    unsigned char *mask;
    float *floats;
    char thread_text[8];
    char target_text[40];
    size_t expected_size;
    size_t size;
    int expected_width;
    int expected_height;
    int pixel_format;
    int thread;
    int attempt;
    int i;
    int c;
    int value;
    int ok;
    SIXELSTATUS status;

    allocator = NULL;
    frame = NULL;
    filter = NULL;
    encoder = NULL;
    output = NULL;
    expected = NULL;
    expected_mask = NULL;
    expected_size = 0;
    expected_width = 0;
    expected_height = 0;
    memset(&encoded, 0, sizeof(encoded));
    memset(&reference, 0, sizeof(reference));
    sixel_sample_stream_init(&samples);
    working_pixels = (unsigned char *)malloc(
        SAMPLE_PIXELS * 3u * sizeof(float));
    pixels = (unsigned char *)malloc(SAMPLE_PIXELS * 3u);
    floats = (float *)malloc(SAMPLE_PIXELS * 3u * sizeof(float));
    mask = (unsigned char *)malloc(SAMPLE_PIXELS);
    ok = 0;
    if (working_pixels == NULL || pixels == NULL ||
            floats == NULL || mask == NULL ||
            SIXEL_FAILED(sixel_allocator_new(&allocator,
                NULL, NULL, NULL, NULL))) {
        goto end;
    }
    for (i = 0; i < 256; ++i) {
        palette[i * 3] = (unsigned char)i;
        palette[i * 3 + 1] = (unsigned char)(i * 37);
        palette[i * 3 + 2] = (unsigned char)(i * 71);
    }
    /* Wide solid strips mixed with noise exercise both histogram reduction
     * and repeated-color mass; the mask crosses job and clip boundaries. */
    for (i = 0; i < SAMPLE_PIXELS; ++i) {
        value = i / SAMPLE_WIDTH % 19 < 4 ? i / SAMPLE_WIDTH : i * 53;
        mask[i] = (unsigned char)(i % 101 == 0);
        for (c = 0; c < 3; ++c) {
            pixels[i * 3 + c] = palette[(value & 255) * 3 + c];
            floats[i * 3 + c] = (float)pixels[i * 3 + c] / 255.0f;
        }
    }
    if (format == 1) {
        for (i = 0; i < SAMPLE_PIXELS; ++i) {
            pixels[i] = (unsigned char)(i / SAMPLE_WIDTH % 19 < 4
                ? i / SAMPLE_WIDTH : i * 53);
        }
    }
    pixel_format = format == 0 ? SIXEL_PIXELFORMAT_RGB888
        : format == 1 ? SIXEL_PIXELFORMAT_PAL8 : SIXEL_PIXELFORMAT_RGBFLOAT32;
    memset(&config, 0, sizeof(config));
    config.policy = SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID;
    config.source = SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME;
    /* At 64 colors this raster uses different strides for auto, high and
     * full (targets 4096, 8192 and 16384), including the clipped cases. */
    config.reqcolors = 64;
    config.quality_mode = mode == 1 ? SIXEL_QUALITY_HIGH
        : mode == 2 ? SIXEL_QUALITY_FULL : SIXEL_QUALITY_AUTO;
    config.palette_sample_override = mode >= 3;
    config.palette_sample_target = mode == 4 ? 100000u : 257u;
    if (clipped) {
        config.clip_x = 7;
        config.clip_y = 5;
        config.clip_width = 231;
        config.clip_height = 179;
    }
    for (attempt = 0; attempt < 8; ++attempt) {
        thread = 1 << (attempt / 2);
        memset(&runtime, 0, sizeof(runtime));
        runtime.sample_parallel_min_pixels_override = 1;
        runtime.sample_parallel_min_pixels = 1;
        sixel_runtime_policy_store(&runtime);
        status = sixel_frame_new(&frame, allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        /* Encoding may crop borrowed storage in place. Every budget must
         * start with the same writable pixels and palette. */
        memcpy(working_pixels, format == 2 ? (unsigned char *)floats : pixels,
            SAMPLE_PIXELS * (format == 2 ? 12u : format == 1 ? 1u : 3u));
        memcpy(working_palette, palette, sizeof(palette));
        status = sixel_frame_init_borrowed(frame, working_pixels,
            SAMPLE_WIDTH, SAMPLE_HEIGHT, pixel_format,
            format == 1 ? working_palette : NULL, format == 1 ? 256 : -1);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        iface = sixel_frame_as_interface(frame);
        memset(&transparency, 0, sizeof(transparency));
        transparency.transparent_mask = (unsigned char *)malloc(SAMPLE_PIXELS);
        if (transparency.transparent_mask == NULL) {
            goto end;
        }
        memcpy(transparency.transparent_mask, mask, SAMPLE_PIXELS);
        transparency.transparent_mask_size = SAMPLE_PIXELS;
        if (SIXEL_FAILED(iface->vtbl->set_transparency(iface, &transparency))) {
            goto end;
        }
        status = sixel_filter_factory_create_by_kind(
            SIXEL_FILTER_KIND_SAMPLE, &config, &filter);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_filter_bind_input(filter, &frame, pixel_format,
            sixel_frame_get_colorspace(frame));
        sixel_filter_bind_sample_output(filter, &samples, pixel_format,
            sixel_frame_get_colorspace(frame));
        /* A global budget of eight must respect the caller's smaller scope. */
        sixel_set_threads(8);
        sixel_thread_budget_enter(&scope, thread);
        status = sixel_filter_run(filter, allocator, NULL);
        sixel_thread_budget_leave(&scope);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        iface = sixel_frame_as_interface(samples.frame);
        if (SIXEL_FAILED(iface->vtbl->get_transparency(iface, &transparency)) ||
                transparency.transparent_mask == NULL) {
            goto end;
        }
        size = samples.point_count * (format == 2 ? 12u : 3u);
        if (attempt == 0) {
            expected = (unsigned char *)malloc(size);
            expected_mask = (unsigned char *)malloc(samples.point_count);
            if (expected == NULL || expected_mask == NULL) {
                goto end;
            }
            expected_size = size;
            expected_width = samples.width;
            expected_height = samples.height;
            memcpy(expected, sixel_frame_get_pixels(samples.frame), size);
            memcpy(expected_mask, transparency.transparent_mask,
                   samples.point_count);
        } else if (size != expected_size || samples.width != expected_width ||
                   samples.height != expected_height ||
                   memcmp(expected, sixel_frame_get_pixels(samples.frame),
                          size) != 0 ||
                   memcmp(expected_mask, transparency.transparent_mask,
                          samples.point_count) != 0) {
            fprintf(stderr, "sample mismatch format=%d clip=%d mode=%d t=%d\n",
                    format, clipped, mode, thread);
            goto end;
        }
        sixel_filter_free(filter);
        filter = NULL;
        sixel_sample_stream_dispose(&samples);
        if (SIXEL_FAILED(sixel_encoder_new(&encoder, allocator)) ||
                SIXEL_FAILED(sixel_output_new(&output, sample_write,
                    &encoded, allocator))) {
            goto end;
        }
        (void)snprintf(thread_text, sizeof(thread_text), "%d", thread);
        /* Encoding itself retains thread-dependent band boundaries and
         * diffusion. Compare extraction modes at each fixed encoder budget. */
        runtime.sample_parallel_min_pixels = attempt % 2 == 0 ? SIZE_MAX : 1;
        sixel_runtime_policy_store(&runtime);
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "64")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'G', "off")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', thread_text)) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, '4',
                    "adaptive-grid")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'q',
                    mode == 1 ? "high" : mode == 2 ? "full" : "auto"))) {
            goto end;
        }
        if (clipped && SIXEL_FAILED(sixel_encoder_setopt(encoder, 'c',
                "231x179+7+5"))) {
            goto end;
        }
        if (mode >= 3) {
            (void)snprintf(target_text, sizeof(target_text),
                "auto:sample_target=%u", mode == 4 ? 100000u : 257u);
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', target_text))) {
                goto end;
            }
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr, "encode: %s\n",
                    sixel_helper_get_additional_message());
            goto end;
        }
        if (attempt % 2 == 0) {
            free(reference.data);
            reference = encoded;
            encoded.data = NULL;
        } else if (encoded.size != reference.size ||
                memcmp(encoded.data, reference.data, encoded.size) != 0) {
            fprintf(stderr, "encode mismatch format=%d clip=%d mode=%d t=%d\n",
                    format, clipped, mode, thread);
            goto end;
        }
        free(encoded.data);
        memset(&encoded, 0, sizeof(encoded));
        sixel_output_unref(output);
        output = NULL;
        sixel_encoder_unref(encoder);
        encoder = NULL;
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    free(expected);
    free(expected_mask);
    free(reference.data);
    free(encoded.data);
    sixel_filter_free(filter);
    sixel_sample_stream_dispose(&samples);
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    sixel_allocator_unref(allocator);
    free(mask);
    free(floats);
    free(pixels);
    free(working_pixels);
    return ok;
}

int
test_filter_0046_filter_sample_parallel_exact(int argc, char **argv)
{
    int format;
    int clipped;
    int mode;

    /* Each TAP producer selects one format so the full matrix remains
     * bounded on slower hosts. Keep every clipping, quality and
     * thread-budget comparison within that format's observation. */
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "rgb888") == 0) {
        format = 0;
    } else if (strcmp(argv[1], "pal8") == 0) {
        format = 1;
    } else if (strcmp(argv[1], "float32") == 0) {
        format = 2;
    } else {
        return EXIT_FAILURE;
    }
    if (sixel_compat_setenv("SIXEL_SAMPLE_PARALLEL_MIN_PIXELS", "1") != 0) {
        return EXIT_FAILURE;
    }
    for (clipped = 0; clipped < 2; ++clipped) {
        for (mode = 0; mode < 5; ++mode) {
            if (!sample_case(format, clipped, mode)) {
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
