/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_YCBCR_H
#define LIBSIXEL_GPU_YCBCR_H

#include <stdint.h>
#include <sixel.h>

/* std430 in GLSL and scalar struct layout in Metal. The host derives taps
 * once, using the CPU's reconstruction, phase, and edge normalization. */
typedef struct sixel_ycbcr_gpu_taps {
    int32_t first, count;
    float weights[17];
} sixel_ycbcr_gpu_taps_t;

typedef struct sixel_ycbcr_gpu_params {
    uint32_t sw, cw, width, height;
    uint32_t cb_offset, cr_offset, chroma_step, bt1886;
    float offset, scale, matrix[4];
} sixel_ycbcr_gpu_params_t;

typedef struct sixel_ycbcr_gpu_plan {
    size_t luma_bytes, chroma_bytes;
    size_t input_bytes, output_bytes, taps_bytes;
} sixel_ycbcr_gpu_plan_t;

typedef struct sixel_ycbcr_gpu_ops {
    SIXELSTATUS (*create)(void **, sixel_ycbcr_gpu_plan_t const *,
        sixel_ycbcr_gpu_taps_t const *, sixel_allocator_t *);
    void (*destroy)(void *, sixel_allocator_t *);
    SIXELSTATUS (*run)(void *, sixel_ycbcr_gpu_plan_t const *,
        sixel_ycbcr_gpu_params_t const *, sixel_ycbcr_frame_view_t const *,
        unsigned char *, size_t);
} sixel_ycbcr_gpu_ops_t;

#ifdef HAVE_METAL
extern sixel_ycbcr_gpu_ops_t const sixel_ycbcr_gpu_metal;
#endif
#ifdef HAVE_VULKAN
extern sixel_ycbcr_gpu_ops_t const sixel_ycbcr_gpu_vulkan;
#endif

void
sixel_ycbcr_gpu_upload(unsigned char *dst, sixel_ycbcr_gpu_params_t const *p,
                      sixel_ycbcr_frame_view_t const *source);

void
sixel_ycbcr_gpu_readback(unsigned char *dst, size_t stride,
    unsigned char const *src, sixel_ycbcr_gpu_params_t const *p);

void
sixel_scale_ycbcr_gpu_axis(sixel_ycbcr_gpu_taps_t *out, int src, int dst,
                          int phase, int nearest);

#endif
