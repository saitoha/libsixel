/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_PROGRAM_H
#define LIBSIXEL_GPU_PROGRAM_H
#include <sixel.h>
#include <6cells.h>

typedef struct sixel_gpu_program sixel_gpu_program_t;
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_device_acquire(
    sixel_gpu_device_interface_t **);
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_program_prepare(
    sixel_gpu_device_interface_t *, sixel_gpu_lookup_plan_interface_t *,
    sixel_gpu_program_t **);
SIXEL_INTERNAL_API void sixel_gpu_program_destroy(sixel_gpu_program_t *);
#endif
