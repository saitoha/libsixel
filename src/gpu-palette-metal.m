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

enum {
    SIXEL_GPU_METAL_MODE_NONE = 0,
    SIXEL_GPU_METAL_MODE_BLUENOISE = 1,
    SIXEL_GPU_METAL_LOOKUP_DIRECT = 0,
    SIXEL_GPU_METAL_LOOKUP_EYTZINGER = 1,
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
    uint32_t accumulation_result_mask_size;
    float bluenoise_strength;
    float bluenoise_gradient_factor;
    int32_t bluenoise_phase_x;
    int32_t bluenoise_phase_y;
    uint32_t bluenoise_channel_rgb;
    uint32_t gradient_size;
    uint32_t gradient_width;
    uint32_t gradient_height;
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
static unsigned char g_sixel_gpu_metal_palette_shadow[
    SIXEL_PALETTE_MAX * 3U];
static NSUInteger g_sixel_gpu_metal_palette_shadow_length = 0U;
static int g_sixel_gpu_metal_palette_shadow_valid = 0;
static int g_sixel_gpu_metal_probe_done = 0;
static int g_sixel_gpu_metal_available = 0;

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
"    uint accumulation_result_mask_size;\n"
"    float bluenoise_strength;\n"
"    float bluenoise_gradient_factor;\n"
"    int bluenoise_phase_x;\n"
"    int bluenoise_phase_y;\n"
"    uint bluenoise_channel_rgb;\n"
"    uint gradient_size;\n"
"    uint gradient_width;\n"
"    uint gradient_height;\n"
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
"}\n"
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
"    uint gid [[thread_position_in_grid]])\n"
"{\n"
"    uint x;\n"
"    uint y;\n"
"    uint best;\n"
"    uint best_dist;\n"
"    uchar q[3];\n"
"    uchar3 source;\n"
"    uchar3 retained;\n"
"    int dr;\n"
"    int dg;\n"
"    int db;\n"
"    if (gid >= params.pixel_count) {\n"
"        return;\n"
"    }\n"
"    if (params.has_transparent != 0u && gid < params.transparent_size &&\n"
"        transparent_mask[gid] != 0u) {\n"
"        result[gid] = uchar(params.transparent_keycolor);\n"
"        if (gid < params.accumulation_result_mask_size) {\n"
"            accumulation_result_mask[gid] = uchar(1);\n"
"        }\n"
"        return;\n"
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
"        dr = dr < 0 ? -dr : dr;\n"
"        dg = dg < 0 ? -dg : dg;\n"
"        db = db < 0 ? -db : db;\n"
"        if (uint(dr) <= params.sixdelta_threshold &&\n"
"            uint(dg) <= params.sixdelta_threshold &&\n"
"            uint(db) <= params.sixdelta_threshold) {\n"
"            result[gid] = uchar(params.accumulation_keycolor);\n"
"            if (gid < params.accumulation_result_mask_size) {\n"
"                accumulation_result_mask[gid] = uchar(1);\n"
"            }\n"
"            return;\n"
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
"    if (params.lookup_mode == 1u) {\n"
"        best = eytzinger_palette_lookup(palette,\n"
"                                        eytzinger_sorted,\n"
"                                        eytzinger_rank,\n"
"                                        eytzinger_params,\n"
"                                        uchar3(q[0], q[1], q[2]));\n"
"    } else {\n"
"        best = 0u;\n"
"        best_dist = 0xffffffffu;\n"
"        for (uint i = 0u; i < params.ncolors; ++i) {\n"
"            uint dist = palette_distance(palette,\n"
"                                         uchar3(q[0], q[1], q[2]),\n"
"                                         i);\n"
"            if (dist < best_dist) {\n"
"                best_dist = dist;\n"
"                best = i;\n"
"            }\n"
"        }\n"
"    }\n"
"    if (params.has_6delta != 0u &&\n"
"        gid < params.accumulation_pixel_count &&\n"
"        (gid >= params.accumulation_valid_mask_size ||\n"
"         accumulation_valid_mask[gid] != 0u)) {\n"
"        retained = uchar3(accumulation[gid * 3u + 0u],\n"
"                          accumulation[gid * 3u + 1u],\n"
"                          accumulation[gid * 3u + 2u]);\n"
"        if (color_distance(source, retained) <=\n"
"            palette_distance(palette, source, best)) {\n"
"            result[gid] = uchar(params.accumulation_keycolor);\n"
"            if (gid < params.accumulation_result_mask_size) {\n"
"                accumulation_result_mask[gid] = uchar(1);\n"
"            }\n"
"            return;\n"
"        }\n"
"    }\n"
"    result[gid] = uchar(best);\n"
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
        params->accumulation_result_mask_size =
            (uint32_t)request->accumulation_result_mask_size;
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
        if (result_buffer == nil || pixel_buffer == nil ||
                palette_buffer == nil || transparent_buffer == nil ||
                blue_noise_buffer == nil || gradient_buffer == nil ||
                params_buffer == nil || accumulation_buffer == nil ||
                accumulation_valid_buffer == nil ||
                accumulation_result_buffer == nil ||
                eytzinger_rank_buffer == nil ||
                eytzinger_sorted_buffer == nil ||
                eytzinger_params_buffer == nil) {
            sixel_helper_set_additional_message(
                "gpu palette apply: Metal buffer allocation failed.");
            status = SIXEL_BAD_ALLOCATION;
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

        [encoder setComputePipelineState:g_sixel_gpu_metal_pipeline];
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

        threads_per_group =
            [g_sixel_gpu_metal_pipeline maxTotalThreadsPerThreadgroup];
        if (threads_per_group > 256U) {
            threads_per_group = 256U;
        }
        if (threads_per_group == 0U) {
            threads_per_group = 1U;
        }
        groups = ((NSUInteger)request->pixel_count + threads_per_group - 1U) /
            threads_per_group;
        [encoder dispatchThreadgroups:MTLSizeMake(groups, 1U, 1U)
                threadsPerThreadgroup:MTLSizeMake(threads_per_group, 1U, 1U)];
        [encoder endEncoding];
        [command_buffer commit];
        [command_buffer waitUntilCompleted];
        if ([command_buffer status] != MTLCommandBufferStatusCompleted) {
            sixel_gpu_palette_metal_set_message(
                "gpu palette apply: Metal command failed",
                [command_buffer error]);
            status = SIXEL_RUNTIME_ERROR;
            goto end;
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
