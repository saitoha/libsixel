/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_DITHER_POLICY_GPU_H
#define LIBSIXEL_DITHER_POLICY_GPU_H
#include "gpu-program.h"
SIXEL_INTERNAL_API sixel_gpu_dither_program_t const *
sixel_gpu_dither_byte_program(int method);
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_dither_byte_create(
    sixel_allocator_t *, int, sixel_gpu_dither_preparer_interface_t **);
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_dither_compat_prepare(
    sixel_allocator_t *, sixel_dither_policy_prepare_request_t const *,
    sixel_gpu_dither_prepare_request_t const *,
    sixel_gpu_dither_plan_interface_t **);
#endif
