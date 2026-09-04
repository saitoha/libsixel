/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FILTER_FACTORY_BINNING_H
#define LIBSIXEL_FILTER_FACTORY_BINNING_H

#include <sixel.h>

#include "filter-binning.h"
#include "filter.h"

/*
 * Narrow factory entry point for users that must not link the complete
 * generic registry, such as the loader helper archive.
 */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_filter_factory_create_binning(
    sixel_filter_binning_config_t const *config,
    sixel_filter_t **filter_out);

#endif /* LIBSIXEL_FILTER_FACTORY_BINNING_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
