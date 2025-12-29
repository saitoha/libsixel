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

#include <sixel.h>

#include "filter-factory.h"
#include "filter-resize.h"
#include "filter.h"
#include "filter_test_common.h"

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
main(void)
{
    int success;

    success = 1;
    printf("1..2\n");

    if (test_resize_changes_dimensions()) {
        printf("ok 1 - resize filter updates frame dimensions\n");
    } else {
        printf("not ok 1 - resize filter updates frame dimensions\n");
        success = 0;
    }

    if (test_resize_prefers_float_when_requested()) {
        printf("ok 2 - resize filter keeps float frames when preferred\n");
    } else {
        printf("not ok 2 - resize filter keeps float frames when preferred\n");
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
