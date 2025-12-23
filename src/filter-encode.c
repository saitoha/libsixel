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

#include "filter-encode.h"
#include "filter.h"
#include "output.h"
#include "pixelformat.h"
#include "status.h"

/*
 * Internal snapshot of the encode configuration. The filter copies the config
 * so the planner can build graphs without keeping caller-owned structs alive.
 */
typedef struct sixel_filter_encode_state {
    sixel_filter_encode_config_t config;
} sixel_filter_encode_state_t;

SIXELSTATUS
sixel_filter_encode_frame(const sixel_filter_encode_config_t *config,
                          sixel_frame_t *frame,
                          sixel_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_output_t *output;
    int width;
    int height;
    int pixelformat;
    int depth;
    int frame_colorspace;
    unsigned char *pixels;

    status = SIXEL_FALSE;
    output = NULL;
    width = 0;
    height = 0;
    pixelformat = 0;
    depth = 0;
    frame_colorspace = SIXEL_COLORSPACE_GAMMA;
    pixels = NULL;

    if (config == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (config->dither == NULL || config->output == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    output = config->output;
    pixelformat = sixel_frame_get_pixelformat(frame);
    frame_colorspace = sixel_frame_get_colorspace(frame);
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;
    output->colorspace = config->output_colorspace;
    sixel_dither_set_pixelformat(config->dither, pixelformat);

    depth = sixel_helper_compute_depth(pixelformat);
    if (depth < 0) {
        sixel_helper_set_additional_message(
            "sixel_filter_encode_frame: invalid pixelformat depth.");
        return SIXEL_LOGIC_ERROR;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    if (width < 1 || height < 1) {
        sixel_helper_set_additional_message(
            "sixel_filter_encode_frame: non-positive frame dimensions.");
        return SIXEL_BAD_ARGUMENT;
    }

    pixels = sixel_frame_get_pixels(frame);
    if (pixels == NULL) {
        sixel_helper_set_additional_message(
            "sixel_filter_encode_frame: frame pixels are null.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (logger != NULL) {
        sixel_logger_logf(logger,
                          "filter",
                          "worker",
                          "encode-start",
                          -1,
                          -1,
                          0,
                          0,
                          width,
                          height,
                          "fmt=%08x depth=%d dst_cs=%d",
                          pixelformat,
                          depth,
                          output->colorspace);
    }

    status = sixel_encode(pixels,
                          width,
                          height,
                          depth,
                          config->dither,
                          output);

    if (logger != NULL && SIXEL_SUCCEEDED(status)) {
        sixel_logger_logf(logger,
                          "filter",
                          "worker",
                          "encode-finish",
                          -1,
                          -1,
                          0,
                          0,
                          width,
                          height,
                          "fmt=%08x depth=%d dst_cs=%d",
                          pixelformat,
                          depth,
                          output->colorspace);
    }

    return status;
}

static SIXELSTATUS
sixel_filter_encode_apply(sixel_filter_t *filter,
                          sixel_allocator_t *allocator,
                          sixel_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_encode_state_t *state;
    sixel_frame_t *frame;
    int height;

    (void)allocator;

    status = SIXEL_FALSE;
    state = NULL;
    frame = NULL;
    height = 0;

    if (filter == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_encode_state_t *)filter->userdata;
    if (state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (filter->input.slot == NULL || filter->input.slot[0] == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame = filter->input.slot[0];
    height = sixel_frame_get_height(frame);
    if (height < 0) {
        height = 0;
    }

    status = sixel_filter_encode_frame(&state->config, frame, logger);
    if (SIXEL_SUCCEEDED(status)) {
        filter->progress.total_units = height;
        filter->progress.completed_units = height;
        (void)sixel_filter_update_progress(filter, height);
    }

    return status;
}

static void
sixel_filter_encode_dispose(sixel_filter_t *filter)
{
    sixel_filter_encode_state_t *state;

    state = NULL;
    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_encode_state_t *)filter->userdata;
    if (state != NULL) {
        free(state);
        filter->userdata = NULL;
    }
}

SIXELSTATUS
sixel_filter_encode_init(sixel_filter_t *filter,
                         const sixel_filter_encode_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_encode_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL || config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_encode_state_t *)calloc(
        1u, sizeof(sixel_filter_encode_state_t));
    if (state == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    state->config = *config;

    status = sixel_filter_init(filter,
                               "encode",
                               SIXEL_FILTER_KIND_ENCODE,
                               sixel_filter_encode_apply,
                               sixel_filter_encode_dispose,
                               state);
    if (SIXEL_FAILED(status)) {
        free(state);
        return status;
    }

    filter->flags |= SIXEL_FILTER_FLAG_PIPELINED;
    filter->progress.total_units = 0;
    filter->progress.completed_units = 0;

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
