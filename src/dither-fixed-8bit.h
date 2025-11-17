/*
 * Fixed diffusion dithering helpers for byte-based pipelines.
 */
#ifndef LIBSIXEL_DITHER_FIXED_8BIT_H
#define LIBSIXEL_DITHER_FIXED_8BIT_H

#include "dither-internal.h"

SIXELSTATUS
sixel_dither_apply_fixed_8bit(sixel_dither_t *dither,
                              sixel_dither_context_t *context);

#endif /* LIBSIXEL_DITHER_FIXED_8BIT_H */
