/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FILTER_DITHER_H
#define LIBSIXEL_FILTER_DITHER_H

#include <sixel.h>

#include "filter.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Configuration for the dither filter. The filter does not take ownership of
 * the supplied dither object; it merely prepares it for the current frame and
 * reports progress so the planner/logger can record the timing of the palette
 * application stage.
 */
typedef struct sixel_filter_dither_config {
    sixel_dither_t *dither;
} sixel_filter_dither_config_t;

SIXELSTATUS
sixel_filter_dither_init(sixel_filter_t *filter,
                         const sixel_filter_dither_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_FILTER_DITHER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
