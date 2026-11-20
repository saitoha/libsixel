/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PALETTE_HECKBERT_GPU_H
#define LIBSIXEL_PALETTE_HECKBERT_GPU_H
#include <sixel.h>
#include <6cells.h>

/* @classid quant/heckbert.gpu */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_heckbert_gpu_new(sixel_allocator_t *allocator, void **object);

/* The native solver owns its device, program and buffers. FEATURE_ERROR
 * means no device or an unsupported request, before any output. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_heckbert_gpu_metal_solve(
    sixel_heckbert_solve_request_t const *request);
#endif
