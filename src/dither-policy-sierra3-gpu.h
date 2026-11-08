/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_DITHER_POLICY_SIERRA3_GPU_H
#define LIBSIXEL_DITHER_POLICY_SIERRA3_GPU_H
#include <sixel.h>
#include <6cells.h>

/* @classid dither/sierra3.gpu */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_policy_sierra3_gpu_new(sixel_allocator_t *allocator, void **object);

/* Native preparation owns shader compilation, private state and execution.
 * The portable preparer has no fallback implementation of the algorithm. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_sierra3_gpu_metal_prepare(sixel_allocator_t *,
    sixel_dither_policy_prepare_request_t const *,
    sixel_gpu_dither_prepare_request_t const *,
    sixel_gpu_dither_plan_interface_t **);
#endif
