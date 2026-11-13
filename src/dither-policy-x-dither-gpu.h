/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_DITHER_POLICY_X_DITHER_GPU_H
#define LIBSIXEL_DITHER_POLICY_X_DITHER_GPU_H
#include "gpu-program.h"

/* @classid dither/x_dither.gpu */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_policy_x_dither_gpu_new(sixel_allocator_t *, void **);

SIXEL_INTERNAL_API extern sixel_gpu_dither_program_t const
    sixel_dither_x_dither_gpu_program;
#endif
