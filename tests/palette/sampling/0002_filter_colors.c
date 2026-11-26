/*
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the colorspace filter. These cases verify pixelformat
 * promotion, colorspace tagging, and progress reporting.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>
#include "src/pixelformat.h"

#include "src/filter-colors.h"
#include "src/filter-factory.h"
#include "src/filter.h"
#include "tests/_support/processing/filter/filter_test_common.h"

static int
test_colors_promotes_to_linear_float(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_colors_config_t config;
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

    config.target_layout = SIXEL_PIXELFORMAT_RGBFLOAT32;
    config.target_colorspace = SIXEL_COLORSPACE_SRGB_LINEAR;

    status = sixel_filter_factory_create_by_name("colorspace",
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &input_frame,
                            sixel_frame_format(input_frame),
                            input_frame->format.coordinates);
    sixel_filter_bind_output(filter,
                             &output_frame,
                             sixel_frame_format(input_frame),
                             input_frame->format.coordinates);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (output_frame == NULL
            || sixel_frame_format(output_frame)
                != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
            || output_frame->format.coordinates !=
                SIXEL_COLORSPACE_SRGB_LINEAR) {
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

static int
test_colors_noop_keeps_frame_bound(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_colors_config_t config;
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

    status = make_rgb_frame(allocator, 3, 1, &input_frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.target_layout = SIXEL_PIXELFORMAT_RGB888;
    config.target_colorspace = SIXEL_COLORSPACE_SRGB;

    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_COLORS,
                                                 &config,
                                                 &filter);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_filter_bind_input(filter,
                            &input_frame,
                            sixel_frame_format(input_frame),
                            input_frame->format.coordinates);
    sixel_filter_bind_output(filter,
                             &output_frame,
                             sixel_frame_format(input_frame),
                             input_frame->format.coordinates);
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (output_frame != input_frame) {
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
test_filter_0004_filter_colors(int argc, char **argv)
{
    int success;

    (void) argc;
    (void) argv;

    success = 1;

    if (!test_colors_promotes_to_linear_float()) {
        fprintf(stderr, "colors filter converts to linear float failed\n");
        success = 0;
    }

    if (!test_colors_noop_keeps_frame_bound()) {
        fprintf(stderr, "colors filter keeps binding for noop failed\n");
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
