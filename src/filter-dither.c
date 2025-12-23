/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

/* STDC_HEADERS */
#include <stdlib.h>

#include <sixel.h>

#include "filter-dither.h"
#include "filter.h"
#include "dither.h"
#include "status.h"

/*
 * Internal snapshot of the dither configuration. The filter keeps a pointer to
 * the dither object but never frees it; ownership stays with the caller.
 */
typedef struct sixel_filter_dither_state {
    sixel_filter_dither_config_t config;
} sixel_filter_dither_state_t;

static SIXELSTATUS
sixel_filter_dither_apply(sixel_filter_t *filter,
                          sixel_allocator_t *allocator,
                          sixel_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_dither_state_t *state;
    sixel_frame_t *frame;
    int height;

    status = SIXEL_FALSE;
    state = NULL;
    frame = NULL;
    height = 0;

    (void)allocator;

    if (filter == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_dither_state_t *)filter->userdata;
    if (state == NULL || state->config.dither == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (filter->input.slot == NULL || filter->input.slot[0] == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame = filter->input.slot[0];
    sixel_dither_set_pixelformat(state->config.dither,
                                 sixel_frame_get_pixelformat(frame));

    height = sixel_frame_get_height(frame);
    if (height < 0) {
        height = 0;
    }

    if (logger != NULL) {
        sixel_logger_logf(logger,
                          "filter",
                          "worker",
                          "dither-prepare",
                          -1,
                          -1,
                          0,
                          0,
                          sixel_frame_get_width(frame),
                          sixel_frame_get_height(frame),
                          "pixelformat=%08x",
                          sixel_frame_get_pixelformat(frame));
    }

    filter->progress.total_units = height;
    filter->progress.completed_units = height;
    status = sixel_filter_update_progress(filter, height);

    if (status == SIXEL_FALSE) {
        status = SIXEL_OK;
    }

    return status;
}

static void
sixel_filter_dither_dispose(sixel_filter_t *filter)
{
    sixel_filter_dither_state_t *state;

    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_dither_state_t *)filter->userdata;
    if (state != NULL) {
        free(state);
    }
}

SIXELSTATUS
sixel_filter_dither_init(sixel_filter_t *filter,
                         const sixel_filter_dither_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_dither_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL || config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_dither_state_t *)calloc(
        1u, sizeof(sixel_filter_dither_state_t));
    if (state == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    state->config = *config;

    status = sixel_filter_init(filter,
                               "dither",
                               SIXEL_FILTER_KIND_DITHER,
                               sixel_filter_dither_apply,
                               sixel_filter_dither_dispose,
                               state);
    if (SIXEL_FAILED(status)) {
        free(state);
        return status;
    }

    filter->progress.total_units = 0;
    filter->progress.completed_units = 0;

    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
