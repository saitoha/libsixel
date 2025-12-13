/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#include "config.h"

/* STDC_HEADERS */
#include <stdlib.h>

#include <sixel.h>

#include "filter-resize.h"
#include "filter.h"
#include "pixelformat.h"

/*
 * Internal snapshot of the resize configuration. The config is copied when the
 * filter is initialized to decouple runtime changes from factory-created
 * filters that may outlive the caller's stack variables.
 */
typedef struct sixel_filter_resize_state {
    sixel_filter_resize_config_t config;
} sixel_filter_resize_state_t;

static void
sixel_filter_resize_compute_target(const sixel_filter_resize_config_t *config,
                                   int src_width,
                                   int src_height,
                                   int *dst_width_out,
                                   int *dst_height_out)
{
    int dst_width;
    int dst_height;

    dst_width = 0;
    dst_height = 0;

    if (config == NULL) {
        *dst_width_out = dst_width;
        *dst_height_out = dst_height;
        return;
    }

    dst_width = config->pixel_width;
    dst_height = config->pixel_height;

    if (config->percent_width > 0) {
        dst_width = src_width * config->percent_width / 100;
    }
    if (config->percent_height > 0) {
        dst_height = src_height * config->percent_height / 100;
    }

    if (dst_width > 0 && dst_height <= 0) {
        dst_height = src_height * dst_width / src_width;
    }
    if (dst_height > 0 && dst_width <= 0) {
        dst_width = src_width * dst_height / src_height;
    }

    *dst_width_out = dst_width;
    *dst_height_out = dst_height;
}

static int
sixel_filter_resize_use_float(const sixel_filter_resize_config_t *config)
{
    int prefer_float;

    prefer_float = 0;

    if (config == NULL) {
        return prefer_float;
    }

    if (config->method_for_resampling != SIXEL_RES_NEAREST
        && SIXEL_PIXELFORMAT_IS_FLOAT32(config->planner_scale_pixelformat)) {
        prefer_float = 1;
    }

    if (config->prefer_float32 != 0) {
        prefer_float = 1;
    }

    return prefer_float;
}

SIXELSTATUS
sixel_filter_resize_frame(const sixel_filter_resize_config_t *config,
                          sixel_frame_t *frame,
                          sixel_logger_t *logger)
{
    SIXELSTATUS status;
    int src_width;
    int src_height;
    int dst_width;
    int dst_height;
    int use_float_resize;

    status = SIXEL_FALSE;
    src_width = 0;
    src_height = 0;
    dst_width = 0;
    dst_height = 0;
    use_float_resize = 0;

    if (frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    src_width = sixel_frame_get_width(frame);
    src_height = sixel_frame_get_height(frame);
    if (src_width < 1 || src_height < 1) {
        sixel_helper_set_additional_message(
            "sixel_filter_resize_frame: "
            "detected a frame with a non-positive dimension.");
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_filter_resize_compute_target(config,
                                       src_width,
                                       src_height,
                                       &dst_width,
                                       &dst_height);

    if (logger != NULL) {
        sixel_logger_logf(logger,
                          "filter",
                          "worker",
                          "scale-start",
                          -1,
                          -1,
                          0,
                          0,
                          src_width,
                          src_height,
                          "dst=%dx%d method=%d",
                          dst_width,
                          dst_height,
                          config != NULL
                              ? config->method_for_resampling
                              : SIXEL_RES_NEAREST);
    }

    if (dst_width > 0 && dst_height > 0) {
        use_float_resize = sixel_filter_resize_use_float(config);
        if (use_float_resize != 0) {
            status = sixel_frame_resize_float32(
                frame,
                dst_width,
                dst_height,
                config != NULL
                    ? config->method_for_resampling
                    : SIXEL_RES_NEAREST);
        } else {
            status = sixel_frame_resize(frame,
                                        dst_width,
                                        dst_height,
                                        config != NULL
                                            ? config->method_for_resampling
                                            : SIXEL_RES_NEAREST);
        }
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    if (logger != NULL) {
        sixel_logger_logf(logger,
                          "filter",
                          "worker",
                          "scale-finish",
                          -1,
                          -1,
                          0,
                          0,
                          sixel_frame_get_width(frame),
                          sixel_frame_get_height(frame),
                          "use-float=%d",
                          use_float_resize);
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_filter_resize_apply(sixel_filter_t *filter,
                          sixel_allocator_t *allocator,
                          sixel_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_resize_state_t *state;
    sixel_frame_t *input_frame;

    (void)allocator;

    status = SIXEL_FALSE;
    state = NULL;
    input_frame = NULL;

    if (filter == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_resize_state_t *)filter->userdata;
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

    status = sixel_filter_resize_frame(&state->config, input_frame, logger);
    if (SIXEL_SUCCEEDED(status)) {
        *(filter->output.slot) = input_frame;
        filter->progress.total_units = sixel_frame_get_height(input_frame);
        filter->progress.completed_units = filter->progress.total_units;
        sixel_filter_update_progress(filter, filter->progress.completed_units);
    }

    return status;
}

static void
sixel_filter_resize_dispose(sixel_filter_t *filter)
{
    sixel_filter_resize_state_t *state;

    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_resize_state_t *)filter->userdata;
    if (state != NULL) {
        free(state);
    }
}

SIXELSTATUS
sixel_filter_resize_init(sixel_filter_t *filter,
                         const sixel_filter_resize_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_resize_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL || config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_resize_state_t *)calloc(
        1u, sizeof(sixel_filter_resize_state_t));
    if (state == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    state->config = *config;

    status = sixel_filter_init(filter,
                               "resize",
                               SIXEL_FILTER_KIND_RESIZE,
                               sixel_filter_resize_apply,
                               sixel_filter_resize_dispose,
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
