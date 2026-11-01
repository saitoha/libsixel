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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_METAL)

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sixel.h>

#include "bluenoise_64x64.h"
#include "gpu-dequant.h"
#include "gpu-palette.h"
#include "loader-common.h"

enum {
    SIXEL_GPU_METAL_MODE_NONE = 0,
    SIXEL_GPU_METAL_MODE_BLUENOISE = 1,
    SIXEL_GPU_METAL_MODE_A_DITHER = 2,
    SIXEL_GPU_METAL_MODE_X_DITHER = 3,
    SIXEL_GPU_METAL_LOOKUP_DIRECT = 0,
    SIXEL_GPU_METAL_LOOKUP_EYTZINGER = 1,
    SIXEL_GPU_METAL_LOOKUP_GRIDLUT = 2,
    SIXEL_GPU_METAL_LOOKUP_ORDERED = 3,
    SIXEL_GPU_METAL_EYTZINGER_KEY_MAX = 255 * 3
};

typedef struct sixel_gpu_metal_params {
    uint32_t pixel_count;
    uint32_t width;
    uint32_t height;
    uint32_t ncolors;
    uint32_t lookup_mode;
    uint32_t mode;
    uint32_t has_transparent;
    uint32_t transparent_size;
    uint32_t transparent_keycolor;
    uint32_t has_6delta;
    uint32_t accumulation_pixel_count;
    uint32_t accumulation_valid_mask_size;
    uint32_t accumulation_keycolor;
    uint32_t sixdelta_threshold;
    uint32_t sixdelta_metric;
    uint32_t residual_threshold;
    uint32_t residual_columns, residual_rows;
    uint32_t residual_tiled;
    uint32_t residual_cells[128];
    uint32_t accumulation_result_mask_size;
    float positional_strength;
    float bluenoise_strength;
    float bluenoise_gradient_factor;
    int32_t bluenoise_phase_x;
    int32_t bluenoise_phase_y;
    uint32_t bluenoise_channel_rgb;
    uint32_t gradient_size;
    uint32_t gradient_width;
    uint32_t gradient_height;
    unsigned char sixdelta_palette[SIXEL_PALETTE_MAX * 3];
} sixel_gpu_metal_params_t;

typedef struct sixel_gpu_metal_dequant_params {
    uint32_t pixel_count;
    uint32_t width;
    uint32_t height;
    uint32_t ncolors;
    int32_t similarity_bias;
} sixel_gpu_metal_dequant_params_t;

/*
 * The GPU lookup uses the same one-dimensional projection as the CPU
 * Eytzinger policy.  The sorted projection and its integer-key lower-bound
 * table are kept separate so each pixel can start at a rank without walking
 * a divergent tree.
 */
typedef struct sixel_gpu_metal_eytzinger_entry {
    float key;
    uint32_t palette_index;
} sixel_gpu_metal_eytzinger_entry_t;

typedef struct sixel_gpu_metal_eytzinger_params {
    uint32_t count;
    uint32_t window;
} sixel_gpu_metal_eytzinger_params_t;

typedef struct sixel_gpu_metal_eytzinger_pair {
    float key;
    uint32_t palette_index;
} sixel_gpu_metal_eytzinger_pair_t;

typedef struct sixel_gpu_metal_cached_buffer {
    id<MTLBuffer> buffer;
    NSUInteger capacity;
} sixel_gpu_metal_cached_buffer_t;

static pthread_mutex_t g_sixel_gpu_metal_lock = PTHREAD_MUTEX_INITIALIZER;
static id<MTLDevice> g_sixel_gpu_metal_device = nil;
static id<MTLCommandQueue> g_sixel_gpu_metal_queue = nil;
static id<MTLComputePipelineState> g_sixel_gpu_metal_pipeline = nil;
static id<MTLComputePipelineState> g_sixel_gpu_metal_dequant_pipeline = nil;
static id<MTLComputePipelineState> g_sixel_gpu_metal_gemm_pipeline = nil;
static id<MTLComputePipelineState> g_sixel_gpu_metal_blockcull_pipeline = nil;
/* The grid cache is protected by the same lock as all Metal dispatches.
 * Its palette identity is independent of the ordinary upload cache. */
static id<MTLComputePipelineState> g_sixel_gpu_metal_grid_build = nil;
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_metal_grid = { nil, 0U };
static unsigned char g_sixel_gpu_metal_grid_palette[SIXEL_PALETTE_MAX * 3];
static int g_sixel_gpu_metal_grid_colors = 0;
/* Ordered rows and seeds have their own palette identity. Publishing it
 * only after command completion prevents reuse of a partial GPU rebuild. */
static id<MTLComputePipelineState> g_sixel_gpu_metal_ordered_sort = nil;
static id<MTLComputePipelineState> g_sixel_gpu_metal_ordered_seeds = nil;
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_metal_ordered = { nil, 0U };
static unsigned char g_sixel_gpu_metal_ordered_palette[SIXEL_PALETTE_MAX * 3];
static int g_sixel_gpu_metal_ordered_colors = 0;
static int g_sixel_gpu_metal_gemm_probe_done = 0;
static id<MTLBuffer> g_sixel_gpu_metal_blue_noise_buffer = nil;
static id<MTLBuffer> g_sixel_gpu_metal_dummy_buffer = nil;
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_metal_result_buffer =
    { nil, 0U };
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_metal_pixel_buffer =
    { nil, 0U };
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_metal_palette_buffer =
    { nil, 0U };
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_metal_transparent_buffer =
    { nil, 0U };
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_metal_gradient_buffer =
    { nil, 0U };
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_metal_params_buffer =
    { nil, 0U };
static sixel_gpu_metal_cached_buffer_t
    g_sixel_gpu_metal_accumulation_buffer = { nil, 0U };
static sixel_gpu_metal_cached_buffer_t
    g_sixel_gpu_metal_accumulation_valid_buffer = { nil, 0U };
static sixel_gpu_metal_cached_buffer_t
    g_sixel_gpu_metal_accumulation_result_buffer = { nil, 0U };
static sixel_gpu_metal_cached_buffer_t
    g_sixel_gpu_metal_dequant_result_buffer = { nil, 0U };
static sixel_gpu_metal_cached_buffer_t
    g_sixel_gpu_metal_dequant_rgba_buffer = { nil, 0U };
static sixel_gpu_metal_cached_buffer_t
    g_sixel_gpu_metal_dequant_params_buffer = { nil, 0U };
static sixel_gpu_metal_cached_buffer_t
    g_sixel_gpu_metal_eytzinger_sorted_buffer = { nil, 0U };
static sixel_gpu_metal_cached_buffer_t
    g_sixel_gpu_metal_eytzinger_rank_buffer = { nil, 0U };
static sixel_gpu_metal_cached_buffer_t
    g_sixel_gpu_metal_eytzinger_params_buffer = { nil, 0U };
static sixel_gpu_metal_cached_buffer_t
    g_sixel_gpu_metal_gemm_tiles_buffer = { nil, 0U };
static unsigned char g_sixel_gpu_metal_palette_shadow[
    SIXEL_PALETTE_MAX * 3U];
static NSUInteger g_sixel_gpu_metal_palette_shadow_length = 0U;
static int g_sixel_gpu_metal_palette_shadow_valid = 0;
static int g_sixel_gpu_metal_probe_done = 0;
static int g_sixel_gpu_metal_available = 0;

/*
 * Compiled on the first direct lookup, appended to the base source:
 * simdgroup matrices need Apple GPU family 7, and a device without them
 * must keep the base library and its direct scan.
 */
static char const * const g_sixel_gpu_metal_gemm_source_chunks[] = {
"/*\n"
" * Exhaustive direct lookup as a matrix product.  For byte coordinates,\n"
" *\n"
" *   |q - c|^2 - |q|^2 = |c|^2 - 2 q.c\n"
" *\n"
" * ranks colors like the direct scan, and every term is an integer below\n"
" * 2^24, so float products and sums are exact in any order.  Each\n"
" * simdgroup multiplies its 32 queries (rows r, g, b, 1) by 8x8 palette\n"
" * tiles (columns -2r, -2g, -2b, |c|^2) and keeps the first minimum, so\n"
" * ties return the lowest index as the scan does.  Every lane takes part\n"
" * in the matrix operations; finished pixels supply a zero query and keep\n"
" * the result they already wrote.\n"
" *\n"
" * Lane layout of simdgroup_float8x8::thread_elements(): lane owns row fm,\n"
" * columns fn and fn + 1, with qid = lane / 4,\n"
" * fm = (qid & 4) + (lane / 2) % 4 and fn = (qid & 2) * 2 + (lane % 2) * 2.\n"
" * The four lanes of a row differ in lane bits 0 and 3.\n"
" */\n"
"kernel void sixel_gpu_palette_apply_gemm(\n"
"    device uchar *result [[buffer(0)]],\n"
"    device const uchar *pixels [[buffer(1)]],\n"
"    device const uchar *transparent_mask [[buffer(3)]],\n"
"    device const uchar *blue_noise [[buffer(4)]],\n"
"    device const uchar *gradient [[buffer(5)]],\n"
"    constant Params& params [[buffer(6)]],\n"
"    device const uchar *accumulation [[buffer(7)]],\n"
"    device const uchar *accumulation_valid_mask [[buffer(8)]],\n"
"    device uchar *accumulation_result_mask [[buffer(9)]],\n"
"    device const float *tiles [[buffer(13)]],\n"
"    uint gid [[thread_position_in_grid]],\n"
"    ushort lane [[thread_index_in_simdgroup]],\n"
"    ushort sg [[simdgroup_index_in_threadgroup]])\n"
"{\n"
"    threadgroup float rows[8][32 * 4];\n"
"    threadgroup uchar picks[8][32];\n"
"    uchar3 query;\n"
"    uchar3 source;\n"
"    bool done;\n"
"    ushort qid;\n"
"    ushort fm;\n"
"    ushort fn;\n"
"    uint row;\n"
"    uint tile_count;\n"
"    uint first;\n"
"    float2 value;\n"
"    float2 score;\n"
"    float best_score[4];\n"
"    uint best_index[4];\n"
"    float other_score;\n"
"    uint other_index;\n"
"    simdgroup_float8x8 a[4];\n"
"    simdgroup_float8x8 b;\n"
"    simdgroup_float8x8 c;\n"
"\n"
"    done = prepare_query(result, pixels, transparent_mask, blue_noise,\n",
"                         gradient, params, accumulation,\n"
"                         accumulation_valid_mask, accumulation_result_mask,\n"
"                         gid, query, source);\n"
"    rows[sg][lane * 4u + 0u] = float(query.x);\n"
"    rows[sg][lane * 4u + 1u] = float(query.y);\n"
"    rows[sg][lane * 4u + 2u] = float(query.z);\n"
"    rows[sg][lane * 4u + 3u] = 1.0f;\n"
"    simdgroup_barrier(mem_flags::mem_threadgroup);\n"
"\n"
"    qid = lane / 4u;\n"
"    fm = (qid & 4u) + (lane / 2u) % 4u;\n"
"    fn = (qid & 2u) * 2u + (lane % 2u) * 2u;\n"
"    for (uint m = 0u; m < 4u; ++m) {\n"
"        row = (m * 8u + fm) * 4u;\n"
"        value = fn < 4u\n"
"            ? float2(rows[sg][row + fn], rows[sg][row + fn + 1u])\n"
"            : float2(0.0f);\n"
"        a[m].thread_elements()[0] = value.x;\n"
"        a[m].thread_elements()[1] = value.y;\n"
"        best_score[m] = INFINITY;\n"
"        best_index[m] = 0u;\n"
"    }\n"
"    tile_count = (params.ncolors + 7u) / 8u;\n"
"    for (uint t = 0u; t < tile_count; ++t) {\n"
"        simdgroup_load(b, tiles + t * 64u, 8);\n"
"        first = t * 8u + fn;\n"
"        for (uint m = 0u; m < 4u; ++m) {\n"
"            simdgroup_multiply(c, a[m], b);\n"
"            score = float2(c.thread_elements()[0], c.thread_elements()[1]);\n"
"            if (score.x < best_score[m]) {\n"
"                best_score[m] = score.x;\n"
"                best_index[m] = first;\n"
"            }\n"
"            if (score.y < best_score[m]) {\n"
"                best_score[m] = score.y;\n"
"                best_index[m] = first + 1u;\n"
"            }\n"
"        }\n"
"    }\n"
"    for (uint m = 0u; m < 4u; ++m) {\n"
"        for (ushort mask = 1u; mask <= 8u; mask = ushort(mask * 8u)) {\n"
"            other_score = simd_shuffle_xor(best_score[m], mask);\n"
"            other_index = simd_shuffle_xor(best_index[m], mask);\n"
"            if (other_score < best_score[m] ||\n"
"                    (other_score == best_score[m] &&\n"
"                     other_index < best_index[m])) {\n"
"                best_score[m] = other_score;\n"
"                best_index[m] = other_index;\n"
"            }\n"
"        }\n"
"        if ((lane & 9u) == 0u) {\n"
"            picks[sg][m * 8u + fm] = uchar(best_index[m]);\n"
"        }\n"
"    }\n"
"    simdgroup_barrier(mem_flags::mem_threadgroup);\n"
"    if (!done) {\n",
"        finish_pixel(result, params, accumulation, accumulation_valid_mask,\n"
"                     accumulation_result_mask, gid, source,\n"
"                     uint(picks[sg][lane]));\n"
"    }\n"
"}\n",
"/*\n"
" * Block-culled exact lookup.  A simdgroup owns an 8x8 pixel block; lane\n"
" * L maps block positions L and L + 32 (column % 8, row / 8).  For color\n"
" * c, far(c) bounds the distance from c to every point of the queries'\n"
" * bounding box, so U = min far(c) bounds each query's nearest distance;\n"
" * a color whose box distance exceeds U is strictly farther than the\n"
" * color attaining U at every query.  Survivors keep palette order in\n"
" * threadgroup GEMM tiles of 32 floats (rows -2r, -2g, -2b, |c|^2); a load\n"
" * also reads the next tile's rows as rows 4..7, which meet the zero query\n"
" * columns.  The first minimum then returns the direct scan's index, ties\n"
" * included.  More than SIXEL_BLOCKCULL_CAP survivors use the full device\n"
" * tiles.\n"
" */\n"
"#define SIXEL_BLOCKCULL_CAP 128u\n"
"static uint blockcull_gid(constant Params& params, uint block, uint pos)\n"
"{\n"
"    uint blocks_x;\n"
"    uint x;\n"
"    uint y;\n"
"\n"
"    blocks_x = (params.width + 7u) / 8u;\n"
"    x = (block % blocks_x) * 8u + pos % 8u;\n"
"    y = (block / blocks_x) * 8u + pos / 8u;\n"
"    if (x < params.width && y < params.height &&\n"
"            y * params.width + x < params.pixel_count) {\n"
"        return y * params.width + x;\n"
"    }\n"
"    return params.pixel_count;\n"
"}\n"
"kernel void sixel_gpu_palette_apply_blockcull(\n"
"    device uchar *result [[buffer(0)]],\n"
"    device const uchar *pixels [[buffer(1)]],\n"
"    constant uchar *palette [[buffer(2)]],\n"
"    device const uchar *transparent_mask [[buffer(3)]],\n"
"    device const uchar *blue_noise [[buffer(4)]],\n"
"    device const uchar *gradient [[buffer(5)]],\n"
"    constant Params& params [[buffer(6)]],\n"
"    device const uchar *accumulation [[buffer(7)]],\n"
"    device const uchar *accumulation_valid_mask [[buffer(8)]],\n"
"    device uchar *accumulation_result_mask [[buffer(9)]],\n"
"    device const float *tiles [[buffer(13)]],\n"
"    uint group [[threadgroup_position_in_grid]],\n"
"    ushort group_simdgroups [[simdgroups_per_threadgroup]],\n"
"    ushort lane [[thread_index_in_simdgroup]],\n"
"    ushort sg [[simdgroup_index_in_threadgroup]])\n"
"{\n"
"    threadgroup float rows[8][64 * 4];\n"
"    threadgroup float culled[8][(SIXEL_BLOCKCULL_CAP / 8u + 1u) * 32u];\n"
"    threadgroup uchar map[8][SIXEL_BLOCKCULL_CAP];\n"
"    threadgroup uchar picks[8][64];\n"
"    uchar3 query[2];\n"
"    uchar3 source[2];\n"
"    bool done[2];\n"
"    uint gid[2];\n"
"    bool spill;\n"
"    uint block;\n"
"    uint3 low;\n"
"    uint3 high;\n"
"    int3 color;\n"
"    int3 gap;\n"
"    int bound;\n"
"    uint count;\n"
"    uint keep;\n"
"    uint slot;\n"
"    uint c;\n"
"    uint h;\n"
"    uint tile_count;\n"
"    threadgroup float *column;\n"
"    ushort qid;\n"
"    ushort fm;\n"
"    ushort fn;\n"
"    uint row;\n"
"    uint first;\n"
"    float2 value;\n"
"    float2 score;\n"
"    float best_score[8];\n"
"    uint best_index[8];\n"
"    float other_score;\n"
"    uint other_index;\n"
"    simdgroup_float8x8 a[8];\n"
"    simdgroup_float8x8 b;\n"
"    simdgroup_float8x8 product;\n"
"\n"
"    block = group * uint(group_simdgroups) + uint(sg);\n"
"    low = uint3(255u);\n"
"    high = uint3(0u);\n"
"    for (h = 0u; h < 2u; ++h) {\n"
"        gid[h] = blockcull_gid(params, block, uint(lane) + h * 32u);\n"
"        done[h] = prepare_query(result, pixels, transparent_mask,\n"
"                                blue_noise, gradient, params, accumulation,\n"
"                                accumulation_valid_mask,\n"
"                                accumulation_result_mask, gid[h],\n"
"                                query[h], source[h]);\n"
"        if (!done[h]) {\n"
"            low = min(low, uint3(query[h]));\n"
"            high = max(high, uint3(query[h]));\n"
"        }\n"
"        rows[sg][(lane + h * 32u) * 4u + 0u] = float(query[h].x);\n"
"        rows[sg][(lane + h * 32u) * 4u + 1u] = float(query[h].y);\n"
"        rows[sg][(lane + h * 32u) * 4u + 2u] = float(query[h].z);\n"
"        rows[sg][(lane + h * 32u) * 4u + 3u] = 1.0f;\n"
"    }\n"
"    if (simd_all(done[0] && done[1])) {\n"
"        return;\n"
"    }\n"
"    low = uint3(simd_min(low.x), simd_min(low.y), simd_min(low.z));\n"
"    high = uint3(simd_max(high.x), simd_max(high.y), simd_max(high.z));\n",
"\n"
"    bound = 0x7fffffff;\n"
"    for (c = lane; c < params.ncolors; c += 32u) {\n"
"        color = int3(palette[c * 3u + 0u], palette[c * 3u + 1u],\n"
"                     palette[c * 3u + 2u]);\n"
"        gap = max(color - int3(low), int3(high) - color);\n"
"        bound = min(bound, gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);\n"
"    }\n"
"    bound = simd_min(bound);\n"
"\n"
"    count = 0u;\n"
"    for (uint base = 0u; base < params.ncolors &&\n"
"            count <= SIXEL_BLOCKCULL_CAP; base += 32u) {\n"
"        c = base + lane;\n"
"        keep = 0u;\n"
"        color = int3(0);\n"
"        if (c < params.ncolors) {\n"
"            color = int3(palette[c * 3u + 0u], palette[c * 3u + 1u],\n"
"                         palette[c * 3u + 2u]);\n"
"            gap = max(max(int3(low) - color, color - int3(high)), int3(0));\n"
"            keep = gap.x * gap.x + gap.y * gap.y + gap.z * gap.z <= bound\n"
"                ? 1u : 0u;\n"
"        }\n"
"        slot = count + simd_prefix_exclusive_sum(keep);\n"
"        if (keep != 0u && slot < SIXEL_BLOCKCULL_CAP) {\n"
"            column = &culled[sg][(slot / 8u) * 32u + slot % 8u];\n"
"            column[0] = -2.0f * float(color.x);\n"
"            column[8] = -2.0f * float(color.y);\n"
"            column[16] = -2.0f * float(color.z);\n"
"            column[24] = float(color.x * color.x + color.y * color.y +\n"
"                               color.z * color.z);\n"
"            map[sg][slot] = uchar(c);\n"
"        }\n"
"        count += simd_sum(keep);\n"
"    }\n"
"    spill = count > SIXEL_BLOCKCULL_CAP;\n"
"    if (spill) {\n"
"        tile_count = (params.ncolors + 7u) / 8u;\n"
"    } else {\n"
"        /* Unused columns never win; the extra tile only feeds rows 4..7\n"
"         * of the last load and must stay finite. */\n"
"        tile_count = (count + 7u) / 8u;\n"
"        for (slot = count + lane; slot < tile_count * 8u + 8u;\n"
"                slot += 32u) {\n"
"            column = &culled[sg][(slot / 8u) * 32u + slot % 8u];\n"
"            column[0] = 0.0f;\n"
"            column[8] = 0.0f;\n"
"            column[16] = 0.0f;\n"
"            column[24] = slot < tile_count * 8u ? 1.0e30f : 0.0f;\n"
"        }\n"
"    }\n"
"    simdgroup_barrier(mem_flags::mem_threadgroup);\n",
"\n"
"    qid = lane / 4u;\n"
"    fm = (qid & 4u) + (lane / 2u) % 4u;\n"
"    fn = (qid & 2u) * 2u + (lane % 2u) * 2u;\n"
"    for (uint m = 0u; m < 8u; ++m) {\n"
"        row = (m * 8u + fm) * 4u;\n"
"        value = fn < 4u\n"
"            ? float2(rows[sg][row + fn], rows[sg][row + fn + 1u])\n"
"            : float2(0.0f);\n"
"        a[m].thread_elements()[0] = value.x;\n"
"        a[m].thread_elements()[1] = value.y;\n"
"        best_score[m] = INFINITY;\n"
"        best_index[m] = 0u;\n"
"    }\n"
"    for (uint t = 0u; t < tile_count; ++t) {\n"
"        if (spill) {\n"
"            simdgroup_load(b, tiles + t * 64u, 8);\n"
"        } else {\n"
"            simdgroup_load(b, &culled[sg][t * 32u], 8);\n"
"        }\n"
"        first = t * 8u + fn;\n"
"        for (uint m = 0u; m < 8u; ++m) {\n"
"            simdgroup_multiply(product, a[m], b);\n"
"            score = float2(product.thread_elements()[0],\n"
"                           product.thread_elements()[1]);\n"
"            if (score.x < best_score[m]) {\n"
"                best_score[m] = score.x;\n"
"                best_index[m] = first;\n"
"            }\n"
"            if (score.y < best_score[m]) {\n"
"                best_score[m] = score.y;\n"
"                best_index[m] = first + 1u;\n"
"            }\n"
"        }\n"
"    }\n"
"    for (uint m = 0u; m < 8u; ++m) {\n"
"        for (ushort mask = 1u; mask <= 8u; mask = ushort(mask * 8u)) {\n"
"            other_score = simd_shuffle_xor(best_score[m], mask);\n"
"            other_index = simd_shuffle_xor(best_index[m], mask);\n"
"            if (other_score < best_score[m] ||\n"
"                    (other_score == best_score[m] &&\n"
"                     other_index < best_index[m])) {\n"
"                best_score[m] = other_score;\n"
"                best_index[m] = other_index;\n"
"            }\n"
"        }\n"
"        if ((lane & 9u) == 0u) {\n"
"            picks[sg][m * 8u + fm] = spill ? uchar(best_index[m])\n"
"                                           : map[sg][best_index[m]];\n"
"        }\n"
"    }\n"
"    simdgroup_barrier(mem_flags::mem_threadgroup);\n"
"    for (h = 0u; h < 2u; ++h) {\n"
"        if (!done[h]) {\n"
"            finish_pixel(result, params, accumulation,\n"
"                         accumulation_valid_mask, accumulation_result_mask,\n"
"                         gid[h], source[h],\n"
"                         uint(picks[sg][lane + h * 32u]));\n"
"        }\n"
"    }\n"
"}\n",
NULL
};

static char const * const g_sixel_gpu_metal_source_chunks[] = {
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct Params {\n"
"    uint pixel_count;\n"
"    uint width;\n"
"    uint height;\n"
"    uint ncolors;\n"
"    uint lookup_mode;\n"
"    uint mode;\n"
"    uint has_transparent;\n"
"    uint transparent_size;\n"
"    uint transparent_keycolor;\n"
"    uint has_6delta;\n"
"    uint accumulation_pixel_count;\n"
"    uint accumulation_valid_mask_size;\n"
"    uint accumulation_keycolor;\n"
"    uint sixdelta_threshold;\n"
"    uint sixdelta_metric;\n"
"    uint residual_threshold;\n"
"    uint residual_columns, residual_rows;\n"
"    uint residual_tiled;\n"
"    uint residual_cells[128];\n"
"    uint accumulation_result_mask_size;\n"
"    float positional_strength;\n"
"    float bluenoise_strength;\n"
"    float bluenoise_gradient_factor;\n"
"    int bluenoise_phase_x;\n"
"    int bluenoise_phase_y;\n"
"    uint bluenoise_channel_rgb;\n"
"    uint gradient_size;\n"
"    uint gradient_width;\n"
"    uint gradient_height;\n"
"    uchar sixdelta_palette[768];\n"
"};\n"
"struct EytzingerEntry {\n"
"    float key;\n"
"    uint palette_index;\n"
"};\n"
"struct EytzingerParams {\n"
"    uint count;\n"
"    uint window;\n"
"};\n"
"static float bn_sample(device const uchar *blue_noise, int x, int y)\n"
"{\n"
"    uint xx = uint(x & 63);\n"
"    uint yy = uint(y & 63);\n"
"    return float(blue_noise[yy * 64u + xx]) / 127.5f - 1.0f;\n"
"}\n"
"static float tri_noise(device const uchar *blue_noise,\n"
"                       constant Params& params,\n"
"                       int x,\n"
"                       int y,\n"
"                       int c)\n"
"{\n"
"    const int offset_x[3] = { 17, 34, 51 };\n"
"    const int offset_y[3] = { 31, 62, 93 };\n"
"    int channel_x = 0;\n"
"    int channel_y = 0;\n"
"    float sample_u;\n"
"    float sample_v;\n"
"    if (params.bluenoise_channel_rgb != 0u && c >= 0 && c < 3) {\n"
"        channel_x = offset_x[c];\n"
"        channel_y = offset_y[c];\n"
"    }\n"
"    sample_u = (bn_sample(blue_noise,\n"
"                          x + params.bluenoise_phase_x + channel_x,\n"
"                          y + params.bluenoise_phase_y + channel_y)\n"
"                + 1.0f) * 0.5f;\n"
"    sample_v = (bn_sample(blue_noise,\n"
"                          x + params.bluenoise_phase_x + channel_x + 13,\n"
"                          y + params.bluenoise_phase_y + channel_y + 29)\n"
"                + 1.0f) * 0.5f;\n"
"    return ((sample_u + sample_v) - 1.0f) * params.bluenoise_strength;\n"
"}\n"
"static float gradient_weight(device const uchar *gradient,\n"
"                             constant Params& params,\n"
"                             uint x,\n"
"                             uint y,\n"
"                             uint index)\n"
"{\n"
"    float normalized;\n"
"    float attenuated;\n"
"    if (params.bluenoise_gradient_factor <= 0.0f) {\n"
"        return 1.0f;\n"
"    }\n"
"    if (params.gradient_width == 0u || params.gradient_height == 0u ||\n"
"        x >= params.gradient_width || y >= params.gradient_height ||\n"
"        index >= params.gradient_size) {\n"
"        return 1.0f;\n"
"    }\n"
"    normalized = float(gradient[index]) / 255.0f;\n"
"    if (normalized <= 0.0f) {\n"
"        return 1.0f;\n"
"    }\n"
"    if (normalized >= 1.0f) {\n"
"        return 0.0f;\n"
"    }\n"
"    attenuated = pow(normalized, params.bluenoise_gradient_factor);\n"
"    attenuated = clamp(attenuated, 0.0f, 1.0f);\n"
"    return 1.0f - attenuated;\n"
"}\n",
"static uint palette_distance(\n"
"    constant uchar *palette,\n"
"    uchar3 q,\n"
"    uint palette_index)\n"
"{\n"
"    int dr = int(q.x) - int(palette[palette_index * 3u + 0u]);\n"
"    int dg = int(q.y) - int(palette[palette_index * 3u + 1u]);\n"
"    int db = int(q.z) - int(palette[palette_index * 3u + 2u]);\n"
"    return uint(dr * dr + dg * dg + db * db);\n"
"}\n"
"static uint color_distance(uchar3 left, uchar3 right)\n"
"{\n"
"    int dr = int(left.x) - int(right.x);\n"
"    int dg = int(left.y) - int(right.y);\n"
"    int db = int(left.z) - int(right.z);\n"
"    return uint(dr * dr + dg * dg + db * db);\n"
"}\n"
"static void update_palette_best(\n"
"    constant uchar *palette,\n"
"    uchar3 q,\n"
"    uint palette_index,\n"
"    thread uint& best,\n"
"    thread uint& best_dist)\n"
"{\n"
"    uint dist = palette_distance(palette, q, palette_index);\n"
"    if (dist < best_dist) {\n"
"        best_dist = dist;\n"
"        best = palette_index;\n"
"    }\n"
"}\n",
"/* Exact fixed RGB-cell candidates. Overflow keeps exhaustive lookup. */\n"
"struct GridCell {\n"
"    uchar count;\n"
"    uchar indexes[64];\n"
"};\n"
"kernel void sixel_gpu_grid_build(\n"
"    constant uchar *palette [[buffer(2)]],\n"
"    constant Params& params [[buffer(6)]],\n"
"    device GridCell *grid [[buffer(14)]],\n"
"    uint cell [[thread_position_in_grid]])\n"
"{\n"
"    int3 low;\n"
"    int3 high;\n"
"    int3 color;\n"
"    int3 gap;\n"
"    uint bound;\n"
"    uint far;\n"
"    uint near;\n"
"    uint count;\n"
"    uint i;\n"
"    if (cell >= 4096u) {\n"
"        return;\n"
"    }\n"
"    low = int3(cell >> 8u, (cell >> 4u) & 15u, cell & 15u) * 16;\n"
"    high = low + 15;\n"
"    bound = 0xffffffffu;\n"
"    for (i = 0u; i < params.ncolors; ++i) {\n"
"        color = int3(palette[i * 3u], palette[i * 3u + 1u],\n"
"                     palette[i * 3u + 2u]);\n"
"        gap = max(color - low, high - color);\n"
"        far = uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);\n"
"        bound = min(bound, far);\n"
"    }\n"
"    count = 0u;\n"
"    for (i = 0u; i < params.ncolors; ++i) {\n"
"        color = int3(palette[i * 3u], palette[i * 3u + 1u],\n"
"                     palette[i * 3u + 2u]);\n"
"        gap = max(max(low - color, color - high), int3(0));\n"
"        near = uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);\n"
"        if (near <= bound) {\n"
"            if (count < 64u) {\n"
"                grid[cell].indexes[count] = uchar(i);\n"
"            }\n"
"            ++count;\n"
"        }\n"
"    }\n"
"    grid[cell].count = count <= 64u ? uchar(count) : uchar(0);\n"
"}\n"
"static uint grid_palette_lookup(constant uchar *palette,\n"
"                                device const GridCell *grid,\n"
"                                uint ncolors, uchar3 q)\n"
"{\n"
"    uint cell;\n"
"    uint count;\n"
"    uint best;\n"
"    uint best_dist;\n"
"    uint index;\n"
"    uint dist;\n"
"    uint i;\n"
"    cell = (uint(q.x >> 4) << 8) | (uint(q.y >> 4) << 4) | (q.z >> 4);\n"
"    count = grid[cell].count;\n"
"    if (count == 1u) {\n"
"        return grid[cell].indexes[0];\n"
"    }\n"
"    best = 0u;\n"
"    best_dist = 0xffffffffu;\n"
"    for (i = 0u; i < (count != 0u ? count : ncolors); ++i) {\n"
"        index = count != 0u ? uint(grid[cell].indexes[i]) : i;\n"
"        dist = palette_distance(palette, q, index);\n"
"        if (dist < best_dist) {\n"
"            best_dist = dist;\n"
"            best = index;\n"
"        }\n"
"    }\n"
"    return best;\n"
"}\n",
"/* Distance rows, packed RGB rows and 32^3 seeds share a 640 KiB table.\n"
" * All query arithmetic is integer. Geometry and float tolerances are absent.\n"
" * Keep rows at fixed stride 256, including short palettes and padded tails. */\n"
"kernel void sixel_gpu_ordered_sort(\n"
"    constant uchar *palette [[buffer(2)]],\n"
"    constant Params& params [[buffer(6)]],\n"
"    device uint *table [[buffer(15)]],\n"
"    uint row [[threadgroup_position_in_grid]],\n"
"    uint lane [[thread_index_in_threadgroup]])\n"
"{\n"
"    threadgroup uint keys[256];\n"
"    threadgroup uint own_at;\n"
"    uchar3 center;\n"
"    uint width, step, other, a, b, at, key, index, rgb;\n"
"    bool ascending;\n"
"\n"
"    center = uchar3(palette[row * 3u], palette[row * 3u + 1u],\n"
"                    palette[row * 3u + 2u]);\n"
"    keys[lane] = lane < params.ncolors\n"
"        ? (palette_distance(palette, center, lane) << 8) | lane\n"
"        : 0xffffffffu;\n"
"    threadgroup_barrier(mem_flags::mem_threadgroup);\n"
"    for (width = 2u; width < 2u * params.ncolors; width <<= 1) {\n"
"        for (step = width >> 1; step > 0u; step >>= 1) {\n"
"            other = lane ^ step;\n"
"            if (other > lane) {\n"
"                a = keys[lane];\n"
"                b = keys[other];\n"
"                ascending = (lane & width) == 0u;\n"
"                keys[lane] = ascending ? min(a, b) : max(a, b);\n"
"                keys[other] = ascending ? max(a, b) : min(a, b);\n"
"            }\n"
"            threadgroup_barrier(mem_flags::mem_threadgroup);\n"
"        }\n"
"    }\n"
"    /* Original indexes distinguish the center even among duplicate colors.\n"
"     * Locate and omit it in parallel, without a serial compaction pass. */\n"
"    if ((keys[lane] & 255u) == row) {\n"
"        own_at = lane;\n"
"    }\n"
"    threadgroup_barrier(mem_flags::mem_threadgroup);\n"
"    at = min(lane, params.ncolors > 1u ? params.ncolors - 2u : 0u);\n"
"    at += at >= own_at && params.ncolors > 1u;\n"
"    key = keys[at];\n"
"    index = key & 255u;\n",
"    rgb = uint(palette[index * 3u])\n"
"        | (uint(palette[index * 3u + 1u]) << 8)\n"
"        | (uint(palette[index * 3u + 2u]) << 16) | (index << 24);\n"
"    table[row * 256u + lane] = key;\n"
"    table[65536u + row * 256u + lane] = rgb;\n"
"}\n"
"kernel void sixel_gpu_ordered_seeds(\n"
"    constant uchar *palette [[buffer(2)]],\n"
"    constant Params& params [[buffer(6)]],\n"
"    device uint *table [[buffer(15)]],\n"
"    uint cell [[thread_position_in_grid]])\n"
"{\n"
"    int3 q, color, gap;\n"
"    uint best, minimum, distance, i;\n"
"\n"
"    if (cell >= 32768u) {\n"
"        return;\n"
"    }\n"
"    /* Exact rational centers scaled by 64. The largest squared distance\n"
"     * is below 2^30, so even signed intermediate arithmetic is exact. */\n"
"    q = int3(2u * (cell >> 10) + 1u,\n"
"             2u * ((cell >> 5) & 31u) + 1u,\n"
"             2u * (cell & 31u) + 1u) * 255;\n"
"    best = 0u;\n"
"    minimum = 0xffffffffu;\n"
"    for (i = 0u; i < params.ncolors; ++i) {\n"
"        color = int3(palette[i * 3u], palette[i * 3u + 1u],\n"
"                     palette[i * 3u + 2u]) * 64;\n"
"        gap = q - color;\n"
"        distance = uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);\n"
"        if (distance < minimum) {\n"
"            minimum = distance;\n"
"            best = i;\n"
"        }\n"
"    }\n"
"    table[131072u + cell] = best;\n"
"}\n"
"static uint ordered_palette_lookup(constant uchar *palette,\n"
"    device const uint *table, uint ncolors, uchar3 query)\n"
"{\n"
"    uint center, best, limit, at, key, cell;\n"
"    uint4 packed, scores;\n"
"    int4 dr, dg, db;\n"
"\n"
"    cell = (uint(query.x >> 3) << 10) | (uint(query.y >> 3) << 5)\n",
"        | uint(query.z >> 3);\n"
"    center = table[131072u + cell];\n"
"    best = (palette_distance(palette, query, center) << 8) | center;\n"
"    /* Every color no worse than c lies within twice |q-c|. Equality must\n"
"     * survive for ties. One scan suffices because the row is complete. */\n"
"    limit = 4u * (best >> 8);\n"
"    for (at = 0u; at + 1u < ncolors; at += 4u) {\n"
"        if ((table[center * 256u + at] >> 8) > limit) {\n"
"            break;\n"
"        }\n"
"        packed = ((device const uint4 *)table)[\n"
"            16384u + center * 64u + at / 4u];\n"
"        dr = int4(packed & 255u) - int(query.x);\n"
"        dg = int4((packed >> 8) & 255u) - int(query.y);\n"
"        db = int4((packed >> 16) & 255u) - int(query.z);\n"
"        scores = (uint4(dr * dr + dg * dg + db * db) << 8)\n"
"            | (packed >> 24);\n"
"        key = min(min(scores.x, scores.y), min(scores.z, scores.w));\n"
"        best = min(best, key);\n"
"    }\n"
"    return best & 255u;\n"
"}\n",
"static uint eytzinger_palette_lookup(\n"
"    constant uchar *palette,\n"
"    constant EytzingerEntry *sorted,\n"
"    constant uint *rank_lut,\n"
"    constant EytzingerParams& params,\n"
"    uchar3 q)\n"
"{\n"
"    float key;\n"
"    uint rank;\n"
"    uint best;\n"
"    uint best_dist;\n"
"    uint left;\n"
"    uint right;\n"
"    uint step;\n"
"    uint candidate;\n"
"    float key_diff;\n"
"    float bound;\n"
"    bool left_open;\n"
"    bool right_open;\n"
"    key = float(q.x) + float(q.y) + float(q.z);\n"
"    rank = rank_lut[uint(key)];\n"
"    best = sorted[rank].palette_index;\n"
"    best_dist = palette_distance(palette, q, best);\n"
"    left = rank;\n"
"    right = rank;\n"
"    left_open = true;\n"
"    right_open = true;\n"
"    for (step = 0u; step < params.window; ++step) {\n"
"        bound = float(best_dist);\n"
"        if (left_open) {\n"
"            if (left == 0u) {\n"
"                left_open = false;\n"
"            } else {\n"
"                candidate = left - 1u;\n"
"                key_diff = key - sorted[candidate].key;\n"
"                if (key_diff * key_diff <= bound) {\n"
"                    update_palette_best(palette, q,\n"
"                                        sorted[candidate].palette_index,\n"
"                                        best,\n"
"                                        best_dist);\n"
"                    left = candidate;\n"
"                } else {\n"
"                    left_open = false;\n"
"                }\n"
"            }\n"
"        }\n"
"        bound = float(best_dist);\n"
"        if (right_open) {\n"
"            if (right + 1u >= params.count) {\n"
"                right_open = false;\n"
"            } else {\n"
"                candidate = right + 1u;\n"
"                key_diff = key - sorted[candidate].key;\n"
"                if (key_diff * key_diff <= bound) {\n"
"                    update_palette_best(palette, q,\n"
"                                        sorted[candidate].palette_index,\n"
"                                        best,\n"
"                                        best_dist);\n"
"                    right = candidate;\n"
"                } else {\n"
"                    right_open = false;\n"
"                }\n"
"            }\n"
"        }\n"
"    }\n"
"    return best;\n"
"}\n",
"/*\n"
" * Shared per-pixel stages.  prepare_query returns true when the pixel is\n"
" * finished without a palette search (out of range, transparent, or kept by\n"
" * 6delta), after writing any result.  Otherwise it yields the search query\n"
" * (after dither noise) and the untouched source color.\n"
" */\n"
"static bool prepare_query(\n"
"    device uchar *result,\n"
"    device const uchar *pixels,\n"
"    device const uchar *transparent_mask,\n"
"    device const uchar *blue_noise,\n"
"    device const uchar *gradient,\n"
"    constant Params& params,\n"
"    device const uchar *accumulation,\n"
"    device const uchar *accumulation_valid_mask,\n"
"    device uchar *accumulation_result_mask,\n"
"    uint gid,\n"
"    thread uchar3& query,\n"
"    thread uchar3& source)\n"
"{\n"
"    uint x;\n"
"    uint y;\n"
"    uchar q[3];\n"
"    uint local_threshold, cell;\n"
"    bool within;\n"
"    int dr;\n"
"    int dg;\n"
"    int db;\n"
"    query = uchar3(0);\n"
"    source = uchar3(0);\n"
"    if (gid >= params.pixel_count) {\n"
"        return true;\n"
"    }\n"
"    if (params.has_transparent != 0u && gid < params.transparent_size &&\n"
"        transparent_mask[gid] != 0u) {\n"
"        result[gid] = uchar(params.transparent_keycolor);\n"
"        if (gid < params.accumulation_result_mask_size) {\n"
"            accumulation_result_mask[gid] = uchar(1);\n"
"        }\n"
"        return true;\n"
"    }\n"
"    x = gid % params.width;\n"
"    y = gid / params.width;\n"
"    q[0] = pixels[gid * 3u + 0u];\n"
"    q[1] = pixels[gid * 3u + 1u];\n"
"    q[2] = pixels[gid * 3u + 2u];\n"
"    source = uchar3(q[0], q[1], q[2]);\n"
"    if (params.has_6delta != 0u &&\n"
"        gid < params.accumulation_pixel_count &&\n"
"        (gid >= params.accumulation_valid_mask_size ||\n"
"         accumulation_valid_mask[gid] != 0u)) {\n"
"        dr = int(q[0]) - int(accumulation[gid * 3u + 0u]);\n"
"        dg = int(q[1]) - int(accumulation[gid * 3u + 1u]);\n"
"        db = int(q[2]) - int(accumulation[gid * 3u + 2u]);\n"
"        dr = dr < 0 ? -dr : dr;\n",
"        dg = dg < 0 ? -dg : dg;\n"
"        db = db < 0 ? -db : db;\n"
"        local_threshold = params.sixdelta_threshold;\n"
"        if (params.residual_columns != 0u) {\n"
"            if (params.residual_tiled != 0u)\n"
"                cell = (y & 63u) * 64u + (x & 63u);\n"
"            else\n"
"                cell = (((y + 1u) * params.residual_rows - 1u) /\n"
"                    params.height) * params.residual_columns +\n"
"                    ((x + 1u) * params.residual_columns - 1u) /\n"
"                    params.width;\n"
"            if ((params.residual_cells[cell / 32u] &\n"
"                    (1u << (cell % 32u))) != 0u)\n"
"                local_threshold = min(local_threshold,\n"
"                    params.residual_threshold);\n"
"        }\n"
"        within = uint(dr) <= local_threshold &&\n"
"            uint(dg) <= local_threshold && uint(db) <= local_threshold;\n"
"        if (params.sixdelta_metric == 2u)\n"
"            within = uint(2*dr*dr + 4*dg*dg + db*db) <=\n"
"                7u * local_threshold * local_threshold;\n"
"        if (within) {\n"
"            result[gid] = uchar(params.accumulation_keycolor);\n"
"            if (gid < params.accumulation_result_mask_size) {\n"
"                accumulation_result_mask[gid] =\n"
"                    uchar((dr | dg | db) != 0 ? 3 : 1);\n"
"            }\n"
"            return true;\n"
"        }\n"
"    }\n"
"    if (params.mode == 1u) {\n"
"        float weight = gradient_weight(gradient, params, x, y, gid);\n"
"        for (uint d = 0u; d < 3u; ++d) {\n"
"            int val = int(q[d]) + int(tri_noise(blue_noise,\n"
"                                                params,\n"
"                                                int(x),\n"
"                                                int(y),\n"
"                                                int(d)) * weight * 32.0f);\n"
"            val = val < 0 ? 0 : (val > 255 ? 255 : val);\n"
"            q[d] = uchar(val);\n"
"        }\n"
"    }\n"
"    // Match CPU unsigned phases and truncate the bias before addition.\n"
"    // Keep source untouched for the emitted-color comparison below.\n"
"    if (params.mode == 2u || params.mode == 3u) {\n"
"        for (uint d = 0u; d < 3u; ++d) {\n"
"            uint phase;\n"
"            float noise;\n"
"            if (params.mode == 2u) {\n"
"                phase = (x + d * 67u + y * 236u) * 119u;\n"
"                noise = float(phase & 255u) / 128.0f - 1.0f;\n"
"            } else {\n"
"                phase = ((x + d * 29u) ^ (y * 149u)) * 1234u;\n"
"                noise = float(phase & 511u) / 256.0f - 1.0f;\n"
"            }\n"
"            int val = int(q[d]) +\n"
"                int((noise * params.positional_strength) * 32.0f);\n"
"            q[d] = uchar(clamp(val, 0, 255));\n"
"        }\n",
"    }\n"
"    query = uchar3(q[0], q[1], q[2]);\n"
"    return false;\n"
"}\n"
"static void finish_pixel(\n"
"    device uchar *result,\n"
"    constant Params& params,\n"
"    device const uchar *accumulation,\n"
"    device const uchar *accumulation_valid_mask,\n"
"    device uchar *accumulation_result_mask,\n"
"    uint gid,\n"
"    uchar3 source,\n"
"    uint best)\n"
"{\n"
"    uchar3 retained;\n"
"    if (params.has_6delta != 0u &&\n"
"        gid < params.accumulation_pixel_count &&\n"
"        (gid >= params.accumulation_valid_mask_size ||\n"
"         accumulation_valid_mask[gid] != 0u)) {\n"
"        retained = uchar3(accumulation[gid * 3u + 0u],\n"
"                          accumulation[gid * 3u + 1u],\n"
"                          accumulation[gid * 3u + 2u]);\n"
"        if (color_distance(source, retained) <=\n"
"            palette_distance(params.sixdelta_palette, source, best)) {\n"
"            result[gid] = uchar(params.accumulation_keycolor);\n"
"            if (gid < params.accumulation_result_mask_size) {\n"
"                accumulation_result_mask[gid] = uchar(1);\n"
"            }\n"
"            return;\n"
"        }\n"
"    }\n"
"    result[gid] = uchar(best);\n"
"}\n"
"kernel void sixel_gpu_palette_apply(\n"
"    device uchar *result [[buffer(0)]],\n"
"    device const uchar *pixels [[buffer(1)]],\n"
"    constant uchar *palette [[buffer(2)]],\n"
"    device const uchar *transparent_mask [[buffer(3)]],\n"
"    device const uchar *blue_noise [[buffer(4)]],\n"
"    device const uchar *gradient [[buffer(5)]],\n"
"    constant Params& params [[buffer(6)]],\n"
"    device const uchar *accumulation [[buffer(7)]],\n"
"    device const uchar *accumulation_valid_mask [[buffer(8)]],\n"
"    device uchar *accumulation_result_mask [[buffer(9)]],\n"
"    constant EytzingerEntry *eytzinger_sorted [[buffer(11)]],\n"
"    constant uint *eytzinger_rank [[buffer(10)]],\n"
"    constant EytzingerParams& eytzinger_params [[buffer(12)]],\n"
"    device const GridCell *grid [[buffer(14)]],\n"
"    device const uint *ordered [[buffer(15)]],\n"
"    uint gid [[thread_position_in_grid]])\n"
"{\n"
"    uchar3 query;\n"
"    uchar3 source;\n"
"    uint best;\n"
"    uint best_dist;\n"
"    if (prepare_query(result, pixels, transparent_mask, blue_noise,\n"
"                      gradient, params, accumulation,\n",
"                      accumulation_valid_mask, accumulation_result_mask,\n"
"                      gid, query, source)) {\n"
"        return;\n"
"    }\n"
"    if (params.lookup_mode == 3u) {\n"
"        best = ordered_palette_lookup(palette, ordered,\n"
"                                      params.ncolors, query);\n"
"    } else if (params.lookup_mode == 2u) {\n"
"        best = grid_palette_lookup(palette, grid, params.ncolors, query);\n"
"    } else if (params.lookup_mode == 1u) {\n"
"        best = eytzinger_palette_lookup(palette,\n"
"                                        eytzinger_sorted,\n"
"                                        eytzinger_rank,\n"
"                                        eytzinger_params,\n"
"                                        query);\n"
"    } else {\n"
"        best = 0u;\n"
"        best_dist = 0xffffffffu;\n"
"        for (uint i = 0u; i < params.ncolors; ++i) {\n"
"            uint dist = palette_distance(palette, query, i);\n"
"            if (dist < best_dist) {\n"
"                best_dist = dist;\n"
"                best = i;\n"
"            }\n"
"        }\n"
"    }\n"
"    finish_pixel(result, params, accumulation, accumulation_valid_mask,\n"
"                 accumulation_result_mask, gid, source, best);\n"
"}\n",
"struct DequantParams {\n"
"    uint pixel_count;\n"
"    uint width;\n"
"    uint height;\n"
"    uint ncolors;\n"
"    int similarity_bias;\n"
"};\n"
"static uint dequant_color_diff(uchar3 a, uchar3 b)\n"
"{\n"
"    int dr = int(a.x) - int(b.x);\n"
"    int dg = int(a.y) - int(b.y);\n"
"    int db = int(a.z) - int(b.z);\n"
"    return uint(dr * dr + dg * dg + db * db);\n"
"}\n"
"static bool dequant_same_color(uchar3 a, uchar3 b)\n"
"{\n"
"    return all(a == b);\n"
"}\n"
"static uint dequant_similarity_weight(device const uchar *palette,\n"
"                                      constant DequantParams& params,\n"
"                                      uchar3 center,\n"
"                                      uchar3 neighbor)\n"
"{\n"
"    uchar3 avg;\n"
"    uint distance;\n"
"    uint base_distance;\n"
"    uint min_diff = 0xffffffffu;\n"
"    int bias;\n"
"    if (dequant_same_color(center, neighbor)) {\n"
"        return 7u;\n"
"    }\n"
"    avg = uchar3(uchar((uint(center.x) + uint(neighbor.x)) >> 1),\n"
"                 uchar((uint(center.y) + uint(neighbor.y)) >> 1),\n"
"                 uchar((uint(center.z) + uint(neighbor.z)) >> 1));\n"
"    distance = dequant_color_diff(avg, center);\n"
"    bias = params.similarity_bias < 1 ? 1 : params.similarity_bias;\n"
"    base_distance = (distance * uint(bias) + 50u) / 100u;\n"
"    if (base_distance == 0u) {\n"
"        base_distance = 1u;\n"
"    }\n"
"    for (uint i = 0u; i < params.ncolors; ++i) {\n"
"        uchar3 p = uchar3(palette[i * 3u + 0u],\n"
"                          palette[i * 3u + 1u],\n"
"                          palette[i * 3u + 2u]);\n"
"        uint diff;\n"
"        if (dequant_same_color(p, center) ||\n"
"            dequant_same_color(p, neighbor)) {\n"
"            continue;\n"
"        }\n"
"        diff = dequant_color_diff(avg, p);\n"
"        if (diff < min_diff) {\n"
"            min_diff = diff;\n"
"        }\n"
"    }\n"
"    if (min_diff == 0xffffffffu) {\n"
"        min_diff = base_distance * 2u;\n"
"    }\n"
"    if (min_diff >= base_distance * 2u) {\n"
"        return 5u;\n"
"    }\n"
"    if (min_diff >= base_distance) {\n"
"        return 8u;\n"
"    }\n"
"    if (min_diff * 6u >= base_distance * 5u) {\n"
"        return 7u;\n"
"    }\n"
"    if (min_diff * 4u >= base_distance * 3u) {\n"
"        return 7u;\n"
"    }\n"
"    if (min_diff * 3u >= base_distance * 2u) {\n"
"        return 5u;\n"
"    }\n"
"    if (min_diff * 5u >= base_distance * 3u) {\n"
"        return 7u;\n"
"    }\n"
"    if (min_diff * 2u >= base_distance) {\n"
"        return 4u;\n"
"    }\n"
"    if (min_diff * 3u >= base_distance) {\n"
"        return 2u;\n"
"    }\n"
"    return 0u;\n",
"}\n"
"static void dequant_store_zero(device uchar *result, uint gid)\n"
"{\n"
"    result[gid * 4u + 0u] = uchar(0);\n"
"    result[gid * 4u + 1u] = uchar(0);\n"
"    result[gid * 4u + 2u] = uchar(0);\n"
"    result[gid * 4u + 3u] = uchar(0);\n"
"}\n"
"kernel void sixel_gpu_dequant_fast4_rgba(\n"
"    device uchar *result [[buffer(0)]],\n"
"    device const uchar *rgba [[buffer(1)]],\n"
"    device const uchar *palette [[buffer(2)]],\n"
"    constant DequantParams& params [[buffer(3)]],\n"
"    uint gid [[thread_position_in_grid]])\n"
"{\n"
"    const int2 offsets[4] = {\n"
"        int2(-1, -1), int2(0, -1), int2(1, -1), int2(-1, 0)\n"
"    };\n"
"    uint x;\n"
"    uint y;\n"
"    uchar3 center_color;\n"
"    uint accum_r;\n"
"    uint accum_g;\n"
"    uint accum_b;\n"
"    uint total;\n"
"    if (gid >= params.pixel_count) {\n"
"        return;\n"
"    }\n"
"    if (rgba[gid * 4u + 3u] == 0u) {\n"
"        dequant_store_zero(result, gid);\n"
"        return;\n"
"    }\n"
"    x = gid % params.width;\n"
"    y = gid / params.width;\n"
"    center_color = uchar3(rgba[gid * 4u + 0u], rgba[gid * 4u + 1u],\n"
"                          rgba[gid * 4u + 2u]);\n"
"    accum_r = uint(center_color.x) * 8u;\n"
"    accum_g = uint(center_color.y) * 8u;\n"
"    accum_b = uint(center_color.z) * 8u;\n"
"    total = 8u;\n"
"    for (uint n = 0u; n < 4u; ++n) {\n"
"        int nx = int(x) + offsets[n].x;\n"
"        int ny = int(y) + offsets[n].y;\n"
"        uint pos;\n"
"        uchar3 neighbor_color;\n"
"        uint weight;\n"
"        if (nx < 0 || ny < 0 || nx >= int(params.width) ||\n"
"            ny >= int(params.height)) {\n"
"            continue;\n"
"        }\n"
"        pos = uint(ny) * params.width + uint(nx);\n"
"        if (rgba[pos * 4u + 3u] == 0u) {\n"
"            continue;\n"
"        }\n"
"        neighbor_color = uchar3(rgba[pos * 4u + 0u],\n"
"                                rgba[pos * 4u + 1u],\n"
"                                rgba[pos * 4u + 2u]);\n"
"        weight = dequant_similarity_weight(palette,\n"
"                                           params,\n"
"                                           center_color,\n"
"                                           neighbor_color);\n"
"        if (weight == 0u) {\n"
"            continue;\n"
"        }\n"
"        accum_r += uint(neighbor_color.x) * weight;\n"
"        accum_g += uint(neighbor_color.y) * weight;\n"
"        accum_b += uint(neighbor_color.z) * weight;\n"
"        total += weight;\n"
"    }\n"
"    result[gid * 4u + 0u] = uchar(accum_r / total);\n"
"    result[gid * 4u + 1u] = uchar(accum_g / total);\n"
"    result[gid * 4u + 2u] = uchar(accum_b / total);\n"
"    result[gid * 4u + 3u] = uchar(255);\n"
"}\n",
NULL
};

static int
sixel_gpu_metal_eytzinger_compare(void const *left, void const *right)
{
    sixel_gpu_metal_eytzinger_pair_t const *a;
    sixel_gpu_metal_eytzinger_pair_t const *b;

    a = (sixel_gpu_metal_eytzinger_pair_t const *)left;
    b = (sixel_gpu_metal_eytzinger_pair_t const *)right;
    if (a->key < b->key) {
        return -1;
    }
    if (a->key > b->key) {
        return 1;
    }
    return 0;
}

static void
sixel_gpu_metal_eytzinger_build(
    unsigned char const *palette,
    int ncolors,
    sixel_gpu_metal_eytzinger_entry_t *sorted,
    uint32_t *rank_lut,
    sixel_gpu_metal_eytzinger_params_t *params)
{
    sixel_gpu_metal_eytzinger_pair_t pairs[SIXEL_PALETTE_MAX];
    int index;
    int key;
    int rank;

    memset(sorted,
           0,
           (size_t)SIXEL_PALETTE_MAX
           * sizeof(sixel_gpu_metal_eytzinger_entry_t));
    memset(rank_lut,
           0,
           (size_t)(SIXEL_GPU_METAL_EYTZINGER_KEY_MAX + 1)
           * sizeof(rank_lut[0]));
    memset(params, 0, sizeof(*params));
    for (index = 0; index < ncolors; ++index) {
        pairs[index].key = (float)palette[index * 3 + 0]
                         + (float)palette[index * 3 + 1]
                         + (float)palette[index * 3 + 2];
        pairs[index].palette_index = (uint32_t)index;
    }
    qsort(pairs,
          (size_t)ncolors,
          sizeof(pairs[0]),
          sixel_gpu_metal_eytzinger_compare);
    for (index = 0; index < ncolors; ++index) {
        sorted[index].key = pairs[index].key;
        sorted[index].palette_index = pairs[index].palette_index;
    }
    for (key = 0; key <= SIXEL_GPU_METAL_EYTZINGER_KEY_MAX; ++key) {
        rank = ncolors - 1;
        for (index = 0; index < ncolors; ++index) {
            if ((float)key <= sorted[index].key) {
                rank = index;
                break;
            }
        }
        rank_lut[key] = (uint32_t)rank;
    }
    params->count = (uint32_t)ncolors;
    params->window = 6U;
}

static void
sixel_gpu_palette_metal_set_message(char const *prefix, NSError *error)
{
    char message[512];
    char const *detail;

    message[0] = '\0';
    detail = NULL;
    if (error != nil && [error localizedDescription] != nil) {
        detail = [[error localizedDescription] UTF8String];
    }
    if (detail != NULL && detail[0] != '\0') {
        (void)snprintf(message, sizeof(message), "%s: %s", prefix, detail);
    } else {
        (void)snprintf(message, sizeof(message), "%s", prefix);
    }
    sixel_helper_set_additional_message(message);
}

static int
sixel_gpu_palette_metal_append_chunks(NSMutableString *source,
                                      char const * const *chunks)
{
    NSString *chunk;
    size_t index;

    for (index = 0U; chunks[index] != NULL; ++index) {
        chunk = [[NSString alloc]
            initWithBytes:chunks[index]
                   length:strlen(chunks[index])
                 encoding:NSUTF8StringEncoding];
        if (chunk == nil) {
            return 0;
        }
        [source appendString:chunk];
        [chunk release];
    }
    return 1;
}

static NSString *
sixel_gpu_palette_metal_create_source(void)
{
    NSMutableString *source;
    NSString *chunk;
    size_t index;

    source = nil;
    chunk = nil;
    index = 0U;

    source = [[NSMutableString alloc] init];
    if (source == nil) {
        return nil;
    }
    for (index = 0U;
            g_sixel_gpu_metal_source_chunks[index] != NULL;
            ++index) {
        chunk = [[NSString alloc]
            initWithBytes:g_sixel_gpu_metal_source_chunks[index]
                   length:strlen(g_sixel_gpu_metal_source_chunks[index])
                 encoding:NSUTF8StringEncoding];
        if (chunk == nil) {
            [source release];
            return nil;
        }
        [source appendString:chunk];
        [chunk release];
        chunk = nil;
    }

    return source;
}

static id<MTLBuffer>
sixel_gpu_metal_ensure_buffer(sixel_gpu_metal_cached_buffer_t *cache,
                              NSUInteger length)
{
    NSUInteger requested_length;

    requested_length = length > 0U ? length : 1U;
    if (cache == NULL || g_sixel_gpu_metal_device == nil) {
        return nil;
    }
    if (cache->buffer != nil && cache->capacity >= requested_length) {
        return cache->buffer;
    }
    if (cache->buffer != nil) {
        [cache->buffer release];
        cache->buffer = nil;
        cache->capacity = 0U;
    }

    cache->buffer =
        [g_sixel_gpu_metal_device
            newBufferWithLength:requested_length
                        options:MTLResourceStorageModeShared];
    if (cache->buffer == nil) {
        return nil;
    }
    cache->capacity = requested_length;
    return cache->buffer;
}

static id<MTLBuffer>
sixel_gpu_metal_upload_buffer(sixel_gpu_metal_cached_buffer_t *cache,
                              void const *bytes,
                              NSUInteger length)
{
    id<MTLBuffer> buffer;

    buffer = sixel_gpu_metal_ensure_buffer(cache, length);
    if (buffer == nil) {
        return nil;
    }
    if (bytes != NULL && length > 0U) {
        memcpy([buffer contents], bytes, length);
    }
    return buffer;
}

static id<MTLBuffer>
sixel_gpu_metal_wrap_dest_buffer(void *bytes, NSUInteger length)
{
    NSUInteger requested_length;

    requested_length = length > 0U ? length : 1U;
    if (bytes == NULL || g_sixel_gpu_metal_device == nil) {
        return nil;
    }

    return [g_sixel_gpu_metal_device
        newBufferWithBytesNoCopy:bytes
                           length:requested_length
                          options:MTLResourceStorageModeShared
                      deallocator:nil];
}

static id<MTLBuffer>
sixel_gpu_metal_upload_palette(void const *bytes, NSUInteger length)
{
    id<MTLBuffer> buffer;
    int force_upload;

    buffer = nil;
    force_upload =
        g_sixel_gpu_metal_palette_buffer.buffer == nil ||
        g_sixel_gpu_metal_palette_buffer.capacity < length;
    buffer = sixel_gpu_metal_ensure_buffer(
        &g_sixel_gpu_metal_palette_buffer,
        length);
    if (buffer == nil) {
        return nil;
    }
    if (bytes == NULL || length == 0U) {
        return buffer;
    }
    if (!force_upload &&
        g_sixel_gpu_metal_palette_shadow_valid != 0 &&
        g_sixel_gpu_metal_palette_shadow_length == length &&
        length <= (NSUInteger)sizeof(g_sixel_gpu_metal_palette_shadow) &&
        memcmp(g_sixel_gpu_metal_palette_shadow, bytes, length) == 0) {
        return buffer;
    }

    memcpy([buffer contents], bytes, length);
    if (length <= (NSUInteger)sizeof(g_sixel_gpu_metal_palette_shadow)) {
        memcpy(g_sixel_gpu_metal_palette_shadow, bytes, length);
        g_sixel_gpu_metal_palette_shadow_length = length;
        g_sixel_gpu_metal_palette_shadow_valid = 1;
    } else {
        g_sixel_gpu_metal_palette_shadow_valid = 0;
        g_sixel_gpu_metal_palette_shadow_length = 0U;
    }
    return buffer;
}

static int
sixel_gpu_palette_metal_prepare_locked(void)
{
    NSError *error;
    NSString *source;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    id<MTLFunction> dequant_function;
    unsigned char dummy;

    error = nil;
    source = nil;
    library = nil;
    function = nil;
    dequant_function = nil;
    dummy = 0U;
    if (g_sixel_gpu_metal_probe_done != 0) {
        return g_sixel_gpu_metal_available;
    }

    g_sixel_gpu_metal_probe_done = 1;
    g_sixel_gpu_metal_device = MTLCreateSystemDefaultDevice();
    if (g_sixel_gpu_metal_device == nil) {
        return 0;
    }
    g_sixel_gpu_metal_queue = [g_sixel_gpu_metal_device newCommandQueue];
    if (g_sixel_gpu_metal_queue == nil) {
        return 0;
    }

    source = sixel_gpu_palette_metal_create_source();
    if (source == nil) {
        return 0;
    }
    library = [g_sixel_gpu_metal_device newLibraryWithSource:source
                                                     options:nil
                                                       error:&error];
    [source release];
    source = nil;
    if (library == nil) {
        return 0;
    }
    function = [library newFunctionWithName:@"sixel_gpu_palette_apply"];
    dequant_function =
        [library newFunctionWithName:@"sixel_gpu_dequant_fast4_rgba"];
    if (function == nil) {
        [library release];
        return 0;
    }
    if (dequant_function == nil) {
        [function release];
        [library release];
        return 0;
    }
    g_sixel_gpu_metal_pipeline =
        [g_sixel_gpu_metal_device newComputePipelineStateWithFunction:function
                                                                 error:&error];
    [function release];
    function = nil;
    if (g_sixel_gpu_metal_pipeline == nil) {
        [dequant_function release];
        [library release];
        return 0;
    }
    g_sixel_gpu_metal_dequant_pipeline =
        [g_sixel_gpu_metal_device
            newComputePipelineStateWithFunction:dequant_function
                                          error:&error];
    [dequant_function release];
    dequant_function = nil;
    function = [library newFunctionWithName:@"sixel_gpu_grid_build"];
    if (function != nil) {
        g_sixel_gpu_metal_grid_build = [g_sixel_gpu_metal_device
            newComputePipelineStateWithFunction:function error:&error];
        [function release];
        function = nil;
    }
    function = [library newFunctionWithName:@"sixel_gpu_ordered_sort"];
    if (function != nil) {
        g_sixel_gpu_metal_ordered_sort = [g_sixel_gpu_metal_device
            newComputePipelineStateWithFunction:function error:&error];
        [function release];
        function = nil;
    }
    function = [library newFunctionWithName:@"sixel_gpu_ordered_seeds"];
    if (function != nil) {
        g_sixel_gpu_metal_ordered_seeds = [g_sixel_gpu_metal_device
            newComputePipelineStateWithFunction:function error:&error];
        [function release];
        function = nil;
    }
    [library release];
    library = nil;
    if (g_sixel_gpu_metal_dequant_pipeline == nil) {
        return 0;
    }
    g_sixel_gpu_metal_blue_noise_buffer =
        [g_sixel_gpu_metal_device
            newBufferWithBytes:sixel_bn64
                        length:sizeof(sixel_bn64)
                       options:MTLResourceStorageModeShared];
    if (g_sixel_gpu_metal_blue_noise_buffer == nil) {
        return 0;
    }
    g_sixel_gpu_metal_dummy_buffer =
        [g_sixel_gpu_metal_device
            newBufferWithBytes:&dummy
                        length:1U
                       options:MTLResourceStorageModeShared];
    if (g_sixel_gpu_metal_dummy_buffer == nil) {
        return 0;
    }

    g_sixel_gpu_metal_available = 1;
    return 1;
}

/* The simdgroup kernels' lane layout assumes 32-wide simdgroups, and
 * dispatch needs at least one whole simdgroup per threadgroup. */
static id<MTLComputePipelineState>
sixel_gpu_palette_metal_simdgroup_pipeline(id<MTLLibrary> library,
                                           NSString *name)
{
    NSError *error;
    id<MTLFunction> function;
    id<MTLComputePipelineState> pipeline;

    error = nil;
    function = [library newFunctionWithName:name];
    if (function == nil) {
        return nil;
    }
    pipeline =
        [g_sixel_gpu_metal_device newComputePipelineStateWithFunction:function
                                                                 error:&error];
    [function release];
    if (pipeline != nil &&
            ([pipeline threadExecutionWidth] != 32U ||
             [pipeline maxTotalThreadsPerThreadgroup] < 32U)) {
        [pipeline release];
        pipeline = nil;
    }
    return pipeline;
}

/*
 * Build the GEMM pipeline on first request.  Failure leaves the pipeline
 * nil and the direct scan in use; it never disables Metal palette apply.
 */
static int
sixel_gpu_palette_metal_prepare_gemm_locked(void)
{
    NSError *error;
    NSMutableString *source;
    id<MTLLibrary> library;

    error = nil;
    library = nil;
    if (g_sixel_gpu_metal_gemm_probe_done != 0) {
        return g_sixel_gpu_metal_gemm_pipeline != nil;
    }
    g_sixel_gpu_metal_gemm_probe_done = 1;
    if (g_sixel_gpu_metal_device == nil ||
            ![g_sixel_gpu_metal_device supportsFamily:MTLGPUFamilyApple7]) {
        return 0;
    }
    source = [[NSMutableString alloc] init];
    if (source == nil) {
        return 0;
    }
    if (!sixel_gpu_palette_metal_append_chunks(
                source, g_sixel_gpu_metal_source_chunks) ||
            !sixel_gpu_palette_metal_append_chunks(
                source, g_sixel_gpu_metal_gemm_source_chunks)) {
        [source release];
        return 0;
    }
    library = [g_sixel_gpu_metal_device newLibraryWithSource:source
                                                     options:nil
                                                       error:&error];
    [source release];
    if (library == nil) {
        return 0;
    }
    g_sixel_gpu_metal_gemm_pipeline =
        sixel_gpu_palette_metal_simdgroup_pipeline(
            library, @"sixel_gpu_palette_apply_gemm");
    /* Block culling shares the library; without it, blockcull keeps the
     * exhaustive GEMM, which returns the same indexes. */
    if (g_sixel_gpu_metal_gemm_pipeline != nil) {
        g_sixel_gpu_metal_blockcull_pipeline =
            sixel_gpu_palette_metal_simdgroup_pipeline(
                library, @"sixel_gpu_palette_apply_blockcull");
    }
    [library release];
    return g_sixel_gpu_metal_gemm_pipeline != nil;
}

/* Palette tiles for the GEMM kernel: tile t holds colors 8t .. 8t + 7 as
 * columns of an 8x8 row-major matrix with rows -2r, -2g, -2b, |c|^2 and
 * zero rows 4..7.  Missing colors get an unreachable score. */
static NSUInteger
sixel_gpu_palette_metal_build_gemm_tiles(unsigned char const *palette,
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

int
sixel_gpu_palette_metal_is_available(void)
{
    int available;

    available = 0;
    (void)pthread_mutex_lock(&g_sixel_gpu_metal_lock);
    available = sixel_gpu_palette_metal_prepare_locked();
    (void)pthread_mutex_unlock(&g_sixel_gpu_metal_lock);
    return available;
}

static void
sixel_gpu_palette_metal_fill_params(
    sixel_gpu_metal_params_t *params,
    sixel_gpu_palette_request_t const *request)
{
    memset(params, 0, sizeof(*params));
    params->pixel_count = (uint32_t)request->pixel_count;
    params->width = (uint32_t)request->width;
    params->height = (uint32_t)request->height;
    params->ncolors = (uint32_t)request->ncolors;
    params->lookup_mode = request->lut_policy == SIXEL_LUT_POLICY_EYTZINGER ?
        SIXEL_GPU_METAL_LOOKUP_EYTZINGER : SIXEL_GPU_METAL_LOOKUP_DIRECT;
    params->mode =
        request->method_for_diffuse == SIXEL_DIFFUSE_BLUENOISE_DITHER ?
        SIXEL_GPU_METAL_MODE_BLUENOISE :
        SIXEL_GPU_METAL_MODE_NONE;
    if (request->method_for_diffuse == SIXEL_DIFFUSE_A_DITHER) {
        params->mode = SIXEL_GPU_METAL_MODE_A_DITHER;
    } else if (request->method_for_diffuse == SIXEL_DIFFUSE_X_DITHER) {
        params->mode = SIXEL_GPU_METAL_MODE_X_DITHER;
    }
    params->positional_strength = request->positional_strength;
    if (request->transparent_mask != NULL &&
            request->transparent_keycolor >= 0 &&
            request->transparent_keycolor < SIXEL_PALETTE_MAX) {
        params->has_transparent = 1U;
        params->transparent_size = (uint32_t)request->transparent_mask_size;
        params->transparent_keycolor =
            (uint32_t)request->transparent_keycolor;
    }
    if (request->has_6delta_accumulation != 0 &&
            request->accumulation_pixels != NULL &&
            request->accumulation_keycolor >= 0 &&
            request->accumulation_keycolor < SIXEL_PALETTE_MAX) {
        params->has_6delta = 1U;
        params->accumulation_pixel_count =
            (uint32_t)(request->accumulation_pixels_size / 3U);
        params->accumulation_valid_mask_size =
            (uint32_t)request->accumulation_valid_mask_size;
        params->accumulation_keycolor =
            (uint32_t)request->accumulation_keycolor;
        params->sixdelta_threshold =
            (uint32_t)request->sixdelta_threshold;
        params->sixdelta_metric = (uint32_t)request->sixdelta_metric;
        if (request->residual_mask != NULL) {
            params->residual_columns = request->residual_mask->columns;
            params->residual_rows = request->residual_mask->rows;
            params->residual_tiled = request->residual_mask->tiled;
            params->residual_threshold = request->residual_mask->threshold;
            memcpy(params->residual_cells, request->residual_mask->cells,
                   sizeof(params->residual_cells));
        }
        params->accumulation_result_mask_size =
            (uint32_t)request->accumulation_result_mask_size;
        /* Lookup keeps its working palette; only keep uses emitted colors.
         * Embed this palette-sized view in the existing parameter upload. */
        memcpy(params->sixdelta_palette,
               request->sixdelta_comparison_palette != NULL
                   ? request->sixdelta_comparison_palette : request->palette,
               (size_t)request->ncolors * 3U);
    }
    params->bluenoise_strength = request->bluenoise_strength;
    params->bluenoise_gradient_factor = request->bluenoise_gradient_factor;
    params->bluenoise_phase_x = (int32_t)request->bluenoise_phase_x;
    params->bluenoise_phase_y = (int32_t)request->bluenoise_phase_y;
    params->bluenoise_channel_rgb =
        request->bluenoise_channel_rgb != 0 ? 1U : 0U;
    params->gradient_size = (uint32_t)request->bluenoise_gradient_map_size;
    params->gradient_width = (uint32_t)request->bluenoise_gradient_width;
    params->gradient_height = (uint32_t)request->bluenoise_gradient_height;
}

SIXELSTATUS
sixel_gpu_palette_metal_apply(sixel_gpu_palette_request_t const *request)
{
    SIXELSTATUS status;
    sixel_gpu_metal_params_t params;
    id<MTLBuffer> result_buffer;
    id<MTLBuffer> pixel_buffer;
    id<MTLBuffer> palette_buffer;
    id<MTLBuffer> transparent_buffer;
    id<MTLBuffer> blue_noise_buffer;
    id<MTLBuffer> gradient_buffer;
    id<MTLBuffer> params_buffer;
    id<MTLBuffer> accumulation_buffer;
    id<MTLBuffer> accumulation_valid_buffer;
    id<MTLBuffer> accumulation_result_buffer;
    id<MTLBuffer> eytzinger_rank_buffer;
    id<MTLBuffer> eytzinger_sorted_buffer;
    id<MTLBuffer> eytzinger_params_buffer;
    id<MTLCommandBuffer> command_buffer;
    id<MTLComputeCommandEncoder> encoder;
    sixel_gpu_metal_eytzinger_entry_t eytzinger_sorted[
        SIXEL_PALETTE_MAX];
    uint32_t eytzinger_rank[
        SIXEL_GPU_METAL_EYTZINGER_KEY_MAX + 1];
    sixel_gpu_metal_eytzinger_params_t eytzinger_params;
    NSUInteger threads_per_group;
    NSUInteger groups;
    NSUInteger result_length;
    NSUInteger pixel_length;
    NSUInteger palette_length;
    NSUInteger transparent_length;
    NSUInteger gradient_length;
    NSUInteger accumulation_length;
    NSUInteger accumulation_valid_length;
    NSUInteger accumulation_result_length;
    NSUInteger eytzinger_sorted_length;
    NSUInteger eytzinger_rank_length;
    NSUInteger gemm_tiles_length;
    float gemm_tiles[(SIXEL_PALETTE_MAX + 7) / 8 * 64];
    id<MTLBuffer> gemm_tiles_buffer;
    id<MTLComputePipelineState> pipeline;
    int use_gemm;
    int use_blockcull;
    int use_grid;
    int rebuild_grid;
    id<MTLBuffer> grid_buffer;
    id<MTLBuffer> ordered_buffer;
    int use_ordered;
    int rebuild_ordered;
    NSUInteger sort_width;
    NSUInteger dispatched;
    int result_buffer_direct;
    int locked;

    status = SIXEL_FALSE;
    memset(&params, 0, sizeof(params));
    result_buffer = nil;
    pixel_buffer = nil;
    palette_buffer = nil;
    transparent_buffer = nil;
    blue_noise_buffer = nil;
    gradient_buffer = nil;
    params_buffer = nil;
    accumulation_buffer = nil;
    accumulation_valid_buffer = nil;
    accumulation_result_buffer = nil;
    eytzinger_rank_buffer = nil;
    eytzinger_sorted_buffer = nil;
    eytzinger_params_buffer = nil;
    command_buffer = nil;
    encoder = nil;
    memset(eytzinger_sorted, 0, sizeof(eytzinger_sorted));
    memset(eytzinger_rank, 0, sizeof(eytzinger_rank));
    memset(&eytzinger_params, 0, sizeof(eytzinger_params));
    threads_per_group = 0U;
    groups = 0U;
    result_length = 0U;
    pixel_length = 0U;
    palette_length = 0U;
    transparent_length = 0U;
    gradient_length = 0U;
    accumulation_length = 0U;
    accumulation_valid_length = 0U;
    accumulation_result_length = 0U;
    eytzinger_sorted_length = 0U;
    eytzinger_rank_length = 0U;
    gemm_tiles_length = 0U;
    gemm_tiles_buffer = nil;
    pipeline = nil;
    use_gemm = 0;
    use_blockcull = 0;
    use_grid = request != NULL &&
        request->lut_policy == SIXEL_LUT_POLICY_GRIDLUT;
    rebuild_grid = 0;
    grid_buffer = nil;
    use_ordered = request != NULL &&
        request->lut_policy == SIXEL_LUT_POLICY_ORDERED;
    rebuild_ordered = 0;
    ordered_buffer = nil;
    sort_width = 0U;
    dispatched = 0U;
    result_buffer_direct = 0;
    locked = 0;

    if (request == NULL) {
        sixel_helper_set_additional_message(
            "gpu palette apply: Metal request is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    @autoreleasepool {
        (void)pthread_mutex_lock(&g_sixel_gpu_metal_lock);
        locked = 1;
        if (!sixel_gpu_palette_metal_prepare_locked()) {
            sixel_helper_set_additional_message(
                "gpu palette apply: Metal is not available.");
            status = SIXEL_FEATURE_ERROR;
            goto end;
        }

        if (request->test_hooks != NULL) {
            request->test_hooks->entered = 1;
            request->test_hooks->matrix_supported =
                [g_sixel_gpu_metal_device supportsFamily:MTLGPUFamilyApple7];
            request->test_hooks->kernel = 0;
            request->test_hooks->rebuild = 0;
            request->test_hooks->published = 0;
        }

        result_length = (NSUInteger)request->pixel_count *
            (NSUInteger)sizeof(sixel_index_t);
        pixel_length = (NSUInteger)request->pixel_count * 3U;
        palette_length = (NSUInteger)request->ncolors * 3U;
        eytzinger_sorted_buffer = g_sixel_gpu_metal_dummy_buffer;
        eytzinger_rank_buffer = g_sixel_gpu_metal_dummy_buffer;
        eytzinger_params_buffer = g_sixel_gpu_metal_dummy_buffer;
        if (request->lut_policy == SIXEL_LUT_POLICY_EYTZINGER) {
            sixel_gpu_metal_eytzinger_build(request->palette,
                                            request->ncolors,
                                            eytzinger_sorted,
                                            eytzinger_rank,
                                            &eytzinger_params);
            eytzinger_sorted_length = (NSUInteger)request->ncolors
                * sizeof(eytzinger_sorted[0]);
            eytzinger_rank_length = (NSUInteger)(
                SIXEL_GPU_METAL_EYTZINGER_KEY_MAX + 1)
                * sizeof(eytzinger_rank[0]);
        }
        transparent_length = request->transparent_mask != NULL ?
            (NSUInteger)request->transparent_mask_size : 1U;
        gradient_length = request->bluenoise_gradient_map != NULL ?
            (NSUInteger)request->bluenoise_gradient_map_size : 1U;
        accumulation_length =
            request->has_6delta_accumulation != 0 ?
            (NSUInteger)request->accumulation_pixels_size : 1U;
        accumulation_valid_length =
            request->accumulation_valid_mask != NULL ?
            (NSUInteger)request->accumulation_valid_mask_size : 1U;
        accumulation_result_length =
            request->accumulation_result_mask != NULL ?
            (NSUInteger)request->accumulation_result_mask_size : 1U;

        result_buffer =
            sixel_gpu_metal_wrap_dest_buffer(request->dest, result_length);
        if (result_buffer != nil) {
            result_buffer_direct = 1;
        } else {
            result_buffer = sixel_gpu_metal_ensure_buffer(
                &g_sixel_gpu_metal_result_buffer,
                result_length);
        }
        pixel_buffer = sixel_gpu_metal_upload_buffer(
            &g_sixel_gpu_metal_pixel_buffer,
            request->pixels,
            pixel_length);
        palette_buffer =
            sixel_gpu_metal_upload_palette(request->palette, palette_length);
        if (request->transparent_mask != NULL) {
            transparent_buffer = sixel_gpu_metal_upload_buffer(
                &g_sixel_gpu_metal_transparent_buffer,
                request->transparent_mask,
                transparent_length);
        } else {
            transparent_buffer = g_sixel_gpu_metal_dummy_buffer;
        }
        blue_noise_buffer = g_sixel_gpu_metal_blue_noise_buffer;
        if (request->bluenoise_gradient_map != NULL) {
            gradient_buffer = sixel_gpu_metal_upload_buffer(
                &g_sixel_gpu_metal_gradient_buffer,
                request->bluenoise_gradient_map,
                gradient_length);
        } else {
            gradient_buffer = g_sixel_gpu_metal_dummy_buffer;
        }
        sixel_gpu_palette_metal_fill_params(&params, request);
        grid_buffer = g_sixel_gpu_metal_dummy_buffer;
        if (use_grid) {
            if (g_sixel_gpu_metal_grid_build == nil) {
                sixel_helper_set_additional_message(
                    "gpu palette apply: gridlut pipeline is unavailable.");
                status = SIXEL_FEATURE_ERROR;
                goto end;
            }
            params.lookup_mode = SIXEL_GPU_METAL_LOOKUP_GRIDLUT;
            grid_buffer = sixel_gpu_metal_ensure_buffer(
                &g_sixel_gpu_metal_grid, 4096U * 65U);
            rebuild_grid = g_sixel_gpu_metal_grid_colors != request->ncolors
                || memcmp(g_sixel_gpu_metal_grid_palette, request->palette,
                          palette_length) != 0;
        }
        ordered_buffer = g_sixel_gpu_metal_dummy_buffer;
        if (use_ordered) {
            if (g_sixel_gpu_metal_ordered_sort == nil ||
                    g_sixel_gpu_metal_ordered_seeds == nil) {
                sixel_helper_set_additional_message(
                    "gpu palette apply: ordered pipelines are unavailable.");
                status = SIXEL_FEATURE_ERROR;
                goto end;
            }
            params.lookup_mode = SIXEL_GPU_METAL_LOOKUP_ORDERED;
            ordered_buffer = sixel_gpu_metal_ensure_buffer(
                &g_sixel_gpu_metal_ordered, 163840U * sizeof(uint32_t));
            rebuild_ordered =
                g_sixel_gpu_metal_ordered_colors != request->ncolors ||
                memcmp(g_sixel_gpu_metal_ordered_palette, request->palette,
                       palette_length) != 0;
        }
        params_buffer = sixel_gpu_metal_upload_buffer(
            &g_sixel_gpu_metal_params_buffer,
            &params,
            sizeof(params));
        if (request->lut_policy == SIXEL_LUT_POLICY_EYTZINGER) {
            eytzinger_sorted_buffer = sixel_gpu_metal_upload_buffer(
                &g_sixel_gpu_metal_eytzinger_sorted_buffer,
                eytzinger_sorted,
                eytzinger_sorted_length);
            eytzinger_rank_buffer = sixel_gpu_metal_upload_buffer(
                &g_sixel_gpu_metal_eytzinger_rank_buffer,
                eytzinger_rank,
                eytzinger_rank_length);
            eytzinger_params_buffer = sixel_gpu_metal_upload_buffer(
                &g_sixel_gpu_metal_eytzinger_params_buffer,
                &eytzinger_params,
                sizeof(eytzinger_params));
        }
        if (request->has_6delta_accumulation != 0) {
            accumulation_buffer = sixel_gpu_metal_upload_buffer(
                &g_sixel_gpu_metal_accumulation_buffer,
                request->accumulation_pixels,
                accumulation_length);
        } else {
            accumulation_buffer = g_sixel_gpu_metal_dummy_buffer;
        }
        if (request->accumulation_valid_mask != NULL) {
            accumulation_valid_buffer = sixel_gpu_metal_upload_buffer(
                &g_sixel_gpu_metal_accumulation_valid_buffer,
                request->accumulation_valid_mask,
                accumulation_valid_length);
        } else {
            accumulation_valid_buffer = g_sixel_gpu_metal_dummy_buffer;
        }
        if (request->accumulation_result_mask != NULL) {
            accumulation_result_buffer = sixel_gpu_metal_upload_buffer(
                &g_sixel_gpu_metal_accumulation_result_buffer,
                request->accumulation_result_mask,
                accumulation_result_length);
        } else {
            accumulation_result_buffer = g_sixel_gpu_metal_dummy_buffer;
        }
        if (request->test_hooks != NULL) {
            request->test_hooks->rebuild = rebuild_grid || rebuild_ordered;
            if (request->test_hooks->fault == 1) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }
        if (result_buffer == nil || pixel_buffer == nil ||
                palette_buffer == nil || transparent_buffer == nil ||
                blue_noise_buffer == nil || gradient_buffer == nil ||
                params_buffer == nil || accumulation_buffer == nil ||
                accumulation_valid_buffer == nil ||
                accumulation_result_buffer == nil ||
                eytzinger_rank_buffer == nil ||
                eytzinger_sorted_buffer == nil ||
                eytzinger_params_buffer == nil || grid_buffer == nil ||
                ordered_buffer == nil) {
            sixel_helper_set_additional_message(
                "gpu palette apply: Metal buffer allocation failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }

        /* Direct lookup runs as a matrix product where the device supports
         * simdgroup matrices; both kernels return the same indexes. */
        pipeline = g_sixel_gpu_metal_pipeline;
        if (!use_grid && !use_ordered &&
                request->lut_policy != SIXEL_LUT_POLICY_EYTZINGER &&
                sixel_gpu_palette_metal_prepare_gemm_locked()) {
            gemm_tiles_length = sixel_gpu_palette_metal_build_gemm_tiles(
                request->palette, request->ncolors, gemm_tiles);
            gemm_tiles_buffer = sixel_gpu_metal_upload_buffer(
                &g_sixel_gpu_metal_gemm_tiles_buffer,
                gemm_tiles,
                gemm_tiles_length);
            if (gemm_tiles_buffer != nil) {
                pipeline = g_sixel_gpu_metal_gemm_pipeline;
                use_gemm = 1;
                /* Culling keeps the full tiles for blocks that overflow
                 * its survivor capacity.  A palette of at most four tiles
                 * costs less to scan than to cull; both return the same
                 * indexes. */
                if (request->lut_policy == SIXEL_LUT_POLICY_BLOCKCULL &&
                        request->ncolors > 32 &&
                        g_sixel_gpu_metal_blockcull_pipeline != nil) {
                    pipeline = g_sixel_gpu_metal_blockcull_pipeline;
                    use_blockcull = 1;
                }
            }
        }
        if (request->test_hooks != NULL) {
            request->test_hooks->kernel = use_ordered ? 5 : use_grid ? 4 :
                use_blockcull ? 3 : use_gemm ? 2 : 1;
        }
        if (sixel_trace_topic_is_enabled("gpu_contract")) {
            fprintf(stderr, "LSXGPU2|consumer=palette-apply|kernel=%s\n",
                    use_ordered ? "ordered" : use_grid ? "gridlut" :
                    use_blockcull ? "blockcull" : use_gemm ? "gemm" :
                    request->lut_policy == SIXEL_LUT_POLICY_EYTZINGER ?
                    "eytzinger" : "direct");
        }

        if (use_ordered && sixel_trace_topic_is_enabled("gpu_contract")) {
            fprintf(stderr, "LSXGPU3|lookup=ordered|table=%s\n",
                    rebuild_ordered ? "rebuild" : "reuse");
        }

        if (request->test_hooks != NULL && request->test_hooks->fault == 2) {
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }
        command_buffer = [g_sixel_gpu_metal_queue commandBuffer];
        encoder = [command_buffer computeCommandEncoder];
        if (command_buffer == nil || encoder == nil) {
            sixel_helper_set_additional_message(
                "gpu palette apply: Metal command encoder creation failed.");
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }

        if (rebuild_ordered) {
            /* The two constructors and apply share one command buffer.
             * Tracked buffers enforce visibility at encoder boundaries. */
            sort_width = 32U;
            while (sort_width < (NSUInteger)request->ncolors) {
                sort_width <<= 1;
            }
            if ([g_sixel_gpu_metal_ordered_sort
                    maxTotalThreadsPerThreadgroup] < sort_width) {
                status = SIXEL_FEATURE_ERROR;
                goto end;
            }
            [encoder setComputePipelineState:g_sixel_gpu_metal_ordered_sort];
            [encoder setBuffer:palette_buffer offset:0 atIndex:2];
            [encoder setBuffer:params_buffer offset:0 atIndex:6];
            [encoder setBuffer:ordered_buffer offset:0 atIndex:15];
            [encoder dispatchThreadgroups:
                MTLSizeMake((NSUInteger)request->ncolors, 1U, 1U)
                threadsPerThreadgroup:MTLSizeMake(sort_width, 1U, 1U)];
            [encoder endEncoding];
            encoder = [command_buffer computeCommandEncoder];
            if (encoder == nil) {
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            [encoder setComputePipelineState:
                g_sixel_gpu_metal_ordered_seeds];
            [encoder setBuffer:palette_buffer offset:0 atIndex:2];
            [encoder setBuffer:params_buffer offset:0 atIndex:6];
            [encoder setBuffer:ordered_buffer offset:0 atIndex:15];
            threads_per_group = [g_sixel_gpu_metal_ordered_seeds
                maxTotalThreadsPerThreadgroup];
            if (threads_per_group > 128U) {
                threads_per_group = 128U;
            }
            groups = (32768U + threads_per_group - 1U) / threads_per_group;
            [encoder dispatchThreadgroups:MTLSizeMake(groups, 1U, 1U)
                threadsPerThreadgroup:
                    MTLSizeMake(threads_per_group, 1U, 1U)];
            [encoder endEncoding];
            encoder = [command_buffer computeCommandEncoder];
            if (encoder == nil) {
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
        }
        if (rebuild_grid) {
            /* Separate encoders give tracked buffers a write/read dependency
             * without a CPU round trip between building and applying. */
            [encoder setComputePipelineState:g_sixel_gpu_metal_grid_build];
            [encoder setBuffer:palette_buffer offset:0 atIndex:2];
            [encoder setBuffer:params_buffer offset:0 atIndex:6];
            [encoder setBuffer:grid_buffer offset:0 atIndex:14];
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
            encoder = [command_buffer computeCommandEncoder];
            if (encoder == nil) {
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
        }
        [encoder setComputePipelineState:pipeline];
        if (!use_gemm) {
            [encoder setBuffer:grid_buffer offset:0 atIndex:14];
            [encoder setBuffer:ordered_buffer offset:0 atIndex:15];
        }
        [encoder setBuffer:result_buffer offset:0 atIndex:0];
        [encoder setBuffer:pixel_buffer offset:0 atIndex:1];
        [encoder setBuffer:palette_buffer offset:0 atIndex:2];
        [encoder setBuffer:transparent_buffer offset:0 atIndex:3];
        [encoder setBuffer:blue_noise_buffer offset:0 atIndex:4];
        [encoder setBuffer:gradient_buffer offset:0 atIndex:5];
        [encoder setBuffer:params_buffer offset:0 atIndex:6];
        [encoder setBuffer:accumulation_buffer offset:0 atIndex:7];
        [encoder setBuffer:accumulation_valid_buffer offset:0 atIndex:8];
        [encoder setBuffer:accumulation_result_buffer offset:0 atIndex:9];
        [encoder setBuffer:eytzinger_rank_buffer offset:0 atIndex:10];
        [encoder setBuffer:eytzinger_sorted_buffer offset:0 atIndex:11];
        [encoder setBuffer:eytzinger_params_buffer offset:0 atIndex:12];
        if (use_gemm) {
            [encoder setBuffer:gemm_tiles_buffer offset:0 atIndex:13];
        }

        threads_per_group = [pipeline maxTotalThreadsPerThreadgroup];
        if (threads_per_group > 256U) {
            threads_per_group = 256U;
        }
        if (use_gemm) {
            /* Whole simdgroups; the kernel's threadgroup arrays hold 8. */
            threads_per_group -= threads_per_group % 32U;
        }
        if (threads_per_group == 0U) {
            threads_per_group = 1U;
        }
        dispatched = (NSUInteger)request->pixel_count;
        if (use_blockcull) {
            /* One simdgroup per 8x8 pixel block. */
            dispatched = (NSUInteger)((request->width + 7) / 8) *
                (NSUInteger)((request->height + 7) / 8) * 32U;
        }
        groups = (dispatched + threads_per_group - 1U) / threads_per_group;
        [encoder dispatchThreadgroups:MTLSizeMake(groups, 1U, 1U)
                threadsPerThreadgroup:MTLSizeMake(threads_per_group, 1U, 1U)];
        [encoder endEncoding];
        [command_buffer commit];
        [command_buffer waitUntilCompleted];
        if (request->test_hooks != NULL && request->test_hooks->fault == 3) {
            /* GPU writes may already exist. The caller must reject them;
             * no cache identity or retained history is published here. */
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }
        if ([command_buffer status] != MTLCommandBufferStatusCompleted) {
            sixel_gpu_palette_metal_set_message(
                "gpu palette apply: Metal command failed",
                [command_buffer error]);
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }

        if (rebuild_ordered) {
            memcpy(g_sixel_gpu_metal_ordered_palette, request->palette,
                   palette_length);
            g_sixel_gpu_metal_ordered_colors = request->ncolors;
        }
        if (rebuild_grid) {
            memcpy(g_sixel_gpu_metal_grid_palette, request->palette,
                   palette_length);
            g_sixel_gpu_metal_grid_colors = request->ncolors;
        }
        if (request->test_hooks != NULL) {
            request->test_hooks->published = 1;
        }
        if (result_buffer_direct == 0) {
            memcpy(request->dest, [result_buffer contents], result_length);
        }
        if (request->accumulation_result_mask != NULL) {
            memcpy(request->accumulation_result_mask,
                   [accumulation_result_buffer contents],
                   accumulation_result_length);
        }
        status = SIXEL_OK;

    end:
        if (rebuild_ordered && status != SIXEL_OK) {
            g_sixel_gpu_metal_ordered_colors = 0;
        }
        if (rebuild_grid && status != SIXEL_OK) {
            g_sixel_gpu_metal_grid_colors = 0;
        }
        if (result_buffer_direct != 0 && result_buffer != nil) {
            [result_buffer release];
            result_buffer = nil;
        }
        if (locked != 0) {
            (void)pthread_mutex_unlock(&g_sixel_gpu_metal_lock);
        }
    }

    return status;
}

int
sixel_gpu_dequant_metal_is_available(void)
{
    int available;

    available = 0;
    (void)pthread_mutex_lock(&g_sixel_gpu_metal_lock);
    available = sixel_gpu_palette_metal_prepare_locked();
    (void)pthread_mutex_unlock(&g_sixel_gpu_metal_lock);

    return available;
}

static void
sixel_gpu_dequant_metal_fill_params(
    sixel_gpu_metal_dequant_params_t *params,
    sixel_gpu_dequant_request_t const *request)
{
    int bias;

    bias = request->similarity_bias;
    if (bias < 1) {
        bias = 1;
    }
    params->pixel_count = (uint32_t)request->pixel_count;
    params->width = (uint32_t)request->width;
    params->height = (uint32_t)request->height;
    params->ncolors = (uint32_t)request->ncolors;
    params->similarity_bias = (int32_t)bias;
}

SIXELSTATUS
sixel_gpu_dequant_metal_fast4_rgba(
    sixel_gpu_dequant_request_t const *request)
{
    SIXELSTATUS status;
    sixel_gpu_metal_dequant_params_t params;
    id<MTLBuffer> result_buffer;
    id<MTLBuffer> rgba_buffer;
    id<MTLBuffer> palette_buffer;
    id<MTLBuffer> params_buffer;
    id<MTLCommandBuffer> command_buffer;
    id<MTLComputeCommandEncoder> encoder;
    NSUInteger threads_per_group;
    NSUInteger groups;
    NSUInteger result_length;
    NSUInteger rgba_length;
    NSUInteger palette_length;
    int result_buffer_direct;
    int locked;

    status = SIXEL_FALSE;
    memset(&params, 0, sizeof(params));
    result_buffer = nil;
    rgba_buffer = nil;
    palette_buffer = nil;
    params_buffer = nil;
    command_buffer = nil;
    encoder = nil;
    threads_per_group = 0U;
    groups = 0U;
    result_length = 0U;
    rgba_length = 0U;
    palette_length = 0U;
    result_buffer_direct = 0;
    locked = 0;

    if (request == NULL) {
        sixel_helper_set_additional_message(
            "gpu dequant: Metal request is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    @autoreleasepool {
        (void)pthread_mutex_lock(&g_sixel_gpu_metal_lock);
        locked = 1;
        if (!sixel_gpu_palette_metal_prepare_locked()) {
            sixel_helper_set_additional_message(
                "gpu dequant: Metal is not available.");
            status = SIXEL_FEATURE_ERROR;
            goto end;
        }

        result_length = (NSUInteger)request->pixel_count * 4U;
        rgba_length = (NSUInteger)request->pixel_count * 4U;
        palette_length = (NSUInteger)request->ncolors * 3U;

        result_buffer =
            sixel_gpu_metal_wrap_dest_buffer(request->dest, result_length);
        if (result_buffer != nil) {
            result_buffer_direct = 1;
        } else {
            result_buffer = sixel_gpu_metal_ensure_buffer(
                &g_sixel_gpu_metal_dequant_result_buffer,
                result_length);
        }
        rgba_buffer = sixel_gpu_metal_upload_buffer(
            &g_sixel_gpu_metal_dequant_rgba_buffer,
            request->rgba,
            rgba_length);
        palette_buffer =
            sixel_gpu_metal_upload_palette(request->palette, palette_length);
        sixel_gpu_dequant_metal_fill_params(&params, request);
        params_buffer = sixel_gpu_metal_upload_buffer(
            &g_sixel_gpu_metal_dequant_params_buffer,
            &params,
            sizeof(params));

        if (result_buffer == nil || rgba_buffer == nil ||
                palette_buffer == nil || params_buffer == nil) {
            sixel_helper_set_additional_message(
                "gpu dequant: Metal buffer allocation failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }

        command_buffer = [g_sixel_gpu_metal_queue commandBuffer];
        encoder = [command_buffer computeCommandEncoder];
        if (command_buffer == nil || encoder == nil) {
            sixel_helper_set_additional_message(
                "gpu dequant: Metal command encoder creation failed.");
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }

        [encoder setComputePipelineState:g_sixel_gpu_metal_dequant_pipeline];
        [encoder setBuffer:result_buffer offset:0 atIndex:0];
        [encoder setBuffer:rgba_buffer offset:0 atIndex:1];
        [encoder setBuffer:palette_buffer offset:0 atIndex:2];
        [encoder setBuffer:params_buffer offset:0 atIndex:3];

        threads_per_group =
            [g_sixel_gpu_metal_dequant_pipeline
                maxTotalThreadsPerThreadgroup];
        if (threads_per_group > 256U) {
            threads_per_group = 256U;
        }
        if (threads_per_group == 0U) {
            threads_per_group = 1U;
        }
        groups = ((NSUInteger)request->pixel_count + threads_per_group - 1U) /
            threads_per_group;
        [encoder dispatchThreadgroups:MTLSizeMake(groups, 1U, 1U)
                threadsPerThreadgroup:MTLSizeMake(threads_per_group,
                                                  1U,
                                                  1U)];
        [encoder endEncoding];
        [command_buffer commit];
        [command_buffer waitUntilCompleted];
        if ([command_buffer status] != MTLCommandBufferStatusCompleted) {
            sixel_gpu_palette_metal_set_message(
                "gpu dequant: Metal command failed",
                [command_buffer error]);
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }

        if (result_buffer_direct == 0) {
            memcpy(request->dest, [result_buffer contents], result_length);
        }
        status = SIXEL_OK;

    end:
        if (result_buffer_direct != 0 && result_buffer != nil) {
            [result_buffer release];
            result_buffer = nil;
        }
        if (locked != 0) {
            (void)pthread_mutex_unlock(&g_sixel_gpu_metal_lock);
        }
    }

    return status;
}

#endif /* defined(HAVE_METAL) */

/* emacs Local Variables:      */
/* emacs mode: objc            */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
