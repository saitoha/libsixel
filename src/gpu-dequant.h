/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_GPU_DEQUANT_H
#define LIBSIXEL_GPU_DEQUANT_H

#include <stddef.h>

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * GPU fast4 dequantization request.
 *
 * The decoder-side accelerator starts after the SIXEL parser has already
 * produced direct RGBA.  Alpha carries the painted/unpainted distinction, so
 * the shader does not need the indexed paint mask.  The palette remains part
 * of the request because k_undither decides whether a neighbour is useful by
 * comparing the midpoint against the other palette colors.
 */
typedef struct sixel_gpu_dequant_request {
    int policy;
    unsigned char *dest;
    unsigned char const *rgba;
    size_t pixel_count;
    int width;
    int height;
    int pixelformat;
    unsigned char const *palette;
    size_t palette_size;
    int palette_depth;
    int ncolors;
    int similarity_bias;
} sixel_gpu_dequant_request_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_gpu_dequant_fast4_rgba(sixel_gpu_dequant_request_t const *request);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_GPU_DEQUANT_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
