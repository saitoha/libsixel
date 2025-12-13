/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FILTER_H
#define LIBSIXEL_FILTER_H

#include <sixel.h>

#include "frame.h"
#include "logger.h"

/*
 * Filter interface shared by resampling, clipping, colorspace conversion,
 * lookup, dithering, and encoding. The planner binds input/output slots and
 * progress hooks, while concrete filters implement the apply callback.
 */

typedef enum sixel_filter_kind {
    SIXEL_FILTER_KIND_GENERIC = 0,
    SIXEL_FILTER_KIND_SAMPLE,
    SIXEL_FILTER_KIND_PALETTE,
    SIXEL_FILTER_KIND_COLORS,
    SIXEL_FILTER_KIND_RESIZE,
    SIXEL_FILTER_KIND_CLIP,
    SIXEL_FILTER_KIND_LOOKUP,
    SIXEL_FILTER_KIND_FINAL_MERGE,
    SIXEL_FILTER_KIND_VPTE,
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

typedef SIXELSTATUS (*sixel_filter_apply_fn)(sixel_filter_t *filter,
                                             sixel_allocator_t *allocator,
                                             sixel_logger_t *logger);

typedef void (*sixel_filter_dispose_fn)(sixel_filter_t *filter);

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

struct sixel_filter {
    const char *name;
    sixel_filter_kind_t kind;
    unsigned int flags;

    sixel_filter_io_t input;
    sixel_filter_io_t output;

    sixel_filter_apply_fn apply;
    sixel_filter_dispose_fn dispose;
    void *userdata;

    sixel_filter_progress_t progress;
    sixel_filter_progress_fn progress_cb;
    void *progress_userdata;
};

void sixel_filter_clear(sixel_filter_t *filter);

SIXELSTATUS sixel_filter_alloc(sixel_filter_t **filter_out);

SIXELSTATUS
sixel_filter_init(sixel_filter_t *filter,
                  const char *name,
                  sixel_filter_kind_t kind,
                  sixel_filter_apply_fn apply,
                  sixel_filter_dispose_fn dispose,
                  void *userdata);

void sixel_filter_bind_input(sixel_filter_t *filter,
                             sixel_frame_t **slot,
                             int pixelformat,
                             int colorspace);

void sixel_filter_bind_output(sixel_filter_t *filter,
                              sixel_frame_t **slot,
                              int pixelformat,
                              int colorspace);

void sixel_filter_set_progress(sixel_filter_t *filter,
                               sixel_filter_progress_fn progress_cb,
                               void *progress_userdata,
                               int total_units);

SIXELSTATUS
sixel_filter_update_progress(sixel_filter_t *filter, int completed_units);

SIXELSTATUS
sixel_filter_run(sixel_filter_t *filter,
                 sixel_allocator_t *allocator,
                 sixel_logger_t *logger);

void sixel_filter_teardown(sixel_filter_t *filter);

void sixel_filter_free(sixel_filter_t *filter);

#endif /* LIBSIXEL_FILTER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
