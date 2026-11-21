/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_METAL)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include "palette-heckbert-gpu.h"
#include "gpu-runtime-metal.h"

static char const * const heckbert_gpu_chunks[] = {
#include <generated-metal/palette-heckbert-gpu-shader.inc.h>
};

typedef struct heckbert_gpu_params {
    uint32_t count;
    uint32_t colors;
    uint32_t total;
    float weight0;
    float weight1;
    float weight2;
} heckbert_gpu_params_t;

typedef struct heckbert_gpu_box {
    uint32_t start;
    uint32_t size;
    uint32_t sum;
    float priority;
} heckbert_gpu_box_t;

enum {
    HECKBERT_GPU_POINTS,
    HECKBERT_GPU_ORDER,
    HECKBERT_GPU_SCRATCH,
    HECKBERT_GPU_BOXES,
    HECKBERT_GPU_RESULT,
    HECKBERT_GPU_BUFFERS
};

/* One solve at a time: the program and the grown buffers are shared. */
static pthread_mutex_t heckbert_gpu_lock = PTHREAD_MUTEX_INITIALIZER;
static id<MTLDevice> heckbert_gpu_device = nil;
static id<MTLCommandQueue> heckbert_gpu_queue = nil;
static id<MTLComputePipelineState> heckbert_gpu_pipeline = nil;
static id<MTLBuffer> heckbert_gpu_buffers[HECKBERT_GPU_BUFFERS];
static size_t heckbert_gpu_capacity[HECKBERT_GPU_BUFFERS];
static int heckbert_gpu_state;  /* 0 unprepared, 1 ready, -1 unavailable */

static int
heckbert_gpu_prepare(void)
{
    NSMutableString *source;
    NSString *chunk;
    MTLCompileOptions *options;
    NSError *error;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    size_t index;

    if (heckbert_gpu_state != 0) {
        return heckbert_gpu_state > 0;
    }
    heckbert_gpu_state = -1;
    heckbert_gpu_device = MTLCreateSystemDefaultDevice();
    if (heckbert_gpu_device == nil) {
        return 0;
    }
    heckbert_gpu_queue = [heckbert_gpu_device newCommandQueue];
    if (heckbert_gpu_queue == nil) {
        return 0;
    }
    source = [[NSMutableString alloc] init];
    for (index = 0u; index < sizeof(heckbert_gpu_chunks) /
             sizeof(heckbert_gpu_chunks[0]); ++index) {
        chunk = [[NSString alloc]
            initWithUTF8String:heckbert_gpu_chunks[index]];
        [source appendString:chunk];
        [chunk release];
    }
    options = [[MTLCompileOptions alloc] init];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    options.fastMathEnabled = NO;
#pragma clang diagnostic pop
    error = nil;
    library = [heckbert_gpu_device newLibraryWithSource:source
                                                options:options
                                                  error:&error];
    [options release];
    [source release];
    if (library == nil) {
        return 0;
    }
    function = [library newFunctionWithName:@"heckbert_solve"];
    [library release];
    if (function == nil) {
        return 0;
    }
    heckbert_gpu_pipeline = [heckbert_gpu_device
        newComputePipelineStateWithFunction:function error:&error];
    [function release];
    if (heckbert_gpu_pipeline == nil) {
        return 0;
    }
    heckbert_gpu_state = 1;
    return 1;
}

static id<MTLBuffer>
heckbert_gpu_buffer(unsigned int slot, size_t bytes)
{
    if (heckbert_gpu_capacity[slot] < bytes) {
        [heckbert_gpu_buffers[slot] release];
        heckbert_gpu_buffers[slot] = [heckbert_gpu_device
            newBufferWithLength:bytes
                        options:MTLResourceStorageModeShared];
        heckbert_gpu_capacity[slot] =
            heckbert_gpu_buffers[slot] != nil ? bytes : 0u;
    }
    return heckbert_gpu_buffers[slot];
}

/* Check the result before the caller uses it to index palette entries.
 * Scratch is no longer used by Metal after the command completes. */
static int
heckbert_gpu_result_valid(uint32_t const *order,
                          heckbert_gpu_box_t const *boxes,
                          uint32_t *scratch, uint32_t count,
                          uint32_t colors, uint32_t box_count)
{
    uint32_t i;
    uint32_t index;
    uint32_t start;

    if (box_count == 0u || box_count > colors) {
        return 0;
    }
    memset(scratch, 0, (size_t)count * sizeof(*scratch));
    for (i = 0u; i < count; ++i) {
        index = order[i];
        if (index >= count || scratch[index] != 0u) {
            return 0;
        }
        scratch[index] = 1u;
    }
    memset(scratch, 0, (size_t)count * sizeof(*scratch));
    for (i = 0u; i < box_count; ++i) {
        start = boxes[i].start;
        if (start >= count || boxes[i].size == 0u ||
                boxes[i].size > count - start || scratch[start] != 0u) {
            return 0;
        }
        scratch[start] = boxes[i].size;
    }
    start = 0u;
    for (i = 0u; i < box_count; ++i) {
        if (start >= count || scratch[start] == 0u) {
            return 0;
        }
        start += scratch[start];
    }
    return start == count;
}

SIXELSTATUS
sixel_palette_heckbert_gpu_metal_solve(
    sixel_heckbert_solve_request_t const *request)
{
    SIXELSTATUS status;
    heckbert_gpu_params_t params;
    heckbert_gpu_box_t const *boxes;
    float *points;
    uint32_t const *order;
    uint32_t *scratch;
    uint32_t *box_count;
    uint32_t total;
    uint32_t count;
    unsigned int index;
    NSUInteger threads;
    NSUInteger simd_width;
    id<MTLCommandBuffer> commands;
    id<MTLComputeCommandEncoder> encoder;

    count = request->count;
    total = 0u;
    for (index = 0u; index < count; ++index) {
        /* Populations above 2^24 lose integer steps in FP32. */
        if (request->weights[index] == 0u ||
                request->weights[index] > (1u << 24) - total) {
            return SIXEL_FEATURE_ERROR;
        }
        total += request->weights[index];
    }
    status = SIXEL_FEATURE_ERROR;
    pthread_mutex_lock(&heckbert_gpu_lock);
    @autoreleasepool { do {
        if (!heckbert_gpu_prepare()) {
            break;
        }
        /* The bin scan needs 256 lanes in complete SIMD groups. The
         * shader allocates scratch for at most 32 groups. */
        simd_width = heckbert_gpu_pipeline.threadExecutionWidth;
        if (simd_width == 0u || 256u % simd_width != 0u) {
            break;
        }
        threads = heckbert_gpu_pipeline.maxTotalThreadsPerThreadgroup;
        if (threads > 1024u) {
            threads = 1024u;
        }
        if (threads > 32u * simd_width) {
            threads = 32u * simd_width;
        }
        threads -= threads % simd_width;
        if (threads < 256u) {
            break;
        }
        if (heckbert_gpu_buffer(HECKBERT_GPU_POINTS,
                    (size_t)count * 4u * sizeof(float)) == nil ||
                heckbert_gpu_buffer(HECKBERT_GPU_ORDER,
                    (size_t)count * sizeof(uint32_t)) == nil ||
                heckbert_gpu_buffer(HECKBERT_GPU_SCRATCH,
                    (size_t)count * sizeof(uint32_t)) == nil ||
                heckbert_gpu_buffer(HECKBERT_GPU_BOXES,
                    256u * sizeof(heckbert_gpu_box_t)) == nil ||
                heckbert_gpu_buffer(HECKBERT_GPU_RESULT,
                    sizeof(uint32_t)) == nil) {
            break;
        }
        points = (float *)[heckbert_gpu_buffers[HECKBERT_GPU_POINTS] contents];
        for (index = 0u; index < count; ++index) {
            points[index * 4u + 0u] = (float)request->tuples[index * 3u + 0u];
            points[index * 4u + 1u] = (float)request->tuples[index * 3u + 1u];
            points[index * 4u + 2u] = (float)request->tuples[index * 3u + 2u];
            points[index * 4u + 3u] = (float)request->weights[index];
        }
        params.count = count;
        params.colors = request->colors;
        params.total = total;
        params.weight0 = (float)request->axis_weights[0];
        params.weight1 = (float)request->axis_weights[1];
        params.weight2 = (float)request->axis_weights[2];
        box_count = (uint32_t *)
            [heckbert_gpu_buffers[HECKBERT_GPU_RESULT] contents];
        *box_count = 0u;
        commands = [heckbert_gpu_queue commandBuffer];
        encoder = [commands computeCommandEncoder];
        [encoder setComputePipelineState:heckbert_gpu_pipeline];
        [encoder setBuffer:heckbert_gpu_buffers[HECKBERT_GPU_POINTS]
                    offset:0 atIndex:0];
        [encoder setBuffer:heckbert_gpu_buffers[HECKBERT_GPU_ORDER]
                    offset:0 atIndex:1];
        [encoder setBuffer:heckbert_gpu_buffers[HECKBERT_GPU_SCRATCH]
                    offset:0 atIndex:2];
        [encoder setBuffer:heckbert_gpu_buffers[HECKBERT_GPU_BOXES]
                    offset:0 atIndex:3];
        [encoder setBytes:&params length:sizeof(params) atIndex:4];
        [encoder setBuffer:heckbert_gpu_buffers[HECKBERT_GPU_RESULT]
                    offset:0 atIndex:5];
        [encoder dispatchThreadgroups:MTLSizeMake(1, 1, 1)
                threadsPerThreadgroup:MTLSizeMake(threads, 1, 1)];
        [encoder endEncoding];
        [commands commit];
        [commands waitUntilCompleted];
        sixel_gpu_statistics_record_metal(commands);
        if (commands.status != MTLCommandBufferStatusCompleted) {
            break;
        }
        order = (uint32_t const *)
            [heckbert_gpu_buffers[HECKBERT_GPU_ORDER] contents];
        boxes = (heckbert_gpu_box_t const *)
            [heckbert_gpu_buffers[HECKBERT_GPU_BOXES] contents];
        scratch = (uint32_t *)
            [heckbert_gpu_buffers[HECKBERT_GPU_SCRATCH] contents];
        if (!heckbert_gpu_result_valid(order, boxes, scratch, count,
                request->colors, *box_count)) {
            break;
        }
        memcpy(request->order, order, (size_t)count * sizeof(uint32_t));
        *request->box_count = *box_count;
        for (index = 0u; index < *box_count; ++index) {
            request->box_start[index] = boxes[index].start;
            request->box_size[index] = boxes[index].size;
            request->box_sum[index] = boxes[index].sum;
            request->box_priority[index] = (double)boxes[index].priority;
        }
        status = SIXEL_OK;
    } while (0); }
    pthread_mutex_unlock(&heckbert_gpu_lock);
    return status;
}
#endif
