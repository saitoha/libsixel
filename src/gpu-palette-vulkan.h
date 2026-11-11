/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_PALETTE_VULKAN_H
#define LIBSIXEL_GPU_PALETTE_VULKAN_H
#if defined(HAVE_VULKAN)
#include "gpu-palette.h"

/* Probes the runtime and builds the apply pipeline on first use. */
SIXEL_INTERNAL_API int sixel_gpu_palette_vulkan_is_available(void);
/* Lookup policies with a Vulkan program; others stay on the CPU. */
SIXEL_INTERNAL_API int sixel_gpu_palette_vulkan_supports_lut(int lut_policy);
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_palette_vulkan_apply(
    sixel_gpu_palette_request_t const *request);
#endif
#endif
