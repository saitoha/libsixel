/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_LOADER_ORDER_SCHEMA_H
#define LIBSIXEL_LOADER_ORDER_SCHEMA_H

#include <sixel.h>

#include "options.h"

sixel_option_argument_schema_t const *
sixel_loader_order_schema_get(void);

SIXELSTATUS
sixel_loader_order_validate_resolution(
    sixel_option_argument_list_resolution_t const *resolution);

SIXELSTATUS
sixel_loader_order_parse_and_validate(
    char const *argument,
    sixel_option_argument_list_resolution_t *resolution,
    char *diagnostic,
    size_t diagnostic_size);

#endif /* LIBSIXEL_LOADER_ORDER_SCHEMA_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
