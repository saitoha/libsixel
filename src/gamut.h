/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */
#ifndef LIBSIXEL_GAMUT_H
#define LIBSIXEL_GAMUT_H

#include "cms.h"

enum {
    SIXEL_GAMUT_AUTO = 0,
    SIXEL_GAMUT_CLIP,
    SIXEL_GAMUT_LOCAL_MINDE
};

typedef struct sixel_gamut sixel_gamut_t;

/* An immutable, encoder-owned policy. Workers borrow it read-only; apply
 * never rounds to the wire grid or limits diffusion compensation. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_gamut_new(sixel_gamut_t **out, sixel_color_def_t const *destination,
                 int legacy_colorspace, sixel_allocator_t *allocator);

SIXEL_INTERNAL_API int
sixel_gamut_apply(sixel_gamut_t const *mapper, double const input[3],
                   double output[3], int colorspace);

SIXEL_INTERNAL_API void
sixel_gamut_free(sixel_gamut_t *mapper);

#endif
