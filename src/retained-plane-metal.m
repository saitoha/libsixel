/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_METAL)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "gpu-runtime-metal.h"
#include "retained-plane-gpu.h"
#include "threading.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstring-concatenation"
#endif
static char const *const retained_metal_source[] = {
#include "generated-metal/retained-plane-gpu-shader.inc.h"
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

typedef struct retained_metal_params {
    uint32_t width, height, x, y, stride, count, record_paint;
    unsigned char palette[768];
} retained_metal_params_t;

struct sixel_retained_gpu {
    sixel_gpu_device_interface_t *device;
    sixel_retained_plane_description_t description;
    sixel_gpu_buffer_interface_t *rgb[2];
    sixel_gpu_buffer_interface_t *valid[2];
    unsigned char *rgb_map[2];
    unsigned char *valid_map[2];
    id<MTLBuffer> indexes;
    id<MTLBuffer> skip;
    id<MTLBuffer> paint;
    id<MTLBuffer> stats;
    id<MTLComputePipelineState> pipeline;
    id<MTLCommandQueue> queue;
    retained_metal_params_t params;
    NSUInteger threads;
    NSUInteger groups;
    int committed;
    uint32_t dirty_x, dirty_y, dirty_width, dirty_height;
};

SIXEL_INTERNAL_API void
sixel_retained_gpu_free(sixel_retained_gpu_t *gpu)
{
    int i;

    if (gpu == NULL) return;
    for (i = 0; i < 2; ++i) {
        if (gpu->rgb[i] != NULL) gpu->rgb[i]->vtbl->unref(gpu->rgb[i]);
        if (gpu->valid[i] != NULL) gpu->valid[i]->vtbl->unref(gpu->valid[i]);
    }
    [gpu->indexes release];
    [gpu->skip release];
    [gpu->paint release];
    [gpu->stats release];
    [gpu->pipeline release];
    [gpu->queue release];
    gpu->device->vtbl->unref(gpu->device);
    free(gpu);
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_new(sixel_gpu_device_interface_t *device,
    sixel_retained_plane_description_t const *description,
    unsigned char const *rgb, unsigned char const *valid,
    sixel_retained_gpu_t **result)
{
    sixel_retained_gpu_t *gpu;
    id<MTLDevice> native;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    NSError *error;
    NSMutableString *source;
    size_t chunk;
    void *memory;
    size_t pixels;
    NSUInteger simd;
    int i;
    SIXELSTATUS status;

    *result = NULL;
    native = sixel_gpu_metal_device(device);
    if (native == nil) return SIXEL_FEATURE_ERROR;
    pixels = (size_t)description->width * description->height;
    if (pixels > UINT32_MAX / 3 || pixels > [native maxBufferLength] / 3)
        return SIXEL_FEATURE_ERROR;
    gpu = calloc(1, sizeof(*gpu));
    if (gpu == NULL) return SIXEL_BAD_ALLOCATION;
    gpu->device = device;
    gpu->description = *description;
    device->vtbl->ref(device);
    status = SIXEL_BAD_ALLOCATION;
    @autoreleasepool {
        error = nil;
        source = [NSMutableString string];
        for (chunk = 0; chunk < sizeof(retained_metal_source) /
                sizeof(retained_metal_source[0]); ++chunk) {
            [source appendString:[NSString stringWithUTF8String:
                retained_metal_source[chunk]]];
        }
        library = [native newLibraryWithSource:source
            options:nil error:&error];
        function = [library newFunctionWithName:@"retained_expand"];
        if (function != nil) {
            gpu->pipeline = [native newComputePipelineStateWithFunction:function
                error:&error];
        }
        [function release];
        [library release];
        if (gpu->pipeline == nil) {
            sixel_helper_set_additional_message(
                "retained plane: Metal kernel preparation failed");
            status = SIXEL_FEATURE_ERROR;
            goto fail;
        }
        simd = [gpu->pipeline threadExecutionWidth];
        gpu->threads = [gpu->pipeline maxTotalThreadsPerThreadgroup];
        if (gpu->threads > 256) gpu->threads = 256;
        if (gpu->threads > simd * simd) gpu->threads = simd * simd;
        if (simd == 0 || simd > 32) {
            status = SIXEL_FEATURE_ERROR;
            goto fail;
        }
        gpu->threads -= gpu->threads % simd;
        if (gpu->threads == 0) {
            status = SIXEL_FEATURE_ERROR;
            goto fail;
        }
        gpu->groups = (pixels + gpu->threads - 1) / gpu->threads;
        gpu->queue = [native newCommandQueue];
        gpu->indexes = [native newBufferWithLength:pixels
            options:MTLResourceStorageModeShared];
        gpu->skip = [native newBufferWithLength:pixels
            options:MTLResourceStorageModeShared];
        gpu->paint = [native newBufferWithLength:1
            options:MTLResourceStorageModeShared];
        gpu->stats = [native newBufferWithLength:gpu->groups * 8
            options:MTLResourceStorageModeShared];
        if (gpu->queue == nil || gpu->indexes == nil || gpu->skip == nil ||
                gpu->paint == nil || gpu->stats == nil) goto fail;
        for (i = 0; i < 2; ++i) {
            status = device->vtbl->create_buffer(device,
                (pixels * 3 + 3) & ~(size_t)3, &gpu->rgb[i]);
            if (status != SIXEL_OK) goto fail;
            status = gpu->rgb[i]->vtbl->shared_memory(gpu->rgb[i], &memory);
            if (status != SIXEL_OK) goto fail;
            gpu->rgb_map[i] = memory;
            status = device->vtbl->create_buffer(device,
                (pixels + 3) & ~(size_t)3, &gpu->valid[i]);
            if (status != SIXEL_OK) goto fail;
            status = gpu->valid[i]->vtbl->shared_memory(gpu->valid[i], &memory);
            if (status != SIXEL_OK) goto fail;
            gpu->valid_map[i] = memory;
        }
        for (i = 0; i < 2; ++i) {
            memset(gpu->rgb_map[i], 0, (pixels * 3 + 3) & ~(size_t)3);
            memset(gpu->valid_map[i], 0, (pixels + 3) & ~(size_t)3);
        }
        memcpy(gpu->rgb_map[0], rgb, pixels * 3);
        memcpy(gpu->valid_map[0], valid, pixels);
        memcpy(gpu->rgb_map[1], rgb, pixels * 3);
        memcpy(gpu->valid_map[1], valid, pixels);
        *result = gpu;
        return SIXEL_OK;
    fail:
        sixel_retained_gpu_free(gpu);
        return status;
    }
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_read(sixel_retained_gpu_t *gpu,
    sixel_gpu_device_interface_t *device, sixel_retained_gpu_read_view_t *view)
{
    if (gpu == NULL || gpu->device != device) return SIXEL_FEATURE_ERROR;
    view->rgb = gpu->rgb[gpu->committed];
    view->valid = gpu->valid[gpu->committed];
    view->rgb_stride = (size_t)gpu->description.width * 3;
    view->valid_stride = (size_t)gpu->description.width;
    view->description = gpu->description;
    return SIXEL_OK;
}

SIXEL_INTERNAL_API void
sixel_retained_gpu_map(sixel_retained_gpu_t *gpu,
    unsigned char **rgb, unsigned char **valid)
{
    *rgb = gpu->rgb_map[gpu->committed];
    *valid = gpu->valid_map[gpu->committed];
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_prepare(sixel_retained_gpu_t *gpu,
    sixel_indexed_frame_description_t const *image,
    sixel_wire_palette_view_t const *palette, int record_paint)
{
    size_t pixels;
    size_t i;
    id<MTLBuffer> paint;

    pixels = (size_t)image->width * image->height;
    if (record_paint && [gpu->paint length] < pixels) {
        paint = [sixel_gpu_metal_device(gpu->device)
            newBufferWithLength:pixels options:MTLResourceStorageModeShared];
        if (paint == nil) return SIXEL_BAD_ALLOCATION;
        [gpu->paint release];
        gpu->paint = paint;
    }
    memset(&gpu->params, 0, sizeof(gpu->params));
    gpu->params.width = (uint32_t)image->width;
    gpu->params.height = (uint32_t)image->height;
    gpu->params.x = (uint32_t)image->origin_x;
    gpu->params.y = (uint32_t)image->origin_y;
    gpu->params.stride = (uint32_t)gpu->description.width;
    gpu->params.count = (uint32_t)pixels;
    gpu->groups = (pixels + gpu->threads - 1) / gpu->threads;
    gpu->params.record_paint = record_paint != 0;
    for (i = 0; i < palette->count; ++i)
        memcpy(gpu->params.palette + 3 * i, palette->colors[i].emitted_rgb, 3);
    return SIXEL_OK;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_retained_gpu_stage(sixel_retained_gpu_t *gpu,
    unsigned char const *indexes, unsigned char const *skip,
    unsigned char *paint, sixel_retained_update_stats_t *stats, int fault)
{
    id<MTLCommandBuffer> command;
    id<MTLComputeCommandEncoder> encoder;
    id<MTLBlitCommandEncoder> blit;
    NSUInteger offset;
    NSUInteger length;
    NSUInteger aligned;
    NSUInteger bytes;
    uint32_t right;
    uint32_t bottom;
    int covered;
    uint32_t const *counts;
    size_t pixels;
    size_t i;
    int current;
    SIXELSTATUS status;

    if (fault == 1) return SIXEL_RUNTIME_ERROR;
    current = gpu->committed;
    pixels = (size_t)gpu->params.width * gpu->params.height;
    memcpy([gpu->indexes contents], indexes, pixels);
    if (skip != NULL) memcpy([gpu->skip contents], skip, pixels);
    else memset([gpu->skip contents], 0, pixels);
    status = SIXEL_RUNTIME_ERROR;
    @autoreleasepool {
        command = [gpu->queue commandBuffer];
        if (command == nil) return status;
        covered = gpu->params.x <= gpu->dirty_x &&
            gpu->params.y <= gpu->dirty_y &&
            gpu->params.x + gpu->params.width >=
                gpu->dirty_x + gpu->dirty_width &&
            gpu->params.y + gpu->params.height >=
                gpu->dirty_y + gpu->dirty_height;
        if (gpu->dirty_height != 0 && !covered) {
            /* Synchronize only the stale span in the inactive allocation.
             * Inter-row gaps are safe to copy too: all are committed bytes.
             * Full-frame kernels cover every stale pixel and skip this copy. */
            offset = (NSUInteger)gpu->dirty_y * gpu->description.width +
                gpu->dirty_x;
            length = (NSUInteger)(gpu->dirty_height - 1) *
                gpu->description.width + gpu->dirty_width;
            blit = [command blitCommandEncoder];
            if (blit == nil) return status;
            /* macOS buffer blits require four-byte offsets and sizes. Owned
             * allocations include three spare bytes for this rounding. */
            aligned = (offset * 3) & ~(NSUInteger)3;
            bytes = ((offset + length) * 3 + 3) & ~(NSUInteger)3;
            [blit copyFromBuffer:sixel_gpu_metal_buffer(gpu->device,
                gpu->rgb[current]) sourceOffset:aligned
                toBuffer:sixel_gpu_metal_buffer(gpu->device,
                    gpu->rgb[1-current]) destinationOffset:aligned
                size:bytes - aligned];
            aligned = offset & ~(NSUInteger)3;
            bytes = (offset + length + 3) & ~(NSUInteger)3;
            [blit copyFromBuffer:sixel_gpu_metal_buffer(gpu->device,
                gpu->valid[current]) sourceOffset:aligned
                toBuffer:sixel_gpu_metal_buffer(gpu->device,
                    gpu->valid[1-current]) destinationOffset:aligned
                size:bytes - aligned];
            [blit endEncoding];
        }
        encoder = [command computeCommandEncoder];
        if (command == nil || encoder == nil) return status;
        [encoder setComputePipelineState:gpu->pipeline];
        [encoder setBuffer:sixel_gpu_metal_buffer(gpu->device,
            gpu->rgb[current]) offset:0 atIndex:0];
        [encoder setBuffer:sixel_gpu_metal_buffer(gpu->device,
            gpu->valid[current]) offset:0 atIndex:1];
        [encoder setBuffer:sixel_gpu_metal_buffer(gpu->device,
            gpu->rgb[1-current]) offset:0 atIndex:2];
        [encoder setBuffer:sixel_gpu_metal_buffer(gpu->device,
            gpu->valid[1-current]) offset:0 atIndex:3];
        [encoder setBuffer:gpu->indexes offset:0 atIndex:4];
        [encoder setBuffer:gpu->skip offset:0 atIndex:5];
        [encoder setBuffer:gpu->paint offset:0 atIndex:6];
        [encoder setBuffer:gpu->stats offset:0 atIndex:7];
        [encoder setBytes:&gpu->params length:sizeof(gpu->params) atIndex:8];
        [encoder dispatchThreadgroups:MTLSizeMake(gpu->groups, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(gpu->threads, 1, 1)];
        [encoder endEncoding];
        /* A failed command may have touched any encoded range. Keep the union
         * dirty until successful completion narrows it to this rectangle. */
        right = gpu->params.x + gpu->params.width;
        bottom = gpu->params.y + gpu->params.height;
        if (gpu->dirty_height == 0) {
            gpu->dirty_x = gpu->params.x;
            gpu->dirty_y = gpu->params.y;
        } else {
            if (right < gpu->dirty_x + gpu->dirty_width)
                right = gpu->dirty_x + gpu->dirty_width;
            if (bottom < gpu->dirty_y + gpu->dirty_height)
                bottom = gpu->dirty_y + gpu->dirty_height;
            if (gpu->dirty_x > gpu->params.x) gpu->dirty_x = gpu->params.x;
            if (gpu->dirty_y > gpu->params.y) gpu->dirty_y = gpu->params.y;
        }
        gpu->dirty_width = right - gpu->dirty_x;
        gpu->dirty_height = bottom - gpu->dirty_y;
        [command commit];
        sixel_thread_work_end();
        [command waitUntilCompleted];
        sixel_thread_work_begin();
        if ([command status] != MTLCommandBufferStatusCompleted || fault == 2)
            return status;
        gpu->dirty_x = gpu->params.x;
        gpu->dirty_y = gpu->params.y;
        gpu->dirty_width = gpu->params.width;
        gpu->dirty_height = gpu->params.height;
        memset(stats, 0, sizeof(*stats));
        counts = [gpu->stats contents];
        for (i = 0; i < gpu->groups; ++i) {
            stats->painted_pixels += counts[i * 2];
            stats->refinement_needed |= counts[i * 2 + 1] != 0;
        }
        if (paint != NULL) memcpy(paint, [gpu->paint contents], pixels);
        return SIXEL_OK;
    }
}

SIXEL_INTERNAL_API void
sixel_retained_gpu_cpu_write(sixel_retained_gpu_t *gpu)
{
    if (gpu == NULL) return;
    gpu->dirty_x = gpu->dirty_y = 0;
    gpu->dirty_width = (uint32_t)gpu->description.width;
    gpu->dirty_height = (uint32_t)gpu->description.height;
}

SIXEL_INTERNAL_API void
sixel_retained_gpu_commit(sixel_retained_gpu_t *gpu)
{
    gpu->committed = 1 - gpu->committed;
}
#endif
