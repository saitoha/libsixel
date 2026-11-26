/* SPDX-License-Identifier: MIT */
/* Metal backend for encoder-body-gpu.h. */

#include "config.h"

#if defined(HAVE_METAL)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "encoder-body-gpu-metal.h"
#include "gpu-runtime-metal.h"
#include "loader-common.h"
#include "timer.h"

static char const *const sixel_body_gpu_source[] = {
#include "generated-metal/encoder-body-gpu-shader.inc.h"
};

typedef struct sixel_body_gpu_params {
    uint32_t width, height, encoded_width, encoded_height;
    uint32_t offset_left, offset_top, ncolors;
    int32_t keycolor;
    uint32_t words, bands, has_mask, nodes;
} sixel_body_gpu_params_t;

typedef struct sixel_body_gpu_emit_params {
    uint32_t encoded_width, words, ncolors, bands;
    uint32_t selectors, limited, nodes, group_limit;
} sixel_body_gpu_emit_params_t;

/* Mirrors BODY_GROUP_NODES and BODY_GROUP_THREADS in the shader. */
#define SIXEL_BODY_GROUP_NODES 4096U
#define SIXEL_BODY_GROUP_THREADS 256U

enum {
    BODY_INDEXES, BODY_MASK, BODY_OCCUPANCY, BODY_COUNTS, BODY_WIDTHS,
    BODY_NODE_FIRST, BODY_MAP_FIRST, BODY_NODES, BODY_MAP, BODY_FIRST_SEEN,
    BODY_BAND_FIRST, BODY_LENGTHS, BODY_REGISTERS, BODY_COLUMNS, BODY_BITS,
    BODY_SORTED, BODY_ENTRIES, BODY_BAND_SIZE, BODY_BAND_BYTES, BODY_BYTES,
    BODY_KEYS, BODY_KEY_SLOTS, BODY_NODE_BAND, BODY_MAP_BANDS,
    BODY_SHAPE, BODY_EMIT_PARAMS, BODY_GRID, BODY_BYTE_COUNT, BODY_SLOTS
};

enum {
    BODY_CLASSIFY, BODY_SPANS, BODY_MAP_BUILD, BODY_LENGTH, BODY_ORDER,
    BODY_ORDER_GROUP, BODY_BASES, BODY_WRITE, BODY_RESIDENT_LOCAL,
    BODY_RESIDENT_PREFIX, BODY_RESIDENT_OFFSETS, BODY_RESIDENT_BASES,
    BODY_PIPELINES
};

static char const *const sixel_body_gpu_kernels[BODY_PIPELINES] = {
    "body_classify", "body_spans", "body_map", "body_lengths", "body_order",
    "body_order_group", "body_bases", "body_write", "body_resident_local",
    "body_resident_prefix", "body_resident_offsets", "body_resident_bases"
};

struct sixel_body_gpu {
    void *device;           /* id<MTLDevice>, retained */
    void *queue;            /* id<MTLCommandQueue> */
    void *pipelines[BODY_PIPELINES];    /* id<MTLComputePipelineState> */
    void *buffers[BODY_SLOTS];  /* id<MTLBuffer>, grown on demand */
    size_t lengths[BODY_SLOTS];
    uint32_t group_limit;
    int resident;
    uint32_t *band_first;
    size_t band_first_count;
};

static id<MTLBuffer>
sixel_body_gpu_buffer(sixel_body_gpu_t *context, int slot, size_t length)
{
    id<MTLBuffer> buffer;
    MTLResourceOptions storage;

    if (length == 0) {
        length = 16;
    }
    if (context->buffers[slot] != NULL && context->lengths[slot] >= length) {
        return (__bridge id<MTLBuffer>)context->buffers[slot];
    }
    if (context->buffers[slot] != NULL) {
        CFRelease(context->buffers[slot]);
        context->buffers[slot] = NULL;
        context->lengths[slot] = 0;
    }
    /* Grow by half again so slowly growing frames do not reallocate each
     * time. Only the final size and bytes are shared in resident mode. */
    length += length / 2;
    storage = context->resident && slot != BODY_BYTES && slot != BODY_BYTE_COUNT
        ? MTLResourceStorageModePrivate : MTLResourceStorageModeShared;
    buffer = [(__bridge id<MTLDevice>)context->device
        newBufferWithLength:length options:storage];
    if (buffer == nil) {
        return nil;
    }
    context->buffers[slot] = (void *)CFBridgingRetain(buffer);
    context->lengths[slot] = length;
    return buffer;
}

static SIXELSTATUS
sixel_body_metal_new(sixel_body_gpu_t **result, int resident)
{
    sixel_body_gpu_t *context;
    SIXELSTATUS status;
    NSMutableString *source;
    size_t chunk;

    *result = NULL;
    context = (sixel_body_gpu_t *)calloc(1, sizeof(*context));
    if (context == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    context->resident = resident;
    status = SIXEL_FEATURE_ERROR;
    @autoreleasepool {
        id<MTLDevice> device;
        id<MTLLibrary> library;
        id<MTLComputePipelineState> pipeline;
        id<MTLCommandQueue> queue;
        NSError *error;
        int i;

        error = nil;
        sixel_trace_topic_message("metal_body", "%.6f device begin",
            sixel_timer_monotonic());
        device = MTLCreateSystemDefaultDevice();
        sixel_trace_topic_message("metal_body", "%.6f device end ok=%d",
            sixel_timer_monotonic(), device != nil);
        if (device == nil) {
            goto end;
        }
        source = [NSMutableString string];
        for (chunk = 0; chunk < sizeof(sixel_body_gpu_source) /
                sizeof(sixel_body_gpu_source[0]); ++chunk) {
            [source appendString:[NSString stringWithUTF8String:
                sixel_body_gpu_source[chunk]]];
        }
        sixel_trace_topic_message("metal_body", "%.6f library begin",
            sixel_timer_monotonic());
        library = [device newLibraryWithSource:source
                                       options:nil
                                         error:&error];
        sixel_trace_topic_message("metal_body", "%.6f library end ok=%d",
            sixel_timer_monotonic(), library != nil);
        if (library == nil) {
            goto end;
        }
        context->device = (void *)CFBridgingRetain(device);
        for (i = 0; i < (resident ? BODY_PIPELINES : BODY_RESIDENT_LOCAL);
                ++i) {
            sixel_trace_topic_message("metal_body", "%.6f pipeline %s begin",
                sixel_timer_monotonic(), sixel_body_gpu_kernels[i]);
            pipeline = [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:
                    [NSString stringWithUTF8String:
                        sixel_body_gpu_kernels[i]]] error:&error];
            sixel_trace_topic_message("metal_body",
                "%.6f pipeline %s end ok=%d", sixel_timer_monotonic(),
                sixel_body_gpu_kernels[i], pipeline != nil);
            /* The serial order kernel covers every band when the grouped
             * kernel exceeds this device's limits (including validation
             * instrumentation). Never submit an unsupported threadgroup. */
            if (i == BODY_ORDER_GROUP) {
                if (pipeline == nil ||
                        pipeline.staticThreadgroupMemoryLength >
                            device.maxThreadgroupMemoryLength ||
                        pipeline.maxTotalThreadsPerThreadgroup <
                            SIXEL_BODY_GROUP_THREADS) continue;
                context->group_limit = SIXEL_BODY_GROUP_NODES;
            }
            if (pipeline == nil || (resident &&
                    ((i == BODY_CLASSIFY &&
                        pipeline.maxTotalThreadsPerThreadgroup < 256) ||
                     ((i == BODY_MAP_BUILD || i == BODY_LENGTH ||
                       i == BODY_WRITE || i == BODY_RESIDENT_OFFSETS) &&
                        pipeline.maxTotalThreadsPerThreadgroup < 64) ||
                     (i == BODY_RESIDENT_LOCAL &&
                        pipeline.maxTotalThreadsPerThreadgroup < 32)))) {
                goto end;
            }
            context->pipelines[i] = (void *)CFBridgingRetain(pipeline);
        }
        sixel_trace_topic_message("metal_body", "%.6f queue begin",
            sixel_timer_monotonic());
        queue = [device newCommandQueue];
        sixel_trace_topic_message("metal_body", "%.6f queue end ok=%d",
            sixel_timer_monotonic(), queue != nil);
        if (queue == nil) {
            goto end;
        }
        context->queue = (void *)CFBridgingRetain(queue);
        status = SIXEL_OK;
    }
end:
    if (SIXEL_FAILED(status)) {
        sixel_body_gpu_destroy(context);
        return status;
    }
    *result = context;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_body_gpu_new(sixel_body_gpu_t **result)
{
    return sixel_body_metal_new(result, 0);
}

SIXELSTATUS
sixel_body_metal_resident_new(sixel_body_gpu_t **result)
{
    return sixel_body_metal_new(result, 1);
}

void
sixel_body_gpu_destroy(sixel_body_gpu_t *context)
{
    size_t i;

    if (context == NULL) {
        return;
    }
    for (i = 0; i < sizeof(context->buffers) / sizeof(context->buffers[0]);
            ++i) {
        if (context->buffers[i] != NULL) {
            CFRelease(context->buffers[i]);
        }
    }
    for (i = 0; i < BODY_PIPELINES; ++i) {
        if (context->pipelines[i] != NULL) CFRelease(context->pipelines[i]);
    }
    if (context->queue != NULL) CFRelease(context->queue);
    if (context->device != NULL) CFRelease(context->device);
    free(context->band_first);
    free(context);
}

static void
sixel_body_gpu_encode_spans(id<MTLComputeCommandEncoder> encoder,
                            sixel_body_gpu_t *context,
                            sixel_body_gpu_params_t const *params,
                            uint32_t write_pass,
                            id<MTLBuffer> indexes, id<MTLBuffer> mask)
{
    static int const slots[] = {
        BODY_OCCUPANCY, BODY_COUNTS, BODY_WIDTHS, BODY_NODE_FIRST,
        BODY_MAP_FIRST, BODY_NODES, BODY_MAP
    };
    NSUInteger i;
    NSUInteger threads;
    NSUInteger group;
    id<MTLComputePipelineState> pipeline;

    pipeline = (__bridge id<MTLComputePipelineState>)
        context->pipelines[BODY_SPANS];
    [encoder setComputePipelineState:pipeline];
    for (i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
        [encoder setBuffer:(__bridge id<MTLBuffer>)context->buffers[slots[i]]
                    offset:0 atIndex:i];
    }
    [encoder setBuffer:indexes offset:0 atIndex:7];
    [encoder setBuffer:mask offset:0 atIndex:8];
    [encoder setBytes:params length:sizeof(*params) atIndex:9];
    [encoder setBytes:&write_pass length:sizeof(write_pass) atIndex:10];
    [encoder setBuffer:(__bridge id<MTLBuffer>)
        context->buffers[BODY_FIRST_SEEN] offset:0 atIndex:11];
    [encoder setBuffer:(__bridge id<MTLBuffer>)
        context->buffers[BODY_NODE_BAND] offset:0 atIndex:12];
    threads = (NSUInteger)params->bands * params->ncolors;
    group = [pipeline maxTotalThreadsPerThreadgroup];
    if (group > 256) group = 256;
    [encoder dispatchThreads:MTLSizeMake(threads, 1, 1)
       threadsPerThreadgroup:MTLSizeMake(group, 1, 1)];
}

/* All three node kernels use 64 lanes when the count comes from the
 * device-side prefix. The same rounded dispatch grid serves each kernel. */
static void
sixel_body_metal_dispatch_nodes(id<MTLComputeCommandEncoder> encoder,
                              sixel_body_gpu_t *context)
{
    [encoder dispatchThreadgroupsWithIndirectBuffer:(__bridge id<MTLBuffer>)
        context->buffers[BODY_GRID] indirectBufferOffset:0
        threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
}

static void
sixel_body_metal_bind_emit(id<MTLComputeCommandEncoder> encoder,
    sixel_body_gpu_t *context, sixel_body_gpu_emit_params_t const *params,
    NSUInteger index)
{
    if (context->resident) {
        [encoder setBuffer:(__bridge id<MTLBuffer>)
            context->buffers[BODY_EMIT_PARAMS] offset:0 atIndex:index];
    } else {
        [encoder setBytes:params length:sizeof(*params) atIndex:index];
    }
}

/* One lane per span writes its column map and reduces its first pixel. */
static void
sixel_body_gpu_encode_map(id<MTLComputeCommandEncoder> encoder,
                          sixel_body_gpu_t *context,
                          sixel_body_gpu_params_t const *params,
                          id<MTLBuffer> indexes, id<MTLBuffer> mask)
{
    static int const slots[] = {
        BODY_NODES, BODY_NODE_BAND, BODY_MAP
    };
    id<MTLComputePipelineState> pipeline;
    NSUInteger i;
    NSUInteger group;

    pipeline = (__bridge id<MTLComputePipelineState>)
        context->pipelines[BODY_MAP_BUILD];
    [encoder setComputePipelineState:pipeline];
    for (i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
        [encoder setBuffer:(__bridge id<MTLBuffer>)context->buffers[slots[i]]
                    offset:0 atIndex:i];
    }
    [encoder setBuffer:indexes offset:0 atIndex:3];
    [encoder setBuffer:mask offset:0 atIndex:4];
    [encoder setBuffer:(__bridge id<MTLBuffer>)
        context->buffers[BODY_FIRST_SEEN] offset:0 atIndex:5];
    if (context->resident) {
        [encoder setBuffer:(__bridge id<MTLBuffer>)
            context->buffers[BODY_SHAPE] offset:0 atIndex:6];
        sixel_body_metal_dispatch_nodes(encoder, context);
        return;
    }
    [encoder setBytes:params length:sizeof(*params) atIndex:6];
    group = [pipeline maxTotalThreadsPerThreadgroup];
    if (group > 256) group = 256;
    [encoder dispatchThreads:MTLSizeMake(params->nodes > 0 ?
            params->nodes : 1, 1, 1)
       threadsPerThreadgroup:MTLSizeMake(group, 1, 1)];
}

static void
sixel_body_gpu_dispatch(id<MTLComputeCommandEncoder> encoder,
                        sixel_body_gpu_t *context, int kernel,
                        int const *slots, NSUInteger count,
                        sixel_body_gpu_emit_params_t const *params,
                        NSUInteger threads)
{
    id<MTLComputePipelineState> pipeline;
    NSUInteger group;
    NSUInteger i;

    pipeline = (__bridge id<MTLComputePipelineState>)
        context->pipelines[kernel];
    [encoder setComputePipelineState:pipeline];
    for (i = 0; i < count; ++i) {
        [encoder setBuffer:(__bridge id<MTLBuffer>)context->buffers[slots[i]]
                    offset:0 atIndex:i];
    }
    sixel_body_metal_bind_emit(encoder, context, params, count);
    if (context->resident && (kernel == BODY_LENGTH || kernel == BODY_WRITE)) {
        sixel_body_metal_dispatch_nodes(encoder, context);
        return;
    }
    group = [pipeline maxTotalThreadsPerThreadgroup];
    if (group > 64) group = 64;
    [encoder dispatchThreads:MTLSizeMake(threads > 0 ? threads : 1, 1, 1)
       threadsPerThreadgroup:MTLSizeMake(group, 1, 1)];
}

/* Spell the body after the write pass: span lengths, the per-band order
 * and offsets, band bases, then the bytes. */
static void
sixel_body_gpu_encode_emit(id<MTLComputeCommandEncoder> encoder,
                           sixel_body_gpu_t *context,
                           sixel_body_gpu_emit_params_t const *params,
                           sixel_body_metal_palette_t const *palette)
{
    static int const lengths[] = { BODY_NODES, BODY_MAP, BODY_LENGTHS };
    static int const order[] = {
        BODY_NODES, BODY_BAND_FIRST, BODY_FIRST_SEEN, BODY_LENGTHS,
        BODY_REGISTERS, BODY_COLUMNS, BODY_BITS, BODY_SORTED, BODY_ENTRIES,
        BODY_BAND_SIZE
    };
    static int const group[] = {
        BODY_NODES, BODY_BAND_FIRST, BODY_FIRST_SEEN, BODY_LENGTHS,
        BODY_REGISTERS, BODY_KEYS, BODY_KEY_SLOTS, BODY_ENTRIES,
        BODY_BAND_SIZE
    };
    static int const bases[] = { BODY_BAND_SIZE, BODY_BAND_BYTES, BODY_BYTES };
    static int const trim[] = { BODY_BAND_FIRST, BODY_NODES, BODY_ENTRIES };
    id<MTLComputePipelineState> pipeline;
    NSUInteger i;
    static int const write[] = {
        BODY_NODES, BODY_MAP, BODY_ENTRIES, BODY_BAND_BYTES, BODY_REGISTERS,
        BODY_BYTES
    };

    sixel_body_gpu_dispatch(encoder, context, BODY_LENGTH, lengths,
        sizeof(lengths) / sizeof(lengths[0]), params, params->nodes);
    sixel_body_gpu_dispatch(encoder, context, BODY_ORDER, order,
        sizeof(order) / sizeof(order[0]), params, params->bands);
    if (params->group_limit != 0) {
        pipeline = (__bridge id<MTLComputePipelineState>)
            context->pipelines[BODY_ORDER_GROUP];
        [encoder setComputePipelineState:pipeline];
        for (i = 0; i < sizeof(group) / sizeof(group[0]); ++i) {
            [encoder setBuffer:(__bridge id<MTLBuffer>)
                context->buffers[group[i]] offset:0 atIndex:i];
        }
        sixel_body_metal_bind_emit(encoder, context, params,
            sizeof(group) / sizeof(group[0]));
        [encoder dispatchThreadgroups:MTLSizeMake(params->bands, 1, 1)
                threadsPerThreadgroup:MTLSizeMake(
                    SIXEL_BODY_GROUP_THREADS, 1, 1)];
    }
    if (context->resident) {
        [encoder setComputePipelineState:(__bridge id<MTLComputePipelineState>)
            context->pipelines[BODY_RESIDENT_BASES]];
        for (i = 0; i < sizeof(bases) / sizeof(bases[0]); ++i) {
            [encoder setBuffer:(__bridge id<MTLBuffer>)
                context->buffers[bases[i]] offset:0 atIndex:i];
        }
        [encoder setBuffer:palette->rgb offset:0 atIndex:3];
        [encoder setBuffer:palette->count offset:palette->count_offset
                    atIndex:4];
        [encoder setBuffer:palette->state offset:palette->failure_offset
                    atIndex:5];
        [encoder setBuffer:(__bridge id<MTLBuffer>)
            context->buffers[BODY_BYTE_COUNT] offset:0 atIndex:6];
        sixel_body_metal_bind_emit(encoder, context, params, 7);
        for (i = 0; i < sizeof(trim) / sizeof(trim[0]); ++i) {
            [encoder setBuffer:(__bridge id<MTLBuffer>)
                context->buffers[trim[i]] offset:0 atIndex:8u + i];
        }
        [encoder dispatchThreads:MTLSizeMake(1, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
    } else {
        sixel_body_gpu_dispatch(encoder, context, BODY_BASES, bases,
            sizeof(bases) / sizeof(bases[0]), params, 1);
    }
    sixel_body_gpu_dispatch(encoder, context, BODY_WRITE, write,
        sizeof(write) / sizeof(write[0]), params, params->nodes);
}

static void
sixel_body_metal_classify(id<MTLComputeCommandEncoder> encoder,
    sixel_body_gpu_t *context, sixel_body_gpu_params_t const *params,
    id<MTLBuffer> indexes, id<MTLBuffer> mask)
{
    [encoder setComputePipelineState:(__bridge id<MTLComputePipelineState>)
        context->pipelines[BODY_CLASSIFY]];
    [encoder setBuffer:indexes offset:0 atIndex:0];
    [encoder setBuffer:mask offset:0 atIndex:1];
    [encoder setBuffer:(__bridge id<MTLBuffer>)
        context->buffers[BODY_OCCUPANCY] offset:0 atIndex:2];
    [encoder setBytes:params length:sizeof(*params) atIndex:3];
    [encoder dispatchThreads:MTLSizeMake(params->width, params->height, 1)
       threadsPerThreadgroup:MTLSizeMake(32, 8, 1)];
}

/* A commit can block before the command reaches the completion wait. */
static void
sixel_body_gpu_submit(id<MTLCommandBuffer> command, char const *pass)
{
    sixel_trace_topic_message("metal_body", "%.6f %s commit begin",
        sixel_timer_monotonic(), pass);
    [command commit];
    sixel_trace_topic_message("metal_body", "%.6f %s wait begin",
        sixel_timer_monotonic(), pass);
    [command waitUntilCompleted];
    sixel_trace_topic_message("metal_body", "%.6f %s wait end status=%d",
        sixel_timer_monotonic(), pass, (int)[command status]);
}

SIXELSTATUS
sixel_body_gpu_run(sixel_body_gpu_t *context,
                   sixel_body_gpu_request_t const *request,
                   sixel_body_gpu_result_t *result)
{
    sixel_body_gpu_params_t params;
    size_t pixels;
    size_t pairs;
    size_t occupancy_words;
    size_t t;
    size_t b;
    uint64_t nodes_total;
    uint64_t map_total;
    uint32_t *counts;
    uint32_t *widths;
    uint32_t *node_first;
    uint32_t *map_first;
    sixel_body_gpu_emit_params_t emit;
    size_t gap_bound;
    size_t byte_bound;
    int spell;

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
    pixels = (size_t)request->width * (size_t)request->height;
    pairs = (size_t)params.bands * params.ncolors;
    occupancy_words = pairs * params.words;
    if (pairs > UINT32_MAX || occupancy_words > SIZE_MAX / 4U) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    @autoreleasepool {
        id<MTLCommandQueue> queue;
        id<MTLCommandBuffer> command;
        id<MTLBlitCommandEncoder> blit;
        id<MTLComputeCommandEncoder> encoder;
        id<MTLBuffer> indexes;
        id<MTLBuffer> mask;
        int staged;

        /* Bind shared frame storage when it lives on this context's
         * device; otherwise stage both planes into owned buffers. */
        indexes = nil;
        mask = nil;
        if (request->device != NULL &&
                sixel_gpu_metal_device(request->device) ==
                    (__bridge id<MTLDevice>)context->device) {
            indexes = sixel_gpu_metal_buffer(request->device,
                request->indexes_buffer);
            mask = request->mask != NULL
                ? sixel_gpu_metal_buffer(request->device,
                    request->mask_buffer) : nil;
            if (indexes == nil || [indexes length] < pixels ||
                    [indexes contents] != (void *)request->indexes ||
                    (request->mask != NULL && (mask == nil ||
                        [mask length] < pixels ||
                        [mask contents] != (void *)request->mask))) {
                indexes = nil;
                mask = nil;
            }
        }
        staged = indexes == nil;
        if (staged) {
            indexes = sixel_body_gpu_buffer(context, BODY_INDEXES, pixels);
        }
        if (mask == nil) {
            mask = sixel_body_gpu_buffer(context, BODY_MASK,
                staged && request->mask != NULL ? pixels : 16);
        }
        if (indexes == nil || mask == nil ||
                sixel_body_gpu_buffer(context, BODY_OCCUPANCY,
                    occupancy_words * 4U) == nil ||
                sixel_body_gpu_buffer(context, BODY_COUNTS, pairs * 4U) == nil ||
                sixel_body_gpu_buffer(context, BODY_FIRST_SEEN,
                    pairs * 4U) == nil ||
                sixel_body_gpu_buffer(context, BODY_WIDTHS, pairs * 4U) == nil ||
                sixel_body_gpu_buffer(context, BODY_NODE_FIRST,
                    pairs * 4U) == nil ||
                sixel_body_gpu_buffer(context, BODY_MAP_FIRST,
                    pairs * 4U) == nil ||
                /* Metal validates write-pass arguments on the count
                 * dispatch too. Grow these after counts are available. */
                sixel_body_gpu_buffer(context, BODY_NODE_BAND, 16) == nil ||
                sixel_body_gpu_buffer(context, BODY_NODES, 16) == nil ||
                sixel_body_gpu_buffer(context, BODY_MAP, 16) == nil) {
            return SIXEL_BAD_ALLOCATION;
        }
        if (staged) {
            memcpy([indexes contents], request->indexes, pixels);
            if (request->mask != NULL) {
                memcpy([mask contents], request->mask, pixels);
            }
        }

        queue = (__bridge id<MTLCommandQueue>)context->queue;
        sixel_trace_topic_message("metal_body", "%.6f classify-count encode",
            sixel_timer_monotonic());
        command = [queue commandBuffer];
        blit = [command blitCommandEncoder];
        [blit fillBuffer:(__bridge id<MTLBuffer>)
                context->buffers[BODY_OCCUPANCY]
                   range:NSMakeRange(0, occupancy_words * 4U) value:0];
        [blit endEncoding];
        encoder = [command computeCommandEncoder];
        sixel_body_metal_classify(encoder, context, &params, indexes, mask);
        sixel_body_gpu_encode_spans(encoder, context, &params, 0U,
            indexes, mask);
        [encoder endEncoding];
        sixel_body_gpu_submit(command, "classify-count");
        sixel_gpu_statistics_record_metal(command);
        if ([command status] != MTLCommandBufferStatusCompleted) {
            return SIXEL_RUNTIME_ERROR;
        }

        /* Prefix offsets; pairs are band-major, so each band's nodes are
         * contiguous and register-ordered. */
        if (context->band_first_count < (size_t)params.bands + 1U) {
            free(context->band_first);
            context->band_first = (uint32_t *)malloc(
                ((size_t)params.bands + 1U) * sizeof(uint32_t));
            if (context->band_first == NULL) {
                context->band_first_count = 0;
                return SIXEL_BAD_ALLOCATION;
            }
            context->band_first_count = (size_t)params.bands + 1U;
        }
        counts = (uint32_t *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_COUNTS] contents];
        widths = (uint32_t *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_WIDTHS] contents];
        node_first = (uint32_t *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_NODE_FIRST] contents];
        map_first = (uint32_t *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_MAP_FIRST] contents];
        nodes_total = 0;
        map_total = 0;
        for (b = 0; b < params.bands; ++b) {
            context->band_first[b] = (uint32_t)nodes_total;
            for (t = b * params.ncolors; t < (b + 1U) * params.ncolors; ++t) {
                node_first[t] = (uint32_t)nodes_total;
                map_first[t] = (uint32_t)map_total;
                nodes_total += counts[t];
                map_total += widths[t];
            }
            if (nodes_total > UINT32_MAX || map_total > UINT32_MAX) {
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
        }
        context->band_first[params.bands] = (uint32_t)nodes_total;
        params.nodes = (uint32_t)nodes_total;
        if (sixel_body_gpu_buffer(context, BODY_NODES,
                    (size_t)nodes_total * sizeof(sixel_body_gpu_node_t))
                    == nil ||
                sixel_body_gpu_buffer(context, BODY_NODE_BAND,
                    (size_t)nodes_total * sizeof(uint32_t)) == nil ||
                sixel_body_gpu_buffer(context, BODY_MAP,
                    (size_t)map_total + SIXEL_BODY_MAP_TAIL) == nil) {
            return SIXEL_BAD_ALLOCATION;
        }

        /* Every span costs at most '$', a selector, its leading gap and
         * one byte per column; a gap is no longer than the limited
         * spelling of the full width. */
        spell = request->registers != NULL;
        if (spell) {
            gap_bound = 5U * (params.encoded_width / 255U) + 2U;
            for (t = params.encoded_width; t > 0; t /= 10) ++gap_bound;
            byte_bound = (size_t)params.bands + (size_t)map_total +
                (size_t)nodes_total * (1U + 4U + gap_bound);
            if (byte_bound > UINT32_MAX) {
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
            memset(&emit, 0, sizeof(emit));
            emit.encoded_width = params.encoded_width;
            emit.words = params.words;
            emit.ncolors = params.ncolors;
            emit.bands = params.bands;
            emit.selectors = request->selectors != 0;
            emit.limited = request->limited != 0;
            emit.nodes = (uint32_t)nodes_total;
            /* Spans pack start and end in 16 bits each. */
            emit.group_limit = params.encoded_width <= 0xffffU
                ? context->group_limit : 0U;
            if (sixel_body_gpu_buffer(context, BODY_BAND_FIRST,
                        ((size_t)params.bands + 1U) * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_LENGTHS,
                        (size_t)nodes_total * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_REGISTERS,
                        (size_t)params.ncolors * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_COLUMNS,
                        (size_t)params.bands *
                        (2U * (size_t)params.encoded_width + 1U) * 4U)
                        == nil ||
                    sixel_body_gpu_buffer(context, BODY_BITS,
                        (size_t)params.bands * params.words * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_SORTED,
                        (size_t)nodes_total * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_ENTRIES,
                        (size_t)nodes_total * 16U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_BAND_SIZE,
                        (size_t)params.bands * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_BAND_BYTES,
                        ((size_t)params.bands + 1U) * 4U) == nil ||
                    sixel_body_gpu_buffer(context, BODY_BYTES,
                        byte_bound) == nil ||
                    sixel_body_gpu_buffer(context, BODY_KEYS,
                        (size_t)params.bands * emit.group_limit * 8U)
                        == nil ||
                    sixel_body_gpu_buffer(context, BODY_KEY_SLOTS,
                        (size_t)params.bands * emit.group_limit * 2U)
                        == nil) {
                return SIXEL_BAD_ALLOCATION;
            }
            memcpy([(__bridge id<MTLBuffer>)
                    context->buffers[BODY_BAND_FIRST] contents],
                context->band_first, ((size_t)params.bands + 1U) * 4U);
            memcpy([(__bridge id<MTLBuffer>)
                    context->buffers[BODY_REGISTERS] contents],
                request->registers, (size_t)params.ncolors * 4U);
        }

        sixel_trace_topic_message("metal_body",
            "%.6f map-emit encode spell=%d group_limit=%u",
            sixel_timer_monotonic(), spell,
            spell ? (unsigned int)emit.group_limit : 0U);
        command = [queue commandBuffer];
        encoder = [command computeCommandEncoder];
        sixel_body_gpu_encode_spans(encoder, context, &params, 1U,
            indexes, mask);
        sixel_body_gpu_encode_map(encoder, context, &params, indexes, mask);
        if (spell) {
            sixel_body_gpu_encode_emit(encoder, context, &emit, NULL);
        }
        [encoder endEncoding];
        sixel_body_gpu_submit(command, "map-emit");
        sixel_gpu_statistics_record_metal(command);
        if ([command status] != MTLCommandBufferStatusCompleted) {
            return SIXEL_RUNTIME_ERROR;
        }
        result->nodes = (sixel_body_gpu_node_t const *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_NODES] contents];
        result->map = (unsigned char *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_MAP] contents];
        result->band_first = context->band_first;
        result->first_seen = (uint32_t const *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_FIRST_SEEN] contents];
        result->ncolors = (int)params.ncolors;
        result->band_count = (int)params.bands;
        result->bytes = spell ? (unsigned char const *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_BYTES] contents] : NULL;
        result->band_bytes = spell ? (uint32_t const *)[(__bridge id<MTLBuffer>)
            context->buffers[BODY_BAND_BYTES] contents] : NULL;
    }
    return SIXEL_OK;
}
static void
sixel_body_metal_bind(id<MTLComputeCommandEncoder> encoder,
    sixel_body_gpu_t *context, int stage, int const *slots, NSUInteger count)
{
    NSUInteger i;

    [encoder setComputePipelineState:(__bridge id<MTLComputePipelineState>)
        context->pipelines[stage]];
    for (i = 0; i < count; ++i) {
        [encoder setBuffer:(__bridge id<MTLBuffer>)context->buffers[slots[i]]
                    offset:0 atIndex:i];
    }
}

/* Full-origin, unmasked RGB legacy output. The compact GPU dispatch can
 * use bounded allocations without reading the number of spans first. */
SIXELSTATUS
sixel_body_metal_record(sixel_body_gpu_t *context, id<MTLCommandBuffer> command,
    id<MTLBuffer> indices, sixel_body_metal_palette_t const *palette,
    unsigned int width, unsigned int height, int limited)
{
    static int const local[] = { BODY_COUNTS, BODY_WIDTHS, BODY_NODE_FIRST,
        BODY_MAP_FIRST, BODY_BAND_FIRST, BODY_MAP_BANDS };
    static int const prefix[] = { BODY_BAND_FIRST, BODY_MAP_BANDS,
        BODY_SHAPE, BODY_EMIT_PARAMS, BODY_GRID };
    static int const offsets[] = { BODY_NODE_FIRST, BODY_MAP_FIRST,
        BODY_BAND_FIRST, BODY_MAP_BANDS, BODY_REGISTERS };
    sixel_body_gpu_params_t p;
    sixel_body_gpu_emit_params_t emit;
    id<MTLBlitCommandEncoder> blit;
    id<MTLComputeCommandEncoder> encoder;
    id<MTLBuffer> mask;
    size_t lengths[BODY_SLOTS];
    size_t pixels, pairs, map_bound, byte_bound, gap_bound, t;
    unsigned int i;

    if (!width || !height || width > 2048u || height > 2048u ||
            width * height > 1024u * 1024u ||
            indices.device != (__bridge id<MTLDevice>)context->device)
        return SIXEL_FALSE;
    p = (sixel_body_gpu_params_t){width, height, width, height, 0, 0,
        SIXEL_PALETTE_MAX, -1, (width + 31u) / 32u, (height + 5u) / 6u,
        0, 0};
    emit = (sixel_body_gpu_emit_params_t){width, p.words, p.ncolors,
        p.bands, 1u, limited != 0, 0u, context->group_limit};
    pixels = (size_t)width * height;
    pairs = (size_t)p.bands * p.ncolors;
    /* Every span contains a painted column, and joining a gap adds at
     * most nine empty columns per additional painted column. */
    map_bound = 10u * pixels;
    gap_bound = limited ? 5u * (width / 255u) + 2u : 2u;
    for (t = width; t > 0; t /= 10u)
        ++gap_bound;
    byte_bound = p.bands + map_bound + pixels * (5u + gap_bound) +
        18u * p.ncolors;
    memcpy(lengths, (size_t[BODY_SLOTS]){
        [BODY_MASK] = 16u,
        [BODY_OCCUPANCY] = pairs * p.words * 4u,
        [BODY_COUNTS] = pairs * 4u,
        [BODY_WIDTHS] = pairs * 4u,
        [BODY_NODE_FIRST] = pairs * 4u,
        [BODY_MAP_FIRST] = pairs * 4u,
        [BODY_NODES] = pixels * sizeof(sixel_body_gpu_node_t),
        [BODY_NODE_BAND] = pixels * 4u,
        [BODY_MAP] = map_bound,
        [BODY_FIRST_SEEN] = pairs * 4u,
        [BODY_BAND_FIRST] = (p.bands + 1u) * 4u,
        [BODY_LENGTHS] = pixels * 4u,
        [BODY_REGISTERS] = p.ncolors * 4u,
        [BODY_COLUMNS] = p.bands * (2u * width + 1u) * 4u,
        [BODY_BITS] = p.bands * p.words * 4u,
        [BODY_SORTED] = pixels * 4u,
        [BODY_ENTRIES] = pixels * 16u,
        [BODY_BAND_SIZE] = p.bands * 4u,
        [BODY_BAND_BYTES] = (p.bands + 1u) * 4u,
        [BODY_BYTES] = byte_bound,
        [BODY_KEYS] = (size_t)p.bands * emit.group_limit * 8u,
        [BODY_KEY_SLOTS] = (size_t)p.bands * emit.group_limit * 2u,
        [BODY_MAP_BANDS] = p.bands * 4u,
        [BODY_SHAPE] = sizeof(p),
        [BODY_EMIT_PARAMS] = sizeof(emit),
        [BODY_GRID] = sizeof(MTLDispatchThreadgroupsIndirectArguments),
        [BODY_BYTE_COUNT] = 4u
    }, sizeof(lengths));
    if (emit.group_limit == 0u)
        lengths[BODY_KEYS] = lengths[BODY_KEY_SLOTS] = 16u;
    for (i = BODY_MASK; i < BODY_SLOTS; ++i) {
        if (sixel_body_gpu_buffer(context, i, lengths[i]) == nil)
            return SIXEL_BAD_ALLOCATION;
    }
    mask = (__bridge id<MTLBuffer>)context->buffers[BODY_MASK];
    blit = [command blitCommandEncoder];
    if (blit == nil)
        return SIXEL_RUNTIME_ERROR;
    [blit fillBuffer:(__bridge id<MTLBuffer>)context->buffers[BODY_OCCUPANCY]
        range:NSMakeRange(0, lengths[BODY_OCCUPANCY]) value:0];
    [blit endEncoding];
    encoder = [command computeCommandEncoder];
    if (encoder == nil)
        return SIXEL_RUNTIME_ERROR;
    sixel_body_metal_classify(encoder, context, &p, indices, mask);
    sixel_body_gpu_encode_spans(encoder, context, &p, 0u, indices, mask);

    sixel_body_metal_bind(encoder, context, BODY_RESIDENT_LOCAL, local, 6);
    [encoder setBytes:&p length:sizeof(p) atIndex:6];
    [encoder dispatchThreads:MTLSizeMake(p.bands, 1, 1)
        threadsPerThreadgroup:MTLSizeMake(32, 1, 1)];
    sixel_body_metal_bind(encoder, context, BODY_RESIDENT_PREFIX, prefix, 5);
    [encoder setBytes:&p length:sizeof(p) atIndex:5];
    [encoder setBytes:&emit length:sizeof(emit) atIndex:6];
    [encoder dispatchThreads:MTLSizeMake(1, 1, 1)
        threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
    sixel_body_metal_bind(encoder, context, BODY_RESIDENT_OFFSETS, offsets, 5);
    [encoder setBytes:&p length:sizeof(p) atIndex:5];
    [encoder dispatchThreads:MTLSizeMake(pairs, 1, 1)
        threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
    sixel_body_gpu_encode_spans(encoder, context, &p, 1u, indices, mask);
    [encoder endEncoding];

    encoder = [command computeCommandEncoder];
    if (encoder == nil)
        return SIXEL_RUNTIME_ERROR;
    sixel_body_gpu_encode_map(encoder, context, &p, indices, mask);
    sixel_body_gpu_encode_emit(encoder, context, &emit, palette);
    [encoder endEncoding];
    return SIXEL_OK;
}

SIXELSTATUS
sixel_body_metal_copy(sixel_body_gpu_t *context, unsigned char **bytes,
                      int *size)
{
    uint32_t count;
    id<MTLBuffer> data;

    count = *(uint32_t const *)[(__bridge id<MTLBuffer>)
        context->buffers[BODY_BYTE_COUNT] contents];
    data = (__bridge id<MTLBuffer>)context->buffers[BODY_BYTES];
    if (count == 0 || count > INT_MAX || count > data.length)
        return SIXEL_FALSE;
    *bytes = malloc(count);
    if (*bytes == NULL)
        return SIXEL_BAD_ALLOCATION;
    memcpy(*bytes, data.contents, count);
    *size = (int)count;
    return SIXEL_OK;
}

#endif /* HAVE_METAL */
