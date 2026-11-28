/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_METAL)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "gpu-ycbcr.h"
#include "gpu-runtime-metal.h"

static char const *const ycbcr_metal_source[] = {
#include "generated-metal/gpu-ycbcr-shader.inc.h"
};

typedef struct sixel_ycbcr_metal_state {
    id<MTLCommandQueue> queue;
    id<MTLComputePipelineState> pipeline;
    id<MTLBuffer> input, output, taps;
} sixel_ycbcr_metal_state_t;

static void
sixel_ycbcr_metal_destroy(void *state, sixel_allocator_t *allocator)
{
    sixel_ycbcr_metal_state_t *c = state;

    [c->taps release];
    [c->output release];
    [c->input release];
    [c->pipeline release];
    [c->queue release];
    sixel_allocator_free(allocator, c);
}

static SIXELSTATUS
sixel_ycbcr_metal_create(void **out, sixel_ycbcr_gpu_plan_t const *plan,
    sixel_ycbcr_gpu_taps_t const *taps, sixel_allocator_t *allocator)
{
    sixel_ycbcr_metal_state_t *c;
    id<MTLDevice> device;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    NSMutableString *source;
    size_t i;
    SIXELSTATUS status = SIXEL_OK;

    device = MTLCreateSystemDefaultDevice();
    if (device == nil) {
        return SIXEL_NOT_IMPLEMENTED;
    }
    c = sixel_allocator_calloc(allocator, 1, sizeof(*c));
    if (c == NULL) {
        [device release];
        return SIXEL_BAD_ALLOCATION;
    }
    @autoreleasepool {
        source = [NSMutableString string];
        for (i = 0; i < sizeof(ycbcr_metal_source) /
                sizeof(ycbcr_metal_source[0]); ++i) {
            [source appendString:
                [NSString stringWithUTF8String:ycbcr_metal_source[i]]];
        }
        library = [device newLibraryWithSource:source options:nil error:NULL];
        function = [library newFunctionWithName:@"ycbcr_rgb8"];
        if (function != nil) {
            c->pipeline = [device newComputePipelineStateWithFunction:function
                                                               error:NULL];
        }
        [function release];
        [library release];
        if (c->pipeline == nil) {
            status = SIXEL_RUNTIME_ERROR;
        } else {
            c->queue = [device newCommandQueue];
            c->input = [device newBufferWithLength:plan->input_bytes
                options:MTLResourceStorageModeShared];
            c->output = [device newBufferWithLength:plan->output_bytes
                options:MTLResourceStorageModeShared];
            c->taps = [device newBufferWithBytes:taps length:plan->taps_bytes
                options:MTLResourceStorageModeShared];
            if (!c->queue || !c->input || !c->output || !c->taps) {
                status = SIXEL_BAD_ALLOCATION;
            }
        }
    }
    [device release];
    if (SIXEL_FAILED(status)) {
        sixel_ycbcr_metal_destroy(c, allocator);
        return status;
    }
    *out = c;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_ycbcr_metal_run(void *state, sixel_ycbcr_gpu_plan_t const *plan,
    sixel_ycbcr_gpu_params_t const *p, sixel_ycbcr_frame_view_t const *source,
    unsigned char *dst, size_t stride)
{
    sixel_ycbcr_metal_state_t *c = state;
    id<MTLCommandBuffer> command;
    id<MTLComputeCommandEncoder> compute;
    NSUInteger group, pixels;
    SIXELSTATUS status = SIXEL_RUNTIME_ERROR;

    (void)plan;
    sixel_ycbcr_gpu_upload([c->input contents], p, source);
    @autoreleasepool {
        command = [c->queue commandBuffer];
        compute = [command computeCommandEncoder];
        if (compute != nil) {
            [compute setComputePipelineState:c->pipeline];
            [compute setBuffer:c->input offset:0 atIndex:0];
            [compute setBuffer:c->output offset:0 atIndex:1];
            [compute setBuffer:c->taps offset:0 atIndex:2];
            [compute setBytes:p length:sizeof(*p) atIndex:3];
            pixels = (NSUInteger)p->width * p->height;
            group = [c->pipeline maxTotalThreadsPerThreadgroup];
            if (group > 256u) {
                group = 256u;
            }
            [compute dispatchThreads:MTLSizeMake(pixels, 1, 1)
                threadsPerThreadgroup:MTLSizeMake(group, 1, 1)];
            [compute endEncoding];
            [command commit];
            [command waitUntilCompleted];
            sixel_gpu_statistics_record_metal(command);
            if ([command status] == MTLCommandBufferStatusCompleted) {
                sixel_ycbcr_gpu_readback(dst, stride, [c->output contents], p);
                status = SIXEL_OK;
            }
        }
    }
    return status;
}

sixel_ycbcr_gpu_ops_t const sixel_ycbcr_gpu_metal = {
    sixel_ycbcr_metal_create, sixel_ycbcr_metal_destroy, sixel_ycbcr_metal_run
};
#endif
