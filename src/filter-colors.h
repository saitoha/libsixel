/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FILTER_COLORS_H
#define LIBSIXEL_FILTER_COLORS_H

#include <sixel.h>

#include "filter.h"

typedef struct sixel_filter_colors_config {
    /*
     * Pixelformat after conversion. The colorspace is derived from this
     * value so callers do not need to duplicate the working colorspace
     * separately.
     */
    int target_pixelformat;
} sixel_filter_colors_config_t;

SIXELSTATUS sixel_filter_colors_convert(
    const sixel_filter_colors_config_t *config,
    sixel_frame_t *frame,
    sixel_logger_t *logger);

SIXELSTATUS sixel_filter_colors_init(
    sixel_filter_t *filter,
    const sixel_filter_colors_config_t *config);

#endif /* LIBSIXEL_FILTER_COLORS_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
