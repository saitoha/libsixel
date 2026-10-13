/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */
#ifndef LIBSIXEL_COLOR_OPTION_H
#define LIBSIXEL_COLOR_OPTION_H

#include "cms.h"
#include "options.h"

/* Parse converter syntax and load an owned ICC endpoint transactionally.
 * Common CMS receives bytes only; file I/O and option grammar stay here. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_color_option_parse(char const *text, int decoder, int *base,
                         sixel_color_def_t **definition);

#endif
