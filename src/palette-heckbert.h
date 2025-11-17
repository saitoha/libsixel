/*
 * SPDX-License-Identifier: MIT
 *
 * Public entry point for the Heckbert median-cut palette builder.  The header
 * isolates the quantizer-specific API so palette.c can include only the pieces
 * it needs without leaking implementation details to unrelated modules.
 */

#ifndef LIBSIXEL_PALETTE_HECKBERT_H
#define LIBSIXEL_PALETTE_HECKBERT_H

#include "palette.h"

#ifdef __cplusplus
extern "C" {
#endif

SIXELSTATUS
sixel_palette_build_heckbert(sixel_palette_t *palette,
                             unsigned char const *data,
                             unsigned int length,
                             int pixelformat,
                             sixel_allocator_t *allocator);

SIXELSTATUS
sixel_palette_build_heckbert_float32(sixel_palette_t *palette,
                                     unsigned char const *data,
                                     unsigned int length,
                                     int pixelformat,
                                     sixel_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_HECKBERT_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
