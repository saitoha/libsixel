/* SPDX-License-Identifier: MIT */

/* The parent's Metal mutex guards this 6bit-only immutable table.
 * Every cell has a deterministic representative, unlike CPU memoization.
 * Publish identity only after the build and application command completes. */
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_lookup6 = { nil, 0U };
static id<MTLComputePipelineState> g_sixel_gpu_lookup_6bit_build = nil;
static unsigned char g_sixel_gpu_lookup6_palette[SIXEL_PALETTE_MAX * 3];
static int g_sixel_gpu_lookup6_colors;

static char const * const g_sixel_gpu_lookup6_source[] = {
#include <generated-metal/lookup-policy-6bit-gpu-shader.inc.h>
NULL
};

static int
sixel_gpu_lookup6_prepare(void)
{
    NSMutableString *source;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    NSError *error;

    if (g_sixel_gpu_lookup_6bit_build != nil) return 1;
    source = [[NSMutableString alloc] init];
    if (source == nil) return 0;
    if (!sixel_gpu_palette_metal_append_chunks(source,
            g_sixel_gpu_lookup6_source)) {
        [source release];
        return 0;
    }
    error = nil;
    library = [g_sixel_gpu_metal_device newLibraryWithSource:source
        options:nil error:&error];
    [source release];
    if (library == nil) {
        sixel_gpu_palette_metal_set_message(
            "gpu 6bit lookup: Metal compilation failed", error);
        return 0;
    }
    function = [library newFunctionWithName:@"lookup_6bit_build"];
    if (function != nil) {
        g_sixel_gpu_lookup_6bit_build = [g_sixel_gpu_metal_device
            newComputePipelineStateWithFunction:function error:&error];
        [function release];
    }
    [library release];
    return g_sixel_gpu_lookup_6bit_build != nil;
}

static SIXELSTATUS
sixel_gpu_lookup6_encode(id<MTLCommandBuffer> command,
    id<MTLBuffer> palette, id<MTLBuffer> table, int colors)
{
    id<MTLComputeCommandEncoder> encoder;
    uint32_t palette_count;
    NSUInteger width, count;

    encoder = [command computeCommandEncoder];
    if (encoder == nil) return SIXEL_RUNTIME_ERROR;
    palette_count = (uint32_t)colors;
    count = 262144U;
    width = [g_sixel_gpu_lookup_6bit_build maxTotalThreadsPerThreadgroup];
    if (width > 128U) width = 128U;
    if (width == 0U) width = 1U;
    [encoder setComputePipelineState:g_sixel_gpu_lookup_6bit_build];
    [encoder setBuffer:table offset:0 atIndex:0];
    [encoder setBuffer:palette offset:0 atIndex:1];
    [encoder setBytes:&palette_count length:sizeof(palette_count) atIndex:2];
    [encoder dispatchThreadgroups:MTLSizeMake((count + width - 1U) / width,
        1U, 1U) threadsPerThreadgroup:MTLSizeMake(width, 1U, 1U)];
    [encoder endEncoding];
    return SIXEL_OK;
}

static char const *const lookup6_metal_chunks[] = {
#include <generated-metal/lookup-policy-6bit-gpu-query-shader.inc.h>
#include <generated-metal/lookup-policy-6bit-gpu-apply-shader.inc.h>
NULL
};

static SIXELSTATUS
lookup6_metal_prepare(gpu_metal_lookup_run_t *run,
    sixel_gpu_palette_request_t const *request,
    sixel_gpu_metal_params_t *params)
{
    SIXELSTATUS status;

    (void)params;
    if (!sixel_gpu_lookup6_prepare()) {
        status = SIXEL_FEATURE_ERROR;
        return status;
    }
    run->buffers[5] = sixel_gpu_metal_ensure_buffer(
        &g_sixel_gpu_lookup6, 262144U);
    run->rebuild =
        g_sixel_gpu_lookup6_colors != request->ncolors ||
        memcmp(g_sixel_gpu_lookup6_palette, request->palette,
            (size_t)request->ncolors * 3U) != 0;
    if (request->test_hooks != NULL) {
        request->test_hooks->lookup_table_bytes = 262144U;
    }
    run->kernel = "6bit";
    run->kernel_id = 8;
    if (run->buffers[5] == nil)
        return SIXEL_BAD_ALLOCATION;
    if (sixel_trace_topic_is_enabled("gpu_contract"))
        fprintf(stderr, "LSXGPU3|lookup=6bit|table=%s|shared=1\n",
            run->rebuild ? "rebuild" : "reuse");
    return SIXEL_OK;
}

static SIXELSTATUS
lookup6_metal_build(gpu_metal_lookup_run_t *run,
    id<MTLCommandBuffer> command, id<MTLBuffer> palette, id<MTLBuffer> params,
    sixel_gpu_palette_request_t const *request)
{
    (void)params;
    return sixel_gpu_lookup6_encode(command, palette, run->buffers[5],
        request->ncolors);
}

/* Identity publication follows successful GPU completion, including every
 * consumer range. A failed rebuild must invalidate the overwritten table. */
static void
lookup6_metal_finish(gpu_metal_lookup_run_t *run,
    sixel_gpu_palette_request_t const *request, SIXELSTATUS status)
{
    if (!run->rebuild) return;
    g_sixel_gpu_lookup6_colors = status == SIXEL_OK ? request->ncolors : 0;
    if (status != SIXEL_OK) return;
    memcpy(g_sixel_gpu_lookup6_palette, request->palette,
        (size_t)request->ncolors * 3U);
}

static gpu_metal_lookup_t const lookup6_metal = {
    lookup6_metal_chunks, "lookup_6bit_apply",
    lookup6_metal_prepare, lookup6_metal_build, lookup6_metal_finish
};

gpu_metal_lookup_t const *
sixel_lookup_6bit_gpu_metal(void)
{
    return &lookup6_metal;
}
