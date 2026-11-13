/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_DITHER_POLICY_NONE_GPU_H
#define LIBSIXEL_DITHER_POLICY_NONE_GPU_H
#include "gpu-program.h"

/* @classid dither/none.gpu */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_policy_none_gpu_new(sixel_allocator_t *, void **);

SIXEL_INTERNAL_API extern sixel_gpu_dither_program_t const
    sixel_dither_none_gpu_program;
#endif
