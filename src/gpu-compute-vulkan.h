/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_GPU_COMPUTE_VULKAN_H
#define LIBSIXEL_GPU_COMPUTE_VULKAN_H
#include "gpu-runtime-vulkan.h"
#if defined(HAVE_VULKAN)
#include <pthread.h>

/* A stage holds gate across its whole operation, including the fence wait.
 * It acquires gate before the runtime lock. The private pool, command,
 * fence and timestamps remain untouched while another stage submits work. */
typedef struct sixel_vk_submission {
    pthread_mutex_t gate;
    VkCommandPool pool;
    VkCommandBuffer command;
    VkFence fence;
    VkQueryPool timestamps;
    double last_busy_ms;
} sixel_vk_submission_t;
/* Vulkan handles and timing fields start at zero until first use. */
#define SIXEL_VK_SUBMISSION_INITIALIZER { .gate = PTHREAD_MUTEX_INITIALIZER }

SIXEL_INTERNAL_API int sixel_vk_submission_init(
    sixel_vk_submission_t *stage);
SIXEL_INTERNAL_API void sixel_vk_submission_destroy(
    sixel_vk_runtime_t *runtime, sixel_vk_submission_t *stage);
SIXEL_INTERNAL_API void sixel_vk_submission_enter(
    sixel_vk_submission_t *stage);
SIXEL_INTERNAL_API void sixel_vk_submission_leave(
    sixel_vk_submission_t *stage);
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_submission_begin(
    sixel_vk_runtime_t *runtime, sixel_vk_submission_t *stage,
    VkCommandBuffer *command);
/* Called with the runtime lock held; returns with it held. Only the fence
 * wait is outside that lock, while the caller continues to hold gate. */
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_submission_submit_and_wait(
    sixel_vk_runtime_t *runtime, sixel_vk_submission_t *stage,
    VkCommandBuffer command);

/* Palette application pipelines several command buffers, so it owns a
 * private ring with the same lock ordering as a synchronous submission. */
typedef struct sixel_vk_ring_submission {
    pthread_mutex_t gate;
    VkCommandPool pool;
    VkCommandBuffer commands[SIXEL_VK_RING];
    VkFence fences[SIXEL_VK_RING];
    double submitted_ms[SIXEL_VK_RING];
} sixel_vk_ring_submission_t;
#define SIXEL_VK_RING_SUBMISSION_INITIALIZER \
    { .gate = PTHREAD_MUTEX_INITIALIZER }
SIXEL_INTERNAL_API void sixel_vk_ring_submission_enter(
    sixel_vk_ring_submission_t *stage);
SIXEL_INTERNAL_API void sixel_vk_ring_submission_leave(
    sixel_vk_ring_submission_t *stage);
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_ring_submission_begin(
    sixel_vk_runtime_t *runtime, sixel_vk_ring_submission_t *stage,
    int slot, VkCommandBuffer *command);
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_ring_submission_submit(
    sixel_vk_runtime_t *runtime, sixel_vk_ring_submission_t *stage,
    int slot);
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_ring_submission_wait(
    sixel_vk_runtime_t *runtime, sixel_vk_ring_submission_t *stage,
    int slot);

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
SIXEL_INTERNAL_API SIXELSTATUS sixel_vk_compute_create_local(
    sixel_vk_runtime_t *runtime, sixel_vk_compute_t *stage,
    char const *const *chunks, size_t count, char const *entry,
    unsigned int bindings, uint32_t local_size_x);
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
