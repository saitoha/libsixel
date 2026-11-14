/* SPDX-License-Identifier: MIT */

/* This policy owns its palette representation even when another
 * policy happens to use the same matrix arithmetic. */
/* Palette tiles for the GEMM kernel: tile t holds colors 8t .. 8t + 7 as
 * columns of an 8x8 row-major matrix with rows -2r, -2g, -2b, |c|^2 and
 * zero rows 4..7.  Missing colors get an unreachable score. */
static NSUInteger
blockcull_metal_tiles(unsigned char const *palette,
                                         int ncolors,
                                         float *tiles)
{
    NSUInteger tile_count;
    int index;
    int column;
    float *tile;
    float r;
    float g;
    float b;

    tile_count = (NSUInteger)((ncolors + 7) / 8);
    memset(tiles, 0, tile_count * 64U * sizeof(float));
    for (index = 0; index < (int)tile_count * 8; ++index) {
        tile = tiles + (size_t)(index / 8) * 64U;
        column = index % 8;
        if (index < ncolors) {
            r = (float)palette[index * 3 + 0];
            g = (float)palette[index * 3 + 1];
            b = (float)palette[index * 3 + 2];
            tile[0 * 8 + column] = -2.0f * r;
            tile[1 * 8 + column] = -2.0f * g;
            tile[2 * 8 + column] = -2.0f * b;
            tile[3 * 8 + column] = r * r + g * g + b * b;
        } else {
            tile[3 * 8 + column] = 1.0e30f;
        }
    }
    return tile_count * 64U * (NSUInteger)sizeof(float);
}

static sixel_gpu_metal_cached_buffer_t blockcull_tiles = { nil, 0U };
static char const *const blockcull_metal_chunks[] = {
#include <generated-metal/lookup-policy-blockcull-gpu-apply-shader.inc.h>
NULL
};
static char const *const blockcull_matrix_chunks[] = {
#include <generated-metal/lookup-policy-blockcull-gpu-scan-shader.inc.h>
#include <generated-metal/lookup-policy-blockcull-gpu-shader.inc.h>
NULL
};

static SIXELSTATUS
blockcull_metal_prepare(gpu_metal_lookup_run_t *run,
    sixel_gpu_palette_request_t const *request,
    sixel_gpu_metal_params_t *params)
{
    float tiles[(SIXEL_PALETTE_MAX + 7) / 8 * 64];
    NSUInteger length;
    id<MTLComputePipelineState> pipeline;
    char const *entry;

    (void)params;
    run->kernel = "blockcull";
    run->kernel_id = 3;
    run->fixed_threads = 32U;
    run->block_width = run->block_height = 8U;
    run->threads_per_block = 32U;
    /* Short palettes use this policy's scan; larger palettes use culling.
     * Both paths and the overflow scan belong to BLOCKCULL itself. */
    entry = request->ncolors > 32 ? "sixel_gpu_palette_apply_blockcull"
        : "lookup_blockcull_scan";
    if ((request->test_hooks != NULL && request->test_hooks->disable_matrix) ||
            ![g_sixel_gpu_metal_device supportsFamily:MTLGPUFamilyApple7])
        return SIXEL_OK;
    pipeline = gpu_metal_lookup_pipeline(blockcull_matrix_chunks,
        run->dither, entry);
    if (pipeline == nil || [pipeline threadExecutionWidth] != 32U ||
            [pipeline maxTotalThreadsPerThreadgroup] < 32U) return SIXEL_OK;
    length = blockcull_metal_tiles(request->palette, request->ncolors, tiles);
    run->buffers[3] = sixel_gpu_metal_upload_buffer(
        &blockcull_tiles, tiles, length);
    if (run->buffers[3] == nil) return SIXEL_BAD_ALLOCATION;
    run->pipeline = pipeline;
    run->thread_multiple = 32U;
    run->fixed_threads = 0U;
    if (request->ncolors <= 32) {
        run->kernel = "gemm";
        run->kernel_id = 2;
        run->block_width = run->block_height = run->threads_per_block = 0U;
    }
    return SIXEL_OK;
}

static gpu_metal_lookup_t const blockcull_metal = {
    blockcull_metal_chunks, "lookup_blockcull_scalar",
    blockcull_metal_prepare, NULL, NULL
};

gpu_metal_lookup_t const *
sixel_lookup_blockcull_gpu_metal(void)
{
    return &blockcull_metal;
}
