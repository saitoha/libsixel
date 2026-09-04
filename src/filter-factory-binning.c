/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <sixel.h>

#include "filter-binning.h"
#include "filter-factory-binning.h"
#include "filter.h"

SIXELSTATUS
sixel_filter_factory_create_binning(
    sixel_filter_binning_config_t const *config,
    sixel_filter_t **filter_out)
{
    SIXELSTATUS status;
    sixel_filter_t *filter;

    status = SIXEL_FALSE;
    filter = NULL;
    if (config == NULL || filter_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *filter_out = NULL;

    status = sixel_filter_alloc(&filter);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_filter_binning_init(filter, config);
    if (SIXEL_FAILED(status)) {
        sixel_filter_free(filter);
        return status;
    }
    *filter_out = filter;
    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
