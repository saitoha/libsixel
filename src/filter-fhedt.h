/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FILTER_FHEDT_H
#define LIBSIXEL_FILTER_FHEDT_H

#include <sixel.h>

#include "filter-lookup.h"
#include "filter.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Configuration for the FHEDT filter. The filter builds a FHEDT lookup table from
 * the merged palette and propagates the result to the caller. The lookup
 * configuration must request `SIXEL_LUT_POLICY_FHEDT`.
 */
typedef struct sixel_filter_fhedt_config {
    sixel_filter_lookup_config_t lookup_config;
    sixel_filter_lookup_result_t *result_out;
} sixel_filter_fhedt_config_t;

SIXELSTATUS
sixel_filter_fhedt_init(sixel_filter_t *filter,
                       const sixel_filter_fhedt_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_FILTER_FHEDT_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
