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
#include "filter-sample.h"
#include "gpu-program.h"
#include "gpu-runtime-metal.h"

static char const *const sample_solid_source[] = {
#include "generated-metal/filter-sample-gpu-shader.inc.h"
};

typedef struct sample_solid_params {
    int32_t width, height, clip_x, clip_y;
    uint32_t points, rows, tolerance;
} sample_solid_params_t;

/* One process-wide scanner; a caller that finds it busy scans on the CPU. */
static pthread_mutex_t g_sample_solid_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_sample_solid_state;
static id<MTLComputePipelineState> g_sample_solid_pipeline;
static id<MTLCommandQueue> g_sample_solid_queue;
static id<MTLBuffer> g_sample_solid_pixels;
static id<MTLBuffer> g_sample_solid_cells;

static int
sample_solid_prepare(void)
{
    sixel_gpu_device_interface_t *device;
    id<MTLDevice> native;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    NSMutableString *source;
    size_t chunk;

    if (g_sample_solid_state != 0)
        return g_sample_solid_state > 0;
    g_sample_solid_state = -1;
    if (sixel_gpu_device_acquire(&device) != SIXEL_OK)
        return 0;
    native = sixel_gpu_metal_device(device);
    if (native == nil)
        return 0;
    @autoreleasepool {
        source = [NSMutableString string];
        for (chunk = 0; chunk < sizeof(sample_solid_source) /
                sizeof(sample_solid_source[0]); ++chunk) {
            [source appendString:[NSString stringWithUTF8String:
                sample_solid_source[chunk]]];
        }
        library = [native newLibraryWithSource:source options:nil error:NULL];
        function = [library newFunctionWithName:@"sample_solid"];
        if (function != nil) {
            g_sample_solid_pipeline =
                [native newComputePipelineStateWithFunction:function
                    error:NULL];
        }
        [function release];
        [library release];
    }
    g_sample_solid_queue = [native newCommandQueue];
    g_sample_solid_cells = [native newBufferWithLength:
        (NSUInteger)SIXEL_SAMPLE_SOLID_CELLS * 5u * sizeof(uint32_t)
        options:MTLResourceStorageModeShared];
    if (g_sample_solid_pipeline == nil || g_sample_solid_queue == nil ||
            g_sample_solid_cells == nil ||
            [g_sample_solid_pipeline threadExecutionWidth] < 4)
        return 0;
    g_sample_solid_state = 1;
    return 1;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_filter_sample_solid_gpu(unsigned char const *pixels,
                              int width,
                              int height,
                              int clip_x,
                              int clip_y,
                              int clip_width,
                              int clip_height,
                              unsigned int *cells)
{
    sample_solid_params_t params;
    id<MTLCommandBuffer> command;
    id<MTLComputeCommandEncoder> compute;
    id<MTLDevice> native;
    NSUInteger length;
    NSUInteger simd;
    NSUInteger lanes;
    SIXELSTATUS status;

    if (pixels == NULL || cells == NULL || width <= 0 || height <= 0
            || clip_width <= 0 || clip_height <= 0
            || (size_t)width * (size_t)height > UINT32_MAX / 3u)
        return SIXEL_BAD_ARGUMENT;
    if (pthread_mutex_trylock(&g_sample_solid_lock) != 0)
        return SIXEL_FEATURE_ERROR;
    status = SIXEL_FEATURE_ERROR;
    if (!sample_solid_prepare())
        goto end;
    length = (NSUInteger)width * (NSUInteger)height * 3u;
    @autoreleasepool {
        if (g_sample_solid_pixels == nil
                || [g_sample_solid_pixels length] < length) {
            native = [g_sample_solid_queue device];
            [g_sample_solid_pixels release];
            g_sample_solid_pixels = [native newBufferWithLength:length
                options:MTLResourceStorageModeShared];
            if (g_sample_solid_pixels == nil)
                goto end;
        }
        memcpy([g_sample_solid_pixels contents], pixels, length);
        memset([g_sample_solid_cells contents], 0,
               (size_t)SIXEL_SAMPLE_SOLID_CELLS * 5u * sizeof(uint32_t));
        params.width = width;
        params.height = height;
        params.clip_x = clip_x;
        params.clip_y = clip_y;
        params.points = (uint32_t)(1 + (clip_width - 1)
            / SIXEL_SAMPLE_SOLID_STRIDE);
        params.rows = (uint32_t)(1 + (clip_height - 1)
            / SIXEL_SAMPLE_SOLID_STRIDE);
        params.tolerance = SIXEL_SAMPLE_SOLID_TOL;
        command = [g_sample_solid_queue commandBuffer];
        compute = [command computeCommandEncoder];
        if (compute == nil)
            goto end;
        simd = [g_sample_solid_pipeline threadExecutionWidth];
        lanes = [g_sample_solid_pipeline maxTotalThreadsPerThreadgroup];
        if (lanes > simd * 8u)
            lanes = simd * 8u;
        [compute setComputePipelineState:g_sample_solid_pipeline];
        [compute setBuffer:g_sample_solid_pixels offset:0 atIndex:0];
        [compute setBuffer:g_sample_solid_cells offset:0 atIndex:1];
        [compute setBytes:&params length:sizeof(params) atIndex:2];
        [compute dispatchThreads:MTLSizeMake(params.points, params.rows, 1)
            threadsPerThreadgroup:MTLSizeMake(simd, lanes / simd, 1)];
        [compute endEncoding];
        [command commit];
        [command waitUntilCompleted];
        sixel_gpu_statistics_record_metal(command);
        if ([command status] != MTLCommandBufferStatusCompleted)
            goto end;
        memcpy(cells, [g_sample_solid_cells contents],
               (size_t)SIXEL_SAMPLE_SOLID_CELLS * 5u * sizeof(uint32_t));
        status = SIXEL_OK;
    }
end:
    pthread_mutex_unlock(&g_sample_solid_lock);
    return status;
}
#endif
