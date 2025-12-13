/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FILTER_SAMPLE_H
#define LIBSIXEL_FILTER_SAMPLE_H

#include <sixel.h>

#include "filter.h"
#include "logger.h"

/*
 * Sample filter configuration. The planner fills this before wiring the
 * filter so apply() can copy the requested region with the right density.
 */
typedef struct sixel_filter_sample_config {
    /*
     * Visible region used for palette sampling. When width/height are not
     * positive, the full frame bounds are used instead.
     */
    int clip_x;
    int clip_y;
    int clip_width;
    int clip_height;

    /*
     * Palette sizing knobs carried from encoder options. The override flag
     * mirrors $SIXEL_PALETTE_SAMPLE_TARGET support.
     */
    int reqcolors;
    int quality_mode;
    int palette_sample_override;
    size_t palette_sample_target;
} sixel_filter_sample_config_t;

SIXELSTATUS
sixel_filter_sample_init(sixel_filter_t *filter,
                         const sixel_filter_sample_config_t *config);

SIXELSTATUS
sixel_filter_sample_frame(const sixel_filter_sample_config_t *config,
                          sixel_frame_t *frame,
                          sixel_allocator_t *allocator,
                          sixel_frame_t **sample_out,
                          sixel_logger_t *logger);

#endif /* LIBSIXEL_FILTER_SAMPLE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
