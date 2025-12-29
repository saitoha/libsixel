/*
 * SPDX-License-Identifier: MIT
 *
 * Dither filter tests. These checks verify that the filter configures the
 * dither object for the incoming frame and reports progress through the
 * shared callback interface.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>

#include <sixel.h>

#include "filter-dither.h"
#include "filter-factory.h"
#include "filter.h"
#include "filter_test_common.h"

static int
test_dither_updates_pixelformat_and_progress(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_filter_t *filter;
    sixel_filter_dither_config_t config;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    test_progress_t progress;

    status = SIXEL_FALSE;
    allocator = NULL;
    filter = NULL;
    frame = NULL;
    dither = NULL;
    progress.began = 0;
    progress.progressed = 0;
    progress.completed = 0;
    progress.aborted = 0;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_rgb_frame(allocator, 3, 2, &frame);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = make_dither(allocator, 16, &dither);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    config.dither = dither;

    status = sixel_filter_factory_create_by_kind(SIXEL_FILTER_KIND_DITHER,
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

    if (dither->pixelformat != frame->pixelformat) {
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

    if (test_dither_updates_pixelformat_and_progress()) {
        printf("ok 1 - dither filter sets format and progress\n");
    } else {
        printf("not ok 1 - dither filter sets format and progress\n");
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
