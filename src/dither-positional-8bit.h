/*
 * Positional/ordered dithering helpers for 8bit pixel buffers.
 */
#ifndef LIBSIXEL_DITHER_POSITIONAL_8BIT_H
#define LIBSIXEL_DITHER_POSITIONAL_8BIT_H

#include "dither-internal.h"

SIXELSTATUS
sixel_dither_apply_positional_8bit(sixel_dither_t *dither,
                                   sixel_dither_context_t *context);

#endif /* LIBSIXEL_DITHER_POSITIONAL_8BIT_H */
