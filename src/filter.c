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
#include <string.h>

#include <sixel.h>

#include "filter.h"

static void sixel_filter_emit_progress(sixel_filter_t *filter,
                                       sixel_filter_event_t event);
static SIXELSTATUS
sixel_filter_invoke_prepare_and_validate(sixel_filter_t *filter,
                                         sixel_allocator_t *allocator,
                                         sixel_logger_t *logger);

static SIXELSTATUS
sixel_filter_invoke_prepare_and_validate(sixel_filter_t *filter,
                                         sixel_allocator_t *allocator,
                                         sixel_logger_t *logger)
{
    SIXELSTATUS status;

    status = SIXEL_OK;

    if (filter->vtbl->prepare != NULL) {
        status = filter->vtbl->prepare(filter, allocator, logger);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    if (filter->vtbl->validate != NULL) {
        status = filter->vtbl->validate(filter);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return status;
}

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

SIXELAPI void
sixel_filter_clear(sixel_filter_t *filter)
{
    if (filter == NULL) {
        return;
    }

    memset(filter, 0, sizeof(*filter));
}

SIXELAPI SIXELSTATUS
sixel_filter_alloc(sixel_filter_t **filter_out)
{
    sixel_filter_t *filter;

    if (filter_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *filter_out = NULL;

    filter = (sixel_filter_t *)malloc(sizeof(*filter));
    if (filter == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    sixel_filter_clear(filter);

    *filter_out = filter;

    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_filter_init_with_vtbl(sixel_filter_t *filter,
                            sixel_filter_vtbl_t const *vtbl,
                            void *userdata)
{
    SIXELSTATUS status;

    if (filter == NULL || vtbl == NULL || vtbl->apply == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_filter_clear(filter);

    filter->name = vtbl->type_name;
    filter->kind = vtbl->kind;
    filter->vtbl = vtbl;
    filter->flags = 0;
    filter->userdata = userdata;

    status = SIXEL_OK;

    return status;
}

SIXELAPI SIXELSTATUS
sixel_filter_init(sixel_filter_t *filter,
                  const char *name,
                  sixel_filter_kind_t kind,
                  sixel_filter_apply_fn apply,
                  sixel_filter_dispose_fn dispose,
                  void *userdata)
{
    if (filter == NULL || apply == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * Keep the legacy initializer stable for existing callers while moving
     * runtime dispatch to a single vtable-based path.
     */
    sixel_filter_clear(filter);
    filter->legacy_vtbl.type_name = name;
    filter->legacy_vtbl.kind = kind;
    filter->legacy_vtbl.apply = apply;
    filter->legacy_vtbl.dispose = dispose;
    filter->legacy_vtbl.validate = NULL;
    filter->legacy_vtbl.prepare = NULL;
    filter->legacy_vtbl.can_pipeline = NULL;

    filter->name = name;
    filter->kind = kind;
    filter->vtbl = &filter->legacy_vtbl;
    filter->flags = 0;
    filter->userdata = userdata;

    return SIXEL_OK;
}

SIXELAPI void
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

SIXELAPI void
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

SIXELAPI void
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

SIXELAPI SIXELSTATUS
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

SIXELAPI SIXELSTATUS
sixel_filter_run(sixel_filter_t *filter,
                 sixel_allocator_t *allocator,
                 sixel_logger_t *logger)
{
    SIXELSTATUS status;

    if (filter == NULL || filter->vtbl == NULL || filter->vtbl->apply == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_filter_invoke_prepare_and_validate(filter,
                                                     allocator,
                                                     logger);
    if (SIXEL_FAILED(status)) {
        sixel_filter_emit_progress(filter, SIXEL_FILTER_EVENT_ABORT);
        return status;
    }

    sixel_filter_emit_progress(filter, SIXEL_FILTER_EVENT_BEGIN);
    status = filter->vtbl->apply(filter, allocator, logger);
    if (SIXEL_SUCCEEDED(status)) {
        sixel_filter_emit_progress(filter, SIXEL_FILTER_EVENT_COMPLETE);
    } else {
        sixel_filter_emit_progress(filter, SIXEL_FILTER_EVENT_ABORT);
    }

    return status;
}

SIXELAPI void
sixel_filter_teardown(sixel_filter_t *filter)
{
    if (filter == NULL) {
        return;
    }

    if (filter->vtbl != NULL && filter->vtbl->dispose != NULL) {
        filter->vtbl->dispose(filter);
    }

    sixel_filter_clear(filter);
}

SIXELAPI void
sixel_filter_free(sixel_filter_t *filter)
{
    if (filter == NULL) {
        return;
    }

    sixel_filter_teardown(filter);
    free(filter);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
