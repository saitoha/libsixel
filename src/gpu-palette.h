/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#ifndef LIBSIXEL_GPU_PALETTE_H
#define LIBSIXEL_GPU_PALETTE_H

#include <stddef.h>

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PaletteApply GPU request.
 *
 * The GPU path is intentionally below lookup-policy and dither-policy.  It sees
 * the same normalized RGB888 source, byte palette, transparent mask, and
 * blue-noise controls that the exact serial CPU lookup would read, then
 * materializes the indexed SIXEL buffer in one dispatch.  Unsupported shapes
 * return
 * SIXEL_FALSE for policy=auto so the caller can continue through the CPU path.
 */
typedef struct sixel_gpu_palette_request {
    int policy;
    sixel_index_t *dest;
    unsigned char const *pixels;
    size_t pixel_count;
    int width;
    int height;
    int pixelformat;
    unsigned char const *palette;
    size_t palette_size;
    int palette_depth;
    int ncolors;
    int lut_policy;
    int method_for_diffuse;
    int method_for_scan;
    int has_parallel_bands;
    unsigned char const *transparent_mask;
    size_t transparent_mask_size;
    int transparent_keycolor;
    int has_6delta_accumulation;
    int bluenoise_strength_override;
    float bluenoise_strength;
    int bluenoise_phase_override;
    int bluenoise_phase_x;
    int bluenoise_phase_y;
    int bluenoise_seed_override;
    int bluenoise_seed;
    int bluenoise_channel_override;
    int bluenoise_channel_rgb;
    int bluenoise_size_override;
    int bluenoise_size;
    int bluenoise_gradient_factor_override;
    float bluenoise_gradient_factor;
    unsigned char const *bluenoise_gradient_map;
    size_t bluenoise_gradient_map_size;
    int bluenoise_gradient_width;
    int bluenoise_gradient_height;
} sixel_gpu_palette_request_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_gpu_palette_apply(sixel_gpu_palette_request_t const *request);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_GPU_PALETTE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
