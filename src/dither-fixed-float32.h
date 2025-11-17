/*
 * Fixed diffusion dithering helpers for float32 pipelines.
 */
#ifndef LIBSIXEL_DITHER_FIXED_FLOAT32_H
#define LIBSIXEL_DITHER_FIXED_FLOAT32_H

#include "dither-internal.h"

SIXELSTATUS
sixel_dither_apply_fixed_float32(sixel_dither_t *dither,
                                 sixel_dither_context_t *context);

#if HAVE_TESTS
void
sixel_dither_diffusion_tests_reset_float32_hits(void);

int
sixel_dither_diffusion_tests_float32_hits(void);
#endif

#endif /* LIBSIXEL_DITHER_FIXED_FLOAT32_H */
