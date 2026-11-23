/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "palette-heckbert-gpu.h"
#include "gpu-compute-vulkan.h"

static char const *const heckbert_source[] = {
#include "generated-vulkan/palette-heckbert-gpu-shader.inc.h"
};
static sixel_vk_compute_t heckbert_stage;
static sixel_gpu_headroom_t heckbert_headroom;
static VkQueryPool heckbert_times;
static VkCommandPool heckbert_pool;
static VkCommandBuffer heckbert_command;
static VkFence heckbert_fence;
/* A solve owns its buffers until its fence signals; other GPU stages may
 * record and submit while this mutex is held without taking this mutex. */
static pthread_mutex_t heckbert_mutex = PTHREAD_MUTEX_INITIALIZER;

enum {
    HECKBERT_POINTS, HECKBERT_ORDER, HECKBERT_SCRATCH,
    HECKBERT_BOXES, HECKBERT_PARAMS, HECKBERT_RESULT,
    HECKBERT_BUFFERS
};

typedef struct heckbert_params {
    uint32_t count;
    uint32_t colors;
    uint32_t total;
    float weights[3];
} heckbert_params_t;

typedef struct heckbert_box {
    uint32_t start;
    uint32_t size;
    uint32_t sum;
    float priority;
} heckbert_box_t;

/* The solver mutex protects these buffers across the unlocked fence wait. */
static sixel_vk_buffer_t heckbert_buffers[HECKBERT_BUFFERS];

static SIXELSTATUS
heckbert_submission_init(sixel_vk_runtime_t *runtime)
{
    VkCommandPoolCreateInfo pool;
    VkCommandBufferAllocateInfo command;
    VkFenceCreateInfo fence;
    VkQueryPoolCreateInfo query;

    if (heckbert_fence != VK_NULL_HANDLE) return SIXEL_OK;
    memset(&pool, 0, sizeof(pool));
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = runtime->compute_queue_family;
    if (vkCreateCommandPool(runtime->device, &pool, NULL,
            &heckbert_pool) != VK_SUCCESS) return SIXEL_RUNTIME_ERROR;
    memset(&command, 0, sizeof(command));
    command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command.commandPool = heckbert_pool;
    command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command.commandBufferCount = 1u;
    if (vkAllocateCommandBuffers(runtime->device, &command,
            &heckbert_command) != VK_SUCCESS) return SIXEL_RUNTIME_ERROR;
    memset(&fence, 0, sizeof(fence));
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(runtime->device, &fence, NULL,
            &heckbert_fence) != VK_SUCCESS) return SIXEL_RUNTIME_ERROR;
    if (runtime->compute_timestamps_supported) {
        memset(&query, 0, sizeof(query));
        query.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        query.queryType = VK_QUERY_TYPE_TIMESTAMP;
        query.queryCount = 4u;
        (void)vkCreateQueryPool(runtime->device, &query, NULL,
            &heckbert_times);
    }
    return SIXEL_OK;
}

int
sixel_palette_heckbert_gpu_vulkan_choose(int gpu_policy)
{
    sixel_vk_runtime_t *runtime;
    int gpu;

    sixel_vk_lock();
    if (gpu_policy == SIXEL_GPU_POLICY_OFF) {
        sixel_gpu_headroom_step(&heckbert_headroom, gpu_policy, 0, 0);
        sixel_vk_unlock();
        return 0;
    }
    runtime = sixel_vk_runtime_get_locked();
    if (runtime == NULL) {
        sixel_vk_unlock();
        return 0;
    }
    gpu = sixel_vk_headroom_choose_locked(&heckbert_headroom, gpu_policy);
    if (runtime->debug)
        fprintf(stderr, "LSXHECKCHOICE|gpu=%d|load=%.3f|windows=%d\n",
            gpu, heckbert_headroom.load, heckbert_headroom.windows);
    sixel_vk_unlock();
    return gpu;
}

/* The 256-bin scan needs complete subgroups and at least 256 lanes. */
static uint32_t
heckbert_threads(sixel_vk_runtime_t *runtime)
{
    VkPhysicalDeviceSubgroupProperties subgroup;
    VkPhysicalDeviceProperties2 properties;
    VkPhysicalDeviceLimits const *limits;
    uint32_t required;
    uint32_t threads;
    uint32_t width;

    memset(&subgroup, 0, sizeof(subgroup));
    subgroup.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    memset(&properties, 0, sizeof(properties));
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &subgroup;
    vkGetPhysicalDeviceProperties2(runtime->physical, &properties);
    required = VK_SUBGROUP_FEATURE_BASIC_BIT |
        VK_SUBGROUP_FEATURE_BALLOT_BIT |
        VK_SUBGROUP_FEATURE_SHUFFLE_BIT |
        VK_SUBGROUP_FEATURE_ARITHMETIC_BIT;
    width = subgroup.subgroupSize;
    if ((subgroup.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) == 0 ||
            (subgroup.supportedOperations & required) != required ||
            width < 8u || width > 64u || (width & (width - 1u)) != 0u ||
            256u % width != 0u) return 0u;
    limits = &runtime->properties.limits;
    if (limits->maxComputeSharedMemorySize < 32768u) return 0u;
    threads = limits->maxComputeWorkGroupInvocations;
    if (threads > limits->maxComputeWorkGroupSize[0])
        threads = limits->maxComputeWorkGroupSize[0];
    if (threads > 1024u) threads = 1024u;
    if (threads > 128u * width) threads = 128u * width;
    threads -= threads % width;
    return threads >= 256u ? threads : 0u;
}

static SIXELSTATUS
heckbert_reserve(sixel_vk_runtime_t *runtime, int slot, size_t bytes)
{
    if (slot == HECKBERT_POINTS || slot == HECKBERT_PARAMS)
        return sixel_vk_buffer_reserve(runtime, &heckbert_buffers[slot],
            bytes, 1);
    if (slot == HECKBERT_SCRATCH)
        return sixel_vk_buffer_reserve_device(runtime,
            &heckbert_buffers[slot], bytes);
    return sixel_vk_buffer_reserve_readback(runtime,
        &heckbert_buffers[slot], bytes);
}

/* Keep unsupported populations on the CPU: FP32 represents integers only
 * through 2^24. The fixed histogram bound protects bucket rank sorting. */
static int
heckbert_gpu_admission(sixel_heckbert_solve_request_t const *request,
    uint32_t *total)
{
    unsigned int i, plane;
    double value;

    if (request->count > 4096u) return 0;
    for (plane = 0u; plane < 3u; ++plane) {
        if (!isfinite(request->axis_weights[plane]) ||
                request->axis_weights[plane] < 0.0) return 0;
    }
    *total = 0u;
    for (i = 0u; i < request->count; ++i) {
        if (request->weights[i] == 0u ||
                request->weights[i] > (1u << 24) - *total) return 0;
        *total += request->weights[i];
        for (plane = 0u; plane < 3u; ++plane) {
            value = request->tuples[(size_t)i * 3u + plane];
            if (!isfinite(value) || value < 0.0 || value > 255.0)
                return 0;
        }
    }
    return 1;
}

/* Reject an incomplete partition or invalid order before output escapes. */
static int
heckbert_result_valid(uint32_t const *order, heckbert_box_t const *boxes,
    uint32_t *seen, uint32_t count, uint32_t colors, uint32_t box_count)
{
    uint32_t i;
    uint32_t index;
    uint32_t start;

    if (box_count == 0u || box_count > colors) return 0;
    memset(seen, 0, (size_t)count * sizeof(*seen));
    for (i = 0u; i < count; ++i) {
        index = order[i];
        if (index >= count || seen[index] != 0u) return 0;
        seen[index] = 1u;
    }
    memset(seen, 0, (size_t)count * sizeof(*seen));
    for (i = 0u; i < box_count; ++i) {
        start = boxes[i].start;
        if (start >= count || boxes[i].size == 0u ||
                boxes[i].size > count - start || seen[start] != 0u)
            return 0;
        seen[start] = boxes[i].size;
    }
    start = 0u;
    for (i = 0u; i < box_count; ++i) {
        if (start >= count || seen[start] == 0u) return 0;
        start += seen[start];
    }
    return start == count;
}

SIXELSTATUS
sixel_palette_heckbert_gpu_vulkan_solve(
    sixel_heckbert_solve_request_t const *request)
{
    sixel_vk_runtime_t *runtime;
    sixel_vk_buffer_t const *bound[HECKBERT_BUFFERS];
    VkCommandBuffer command;
    VkCommandBufferBeginInfo begin;
    VkSubmitInfo submit;
    VkResult vk_status;
    size_t lengths[HECKBERT_BUFFERS];
    float *points;
    heckbert_params_t *params;
    heckbert_box_t const *boxes;
    uint32_t const *order;
    uint32_t const *result;
    uint64_t stamps[4];
    uint64_t mask;
    unsigned long long busy;
    uint32_t threads;
    uint32_t total;
    unsigned int index;
    unsigned int plane;
    double entered;
    double locked;
    double prepared;
    double waited;
    double finished;
    double busy_ms;
    double kernel_ms;
    double transfer_ms;
    double wait_ms;
    SIXELSTATUS status;

    if (!heckbert_gpu_admission(request, &total))
        return SIXEL_FEATURE_ERROR;
    lengths[HECKBERT_POINTS] = (size_t)request->count * 4u * sizeof(float);
    lengths[HECKBERT_ORDER] = (size_t)request->count * sizeof(uint32_t);
    lengths[HECKBERT_SCRATCH] = lengths[HECKBERT_ORDER];
    lengths[HECKBERT_BOXES] = 256u * sizeof(heckbert_box_t);
    lengths[HECKBERT_PARAMS] = sizeof(heckbert_params_t);
    lengths[HECKBERT_RESULT] = sizeof(uint32_t);
    entered = sixel_vk_now_ms();
    (void)pthread_mutex_lock(&heckbert_mutex);
    sixel_vk_lock();
    locked = sixel_vk_now_ms();
    runtime = sixel_vk_runtime_get_locked();
    status = SIXEL_FEATURE_ERROR;
    if (runtime == NULL) goto end;
    threads = heckbert_threads(runtime);
    if (threads == 0u) goto end;
    status = sixel_vk_compute_create_local(runtime, &heckbert_stage,
        heckbert_source, sizeof(heckbert_source) /
            sizeof(heckbert_source[0]), "heckbert_solve", HECKBERT_BUFFERS,
        threads);
    if (status != SIXEL_OK) goto end;
    status = heckbert_submission_init(runtime);
    if (status != SIXEL_OK) goto end;
    for (index = 0u; index < HECKBERT_BUFFERS; ++index) {
        status = heckbert_reserve(runtime, (int)index, lengths[index]);
        if (status != SIXEL_OK) goto end;
        bound[index] = &heckbert_buffers[index];
    }
    points = heckbert_buffers[HECKBERT_POINTS].mapped;
    for (index = 0u; index < request->count; ++index) {
        for (plane = 0u; plane < 3u; ++plane)
            points[(size_t)index * 4u + plane] =
                (float)request->tuples[(size_t)index * 3u + plane];
        points[(size_t)index * 4u + 3u] =
            (float)request->weights[index];
    }
    params = heckbert_buffers[HECKBERT_PARAMS].mapped;
    params->count = request->count;
    params->colors = request->colors;
    params->total = total;
    for (plane = 0u; plane < 3u; ++plane)
        params->weights[plane] = (float)request->axis_weights[plane];
    status = sixel_vk_compute_bind(runtime, &heckbert_stage, bound);
    if (status != SIXEL_OK) goto end;
    command = heckbert_command;
    if (vkResetCommandBuffer(command, 0u) != VK_SUCCESS) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    memset(&begin, 0, sizeof(begin));
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(command, &begin) != VK_SUCCESS) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    if (heckbert_times != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(command, heckbert_times, 0u, 4u);
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            heckbert_times, 0u);
    }
    sixel_vk_buffer_record_upload(command,
        &heckbert_buffers[HECKBERT_POINTS], lengths[HECKBERT_POINTS]);
    sixel_vk_buffer_record_upload(command,
        &heckbert_buffers[HECKBERT_PARAMS], lengths[HECKBERT_PARAMS]);
    sixel_vk_compute_barrier(command);
    if (heckbert_times != VK_NULL_HANDLE)
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            heckbert_times, 1u);
    sixel_vk_compute_dispatch(command, &heckbert_stage, 1u);
    if (heckbert_times != VK_NULL_HANDLE)
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            heckbert_times, 2u);
    sixel_vk_compute_download_barrier(command);
    for (index = HECKBERT_ORDER; index < HECKBERT_BUFFERS; ++index) {
        if (index == HECKBERT_SCRATCH || index == HECKBERT_PARAMS) continue;
        sixel_vk_buffer_record_download(command,
            &heckbert_buffers[index], lengths[index]);
    }
    if (heckbert_times != VK_NULL_HANDLE)
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            heckbert_times, 3u);
    if (vkEndCommandBuffer(command) != VK_SUCCESS ||
            vkResetFences(runtime->device, 1u,
                &heckbert_fence) != VK_SUCCESS) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    memset(&submit, 0, sizeof(submit));
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &command;
    prepared = sixel_vk_now_ms();
    vk_status = vkQueueSubmit(runtime->compute_queue, 1u, &submit,
        heckbert_fence);
    if (vk_status != VK_SUCCESS) {
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }
    /* No shared runtime state is used until the private fence completes. */
    sixel_vk_unlock();
    vk_status = vkWaitForFences(runtime->device, 1u, &heckbert_fence,
        VK_TRUE, UINT64_MAX);
    waited = sixel_vk_now_ms();
    sixel_vk_lock();
    if (vk_status != VK_SUCCESS) {
        status = SIXEL_RUNTIME_ERROR;
        goto end_unlocked;
    }
    kernel_ms = 0.0;
    busy_ms = waited - prepared;
    if (heckbert_times != VK_NULL_HANDLE &&
            vkGetQueryPoolResults(runtime->device, heckbert_times, 0u, 4u,
                sizeof(stamps), stamps, sizeof(stamps[0]),
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) ==
                    VK_SUCCESS) {
        mask = runtime->compute_timestamp_mask;
        kernel_ms = (double)((stamps[2] - stamps[1]) & mask) *
            runtime->timestamp_period / 1000000.0;
        busy_ms = (double)((stamps[3] - stamps[0]) & mask) *
            runtime->timestamp_period / 1000000.0;
    }
    busy = (unsigned long long)(busy_ms * 1000000.0);
    sixel_gpu_statistics_record(busy);
    if (runtime->debug) {
        fprintf(stderr, "LSXHECKTIME|colors=%u|kernel=%.6f\n",
            request->colors, kernel_ms);
    }
    for (index = HECKBERT_ORDER; index < HECKBERT_BUFFERS; ++index) {
        if (index == HECKBERT_SCRATCH || index == HECKBERT_PARAMS) continue;
        status = sixel_vk_buffer_invalidate(runtime,
            &heckbert_buffers[index], lengths[index]);
        if (status != SIXEL_OK) goto end_unlocked;
    }
    result = heckbert_buffers[HECKBERT_RESULT].mapped;
    order = heckbert_buffers[HECKBERT_ORDER].mapped;
    boxes = heckbert_buffers[HECKBERT_BOXES].mapped;
    if (!heckbert_result_valid(order, boxes, request->order, request->count,
            request->colors, result[0])) {
        status = SIXEL_FEATURE_ERROR;
        goto end_unlocked;
    }
    memcpy(request->order, order, lengths[HECKBERT_ORDER]);
    *request->box_count = result[0];
    for (index = 0u; index < result[0]; ++index) {
        request->box_start[index] = boxes[index].start;
        request->box_size[index] = boxes[index].size;
        request->box_sum[index] = boxes[index].sum;
        request->box_priority[index] = (double)boxes[index].priority;
    }
    status = SIXEL_OK;
end_unlocked:
    finished = sixel_vk_now_ms();
    if (runtime != NULL && runtime->debug && status == SIXEL_OK) {
        transfer_ms = busy_ms - kernel_ms;
        wait_ms = waited - prepared - busy_ms;
        fprintf(stderr, "LSXHECKPROFILE|count=%u|colors=%u|lock=%.6f"
            "|host=%.6f|kernel=%.6f|transfer=%.6f|wait=%.6f"
            "|readback=%.6f|total=%.6f\n", request->count,
            request->colors, locked - entered, prepared - locked,
            kernel_ms, transfer_ms, wait_ms, finished - waited,
            finished - entered);
    }
    sixel_vk_unlock();
    (void)pthread_mutex_unlock(&heckbert_mutex);
    return status;
end:
    sixel_vk_unlock();
    (void)pthread_mutex_unlock(&heckbert_mutex);
    return status;
}
#else
/* Meson also compiles this file when the Vulkan solver is disabled. */
typedef int palette_heckbert_gpu_vulkan_disabled;
#endif
