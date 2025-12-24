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

#if HAVE_CONFIG_H
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdlib.h>

#include <sixel.h>

#include "filter-colors.h"
#include "filter.h"
#include "frame.h"

static int
sixel_filter_colors_target_colorspace(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
        return SIXEL_COLORSPACE_LINEAR;
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
        return SIXEL_COLORSPACE_OKLAB;
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
        return SIXEL_COLORSPACE_CIELAB;
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
        return SIXEL_COLORSPACE_DIN99D;
    case SIXEL_PIXELFORMAT_YUVFLOAT32:
        return SIXEL_COLORSPACE_YUV;
    default:
        return SIXEL_COLORSPACE_GAMMA;
    }
}

/*
 * Snapshot of the conversion target. Stored separately so filters created via
 * the factory keep a stable configuration even if the caller's stack
 * variables go out of scope before evaluation.
 */
typedef struct sixel_filter_colors_state {
    sixel_filter_colors_config_t config;
} sixel_filter_colors_state_t;

SIXELSTATUS
sixel_filter_colors_convert(const sixel_filter_colors_config_t *config,
                            sixel_frame_t *frame,
                            sixel_logger_t *logger)
{
    SIXELSTATUS status;
    int target_pixelformat;
    int source_pixelformat;
    int source_colorspace;
    int target_colorspace;

    status = SIXEL_FALSE;
    target_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    source_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    source_colorspace = SIXEL_COLORSPACE_GAMMA;
    target_colorspace = SIXEL_COLORSPACE_GAMMA;

    if (config == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    target_pixelformat = config->target_pixelformat;
    source_pixelformat = sixel_frame_get_pixelformat(frame);
    source_colorspace = sixel_frame_get_colorspace(frame);
    target_colorspace = sixel_filter_colors_target_colorspace(
        target_pixelformat);

    if (logger != NULL) {
        sixel_logger_logf(logger,
                          "filter",
                          "worker",
                          "colorspace-start",
                          -1,
                          -1,
                          0,
                          0,
                          sixel_frame_get_width(frame),
                          sixel_frame_get_height(frame),
                          "src=format%d colorspace%d dst=format%d colorspace%d",
                          source_pixelformat,
                          source_colorspace,
                          target_pixelformat,
                          target_colorspace);
    }

    status = sixel_frame_set_pixelformat(frame, target_pixelformat);
    if (SIXEL_SUCCEEDED(status) && logger != NULL) {
        sixel_logger_logf(logger,
                          "filter",
                          "worker",
                          "colorspace-finish",
                          -1,
                          -1,
                          0,
                          0,
                          sixel_frame_get_width(frame),
                          sixel_frame_get_height(frame),
                          "result=format%d colorspace%d",
                          sixel_frame_get_pixelformat(frame),
                          sixel_frame_get_colorspace(frame));
    }

    return status;
}

static SIXELSTATUS
sixel_filter_colors_apply(sixel_filter_t *filter,
                          sixel_allocator_t *allocator,
                          sixel_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_colors_state_t *state;
    sixel_frame_t *input_frame;

    (void)allocator;

    status = SIXEL_FALSE;
    state = NULL;
    input_frame = NULL;

    if (filter == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_colors_state_t *)filter->userdata;
    if (state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (filter->input.slot == NULL || filter->output.slot == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    input_frame = *(filter->input.slot);
    if (input_frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_filter_colors_convert(
        &state->config, input_frame, logger);
    if (SIXEL_SUCCEEDED(status)) {
        *(filter->output.slot) = input_frame;
        filter->progress.total_units = sixel_frame_get_height(input_frame);
        filter->progress.completed_units = filter->progress.total_units;
        sixel_filter_update_progress(
            filter, filter->progress.completed_units);
    }

    return status;
}

static void
sixel_filter_colors_dispose(sixel_filter_t *filter)
{
    sixel_filter_colors_state_t *state;

    state = NULL;

    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_colors_state_t *)filter->userdata;
    if (state != NULL) {
        free(state);
    }
}

SIXELSTATUS
sixel_filter_colors_init(sixel_filter_t *filter,
                         const sixel_filter_colors_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_colors_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL || config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_colors_state_t *)calloc(
        1u, sizeof(sixel_filter_colors_state_t));
    if (state == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    state->config = *config;

    status = sixel_filter_init(filter,
                               "colorspace",
                               SIXEL_FILTER_KIND_COLORS,
                               sixel_filter_colors_apply,
                               sixel_filter_colors_dispose,
                               state);
    if (SIXEL_FAILED(status)) {
        free(state);
        return status;
    }

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
