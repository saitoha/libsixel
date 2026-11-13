/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN)
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "progressive-gpu.h"
#include "gpu-compute-vulkan.h"

static char const *const repair_scan_source[] = {
#include "generated-vulkan/progressive-gpu-scan-shader.inc.h"
};
static char const *const repair_lookup_source[] = {
#include "generated-vulkan/progressive-gpu-lookup-shader.inc.h"
};
static sixel_vk_compute_t repair_scan_stages[3];
static sixel_vk_compute_t repair_lookup_stage;
enum sixel_repair_vulkan_buffer {
    REPAIR_GPU_SOURCE, REPAIR_GPU_HISTORY, REPAIR_GPU_DOMAIN,
    REPAIR_GPU_VALID, REPAIR_GPU_POSITIONS, REPAIR_GPU_ERRORS,
    REPAIR_GPU_FLAGS, REPAIR_GPU_GROUPS, REPAIR_GPU_SUMMARY,
    REPAIR_GPU_SAMPLES, REPAIR_GPU_FILTER, REPAIR_GPU_SCAN_BUFFERS,
    REPAIR_GPU_QUERIES = REPAIR_GPU_SCAN_BUFFERS,
    REPAIR_GPU_PALETTE, REPAIR_GPU_MATCHES, REPAIR_GPU_BUFFERS
};

typedef struct sixel_repair_vulkan_params {
    uint64_t first;
    uint32_t count;
    uint32_t sparse;
    uint32_t width;
    uint32_t plane_width;
    uint32_t origin_x;
    uint32_t origin_y;
    float stop;
    float threshold;
    uint32_t sample_limit;
    uint32_t filtered;
    uint64_t sample_population;
    uint64_t sample_offset;
    uint32_t sample_total;
    uint32_t sample_first;
    uint32_t preselected;
    uint32_t reserved;
} sixel_repair_vulkan_params_t;


typedef struct sixel_repair_vulkan_cache {
    sixel_vk_buffer_t buffers[REPAIR_GPU_BUFFERS];
    sixel_vk_buffer_t params;
    size_t upload[REPAIR_GPU_SCAN_BUFFERS];
    sixel_repair_gpu_view_t view;
    int bound;
} sixel_repair_vulkan_cache_t;

void
sixel_repair_gpu_vulkan_free(void *backend)
{
    sixel_repair_vulkan_cache_t *cache;
    sixel_vk_runtime_t *runtime;
    unsigned int i;

    cache = backend;
    if (cache == NULL) return;
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    for (i = 0; i < REPAIR_GPU_BUFFERS; ++i)
        sixel_vk_buffer_release(runtime, &cache->buffers[i]);
    sixel_vk_buffer_release(runtime, &cache->params);
    sixel_vk_unlock();
    free(cache);
}

static int
sixel_repair_vulkan_buffer(sixel_vk_runtime_t *runtime,
    sixel_repair_vulkan_cache_t *cache, unsigned int slot, size_t bytes)
{
    return sixel_vk_buffer_reserve(runtime, &cache->buffers[slot], bytes, 0)
        == SIXEL_OK;
}

static void
sixel_repair_vulkan_upload(sixel_repair_vulkan_cache_t *cache,
    unsigned int slot, void const *data, size_t bytes,
    sixel_repair_gpu_result_t *result)
{
    memcpy(cache->buffers[slot].mapped, data, bytes);
    cache->upload[slot] = bytes;
    result->uploaded_bytes += bytes;
}

SIXELSTATUS
sixel_repair_gpu_vulkan_scan(void **backend,
    sixel_repair_gpu_request_t const *request,
    sixel_repair_gpu_result_t *result)
{
    static char const *const entries[3] = {
        "repair_scan", "repair_reduce", "repair_sample"
    };
    sixel_repair_vulkan_cache_t *cache;
    sixel_repair_gpu_view_t const *view;
    sixel_repair_gpu_view_t const *old;
    sixel_repair_vulkan_params_t params;
    sixel_vk_runtime_t *runtime;
    sixel_vk_buffer_t const *bound[12];
    VkCommandBuffer command;
    size_t sizes[REPAIR_GPU_BUFFERS];
    size_t groups;
    size_t bytes;
    unsigned int i;
    unsigned int stage;
    int changed;
    SIXELSTATUS status;

    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = SIXEL_FEATURE_ERROR;
    if (runtime == NULL || !runtime->shader_int64 ||
            request->view.count > UINT32_MAX / 12u) goto end;
    for (stage = 0; stage < 3; ++stage) {
        status = sixel_vk_compute_create(runtime, &repair_scan_stages[stage],
            repair_scan_source,
            sizeof(repair_scan_source) / sizeof(repair_scan_source[0]),
            entries[stage], 12);
        if (status != SIXEL_OK) goto end;
    }
    cache = *backend;
    if (cache == NULL) {
        cache = calloc(1, sizeof(*cache));
        if (cache == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        *backend = cache;
    }
    memset(cache->upload, 0, sizeof(cache->upload));
    view = &request->view;
    old = &cache->view;
    groups = (request->count + 255) / 256;
    sizes[REPAIR_GPU_SOURCE] = sizes[REPAIR_GPU_HISTORY] =
        view->count * 3 * sizeof(float);
    sizes[REPAIR_GPU_DOMAIN] = sizes[REPAIR_GPU_VALID] = view->count;
    sizes[REPAIR_GPU_POSITIONS] = request->count * sizeof(uint64_t);
    sizes[REPAIR_GPU_ERRORS] = request->count * sizeof(float);
    sizes[REPAIR_GPU_FLAGS] = request->count * sizeof(uint32_t);
    sizes[REPAIR_GPU_GROUPS] = groups *
        sizeof(sixel_repair_gpu_summary_t);
    sizes[REPAIR_GPU_SUMMARY] = sizeof(sixel_repair_gpu_summary_t);
    sizes[REPAIR_GPU_SAMPLES] = sizeof(result->samples);
    sizes[REPAIR_GPU_FILTER] = request->count;
    for (i = 0; i < REPAIR_GPU_SCAN_BUFFERS; ++i) {
        if (!sixel_repair_vulkan_buffer(runtime, cache, i, sizes[i])) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }
    for (i = 0; i < REPAIR_GPU_BUFFERS; ++i) {
        if (cache->buffers[i].capacity >
                SIZE_MAX - result->resident_bytes) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        result->resident_bytes += cache->buffers[i].capacity;
    }
    changed = !cache->bound || old->count != view->count ||
        old->width != view->width || old->height != view->height ||
        old->plane_width != view->plane_width ||
        old->plane_height != view->plane_height ||
        old->origin_x != view->origin_x ||
        old->origin_y != view->origin_y ||
        old->workspace != view->workspace ||
        old->configuration_revision != view->configuration_revision;
    if (changed || old->source_generation != view->source_generation ||
            old->source != view->source || old->domain != view->domain) {
        sixel_repair_vulkan_upload(cache, REPAIR_GPU_SOURCE, view->source,
            sizes[REPAIR_GPU_SOURCE], result);
        sixel_repair_vulkan_upload(cache, REPAIR_GPU_DOMAIN, view->domain,
            sizes[REPAIR_GPU_DOMAIN], result);
    }
    if (changed || old->history_revision != view->history_revision ||
            old->retained != view->retained || old->valid != view->valid) {
        sixel_repair_vulkan_upload(cache, REPAIR_GPU_HISTORY,
            view->retained, sizes[REPAIR_GPU_HISTORY], result);
        sixel_repair_vulkan_upload(cache, REPAIR_GPU_VALID, view->valid,
            sizes[REPAIR_GPU_VALID], result);
    }
    if (request->positions != NULL) {
        sixel_repair_vulkan_upload(cache, REPAIR_GPU_POSITIONS,
            request->positions, sizes[REPAIR_GPU_POSITIONS], result);
    }
    if (request->filter != NULL) {
        sixel_repair_vulkan_upload(cache, REPAIR_GPU_FILTER,
            request->filter, sizes[REPAIR_GPU_FILTER], result);
    }
    memset(&params, 0, sizeof(params));
    params.first = request->first;
    params.count = (uint32_t)request->count;
    params.sparse = request->positions != NULL;
    params.width = (uint32_t)view->width;
    params.plane_width = (uint32_t)view->plane_width;
    params.origin_x = (uint32_t)view->origin_x;
    params.origin_y = (uint32_t)view->origin_y;
    params.stop = request->stop_squared > FLT_MAX ? INFINITY
        : (float)request->stop_squared;
    params.threshold = request->selection_squared > FLT_MAX ? INFINITY
        : (float)request->selection_squared;
    params.sample_limit = request->sample_limit;
    params.filtered = request->filter != NULL;
    params.preselected = request->preselected;
    params.sample_population = request->sample_population;
    params.sample_offset = request->sample_offset;
    params.sample_total = request->sample_total;
    params.sample_first = request->sample_first;
    status = sixel_vk_buffer_reserve(runtime, &cache->params,
        sizeof(params), 1);
    if (status != SIXEL_OK) goto end;
    memcpy(cache->params.mapped, &params, sizeof(params));
    for (i = 0; i < REPAIR_GPU_SCAN_BUFFERS; ++i)
        bound[i] = &cache->buffers[i];
    bound[11] = &cache->params;
    for (stage = 0; stage < 3; ++stage) {
        status = sixel_vk_compute_bind(runtime,
            &repair_scan_stages[stage], bound);
        if (status != SIXEL_OK) goto end;
    }
    status = sixel_vk_command_begin(runtime, &command);
    if (status != SIXEL_OK) goto end;
    for (i = 0; i < REPAIR_GPU_SCAN_BUFFERS; ++i)
        sixel_vk_buffer_record_upload(command, &cache->buffers[i],
            cache->upload[i]);
    sixel_vk_buffer_record_upload(command, &cache->params, sizeof(params));
    for (stage = 0; stage < 3; ++stage) {
        sixel_vk_compute_barrier(command);
        sixel_vk_compute_dispatch(command, &repair_scan_stages[stage],
            stage == 0 ? (uint32_t)groups : 1);
    }
    sixel_vk_compute_download_barrier(command);
    sixel_vk_buffer_record_download(command,
        &cache->buffers[REPAIR_GPU_SUMMARY], sizeof(result->summary));
    status = sixel_vk_submit_and_wait(runtime, command);
    if (status != SIXEL_OK) goto end;
    memcpy(&result->summary, cache->buffers[REPAIR_GPU_SUMMARY].mapped,
        sizeof(result->summary));
    if (result->summary.nonfinite) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (result->summary.sample_count > request->sample_limit ||
            result->summary.eligible_count > request->count) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }
    bytes = result->summary.sample_count * sizeof(result->samples[0]);
    /* Only selected samples cross a discrete device's staging boundary.
     * Keep the owner's operation gate through both completed submissions. */
    if (bytes != 0 && cache->buffers[REPAIR_GPU_SAMPLES].staging !=
            VK_NULL_HANDLE) {
        status = sixel_vk_command_begin(runtime, &command);
        if (status != SIXEL_OK) goto end;
        sixel_vk_compute_download_barrier(command);
        sixel_vk_buffer_record_download(command,
            &cache->buffers[REPAIR_GPU_SAMPLES], bytes);
        status = sixel_vk_submit_and_wait(runtime, command);
        if (status != SIXEL_OK) goto end;
    }
    memcpy(result->samples, cache->buffers[REPAIR_GPU_SAMPLES].mapped, bytes);
    result->downloaded_bytes = sizeof(result->summary) + bytes;
    cache->view = *view;
    cache->bound = 1;
    status = SIXEL_OK;
end:
    sixel_vk_unlock();
    return status;
}

SIXELSTATUS
sixel_repair_gpu_vulkan_lookup(void **backend,
    sixel_repair_gpu_lookup_t const *request,
    sixel_repair_gpu_match_t *matches)
{
    sixel_repair_vulkan_cache_t *cache;
    sixel_repair_gpu_match_t const *result;
    sixel_vk_runtime_t *runtime;
    sixel_vk_buffer_t const *bound[4];
    VkCommandBuffer command;
    uint32_t params[2];
    size_t sizes[3];
    size_t i;
    SIXELSTATUS status;

    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = SIXEL_FEATURE_ERROR;
    if (runtime == NULL) goto end;
    status = sixel_vk_compute_create(runtime, &repair_lookup_stage,
        repair_lookup_source,
        sizeof(repair_lookup_source) / sizeof(repair_lookup_source[0]),
        "repair_lookup", 4);
    if (status != SIXEL_OK) goto end;
    cache = *backend;
    if (cache == NULL) {
        cache = calloc(1, sizeof(*cache));
        if (cache == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        *backend = cache;
    }
    sizes[0] = request->count * 3 * sizeof(float);
    sizes[1] = request->colors * 3 * sizeof(float);
    sizes[2] = request->count * sizeof(*matches);
    for (i = 0; i < 3; ++i) {
        if (!sixel_repair_vulkan_buffer(runtime, cache,
                REPAIR_GPU_QUERIES + (unsigned int)i, sizes[i])) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        bound[i] = &cache->buffers[REPAIR_GPU_QUERIES + i];
    }
    status = sixel_vk_buffer_reserve(runtime, &cache->params,
        sizeof(params), 1);
    if (status != SIXEL_OK) goto end;
    memcpy(cache->buffers[REPAIR_GPU_QUERIES].mapped, request->queries,
        sizes[0]);
    memcpy(cache->buffers[REPAIR_GPU_PALETTE].mapped, request->palette,
        sizes[1]);
    params[0] = (uint32_t)request->count;
    params[1] = request->colors;
    memcpy(cache->params.mapped, params, sizeof(params));
    bound[3] = &cache->params;
    status = sixel_vk_compute_bind(runtime, &repair_lookup_stage, bound);
    if (status != SIXEL_OK) goto end;
    status = sixel_vk_command_begin(runtime, &command);
    if (status != SIXEL_OK) goto end;
    for (i = 0; i < 2; ++i)
        sixel_vk_buffer_record_upload(command, bound[i], sizes[i]);
    sixel_vk_buffer_record_upload(command, bound[3], sizeof(params));
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &repair_lookup_stage,
        ((uint32_t)request->count + 255u) / 256u);
    sixel_vk_compute_download_barrier(command);
    sixel_vk_buffer_record_download(command, bound[2], sizes[2]);
    status = sixel_vk_submit_and_wait(runtime, command);
    if (status != SIXEL_OK) goto end;
    result = cache->buffers[REPAIR_GPU_MATCHES].mapped;
    for (i = 0; i < request->count; ++i) {
        if (result[i].index >= request->colors ||
                !isfinite(result[i].distance) || result[i].distance < 0) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
    }
    /* Leave caller-owned matches untouched if any proposal is invalid. */
    memcpy(matches, result, sizes[2]);
end:
    sixel_vk_unlock();
    return status;
}
#endif
