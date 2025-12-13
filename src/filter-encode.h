/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FILTER_ENCODE_H
#define LIBSIXEL_FILTER_ENCODE_H

#include <sixel.h>

#include "filter.h"
#include "logger.h"

/*
 * Encode filter configuration. The planner supplies the prepared dither and
 * output objects. Ownership stays with the caller; the filter only borrows the
 * pointers while encoding the bound frame.
 */
typedef struct sixel_filter_encode_config {
    sixel_dither_t *dither;
    sixel_output_t *output;

    /*
     * Desired output colorspace. The filter writes this to the output object
     * before invoking sixel_encode(), allowing the planner to control the
     * emitted stream's colorspace independent of the input frame.
     */
    int output_colorspace;
} sixel_filter_encode_config_t;

SIXELSTATUS
sixel_filter_encode_init(sixel_filter_t *filter,
                         const sixel_filter_encode_config_t *config);

SIXELSTATUS
sixel_filter_encode_frame(const sixel_filter_encode_config_t *config,
                          sixel_frame_t *frame,
                          sixel_logger_t *logger);

#endif /* LIBSIXEL_FILTER_ENCODE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
