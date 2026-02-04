/*
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the resize filter. These cases verify dimension
 * calculations, float preference, and progress reporting.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/filter-factory.h"
#include "src/filter-resize.h"
#include "src/filter.h"
#include "tests/processing/filter/filter_test_common.h"

static int
test_resize_changes_dimensions(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_resize_config_t config;
    sixel_frame_t *frame;
    test_progress_t progress;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    frame = NULL;
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_rgb_frame(allocator, 2, 2, &frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.pixel_width = 4;
    config.pixel_height = 2;
    config.percent_width = 0;
    config.percent_height = 0;
    config.method_for_resampling = SIXEL_RES_NEAREST;
    config.prefer_float32 = 0;
    config.planner_scale_pixelformat = frame->pixelformat;

    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_RESIZE,
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &frame,
                            frame->pixelformat,
                            frame->colorspace);
    sixel_filter_bind_output(filter,
                             &frame,
                             frame->pixelformat,
                             frame->colorspace);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (frame->width != 4 || frame->height != 2) {
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
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_resize_prefers_float_when_requested(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_resize_config_t config;
    sixel_frame_t *frame;
    test_progress_t progress;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    frame = NULL;
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_float_frame(allocator, 1, 1, &frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.pixel_width = 2;
    config.pixel_height = 2;
    config.percent_width = 0;
    config.percent_height = 0;
    config.method_for_resampling = SIXEL_RES_BICUBIC;
    config.prefer_float32 = 1;
    config.planner_scale_pixelformat = frame->pixelformat;

    status = sixel_filter_factory_create_by_name("resize",
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &frame,
                            frame->pixelformat,
                            frame->colorspace);
    sixel_filter_bind_output(filter,
                             &frame,
                             frame->pixelformat,
                             frame->colorspace);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (frame->pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (frame->width != 2 || frame->height != 2) {
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
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

int
test_filter_0003_filter_resize(int argc, char **argv)
{
    int success;

    (void) argc;
    (void) argv;

    success = 1;

    if (!test_resize_changes_dimensions()) {
        fprintf(stderr, "resize filter updates frame dimensions failed\n");
        success = 0;
    }

    if (!test_resize_prefers_float_when_requested()) {
        fprintf(stderr,
                "resize filter keeps float frames when preferred failed\n");
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
