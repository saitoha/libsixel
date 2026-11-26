/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_RUNTIME_VULKAN_H
#define LIBSIXEL_GPU_RUNTIME_VULKAN_H
#if defined(HAVE_VULKAN)
#include <stddef.h>
#include <vulkan/vulkan.h>
#include <sixel.h>
#include "gpu-statistics.h"

/*
 * Process-wide Vulkan compute runtime. Initialization is lazy: nothing
 * touches the loader until a GPU stage asks for the runtime, because
 * instance creation alone scans every installed ICD.
 *
 * Every function below except sixel_vk_lock() requires the caller to hold
 * the runtime lock. One queue is shared; each submitting stage owns its
 * command pool, buffers and fences through gpu-compute-vulkan.h.
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
    /* Heckbert uses another compute queue when the device provides one. */
    VkQueue compute_queue;
    uint32_t compute_queue_family;
    int compute_timestamps_supported;
    uint64_t compute_timestamp_mask;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory;
    /* Without timestamp support submit/wait wall time is recorded. */
    int timestamps_supported;
    uint64_t timestamp_mask;
    double timestamp_period;
    /* Device time of the latest submission in milliseconds, or its
     * submit/wait wall time without timestamps; for debug breakdowns. */
    double last_busy_ms;
    int debug;
    /* Optional for 64-bit progressive canvas positions and strata. */
    int shader_int64;
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
    /* The mapping needs sixel_vk_buffer_invalidate() before host reads. */
    int noncoherent;
    /* The shader's buffer allocation belongs to a DEVICE_LOCAL type. */
    int device_local;
} sixel_vk_buffer_t;
typedef struct sixel_vk_submission sixel_vk_submission_t;

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
/* A buffer the shader writes and the host reads in place. It prefers
 * cached host memory even without coherence; call invalidate after the
 * fence wait and before reading the first size bytes, and flush after
 * host writes the shader must see. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_buffer_reserve_readback(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t *buffer,
    VkDeviceSize size);
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_buffer_invalidate(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t const *buffer,
    VkDeviceSize size);
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_buffer_flush(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t const *buffer,
    VkDeviceSize size);
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_buffer_invalidate_range(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t const *buffer,
    VkDeviceSize offset, VkDeviceSize size);
/* Nonzero when a host-visible memory type is both coherent and cached, so
 * one allocation serves host reads and device transfers without syncs. */
SIXEL_INTERNAL_API int sixel_vk_has_cached_coherent(
    sixel_vk_runtime_t const *runtime);
/* One buffer in coherent cached host memory and nothing on the device:
 * the host uses mapped and device work reaches it by transfer copies.
 * SIXEL_FEATURE_ERROR without such memory. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_buffer_reserve_host(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t *buffer,
    VkDeviceSize size);
/* A direct, cached mapping for a shader-written readback buffer. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_buffer_reserve_host_cached(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t *buffer,
    VkDeviceSize size);
/* Copy size bytes at the same offset of two buffers' device storage. */
SIXEL_INTERNAL_API void sixel_vk_record_copy(VkCommandBuffer command,
    sixel_vk_buffer_t const *source, sixel_vk_buffer_t const *target,
    VkDeviceSize offset, VkDeviceSize size);
/* Promote an existing allocation to directly shared coherent storage,
 * preserving its bytes. Prefer host-visible VRAM; never expose a staging
 * pointer as shared memory because host writes must reach future kernels. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_buffer_make_shared(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t *buffer,
    sixel_vk_submission_t *submission);
/* Device-local storage the host never maps (mapped stays NULL), for
 * intermediate tables that shaders both write and read. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_buffer_reserve_device(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t *buffer,
    VkDeviceSize size);
/* Additional usage flags stay fixed for the lifetime of a reserved slot. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_buffer_reserve_device_usage(
    sixel_vk_runtime_t *runtime, sixel_vk_buffer_t *buffer,
    VkDeviceSize size, VkBufferUsageFlags usage);
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

SIXEL_INTERNAL_API double sixel_vk_now_ms(void);
/* Each stage owns its state; callers hold the runtime lock. */
SIXEL_INTERNAL_API int sixel_vk_headroom_choose_locked(
    sixel_gpu_headroom_t *state, int gpu_policy);
#endif
#endif
