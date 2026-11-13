/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_DITHER_POLICY_BLUENOISE_GPU_H
#define LIBSIXEL_DITHER_POLICY_BLUENOISE_GPU_H
#include "gpu-program.h"

/* @classid dither/bluenoise.gpu */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_policy_bluenoise_gpu_new(sixel_allocator_t *, void **);

SIXEL_INTERNAL_API extern sixel_gpu_dither_program_t const
    sixel_dither_bluenoise_gpu_program;
#endif
