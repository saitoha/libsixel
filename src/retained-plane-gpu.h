/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_RETAINED_PLANE_GPU_H
#define LIBSIXEL_RETAINED_PLANE_GPU_H
#include <6cells.h>

typedef struct sixel_retained_gpu sixel_retained_gpu_t;

/* Private RetainedPlane execution backend. It owns two canvas allocations,
 * the emitted-color expansion kernel and its staging resources. The runtime
 * alone translates gpu_buffer capabilities into native device objects. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_new(sixel_gpu_device_interface_t *device,
    sixel_retained_plane_description_t const *description,
    unsigned char const *rgb, unsigned char const *valid,
    sixel_retained_gpu_t **result);
SIXEL_INTERNAL_API void
sixel_retained_gpu_free(sixel_retained_gpu_t *gpu);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_read(sixel_retained_gpu_t *gpu,
    sixel_gpu_device_interface_t *device, sixel_retained_gpu_read_view_t *view);
SIXEL_INTERNAL_API void
sixel_retained_gpu_map(sixel_retained_gpu_t *gpu,
    unsigned char **rgb, unsigned char **valid);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_prepare(sixel_retained_gpu_t *gpu,
    sixel_indexed_frame_description_t const *image,
    sixel_wire_palette_view_t const *palette, int record_paint);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_stage(sixel_retained_gpu_t *gpu,
    unsigned char const *indexes, unsigned char const *skip,
    unsigned char *paint, sixel_retained_update_stats_t *stats, int fault);
SIXEL_INTERNAL_API void
sixel_retained_gpu_cpu_write(sixel_retained_gpu_t *gpu);
SIXEL_INTERNAL_API void
sixel_retained_gpu_commit(sixel_retained_gpu_t *gpu);
#endif
