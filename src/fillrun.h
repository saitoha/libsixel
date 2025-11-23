/*
 * SPDX-License-Identifier: MIT
 *
 * Helpers to store long RGBA runs efficiently. Long spans may opt into
 * non-temporal stores when the pointer is suitably aligned; shorter spans
 * fall back to scalar copies.
 */
#ifndef SIXEL_FILLRUN_H
#define SIXEL_FILLRUN_H

#include <stdint.h>

void sixel_fillrun_store_rgba(unsigned char *dst,
                              int repeat,
                              uint32_t rgba,
                              int use_non_temporal);

#endif /* SIXEL_FILLRUN_H */
