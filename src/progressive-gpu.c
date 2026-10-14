/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <limits.h>
#include <math.h>
#include <string.h>
#include "progressive-gpu.h"

struct sixel_repair_gpu {
    sixel_allocator_t *allocator;
    void *backend;
};

#if defined(HAVE_METAL)
SIXELSTATUS
sixel_repair_gpu_metal_scan(void **backend,
    sixel_repair_gpu_request_t const *request,
    sixel_repair_gpu_result_t *result);
SIXELSTATUS
sixel_repair_gpu_metal_lookup(void **backend,
    sixel_repair_gpu_lookup_t const *request,
    sixel_repair_gpu_match_t *matches);
void
sixel_repair_gpu_metal_free(void *backend);
#endif

SIXELSTATUS
sixel_repair_gpu_new(sixel_repair_gpu_t **cache,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;

    if (cache == NULL) return SIXEL_BAD_ARGUMENT;
    *cache = NULL;
    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (status != SIXEL_OK) return status;
    } else {
        sixel_allocator_ref(allocator);
    }
    *cache = sixel_allocator_calloc(allocator, 1, sizeof(**cache));
    if (*cache == NULL) {
        sixel_allocator_unref(allocator);
        return SIXEL_BAD_ALLOCATION;
    }
    (*cache)->allocator = allocator;
    return SIXEL_OK;
}

void
sixel_repair_gpu_evict(sixel_repair_gpu_t *cache)
{
    if (cache == NULL) return;
#if defined(HAVE_METAL)
    sixel_repair_gpu_metal_free(cache->backend);
#endif
    cache->backend = NULL;
}

void
sixel_repair_gpu_free(sixel_repair_gpu_t *cache)
{
    sixel_allocator_t *allocator;

    if (cache == NULL) return;
    allocator = cache->allocator;
    sixel_repair_gpu_evict(cache);
    sixel_allocator_free(allocator, cache);
    sixel_allocator_unref(allocator);
}

static int
sixel_repair_gpu_request_valid(sixel_repair_gpu_request_t const *request)
{
    sixel_repair_gpu_view_t const *view;
    size_t i;

    if (request == NULL) return 0;
    view = &request->view;
    if (request->policy < SIXEL_GPU_POLICY_OFF ||
            request->policy > SIXEL_GPU_POLICY_FORCE ||
            view->source == NULL || view->retained == NULL ||
            view->domain == NULL || view->valid == NULL ||
            view->width <= 0 || view->height <= 0 ||
            view->plane_width <= 0 || view->plane_height <= 0 ||
            view->width > view->plane_width ||
            view->height > view->plane_height ||
            view->origin_x < 0 || view->origin_y < 0 ||
            view->origin_x > view->plane_width - view->width ||
            view->origin_y > view->plane_height - view->height ||
            (size_t)view->width > SIZE_MAX / (size_t)view->height ||
            view->count != (size_t)view->width * view->height ||
            view->count > SIZE_MAX / (6 * sizeof(float) + 2) ||
            request->count == 0 ||
            request->count > SIXEL_REPAIR_GPU_BATCH_MAX ||
            request->sample_limit > SIXEL_REPAIR_GPU_SAMPLE_MAX ||
            !isfinite(request->stop_squared) || request->stop_squared < 0 ||
            !isfinite(request->selection_squared) ||
            request->selection_squared < 0 ||
            (request->preselected != 0 && request->preselected != 1) ||
            (request->preselected && request->filter == NULL)) {
        return 0;
    }
    if (request->sample_total != 0 &&
            (!request->preselected || request->sample_total > 65536 ||
             request->sample_total > request->sample_population ||
             request->sample_population > view->count ||
             request->sample_offset > request->sample_population ||
             request->sample_first > request->sample_total ||
             request->count > request->sample_limit)) return 0;
    if (request->positions == NULL) {
        return request->first <= view->count &&
            request->count <= view->count - request->first;
    }
    if (request->first != 0) return 0;
    for (i = 0; i < request->count; ++i) {
        if (request->positions[i] >= view->count ||
                (i && request->positions[i] <= request->positions[i - 1])) {
            return 0;
        }
    }
    return 1;
}

SIXELSTATUS
sixel_repair_gpu_scan(sixel_repair_gpu_t *cache,
    sixel_repair_gpu_request_t const *request,
    sixel_repair_gpu_result_t *result)
{
    SIXELSTATUS status;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    memset(result, 0, sizeof(*result));
    if (cache == NULL || !sixel_repair_gpu_request_valid(request)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->policy == SIXEL_GPU_POLICY_OFF ||
            (request->policy == SIXEL_GPU_POLICY_AUTO &&
             request->count < request->auto_threshold)) {
        return SIXEL_FALSE;
    }
    status = SIXEL_FEATURE_ERROR;
#if defined(HAVE_METAL)
    status = sixel_repair_gpu_metal_scan(&cache->backend, request, result);
#endif
    if (status == SIXEL_OK) return status;
    /* A failed batch never publishes partial samples or reusable GPU state.
     * AUTO may continue on the CPU; FORCE preserves the backend error. */
    sixel_repair_gpu_evict(cache);
    memset(result, 0, sizeof(*result));
    return request->policy == SIXEL_GPU_POLICY_AUTO ? SIXEL_FALSE : status;
}

SIXELSTATUS
sixel_repair_gpu_lookup(sixel_repair_gpu_t *cache,
    sixel_repair_gpu_lookup_t const *request,
    sixel_repair_gpu_match_t *matches)
{
    SIXELSTATUS status;

    if (cache == NULL || request == NULL || matches == NULL ||
            request->queries == NULL || request->palette == NULL ||
            request->count == 0 ||
            request->count > SIXEL_REPAIR_GPU_BATCH_MAX ||
            request->colors == 0 || request->colors > 255 ||
            request->policy < SIXEL_GPU_POLICY_OFF ||
            request->policy > SIXEL_GPU_POLICY_FORCE) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->policy == SIXEL_GPU_POLICY_OFF) return SIXEL_FALSE;
    status = SIXEL_FEATURE_ERROR;
#if defined(HAVE_METAL)
    status = sixel_repair_gpu_metal_lookup(&cache->backend, request, matches);
#endif
    if (status == SIXEL_OK) return status;
    sixel_repair_gpu_evict(cache);
    return request->policy == SIXEL_GPU_POLICY_AUTO ? SIXEL_FALSE : status;
}
