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

#include <sixel.h>

#include "filter-encode.h"
#include "filter-factory.h"
#include "filter.h"
#include "filter_test_common.h"
#include "output.h"

static int
test_encode_updates_output_and_progress(void)
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
    int expected_pixelformat;
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
    expected_pixelformat = SIXEL_PIXELFORMAT_RGB888;
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
                            sixel_frame_get_colorspace(frame));
    sixel_filter_set_progress(filter, progress_cb, &progress, 1);

    status = sixel_filter_run(filter, allocator, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (output->pixelformat != expected_pixelformat) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (output->colorspace != expected_colorspace) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    if (output->source_colorspace != sixel_frame_get_colorspace(frame)) {
        status = SIXEL_BAD_ARGUMENT;
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

int main(void)
{
    int success;

    success = 1;
    printf("1..1\n");

    if (test_encode_updates_output_and_progress()) {
        printf("ok 1 - encode filter writes metadata and streams data\n");
    } else {
        printf("not ok 1 - encode filter writes metadata and streams data\n");
        success = 0;
    }

    return success ? 0 : 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
