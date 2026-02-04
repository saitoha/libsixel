/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_LOOKUP_VPTREE_8BIT_H
#define LIBSIXEL_LOOKUP_VPTREE_8BIT_H

#include "allocator.h"

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sixel_lookup_vptree_8bit sixel_lookup_vptree_8bit_t;

SIXELSTATUS
sixel_lookup_vptree_8bit_create(sixel_allocator_t *allocator,
                                sixel_lookup_vptree_8bit_t **tree_out);

void
sixel_lookup_vptree_8bit_unref(sixel_lookup_vptree_8bit_t *tree);

SIXELSTATUS
sixel_lookup_vptree_8bit_configure(sixel_lookup_vptree_8bit_t *tree,
                                   unsigned char const *palette,
                                   int ncolors,
                                   int depth,
                                   int complexion);

int
sixel_lookup_vptree_8bit_map(sixel_lookup_vptree_8bit_t *tree,
                             unsigned char const *pixel);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_LOOKUP_VPTREE_8BIT_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
