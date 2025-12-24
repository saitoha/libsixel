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

#include "filter-clip.h"
#include "filter.h"
#include "frame.h"

/*
 * Internal state holding the clipping rectangle. The rectangle is copied from
 * the user-provided config so filters created from the factory keep a stable
 * snapshot even if the caller updates the original structure.
 */
typedef struct sixel_filter_clip_state {
    sixel_filter_clip_config_t config;
} sixel_filter_clip_state_t;

static void
sixel_filter_clip_adjust_bounds(sixel_filter_clip_config_t const *config,
                                int width,
                                int height,
                                int *clip_x_out,
                                int *clip_y_out,
                                int *clip_w_out,
                                int *clip_h_out)
{
    int clip_x;
    int clip_y;
    int clip_w;
    int clip_h;

    clip_x = 0;
    clip_y = 0;
    clip_w = 0;
    clip_h = 0;

    if (config != NULL) {
        clip_x = config->clip_x;
        clip_y = config->clip_y;
        clip_w = config->clip_width;
        clip_h = config->clip_height;
    }

    if (clip_w <= 0 || clip_h <= 0) {
        clip_x = 0;
        clip_y = 0;
        clip_w = width;
        clip_h = height;
    } else {
        if (clip_w + clip_x > width) {
            if (clip_x > width) {
                clip_w = 0;
            } else {
                clip_w = width - clip_x;
            }
        }

        if (clip_h + clip_y > height) {
            if (clip_y > height) {
                clip_h = 0;
            } else {
                clip_h = height - clip_y;
            }
        }
    }

    *clip_x_out = clip_x;
    *clip_y_out = clip_y;
    *clip_w_out = clip_w;
    *clip_h_out = clip_h;
}

SIXELSTATUS
sixel_filter_clip_frame(const sixel_filter_clip_config_t *config,
                        sixel_frame_t *frame,
                        sixel_logger_t *logger)
{
    SIXELSTATUS status;
    int src_width;
    int src_height;
    int clip_x;
    int clip_y;
    int clip_w;
    int clip_h;

    status = SIXEL_FALSE;
    src_width = 0;
    src_height = 0;
    clip_x = 0;
    clip_y = 0;
    clip_w = 0;
    clip_h = 0;

    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    src_width = sixel_frame_get_width(frame);
    src_height = sixel_frame_get_height(frame);

    sixel_filter_clip_adjust_bounds(config,
                                    src_width,
                                    src_height,
                                    &clip_x,
                                    &clip_y,
                                    &clip_w,
                                    &clip_h);

    /* Clip silently becomes a no-op when the region is empty. */
    if (clip_w > 0 && clip_h > 0) {
        status = sixel_frame_clip(frame, clip_x, clip_y, clip_w, clip_h);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    /* Logger hook reserved for planner-driven progress reporting. */
    (void)logger;

    status = SIXEL_OK;

    return status;
}

static SIXELSTATUS
sixel_filter_clip_apply(sixel_filter_t *filter,
                        sixel_allocator_t *allocator,
                        sixel_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_clip_state_t *state;
    sixel_frame_t *input_frame;

    status = SIXEL_FALSE;
    state = NULL;
    input_frame = NULL;

    if (filter == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_clip_state_t *)filter->userdata;
    if (state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (filter->input.slot == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    input_frame = *(filter->input.slot);
    if (input_frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_filter_clip_frame(&state->config, input_frame, logger);
    if (SIXEL_SUCCEEDED(status)) {
        if (filter->output.slot != NULL) {
            *(filter->output.slot) = input_frame;
        }
        filter->progress.total_units = 1;
        filter->progress.completed_units = 1;
        sixel_filter_update_progress(filter, 1);
    }

    return status;
}

static void
sixel_filter_clip_dispose(sixel_filter_t *filter)
{
    sixel_filter_clip_state_t *state;

    state = NULL;

    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_clip_state_t *)filter->userdata;
    if (state != NULL) {
        free(state);
    }
}

SIXELSTATUS
sixel_filter_clip_init(sixel_filter_t *filter,
                       const sixel_filter_clip_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_clip_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL || config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_clip_state_t *)malloc(sizeof(*state));
    if (state == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    state->config = *config;

    status = sixel_filter_init(filter,
                               "clip",
                               SIXEL_FILTER_KIND_CLIP,
                               sixel_filter_clip_apply,
                               sixel_filter_clip_dispose,
                               state);
    if (SIXEL_FAILED(status)) {
        free(state);
    }

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
