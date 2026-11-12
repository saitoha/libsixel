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
/* Command buffers a pipelined submission may keep in flight at once. */
#define SIXEL_VK_RING 3

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
    /* Device time of the latest submission in milliseconds, or its
     * submit/wait wall time without timestamps; for debug breakdowns. */
    double last_busy_ms;
    /* Reused by every submission under sixel_vk_lock(); allocating and
     * destroying them per frame cost about 0.5 ms on a discrete GPU. */
    VkCommandBuffer command;
    VkFence fence;
    /* A ring for work that overlaps the host: each slot owns a command
     * buffer and a fence, and submitted_ms brackets its wall time. */
    VkCommandBuffer ring[SIXEL_VK_RING];
    VkFence ring_fences[SIXEL_VK_RING];
    double ring_submitted_ms[SIXEL_VK_RING];
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

SIXEL_INTERNAL_API void sixel_vk_lock(void);
SIXEL_INTERNAL_API void sixel_vk_unlock(void);

/* Returns NULL when no usable device exists; the probe runs only once. */
SIXEL_INTERNAL_API sixel_vk_runtime_t *sixel_vk_runtime_get_locked(void);

/* host_written marks a buffer the host only fills and the shader only
 * reads. It is placed in host-visible device memory when the device exposes
 * that (resizable BAR, unified memory), so the host writes once and no
 * staging copy runs. A buffer the host reads back keeps cached host memory
 * for its mapping. Each buffer must keep the same kind across reserves. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_buffer_reserve(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t *buffer,
    VkDeviceSize size, int host_written);
/* Device-local storage the host never maps (mapped stays NULL), for
 * intermediate tables that shaders both write and read. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_buffer_reserve_device(
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
/* Copy size bytes at offset from device storage to the same offset of
 * staging. */
SIXEL_INTERNAL_API void sixel_vk_buffer_record_download_range(
    VkCommandBuffer command, sixel_vk_buffer_t const *buffer,
    VkDeviceSize offset, VkDeviceSize size);
SIXEL_INTERNAL_API void sixel_vk_record_barrier(VkCommandBuffer command,
    VkPipelineStageFlags src_stage, VkAccessFlags src_access,
    VkPipelineStageFlags dst_stage, VkAccessFlags dst_access);

/* One run of embedded GLSL chunks; a composed program concatenates runs. */
typedef struct sixel_vk_source_part {
    char const * const *chunks;
    size_t count;
} sixel_vk_source_part_t;

/* Create a module from the exact concatenation of parts. Its SPIR-V comes
 * from the build-time program matrix when that text was built there, and
 * otherwise from libshaderc. The text, not a policy name, is the identity:
 * a composition the matrix lacks fails without a runtime compiler. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_shader_module_compose(
    sixel_vk_runtime_t *runtime, char const *name,
    sixel_vk_source_part_t const *parts, size_t part_count,
    VkShaderModule *module);

/* command_begin resets and opens the runtime's single primary command
 * buffer and the statistics bracket; the caller records work while holding
 * sixel_vk_lock(); submit_and_wait closes the bracket, submits, waits on the
 * runtime's fence and adds the GPU time to the statistics. command_free is
 * kept for symmetry: the buffer is reused by the next command_begin. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_submit_and_wait(
    sixel_vk_runtime_t *runtime, VkCommandBuffer command);
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_command_begin(
    sixel_vk_runtime_t *runtime, VkCommandBuffer *command);
SIXEL_INTERNAL_API void sixel_vk_command_free(
    sixel_vk_runtime_t *runtime, VkCommandBuffer command);

/* Pipelined submission through ring slot slot (0 .. SIXEL_VK_RING - 1).
 * ring_begin resets and opens the slot's command buffer, which must not be
 * in flight; ring_submit closes and submits it without waiting; ring_wait
 * waits for its fence and adds the submit-to-completion wall time to the
 * statistics. Every submitted slot must be waited for before the caller
 * releases sixel_vk_lock() or reuses the slot. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_ring_begin(
    sixel_vk_runtime_t *runtime, int slot, VkCommandBuffer *command);
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_ring_submit(
    sixel_vk_runtime_t *runtime, int slot);
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_ring_wait(
    sixel_vk_runtime_t *runtime, int slot);

SIXEL_INTERNAL_API double sixel_vk_now_ms(void);
#endif
#endif
