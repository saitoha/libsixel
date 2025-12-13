/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_FILTER_FACTORY_H
#define LIBSIXEL_FILTER_FACTORY_H

#include <sixel.h>

#include "filter.h"

/*
 * Minimal abstract factory for filter creation. The planner can pick a filter
 * by name or kind, hand in a configuration structure, and receive a fully
 * initialized `sixel_filter_t` owned by the caller.
 */

SIXELSTATUS
sixel_filter_factory_create_by_name(const char *name,
                                    const void *config,
                                    sixel_filter_t **filter_out);

SIXELSTATUS
sixel_filter_factory_create_by_kind(sixel_filter_kind_t kind,
                                    const void *config,
                                    sixel_filter_t **filter_out);

#endif /* LIBSIXEL_FILTER_FACTORY_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
