/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_PALETTE_VULKAN_H
#define LIBSIXEL_GPU_PALETTE_VULKAN_H
#if defined(HAVE_VULKAN)
#include "gpu-palette.h"
#include "gpu-runtime-vulkan.h"

/* Probes the runtime and builds the apply pipeline on first use. */
SIXEL_INTERNAL_API int sixel_gpu_palette_vulkan_is_available(void);
/* Exact policies whose indexes the direct scan reproduces. */
SIXEL_INTERNAL_API int sixel_gpu_palette_vulkan_lut_is_exact(int lut_policy);
/* Lookup policies with a Vulkan program; others stay on the CPU. */
SIXEL_INTERNAL_API int sixel_gpu_palette_vulkan_supports_lut(int lut_policy);
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_palette_vulkan_apply(
    sixel_gpu_palette_request_t const *request);
/* The shared device FHEDT table for callers that record their own
 * submission. All three run under sixel_vk_lock(). acquire reports whether
 * the table must be rebuilt; the caller then records the build after its
 * palette upload and publishes once the submission completed. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_palette_vulkan_fhedt_acquire_locked(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t const *palette_buffer,
    unsigned char const *palette, int ncolors, int resolution, int refine,
    sixel_vk_buffer_t const **table, int *rebuild);
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_palette_vulkan_fhedt_record_locked(
    sixel_vk_runtime_t *runtime, VkCommandBuffer command, int resolution,
    int refine, int ncolors);
SIXEL_INTERNAL_API void sixel_gpu_palette_vulkan_fhedt_publish_locked(
    unsigned char const *palette, int ncolors, int resolution, int refine);
/* The same contract for the shared 5bit/6bit cell-center byte table. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_palette_vulkan_bitlut_acquire_locked(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t const *palette_buffer,
    unsigned char const *palette, int ncolors, int bits,
    sixel_vk_buffer_t const **table, int *rebuild);
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_palette_vulkan_bitlut_record_locked(
    sixel_vk_runtime_t *runtime, VkCommandBuffer command, int bits,
    int ncolors);
SIXEL_INTERNAL_API void sixel_gpu_palette_vulkan_bitlut_publish_locked(
    unsigned char const *palette, int ncolors, int bits);
#endif
#endif
