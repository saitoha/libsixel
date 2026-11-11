/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_STATISTICS_H
#define LIBSIXEL_GPU_STATISTICS_H
#include <sixel.h>

/* Backends publish their identity once a device is usable, then record each
 * completed submission. Both calls are thread-safe and never block on GPU
 * work. The public reader is sixel_gpu_get_statistics(). Metal command
 * buffers go through sixel_gpu_statistics_record_metal() in
 * gpu-runtime-metal.h. */
SIXEL_INTERNAL_API void sixel_gpu_statistics_set_device(char const *backend,
    char const *device);
SIXEL_INTERNAL_API void sixel_gpu_statistics_record(
    unsigned long long busy_nanoseconds);

#endif
