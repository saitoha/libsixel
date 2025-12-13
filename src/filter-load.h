/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FILTER_LOAD_H
#define LIBSIXEL_FILTER_LOAD_H

#include <sixel.h>

#include "filter.h"
#include "logger.h"

/*
 * Load filter configuration. The loader callback is expected to populate the
 * output slot with a freshly decoded frame and return ownership to the caller
 * who owns the slot.
 */
typedef SIXELSTATUS (*sixel_filter_load_fn)(void *userdata,
                                            sixel_frame_t **frame_out,
                                            sixel_allocator_t *allocator,
                                            sixel_logger_t *logger);

typedef struct sixel_filter_load_config {
    sixel_filter_load_fn loader;
    void *loader_userdata;

    /* Optional explicit slot to hand to the loader. */
    sixel_frame_t **frame_slot;
} sixel_filter_load_config_t;

SIXELSTATUS
sixel_filter_load_init(sixel_filter_t *filter,
                       const sixel_filter_load_config_t *config);

#endif /* LIBSIXEL_FILTER_LOAD_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
