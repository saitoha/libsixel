/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Private Metal FHEDT implementation. All access uses the parent's GPU
 * lock; cached identities are published only after command completion. */
static char const * const g_sixel_gpu_fhedt_source[] = {
#include <generated-metal/lookup-policy-fhedt-gpu-shader.inc.h>
NULL
};

typedef struct sixel_gpu_fhedt_build_params {
    uint32_t colors, pixels, shift, axis, refine, dist2;
} sixel_gpu_fhedt_build_params_t;

static id<MTLComputePipelineState> g_sixel_gpu_fhedt_pipelines[3][2][5];
static int g_sixel_gpu_fhedt_attempted[3][2];
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_fhedt_first = { nil, 0U };
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_fhedt_second = { nil, 0U };
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_fhedt_table = { nil, 0U };
static unsigned char g_sixel_gpu_fhedt_palette[SIXEL_PALETTE_MAX * 3];
static int g_sixel_gpu_fhedt_colors;
static int g_sixel_gpu_fhedt_resolution;
static int g_sixel_gpu_fhedt_refine;
static int g_sixel_gpu_fhedt_dist2;

/* Byte queries at R256 are exact lattice points. Their stored label is
 * already nearest, and corner refinement visits that label first with a
 * strict distance comparison. Use the byte table without changing ties. */
static int
sixel_gpu_fhedt_needs_refine(sixel_gpu_palette_request_t const *request)
{
    return request->fhedt_refine && request->fhedt_resolution != 256;
}

static id<MTLBuffer>
sixel_gpu_fhedt_buffer(sixel_gpu_metal_cached_buffer_t *cache,
                       NSUInteger length)
{
    id<MTLBuffer> buffer;

    if (cache->buffer != nil && cache->capacity >= length) {
        return cache->buffer;
    }
    buffer = [g_sixel_gpu_metal_device newBufferWithLength:length
        options:MTLResourceStorageModePrivate];
    if (buffer == nil) return nil;
    [cache->buffer release];
    cache->buffer = buffer;
    cache->capacity = length;
    return buffer;
}

static int
sixel_gpu_fhedt_prepare(int resolution, int refine)
{
    NSMutableString *source;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    NSError *error;
    NSString *names[5];
    unsigned int index, i, ready;

    index = resolution == 64 ? 0U : resolution == 128 ? 1U : 2U;
    if (g_sixel_gpu_fhedt_attempted[index][refine]) {
        return g_sixel_gpu_fhedt_pipelines[index][refine][0] != nil;
    }
    g_sixel_gpu_fhedt_attempted[index][refine] = 1;
    source = [[NSMutableString alloc] initWithFormat:
        @"#define R %d\n#define PACKED 1\n#define REFINE %d\n",
        resolution, refine];
    if (source == nil) return 0;
    if (!sixel_gpu_palette_metal_append_chunks(source,
            g_sixel_gpu_fhedt_source)) {
        [source release];
        return 0;
    }
    error = nil;
    library = [g_sixel_gpu_metal_device newLibraryWithSource:source
        options:nil error:&error];
    [source release];
    if (library == nil) return 0;
    names[0] = @"sixel_gpu_fhedt_clear";
    names[1] = @"sixel_gpu_fhedt_seed";
    names[2] = @"sixel_gpu_fhedt_x";
    names[3] = @"sixel_gpu_fhedt_edt";
    names[4] = @"sixel_gpu_fhedt_pack";
    ready = 1U;
    for (i = 0U; i < 5U; ++i) {
        function = [library newFunctionWithName:names[i]];
        if (function == nil) {
            ready = 0U;
            continue;
        }
        g_sixel_gpu_fhedt_pipelines[index][refine][i] =
            [g_sixel_gpu_metal_device
                newComputePipelineStateWithFunction:function error:&error];
        [function release];
        if (g_sixel_gpu_fhedt_pipelines[index][refine][i] == nil ||
                [g_sixel_gpu_fhedt_pipelines[index][refine][i]
                    maxTotalThreadsPerThreadgroup] < 32U) {
            ready = 0U;
        }
    }
    [library release];
    if (!ready) {
        for (i = 0U; i < 5U; ++i) {
            [g_sixel_gpu_fhedt_pipelines[index][refine][i] release];
            g_sixel_gpu_fhedt_pipelines[index][refine][i] = nil;
        }
    }
    return ready != 0U;
}

/* Separate tracked encoders establish each write/read dependency without
 * waiting on the CPU between axes. F0 writes the byte LUT directly in Z. */
static SIXELSTATUS
sixel_gpu_fhedt_build(id<MTLCommandBuffer> command, id<MTLBuffer> palette,
    id<MTLBuffer> table, sixel_gpu_palette_request_t const *request)
{
    id<MTLComputeCommandEncoder> encoder;
    id<MTLComputePipelineState> pipeline;
    id<MTLBuffer> first, second;
    sixel_gpu_fhedt_build_params_t params;
    NSUInteger size, count, group, pass, slot, index;

    size = (NSUInteger)request->fhedt_resolution;
    index = size == 64U ? 0U : size == 128U ? 1U : 2U;
    memset(&params, 0, sizeof(params));
    params.colors = request->ncolors;
    params.shift = size == 64U ? 2U : size == 128U ? 1U : 0U;
    params.refine = sixel_gpu_fhedt_needs_refine(request);
    params.dist2 = request->fhedt_use_dist2;
    first = g_sixel_gpu_fhedt_first.buffer;
    second = g_sixel_gpu_fhedt_second.buffer;
    for (pass = 0U; pass < (params.refine ? 6U : 5U); ++pass) {
        slot = pass < 3U ? pass : pass < 5U ? 3U : 4U;
        params.axis = pass >= 2U && pass < 5U ? pass - 2U : 0U;
        count = pass == 1U ? params.colors :
            pass == 0U || pass == 3U || pass == 4U ? size * size :
            size * size * size;
        pipeline = g_sixel_gpu_fhedt_pipelines[index][params.refine][slot];
        group = slot == 1U || slot == 3U ? 32U : 128U;
        if (group > [pipeline maxTotalThreadsPerThreadgroup]) group = 32U;
        encoder = [command computeCommandEncoder];
        if (encoder == nil) return SIXEL_RUNTIME_ERROR;
        [encoder setComputePipelineState:pipeline];
        [encoder setBuffer:pass == 3U || pass == 5U ? second : first
            offset:0 atIndex:0];
        [encoder setBuffer:pass == 3U ? first : second offset:0 atIndex:1];
        [encoder setBuffer:palette offset:0 atIndex:2];
        [encoder setBuffer:table offset:0 atIndex:3];
        [encoder setBytes:&params length:sizeof(params) atIndex:6];
        [encoder dispatchThreadgroups:
            MTLSizeMake((count + group - 1U) / group, 1U, 1U)
            threadsPerThreadgroup:MTLSizeMake(group, 1U, 1U)];
        [encoder endEncoding];
    }
    return SIXEL_OK;
}

static char const *const fhedt_metal_chunks[] = {
#include <generated-metal/lookup-policy-fhedt-gpu-query-shader.inc.h>
#include <generated-metal/lookup-policy-fhedt-gpu-apply-shader.inc.h>
NULL
};

static SIXELSTATUS
fhedt_metal_prepare(gpu_metal_lookup_run_t *run,
    sixel_gpu_palette_request_t const *request,
    sixel_gpu_metal_params_t *params)
{
    SIXELSTATUS status;
    NSUInteger fhedt_grid_size;

    if (!sixel_gpu_fhedt_prepare(request->fhedt_resolution,
            sixel_gpu_fhedt_needs_refine(request))) {
        sixel_helper_set_additional_message(
            "gpu palette apply: FHEDT pipelines unavailable.");
        status = SIXEL_FEATURE_ERROR;
        return status;
    }
    params->lookup_parameters[0] = request->fhedt_resolution;
    params->lookup_parameters[1] = request->fhedt_resolution == 64 ? 2U :
        request->fhedt_resolution == 128 ? 1U : 0U;
    params->lookup_parameters[2] = sixel_gpu_fhedt_needs_refine(request);
    params->lookup_parameters[3] = request->fhedt_use_dist2;
    if (request->test_hooks != NULL) {
        request->test_hooks->fhedt_table_entry_bytes =
            params->lookup_parameters[2] ? 2U : 1U;
    }
    fhedt_grid_size = (NSUInteger)request->fhedt_resolution;
    fhedt_grid_size *= fhedt_grid_size * fhedt_grid_size;
    run->rebuild = g_sixel_gpu_fhedt_colors != request->ncolors ||
        g_sixel_gpu_fhedt_resolution != request->fhedt_resolution ||
        g_sixel_gpu_fhedt_refine != request->fhedt_refine ||
        g_sixel_gpu_fhedt_dist2 != request->fhedt_use_dist2 ||
        memcmp(g_sixel_gpu_fhedt_palette, request->palette,
            (size_t)request->ncolors * 3U) != 0;
    run->buffers[5] = sixel_gpu_fhedt_buffer(&g_sixel_gpu_fhedt_table,
        fhedt_grid_size * (params->lookup_parameters[2] ? 2U : 1U));
    if (run->rebuild &&
            (sixel_gpu_fhedt_buffer(&g_sixel_gpu_fhedt_first,
                fhedt_grid_size * 4U) == nil ||
             sixel_gpu_fhedt_buffer(&g_sixel_gpu_fhedt_second,
                fhedt_grid_size * 4U) == nil)) {
        status = SIXEL_BAD_ALLOCATION;
        return status;
    }
    run->kernel = "fhedt";
    run->kernel_id = 7;
    if (run->buffers[5] == nil)
        return SIXEL_BAD_ALLOCATION;
    if (sixel_trace_topic_is_enabled("gpu_contract"))
        fprintf(stderr, "LSXGPU3|lookup=fhedt|table=%s|R=%d|F=%d|D=%d\n",
            run->rebuild ? "rebuild" : "reuse", request->fhedt_resolution,
            request->fhedt_refine, request->fhedt_use_dist2);
    return SIXEL_OK;
}

static SIXELSTATUS
fhedt_metal_build(gpu_metal_lookup_run_t *run,
    id<MTLCommandBuffer> command, id<MTLBuffer> palette, id<MTLBuffer> params,
    sixel_gpu_palette_request_t const *request)
{
    (void)params;
    return sixel_gpu_fhedt_build(command, palette, run->buffers[5], request);
}

/* Identity publication follows successful GPU completion, including every
 * consumer range. A failed rebuild must invalidate the overwritten table. */
static void
fhedt_metal_finish(gpu_metal_lookup_run_t *run,
    sixel_gpu_palette_request_t const *request, SIXELSTATUS status)
{
    if (!run->rebuild) return;
    g_sixel_gpu_fhedt_colors = status == SIXEL_OK ? request->ncolors : 0;
    if (status != SIXEL_OK) return;
    memcpy(g_sixel_gpu_fhedt_palette, request->palette,
        (size_t)request->ncolors * 3U);
    g_sixel_gpu_fhedt_resolution = request->fhedt_resolution;
    g_sixel_gpu_fhedt_refine = request->fhedt_refine;
    g_sixel_gpu_fhedt_dist2 = request->fhedt_use_dist2;
}

static gpu_metal_lookup_t const fhedt_metal = {
    fhedt_metal_chunks, "lookup_fhedt_apply",
    fhedt_metal_prepare, fhedt_metal_build, fhedt_metal_finish
};

gpu_metal_lookup_t const *
sixel_lookup_fhedt_gpu_metal(void)
{
    return &fhedt_metal;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
