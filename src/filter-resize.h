/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FILTER_RESIZE_H
#define LIBSIXEL_FILTER_RESIZE_H

#include <sixel.h>

#include "filter.h"
#include "logger.h"

/*
 * Resize filter configuration. The planner populates target dimensions and the
 * preferred resampling strategy. Percent-based sizing is resolved inside the
 * filter using the source frame size.
 */
typedef struct sixel_filter_resize_config {
    int pixel_width;
    int pixel_height;
    int percent_width;
    int percent_height;
    int method_for_resampling;
    int prefer_float32;
    int planner_scale_pixelformat;
} sixel_filter_resize_config_t;

SIXELSTATUS
sixel_filter_resize_init(sixel_filter_t *filter,
                         const sixel_filter_resize_config_t *config);

SIXELSTATUS
sixel_filter_resize_frame(const sixel_filter_resize_config_t *config,
                          sixel_frame_t *frame,
                          sixel_logger_t *logger);

#endif /* LIBSIXEL_FILTER_RESIZE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
