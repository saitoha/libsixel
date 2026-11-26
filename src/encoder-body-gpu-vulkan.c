/* SPDX-License-Identifier: MIT */
/* Vulkan span composition and optional spelling for encoder-body-gpu.h. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "encoder-body-gpu.h"
#include "gpu-compute-vulkan.h"
#include "gpu-palette-vulkan.h"
#include "palette-resident-vulkan.h"

static char const *const body_source[] = {
#include "generated-vulkan/encoder-body-gpu-shader.inc.h"
};
static char const *const emit_source[] = {
#include "generated-vulkan/encoder-body-emit-gpu-shader.inc.h"
};
static char const *const copy_source[] = {
#include "generated-vulkan/encoder-body-copy-gpu-shader.inc.h"
};
enum {
    BR_CLASSIFY, BR_SPANS, BR_LOCAL, BR_PREFIX, BR_OFFSETS, BR_LENGTHS,
    BR_ORDER, BR_GROUP, BR_BASES, BR_WRITE, BR_COPY, BR_STAGE_COUNT
};

static sixel_vk_compute_t body_classify_stage;
static sixel_vk_compute_t body_spans_stage;
static sixel_vk_compute_t body_emit_stages[BR_STAGE_COUNT];
static sixel_vk_compute_t body_copy_stage;
static sixel_vk_submission_t body_submission =
    SIXEL_VK_SUBMISSION_INITIALIZER;
/* The runtime lock protects this stage history independently of sampling. */
static sixel_gpu_headroom_t body_headroom;

typedef struct sixel_body_gpu_params {
    uint32_t width, height, encoded_width, encoded_height;
    uint32_t offset_left, offset_top, ncolors;
    int32_t keycolor;
    uint32_t words, bands, has_mask, write_pass;
} sixel_body_gpu_params_t;

typedef struct sixel_body_gpu_emit_params {
    uint32_t width, words, ncolors, bands;
    uint32_t selectors, limited, nodes, group_limit;
} sixel_body_gpu_emit_params_t;

/* Keep Metal's column-bucket order in shared memory within Vulkan's
 * 48 KiB workgroup budget. Larger bands use its serial order stage. */
#define SIXEL_BODY_GROUP_NODES 2048U

/* Binding order of encoder-body-gpu.glsl. */
enum {
    BODY_OCCUPANCY, BODY_COUNTS, BODY_WIDTHS, BODY_NODE_FIRST,
    BODY_MAP_FIRST, BODY_NODES, BODY_MAP, BODY_INDEXES, BODY_MASK,
    BODY_PARAMS, BODY_FIRST_SEEN, BODY_BAND_FIRST, BODY_LENGTHS,
    BODY_REGISTERS, BODY_COLUMNS, BODY_BITS, BODY_SORTED, BODY_ENTRIES,
    BODY_BAND_SIZE, BODY_BAND_BYTES, BODY_BYTES, BODY_EMIT_PARAMS,
    BODY_BYTES_HOST,
    BODY_BINDINGS
};
#define BODY_SPAN_BINDINGS (BODY_FIRST_SEEN + 1)

/* Binding order of encoder-body-emit-gpu.glsl. */
static int const body_emit_slots[] = {
    BODY_NODES, BODY_MAP, BODY_FIRST_SEEN, BODY_BAND_FIRST,
    BODY_LENGTHS, BODY_REGISTERS, BODY_COLUMNS, BODY_BITS, BODY_SORTED,
    BODY_ENTRIES, BODY_BAND_SIZE, BODY_BAND_BYTES, BODY_BYTES,
    BODY_EMIT_PARAMS
};
#define BODY_EMIT_BINDINGS (sizeof(body_emit_slots) / \
                            sizeof(body_emit_slots[0]))

static int const body_copy_slots[] = {
    BODY_BYTES, BODY_BYTES_HOST, BODY_BAND_BYTES, BODY_EMIT_PARAMS
};
#define BODY_COPY_BINDINGS (sizeof(body_copy_slots) / \
                            sizeof(body_copy_slots[0]))

enum { BODY_MAP_BANDS = BODY_BINDINGS, BODY_GRID, BODY_RES_SLOTS };

static sixel_vk_compute_t body_resident_stages[BR_STAGE_COUNT];
static sixel_vk_buffer_t body_resident_buffers[BODY_RES_SLOTS];

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
        slot == BODY_MAP_FIRST || slot == BODY_BAND_FIRST ||
        slot == BODY_REGISTERS || slot == BODY_EMIT_PARAMS;
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
    if (sixel_body_gpu_host_written(slot))
        return sixel_vk_buffer_reserve(runtime, buffer, length, 1);
    if (slot == BODY_OCCUPANCY)
        return sixel_vk_buffer_reserve_device_usage(runtime, buffer, length,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    if (slot >= BODY_LENGTHS && slot <= BODY_BAND_SIZE)
        return sixel_vk_buffer_reserve_device(runtime, buffer, length);
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
            BODY_SPAN_BINDINGS);
    if (status == SIXEL_OK)
        status = sixel_vk_compute_create(runtime, &body_spans_stage,
            body_source, sizeof(body_source) / sizeof(body_source[0]),
            "body_spans", BODY_SPAN_BINDINGS);
    if (status == SIXEL_OK)
        status = sixel_vk_compute_create_local(runtime,
            &body_emit_stages[BR_LENGTHS],
            emit_source, sizeof(emit_source) / sizeof(emit_source[0]),
            "body_emit_lengths", BODY_EMIT_BINDINGS, 256);
    if (status == SIXEL_OK)
        status = sixel_vk_compute_create_local(runtime,
            &body_emit_stages[BR_ORDER],
            emit_source, sizeof(emit_source) / sizeof(emit_source[0]),
            "body_emit_order", BODY_EMIT_BINDINGS, 1);
    if (status == SIXEL_OK)
        status = sixel_vk_compute_create_local(runtime,
            &body_emit_stages[BR_GROUP],
            emit_source, sizeof(emit_source) / sizeof(emit_source[0]),
            "body_emit_order_group", BODY_EMIT_BINDINGS, 256);
    if (status == SIXEL_OK)
        status = sixel_vk_compute_create_local(runtime,
            &body_emit_stages[BR_BASES],
            emit_source, sizeof(emit_source) / sizeof(emit_source[0]),
            "body_emit_bases", BODY_EMIT_BINDINGS, 256);
    if (status == SIXEL_OK)
        status = sixel_vk_compute_create_local(runtime,
            &body_emit_stages[BR_WRITE],
            emit_source, sizeof(emit_source) / sizeof(emit_source[0]),
            "body_emit_write", BODY_EMIT_BINDINGS, 256);
    if (status == SIXEL_OK)
        status = sixel_vk_compute_create(runtime, &body_copy_stage,
            copy_source, sizeof(copy_source) / sizeof(copy_source[0]),
            "body_copy_bytes", BODY_COPY_BINDINGS);
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
    sixel_vk_submission_enter(&body_submission);
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    for (i = 0; i < BODY_BINDINGS; ++i)
        sixel_vk_buffer_release(runtime, &context->buffers[i]);
    sixel_vk_unlock();
    sixel_vk_submission_leave(&body_submission);
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

static SIXELSTATUS
sixel_body_gpu_bind_emit(sixel_vk_runtime_t *runtime,
    sixel_body_gpu_t *context)
{
    sixel_vk_buffer_t const *bound[BODY_EMIT_BINDINGS];
    SIXELSTATUS status;
    size_t i;

    for (i = 0; i < BODY_EMIT_BINDINGS; ++i)
        bound[i] = &context->buffers[body_emit_slots[i]];
    status = sixel_vk_compute_bind(runtime,
            &body_emit_stages[BR_LENGTHS], bound);
    if (status == SIXEL_OK)
        status = sixel_vk_compute_bind(runtime,
            &body_emit_stages[BR_ORDER], bound);
    if (status == SIXEL_OK)
        status = sixel_vk_compute_bind(runtime, &body_emit_stages[BR_GROUP],
            bound);
    if (status == SIXEL_OK)
        status = sixel_vk_compute_bind(runtime,
            &body_emit_stages[BR_BASES], bound);
    if (status == SIXEL_OK)
        status = sixel_vk_compute_bind(runtime,
            &body_emit_stages[BR_WRITE], bound);
    return status;
}

static SIXELSTATUS
sixel_body_gpu_bind_copy(sixel_vk_runtime_t *runtime,
    sixel_body_gpu_t *context)
{
    sixel_vk_buffer_t const *bound[BODY_COPY_BINDINGS];
    size_t i;

    for (i = 0; i < BODY_COPY_BINDINGS; ++i)
        bound[i] = &context->buffers[body_copy_slots[i]];
    return sixel_vk_compute_bind(runtime, &body_copy_stage, bound);
}

static uint32_t
sixel_body_gpu_groups(size_t threads)
{
    return (uint32_t)((threads + 255U) / 256U);
}

/* Span lengths, band ordering and spelling use one schedule. Descriptor
 * instances differ only because in-flight submissions own their bindings. */
static void
body_record_emission(VkCommandBuffer command,
    sixel_vk_compute_t const *stages, uint32_t bands, int grouped,
    uint32_t groups, VkBuffer grid)
{
    if (grid == VK_NULL_HANDLE) {
        sixel_vk_compute_barrier(command);
        sixel_vk_compute_dispatch(command, &stages[BR_LENGTHS], groups);
    } else {
        sixel_vk_compute_dispatch_indirect(command, &stages[BR_LENGTHS],
            grid, 0);
    }
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &stages[BR_ORDER], bands);
    if (grouped) {
        sixel_vk_compute_barrier(command);
        sixel_vk_compute_dispatch(command, &stages[BR_GROUP], bands);
    }
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &stages[BR_BASES], 1u);
    if (grid == VK_NULL_HANDLE) {
        sixel_vk_compute_barrier(command);
        sixel_vk_compute_dispatch(command, &stages[BR_WRITE], groups);
    } else {
        sixel_vk_compute_dispatch_indirect(command, &stages[BR_WRITE], grid, 0);
    }
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
    sixel_body_gpu_emit_params_t emit;
    uint32_t const *band_bytes;
    uint64_t byte_bound;
    uint64_t gap_bound;
    uint32_t actual_bytes;
    int spell;
    int direct_copy;
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
    status = sixel_vk_submission_begin(runtime, &body_submission,
        &command);
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
    status = sixel_vk_submission_submit_and_wait(runtime,
        &body_submission, command);
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

    /* The retained/sparse frame inputs do not affect spelling: the spans
     * already encode painted cells and first-seen register order. Bound
     * each node by its separator, selector, widest gap, and map cells.
     * If a device cannot address the bound, retain the span-only path. */
    gap_bound = request->limited != 0
        ? 5U * (params->encoded_width / 255U) + 2U : 2U;
    for (t = params->encoded_width; t > 0; t /= 10U) ++gap_bound;
    byte_bound = params->bands + map_total +
        nodes_total * (1U + 4U + gap_bound);
    /* Spelling is byte-identical to CPU emission. Admit it only with
     * sustained measured device headroom, independently of sampling. */
    spell = body_headroom.gpu && request->registers != NULL &&
        byte_bound <= UINT32_MAX &&
        byte_bound <= runtime->properties.limits.maxStorageBufferRange;
    direct_copy = 0;
    if (spell) {
        memset(&emit, 0, sizeof(emit));
        emit.width = params->encoded_width;
        emit.words = params->words;
        emit.ncolors = params->ncolors;
        emit.bands = params->bands;
        emit.selectors = request->selectors != 0;
        emit.limited = request->limited != 0;
        emit.nodes = (uint32_t)nodes_total;
        emit.group_limit = params->encoded_width <= 2048U
            ? SIXEL_BODY_GROUP_NODES : 0U;
        status = sixel_body_gpu_reserve(runtime, context, BODY_BAND_FIRST,
            ((size_t)params->bands + 1U) * 4U);
        if (status == SIXEL_OK)
            status = sixel_body_gpu_reserve(runtime, context,
                BODY_REGISTERS, params->ncolors * 4U);
        if (status == SIXEL_OK)
            status = sixel_body_gpu_reserve(runtime, context,
                BODY_EMIT_PARAMS, sizeof(emit));
        if (status == SIXEL_OK)
            status = sixel_body_gpu_reserve(runtime, context,
                BODY_LENGTHS, (size_t)nodes_total * 4U);
        if (status == SIXEL_OK)
            status = sixel_body_gpu_reserve(runtime, context,
                BODY_COLUMNS, (size_t)params->bands *
                (2U * (size_t)params->encoded_width + 1U) * 4U);
        if (status == SIXEL_OK)
            status = sixel_body_gpu_reserve(runtime, context,
                BODY_BITS, (size_t)params->bands * params->words * 4U);
        if (status == SIXEL_OK)
            status = sixel_body_gpu_reserve(runtime, context,
                BODY_SORTED, (size_t)nodes_total * 4U);
        if (status == SIXEL_OK)
            status = sixel_body_gpu_reserve(runtime, context,
                BODY_ENTRIES, (size_t)nodes_total * 16U);
        if (status == SIXEL_OK)
            status = sixel_body_gpu_reserve(runtime, context,
                BODY_BAND_SIZE, params->bands * 4U);
        if (status == SIXEL_OK)
            status = sixel_body_gpu_reserve(runtime, context,
                BODY_BAND_BYTES, ((size_t)params->bands + 1U) * 4U);
        if (status == SIXEL_OK)
            status = sixel_body_gpu_reserve(runtime, context,
                BODY_BYTES, (size_t)byte_bound);
        if (status != SIXEL_OK) return status;
        /* A cached host mapping accepts the exact GPU-produced length.
         * If unavailable, the safe bound is copied by transfer instead. */
        direct_copy = sixel_vk_buffer_reserve_host_cached(runtime,
            &buffers[BODY_BYTES_HOST], (size_t)byte_bound) == SIXEL_OK;
        memcpy(buffers[BODY_BAND_FIRST].mapped, context->band_first,
            ((size_t)params->bands + 1U) * 4U);
        memcpy(buffers[BODY_REGISTERS].mapped, request->registers,
            params->ncolors * 4U);
        memcpy(buffers[BODY_EMIT_PARAMS].mapped, &emit, sizeof(emit));
    }

    params->write_pass = 1;
    memcpy(buffers[BODY_PARAMS].mapped, params, sizeof(*params));
    status = sixel_body_gpu_bind(runtime, context);
    if (status != SIXEL_OK) return status;
    status = sixel_vk_submission_begin(runtime, &body_submission,
        &command);
    if (status != SIXEL_OK) return status;
    sixel_vk_buffer_record_upload(command, &buffers[BODY_NODE_FIRST],
        pairs * 4U);
    sixel_vk_buffer_record_upload(command, &buffers[BODY_MAP_FIRST],
        pairs * 4U);
    sixel_vk_buffer_record_upload(command, &buffers[BODY_PARAMS],
        sizeof(*params));
    if (spell) {
        sixel_vk_buffer_record_upload(command, &buffers[BODY_BAND_FIRST],
            ((size_t)params->bands + 1U) * 4U);
        sixel_vk_buffer_record_upload(command, &buffers[BODY_REGISTERS],
            params->ncolors * 4U);
        sixel_vk_buffer_record_upload(command, &buffers[BODY_EMIT_PARAMS],
            sizeof(emit));
    }
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &body_spans_stage,
        sixel_body_gpu_groups(pairs));
    if (spell) {
        /* Prefix and register buffers are uploaded before the stages read
         * them; each barrier makes the preceding shader writes visible. */
        status = sixel_body_gpu_bind_emit(runtime, context);
        if (status != SIXEL_OK) return status;
        body_record_emission(command, body_emit_stages, params->bands,
            emit.group_limit != 0, sixel_body_gpu_groups(nodes_total),
            VK_NULL_HANDLE);
        sixel_vk_compute_download_barrier(command);
        sixel_vk_buffer_record_download(command, &buffers[BODY_BAND_BYTES],
            ((size_t)params->bands + 1U) * 4U);
        if (direct_copy) {
            status = sixel_body_gpu_bind_copy(runtime, context);
            if (status != SIXEL_OK) return status;
            sixel_vk_compute_barrier(command);
            sixel_vk_compute_dispatch(command, &body_copy_stage,
                sixel_body_gpu_groups(((size_t)byte_bound + 3U) / 4U));
            sixel_vk_compute_download_barrier(command);
        } else {
            sixel_vk_buffer_record_download(command, &buffers[BODY_BYTES],
                (size_t)byte_bound);
        }
    } else {
        sixel_vk_compute_download_barrier(command);
        if (nodes_total != 0)
            sixel_vk_buffer_record_download(command, &buffers[BODY_NODES],
                (size_t)nodes_total * sizeof(sixel_body_gpu_node_t));
        if (map_total != 0)
            sixel_vk_buffer_record_download(command, &buffers[BODY_MAP],
                (size_t)map_total);
        sixel_vk_buffer_record_download(command, &buffers[BODY_FIRST_SEEN],
            pairs * 4U);
    }
    status = sixel_vk_submission_submit_and_wait(runtime,
        &body_submission, command);
    if (spell) {
        if (status == SIXEL_OK)
            status = sixel_vk_buffer_invalidate(runtime,
                &buffers[BODY_BAND_BYTES],
                ((size_t)params->bands + 1U) * 4U);
        if (status != SIXEL_OK) return status;
        band_bytes = (uint32_t const *)buffers[BODY_BAND_BYTES].mapped;
        actual_bytes = band_bytes[params->bands];
        if (actual_bytes > byte_bound) return SIXEL_RUNTIME_ERROR;
        status = sixel_vk_buffer_invalidate(runtime,
            &buffers[direct_copy ? BODY_BYTES_HOST : BODY_BYTES],
            actual_bytes);
    } else {
        if (status == SIXEL_OK)
            status = sixel_vk_buffer_invalidate(runtime, &buffers[BODY_NODES],
                (size_t)nodes_total * sizeof(sixel_body_gpu_node_t));
        if (status == SIXEL_OK)
            status = sixel_vk_buffer_invalidate(runtime, &buffers[BODY_MAP],
                (size_t)map_total);
        if (status == SIXEL_OK)
            status = sixel_vk_buffer_invalidate(runtime,
                &buffers[BODY_FIRST_SEEN], pairs * 4U);
    }
    if (status != SIXEL_OK) return status;

    result->nodes = (sixel_body_gpu_node_t const *)buffers[BODY_NODES].mapped;
    result->map = (unsigned char *)buffers[BODY_MAP].mapped;
    result->band_first = context->band_first;
    result->first_seen = (uint32_t const *)buffers[BODY_FIRST_SEEN].mapped;
    result->ncolors = (int)params->ncolors;
    result->band_count = (int)params->bands;
    result->bytes = spell ? (unsigned char const *)
        buffers[direct_copy ? BODY_BYTES_HOST : BODY_BYTES].mapped : NULL;
    result->band_bytes = spell ? (uint32_t const *)
        buffers[BODY_BAND_BYTES].mapped : NULL;
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
    sixel_vk_submission_enter(&body_submission);
    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    if (runtime == NULL) {
        status = SIXEL_FEATURE_ERROR;
    } else {
        sixel_vk_headroom_choose_locked(&body_headroom, request->gpu_policy);
        status = sixel_body_gpu_run_locked(runtime, context, request,
            &params, result);
        if (runtime->debug && status == SIXEL_OK)
            fprintf(stderr,
                "LSXBODYCHOICE|gpu=%d|bands=%u|load=%.3f|windows=%d\n",
                result->bytes != NULL, params.bands, body_headroom.load,
                body_headroom.windows);
    }
    sixel_vk_unlock();
    sixel_vk_submission_leave(&body_submission);
    return status;
}

/* The legacy body submission may wait with the runtime unlocked. Keep
 * distinct descriptors for the resident graph, owned by the palette gate. */
static SIXELSTATUS
body_resident_bind(sixel_vk_runtime_t *runtime,
    sixel_vk_buffer_t const *indices, sixel_vk_buffer_t const *palette)
{
    static char const *const names[BR_STAGE_COUNT] = {
        "body_classify", "body_spans", "body_resident_local",
        "body_resident_prefix", "body_resident_offsets", "body_emit_lengths",
        "body_emit_order", "body_emit_order_group", "body_resident_bases",
        "body_emit_write", "body_resident_copy"
    };
    static int const tail[] = { BODY_BAND_FIRST, BODY_MAP_BANDS,
        BODY_EMIT_PARAMS, BODY_GRID, BODY_REGISTERS };
    sixel_vk_buffer_t const *bound[SIXEL_VK_COMPUTE_BINDINGS];
    char const *const *source;
    unsigned int stage, bindings, i;
    uint32_t local;
    size_t chunks;
    SIXELSTATUS status;

    for (stage = 0; stage < BR_STAGE_COUNT; ++stage) {
        local = 0;
        if (stage <= BR_OFFSETS) {
            source = body_source;
            chunks = sizeof(body_source) / sizeof(body_source[0]);
            bindings = BODY_SPAN_BINDINGS;
            for (i = 0; i < bindings; ++i)
                bound[i] = &body_resident_buffers[i];
            bound[BODY_INDEXES] = indices;
            if (stage >= BR_LOCAL) {
                for (i = 0; i < sizeof(tail) / sizeof(tail[0]); ++i)
                    bound[bindings + i] = &body_resident_buffers[tail[i]];
                bindings += sizeof(tail) / sizeof(tail[0]);
            }
        } else if (stage == BR_COPY) {
            source = copy_source;
            chunks = sizeof(copy_source) / sizeof(copy_source[0]);
            bindings = BODY_COPY_BINDINGS;
            for (i = 0; i < bindings; ++i)
                bound[i] = &body_resident_buffers[body_copy_slots[i]];
        } else {
            source = emit_source;
            chunks = sizeof(emit_source) / sizeof(emit_source[0]);
            bindings = BODY_EMIT_BINDINGS;
            local = stage == BR_ORDER || stage == BR_BASES ? 1u : 256u;
            for (i = 0; i < bindings; ++i)
                bound[i] = &body_resident_buffers[body_emit_slots[i]];
            if (stage == BR_BASES) {
                bound[bindings++] = palette;
                bound[bindings++] = &body_resident_buffers[BODY_GRID];
            }
        }
        status = sixel_vk_compute_create_local(runtime,
            &body_resident_stages[stage], source, chunks, names[stage],
            bindings, local);
        if (status == SIXEL_OK)
            status = sixel_vk_compute_bind(runtime,
                &body_resident_stages[stage], bound);
        if (status != SIXEL_OK)
            return status;
    }
    return SIXEL_OK;
}

static void
body_resident_dispatch(VkCommandBuffer command, int stage, uint32_t groups)
{
    sixel_vk_compute_barrier(command);
    sixel_vk_compute_dispatch(command, &body_resident_stages[stage], groups);
}

static void
body_resident_indirect(VkCommandBuffer command, int stage, VkDeviceSize offset)
{
    sixel_vk_compute_dispatch_indirect(command, &body_resident_stages[stage],
        body_resident_buffers[BODY_GRID].buffer, offset);
}

SIXELSTATUS
sixel_vk_body_record_resident(sixel_vk_runtime_t *runtime,
    VkCommandBuffer command, sixel_vk_buffer_t const *indices,
    sixel_vk_buffer_t const *palette, uint32_t width, uint32_t height,
    int limited, VkQueryPool timestamps)
{
    sixel_body_gpu_params_t p;
    sixel_body_gpu_emit_params_t emit;
    sixel_vk_buffer_t *buffers;
    size_t lengths[BODY_RES_SLOTS];
    size_t pixels, pairs, bound, gap, t;
    VkBufferUsageFlags usage;
    unsigned int i;
    SIXELSTATUS status;

    if (!width || !height || width > 2048u || height > 2048u ||
            width * height > 1024u * 1024u)
        return SIXEL_FALSE;
    p = (sixel_body_gpu_params_t){width, height, width, height, 0u, 0u,
        SIXEL_PALETTE_MAX, -1, (width + 31u) / 32u, (height + 5u) / 6u,
        0u, 0u};
    emit = (sixel_body_gpu_emit_params_t){width, p.words, p.ncolors,
        p.bands, 1u, limited != 0, 0u, SIXEL_BODY_GROUP_NODES};
    pixels = (size_t)width * height;
    pairs = (size_t)p.bands * p.ncolors;
    gap = limited ? 5u * (width / 255u) + 2u : 2u;
    for (t = width; t; t /= 10u) ++gap;
    /* One occupied column per possible span. Joining adds up to nine
     * empty columns per further occupied column, so map <= 10*pixels. */
    bound = p.bands + pixels * (15u + gap) + 18u * p.ncolors;
    bound = (bound + 3u) & ~(size_t)3u;
    memcpy(lengths, (size_t[BODY_RES_SLOTS]){
        [BODY_OCCUPANCY] = pairs * p.words * 4u,
        [BODY_COUNTS] = pairs * 4u, [BODY_WIDTHS] = pairs * 4u,
        [BODY_NODE_FIRST] = pairs * 4u, [BODY_MAP_FIRST] = pairs * 4u,
        [BODY_NODES] = pixels * sizeof(sixel_body_gpu_node_t),
        [BODY_MAP] = 10u * pixels, [BODY_MASK] = 16u,
        [BODY_PARAMS] = sizeof(p), [BODY_FIRST_SEEN] = pairs * 4u,
        [BODY_BAND_FIRST] = 4u * (p.bands + 1u),
        [BODY_LENGTHS] = pixels * 4u, [BODY_REGISTERS] = 4u * p.ncolors,
        [BODY_COLUMNS] = p.bands * (2u * width + 1u) * 4u,
        [BODY_BITS] = p.bands * p.words * 4u, [BODY_SORTED] = pixels * 4u,
        [BODY_ENTRIES] = pixels * 16u, [BODY_BAND_SIZE] = 4u * p.bands,
        [BODY_BAND_BYTES] = 4u * (p.bands + 1u), [BODY_BYTES] = bound,
        [BODY_EMIT_PARAMS] = sizeof(emit), [BODY_BYTES_HOST] = bound + 4u,
        [BODY_MAP_BANDS] = 4u * p.bands, [BODY_GRID] = 6u * sizeof(uint32_t)
    }, sizeof(lengths));
    buffers = body_resident_buffers;
    for (i = 0; i < BODY_RES_SLOTS; ++i) {
        if (lengths[i] == 0)
            continue;
        if (lengths[i] > runtime->properties.limits.maxStorageBufferRange)
            return SIXEL_FALSE;
        if (i == BODY_BYTES_HOST) {
            status = sixel_vk_buffer_reserve_host_cached(runtime,
                &buffers[i], lengths[i]);
            if (status != SIXEL_OK)
                return SIXEL_FALSE;
        } else if (i == BODY_PARAMS || i == BODY_EMIT_PARAMS) {
            status = sixel_vk_buffer_reserve(runtime, &buffers[i],
                lengths[i], 1);
        } else {
            usage = i == BODY_OCCUPANCY ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0;
            if (i == BODY_GRID) usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            status = sixel_vk_buffer_reserve_device_usage(runtime,
                &buffers[i], lengths[i], usage);
        }
        if (status != SIXEL_OK)
            return status;
    }

    status = body_resident_bind(runtime, indices, palette);
    if (status != SIXEL_OK)
        return status;
    memcpy(buffers[BODY_PARAMS].mapped, &p, sizeof(p));
    memcpy(buffers[BODY_EMIT_PARAMS].mapped, &emit, sizeof(emit));
    sixel_vk_buffer_record_upload(command, &buffers[BODY_PARAMS], sizeof(p));
    sixel_vk_buffer_record_upload(command, &buffers[BODY_EMIT_PARAMS],
        sizeof(emit));
    vkCmdFillBuffer(command, buffers[BODY_OCCUPANCY].buffer, 0,
        lengths[BODY_OCCUPANCY], 0u);
    body_resident_dispatch(command, BR_CLASSIFY, sixel_body_gpu_groups(pixels));
    body_resident_dispatch(command, BR_SPANS, sixel_body_gpu_groups(pairs));
    body_resident_dispatch(command, BR_LOCAL, sixel_body_gpu_groups(p.bands));
    body_resident_dispatch(command, BR_PREFIX, 1u);
    body_resident_dispatch(command, BR_OFFSETS, sixel_body_gpu_groups(pairs));
    body_resident_dispatch(command, BR_SPANS, sixel_body_gpu_groups(pairs));
    body_record_emission(command, body_resident_stages, p.bands,
        emit.group_limit != 0, 0, buffers[BODY_GRID].buffer);
    if (timestamps != VK_NULL_HANDLE)
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            timestamps, 1u);
    body_resident_indirect(command, BR_COPY, 3u * sizeof(uint32_t));
    sixel_vk_compute_download_barrier(command);
    return SIXEL_OK;
}

SIXELSTATUS
sixel_vk_body_read_resident(sixel_vk_runtime_t *runtime, unsigned char **bytes,
    int *size)
{
    sixel_vk_buffer_t *output;
    uint32_t count;
    SIXELSTATUS status;

    output = &body_resident_buffers[BODY_BYTES_HOST];
    status = sixel_vk_buffer_invalidate(runtime, output, sizeof(count));
    if (status != SIXEL_OK)
        return status;
    count = *(uint32_t const *)output->mapped;
    if (!count || count > INT_MAX ||
            (VkDeviceSize)count + sizeof(count) > output->capacity)
        return SIXEL_FALSE;
    status = sixel_vk_buffer_invalidate(runtime, output, count + sizeof(count));
    if (status != SIXEL_OK)
        return status;
    *bytes = malloc(count);
    if (*bytes == NULL)
        return SIXEL_BAD_ALLOCATION;
    memcpy(*bytes, (unsigned char const *)output->mapped + sizeof(count),
        count);
    *size = (int)count;
    return SIXEL_OK;
}
#endif
