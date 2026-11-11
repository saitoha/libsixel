/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_RUNTIME_VULKAN_H
#define LIBSIXEL_GPU_RUNTIME_VULKAN_H
#if defined(HAVE_VULKAN)
#include <stddef.h>
#include <vulkan/vulkan.h>
#include <sixel.h>

/*
 * Process-wide Vulkan compute runtime. Initialization is lazy: nothing
 * touches the loader until a GPU stage asks for the runtime, because
 * instance creation alone scans every installed ICD.
 *
 * Every function below except sixel_vk_lock() requires the caller to hold
 * the runtime lock; one queue and one command pool are shared serially.
 *
 * Environment:
 *   SIXEL_VULKAN_DEVICE  physical-device index or name substring
 *                        (default: discrete > integrated > virtual > cpu)
 *   SIXEL_VULKAN_SHADER  "prebuilt" or "runtime" to force one SPIR-V source
 *   SIXEL_VULKAN_DEBUG   nonzero prints device/shader/timing notes to stderr
 */
typedef struct sixel_vk_runtime {
    VkInstance instance;
    VkPhysicalDevice physical;
    VkDevice device;
    VkQueue queue;
    uint32_t queue_family;
    VkCommandPool command_pool;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory;
    /* Two timestamps bracket every submission for sixel_gpu_get_statistics;
     * without timestamp support the submit/wait wall time is recorded. */
    VkQueryPool timestamps;
    uint64_t timestamp_mask;
    double timestamp_period;
    int debug;
} sixel_vk_runtime_t;

/* Storage buffer that grows on demand. mapped is always host-visible
 * coherent memory. When that memory is not device-local (discrete GPUs),
 * buffer is separate VRAM and staging holds the mapped copy; record_upload
 * and record_download move the bytes inside the command buffer. Otherwise
 * staging is VK_NULL_HANDLE, buffer is the mapped memory, and both record
 * calls do nothing. */
typedef struct sixel_vk_buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkBuffer staging;
    VkDeviceMemory staging_memory;
    void *mapped;
    VkDeviceSize capacity;
} sixel_vk_buffer_t;

/* One compute shader in both embedded forms. Either may be absent:
 * spirv is the build-time glslc output, glsl_chunks the NULL-terminated
 * source compiled with libshaderc when the prebuilt form is missing. */
typedef struct sixel_vk_shader_source {
    char const *name;
    unsigned char const *spirv;
    size_t spirv_size;
    char const * const *glsl_chunks;
} sixel_vk_shader_source_t;

SIXEL_INTERNAL_API void sixel_vk_lock(void);
SIXEL_INTERNAL_API void sixel_vk_unlock(void);

/* Returns NULL when no usable device exists; the probe runs only once. */
SIXEL_INTERNAL_API sixel_vk_runtime_t *sixel_vk_runtime_get_locked(void);

SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_buffer_reserve(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t *buffer,
    VkDeviceSize size);
SIXEL_INTERNAL_API void sixel_vk_buffer_release(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t *buffer);

/* Copy the first size bytes between staging and device storage. Callers
 * separate the copies from shader access with sixel_vk_record_barrier(). */
SIXEL_INTERNAL_API void sixel_vk_buffer_record_upload(
    VkCommandBuffer command, sixel_vk_buffer_t const *buffer,
    VkDeviceSize size);
SIXEL_INTERNAL_API void sixel_vk_buffer_record_download(
    VkCommandBuffer command, sixel_vk_buffer_t const *buffer,
    VkDeviceSize size);
SIXEL_INTERNAL_API void sixel_vk_record_barrier(VkCommandBuffer command,
    VkPipelineStageFlags src_stage, VkAccessFlags src_access,
    VkPipelineStageFlags dst_stage, VkAccessFlags dst_access);

SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_shader_module_create(
    sixel_vk_runtime_t *runtime, sixel_vk_shader_source_t const *source,
    VkShaderModule *module);

/* command_begin allocates a one-shot primary command buffer and opens the
 * statistics bracket; the caller records work; submit_and_wait closes the
 * bracket, submits, waits and adds the GPU time to the statistics. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_submit_and_wait(
    sixel_vk_runtime_t *runtime, VkCommandBuffer command);
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_command_begin(
    sixel_vk_runtime_t *runtime, VkCommandBuffer *command);
SIXEL_INTERNAL_API void sixel_vk_command_free(
    sixel_vk_runtime_t *runtime, VkCommandBuffer command);

SIXEL_INTERNAL_API double sixel_vk_now_ms(void);
#endif
#endif
