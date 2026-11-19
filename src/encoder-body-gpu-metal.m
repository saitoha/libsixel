/* SPDX-License-Identifier: MIT */
/* Metal backend for encoder-body-gpu.h. */

#include "config.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "encoder-body-gpu.h"
#include "gpu-runtime-metal.h"

static NSString *const sixel_body_gpu_source = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct body_params {\n"
"  uint width; uint height; uint encoded_width; uint encoded_height;\n"
"  uint offset_left; uint offset_top; uint ncolors; int keycolor;\n"
"  uint words; uint bands; uint has_mask; uint pad;\n"
"};\n"
/* Mark each painted source pixel in its (band, register) column bitmap. */
"kernel void body_classify(device const uchar *indexes [[buffer(0)]],\n"
"                          device const uchar *mask [[buffer(1)]],\n"
"                          device atomic_uint *occupancy [[buffer(2)]],\n"
"                          constant body_params &p [[buffer(3)]],\n"
"                          uint2 g [[thread_position_in_grid]]) {\n"
"  if (g.x >= p.width || g.y >= p.height) return;\n"
"  uint i = g.y * p.width + g.x;\n"
"  if (p.has_mask != 0 && mask[i] != 0) return;\n"
"  uint c = indexes[i];\n"
"  if (c >= p.ncolors || (int)c == p.keycolor) return;\n"
"  uint x = g.x + p.offset_left;\n"
"  uint band = (g.y + p.offset_top) / 6;\n"
"  atomic_fetch_or_explicit(&occupancy[(band * p.ncolors + c) * p.words + x / 32],\n"
"                           1u << (x % 32), memory_order_relaxed);\n"
"}\n"
"static uint body_next(device const uint *o, uint words, uint limit, uint from) {\n"
"  uint w = from / 32;\n"
"  if (w >= words) return limit;\n"
"  uint bits = o[w] & (0xffffffffu << (from % 32));\n"
"  while (bits == 0) { if (++w >= words) return limit; bits = o[w]; }\n"
"  return min(w * 32 + ctz(bits), limit);\n"
"}\n"
/* One thread per (band, register).  The count pass sizes the spans and their
 * column values; the write pass stores them at the prefix offsets, together
 * with the register's first row-major position in the band. */
"kernel void body_spans(device const uint *occupancy [[buffer(0)]],\n"
"                       device uint *counts [[buffer(1)]],\n"
"                       device uint *widths [[buffer(2)]],\n"
"                       device const uint *node_first [[buffer(3)]],\n"
"                       device const uint *map_first [[buffer(4)]],\n"
"                       device uint4 *nodes [[buffer(5)]],\n"
"                       device uchar *map [[buffer(6)]],\n"
"                       device const uchar *indexes [[buffer(7)]],\n"
"                       device const uchar *mask [[buffer(8)]],\n"
"                       constant body_params &p [[buffer(9)]],\n"
"                       constant uint &write_pass [[buffer(10)]],\n"
"                       device uint *first_seen [[buffer(11)]],\n"
"                       uint t [[thread_position_in_grid]]) {\n"
"  if (t >= p.bands * p.ncolors) return;\n"
"  device const uint *o = occupancy + t * p.words;\n"
"  uint band = t / p.ncolors; uint c = t % p.ncolors;\n"
"  uint limit = p.encoded_width;\n"
"  uint n = 0; uint used = 0; uint first = 0xffffffffu;\n"
"  uint node_base = write_pass != 0 ? node_first[t] : 0;\n"
"  uint map_base = write_pass != 0 ? map_first[t] : 0;\n"
"  uint sx = body_next(o, p.words, limit, 0);\n"
"  while (sx < limit) {\n"
"    uint mx = sx + 1;\n"
"    uint nx = body_next(o, p.words, limit, mx);\n"
"    while (nx < limit && nx - mx <= 9) { mx = nx + 1; nx = body_next(o, p.words, limit, mx); }\n"
"    if (write_pass != 0) {\n"
"      nodes[node_base + n] = uint4(c, sx, mx, map_base + used);\n"
"      for (uint x = sx; x < mx; x++) {\n"
"        uint v = 0;\n"
"        int sxp = (int)x - (int)p.offset_left;\n"
"        if (sxp >= 0 && (uint)sxp < p.width) {\n"
"          for (uint r = 0; r < 6; r++) {\n"
"            int syp = (int)(band * 6 + r) - (int)p.offset_top;\n"
"            if (syp < 0 || (uint)syp >= p.height) continue;\n"
"            uint i = (uint)syp * p.width + (uint)sxp;\n"
"            if (indexes[i] == c && (p.has_mask == 0 || mask[i] == 0)) v |= 1u << r;\n"
"          }\n"
"        }\n"
"        if (v != 0) first = min(first, ctz(v) * p.encoded_width + x);\n"
"        map[map_base + used + (x - sx)] = (uchar)v;\n"
"      }\n"
"    }\n"
"    used += mx - sx; n++; sx = nx;\n"
"  }\n"
"  if (write_pass == 0) { counts[t] = n; widths[t] = used; }\n"
"  else first_seen[t] = first;\n"
"}\n";

typedef struct sixel_body_gpu_params {
    uint32_t width, height, encoded_width, encoded_height;
    uint32_t offset_left, offset_top, ncolors;
    int32_t keycolor;
    uint32_t words, bands, has_mask, pad;
} sixel_body_gpu_params_t;

struct sixel_body_gpu {
    void *device;           /* id<MTLDevice>, retained */
    void *queue;            /* id<MTLCommandQueue> */
    void *classify;         /* id<MTLComputePipelineState> */
    void *spans;
    void *buffers[10];      /* id<MTLBuffer>, grown on demand */
    size_t lengths[10];
    uint32_t *band_first;
    size_t band_first_count;
};

enum {
    BODY_INDEXES, BODY_MASK, BODY_OCCUPANCY, BODY_COUNTS, BODY_WIDTHS,
    BODY_NODE_FIRST, BODY_MAP_FIRST, BODY_NODES, BODY_MAP, BODY_FIRST_SEEN
};

static id<MTLBuffer>
sixel_body_gpu_buffer(sixel_body_gpu_t *context, int slot, size_t length)
{
    id<MTLBuffer> buffer;

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
     * time. Shared storage lets the CPU read the result in place. */
    length += length / 2;
    buffer = [(__bridge id<MTLDevice>)context->device
        newBufferWithLength:length options:MTLResourceStorageModeShared];
    if (buffer == nil) {
        return nil;
    }
    context->buffers[slot] = (void *)CFBridgingRetain(buffer);
    context->lengths[slot] = length;
    return buffer;
}

SIXELSTATUS
sixel_body_gpu_new(sixel_body_gpu_t **result)
{
    sixel_body_gpu_t *context;
    SIXELSTATUS status;

    *result = NULL;
    context = (sixel_body_gpu_t *)calloc(1, sizeof(*context));
    if (context == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    status = SIXEL_FEATURE_ERROR;
    @autoreleasepool {
        id<MTLDevice> device;
        id<MTLLibrary> library;
        id<MTLComputePipelineState> classify;
        id<MTLComputePipelineState> spans;
        id<MTLCommandQueue> queue;
        NSError *error;

        error = nil;
        device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            goto end;
        }
        library = [device newLibraryWithSource:sixel_body_gpu_source
                                       options:nil
                                         error:&error];
        if (library == nil) {
            goto end;
        }
        classify = [device newComputePipelineStateWithFunction:
            [library newFunctionWithName:@"body_classify"] error:&error];
        spans = [device newComputePipelineStateWithFunction:
            [library newFunctionWithName:@"body_spans"] error:&error];
        queue = [device newCommandQueue];
        if (classify == nil || spans == nil || queue == nil) {
            goto end;
        }
        context->device = (void *)CFBridgingRetain(device);
        context->queue = (void *)CFBridgingRetain(queue);
        context->classify = (void *)CFBridgingRetain(classify);
        context->spans = (void *)CFBridgingRetain(spans);
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
    if (context->spans != NULL) CFRelease(context->spans);
    if (context->classify != NULL) CFRelease(context->classify);
    if (context->queue != NULL) CFRelease(context->queue);
    if (context->device != NULL) CFRelease(context->device);
    free(context->band_first);
    free(context);
}

static void
sixel_body_gpu_encode_spans(id<MTLComputeCommandEncoder> encoder,
                            sixel_body_gpu_t *context,
                            sixel_body_gpu_params_t const *params,
                            uint32_t write_pass)
{
    static int const slots[] = {
        BODY_OCCUPANCY, BODY_COUNTS, BODY_WIDTHS, BODY_NODE_FIRST,
        BODY_MAP_FIRST, BODY_NODES, BODY_MAP, BODY_INDEXES, BODY_MASK
    };
    NSUInteger i;
    NSUInteger threads;
    NSUInteger group;
    id<MTLComputePipelineState> pipeline;

    pipeline = (__bridge id<MTLComputePipelineState>)context->spans;
    [encoder setComputePipelineState:pipeline];
    for (i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
        [encoder setBuffer:(__bridge id<MTLBuffer>)context->buffers[slots[i]]
                    offset:0 atIndex:i];
    }
    [encoder setBytes:params length:sizeof(*params) atIndex:9];
    [encoder setBytes:&write_pass length:sizeof(write_pass) atIndex:10];
    [encoder setBuffer:(__bridge id<MTLBuffer>)
        context->buffers[BODY_FIRST_SEEN] offset:0 atIndex:11];
    threads = (NSUInteger)params->bands * params->ncolors;
    group = [pipeline maxTotalThreadsPerThreadgroup];
    if (group > 256) group = 256;
    [encoder dispatchThreads:MTLSizeMake(threads, 1, 1)
       threadsPerThreadgroup:MTLSizeMake(group, 1, 1)];
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
        id<MTLComputePipelineState> classify;
        id<MTLBuffer> indexes;
        id<MTLBuffer> mask;

        indexes = sixel_body_gpu_buffer(context, BODY_INDEXES, pixels);
        mask = sixel_body_gpu_buffer(context, BODY_MASK,
            request->mask != NULL ? pixels : 16);
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
                sixel_body_gpu_buffer(context, BODY_NODES, 16) == nil ||
                sixel_body_gpu_buffer(context, BODY_MAP, 16) == nil) {
            return SIXEL_BAD_ALLOCATION;
        }
        memcpy([indexes contents], request->indexes, pixels);
        if (request->mask != NULL) {
            memcpy([mask contents], request->mask, pixels);
        }

        queue = (__bridge id<MTLCommandQueue>)context->queue;
        classify = (__bridge id<MTLComputePipelineState>)context->classify;
        command = [queue commandBuffer];
        blit = [command blitCommandEncoder];
        [blit fillBuffer:(__bridge id<MTLBuffer>)
                context->buffers[BODY_OCCUPANCY]
                   range:NSMakeRange(0, occupancy_words * 4U) value:0];
        [blit endEncoding];
        encoder = [command computeCommandEncoder];
        [encoder setComputePipelineState:classify];
        [encoder setBuffer:indexes offset:0 atIndex:0];
        [encoder setBuffer:mask offset:0 atIndex:1];
        [encoder setBuffer:(__bridge id<MTLBuffer>)
            context->buffers[BODY_OCCUPANCY] offset:0 atIndex:2];
        [encoder setBytes:&params length:sizeof(params) atIndex:3];
        [encoder dispatchThreads:MTLSizeMake(params.width, params.height, 1)
           threadsPerThreadgroup:MTLSizeMake(32, 8, 1)];
        sixel_body_gpu_encode_spans(encoder, context, &params, 0U);
        [encoder endEncoding];
        [command commit];
        [command waitUntilCompleted];
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
        if (sixel_body_gpu_buffer(context, BODY_NODES,
                    (size_t)nodes_total * sizeof(sixel_body_gpu_node_t))
                    == nil ||
                sixel_body_gpu_buffer(context, BODY_MAP,
                    (size_t)map_total + SIXEL_BODY_MAP_TAIL) == nil) {
            return SIXEL_BAD_ALLOCATION;
        }

        command = [queue commandBuffer];
        encoder = [command computeCommandEncoder];
        sixel_body_gpu_encode_spans(encoder, context, &params, 1U);
        [encoder endEncoding];
        [command commit];
        [command waitUntilCompleted];
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
    }
    return SIXEL_OK;
}
