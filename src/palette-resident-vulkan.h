/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_PALETTE_RESIDENT_VULKAN_H
#define LIBSIXEL_PALETTE_RESIDENT_VULKAN_H
#include "gpu-runtime-vulkan.h"

#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
SIXEL_INTERNAL_API uint32_t
sixel_vk_heckbert_threads(sixel_vk_runtime_t *runtime);
#endif
#endif
