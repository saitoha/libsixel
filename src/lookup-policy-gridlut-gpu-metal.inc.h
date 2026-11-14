/* SPDX-License-Identifier: MIT */

/* The grid cache is protected by the same lock as all Metal dispatches.
 * Its palette identity is independent of the ordinary upload cache. */
static id<MTLComputePipelineState> g_sixel_gpu_metal_grid_build = nil;
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_metal_grid = { nil, 0U };
static unsigned char g_sixel_gpu_metal_grid_palette[SIXEL_PALETTE_MAX * 3];
static int g_sixel_gpu_metal_grid_colors = 0;

static char const *const gridlut_metal_chunks[] = {
#include <generated-metal/lookup-policy-gridlut-gpu-shader.inc.h>
#include <generated-metal/lookup-policy-gridlut-gpu-apply-shader.inc.h>
NULL
};

static SIXELSTATUS
gridlut_metal_prepare(gpu_metal_lookup_run_t *run,
    sixel_gpu_palette_request_t const *request,
    sixel_gpu_metal_params_t *params)
{
    SIXELSTATUS status;

    (void)params;
    g_sixel_gpu_metal_grid_build = gpu_metal_lookup_pipeline(
        gridlut_metal_chunks, run->dither, "sixel_gpu_grid_build");
    if (g_sixel_gpu_metal_grid_build == nil) {
        sixel_helper_set_additional_message(
            "gpu palette apply: gridlut pipeline is unavailable.");
        status = SIXEL_FEATURE_ERROR;
        return status;
    }
    run->buffers[4] = sixel_gpu_metal_ensure_buffer(
        &g_sixel_gpu_metal_grid, 4096U * 65U);
    run->rebuild = g_sixel_gpu_metal_grid_colors != request->ncolors
        || memcmp(g_sixel_gpu_metal_grid_palette, request->palette,
                  (size_t)request->ncolors * 3U) != 0;
    run->kernel = "gridlut";
    run->kernel_id = 4;
    if (run->buffers[4] == nil)
        return SIXEL_BAD_ALLOCATION;
    if (sixel_trace_topic_is_enabled("gpu_contract"))
        fprintf(stderr, "LSXGPU3|lookup=gridlut|table=%s\n",
            run->rebuild ? "rebuild" : "reuse");
    return SIXEL_OK;
}

static SIXELSTATUS
gridlut_metal_build(gpu_metal_lookup_run_t *run,
    id<MTLCommandBuffer> command, id<MTLBuffer> palette, id<MTLBuffer> params,
    sixel_gpu_palette_request_t const *request)
{
    id<MTLComputeCommandEncoder> encoder;
    NSUInteger threads_per_group, groups;

    encoder = [command computeCommandEncoder];
    if (encoder == nil) return SIXEL_RUNTIME_ERROR;
    (void)request;
    /* Separate encoders give tracked buffers a write/read dependency
     * without a CPU round trip between building and applying. */
    [encoder setComputePipelineState:g_sixel_gpu_metal_grid_build];
    [encoder setBuffer:palette offset:0 atIndex:2];
    [encoder setBuffer:params offset:0 atIndex:6];
    [encoder setBuffer:run->buffers[4] offset:0 atIndex:14];
    threads_per_group =
        [g_sixel_gpu_metal_grid_build maxTotalThreadsPerThreadgroup];
    if (threads_per_group > 128U) {
        threads_per_group = 128U;
    }
    groups = (4096U + threads_per_group - 1U) / threads_per_group;
    [encoder dispatchThreadgroups:MTLSizeMake(groups, 1U, 1U)
            threadsPerThreadgroup:
                MTLSizeMake(threads_per_group, 1U, 1U)];
    [encoder endEncoding];
    encoder = [command computeCommandEncoder];
    if (encoder == nil) {
        return SIXEL_RUNTIME_ERROR;
    }
    [encoder endEncoding];
    return SIXEL_OK;
}

/* Identity publication follows successful GPU completion, including every
 * consumer range. A failed rebuild must invalidate the overwritten table. */
static void
gridlut_metal_finish(gpu_metal_lookup_run_t *run,
    sixel_gpu_palette_request_t const *request, SIXELSTATUS status)
{
    if (!run->rebuild) return;
    g_sixel_gpu_metal_grid_colors = status == SIXEL_OK ? request->ncolors : 0;
    if (status != SIXEL_OK) return;
    memcpy(g_sixel_gpu_metal_grid_palette, request->palette,
        (size_t)request->ncolors * 3U);
}

static gpu_metal_lookup_t const gridlut_metal = {
    gridlut_metal_chunks, "lookup_gridlut_apply",
    gridlut_metal_prepare, gridlut_metal_build, gridlut_metal_finish
};

gpu_metal_lookup_t const *
sixel_lookup_gridlut_gpu_metal(void)
{
    return &gridlut_metal;
}
