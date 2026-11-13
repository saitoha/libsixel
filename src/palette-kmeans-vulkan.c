/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN)
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "palette-kmeans-gpu.h"
#include "gpu-compute-vulkan.h"
#include "sixel_atomic.h"

static char const *const kmeans_source[] = {
#include "generated-vulkan/palette-kmeans-gpu-shader.inc.h"
};
/* Pipelines are immutable after publication; attempt buffers never escape
 * the runtime lock or become visible before complete validation. */
static sixel_vk_compute_t kmeans_stages[4];
static sixel_atomic_i32_t kmeans_initialized;

enum {
    SIXEL_KM_POINTS,
    SIXEL_KM_WEIGHTS,
    SIXEL_KM_CENTERS,
    SIXEL_KM_MEMBERSHIP,
    SIXEL_KM_DISTANCES,
    SIXEL_KM_PARTIALS,
    SIXEL_KM_SUMS,
    SIXEL_KM_STATE,
    SIXEL_KM_MAXIMA,
    SIXEL_KM_BUFFERS
};

typedef struct sixel_km_vulkan_params {
    uint32_t points;
    uint32_t centers;
    uint32_t groups;
    uint32_t min_iterations;
    uint32_t round_centers;
    float threshold;
} sixel_km_vulkan_params_t;

typedef struct sixel_km_vulkan_state {
    uint32_t done;
    uint32_t iterations;
    uint32_t failed;
    float delta;
    uint32_t repairs;
} sixel_km_vulkan_state_t;


int
sixel_kmeans_vulkan_is_available(void)
{
    int available;

    sixel_vk_lock();
    available = sixel_vk_runtime_get_locked() != NULL;
    sixel_vk_unlock();
    return available;
}

int
sixel_kmeans_vulkan_is_initialized(void)
{
    /* Readiness never waits for another component's queue work. Pipelines
     * are subsequently accessed under the runtime lock, which publishes
     * their contents; this atomic flag only controls AUTO admission. */
    return sixel_atomic_load_relaxed_i32(&kmeans_initialized);
}

SIXELSTATUS
sixel_kmeans_vulkan_refine(sixel_kmeans_gpu_request_t const *request,
    unsigned int *iterations)
{
    static char const *const entries[4] = {
        "km_assign", "km_partial", "km_reduce", "km_update"
    };
    sixel_vk_runtime_t *runtime;
    sixel_km_vulkan_params_t params;
    sixel_km_vulkan_state_t *state;
    sixel_vk_buffer_t buffers[SIXEL_KM_BUFFERS + 1];
    sixel_vk_buffer_t const *bound[SIXEL_KM_BUFFERS + 1];
    size_t lengths[SIXEL_KM_BUFFERS + 1];
    VkCommandBuffer command;
    uint32_t groups[4];
    unsigned int stage;
    unsigned int iteration;
    unsigned int channel;
    size_t index;
    float *values;
    double minimum[3], maximum[3], value, margin;
    SIXELSTATUS status;

    if (request->test_failure == 1) return SIXEL_FEATURE_ERROR;
    memset(buffers, 0, sizeof(buffers));
    memset(&params, 0, sizeof(params));
    params.points = request->point_count;
    params.centers = request->center_count;
    params.groups = (params.points + 511u) / 512u;
    params.min_iterations = request->min_iterations;
    params.round_centers = request->round_centers != 0;
    params.threshold = (float)request->threshold;
    lengths[SIXEL_KM_POINTS] = (size_t)params.points * 3 * sizeof(float);
    lengths[SIXEL_KM_WEIGHTS] = (size_t)params.points * sizeof(float);
    lengths[SIXEL_KM_CENTERS] = (size_t)params.centers * 3 * sizeof(float);
    lengths[SIXEL_KM_MEMBERSHIP] = (size_t)params.points * sizeof(uint32_t);
    lengths[SIXEL_KM_DISTANCES] = lengths[SIXEL_KM_WEIGHTS];
    lengths[SIXEL_KM_PARTIALS] =
        (size_t)params.groups * params.centers * 4 * sizeof(float);
    lengths[SIXEL_KM_SUMS] = (size_t)params.centers * 4 * sizeof(float);
    lengths[SIXEL_KM_STATE] = sizeof(*state);
    lengths[SIXEL_KM_MAXIMA] = (size_t)params.groups * sizeof(uint32_t);
    lengths[SIXEL_KM_BUFFERS] = sizeof(params);
    groups[0] = (params.points + 255u) / 256u;
    groups[1] = params.groups;
    groups[2] = (params.centers + 255u) / 256u;
    groups[3] = 1;
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = SIXEL_FEATURE_ERROR;
    if (runtime == NULL) goto cleanup;
    for (stage = 0; stage < 4; ++stage) {
        status = sixel_vk_compute_create(runtime, &kmeans_stages[stage],
            kmeans_source, sizeof(kmeans_source) / sizeof(kmeans_source[0]),
            entries[stage], SIXEL_KM_BUFFERS + 1);
        if (status != SIXEL_OK) goto cleanup;
    }
    sixel_atomic_store_relaxed_i32(&kmeans_initialized, 1);
    for (index = 0; index <= SIXEL_KM_BUFFERS; ++index) {
        status = sixel_vk_buffer_reserve(runtime, &buffers[index],
            lengths[index], 0);
        if (status != SIXEL_OK) goto cleanup;
        bound[index] = &buffers[index];
    }
    if (request->test_failure == 2) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    values = buffers[SIXEL_KM_POINTS].mapped;
    for (index = 0; index < (size_t)params.points * 3; ++index)
        values[index] = (float)request->points[index];
    values = buffers[SIXEL_KM_WEIGHTS].mapped;
    for (index = 0; index < params.points; ++index)
        values[index] = request->weights == NULL ? 1.0f
            : (float)request->weights[index];
    values = buffers[SIXEL_KM_CENTERS].mapped;
    for (index = 0; index < (size_t)params.centers * 3; ++index)
        values[index] = (float)request->centers[index];
    state = buffers[SIXEL_KM_STATE].mapped;
    memset(state, 0, sizeof(*state));
    memcpy(buffers[SIXEL_KM_BUFFERS].mapped, &params, sizeof(params));
    for (stage = 0; stage < 4; ++stage) {
        status = sixel_vk_compute_bind(runtime, &kmeans_stages[stage], bound);
        if (status != SIXEL_OK) goto cleanup;
    }
    status = sixel_vk_command_begin(runtime, &command);
    if (status != SIXEL_OK) goto cleanup;
    for (index = 0; index <= SIXEL_KM_BUFFERS; ++index) {
        if (index <= SIXEL_KM_CENTERS || index == SIXEL_KM_STATE ||
                index == SIXEL_KM_BUFFERS)
            sixel_vk_buffer_record_upload(command, &buffers[index],
                lengths[index]);
    }
    /* No host readback occurs between Lloyd iterations. The state buffer
     * makes remaining dispatches no-ops after convergence or rejection. */
    for (iteration = 0; iteration < request->max_iterations; ++iteration) {
        for (stage = 0; stage < 4; ++stage) {
            sixel_vk_compute_barrier(command);
            sixel_vk_compute_dispatch(command, &kmeans_stages[stage],
                groups[stage]);
        }
    }
    sixel_vk_compute_download_barrier(command);
    sixel_vk_buffer_record_download(command, &buffers[SIXEL_KM_STATE],
        lengths[SIXEL_KM_STATE]);
    sixel_vk_buffer_record_download(command, &buffers[SIXEL_KM_CENTERS],
        lengths[SIXEL_KM_CENTERS]);
    if (request->membership != NULL)
        sixel_vk_buffer_record_download(command,
            &buffers[SIXEL_KM_MEMBERSHIP], lengths[SIXEL_KM_MEMBERSHIP]);
    status = sixel_vk_submit_and_wait(runtime, command);
    if (status != SIXEL_OK) goto cleanup;
    status = SIXEL_RUNTIME_ERROR;
    if (request->test_failure == 3 || state->failed ||
            state->iterations == 0 ||
            state->iterations > request->max_iterations) goto cleanup;
    /* Means must remain in the convex coordinate envelope. Empty
     * centers may retain their seeds; byte rounding expands by 0.5.
     * This also catches unstable cancellation during donor repair. */
    for (channel = 0u; channel < 3u; ++channel) {
        minimum[channel] = maximum[channel] = request->points[channel];
    }
    for (index = 0u; index < (size_t)params.points * 3u; ++index) {
        channel = (unsigned int)(index % 3u);
        value = request->points[index];
        minimum[channel] = fmin(minimum[channel], value);
        maximum[channel] = fmax(maximum[channel], value);
    }
    for (index = 0u; index < (size_t)params.centers * 3u; ++index) {
        channel = (unsigned int)(index % 3u);
        value = request->centers[index];
        minimum[channel] = fmin(minimum[channel], value);
        maximum[channel] = fmax(maximum[channel], value);
    }
    values = buffers[SIXEL_KM_CENTERS].mapped;
    for (index = 0u; index < (size_t)params.centers * 3u; ++index) {
        channel = (unsigned int)(index % 3u);
        margin =
            fmax(maximum[channel] - minimum[channel],
                 fmax(fabs(minimum[channel]), fabs(maximum[channel]))) *
            0.0001;
        if (request->round_centers)
            margin += 0.5;
        if (!isfinite(values[index]) ||
            values[index] < minimum[channel] - margin ||
            values[index] > maximum[channel] + margin) {
            goto cleanup;
        }
    }
    /* Publish only after every center has passed the numeric checks. */
    for (index = 0; index < (size_t)params.centers * 3; ++index)
        request->centers[index] = (double)values[index];
    if (request->membership != NULL)
        memcpy(request->membership, buffers[SIXEL_KM_MEMBERSHIP].mapped,
            lengths[SIXEL_KM_MEMBERSHIP]);
    *iterations = state->iterations;
    status = SIXEL_OK;
cleanup:
    if (runtime != NULL) {
        for (index = 0; index <= SIXEL_KM_BUFFERS; ++index)
            sixel_vk_buffer_release(runtime, &buffers[index]);
    }
    sixel_vk_unlock();
    return status;
}
#else
/* Meson compiles this file when Vulkan support is disabled. */
typedef int palette_kmeans_vulkan_disabled;
#endif
