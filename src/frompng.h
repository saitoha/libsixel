/*
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBSIXEL_FROMPNG_H
#define LIBSIXEL_FROMPNG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include <sixel.h>

#include "chunk.h"
#include "frame.h"

void
sixel_frompng_convert_icc_to_srgb(unsigned char *pixels,
                                  int width,
                                  int height,
                                  unsigned char const *profile,
                                  size_t profile_length);

void
sixel_frompng_apply_colorspace_fallback(unsigned char *pixels,
                                        int width,
                                        int height,
                                        unsigned char const *buffer,
                                        size_t size,
                                        sixel_allocator_t *allocator);

SIXELSTATUS
sixel_frompng_load_nonindexed(sixel_chunk_t const *pchunk,
                              sixel_frame_t *frame,
                              int enable_cms);

#ifdef __cplusplus
}
#endif

#endif  /* LIBSIXEL_FROMPNG_H */
