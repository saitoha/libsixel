/* SPDX-License-Identifier: MIT */
/* Sampling must preserve typed coordinates before palette construction. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/filter-factory.h"
#include "src/filter-sample.h"
#include "tests/data/color-numeric/palette-space.h"

static char const *const sampling_spaces[] = {
    "gamma", "linear", "oklab", "cielab", "din99d"
};
static int const sampling_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};

typedef struct sampling_capture {
    char bytes[4096];
    size_t size;
} sampling_capture_t;

static int
sampling_write(char *data, int size, void *context)
{
    sampling_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->bytes) - capture->size) {
        return -1;
    }
    memcpy(capture->bytes + capture->size, data, (size_t)size);
    capture->size += (size_t)size;
    capture->bytes[capture->size] = '\0';
    return size;
}

static int
sampling_copy(int space)
{
    static int const positions[4] = { 2, 4, 14, 16 };
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_filter_t *filter;
    sixel_filter_sample_config_t config;
    sixel_sample_stream_t samples;
    float pixels[72];
    float saved[72];
    float expected[12];
    int n;
    int ok;
    int colorspace;

    allocator = NULL;
    frame = NULL;
    filter = NULL;
    ok = 0;
    memset(&config, 0, sizeof(config));
    sixel_sample_stream_init(&samples);
    /* Binary fractions make copying, clipping and membership exact. Values
     * cross nominal bounds in every channel, including opponent axes. */
    for (n = 0; n < 72; ++n) {
        pixels[n] = (float)(n - 36) / 8.0f;
    }
    memcpy(saved, pixels, sizeof(saved));
    for (n = 0; n < 4; ++n) {
        memcpy(expected + n * 3, pixels + positions[n] * 3,
            3 * sizeof(float));
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                NULL, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 6, 4,
                sampling_formats[space], NULL, -1))) {
        goto end;
    }
    colorspace = sixel_frame_get_colorspace(frame);
    config.policy = SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID;
    config.source = SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME;
    config.clip_x = 2;
    config.clip_width = 4;
    config.clip_height = 4;
    config.palette_sample_override = 1;
    config.palette_sample_target = 4;
    if (SIXEL_FAILED(sixel_filter_factory_create_by_kind(
                SIXEL_FILTER_KIND_SAMPLE, &config, &filter))) {
        goto end;
    }
    sixel_filter_bind_input(filter, &frame, sampling_formats[space],
        colorspace);
    sixel_filter_bind_sample_output(filter, &samples,
        sampling_formats[space], colorspace);
    ok = SIXEL_SUCCEEDED(sixel_filter_run(filter, allocator, NULL)) &&
        samples.storage == SIXEL_SAMPLE_STREAM_OWNED_FRAME &&
        samples.width == 2 && samples.height == 2 &&
        samples.pixelformat == sampling_formats[space] &&
        samples.colorspace == colorspace &&
        samples.source == SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME &&
        samples.frame != frame &&
        memcmp(sixel_frame_get_pixels(samples.frame), expected,
            sizeof(expected)) == 0 &&
        memcmp(pixels, saved, sizeof(saved)) == 0;
end:
    sixel_filter_free(filter);
    sixel_sample_stream_dispose(&samples);
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    return ok;
}

static int
sampling_wire(int space, int p3, char const *mode, char const *precision,
              char const *binning)
{
    static struct {
        int flag;
        char const *value;
    } const options[] = {
        { 'p', "1" }, { 'Q', "kmeans:seed=1" }, { 'F', "none" },
        { 'a', "off" }, { '_', "none" }, { '~', "none" },
        { 'd', "none" }, { 't', "rgb" }, { '0', "soft" }
    };
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_dither_t *diagnostic;
    sixel_frame_t *frame;
    sampling_capture_t capture;
    float pixels[24];
    unsigned char byte_pixels[6] = { 0, 0, 0, 255, 255, 255 };
    unsigned char const *wire;
    char expected[64];
    size_t n;
    int ok;
    int worker;
    int automatic;
    int failure;
    int byte;
    SIXELSTATUS status;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    worker = strcmp(mode, "sync") != 0 && strcmp(mode, "auto-sync") != 0;
    automatic = strncmp(mode, "auto", 4) == 0;
    failure = strcmp(mode, "error") == 0;
    byte = strcmp(mode, "byte") == 0;
    memset(&capture, 0, sizeof(capture));
    /* Stride two selects indices 0 and 2, one of each color. The full
     * frame has four of each, so either policy has the same X centroid.
     * Membership itself is protected by sampling_copy(). */
    for (n = 0U; n < 8U; ++n) {
        pixels[n * 3U] = (n % 4U < 2U) ? 2.0f : -.25f;
        pixels[n * 3U + 1U] = .1f;
        pixels[n * 3U + 2U] = .1f;
    }
    wire = color_palette_space[space].wire[p3];
    snprintf(expected, sizeof(expected), "#0;2;%u;%u;%u#",
        (unsigned)wire[0], (unsigned)wire[1], (unsigned)wire[2]);
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, sampling_write,
                &capture, NULL))) {
        goto end;
    }
    for (n = 0U; n < sizeof(options) / sizeof(options[0]); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, options[n].flag,
                options[n].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '5', binning)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=',
                worker ? "4" : "1")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '.', precision)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '4', automatic
                ? "auto" : "adaptive-grid")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X',
                sampling_spaces[space])) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', "gamma")) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL))) {
        goto end;
    }
    if (byte) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "2")) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, byte_pixels,
                    2, 1, SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
            goto end;
        }
    } else if (SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 4, 2,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    diagnostic = (sixel_dither_t *)encoder->diagnostic_dither;
    if (failure) {
        ok = status == SIXEL_RUNTIME_ERROR && capture.size == 0U;
    } else if (byte) {
        ok = SIXEL_SUCCEEDED(status) && diagnostic != NULL &&
            diagnostic->use_rgb101_palette == 0 &&
            diagnostic->pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
            strstr(capture.bytes, ";2;0;0;0") != NULL &&
            strstr(capture.bytes, ";2;100;100;100") != NULL;
    } else if (strcmp(precision, "8bit") == 0) {
        /* Sampling and clustering stay float; application rounds to bytes.
         * This centroid's wire code is the same from either palette view. */
        ok = SIXEL_SUCCEEDED(status) && diagnostic != NULL &&
            diagnostic->use_rgb101_palette == 0 &&
            diagnostic->pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
            strstr(capture.bytes, expected) != NULL;
    } else {
        ok = SIXEL_SUCCEEDED(status) && diagnostic != NULL &&
            diagnostic->use_rgb101_palette != 0 &&
            diagnostic->pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32 &&
            strstr(capture.bytes, expected) != NULL;
    }
end:
    if (!ok) {
        fprintf(stderr, "%s X=%s p3=%d precision=%s: %s; got %s; %s\n",
            mode, sampling_spaces[space], p3, precision, expected,
            capture.bytes, sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

int
test_palette_sampling_float(int argc, char **argv)
{
    int space;
    int p3;
    int ok;

    if (argc != 5 && argc != 6) {
        return EXIT_FAILURE;
    }
    space = atoi(argv[2]);
    p3 = atoi(argv[3]);
    if (space < 0 || space >= 5 || p3 < 0 || p3 > 1) {
        return EXIT_FAILURE;
    }
    ok = strcmp(argv[1], "copy") == 0 ? sampling_copy(space)
        : sampling_wire(space, p3, argv[1], argv[4],
            argc == 6 ? argv[5] : "none");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
