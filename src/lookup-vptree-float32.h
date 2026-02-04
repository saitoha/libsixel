/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_LOOKUP_VPTREE_FLOAT32_H
#define LIBSIXEL_LOOKUP_VPTREE_FLOAT32_H

#include "allocator.h"

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sixel_lookup_vptree_float32 sixel_lookup_vptree_float32_t;

SIXELSTATUS
sixel_lookup_vptree_float32_create(sixel_allocator_t *allocator,
                                   sixel_lookup_vptree_float32_t **tree_out);

void
sixel_lookup_vptree_float32_unref(sixel_lookup_vptree_float32_t *tree);

SIXELSTATUS
sixel_lookup_vptree_float32_configure(sixel_lookup_vptree_float32_t *tree,
                                      float const *palette,
                                      int ncolors,
                                      int depth,
                                      float const *weights);

int
sixel_lookup_vptree_float32_map(sixel_lookup_vptree_float32_t *tree,
                                float const *pixel);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_LOOKUP_VPTREE_FLOAT32_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
