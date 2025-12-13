/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FILTER_PALETTE_H
#define LIBSIXEL_FILTER_PALETTE_H

#include <sixel.h>

#include "dither.h"
#include "filter.h"

typedef SIXELSTATUS (*sixel_filter_palette_builder_fn)(
        void *userdata,
        sixel_frame_t *frame,
        sixel_dither_t **dither_out,
        sixel_logger_t *logger);

typedef struct sixel_filter_palette_config {
    sixel_filter_palette_builder_fn builder;
    void *builder_userdata;
    sixel_dither_t **dither_out;
} sixel_filter_palette_config_t;

SIXELSTATUS
sixel_filter_palette_init(sixel_filter_t *filter,
                          const sixel_filter_palette_config_t *config);

#endif /* LIBSIXEL_FILTER_PALETTE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
