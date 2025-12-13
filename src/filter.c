/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#include "config.h"

/* STDC_HEADERS */
#include <string.h>

#include <sixel.h>

#include "filter.h"

static void sixel_filter_emit_progress(sixel_filter_t *filter,
                                       sixel_filter_event_t event);

static void
sixel_filter_emit_progress(sixel_filter_t *filter, sixel_filter_event_t event)
{
    int completed;
    int total;

    if (filter == NULL) {
        return;
    }

    completed = filter->progress.completed_units;
    total = filter->progress.total_units;
    if (filter->progress_cb != NULL) {
        filter->progress_cb(filter, event, completed, total,
                            filter->progress_userdata);
    }
}

void
sixel_filter_clear(sixel_filter_t *filter)
{
    if (filter == NULL) {
        return;
    }

    memset(filter, 0, sizeof(*filter));
}

SIXELSTATUS
sixel_filter_init(sixel_filter_t *filter,
                  const char *name,
                  sixel_filter_kind_t kind,
                  sixel_filter_apply_fn apply,
                  sixel_filter_dispose_fn dispose,
                  void *userdata)
{
    SIXELSTATUS status;

    if (filter == NULL || apply == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_filter_clear(filter);

    filter->name = name;
    filter->kind = kind;
    filter->flags = 0;
    filter->apply = apply;
    filter->dispose = dispose;
    filter->userdata = userdata;

    status = SIXEL_OK;

    return status;
}

void
sixel_filter_bind_input(sixel_filter_t *filter,
                        sixel_frame_t **slot,
                        int pixelformat,
                        int colorspace)
{
    if (filter == NULL) {
        return;
    }

    filter->input.slot = slot;
    filter->input.pixelformat = pixelformat;
    filter->input.colorspace = colorspace;
}

void
sixel_filter_bind_output(sixel_filter_t *filter,
                         sixel_frame_t **slot,
                         int pixelformat,
                         int colorspace)
{
    if (filter == NULL) {
        return;
    }

    filter->output.slot = slot;
    filter->output.pixelformat = pixelformat;
    filter->output.colorspace = colorspace;
}

void
sixel_filter_set_progress(sixel_filter_t *filter,
                          sixel_filter_progress_fn progress_cb,
                          void *progress_userdata,
                          int total_units)
{
    if (filter == NULL) {
        return;
    }

    filter->progress_cb = progress_cb;
    filter->progress_userdata = progress_userdata;
    filter->progress.total_units = total_units;
    filter->progress.completed_units = 0;
}

SIXELSTATUS
sixel_filter_update_progress(sixel_filter_t *filter, int completed_units)
{
    SIXELSTATUS status;

    if (filter == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    filter->progress.completed_units = completed_units;
    sixel_filter_emit_progress(filter, SIXEL_FILTER_EVENT_PROGRESS);

    status = SIXEL_OK;

    return status;
}

SIXELSTATUS
sixel_filter_run(sixel_filter_t *filter,
                 sixel_allocator_t *allocator,
                 sixel_logger_t *logger)
{
    SIXELSTATUS status;

    if (filter == NULL || filter->apply == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_filter_emit_progress(filter, SIXEL_FILTER_EVENT_BEGIN);
    status = filter->apply(filter, allocator, logger);
    if (SIXEL_SUCCEEDED(status)) {
        sixel_filter_emit_progress(filter, SIXEL_FILTER_EVENT_COMPLETE);
    } else {
        sixel_filter_emit_progress(filter, SIXEL_FILTER_EVENT_ABORT);
    }

    return status;
}

void
sixel_filter_teardown(sixel_filter_t *filter)
{
    if (filter == NULL) {
        return;
    }

    if (filter->dispose != NULL) {
        filter->dispose(filter);
    }

    sixel_filter_clear(filter);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
