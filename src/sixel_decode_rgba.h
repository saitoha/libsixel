/*
 * SPDX-License-Identifier: MIT
 *
 * Shared SIXEL memory decoding helper for RGB/RGBA output.
 */

#ifndef SIXEL_DECODE_RGBA_H
#define SIXEL_DECODE_RGBA_H

#include <stddef.h>
#include <stdint.h>

#include <sixel.h>

SIXELAPI SIXELSTATUS
sixel_decode_rgba(unsigned char const *data,
                  size_t size,
                  int request_alpha,
                  unsigned char const *bgcolor,
                  unsigned char **out_pixels,
                  int *out_width,
                  int *out_height,
                  int *out_channels,
                  sixel_allocator_t *allocator);

#endif /* SIXEL_DECODE_RGBA_H */
