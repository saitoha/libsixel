/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FILTER_CLIP_H
#define LIBSIXEL_FILTER_CLIP_H

#include <sixel.h>

#include "filter.h"
#include "logger.h"

/*
 * Clip filter configuration. The planner passes the region to preserve and the
 * filter trims the frame in-place. A zero or negative width/height disables the
 * operation.
 */
typedef struct sixel_filter_clip_config {
    int clip_x;
    int clip_y;
    int clip_width;
    int clip_height;
} sixel_filter_clip_config_t;

SIXELSTATUS
sixel_filter_clip_init(sixel_filter_t *filter,
                       const sixel_filter_clip_config_t *config);

SIXELSTATUS
sixel_filter_clip_frame(const sixel_filter_clip_config_t *config,
                        sixel_frame_t *frame,
                        sixel_logger_t *logger);

#endif /* LIBSIXEL_FILTER_CLIP_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
