/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN)
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "filter-sample.h"
#include "gpu-compute-vulkan.h"
#include "gpu-statistics.h"

static char const *const sample_source[] = {
#include "generated-vulkan/filter-sample-gpu-shader.inc.h"
};

typedef struct sample_params {
    int32_t width, height, clip_x, clip_y;
    uint32_t points, rows, tolerance;
} sample_params_t;

/* The process-wide runtime lock protects these reusable allocations. */
static sixel_vk_compute_t sample_stage;
static sixel_vk_buffer_t sample_buffers[3];
static VkQueryPool sample_times;
static sixel_sample_vulkan_headroom_t sample_headroom;

int
sixel_filter_sample_vulkan_headroom_step(
    sixel_sample_vulkan_headroom_t *state, int gpu_policy,
    unsigned long long busy_ns, unsigned long long wall_ns)
{
    unsigned long long elapsed;
    unsigned long long busy;
    double load;

    if (state == NULL) return 0;
    if (gpu_policy == SIXEL_GPU_POLICY_OFF) {
        memset(state, 0, sizeof(*state));
        return 0;
    }
    if (state->last_wall_ns == 0 || wall_ns <= state->last_wall_ns ||
            busy_ns < state->last_busy_ns) {
        memset(state, 0, sizeof(*state));
        state->last_busy_ns = busy_ns;
        state->last_wall_ns = wall_ns;
        return 0;
    }
    ++state->frames;
    elapsed = wall_ns - state->last_wall_ns;
    if (state->frames < 8 || elapsed < 20000000ULL)
        return state->gpu;
    busy = busy_ns - state->last_busy_ns;
    load = (double)busy / (double)elapsed;
    state->load = state->windows == 0 ? load :
        (state->load + load) * 0.5;
    ++state->windows;
    state->frames = 0;
    state->last_busy_ns = busy_ns;
    state->last_wall_ns = wall_ns;
    /* A GPU needs ample idle time before taking the scan; once enabled,
     * it stays there until device work consumes most of the frame window. */
    if (state->windows >= 2 && state->load < 0.50)
        state->gpu = 1;
    else if (state->load > 0.70)
        state->gpu = 0;
    return state->gpu;
}

int
sixel_filter_sample_vulkan_choose(int gpu_policy)
{
    sixel_vk_runtime_t *runtime;
    sixel_gpu_statistics_t statistics;
    unsigned long long now_ns;
    int gpu;

    if (gpu_policy == SIXEL_GPU_POLICY_OFF) return 0;
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    if (runtime == NULL) {
        sixel_vk_unlock();
        return 0;
    }
    memset(&statistics, 0, sizeof(statistics));
    if (sixel_gpu_get_statistics(&statistics) != SIXEL_OK ||
            !statistics.available ||
            strcmp(statistics.backend, "vulkan") != 0) {
        sixel_vk_unlock();
        return 0;
    }
    now_ns = (unsigned long long)(sixel_vk_now_ms() * 1000000.0);
    gpu = sixel_filter_sample_vulkan_headroom_step(&sample_headroom,
        gpu_policy, statistics.busy_nanoseconds, now_ns);
    if (runtime->debug)
        fprintf(stderr, "LSXSAMCHOICE|gpu=%d|load=%.3f|windows=%d\n",
            gpu, sample_headroom.load, sample_headroom.windows);
    sixel_vk_unlock();
    return gpu;
}

#define SIXEL_SAMPLE_SHARED_SLOTS 4
typedef struct sample_shared_input {
    sixel_vk_buffer_t buffer;
    sixel_frame_t *owner;
    unsigned char const *pixels;
    size_t length;
    unsigned long age;
    int borrowed;
    int ready;
} sample_shared_input_t;

/* All slots are accessed under the process-wide Vulkan runtime lock. The
 * owner reference prevents an address from being recycled before dither. */
static sample_shared_input_t sample_shared[SIXEL_SAMPLE_SHARED_SLOTS];
static unsigned long sample_shared_age;

int
sixel_filter_sample_vulkan_borrow_locked(unsigned char const *pixels,
    size_t length, sixel_vk_buffer_t const **buffer)
{
    int slot;

    if (buffer == NULL) return -1;
    *buffer = NULL;
    for (slot = 0; slot < SIXEL_SAMPLE_SHARED_SLOTS; ++slot) {
        if (sample_shared[slot].owner != NULL &&
                sample_shared[slot].ready &&
                !sample_shared[slot].borrowed &&
                sample_shared[slot].buffer.device_local &&
                sample_shared[slot].pixels == pixels &&
                sample_shared[slot].length == length &&
                sixel_frame_get_pixels(sample_shared[slot].owner) ==
                    pixels) {
            sample_shared[slot].borrowed = 1;
            *buffer = &sample_shared[slot].buffer;
            return slot;
        }
    }
    return -1;
}

sixel_frame_t *
sixel_filter_sample_vulkan_release_locked(int slot)
{
    sample_shared_input_t *entry;
    sixel_frame_t *owner;

    if (slot < 0 || slot >= SIXEL_SAMPLE_SHARED_SLOTS) return NULL;
    entry = &sample_shared[slot];
    if (!entry->borrowed) return NULL;
    owner = entry->owner;
    entry->borrowed = 0;
    entry->ready = 0;
    entry->pixels = NULL;
    entry->length = 0;
    entry->owner = NULL;
    return owner;
}

void
sixel_filter_sample_vulkan_finish_preplan(sixel_frame_t *owner,
    int unchanged)
{
    sample_shared_input_t *entry;
    sixel_frame_t *released[SIXEL_SAMPLE_SHARED_SLOTS];
    int slot;
    int count;

    if (owner == NULL) return;
    count = 0;
    sixel_vk_lock();
    for (slot = 0; slot < SIXEL_SAMPLE_SHARED_SLOTS; ++slot) {
        entry = &sample_shared[slot];
        if (entry->owner != owner || entry->borrowed) continue;
        if (unchanged && sixel_frame_get_pixels(owner) == entry->pixels) {
            entry->ready = 1;
        } else {
            entry->pixels = NULL;
            entry->length = 0;
            released[count++] = entry->owner;
            entry->owner = NULL;
            entry->ready = 0;
        }
    }
    sixel_vk_unlock();
    for (slot = 0; slot < count; ++slot)
        sixel_frame_unref(released[slot]);
}

SIXELSTATUS
sixel_filter_sample_solid_gpu(unsigned char const *pixels,
    int width, int height, int clip_x, int clip_y, int clip_width,
    int clip_height, sixel_frame_t *share_owner, unsigned int *cells)
{
    sixel_vk_runtime_t *runtime;
    sixel_frame_t *evicted_owner;
    sixel_vk_buffer_t *input;
    sixel_vk_buffer_t const *bound[3];
    sample_shared_input_t *entry;
    VkQueryPoolCreateInfo query_info;
    VkCommandBuffer command;
    sample_params_t params;
    uint64_t stamps[4];
    size_t length;
    size_t cell_bytes;
    uint32_t groups;
    double entered, locked, staged, finished;
    double period;
    SIXELSTATUS status;
    int timing;
    int slot;
    int i;

    if (pixels == NULL || cells == NULL || width <= 0 || height <= 0 ||
        clip_x < 0 || clip_y < 0 || clip_width <= 0 || clip_height <= 0 ||
        clip_x > width - clip_width || clip_y > height - clip_height ||
        (size_t)width * (size_t)height > UINT32_MAX / 3u)
        return SIXEL_BAD_ARGUMENT;
    length = (size_t)width * (size_t)height * 3u;
    cell_bytes = (size_t)SIXEL_SAMPLE_SOLID_CELLS * 5u * sizeof(uint32_t);
    params.width = width;
    params.height = height;
    params.clip_x = clip_x;
    params.clip_y = clip_y;
    params.points = (uint32_t)(1 + (clip_width - 1) /
        SIXEL_SAMPLE_SOLID_STRIDE);
    params.rows = (uint32_t)(1 + (clip_height - 1) /
        SIXEL_SAMPLE_SOLID_STRIDE);
    params.tolerance = SIXEL_SAMPLE_SOLID_TOL;
    groups = (params.points * params.rows + 511u) / 512u;
    entered = sixel_vk_now_ms();
    evicted_owner = NULL;
    sixel_vk_lock();
    locked = sixel_vk_now_ms();
    runtime = sixel_vk_runtime_get_locked();
    status = SIXEL_FEATURE_ERROR;
    if (runtime == NULL) goto end;
    slot = -1;
    input = &sample_buffers[0];
    if (share_owner != NULL) {
        for (i = 0; i < SIXEL_SAMPLE_SHARED_SLOTS; ++i) {
            if (!sample_shared[i].borrowed &&
                    sample_shared[i].owner == NULL) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            for (i = 0; i < SIXEL_SAMPLE_SHARED_SLOTS; ++i) {
                if (!sample_shared[i].borrowed &&
                        (slot < 0 || sample_shared[i].age <
                            sample_shared[slot].age))
                    slot = i;
            }
        }
        if (slot >= 0) {
            entry = &sample_shared[slot];
            evicted_owner = entry->owner;
            entry->owner = NULL;
            entry->pixels = NULL;
            entry->ready = 0;
            input = &entry->buffer;
        }
    }
    status = sixel_vk_compute_create(runtime, &sample_stage,
        sample_source, sizeof(sample_source) / sizeof(sample_source[0]),
        "sample_solid", 3);
    if (status != SIXEL_OK) goto end;
    status = sixel_vk_buffer_reserve(runtime, input, length, 1);
    if (status != SIXEL_OK) goto end;
    if (share_owner != NULL && !input->device_local) {
        status = SIXEL_FEATURE_ERROR;
        goto end;
    }
    status = sixel_vk_buffer_reserve_readback(runtime, &sample_buffers[1],
        cell_bytes);
    if (status != SIXEL_OK) goto end;
    status = sixel_vk_buffer_reserve(runtime, &sample_buffers[2],
        sizeof(params), 1);
    if (status != SIXEL_OK) goto end;
    memcpy(input->mapped, pixels, length);
    memset(sample_buffers[1].mapped, 0, cell_bytes);
    memcpy(sample_buffers[2].mapped, &params, sizeof(params));
    bound[0] = input;
    bound[1] = &sample_buffers[1];
    bound[2] = &sample_buffers[2];
    status = sixel_vk_buffer_flush(runtime, &sample_buffers[1], cell_bytes);
    if (status != SIXEL_OK) goto end;
    status = sixel_vk_compute_bind(runtime, &sample_stage, bound);
    if (status != SIXEL_OK) goto end;
    timing = runtime->debug && runtime->timestamps != VK_NULL_HANDLE;
    if (timing && sample_times == VK_NULL_HANDLE) {
        memset(&query_info, 0, sizeof(query_info));
        query_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        query_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
        query_info.queryCount = 4;
        if (vkCreateQueryPool(runtime->device, &query_info, NULL,
                &sample_times) != VK_SUCCESS)
            timing = 0;
    }
    staged = sixel_vk_now_ms();
    status = sixel_vk_command_begin(runtime, &command);
    if (status != SIXEL_OK) goto end;
    if (timing) {
        vkCmdResetQueryPool(command, sample_times, 0, 4);
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            sample_times, 0);
    }
    sixel_vk_buffer_record_upload(command, input, length);
    sixel_vk_buffer_record_upload(command, &sample_buffers[1], cell_bytes);
    sixel_vk_buffer_record_upload(command, &sample_buffers[2],
        sizeof(params));
    if (timing)
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
            sample_times, 1);
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &sample_stage, groups);
    if (timing)
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            sample_times, 2);
    sixel_vk_compute_download_barrier(command);
    sixel_vk_buffer_record_download(command, &sample_buffers[1], cell_bytes);
    if (timing)
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            sample_times, 3);
    status = sixel_vk_submit_and_wait(runtime, command);
    finished = sixel_vk_now_ms();
    if (status != SIXEL_OK) goto end;
    status = sixel_vk_buffer_invalidate(runtime, &sample_buffers[1],
        cell_bytes);
    if (status != SIXEL_OK) goto end;
    memcpy(cells, sample_buffers[1].mapped, cell_bytes);
    if (slot >= 0) {
        entry = &sample_shared[slot];
        sixel_frame_ref(share_owner);
        entry->owner = share_owner;
        entry->pixels = pixels;
        entry->length = length;
        entry->age = ++sample_shared_age;
        entry->ready = 0;
        if (runtime->debug)
            fprintf(stderr, "LSXSAMSHARE|publish=%p|slot=%d\n",
                (void *)pixels, slot);
    }
    if (timing && vkGetQueryPoolResults(runtime->device, sample_times,
            0, 4, sizeof(stamps), stamps, sizeof(stamps[0]),
            VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
        period = runtime->timestamp_period / 1000000.0;
        fprintf(stderr, "LSXSAM1|lock=%.3f|host=%.3f|upload=%.3f"
            "|kernel=%.3f|readback=%.3f|wait=%.3f\n",
            locked - entered, staged - locked,
            (stamps[1] - stamps[0]) * period,
            (stamps[2] - stamps[1]) * period,
            (stamps[3] - stamps[2]) * period,
            finished - staged - (stamps[3] - stamps[0]) * period);
    }
end:
    sixel_vk_unlock();
    sixel_frame_unref(evicted_owner);
    return status;
}
#endif
