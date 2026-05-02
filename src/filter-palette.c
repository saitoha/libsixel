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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdlib.h>
#if HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */

#include <sixel.h>

#include "filter-palette.h"
#include "filter.h"
#include "status.h"

typedef struct sixel_filter_palette_state {
    sixel_filter_palette_config_t config;
} sixel_filter_palette_state_t;

static SIXELSTATUS
sixel_filter_palette_apply(sixel_filter_t *filter,
      sixel_allocator_t *allocator,
      sixel_timeline_logger_t *logger);

static void
sixel_filter_palette_dispose(sixel_filter_t *filter);

static sixel_filter_vtbl_t const sixel_filter_palette_vtbl = {
    "palette",
    SIXEL_FILTER_KIND_PALETTE,
    sixel_filter_palette_apply,
    sixel_filter_palette_dispose,
    NULL,
    NULL,
    NULL
};

static SIXELSTATUS
sixel_filter_palette_apply(sixel_filter_t *filter,
                           sixel_allocator_t *allocator,
                           sixel_timeline_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_palette_state_t *state;
    sixel_frame_t *frame;
    sixel_dither_t **dither_out;
    int height;

    status = SIXEL_FALSE;
    state = NULL;
    frame = NULL;
    dither_out = NULL;
    height = 0;

    (void)allocator;

    if (filter == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_palette_state_t *)filter->userdata;
    if (state == NULL || state->config.builder == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (filter->input.slot == NULL || filter->input.slot[0] == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame = filter->input.slot[0];
    dither_out = state->config.dither_out;

    if (dither_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    height = sixel_frame_get_height(frame);
    if (height < 0) {
        height = 0;
    }

    status = state->config.builder(state->config.builder_userdata,
                                   frame,
                                   dither_out,
                                   logger);
    if (SIXEL_FAILED(status)) {
        return status;
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
sixel_filter_palette_dispose(sixel_filter_t *filter)
{
    sixel_filter_palette_state_t *state;

    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_palette_state_t *)filter->userdata;
    if (state != NULL) {
        free(state);
        filter->userdata = NULL;
    }
}

SIXELSTATUS
sixel_filter_palette_init(sixel_filter_t *filter,
                          const sixel_filter_palette_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_palette_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL || config == NULL || config->builder == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_palette_state_t *)malloc(sizeof(*state));
    if (state == NULL) {
        sixel_helper_set_additional_message(
            "sixel_filter_palette_init: malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(state, 0, sizeof(*state));
    state->config = *config;

    status = sixel_filter_init_with_vtbl(
        filter,
        &sixel_filter_palette_vtbl,
        state);
    if (SIXEL_FAILED(status)) {
        free(state);
        return status;
    }

    sixel_filter_set_progress(filter, NULL, NULL, 1);

    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
