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

#include <sixel.h>

#include "src/filter-encode.h"
#include "src/filter-factory.h"
#include "src/filter.h"
#include "tests/processing/filter/filter_test_common.h"
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

    status = SIXEL_FALSE;
    allocator = NULL;
    encoder = NULL;
    frame = NULL;
    output = NULL;
    counter.calls = 0;
    counter.bytes = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_encoder_new(&encoder, allocator);
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
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_QUANTIZE_MODEL, "sticky");
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
                            frame->pixelformat,
                            frame->colorspace);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (dither->pixelformat != frame->pixelformat) {
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

int
test_filter_0010_filter_encode(int argc, char **argv)
{
    int success;

    (void) argc;
    (void) argv;

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
