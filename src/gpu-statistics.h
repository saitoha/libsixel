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

/* Completed GPU time and elapsed wall time are sampled at frame boundaries.
 * The window and separate enter/leave thresholds avoid per-frame flapping. */
typedef struct sixel_gpu_headroom {
    unsigned long long last_busy_ns;
    unsigned long long last_wall_ns;
    unsigned int frames;
    double load;
    int windows;
    int gpu;
} sixel_gpu_headroom_t;
SIXEL_INTERNAL_API int
sixel_gpu_headroom_step(
    sixel_gpu_headroom_t *state, int gpu_policy,
    unsigned long long busy_ns, unsigned long long wall_ns);

#endif
