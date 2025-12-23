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
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "filter-sample.h"
#include "filter.h"
#include "pixelformat.h"

typedef struct sixel_filter_sample_state {
    sixel_filter_sample_config_t config;
} sixel_filter_sample_state_t;

static size_t
sixel_filter_sample_select_stride(
        sixel_filter_sample_config_t const *config,
        int width,
        int height)
{
    size_t stride;
    size_t base_target;
    size_t color_budget;
    size_t target;
    size_t total;

    stride = 1u;
    base_target = 4096u;
    color_budget = 0u;
    target = base_target;
    total = 0u;

    if (width <= 0 || height <= 0) {
        return stride;
    }

    if (config != NULL && config->palette_sample_override != 0) {
        target = config->palette_sample_target;
    } else {
        if (config != NULL && config->reqcolors > 0) {
            color_budget = (size_t)config->reqcolors * 64u;
            if (color_budget / 64u != (size_t)config->reqcolors) {
                color_budget = base_target;
            }
            if (color_budget > target) {
                target = color_budget;
            }
        }

        if (config != NULL
                && (config->quality_mode == SIXEL_QUALITY_HIGH
                    || config->quality_mode == SIXEL_QUALITY_HIGHCOLOR)) {
            if (target <= SIZE_MAX / 2u) {
                target *= 2u;
            } else {
                target = SIZE_MAX;
            }
        } else if (config != NULL
                && config->quality_mode == SIXEL_QUALITY_FULL) {
            if (target <= SIZE_MAX / 4u) {
                target *= 4u;
            } else {
                target = SIZE_MAX;
            }
        }
    }

    total = (size_t)width * (size_t)height;
    while (stride < total && total / (stride * stride) > target) {
        ++stride;
    }

    return stride;
}

static SIXELSTATUS
sixel_filter_sample_copy_frame(
        sixel_filter_sample_config_t const *config,
        sixel_frame_t *frame,
        sixel_allocator_t *allocator,
        sixel_frame_t **sample_out,
        sixel_logger_t *logger,
        int *sample_width_out,
        int *sample_height_out)
{
    SIXELSTATUS status;
    sixel_frame_t *sample;
    unsigned char *src_pixels;
    unsigned char *dst_pixels;
    size_t stride;
    int clip_x;
    int clip_y;
    int clip_w;
    int clip_h;
    int src_width;
    int src_height;
    int width;
    int height;
    int depth;
    int sample_width;
    int sample_height;
    size_t sample_count;
    size_t payload_size;
    size_t dst_index;
    size_t src_offset;
    int x;
    int y;

    status = SIXEL_FALSE;
    sample = NULL;
    src_pixels = NULL;
    dst_pixels = NULL;
    stride = 1u;
    clip_x = 0;
    clip_y = 0;
    clip_w = 0;
    clip_h = 0;
    src_width = 0;
    src_height = 0;
    width = 0;
    height = 0;
    depth = 0;
    sample_width = 0;
    sample_height = 0;
    sample_count = 0u;
    payload_size = 0u;
    dst_index = 0u;
    src_offset = 0u;
    x = 0;
    y = 0;

    if (frame == NULL || sample_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_PALETTE)) {
        return SIXEL_FEATURE_ERROR;
    }

    src_pixels = sixel_frame_get_pixels(frame);
    src_width = sixel_frame_get_width(frame);
    src_height = sixel_frame_get_height(frame);
    depth = sixel_helper_compute_depth(sixel_frame_get_pixelformat(frame));

    if (depth <= 0 || src_pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * The logger is currently unused, but it will be wired once planner
     * driven progress messages are emitted from the filter layer.
     */
    (void)logger;

    if (config != NULL) {
        clip_x = config->clip_x;
        clip_y = config->clip_y;
        clip_w = config->clip_width;
        clip_h = config->clip_height;
    }

    if (clip_w <= 0 || clip_h <= 0) {
        clip_x = 0;
        clip_y = 0;
        clip_w = src_width;
        clip_h = src_height;
    } else {
        if (clip_w + clip_x > src_width) {
            if (clip_x > src_width) {
                clip_w = 0;
            } else {
                clip_w = src_width - clip_x;
            }
        }

        if (clip_h + clip_y > src_height) {
            if (clip_y > src_height) {
                clip_h = 0;
            } else {
                clip_h = src_height - clip_y;
            }
        }

        if (clip_w <= 0 || clip_h <= 0) {
            return SIXEL_BAD_ARGUMENT;
        }
    }

    width = clip_w;
    height = clip_h;

    stride = sixel_filter_sample_select_stride(config, width, height);
    sample_width = (width + (int)stride - 1) / (int)stride;
    sample_height = (height + (int)stride - 1) / (int)stride;
    if (sample_width <= 0 || sample_height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (sample_width_out != NULL) {
        *sample_width_out = sample_width;
    }
    if (sample_height_out != NULL) {
        *sample_height_out = sample_height;
    }

    sample_count = (size_t)sample_width * (size_t)sample_height;
    if (sample_count != 0u
            && sample_count / (size_t)sample_height != (size_t)sample_width) {
        return SIXEL_RUNTIME_ERROR;
    }

    payload_size = sample_count * (size_t)depth;
    if (sample_count != 0u && payload_size / sample_count != (size_t)depth) {
        return SIXEL_RUNTIME_ERROR;
    }

    status = sixel_frame_new(&sample, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    dst_pixels = (unsigned char *)sixel_allocator_malloc(allocator,
                                                         payload_size);
    if (dst_pixels == NULL) {
        sixel_frame_unref(sample);
        return SIXEL_BAD_ALLOCATION;
    }

    sample->pixels.u8ptr = dst_pixels;
    sample->width = sample_width;
    sample->height = sample_height;
    sample->pixelformat = sixel_frame_get_pixelformat(frame);
    sample->colorspace = sixel_frame_get_colorspace(frame);
    sample->ncolors = (-1);

    dst_index = 0u;
    for (y = 0; y < height; y += (int)stride) {
        for (x = 0; x < width; x += (int)stride) {
            src_offset = ((size_t)(clip_y + y) * (size_t)src_width
                       + (size_t)(clip_x + x))
                       * (size_t)depth;
            memcpy(dst_pixels + dst_index * (size_t)depth,
                   src_pixels + src_offset,
                   (size_t)depth);
            ++dst_index;
        }

    }

    *sample_out = sample;

    return SIXEL_OK;
}

SIXELSTATUS
sixel_filter_sample_frame(const sixel_filter_sample_config_t *config,
                          sixel_frame_t *frame,
                          sixel_allocator_t *allocator,
                          sixel_frame_t **sample_out,
                          sixel_logger_t *logger)
{
    SIXELSTATUS status;

    status = sixel_filter_sample_copy_frame(config, frame, allocator,
                                            sample_out, logger, NULL, NULL);

    return status;
}

static SIXELSTATUS
sixel_filter_sample_apply(sixel_filter_t *filter,
                          sixel_allocator_t *allocator,
                          sixel_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_sample_state_t *state;
    sixel_frame_t *input_frame;
    sixel_frame_t *sample;
    int sample_height;

    if (filter == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_sample_state_t *)filter->userdata;
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

    if (*(filter->output.slot) != NULL) {
        sixel_frame_unref(*(filter->output.slot));
        *(filter->output.slot) = NULL;
    }

    sample_height = 0;
    status = sixel_filter_sample_copy_frame(&state->config, input_frame,
                                            allocator, &sample, logger,
                                            NULL, &sample_height);
    if (SIXEL_SUCCEEDED(status)) {
        *(filter->output.slot) = sample;
        if (sample_height > 0) {
            filter->progress.total_units = sample_height;
            filter->progress.completed_units = sample_height;
            sixel_filter_update_progress(filter, sample_height);
        }
    }

    return status;
}

static void
sixel_filter_sample_dispose(sixel_filter_t *filter)
{
    sixel_filter_sample_state_t *state;

    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_sample_state_t *)filter->userdata;
    if (state != NULL) {
        free(state);
    }
}

SIXELSTATUS
sixel_filter_sample_init(sixel_filter_t *filter,
                         const sixel_filter_sample_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_sample_state_t *state;

    if (filter == NULL || config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_sample_state_t *)malloc(sizeof(*state));
    if (state == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    state->config = *config;

    status = sixel_filter_init(filter,
                               "sample",
                               SIXEL_FILTER_KIND_SAMPLE,
                               sixel_filter_sample_apply,
                               sixel_filter_sample_dispose,
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
