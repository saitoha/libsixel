/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FILTER_FINAL_MERGE_H
#define LIBSIXEL_FILTER_FINAL_MERGE_H

#include <sixel.h>

#include "filter.h"

/*
 * Configuration for the final-merge filter. The filter updates the dither's
 * merge policy so palette clusters are consolidated according to the selected
 * mode. It is a light-weight control stage without frame I/O.
 */
typedef struct sixel_filter_final_merge_config {
    sixel_dither_t *dither;
    int final_merge_mode;
} sixel_filter_final_merge_config_t;

SIXELSTATUS
sixel_filter_final_merge_init(
    sixel_filter_t *filter,
    const sixel_filter_final_merge_config_t *config);

SIXELAPI SIXELSTATUS
sixel_filter_final_merge_apply(
    const sixel_filter_final_merge_config_t *config,
    sixel_logger_t *logger);

#endif /* LIBSIXEL_FILTER_FINAL_MERGE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
