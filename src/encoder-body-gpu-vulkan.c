/* SPDX-License-Identifier: MIT */
/* Vulkan backend for encoder-body-gpu.h; the same two passes as
 * encoder-body-gpu-metal.m. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "encoder-body-gpu.h"
#include "gpu-compute-vulkan.h"
#include "gpu-palette-vulkan.h"

static char const *const body_source[] = {
#include "generated-vulkan/encoder-body-gpu-shader.inc.h"
};
static sixel_vk_compute_t body_classify_stage;
static sixel_vk_compute_t body_spans_stage;

typedef struct sixel_body_gpu_params {
    uint32_t width, height, encoded_width, encoded_height;
    uint32_t offset_left, offset_top, ncolors;
    int32_t keycolor;
    uint32_t words, bands, has_mask, write_pass;
} sixel_body_gpu_params_t;

/* Binding order of encoder-body-gpu.glsl. */
enum {
    BODY_OCCUPANCY, BODY_COUNTS, BODY_WIDTHS, BODY_NODE_FIRST,
    BODY_MAP_FIRST, BODY_NODES, BODY_MAP, BODY_INDEXES, BODY_MASK,
    BODY_PARAMS, BODY_FIRST_SEEN, BODY_BINDINGS
};

struct sixel_body_gpu {
    sixel_vk_buffer_t buffers[BODY_BINDINGS];
    uint32_t *band_first;
    size_t band_first_count;
};

/* The host fills these and the shaders only read them. */
static int
sixel_body_gpu_host_written(int slot)
{
    return slot == BODY_INDEXES || slot == BODY_MASK ||
        slot == BODY_PARAMS || slot == BODY_NODE_FIRST ||
        slot == BODY_MAP_FIRST;
}

/* Grow by half again so slowly growing frames do not reallocate each
 * time. Occupancy never leaves the device. */
static SIXELSTATUS
sixel_body_gpu_reserve(sixel_vk_runtime_t *runtime, sixel_body_gpu_t *context,
    int slot, size_t length)
{
    sixel_vk_buffer_t *buffer;

    buffer = &context->buffers[slot];
    if (length == 0) length = 16;
    if (buffer->buffer != VK_NULL_HANDLE && buffer->capacity >= length)
        return SIXEL_OK;
    length += length / 2;
    if (slot == BODY_OCCUPANCY)
        return sixel_vk_buffer_reserve_device(runtime, buffer, length);
    if (sixel_body_gpu_host_written(slot))
        return sixel_vk_buffer_reserve(runtime, buffer, length, 1);
    /* Emission reads nodes and maps in place, so they need cached memory. */
    return sixel_vk_buffer_reserve_readback(runtime, buffer, length);
}

SIXELSTATUS
sixel_body_gpu_new(sixel_body_gpu_t **result)
{
    sixel_body_gpu_t *context;
    sixel_vk_runtime_t *runtime;
    SIXELSTATUS status;

    *result = NULL;
    context = (sixel_body_gpu_t *)calloc(1, sizeof(*context));
    if (context == NULL) return SIXEL_BAD_ALLOCATION;
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = runtime == NULL ? SIXEL_FEATURE_ERROR :
        sixel_vk_compute_create(runtime, &body_classify_stage, body_source,
            sizeof(body_source) / sizeof(body_source[0]), "body_classify",
            BODY_BINDINGS);
    if (status == SIXEL_OK)
        status = sixel_vk_compute_create(runtime, &body_spans_stage,
            body_source, sizeof(body_source) / sizeof(body_source[0]),
            "body_spans", BODY_BINDINGS);
    sixel_vk_unlock();
    if (SIXEL_FAILED(status)) {
        free(context);
        return status;
    }
    *result = context;
    return SIXEL_OK;
}

void
sixel_body_gpu_destroy(sixel_body_gpu_t *context)
{
    sixel_vk_runtime_t *runtime;
    int i;

    if (context == NULL) return;
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    for (i = 0; i < BODY_BINDINGS; ++i)
        sixel_vk_buffer_release(runtime, &context->buffers[i]);
    sixel_vk_unlock();
    free(context->band_first);
    free(context);
}

/* Both stages share the binding list; growth replaces buffers, so bind
 * again before every submission. */
static SIXELSTATUS
sixel_body_gpu_bind(sixel_vk_runtime_t *runtime, sixel_body_gpu_t *context)
{
    sixel_vk_buffer_t const *bound[BODY_BINDINGS];
    SIXELSTATUS status;
    int i;

    for (i = 0; i < BODY_BINDINGS; ++i) bound[i] = &context->buffers[i];
    status = sixel_vk_compute_bind(runtime, &body_classify_stage, bound);
    if (status != SIXEL_OK) return status;
    return sixel_vk_compute_bind(runtime, &body_spans_stage, bound);
}

static uint32_t
sixel_body_gpu_groups(size_t threads)
{
    return (uint32_t)((threads + 255U) / 256U);
}

static SIXELSTATUS
sixel_body_gpu_run_locked(sixel_vk_runtime_t *runtime,
    sixel_body_gpu_t *context, sixel_body_gpu_request_t const *request,
    sixel_body_gpu_params_t *params, sixel_body_gpu_result_t *result)
{
    sixel_vk_buffer_t *buffers;
    sixel_vk_buffer_t const *frame_indexes;
    sixel_vk_buffer_t const *frame_mask;
    VkCommandBuffer command;
    size_t pixels;
    size_t pairs;
    size_t occupancy_bytes;
    size_t t;
    size_t b;
    uint64_t nodes_total;
    uint64_t map_total;
    uint32_t const *counts;
    uint32_t const *widths;
    uint32_t *node_first;
    uint32_t *map_first;
    SIXELSTATUS status;

    buffers = context->buffers;
    pixels = (size_t)params->width * params->height;
    pairs = (size_t)params->bands * params->ncolors;
    occupancy_bytes = pairs * params->words * 4U;
    status = sixel_body_gpu_reserve(runtime, context, BODY_OCCUPANCY,
        occupancy_bytes);
    if (status == SIXEL_OK)
        status = sixel_body_gpu_reserve(runtime, context, BODY_INDEXES,
            pixels);
    if (status == SIXEL_OK)
        status = sixel_body_gpu_reserve(runtime, context, BODY_MASK,
            request->mask != NULL ? pixels : 16);
    if (status == SIXEL_OK)
        status = sixel_body_gpu_reserve(runtime, context, BODY_PARAMS,
            sizeof(*params));
    for (t = BODY_COUNTS; t <= BODY_MAP_FIRST && status == SIXEL_OK; ++t)
        status = sixel_body_gpu_reserve(runtime, context, (int)t, pairs * 4U);
    if (status == SIXEL_OK)
        status = sixel_body_gpu_reserve(runtime, context, BODY_FIRST_SEEN,
            pairs * 4U);
    if (status == SIXEL_OK)
        status = sixel_body_gpu_reserve(runtime, context, BODY_NODES, 16);
    if (status == SIXEL_OK)
        status = sixel_body_gpu_reserve(runtime, context, BODY_MAP, 16);
    if (status != SIXEL_OK) return status;

    /* Shared frame planes reach device storage by transfer copies. */
    frame_indexes = sixel_gpu_vulkan_frame_buffer(request->indexes_buffer,
        request->indexes, pixels);
    frame_mask = request->mask != NULL ? sixel_gpu_vulkan_frame_buffer(
        request->mask_buffer, request->mask, pixels) : NULL;
    if (frame_indexes == NULL)
        memcpy(buffers[BODY_INDEXES].mapped, request->indexes, pixels);
    if (request->mask != NULL && frame_mask == NULL)
        memcpy(buffers[BODY_MASK].mapped, request->mask, pixels);
    params->write_pass = 0;
    memcpy(buffers[BODY_PARAMS].mapped, params, sizeof(*params));
    status = sixel_body_gpu_bind(runtime, context);
    if (status != SIXEL_OK) return status;
    status = sixel_vk_command_begin(runtime, &command);
    if (status != SIXEL_OK) return status;
    if (frame_indexes != NULL)
        sixel_vk_record_copy(command, frame_indexes, &buffers[BODY_INDEXES],
            0, pixels);
    else
        sixel_vk_buffer_record_upload(command, &buffers[BODY_INDEXES],
            pixels);
    if (frame_mask != NULL)
        sixel_vk_record_copy(command, frame_mask, &buffers[BODY_MASK], 0,
            pixels);
    else if (request->mask != NULL)
        sixel_vk_buffer_record_upload(command, &buffers[BODY_MASK], pixels);
    sixel_vk_buffer_record_upload(command, &buffers[BODY_PARAMS],
        sizeof(*params));
    vkCmdFillBuffer(command, buffers[BODY_OCCUPANCY].buffer, 0,
        occupancy_bytes, 0);
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &body_classify_stage,
        sixel_body_gpu_groups(pixels));
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &body_spans_stage,
        sixel_body_gpu_groups(pairs));
    sixel_vk_compute_download_barrier(command);
    sixel_vk_buffer_record_download(command, &buffers[BODY_COUNTS],
        pairs * 4U);
    sixel_vk_buffer_record_download(command, &buffers[BODY_WIDTHS],
        pairs * 4U);
    status = sixel_vk_submit_and_wait(runtime, command);
    if (status == SIXEL_OK)
        status = sixel_vk_buffer_invalidate(runtime, &buffers[BODY_COUNTS],
            pairs * 4U);
    if (status == SIXEL_OK)
        status = sixel_vk_buffer_invalidate(runtime, &buffers[BODY_WIDTHS],
            pairs * 4U);
    if (status != SIXEL_OK) return status;

    /* Prefix offsets; pairs are band-major, so each band's nodes are
     * contiguous and register-ordered. */
    if (context->band_first_count < (size_t)params->bands + 1U) {
        free(context->band_first);
        context->band_first = (uint32_t *)malloc(
            ((size_t)params->bands + 1U) * sizeof(uint32_t));
        if (context->band_first == NULL) {
            context->band_first_count = 0;
            return SIXEL_BAD_ALLOCATION;
        }
        context->band_first_count = (size_t)params->bands + 1U;
    }
    counts = (uint32_t const *)buffers[BODY_COUNTS].mapped;
    widths = (uint32_t const *)buffers[BODY_WIDTHS].mapped;
    node_first = (uint32_t *)buffers[BODY_NODE_FIRST].mapped;
    map_first = (uint32_t *)buffers[BODY_MAP_FIRST].mapped;
    nodes_total = 0;
    map_total = 0;
    for (b = 0; b < params->bands; ++b) {
        context->band_first[b] = (uint32_t)nodes_total;
        for (t = b * params->ncolors; t < (b + 1U) * params->ncolors; ++t) {
            node_first[t] = (uint32_t)nodes_total;
            map_first[t] = (uint32_t)map_total;
            nodes_total += counts[t];
            map_total += widths[t];
        }
        if (nodes_total > UINT32_MAX || map_total > UINT32_MAX)
            return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    context->band_first[params->bands] = (uint32_t)nodes_total;
    status = sixel_body_gpu_reserve(runtime, context, BODY_NODES,
        (size_t)nodes_total * sizeof(sixel_body_gpu_node_t));
    if (status == SIXEL_OK)
        status = sixel_body_gpu_reserve(runtime, context, BODY_MAP,
            (size_t)map_total + SIXEL_BODY_MAP_TAIL);
    if (status != SIXEL_OK) return status;

    params->write_pass = 1;
    memcpy(buffers[BODY_PARAMS].mapped, params, sizeof(*params));
    status = sixel_body_gpu_bind(runtime, context);
    if (status != SIXEL_OK) return status;
    status = sixel_vk_command_begin(runtime, &command);
    if (status != SIXEL_OK) return status;
    sixel_vk_buffer_record_upload(command, &buffers[BODY_NODE_FIRST],
        pairs * 4U);
    sixel_vk_buffer_record_upload(command, &buffers[BODY_MAP_FIRST],
        pairs * 4U);
    sixel_vk_buffer_record_upload(command, &buffers[BODY_PARAMS],
        sizeof(*params));
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &body_spans_stage,
        sixel_body_gpu_groups(pairs));
    sixel_vk_compute_download_barrier(command);
    if (nodes_total != 0)
        sixel_vk_buffer_record_download(command, &buffers[BODY_NODES],
            (size_t)nodes_total * sizeof(sixel_body_gpu_node_t));
    if (map_total != 0)
        sixel_vk_buffer_record_download(command, &buffers[BODY_MAP],
            (size_t)map_total);
    sixel_vk_buffer_record_download(command, &buffers[BODY_FIRST_SEEN],
        pairs * 4U);
    status = sixel_vk_submit_and_wait(runtime, command);
    if (status == SIXEL_OK)
        status = sixel_vk_buffer_invalidate(runtime, &buffers[BODY_NODES],
            (size_t)nodes_total * sizeof(sixel_body_gpu_node_t));
    if (status == SIXEL_OK)
        status = sixel_vk_buffer_invalidate(runtime, &buffers[BODY_MAP],
            (size_t)map_total);
    if (status == SIXEL_OK)
        status = sixel_vk_buffer_invalidate(runtime,
            &buffers[BODY_FIRST_SEEN], pairs * 4U);
    if (status != SIXEL_OK) return status;

    result->nodes = (sixel_body_gpu_node_t const *)buffers[BODY_NODES].mapped;
    result->map = (unsigned char *)buffers[BODY_MAP].mapped;
    result->band_first = context->band_first;
    result->first_seen = (uint32_t const *)buffers[BODY_FIRST_SEEN].mapped;
    result->ncolors = (int)params->ncolors;
    result->band_count = (int)params->bands;
    result->bytes = NULL;
    result->band_bytes = NULL;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_body_gpu_run(sixel_body_gpu_t *context,
                   sixel_body_gpu_request_t const *request,
                   sixel_body_gpu_result_t *result)
{
    sixel_body_gpu_params_t params;
    sixel_vk_runtime_t *runtime;
    size_t pairs;
    SIXELSTATUS status;

    if (context == NULL || request == NULL || result == NULL ||
            request->indexes == NULL || request->width <= 0 ||
            request->height <= 0 || request->ncolors <= 0 ||
            request->ncolors > 256 || request->offset_left < 0 ||
            request->offset_top < 0 ||
            request->encoded_width < request->width + request->offset_left ||
            request->encoded_height <
                request->height + request->offset_top) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(&params, 0, sizeof(params));
    params.width = (uint32_t)request->width;
    params.height = (uint32_t)request->height;
    params.encoded_width = (uint32_t)request->encoded_width;
    params.encoded_height = (uint32_t)request->encoded_height;
    params.offset_left = (uint32_t)request->offset_left;
    params.offset_top = (uint32_t)request->offset_top;
    params.ncolors = (uint32_t)request->ncolors;
    params.keycolor = request->keycolor;
    params.words = (params.encoded_width + 31U) / 32U;
    params.bands = (params.encoded_height + 5U) / 6U;
    params.has_mask = request->mask != NULL;
    pairs = (size_t)params.bands * params.ncolors;
    /* Shader indexing is 32-bit: pixels, pairs and occupancy words. */
    if ((size_t)params.width * params.height > UINT32_MAX ||
            pairs > UINT32_MAX ||
            pairs * params.words > UINT32_MAX / 4U) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    status = runtime == NULL ? SIXEL_FEATURE_ERROR :
        sixel_body_gpu_run_locked(runtime, context, request, &params, result);
    sixel_vk_unlock();
    return status;
}
#endif
