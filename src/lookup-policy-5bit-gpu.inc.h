/* SPDX-License-Identifier: MIT */

/* The parent's Metal mutex guards this 5bit-only immutable table.
 * Unlike the CPU first-query memo, every cell has a deterministic representative. Publish
 * its identity only after the build and application command has completed. */
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_lookup5 = { nil, 0U };
static id<MTLComputePipelineState> g_sixel_gpu_lookup_5bit_build = nil;
static unsigned char g_sixel_gpu_lookup5_palette[SIXEL_PALETTE_MAX * 3];
static int g_sixel_gpu_lookup5_colors;

static char const * const g_sixel_gpu_lookup5_source[] = {
#include <generated-metal/lookup-policy-5bit-gpu-shader.inc.h>
NULL
};

static int
sixel_gpu_lookup5_prepare(void)
{
    NSMutableString *source;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    NSError *error;

    if (g_sixel_gpu_lookup_5bit_build != nil) return 1;
    source = [[NSMutableString alloc] init];
    if (source == nil) return 0;
    if (!sixel_gpu_palette_metal_append_chunks(source,
            g_sixel_gpu_lookup5_source)) {
        [source release];
        return 0;
    }
    error = nil;
    library = [g_sixel_gpu_metal_device newLibraryWithSource:source
        options:nil error:&error];
    [source release];
    if (library == nil) {
        sixel_gpu_palette_metal_set_message(
            "gpu 5bit lookup: Metal compilation failed", error);
        return 0;
    }
    function = [library newFunctionWithName:@"lookup_5bit_build"];
    if (function != nil) {
        g_sixel_gpu_lookup_5bit_build = [g_sixel_gpu_metal_device
            newComputePipelineStateWithFunction:function error:&error];
        [function release];
    }
    [library release];
    return g_sixel_gpu_lookup_5bit_build != nil;
}

static SIXELSTATUS
sixel_gpu_lookup5_encode(id<MTLCommandBuffer> command,
    id<MTLBuffer> palette, id<MTLBuffer> table, int colors)
{
    id<MTLComputeCommandEncoder> encoder;
    uint32_t palette_count;
    NSUInteger width, count;

    encoder = [command computeCommandEncoder];
    if (encoder == nil) return SIXEL_RUNTIME_ERROR;
    palette_count = (uint32_t)colors;
    count = 32768U;
    width = [g_sixel_gpu_lookup_5bit_build maxTotalThreadsPerThreadgroup];
    if (width > 128U) width = 128U;
    if (width == 0U) width = 1U;
    [encoder setComputePipelineState:g_sixel_gpu_lookup_5bit_build];
    [encoder setBuffer:table offset:0 atIndex:0];
    [encoder setBuffer:palette offset:0 atIndex:1];
    [encoder setBytes:&palette_count length:sizeof(palette_count) atIndex:2];
    [encoder dispatchThreadgroups:MTLSizeMake((count + width - 1U) / width,
        1U, 1U) threadsPerThreadgroup:MTLSizeMake(width, 1U, 1U)];
    [encoder endEncoding];
    return SIXEL_OK;
}
