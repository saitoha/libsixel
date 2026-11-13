/* SPDX-License-Identifier: MIT */

/* Delaunay has independent storage and identity: alternating lookup policies
 * must not reuse rows built for another palette or construction method. */
static id<MTLComputePipelineState> g_sixel_gpu_metal_delaunay_bounds = nil;
static id<MTLComputePipelineState> g_sixel_gpu_metal_delaunay_sort = nil;
static id<MTLComputePipelineState> g_sixel_gpu_metal_delaunay_seeds = nil;
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_metal_delaunay =
    { nil, 0U };
static unsigned char g_sixel_gpu_metal_delaunay_palette[SIXEL_PALETTE_MAX * 3];
static int g_sixel_gpu_metal_delaunay_colors = 0;

static char const *const delaunay_chunks[] = {
#include <generated-metal/lookup-policy-delaunay-gpu-shader.inc.h>
#include <generated-metal/lookup-policy-delaunay-gpu-apply-shader.inc.h>
NULL
};

static SIXELSTATUS
delaunay_metal_prepare(gpu_metal_lookup_run_t *run,
    sixel_gpu_palette_request_t const *request,
    sixel_gpu_metal_params_t *params)
{
    SIXELSTATUS status;
    id<MTLComputePipelineState> builders[3];
    NSUInteger sort_width;

    (void)params;
    g_sixel_gpu_metal_delaunay_bounds = gpu_metal_lookup_pipeline(
        delaunay_chunks, run->dither, "sixel_gpu_delaunay_bounds");
    g_sixel_gpu_metal_delaunay_sort = gpu_metal_lookup_pipeline(
        delaunay_chunks, run->dither, "sixel_gpu_delaunay_sort");
    g_sixel_gpu_metal_delaunay_seeds = gpu_metal_lookup_pipeline(
        delaunay_chunks, run->dither, "sixel_gpu_delaunay_seeds");
    builders[0] = g_sixel_gpu_metal_delaunay_bounds;
    builders[1] = g_sixel_gpu_metal_delaunay_sort;
    builders[2] = g_sixel_gpu_metal_delaunay_seeds;
    sort_width = 32U;
    while (sort_width < (NSUInteger)request->ncolors) {
        sort_width <<= 1;
    }
    /* Reductions assume 32-lane SIMD groups. Reject unavailable
     * pipelines before publishing results. A prepared indexed plan
     * reports this failure without retrying the frame on CPU. */
    if (builders[0] == nil || builders[1] == nil ||
            builders[2] == nil ||
            [builders[0] threadExecutionWidth] != 32U ||
            [builders[1] threadExecutionWidth] != 32U ||
            [builders[0] maxTotalThreadsPerThreadgroup] < 32U ||
            [builders[1] maxTotalThreadsPerThreadgroup] < sort_width) {
        sixel_helper_set_additional_message(
            "gpu palette apply: delaunay pipelines unavailable.");
        status = SIXEL_FEATURE_ERROR;
        return status;
    }
    run->buffers[5] = sixel_gpu_metal_ensure_buffer(
        &g_sixel_gpu_metal_delaunay, 137472U * sizeof(uint32_t));
    run->rebuild =
        g_sixel_gpu_metal_delaunay_colors != request->ncolors ||
        memcmp(g_sixel_gpu_metal_delaunay_palette, request->palette,
               (size_t)request->ncolors * 3U) != 0;
    run->kernel = "delaunay";
    run->kernel_id = 6;
    if (run->buffers[5] == nil)
        return SIXEL_BAD_ALLOCATION;
    if (sixel_trace_topic_is_enabled("gpu_contract"))
        fprintf(stderr, "LSXGPU3|lookup=delaunay|table=%s\n",
            run->rebuild ? "rebuild" : "reuse");
    return SIXEL_OK;
}

static SIXELSTATUS
delaunay_metal_build(gpu_metal_lookup_run_t *run,
    id<MTLCommandBuffer> command, id<MTLBuffer> palette, id<MTLBuffer> params,
    sixel_gpu_palette_request_t const *request)
{
    id<MTLComputeCommandEncoder> encoder;
    NSUInteger threads_per_group, groups;
    id<MTLComputePipelineState> builders[3];
    NSUInteger sort_width, build_stage;

    encoder = [command computeCommandEncoder];
    if (encoder == nil) return SIXEL_RUNTIME_ERROR;
    builders[0] = g_sixel_gpu_metal_delaunay_bounds;
    builders[1] = g_sixel_gpu_metal_delaunay_sort;
    builders[2] = g_sixel_gpu_metal_delaunay_seeds;
    sort_width = 32U;
    while (sort_width < (NSUInteger)request->ncolors) sort_width <<= 1;
    /* Bounds, compact sorted adjacency and seeds stay on the GPU.
     * Encoder boundaries establish tracked-buffer visibility; apply
     * follows in this command buffer with no intermediate wait. */
    for (build_stage = 0U; build_stage < 3U; ++build_stage) {
        [encoder setComputePipelineState:builders[build_stage]];
        [encoder setBuffer:palette offset:0 atIndex:2];
        [encoder setBuffer:params offset:0 atIndex:6];
        [encoder setBuffer:run->buffers[5] offset:0 atIndex:15];
        threads_per_group = build_stage == 0U ? 32U : sort_width;
        groups = (NSUInteger)request->ncolors;
        if (build_stage == 2U) {
            threads_per_group = [builders[2]
                maxTotalThreadsPerThreadgroup];
            if (threads_per_group > 128U) {
                threads_per_group = 128U;
            }
            groups = (4096U + threads_per_group - 1U)
                / threads_per_group;
        }
        [encoder dispatchThreadgroups:MTLSizeMake(groups, 1U, 1U)
            threadsPerThreadgroup:
                MTLSizeMake(threads_per_group, 1U, 1U)];
        [encoder endEncoding];
        encoder = [command computeCommandEncoder];
        if (encoder == nil) {
            return SIXEL_RUNTIME_ERROR;
        }
    }
    [encoder endEncoding];
    return SIXEL_OK;
}

/* Identity publication follows successful GPU completion, including every
 * consumer range. A failed rebuild must invalidate the overwritten table. */
static void
delaunay_metal_finish(gpu_metal_lookup_run_t *run,
    sixel_gpu_palette_request_t const *request, SIXELSTATUS status)
{
    if (!run->rebuild) return;
    g_sixel_gpu_metal_delaunay_colors =
        status == SIXEL_OK ? request->ncolors : 0;
    if (status != SIXEL_OK) return;
    memcpy(g_sixel_gpu_metal_delaunay_palette, request->palette,
        (size_t)request->ncolors * 3U);
}

static gpu_metal_lookup_t const delaunay_metal = {
    delaunay_chunks, "lookup_delaunay_apply",
    delaunay_metal_prepare, delaunay_metal_build, delaunay_metal_finish
};

gpu_metal_lookup_t const *
sixel_lookup_delaunay_gpu_metal(void)
{
    return &delaunay_metal;
}
