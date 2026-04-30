/*
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the sample filter. These tests exercise stride selection,
 * clipping support, and progress reporting.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/filter-factory.h"
#include "src/filter-sample.h"
#include "src/filter.h"
#include "tests/processing/filter/filter_test_common.h"

static SIXELSTATUS
test_frame_get_pixels_view(sixel_frame_t const *frame,
                           sixel_frame_pixels_view_t *view)
{
    sixel_frame_interface_t *frame_if;

    frame_if = NULL;
    if (frame == NULL || view == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame_if = (sixel_frame_interface_t *)frame;
    if (frame_if->vtbl == NULL || frame_if->vtbl->get_pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return frame_if->vtbl->get_pixels(frame_if, view);
}

static int
test_sample_stride_override(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_sample_config_t config;
    sixel_frame_t *input_frame;
    sixel_frame_t *sample_frame;
    sixel_frame_pixels_view_t input_view;
    sixel_frame_pixels_view_t sample_view;
    test_progress_t progress;
    size_t stride;
    size_t total;
    int expected_width;
    int expected_height;
    unsigned char expected_first;
    unsigned char expected_last;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    input_frame = NULL;
    sample_frame = NULL;
    memset(&input_view, 0, sizeof(input_view));
    memset(&sample_view, 0, sizeof(sample_view));
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;
    stride = 1u;
    total = 0u;
    expected_width = 0;
    expected_height = 0;
    expected_first = 0;
    expected_last = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_rgb_frame(allocator, 4, 4, &input_frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = test_frame_get_pixels_view(input_frame, &input_view);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.clip_x = 0;
    config.clip_y = 0;
    config.clip_width = 0;
    config.clip_height = 0;
    config.reqcolors = 256;
    config.quality_mode = SIXEL_QUALITY_AUTO;
    config.palette_sample_override = 1;
    config.palette_sample_target = 1;

    status = sixel_filter_factory_create_by_name("sample",
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &input_frame,
                            input_view.pixelformat,
                            input_view.colorspace);
    sixel_filter_bind_output(filter,
                             &sample_frame,
                             input_view.pixelformat,
                             input_view.colorspace);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    expected_first = input_view.pixels[0];
    expected_last = input_view.pixels[((size_t)(input_view.width
                    * (input_view.height - 1)
                    + (input_view.width - 1))) * 3u];

    total = (size_t)input_view.width * (size_t)input_view.height;
    while (stride < total
           && total / (stride * stride)
               > config.palette_sample_target) {
        ++stride;
    }

    expected_width = (input_view.width + (int)stride - 1) / (int)stride;
    expected_height = (input_view.height + (int)stride - 1)
            / (int)stride;

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (sample_frame == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    status = test_frame_get_pixels_view(sample_frame, &sample_view);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (sample_view.width != expected_width
            || sample_view.height != expected_height) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (sample_view.pixels[0] != expected_first
            || sample_view.pixels[(expected_width * expected_height - 1) * 3]
                != expected_last) {
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
    sixel_frame_unref(sample_frame);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_sample_respects_clip_region(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_sample_config_t config;
    sixel_frame_t *input_frame;
    sixel_frame_t *sample_frame;
    sixel_frame_pixels_view_t input_view;
    sixel_frame_pixels_view_t sample_view;
    test_progress_t progress;
    unsigned char expected;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    input_frame = NULL;
    sample_frame = NULL;
    memset(&input_view, 0, sizeof(input_view));
    memset(&sample_view, 0, sizeof(sample_view));
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;
    expected = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_rgb_frame(allocator, 3, 2, &input_frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = test_frame_get_pixels_view(input_frame, &input_view);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    expected = input_view.pixels[3];

    config.clip_x = 1;
    config.clip_y = 0;
    config.clip_width = 2;
    config.clip_height = 1;
    config.reqcolors = 256;
    config.quality_mode = SIXEL_QUALITY_AUTO;
    config.palette_sample_override = 0;
    config.palette_sample_target = 0;

    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_SAMPLE,
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &input_frame,
                            input_view.pixelformat,
                            input_view.colorspace);
    sixel_filter_bind_output(filter,
                             &sample_frame,
                             input_view.pixelformat,
                             input_view.colorspace);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (sample_frame == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    status = test_frame_get_pixels_view(sample_frame, &sample_view);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (sample_view.width != 2 || sample_view.height != 1) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (sample_view.pixels[0] != expected) {
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
    sixel_frame_unref(sample_frame);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

int
test_filter_0002_filter_sample(int argc, char **argv)
{
    int success;

    (void) argc;
    (void) argv;

    success = 1;

    if (!test_sample_stride_override()) {
        fprintf(stderr, "sample filter honors override target failed\n");
        success = 0;
    }

    if (!test_sample_respects_clip_region()) {
        fprintf(stderr, "sample filter crops before sampling failed\n");
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
