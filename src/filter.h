/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LIBSIXEL_FILTER_H
#define LIBSIXEL_FILTER_H

#include <sixel.h>

#include "frame.h"
#include "timeline-logger.h"

/*
 * Filter interface shared by resampling, clipping, colorspace conversion,
 * lookup, dithering, and encoding. The planner binds input/output slots and
 * progress hooks, while concrete filters implement the apply callback.
 */

typedef enum sixel_filter_kind {
    SIXEL_FILTER_KIND_GENERIC = 0,
    SIXEL_FILTER_KIND_LOAD,
    SIXEL_FILTER_KIND_SAMPLE,
    SIXEL_FILTER_KIND_PALETTE,
    SIXEL_FILTER_KIND_COLORS,
    SIXEL_FILTER_KIND_RESIZE,
    SIXEL_FILTER_KIND_CLIP,
    SIXEL_FILTER_KIND_LOOKUP,
    SIXEL_FILTER_KIND_FINAL_MERGE,
    SIXEL_FILTER_KIND_FHEDT,
    SIXEL_FILTER_KIND_VPTREE,
    SIXEL_FILTER_KIND_EYTZINGER,
    SIXEL_FILTER_KIND_GRADIENT,
    SIXEL_FILTER_KIND_DITHER,
    SIXEL_FILTER_KIND_ENCODE,
} sixel_filter_kind_t;

typedef enum sixel_filter_flags {
    /*
     * Flag indicating that downstream consumption can start while the filter
     * is still producing output (e.g., dither -> encode pipeline).
     */
    SIXEL_FILTER_FLAG_PIPELINED = 1 << 0,
} sixel_filter_flags_t;

typedef enum sixel_filter_event {
    SIXEL_FILTER_EVENT_BEGIN = 0,
    SIXEL_FILTER_EVENT_PROGRESS,
    SIXEL_FILTER_EVENT_COMPLETE,
    SIXEL_FILTER_EVENT_ABORT,
} sixel_filter_event_t;

typedef struct sixel_filter_io {
    /*
     * Slot storing the frame pointer owned by the planner. The filter reads
     * from input.slot and may replace *output.slot with a new frame.
     */
    sixel_frame_t **slot;

    /*
     * Expected pixel format and colorspace at this edge. These are set by the
     * planner when building the DAG so filters can validate inputs before
     * running.
     */
    int pixelformat;
    int colorspace;
} sixel_filter_io_t;

typedef struct sixel_filter sixel_filter_t;
typedef struct sixel_filter_vtbl sixel_filter_vtbl_t;

typedef SIXELSTATUS (*sixel_filter_apply_fn)(sixel_filter_t *filter,
                                             sixel_allocator_t *allocator,
                                             sixel_timeline_logger_t *logger);

typedef void (*sixel_filter_dispose_fn)(sixel_filter_t *filter);

typedef SIXELSTATUS (*sixel_filter_validate_fn)(sixel_filter_t *filter);

typedef SIXELSTATUS (*sixel_filter_prepare_fn)(sixel_filter_t *filter,
                                               sixel_allocator_t *allocator,
                                               sixel_timeline_logger_t *logger);

typedef int (*sixel_filter_can_pipeline_fn)(sixel_filter_t const *filter);

typedef void (*sixel_filter_progress_fn)(sixel_filter_t *filter,
                                         sixel_filter_event_t event,
                                         int completed_units,
                                         int total_units,
                                         void *userdata);

typedef struct sixel_filter_progress {
    /*
     * Total amount of logical work units (e.g., rows) expected. This allows
     * planners and loggers to aggregate completion ratios without peeking into
     * filter internals.
     */
    int total_units;

    /*
     * Work units completed so far. Filters should update this through
     * sixel_filter_update_progress() to notify observers.
     */
    int completed_units;
} sixel_filter_progress_t;

struct sixel_filter_vtbl {
    char const *type_name;
    sixel_filter_kind_t kind;
    sixel_filter_apply_fn apply;
    sixel_filter_dispose_fn dispose;
    sixel_filter_validate_fn validate;
    sixel_filter_prepare_fn prepare;
    sixel_filter_can_pipeline_fn can_pipeline;
};

struct sixel_filter {
    const char *name;
    sixel_filter_kind_t kind;
    sixel_filter_vtbl_t const *vtbl;
    unsigned int flags;

    sixel_filter_io_t input;
    sixel_filter_io_t output;

    /*
     * Legacy initializer stores callbacks in this embedded vtable so the
     * runtime always dispatches through filter->vtbl without temporary
     * objects or dual callback paths.
     */
    sixel_filter_vtbl_t legacy_vtbl;
    void *userdata;

    sixel_filter_progress_t progress;
    sixel_filter_progress_fn progress_cb;
    void *progress_userdata;
};

SIXEL_INTERNAL_API void sixel_filter_clear(sixel_filter_t *filter);

SIXEL_INTERNAL_API SIXELSTATUS sixel_filter_alloc(sixel_filter_t **filter_out);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_filter_init_with_vtbl(sixel_filter_t *filter,
                            sixel_filter_vtbl_t const *vtbl,
                            void *userdata);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_filter_init(sixel_filter_t *filter,
                  const char *name,
                  sixel_filter_kind_t kind,
                  sixel_filter_apply_fn apply,
                  sixel_filter_dispose_fn dispose,
                  void *userdata);

SIXEL_INTERNAL_API void sixel_filter_bind_input(sixel_filter_t *filter,
                                      sixel_frame_t **slot,
                                      int pixelformat,
                                      int colorspace);

SIXEL_INTERNAL_API void sixel_filter_bind_output(sixel_filter_t *filter,
                                       sixel_frame_t **slot,
                                       int pixelformat,
                                       int colorspace);

SIXEL_INTERNAL_API void sixel_filter_set_progress(sixel_filter_t *filter,
                                        sixel_filter_progress_fn progress_cb,
                                        void *progress_userdata,
                                        int total_units);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_filter_update_progress(sixel_filter_t *filter, int completed_units);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_filter_run(sixel_filter_t *filter,
                 sixel_allocator_t *allocator,
                 sixel_timeline_logger_t *logger);

SIXEL_INTERNAL_API void sixel_filter_teardown(sixel_filter_t *filter);

SIXEL_INTERNAL_API void sixel_filter_free(sixel_filter_t *filter);

#endif /* LIBSIXEL_FILTER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
