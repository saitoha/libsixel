/*
 * SPDX-License-Identifier: MIT
 *
 * Encode filter tests. These verify that the filter sets output metadata,
 * forwards pixels to the encoder, and reports progress via callbacks.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/compat_stub.h"
#include "src/encoder.h"
#include "src/stdio_stub.h"
#include "src/filter-encode.h"
#include "src/filter-factory.h"
#include "src/filter.h"
#include "tests/_support/processing/filter/filter_test_common.h"
#include "src/output.h"

static int g_issue222_capture_enabled;
static int g_issue222_capture_complete;
static size_t g_issue222_first_size;

/*
 * Capture the first allocation request issued during the encode_bytes call.
 * The regression test uses this to verify that packed formats allocate the
 * packed source length instead of width*height bytes.
 */
static void
issue222_record_allocation(size_t size)
{
    if (g_issue222_capture_enabled != 0 &&
            g_issue222_capture_complete == 0) {
        g_issue222_capture_complete = 1;
        g_issue222_first_size = size;
    }
}

static void *
issue222_malloc(size_t size)
{
    void *result;

    result = malloc(size);
    issue222_record_allocation(size);
    return result;
}

static void *
issue222_calloc(size_t nmemb, size_t size)
{
    void *result;

    result = calloc(nmemb, size);
    issue222_record_allocation(nmemb * size);
    return result;
}

static void *
issue222_realloc(void *ptr, size_t size)
{
    void *result;

    result = realloc(ptr, size);
    issue222_record_allocation(size);
    return result;
}

static void
issue222_free(void *ptr)
{
    free(ptr);
}

static int
test_encode_bytes_uses_packed_byte_count_issue222(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    unsigned char packed[17];
    size_t expected_packed_bytes;
    char const *sink_path;
    int i;
    int success;

    status = SIXEL_FALSE;
    allocator = NULL;
    encoder = NULL;
    expected_packed_bytes = 0u;
    sink_path = NULL;
    i = 0;
    success = 0;

#if defined(_WIN32)
    sink_path = "NUL";
#else
    sink_path = "/dev/null";
#endif

    for (i = 0; i < (int)sizeof(packed); ++i) {
        packed[i] = 0u;
    }
    packed[0] = 0xaau;
    packed[1] = 0x55u;
    packed[2] = 0x80u;

    expected_packed_bytes = ((size_t)17u + 7u) / 8u;

    status = sixel_allocator_new(&allocator,
                                 issue222_malloc,
                                 issue222_calloc,
                                 issue222_realloc,
                                 issue222_free);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT, sink_path);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    g_issue222_capture_enabled = 1;
    g_issue222_capture_complete = 0;
    g_issue222_first_size = 0u;
    status = sixel_encoder_encode_bytes(encoder,
                                        packed,
                                        17,
                                        1,
                                        SIXEL_PIXELFORMAT_G1,
                                        NULL,
                                        0);
    g_issue222_capture_enabled = 0;
    if (g_issue222_capture_complete == 0) {
        fprintf(stderr,
                "issue #222: encode_bytes finished without source copy "
                "allocation (%d): %s\n",
                status,
                sixel_helper_get_additional_message());
        goto cleanup;
    }

    if (g_issue222_first_size != expected_packed_bytes) {
        fprintf(stderr,
                "issue #222: expected first allocation %zu, got %zu\n",
                expected_packed_bytes,
                g_issue222_first_size);
        goto cleanup;
    }

    success = 1;

cleanup:
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }

    return success;
}

static int
test_encoder_encode_frame_writes_output(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    test_output_counter_t counter;
    char const *sink_path;

    status = SIXEL_FALSE;
    allocator = NULL;
    encoder = NULL;
    frame = NULL;
    output = NULL;
    counter.calls = 0;
    counter.bytes = 0;
    sink_path = NULL;

#if defined(_WIN32)
    sink_path = "NUL";
#else
    sink_path = "/dev/null";
#endif

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    /*
     * encode_frame writes animation macro controls to encoder->outfd even
     * when a custom sixel_output_t receives the image body. Keep this TAP
     * runner quiet while still counting the callback output below.
     */
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT, sink_path);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_rgb_frame(allocator, 2, 2, &frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_counter_output(allocator, &counter, &output);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "8");
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none");
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status =
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_QUANTIZE_MODEL,
                             "heckbert");
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_frame_set_multiframe(frame, 1);
    sixel_frame_set_frame_no(frame, 1);

    status = sixel_encoder_encode_frame(encoder, frame, output);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (counter.calls <= 0 || counter.bytes <= 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

cleanup:
    if (output != NULL) {
        sixel_output_unref(output);
    }
    sixel_frame_unref(frame);
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_encode_writes_stream_and_progress(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_encode_config_t config;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_output_t *output;
    test_progress_t progress;
    test_output_counter_t counter;
    int expected_colorspace;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    frame = NULL;
    dither = NULL;
    output = NULL;
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;
    counter.calls = 0;
    counter.bytes = 0;
    expected_colorspace = SIXEL_COLORSPACE_LINEAR;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_rgb_frame(allocator, 2, 2, &frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_dither(allocator, 8, &dither);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_counter_output(allocator, &counter, &output);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.dither = dither;
    config.output = output;
    config.output_colorspace = expected_colorspace;

    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_ENCODE,
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &frame,
                            sixel_frame_format(frame),
                            frame->format.coordinates);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (dither->pixelformat != sixel_frame_format(frame)) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (counter.calls <= 0 || counter.bytes <= 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (progress.began != 1 || progress.completed != 1 || progress.aborted) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

cleanup:
    sixel_filter_teardown(filter);
    sixel_filter_free(filter);
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

/* Decode the public encoder's stream into display-order RGB triplets.
 * The caller can compare image content independently of register numbering. */
static int
encode_to_rgb(int via_frame, unsigned char *source, int width, int height,
              int format, unsigned char *rgb)
{
    unsigned char wire[8192];
    sixel_frame_t *frame = NULL;
    sixel_encoder_t *encoder = NULL;
    FILE *capture = NULL;
    unsigned char *indices = NULL;
    unsigned char *palette = NULL;
    int decoded_width, decoded_height, colors, i;
    size_t size;
    SIXELSTATUS status;
    int result = EXIT_FAILURE;

    capture = sixel_compat_tmpfile();
    if (capture == NULL || sixel_encoder_new(&encoder, NULL) != SIXEL_OK ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none") !=
            SIXEL_OK ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_PALETTE_TYPE, "rgb") !=
            SIXEL_OK) {
        goto end;
    }
    encoder->outfd = sixel_fileno(capture);
    if (via_frame) {
        status = sixel_frame_new(&frame, NULL);
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_frame_init_borrowed(frame, source, width, height,
                                               format, NULL, -1);
        }
        if (SIXEL_SUCCEEDED(status)) {
            status = sixel_encoder_encode_frame(encoder, frame, NULL);
        }
        sixel_frame_unref(frame);
    } else {
        status = sixel_encoder_encode_bytes(encoder, source, width, height,
                                            format, NULL, -1);
    }
    encoder->outfd = 1;  /* The stream owns its descriptor. */
    if (status != SIXEL_OK) {
        fprintf(stderr, "encode format 0x%x returned 0x%x: %s\n",
                (unsigned)format, (unsigned)status,
                sixel_helper_get_additional_message());
        goto end;
    }
    if (fseek(capture, 0, SEEK_SET) != 0) {
        goto end;
    }
    size = fread(wire, 1, sizeof(wire), capture);
    if (size == 0 || size == sizeof(wire) ||
        sixel_decode_raw(wire, (int)size, &indices, &decoded_width,
                         &decoded_height, &palette, &colors, NULL) != SIXEL_OK
        || decoded_width != width || decoded_height != height ||
        indices == NULL || palette == NULL) {
        goto end;
    }
    for (i = 0; i < width * height; ++i) {
        if (indices[i] >= colors) {
            goto end;
        }
        memcpy(rgb + 3 * i, palette + 3 * indices[i], 3);
    }
    result = EXIT_SUCCESS;

end:
    free(indices);
    free(palette);
    if (capture != NULL) {
        fclose(capture);
    }
    sixel_encoder_unref(encoder);
    return result;
}


/* Unequal red and blue components expose both missed and double swaps.
 * The odd row width and a partial second SIXEL band also exercise the tail. */
static int
test_encode_bgr888(void)
{
    static unsigned char const colors[][3] = {
        {16, 80, 208}, {208, 32, 104}, {72, 184, 40}, {224, 8, 128}
    };
    /* The above colors after rounding to integral SIXEL percentages. */
    static unsigned char const expected[][3] = {
        {15, 79, 209}, {209, 33, 105}, {71, 184, 41}, {224, 8, 128}
    };
    static unsigned char const pattern[7][5] = {
        {0, 1, 2, 3, 2}, {1, 0, 3, 2, 0}, {2, 2, 1, 0, 3}, {3, 1, 0, 0, 2},
        {0, 2, 3, 1, 1}, {2, 1, 0, 2, 3}, {3, 0, 2, 1, 0}
    };
    unsigned char rgb[7][5][3], bgr[7][5][3];
    unsigned char decoded_rgb[7][5][3], decoded_bgr[7][5][3];
    unsigned char const *color;
    int x, y;

    for (y = 0; y < 7; ++y) {
        for (x = 0; x < 5; ++x) {
            color = colors[pattern[y][x]];
            memcpy(rgb[y][x], color, 3);
            bgr[y][x][0] = color[2];
            bgr[y][x][1] = color[1];
            bgr[y][x][2] = color[0];
        }
    }
    if (encode_to_rgb(0, (unsigned char *)rgb, 5, 7,
            SIXEL_PIXELFORMAT_RGB888, (unsigned char *)decoded_rgb) != 0 ||
        encode_to_rgb(0, (unsigned char *)bgr, 5, 7,
            SIXEL_PIXELFORMAT_BGR888, (unsigned char *)decoded_bgr) != 0) {
        return EXIT_FAILURE;
    }
    for (y = 0; y < 7; ++y) {
        for (x = 0; x < 5; ++x) {
            color = expected[pattern[y][x]];
            if (memcmp(decoded_rgb[y][x], color, 3) != 0 ||
                    memcmp(decoded_bgr[y][x], color, 3) != 0) {
                fprintf(stderr, "(%d,%d) RGB %u,%u,%u, BGR %u,%u,%u, "
                        "expected %u,%u,%u\n", x, y,
                        decoded_rgb[y][x][0], decoded_rgb[y][x][1],
                        decoded_rgb[y][x][2], decoded_bgr[y][x][0],
                        decoded_bgr[y][x][1], decoded_bgr[y][x][2],
                        color[0], color[1], color[2]);
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}


/* Each row has 17 samples followed by padding. Alternate an ascending
 * ramp with its reverse so that both the full bytes and the final sample
 * on every row are observable. The expected colors are the 16 gray levels
 * after rounding to SIXEL percentages and decoding back to bytes.
 */
static int
test_encode_packed_gray(char const *name, int via_frame)
{
    static struct {
        char const *name;
        int format;
        int levels;
        size_t row_bytes;
        unsigned char rows[2][9];
    } const cases[] = {
        {"g1", SIXEL_PIXELFORMAT_G1, 2, 3,
         {{0x55, 0x55, 0x7f}, {0xaa, 0xaa, 0x80}}},
        {"g2", SIXEL_PIXELFORMAT_G2, 4, 5,
         {{0x1b, 0x1b, 0x1b, 0x1b, 0x3f},
          {0xe4, 0xe4, 0xe4, 0xe4, 0xc0}}},
        {"g4", SIXEL_PIXELFORMAT_G4, 16, 9,
         {{0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x0f},
          {0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10, 0xf0}}}
    };
    static unsigned char const gray[] = {
        0, 18, 33, 51, 69, 84, 102, 120, 135, 153, 171, 186, 204, 222, 237, 255
    };
    unsigned char source[6 * 9];
    unsigned char decoded[6][17][3];
    unsigned char const *rgb;
    size_t mode;
    int x, y, code, expected;

    for (mode = 0; mode < sizeof(cases) / sizeof(cases[0]); ++mode) {
        if (strcmp(name, cases[mode].name) == 0) {
            break;
        }
    }
    if (mode == sizeof(cases) / sizeof(cases[0])) {
        return EXIT_FAILURE;
    }
    for (y = 0; y < 6; ++y) {
        memcpy(source + (size_t)y * cases[mode].row_bytes,
               cases[mode].rows[y % 2], cases[mode].row_bytes);
    }
    if (encode_to_rgb(via_frame, source, 17, 6, cases[mode].format,
                            (unsigned char *)decoded) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    for (y = 0; y < 6; ++y) {
        for (x = 0; x < 17; ++x) {
            code = x % cases[mode].levels;
            if (y % 2 != 0) {
                code = cases[mode].levels - 1 - code;
            }
            expected = gray[code * 15 / (cases[mode].levels - 1)];
            rgb = decoded[y][x];
            if (rgb[0] != expected || rgb[1] != expected ||
                rgb[2] != expected) {
                fprintf(stderr, "%s (%d,%d) code %d: %u,%u,%u expected %d\n",
                        name, x, y, code, (unsigned)rgb[0], (unsigned)rgb[1],
                        (unsigned)rgb[2], expected);
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}

int
test_filter_0010_filter_encode(int argc, char **argv)
{
    int success;

    if (argc == 3 && strcmp(argv[1], "frame") == 0) {
        return test_encode_packed_gray(argv[2], 1);
    }
    if (argc == 2 && strcmp(argv[1], "bgr888") == 0) {
        return test_encode_bgr888();
    }
    if (argc == 2) {
        return test_encode_packed_gray(argv[1], 0);
    }

    success = 1;

    if (!test_encode_writes_stream_and_progress()) {
        fprintf(stderr,
                "encode filter streams data and reports progress failed\n");
        success = 0;
    }
    if (!test_encoder_encode_frame_writes_output()) {
        fprintf(stderr,
                "encoder encode_frame writes callback output failed\n");
        success = 0;
    }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_security_0001_issue222_encoder_encode_bytes_packed_g1(int argc,
                                                            char **argv)
{
    int success;

    (void) argc;
    (void) argv;

    success = 1;

    if (!test_encode_bytes_uses_packed_byte_count_issue222()) {
        fprintf(stderr,
                "issue #222: packed G1 encode_bytes size handling failed\n");
        success = 0;
    }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
