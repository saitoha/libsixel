/*
 * SPDX-License-Identifier: MIT
 *
 * Public entry points for the K-means palette builder and its tuning knobs.
 * Consumers include palette.c as well as tests that want to drive the
 * quantizer directly.  Keeping the declarations separate from palette.h keeps
 * the orchestrator lightweight and clarifies module ownership.
 */

#ifndef LIBSIXEL_PALETTE_KMEANS_H
#define LIBSIXEL_PALETTE_KMEANS_H

#include "palette.h"

#ifdef __cplusplus
extern "C" {
#endif

SIXELSTATUS
sixel_palette_build_kmeans(sixel_palette_t *palette,
                           unsigned char const *data,
                           unsigned int length,
                           int pixelformat,
                           sixel_allocator_t *allocator);

unsigned int
sixel_palette_kmeans_iter_max(void);

double
sixel_palette_kmeans_threshold(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_KMEANS_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
