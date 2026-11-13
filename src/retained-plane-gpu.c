/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "retained-plane-gpu.h"

#if !defined(HAVE_METAL) && !defined(HAVE_VULKAN)
SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_new(sixel_gpu_device_interface_t *device,
    sixel_retained_plane_description_t const *description,
    unsigned char const *rgb, unsigned char const *valid,
    sixel_retained_gpu_t **result)
{
    (void)device;
    (void)description;
    (void)rgb;
    (void)valid;
    *result = NULL;
    return SIXEL_FEATURE_ERROR;
}

SIXEL_INTERNAL_API void
sixel_retained_gpu_free(sixel_retained_gpu_t *gpu)
{
    (void)gpu;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_read(sixel_retained_gpu_t *gpu,
    sixel_gpu_device_interface_t *device, sixel_retained_gpu_read_view_t *view)
{
    (void)gpu;
    (void)device;
    (void)view;
    return SIXEL_FEATURE_ERROR;
}

SIXEL_INTERNAL_API void
sixel_retained_gpu_map(sixel_retained_gpu_t *gpu,
    unsigned char **rgb, unsigned char **valid)
{
    (void)gpu;
    *rgb = *valid = NULL;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_prepare(sixel_retained_gpu_t *gpu,
    sixel_indexed_frame_description_t const *image,
    sixel_wire_palette_view_t const *palette, int record_paint)
{
    (void)gpu;
    (void)image;
    (void)palette;
    (void)record_paint;
    return SIXEL_FEATURE_ERROR;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_stage(sixel_retained_gpu_t *gpu,
    unsigned char const *indexes, unsigned char const *skip,
    unsigned char *paint, sixel_retained_update_stats_t *stats, int fault)
{
    (void)gpu;
    (void)indexes;
    (void)skip;
    (void)paint;
    (void)stats;
    (void)fault;
    return SIXEL_FEATURE_ERROR;
}

SIXEL_INTERNAL_API void
sixel_retained_gpu_cpu_write(sixel_retained_gpu_t *gpu)
{
    (void)gpu;
}

SIXEL_INTERNAL_API void
sixel_retained_gpu_commit(sixel_retained_gpu_t *gpu)
{
    (void)gpu;
}
#endif
