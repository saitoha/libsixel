/*
 * SPDX-License-Identifier: MIT
 *
 * Simple clip filter tests. The wrapper reports TAP based on exit status.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "filter-clip.h"
#include "filter-factory.h"
#include "filter.h"
#include "filter_test_common.h"

static int test_clip_noop(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_clip_config_t config;
    sixel_frame_t *input_frame;
    sixel_frame_t *output_frame;
    test_progress_t progress;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    input_frame = NULL;
    output_frame = NULL;
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_rgb_frame(allocator, 2, 2, &input_frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.clip_x = 0;
    config.clip_y = 0;
    config.clip_width = 0;
    config.clip_height = 0;

    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_CLIP,
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &input_frame,
                            input_frame->pixelformat,
                            input_frame->colorspace);
    sixel_filter_bind_output(filter,
                             &output_frame,
                             input_frame->pixelformat,
                             input_frame->colorspace);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (input_frame != output_frame) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (input_frame->width != 2 || input_frame->height != 2) {
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
    sixel_frame_unref(input_frame);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int test_clip_float_accepts_format(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_clip_config_t config;
    sixel_frame_t *input_frame;
    sixel_frame_t *output_frame;
    test_progress_t progress;
    float expected;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    input_frame = NULL;
    output_frame = NULL;
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;
    expected = 0.0f;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_float_frame(allocator, 2, 1, &input_frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    expected = input_frame->pixels.f32ptr[3];

    config.clip_x = 1;
    config.clip_y = 0;
    config.clip_width = 1;
    config.clip_height = 1;

    status = sixel_filter_factory_create_by_name("clip",
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &input_frame,
                            input_frame->pixelformat,
                            input_frame->colorspace);
    sixel_filter_bind_output(filter,
                             &output_frame,
                             input_frame->pixelformat,
                             input_frame->colorspace);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (output_frame == NULL || output_frame->pixelformat !=
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (output_frame->width != 1 || output_frame->height != 1) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (fabsf(output_frame->pixels.f32ptr[0] - expected) > 0.0001f) {
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
    sixel_frame_unref(input_frame);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

int
test_filter_0001_filter_clip(int argc, char **argv)
{
    int success;

    (void) argc;
    (void) argv;

    success = 1;

    if (!test_clip_noop()) {
        fprintf(stderr, "clip filter skips empty region failed\n");
        success = 0;
    }

    if (!test_clip_float_accepts_format()) {
        fprintf(stderr, "clip filter trims float32 frames failed\n");
        success = 0;
    }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "gnu"
 * indent-tabs-mode: nil
 * End:
 */
