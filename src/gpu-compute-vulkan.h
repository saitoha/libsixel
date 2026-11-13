/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_COMPUTE_VULKAN_H
#define LIBSIXEL_GPU_COMPUTE_VULKAN_H
#include "gpu-runtime-vulkan.h"
#if defined(HAVE_VULKAN)

/* Small standalone compute stages share only Vulkan resource plumbing.
 * Each caller owns its algorithms, buffers and publication rules. All
 * operations require the runtime lock, including destruction. */
#define SIXEL_VK_COMPUTE_BINDINGS 16
typedef struct sixel_vk_compute {
    VkDescriptorSetLayout set_layout;
    VkPipelineLayout layout;
    VkDescriptorPool pool;
    VkDescriptorSet set;
    VkPipeline pipeline;
    unsigned int bindings;
} sixel_vk_compute_t;

SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_compute_create(
    sixel_vk_runtime_t *runtime, sixel_vk_compute_t *stage,
    char const *const *chunks, size_t count, char const *entry,
    unsigned int bindings);
SIXEL_INTERNAL_API void sixel_vk_compute_destroy(
    sixel_vk_runtime_t *runtime, sixel_vk_compute_t *stage);
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_compute_bind(
    sixel_vk_runtime_t *runtime, sixel_vk_compute_t *stage,
    sixel_vk_buffer_t const *const *buffers);
SIXEL_INTERNAL_API void sixel_vk_compute_dispatch(
    VkCommandBuffer command, sixel_vk_compute_t const *stage,
    uint32_t groups);
/* Order uploads/earlier dispatches before the next compute operation, or
 * compute writes before readback. Host publication follows fence wait. */
SIXEL_INTERNAL_API void sixel_vk_compute_barrier(VkCommandBuffer command);
SIXEL_INTERNAL_API void sixel_vk_compute_download_barrier(
    VkCommandBuffer command);
#endif
#endif
