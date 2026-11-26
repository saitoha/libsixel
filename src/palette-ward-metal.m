/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_METAL)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <pthread.h>
#include <string.h>

#include "gpu-runtime-metal.h"
#include "palette-common-merge.h"
#include "sixel_atomic.h"

static char const *const sixel_ward_metal_source[] = {
#include <generated-metal/palette-ward-shader.inc.h>
};

static pthread_mutex_t sixel_ward_metal_lock = PTHREAD_MUTEX_INITIALIZER;
static sixel_atomic_i32_t sixel_ward_metal_ready;
static id<MTLCommandQueue> sixel_ward_metal_queue;
static id<MTLComputePipelineState> sixel_ward_metal_rows;
static id<MTLComputePipelineState> sixel_ward_metal_merge;
static id<MTLBuffer> sixel_ward_metal_inputs;
static id<MTLBuffer> sixel_ward_metal_cache;
static id<MTLBuffer> sixel_ward_metal_output;

static int
sixel_ward_metal_prepare(void)
{
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLComputePipelineState> rows, merge;
    id<MTLBuffer> inputs, cache, output;
    MTLCompileOptions *options;
    id<MTLLibrary> library;
    id<MTLFunction> row_function, merge_function;
    NSMutableString *source;
    size_t i;

    if (sixel_ward_metal_queue != nil)
        return 1;
    device = MTLCreateSystemDefaultDevice();
    if (device == nil)
        return 0;
    source = [NSMutableString string];
    for (i = 0; i < sizeof(sixel_ward_metal_source) /
                         sizeof(sixel_ward_metal_source[0]); ++i)
        [source appendString:@(sixel_ward_metal_source[i])];
    options = [[MTLCompileOptions alloc] init];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    options.fastMathEnabled = NO;
#pragma clang diagnostic pop
    library = [device newLibraryWithSource:source options:options error:nil];
    [options release];
    row_function = [library newFunctionWithName:@"ward_rows"];
    merge_function = [library newFunctionWithName:@"ward_merge"];
    [library release];
    rows = row_function == nil ? nil :
        [device newComputePipelineStateWithFunction:row_function error:nil];
    merge = merge_function == nil ? nil :
        [device newComputePipelineStateWithFunction:merge_function error:nil];
    [row_function release];
    [merge_function release];
    queue = [device newCommandQueue];
    inputs = [device newBufferWithLength:SIXEL_WARD_MAX_CLUSTERS *
        4u * sizeof(float) options:MTLResourceStorageModeShared];
    cache = [device newBufferWithLength:SIXEL_WARD_MAX_CLUSTERS *
        2u * sizeof(uint32_t) options:MTLResourceStorageModePrivate];
    output = [device newBufferWithLength:SIXEL_WARD_MAX_CLUSTERS *
        sizeof(sixel_ward_pair_t) options:MTLResourceStorageModeShared];
    [device release];
    if (rows.threadExecutionWidth != 32 || merge.threadExecutionWidth != 32 ||
            queue == nil || inputs == nil || cache == nil || output == nil) {
        [rows release];
        [merge release];
        [queue release];
        [inputs release];
        [cache release];
        [output release];
        return 0;
    }

    sixel_ward_metal_rows = rows;
    sixel_ward_metal_merge = merge;
    sixel_ward_metal_inputs = inputs;
    sixel_ward_metal_cache = cache;
    sixel_ward_metal_output = output;
    sixel_ward_metal_queue = queue;
    sixel_atomic_store_relaxed_i32(&sixel_ward_metal_ready, 1);
    return 1;
}

int
sixel_ward_metal_is_ready(void)
{
    return sixel_atomic_load_relaxed_i32(&sixel_ward_metal_ready);
}

int
sixel_ward_metal_pairs(float const *input, unsigned int count,
    unsigned int merges, sixel_ward_pair_t *pairs)
{
    id<MTLCommandBuffer> command;
    id<MTLComputeCommandEncoder> encoder;
    uint32_t params[2];
    unsigned int lanes;
    int ok;

    lanes = (count + 31u) & ~31u;
    if (!count || count > SIXEL_WARD_MAX_CLUSTERS || merges >= count)
        return 0;
    params[0] = count;
    params[1] = merges;
    ok = 0;
    /* The gate covers both reusable buffers and the final pair readback. */
    (void)pthread_mutex_lock(&sixel_ward_metal_lock);
    @autoreleasepool {
        if (!sixel_ward_metal_prepare() ||
                lanes > sixel_ward_metal_merge.maxTotalThreadsPerThreadgroup)
            goto end;
        memcpy(sixel_ward_metal_inputs.contents, input,
            (size_t)count * 4u * sizeof(*input));
        /* An incomplete command must never validate old pairs from a
         * previous frame. The sentinel is outside the palette capacity. */
        memset(sixel_ward_metal_output.contents, 255, merges * sizeof(*pairs));
        command = [sixel_ward_metal_queue commandBuffer];
        encoder = [command computeCommandEncoder];
        if (encoder == nil)
            goto end;
        [encoder setComputePipelineState:sixel_ward_metal_rows];
        [encoder setBuffer:sixel_ward_metal_inputs offset:0 atIndex:0];
        [encoder setBuffer:sixel_ward_metal_cache offset:0 atIndex:1];
        [encoder setBytes:params length:sizeof(params) atIndex:2];
        [encoder dispatchThreadgroups:MTLSizeMake(count, 1, 1)
                 threadsPerThreadgroup:MTLSizeMake(32, 1, 1)];
        [encoder endEncoding];
        encoder = [command computeCommandEncoder];
        if (encoder == nil)
            goto end;
        [encoder setComputePipelineState:sixel_ward_metal_merge];
        [encoder setBuffer:sixel_ward_metal_inputs offset:0 atIndex:0];
        [encoder setBuffer:sixel_ward_metal_cache offset:0 atIndex:1];
        [encoder setBuffer:sixel_ward_metal_output offset:0 atIndex:3];
        [encoder setBytes:params length:sizeof(params) atIndex:2];
        [encoder dispatchThreadgroups:MTLSizeMake(1, 1, 1)
                 threadsPerThreadgroup:MTLSizeMake(lanes, 1, 1)];
        [encoder endEncoding];
        [command commit];
        [command waitUntilCompleted];
        sixel_gpu_statistics_record_metal(command);
        if (command.status != MTLCommandBufferStatusCompleted)
            goto end;
        memcpy(pairs, sixel_ward_metal_output.contents,
            merges * sizeof(*pairs));
        ok = 1;
end:
        (void)pthread_mutex_unlock(&sixel_ward_metal_lock);
    }
    return ok;
}
#endif
