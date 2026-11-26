/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PALETTE_RESIDENT_VULKAN_H
#define LIBSIXEL_PALETTE_RESIDENT_VULKAN_H
#include "gpu-runtime-vulkan.h"

#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
SIXEL_INTERNAL_API uint32_t
sixel_vk_heckbert_threads(sixel_vk_runtime_t *runtime);

/* The resident-palette submission owns these descriptors and scratch
 * buffers, including the period when it releases the runtime lock. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_vk_body_record_resident(sixel_vk_runtime_t *runtime,
    VkCommandBuffer command, sixel_vk_buffer_t const *indices,
    sixel_vk_buffer_t const *palette, uint32_t width, uint32_t height,
    int limited, VkQueryPool timestamps);
SIXEL_INTERNAL_API SIXELSTATUS
sixel_vk_body_read_resident(sixel_vk_runtime_t *runtime, unsigned char **bytes,
    int *size);
#endif
#endif
