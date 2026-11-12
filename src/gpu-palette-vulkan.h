/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_PALETTE_VULKAN_H
#define LIBSIXEL_GPU_PALETTE_VULKAN_H
#if defined(HAVE_VULKAN)
#include "gpu-palette.h"
#include "gpu-runtime-vulkan.h"

/*
 * Vulkan program runtime of the "vulkan.lookup-rgb8.v1" ABI and the
 * palette-apply consumer. sixel_gpu_device_acquire() returns its device;
 * sixel_gpu_program_prepare() composes a lookup plan's program view with
 * the fused apply kernel. The helpers below let other consumers of the ABI
 * (the GPU dither components) resolve workspaces and run a plan's builders
 * without knowing any lookup policy.
 */

#define SIXEL_GPU_VULKAN_MAX_BUILD_PASSES 16

/* One compiled pipeline per builder pass of a program view. */
typedef struct sixel_gpu_vulkan_builders {
    size_t count;
    VkPipeline pipelines[SIXEL_GPU_VULKAN_MAX_BUILD_PASSES];
} sixel_gpu_vulkan_builders_t;

/* Probes the runtime and prepares the apply state on first use. */
SIXEL_INTERNAL_API int sixel_gpu_palette_vulkan_is_available(void);
/* Runs request->program, a program prepared on the Vulkan device. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_palette_vulkan_apply(
    sixel_gpu_palette_request_t const *request);

/* Nonzero for the Vulkan runtime device. */
SIXEL_INTERNAL_API int sixel_gpu_vulkan_is_device(
    sixel_gpu_device_interface_t const *device);
/* The native storage of a buffer the Vulkan device created, or NULL. */
SIXEL_INTERNAL_API sixel_vk_buffer_t const *sixel_gpu_vulkan_buffer(
    sixel_gpu_device_interface_t *device,
    sixel_gpu_buffer_interface_t *buffer);
/* Verify a view against the ABI and the consumer's query kind. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_vulkan_view_check(
    sixel_gpu_lookup_program_view_t const *view,
    sixel_gpu_lookup_query_kind_t query_kind);

/* The builder pipeline layout: storage buffers 0 palette, 1 color count,
 * 2 constants and 3 workspace in set 0. The caller holds sixel_vk_lock()
 * for these four. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_vulkan_builder_layout_create(
    sixel_vk_runtime_t *runtime, VkDescriptorSetLayout *set_layout,
    VkPipelineLayout *layout);
SIXEL_INTERNAL_API SIXELSTATUS sixel_gpu_vulkan_builders_create(
    sixel_vk_runtime_t *runtime, VkPipelineLayout layout,
    sixel_gpu_lookup_program_view_t const *view,
    sixel_gpu_vulkan_builders_t *builders);
SIXEL_INTERNAL_API void sixel_gpu_vulkan_builders_destroy(
    sixel_vk_runtime_t *runtime, sixel_gpu_vulkan_builders_t *builders);
SIXEL_INTERNAL_API void sixel_gpu_vulkan_builders_record(
    VkCommandBuffer command, VkPipelineLayout layout, VkDescriptorSet set,
    sixel_gpu_vulkan_builders_t const *builders,
    sixel_gpu_lookup_program_view_t const *view);
#endif
#endif
