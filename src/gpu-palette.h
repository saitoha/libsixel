/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LIBSIXEL_GPU_PALETTE_H
#define LIBSIXEL_GPU_PALETTE_H

#include <stddef.h>

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIXEL_GPU_PALETTE_AUTO_THRESHOLD_DEFAULT 262144U

/*
 * PaletteApply GPU request.
 *
 * The GPU path is intentionally below lookup-policy and dither-policy.  It sees
 * the same normalized RGB888 source, byte palette, transparent mask, and
 * blue-noise controls that the CPU lookup would read, then materializes the
 * indexed SIXEL buffer in one dispatch.  NONE uses the exact direct scan;
 * EYTZINGER uses the one-dimensional projected lookup.  Unsupported shapes
 * return SIXEL_FALSE for policy=auto so the caller can continue through the
 * CPU path.
 */
typedef struct sixel_gpu_palette_request {
    int policy;
    size_t auto_threshold;
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
    /*
     * sixdelta_enabled says the caller wants 6delta keeps for this frame;
     * has_6delta_accumulation says this request can actually produce them.
     * The two differ when the retained plane is larger than the frame, because
     * the GPU kernel addresses the plane by frame pixel index.
     */
    int sixdelta_enabled;
    int has_6delta_accumulation;
    unsigned char const *accumulation_pixels;
    size_t accumulation_pixels_size;
    unsigned char const *accumulation_valid_mask;
    size_t accumulation_valid_mask_size;
    int accumulation_keycolor;
    unsigned int sixdelta_threshold;
    unsigned char *accumulation_result_mask;
    size_t accumulation_result_mask_size;
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

/*
 * Resolve the AUTO cutoff independently of backend availability.  Keeping
 * this seam observable lets registry migrations preserve the historical
 * strtoul() contract before GPU dispatch policy is changed.
 */
SIXEL_INTERNAL_API size_t
sixel_gpu_palette_auto_threshold(void);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_gpu_palette_apply(sixel_gpu_palette_request_t const *request);

/*
 * Return whether the GPU policy owns the palette-apply stage before CPU dither
 * workers are scheduled.  FORCE claims the stage even when the final request
 * shape may fail, because FORCE reports that error instead of falling back to
 * the CPU path.
 */
SIXEL_INTERNAL_API int
sixel_gpu_palette_policy_claims_apply_stage(int gpu_policy,
                                            size_t auto_threshold,
                                            int lut_policy,
                                            int method_for_diffuse,
                                            int method_for_scan,
                                            size_t pixel_count);

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
