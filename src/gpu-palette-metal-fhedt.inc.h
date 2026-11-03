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
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"#ifndef REFINE\n"
"#define REFINE 1\n"
"#endif\n"
"#if REFINE\n"
"typedef ushort Table;\n"
"#else\n"
"typedef uchar Table;\n"
"#endif\n"
"\n"
"#if PACKED\n"
"/* A grid cost needs 18 bits and a label needs eight. Reserve UINT_MAX\n"
" * for a seedless cell; actual distances fit below that sentinel. */\n"
"typedef uint Grid;\n"
"static Grid grid_value(int cost, int label)\n"
"{\n"
"    return label < 0 ? UINT_MAX : (uint(cost) << 8) | uint(255 - label);\n"
"}\n"
"static int grid_cost(Grid value) { return int(value >> 8); }\n"
"static int grid_label(Grid value)\n"
"{\n"
"    return value == UINT_MAX ? -1 : 255 - int(value & 255);\n"
"}\n"
"#else\n"
"typedef int2 Grid;\n"
"static Grid grid_value(int cost, int label) { return int2(cost, label); }\n"
"static int grid_cost(Grid value) { return value.x; }\n"
"static int grid_label(Grid value) { return value.y; }\n"
"#endif\n"
"\n"
"/* R is specialized once per pipeline compilation.\n"
" * Preserve quantized palette coordinates, collisions and axis ties. */\n"
"struct FhedtBuildParams {\n"
"    uint colors, pixels, shift, axis, refine, dist2;\n"
"};\n"
"\n"
"/* Only R squared column heads need clearing. Seed lists contain at most\n"
" * 256 entries and are consumed after the tracked seed encoder completes. */\n"
"kernel void sixel_gpu_fhedt_clear(device uint *heads [[buffer(0)]],\n"
"                       uint i [[thread_position_in_grid]])\n"
"{\n"
"    if (i < R * R) heads[i] = UINT_MAX;\n"
"}\n"
"\n"
"kernel void sixel_gpu_fhedt_seed(device atomic_uint *heads [[buffer(0)]],\n"
"                      constant uchar *palette [[buffer(2)]],\n"
"                      constant FhedtBuildParams &p [[buffer(6)]],\n"
"                      uint i [[thread_position_in_grid]])\n"
"{\n"
"    uint y, z, previous;\n"
"\n"
"    if (i >= p.colors) return;\n"
"    y = palette[3 * i + 1] >> p.shift;\n"
"    z = palette[3 * i + 2] >> p.shift;\n"
"    previous = atomic_exchange_explicit(heads + y + z * R, i,\n"
"        memory_order_relaxed);\n"
"    atomic_store_explicit(heads + R * R + i, previous,\n"
"        memory_order_relaxed);\n"
"}\n"
"\n"
"/* A seed's X cost is zero, so scan only that column's palette entries.\n"
" * Threads write consecutive X cells. Explicit tie rules make the result\n"
" * independent of list order: earlier X, then last colliding label. */\n"
"kernel void sixel_gpu_fhedt_x(device const uint *heads [[buffer(0)]],\n"
"                     device Grid *output [[buffer(1)]],\n"
"                     constant uchar *palette [[buffer(2)]],\n"
"                     constant FhedtBuildParams &p [[buffer(6)]],\n"
"                     uint at [[thread_position_in_grid]])\n"
"{\n"
"    uint candidate;\n"
"    int x, seed, best_x, best, distance, minimum;\n"
"\n"
"    if (at >= R * R * R) return;\n"
"    x = int(at % R);\n"
"    candidate = heads[at / R];\n"
"    minimum = 1 << 28;\n"
"    best_x = R;\n"
"    best = -1;\n"
"    while (candidate != UINT_MAX) {\n"
"        seed = palette[3 * candidate] >> p.shift;\n"
"        distance = (x - seed) * (x - seed);\n"
"        if (distance < minimum || (distance == minimum &&\n"
"                (seed < best_x ||\n"
"                 (seed == best_x && int(candidate) > best)))) {\n"
"            minimum = distance;\n"
"            best_x = seed;\n"
,
"            best = int(candidate);\n"
"        }\n"
"        candidate = heads[R * R + candidate];\n"
"    }\n"
"    output[at] = grid_value(minimum, best);\n"
"}\n"
"\n"
"kernel void sixel_gpu_fhedt_edt(device const Grid *input [[buffer(0)]],\n"
"                     device Grid *output [[buffer(1)]],\n"
"                     constant FhedtBuildParams &p [[buffer(6)]],\n"
"                     device Table *table [[buffer(3)]],\n"
"                     uint line [[thread_position_in_grid]])\n"
"{\n"
"    int sites[R], starts[R];\n"
"    int k, q, i, old, numerator, denominator, start, end, at;\n"
"    uint base, stride;\n"
"    Grid source;\n"
"    int cost;\n"
"    device ushort *compact;\n"
"\n"
"    if (line >= R * R) return;\n"
"    base = p.axis == 0 ? line * R\n"
"        : p.axis == 1 ? line % R + (line / R) * R * R : line;\n"
"    stride = p.axis == 0 ? 1 : p.axis == 1 ? R : R * R;\n"
"    k = -1;\n"
"    for (q = 0; q < R; ++q) {\n"
"        source = input[base + uint(q) * stride];\n"
"        if (grid_label(source) < 0) continue;\n"
"        start = -1000000000;\n"
"        while (k >= 0) {\n"
"            old = sites[k];\n"
"            numerator = grid_cost(source) + q * q\n"
"                - grid_cost(input[base + uint(old) * stride]) - old * old;\n"
"            denominator = 2 * (q - old);\n"
"            /* First integer position at which the later source strictly\n"
"             * wins. Rounding preserves the CPU earlier-source tie. */\n"
"            start = numerator / denominator;\n"
"            start -= numerator < 0 && numerator % denominator != 0;\n"
"            ++start;\n"
"            if (start > starts[k]) break;\n"
"            --k;\n"
"        }\n"
"        ++k;\n"
"        sites[k] = q;\n"
"        starts[k] = k == 0 ? -1000000000 : start;\n"
"    }\n"
"    end = k;\n"
"    k = 0;\n"
"    for (i = 0; i < R; ++i) {\n"
"        at = int(base + uint(i) * stride);\n"
"        if (end < 0) {\n"
"            output[at] = grid_value(1 << 28, -1);\n"
"            continue;\n"
"        }\n"
"        while (k < end && starts[k + 1] <= i) ++k;\n"
"        old = sites[k];\n"
"        source = input[base + uint(old) * stride];\n"
"        cost = grid_cost(source) + (i - old) * (i - old);\n"
"        if (p.axis == 2) {\n"
"#if REFINE\n"
"            /* The final pass no longer needs full distances. Boundary\n"
"             * packing only consumes labels and the zero-distance flag. */\n"
"            compact = (device ushort *)output;\n"
"            compact[at] = ushort(grid_label(source)\n"
"                | (uint(cost == 0) << 9));\n"
"#else\n"
"            table[at] = uchar(grid_label(source));\n"
"#endif\n"
"            continue;\n"
"        }\n"
"        output[at] = grid_value(cost, grid_label(source));\n"
"    }\n"
"}\n"
"\n"
"kernel void sixel_gpu_fhedt_pack(device const ushort *grid [[buffer(0)]],\n"
"                      device ushort *table [[buffer(3)]],\n"
"                      uint at [[thread_position_in_grid]])\n"
"{\n"
"    uint x, y, z, boundary;\n"
"    int label;\n"
"\n"
"    if (at >= R * R * R) return;\n"
"    x = at % R;\n"
"    y = (at / R) % R;\n"
"    z = at / (R * R);\n"
"    label = grid[at] & 255;\n"
"    boundary = (x > 0 && (grid[at - 1] & 255) != label)\n"
"        || (x + 1 < R && (grid[at + 1] & 255) != label)\n"
,
"        || (y > 0 && (grid[at - R] & 255) != label)\n"
"        || (y + 1 < R && (grid[at + R] & 255) != label)\n"
"        || (z > 0 && (grid[at - R * R] & 255) != label)\n"
"        || (z + 1 < R && (grid[at + R * R] & 255) != label);\n"
"    /* Equal axis weights make the optional dist2 shortcut true only at\n"
"     * zero distance (the CPU safe radius squared is 0.75 grid units). */\n"
"    table[at] = ushort(label | (boundary << 8)\n"
"        | (grid[at] & 512));\n"
"}\n"
"\n"
,
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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
