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
#include "gpu-runtime-metal.h"
#include "loader-common.h"

enum {
    SIXEL_GPU_METAL_MODE_NONE = 0,
    SIXEL_GPU_METAL_MODE_BLUENOISE = 1,
    SIXEL_GPU_METAL_MODE_A_DITHER = 2,
    SIXEL_GPU_METAL_MODE_X_DITHER = 3,
    SIXEL_GPU_METAL_LOOKUP_DIRECT = 0,
    SIXEL_GPU_METAL_LOOKUP_EYTZINGER = 1,
    SIXEL_GPU_METAL_LOOKUP_GRIDLUT = 2,
    SIXEL_GPU_METAL_LOOKUP_DELAUNAY = 4,
    SIXEL_GPU_METAL_LOOKUP_FHEDT = 5,
    SIXEL_GPU_METAL_EYTZINGER_KEY_MAX = 255 * 3
};

typedef struct sixel_gpu_metal_params {
    uint32_t pixel_count;
    uint32_t width;
    uint32_t height;
    uint32_t ncolors;
    uint32_t lookup_mode;
    uint32_t fhedt_resolution, fhedt_shift, fhedt_refine, fhedt_dist2;
    uint32_t mode;
    uint32_t has_transparent;
    uint32_t transparent_size;
    uint32_t transparent_keycolor;
    uint32_t has_6delta;
    uint32_t accumulation_pixel_count;
    uint32_t accumulation_valid_mask_size;
    uint32_t accumulation_keycolor;
    uint32_t accumulation_stride, accumulation_origin;
    uint32_t sixdelta_threshold;
    uint32_t sixdelta_metric;
    uint32_t sixdelta_compare;
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
/* Delaunay has independent storage and identity: alternating lookup policies
 * must not reuse rows built for another palette or construction method. */
static id<MTLComputePipelineState> g_sixel_gpu_metal_delaunay_bounds = nil;
static id<MTLComputePipelineState> g_sixel_gpu_metal_delaunay_sort = nil;
static id<MTLComputePipelineState> g_sixel_gpu_metal_delaunay_seeds = nil;
static sixel_gpu_metal_cached_buffer_t g_sixel_gpu_metal_delaunay =
    { nil, 0U };
static unsigned char g_sixel_gpu_metal_delaunay_palette[SIXEL_PALETTE_MAX * 3];
static int g_sixel_gpu_metal_delaunay_colors = 0;
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
#include <generated-metal/lookup-policy-none-gpu-shader.inc.h>
#include <generated-metal/lookup-policy-blockcull-gpu-shader.inc.h>
NULL
};

static char const * const g_sixel_gpu_metal_source_chunks[] = {
#include <generated-metal/gpu-palette-legacy-types-shader.inc.h>
#include <generated-metal/gpu-palette-legacy-query-shader.inc.h>
#include <generated-metal/lookup-policy-gridlut-gpu-shader.inc.h>
#include <generated-metal/lookup-policy-delaunay-gpu-shader.inc.h>
#include <generated-metal/lookup-policy-eytzinger-gpu-shader.inc.h>
#include <generated-metal/lookup-policy-fhedt-gpu-query-shader.inc.h>
#include <generated-metal/gpu-palette-legacy-pixel-shader.inc.h>
#include <generated-metal/gpu-palette-legacy-apply-shader.inc.h>
#include <generated-metal/gpu-dequant-fast4-shader.inc.h>
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

#include "lookup-policy-fhedt-gpu.inc.h"

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

#include "lookup-policy-bitlut-gpu.inc.h"

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
sixel_gpu_metal_upload_retained(sixel_gpu_metal_cached_buffer_t *cache,
    unsigned char const *bytes, sixel_gpu_palette_request_t const *request,
    size_t depth)
{
    id<MTLBuffer> buffer;
    unsigned char *dest;
    size_t stride;
    size_t row_bytes;
    size_t length;
    int y;

    /* Validation has bounded the canvas and frame before this dispatch. Pack
     * directly into the reusable shared buffer: a small desktop update must
     * not upload the entire retained canvas or allocate a staging image. The
     * synchronous command owns these packed rows until completion. */
    stride = (size_t)(request->accumulation_width != 0
        ? request->accumulation_width : request->width) * depth;
    row_bytes = (size_t)request->width * depth;
    length = request->pixel_count * depth;
    bytes += (size_t)request->accumulation_origin_y * stride +
        (size_t)request->accumulation_origin_x * depth;
    if (stride == row_bytes) {
        return sixel_gpu_metal_upload_buffer(cache, bytes, length);
    }
    buffer = sixel_gpu_metal_ensure_buffer(cache, length);
    if (buffer == nil) {
        return nil;
    }
    dest = [buffer contents];
    for (y = 0; y < request->height; ++y) {
        memcpy(dest, bytes + (size_t)y * stride, row_bytes);
        dest += row_bytes;
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

/* Device selection is independent of every lookup program's compilation. */
static int
sixel_gpu_palette_metal_prepare_device_locked(void)
{
    static int done;
    static int available;
    unsigned char dummy;

    if (done) return available;
    done = 1;
    dummy = 0;
    g_sixel_gpu_metal_device = MTLCreateSystemDefaultDevice();
    if (g_sixel_gpu_metal_device == nil) {
        return 0;
    }
    g_sixel_gpu_metal_queue = [g_sixel_gpu_metal_device newCommandQueue];
    if (g_sixel_gpu_metal_queue == nil) {
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

    available = 1;
    return 1;
}

static int
sixel_gpu_palette_metal_prepare_locked(void)
{
    NSError *error;
    NSString *source;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    id<MTLFunction> dequant_function;

    error = nil;
    source = nil;
    library = nil;
    function = nil;
    dequant_function = nil;
    if (g_sixel_gpu_metal_probe_done != 0) {
        return g_sixel_gpu_metal_available;
    }

    g_sixel_gpu_metal_probe_done = 1;
    if (!sixel_gpu_palette_metal_prepare_device_locked()) return 0;

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
    function = [library newFunctionWithName:@"sixel_gpu_delaunay_bounds"];
    if (function != nil) {
        g_sixel_gpu_metal_delaunay_bounds = [g_sixel_gpu_metal_device
            newComputePipelineStateWithFunction:function error:&error];
        [function release];
        function = nil;
    }
    function = [library newFunctionWithName:@"sixel_gpu_delaunay_sort"];
    if (function != nil) {
        g_sixel_gpu_metal_delaunay_sort = [g_sixel_gpu_metal_device
            newComputePipelineStateWithFunction:function error:&error];
        [function release];
        function = nil;
    }
    function = [library newFunctionWithName:@"sixel_gpu_delaunay_seeds"];
    if (function != nil) {
        g_sixel_gpu_metal_delaunay_seeds = [g_sixel_gpu_metal_device
            newComputePipelineStateWithFunction:function error:&error];
        [function release];
        function = nil;
    }
    [library release];
    library = nil;
    if (g_sixel_gpu_metal_dequant_pipeline == nil) {
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
    params->accumulation_stride = (uint32_t)(request->retained_rgb != NULL
        ? request->accumulation_width : request->width);
    params->accumulation_origin = request->retained_rgb != NULL
        ? (uint32_t)((size_t)request->accumulation_origin_y *
            request->accumulation_width + request->accumulation_origin_x) : 0;
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
            (uint32_t)request->pixel_count;
        params->accumulation_valid_mask_size =
            request->accumulation_valid_mask != NULL
                ? (uint32_t)request->pixel_count : 0U;
        params->accumulation_keycolor =
            (uint32_t)request->accumulation_keycolor;
        params->sixdelta_threshold =
            (uint32_t)request->sixdelta_threshold;
        params->sixdelta_metric = (uint32_t)request->sixdelta_metric;
        params->sixdelta_compare = (uint32_t)request->sixdelta_compare;
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

#include "gpu-palette-fused-prefix.inc.h"
#include "gpu-palette-fused-suffix.inc.h"
#include "gpu-palette-program.inc.h"

/* Commands own their encoded resources. Keep a bounded window alive until
 * completion, including after producer cancellation or consumer failure.
 * Per-slot validation flags prevent a later failed range from poisoning an
 * earlier completed range. Slots occupy separate CPU cache lines. */
#define SIXEL_GPU_RANGE_WINDOW 3
#define SIXEL_GPU_RANGE_FLAG_STRIDE 256U

typedef struct sixel_gpu_metal_dispatch {
    id<MTLComputePipelineState> pipeline;
    id<MTLBuffer> buffers[18];
    int generic;
    int gemm;
    int blockcull;
    int result_direct;
} sixel_gpu_metal_dispatch_t;

typedef struct sixel_gpu_metal_range {
    id<MTLCommandBuffer> command;
    int first;
    int rows;
    int completed;
    unsigned int sequence;
} sixel_gpu_metal_range_t;

static SIXELSTATUS
sixel_gpu_metal_apply_ranges(sixel_gpu_palette_request_t const *request,
    sixel_gpu_metal_dispatch_t const *dispatch,
    id<MTLCommandBuffer> initial_command,
    id<MTLComputeCommandEncoder> initial_encoder)
{
    sixel_gpu_metal_range_t ranges[SIXEL_GPU_RANGE_WINDOW];
    sixel_gpu_metal_range_t *range;
    sixel_gpu_palette_test_hooks_t *hooks;
    id<MTLCommandBuffer> command;
    id<MTLComputeCommandEncoder> encoder;
    NSUInteger threads;
    NSUInteger count;
    NSUInteger groups;
    NSUInteger flag_offset;
    uint32_t limits[2];
    uint32_t *invalid;
    size_t offset;
    size_t length;
    int streamed;
    int chunk;
    int next;
    int head;
    int pending;
    int slot;
    int i;
    unsigned int submitted;
    SIXELSTATUS status;

    memset(ranges, 0, sizeof(ranges));
    hooks = request->test_hooks;
    streamed = dispatch->generic && request->rows_ready != NULL &&
        request->rows_per_chunk > 0 &&
        request->rows_per_chunk < request->height;
    chunk = streamed ? request->rows_per_chunk : request->height;
    next = head = pending = 0;
    submitted = 0;
    encoder = initial_encoder;
    command = initial_command;
    status = SIXEL_OK;
    invalid = dispatch->generic ? [dispatch->buffers[17] contents] : NULL;
    threads = [dispatch->pipeline maxTotalThreadsPerThreadgroup];
    if (threads > 256U) threads = 256U;
    if (dispatch->gemm) threads -= threads % 32U;
    if (threads == 0U) threads = 1U;

    while (next < request->height || pending != 0) {
        while (next < request->height && pending < SIXEL_GPU_RANGE_WINDOW) {
            if (request->cancel != NULL &&
                    request->cancel(request->cancel_context)) {
                status = SIXEL_INTERRUPTED;
                goto end;
            }
            slot = (head + pending) % SIXEL_GPU_RANGE_WINDOW;
            range = &ranges[slot];
            range->first = next;
            range->rows = request->height - next;
            if (range->rows > chunk) range->rows = chunk;
            range->completed = 0;
            range->sequence = submitted;
            if (submitted != 0) {
                command = [g_sixel_gpu_metal_queue commandBuffer];
                encoder = [command computeCommandEncoder];
            }
            if (command == nil || encoder == nil ||
                    (hooks != NULL && hooks->fault == 2 &&
                     hooks->fault_after_chunks == submitted)) {
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            [encoder setComputePipelineState:dispatch->pipeline];
            for (i = 0; i < (dispatch->generic ? 18 : 16); ++i) {
                flag_offset = i == 17
                    ? (NSUInteger)slot * SIXEL_GPU_RANGE_FLAG_STRIDE : 0U;
                [encoder setBuffer:dispatch->buffers[i]
                    offset:flag_offset atIndex:(NSUInteger)i];
            }
            limits[0] = (uint32_t)((size_t)next * request->width);
            limits[1] = (uint32_t)((size_t)range->rows * request->width);
            count = limits[1];
            if (dispatch->generic) {
                /* The previous use of this slot has completed and released
                 * its command before the CPU clears the validation flag. */
                invalid[slot * (SIXEL_GPU_RANGE_FLAG_STRIDE / 4U)] = 0;
                [encoder setBytes:limits length:sizeof(limits) atIndex:18];
            }
            if (dispatch->blockcull) {
                count = (NSUInteger)((request->width + 7) / 8) *
                    (NSUInteger)((request->height + 7) / 8) * 32U;
            }
            groups = (count + threads - 1U) / threads;
            [encoder dispatchThreadgroups:MTLSizeMake(groups, 1U, 1U)
                threadsPerThreadgroup:MTLSizeMake(threads, 1U, 1U)];
            [encoder endEncoding];
            encoder = nil;
            range->command = [command retain];
            [command commit];
            ++submitted;
            ++pending;
            next += range->rows;
            if (hooks != NULL) {
                hooks->submitted_chunks = submitted;
                if ((unsigned int)pending > hooks->peak_inflight) {
                    hooks->peak_inflight = (unsigned int)pending;
                }
            }
        }
        range = &ranges[head];
        [range->command waitUntilCompleted];
        range->completed = 1;
        if (hooks != NULL) ++hooks->completed_chunks;
        if ([range->command status] != MTLCommandBufferStatusCompleted ||
                (hooks != NULL && hooks->fault == 3 &&
                 hooks->fault_after_chunks == range->sequence)) {
            sixel_gpu_palette_metal_set_message(
                "gpu palette apply: Metal command failed",
                [range->command error]);
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }
        if (invalid != NULL &&
                invalid[head * (SIXEL_GPU_RANGE_FLAG_STRIDE / 4U)] != 0) {
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        if (request->cancel != NULL &&
                request->cancel(request->cancel_context)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }
        offset = (size_t)range->first * request->width;
        length = (size_t)range->rows * request->width;
        if (!dispatch->result_direct) {
            memcpy(request->dest + offset,
                (unsigned char *)[dispatch->buffers[0] contents] + offset,
                length);
        }
        if (request->accumulation_result_mask != NULL) {
            memcpy(request->accumulation_result_mask + offset,
                (unsigned char *)[dispatch->buffers[9] contents] + offset,
                length);
        }
        if (streamed) {
            /* The callback runs on the caller, never a Metal completion
             * thread. It may block on CPU band backpressure; at most two
             * later commands continue while this completed prefix is used. */
            status = request->rows_ready(request->rows_context,
                range->first + range->rows);
            if (status != SIXEL_OK) goto end;
            if (hooks != NULL) {
                hooks->notified_rows =
                    (unsigned int)(range->first + range->rows);
            }
        }
        [range->command release];
        range->command = nil;
        head = (head + 1) % SIXEL_GPU_RANGE_WINDOW;
        --pending;
    }
end:
    /* Never return a borrowed buffer, finish a lookup lease, or recycle the
     * shared backend scratch while any submitted command can still write. */
    if (encoder != nil) [encoder endEncoding];
    for (i = 0; i < SIXEL_GPU_RANGE_WINDOW; ++i) {
        if (ranges[i].command == nil) continue;
        [ranges[i].command waitUntilCompleted];
        if (!ranges[i].completed && hooks != NULL) {
            ++hooks->completed_chunks;
        }
        [ranges[i].command release];
    }
    return status;
}

SIXELSTATUS
sixel_gpu_palette_metal_apply(sixel_gpu_palette_request_t const *input)
{
    SIXELSTATUS status;
    sixel_gpu_palette_request_t local;
    sixel_gpu_palette_request_t const *request;
    sixel_gpu_program_t *program;
    sixel_gpu_lookup_binding_interface_t *binding;
    sixel_gpu_lookup_binding_view_t binding_view;
    sixel_gpu_lookup_program_view_t const *program_view;
    sixel_gpu_lookup_build_pass_t const *pass;
    sixel_lookup_policy_configuration_t const *legacy_configuration;
    gpu_native_buffer_t *workspace;
    id<MTLBuffer> configuration_buffer;
    id<MTLBuffer> invalid_buffer;
    size_t pass_index;
    uint32_t zero;
    int generic;

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
    id<MTLBuffer> lookup_buffer;
    int use_delaunay;
    int rebuild_delaunay;
    int use_bitlut;
    int rebuild_bitlut;
    int bitlut_bits;
    size_t bitlut_size;
    int use_fhedt;
    int rebuild_fhedt;
    NSUInteger fhedt_grid_size;
    NSUInteger build_stage;
    id<MTLComputePipelineState> builders[3];
    NSUInteger sort_width;
    int result_buffer_direct;
    int locked;
    sixel_gpu_metal_dispatch_t dispatch;

    if (input == NULL) return SIXEL_BAD_ARGUMENT;
    local = *input;
    request = &local;
    program = request->program;
    binding = NULL;
    memset(&binding_view, 0, sizeof(binding_view));
    program_view = program == NULL ? NULL : &program->view;
    generic = program != NULL && !program->legacy;
    configuration_buffer = nil;
    invalid_buffer = nil;
    zero = 0;
    if (program != NULL) {
        request = &local;
        local.palette = program_view->palette;
        local.palette_size = program_view->palette_size;
        local.ncolors = (int)program_view->ncolors;
        local.lut_policy = -1;
        if (program->legacy) {
            legacy_configuration = program_view->constants;
            local.lut_policy = legacy_configuration->kind;
            sixel_gpu_palette_set_lookup_configuration(&local,
                legacy_configuration);
        }
    }
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
    use_delaunay = request != NULL &&
        request->lut_policy == SIXEL_LUT_POLICY_DELAUNAY;
    rebuild_delaunay = 0;
    use_bitlut = request != NULL &&
        (request->lut_policy == SIXEL_LUT_POLICY_5BIT ||
         request->lut_policy == SIXEL_LUT_POLICY_6BIT);
    rebuild_bitlut = 0;
    bitlut_bits = request != NULL &&
        request->lut_policy == SIXEL_LUT_POLICY_5BIT ? 5 : 6;
    bitlut_size = (size_t)1U << (bitlut_bits * 3);
    use_fhedt = request != NULL &&
        request->lut_policy == SIXEL_LUT_POLICY_FHEDT;
    rebuild_fhedt = 0;
    fhedt_grid_size = 0U;
    lookup_buffer = nil;
    sort_width = 0U;
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
        if (!(generic ? sixel_gpu_palette_metal_prepare_device_locked()
                : sixel_gpu_palette_metal_prepare_locked())) {
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
            request->test_hooks->submitted_chunks = 0;
            request->test_hooks->completed_chunks = 0;
            request->test_hooks->peak_inflight = 0;
            request->test_hooks->notified_rows = 0;
            request->test_hooks->retained_rgb_upload_bytes = 0U;
            request->test_hooks->retained_valid_upload_bytes = 0U;
        }

        if (program != NULL) {
            status = program->plan->vtbl->begin(program->plan, &binding);
            if (status != SIXEL_OK) goto end;
            status = binding->vtbl->get_view(binding, &binding_view);
            if (status != SIXEL_OK) goto end;
        }
        if (generic) {
            if (binding_view.workspace == NULL ||
                    binding_view.workspace->vtbl != &gpu_buffer_vtbl ||
                    binding_view.workspace->vtbl->size(binding_view.workspace)
                        < program_view->workspace_size) {
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            configuration_buffer = [g_sixel_gpu_metal_device
                newBufferWithBytes:program_view->constants_size == 0
                    ? &zero : program_view->constants
                length:program_view->constants_size == 0
                    ? sizeof(zero) : program_view->constants_size
                options:MTLResourceStorageModeShared];
            invalid_buffer = [g_sixel_gpu_metal_device newBufferWithLength:
                SIXEL_GPU_RANGE_WINDOW * SIXEL_GPU_RANGE_FLAG_STRIDE
                options:MTLResourceStorageModeShared];
            if (configuration_buffer == nil || invalid_buffer == nil) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
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
            (NSUInteger)request->pixel_count * 3U : 0U;
        accumulation_valid_length =
            request->has_6delta_accumulation != 0 &&
            request->accumulation_valid_mask != NULL ?
            (NSUInteger)request->pixel_count : 0U;
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
        lookup_buffer = g_sixel_gpu_metal_dummy_buffer;
        if (use_delaunay) {
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
                goto end;
            }
            params.lookup_mode = SIXEL_GPU_METAL_LOOKUP_DELAUNAY;
            lookup_buffer = sixel_gpu_metal_ensure_buffer(
                &g_sixel_gpu_metal_delaunay, 137472U * sizeof(uint32_t));
            rebuild_delaunay =
                g_sixel_gpu_metal_delaunay_colors != request->ncolors ||
                memcmp(g_sixel_gpu_metal_delaunay_palette, request->palette,
                       palette_length) != 0;
        }
        if (use_bitlut) {
            if (!sixel_gpu_bitlut_prepare()) {
                status = SIXEL_FEATURE_ERROR;
                goto end;
            }
            params.lookup_mode = (uint32_t)bitlut_bits + 1U;
            lookup_buffer = sixel_gpu_metal_ensure_buffer(
                &g_sixel_gpu_bitlut, bitlut_size);
            rebuild_bitlut = g_sixel_gpu_bitlut_colors != request->ncolors ||
                g_sixel_gpu_bitlut_bits != bitlut_bits ||
                memcmp(g_sixel_gpu_bitlut_palette, request->palette,
                    palette_length) != 0;
            if (request->test_hooks != NULL) {
                request->test_hooks->bitlut_table_bytes = bitlut_size;
            }
        }
        if (use_fhedt) {
            if (!sixel_gpu_fhedt_prepare(request->fhedt_resolution,
                    sixel_gpu_fhedt_needs_refine(request))) {
                sixel_helper_set_additional_message(
                    "gpu palette apply: FHEDT pipelines unavailable.");
                status = SIXEL_FEATURE_ERROR;
                goto end;
            }
            params.lookup_mode = SIXEL_GPU_METAL_LOOKUP_FHEDT;
            params.fhedt_resolution = request->fhedt_resolution;
            params.fhedt_shift = request->fhedt_resolution == 64 ? 2U :
                request->fhedt_resolution == 128 ? 1U : 0U;
            params.fhedt_refine = sixel_gpu_fhedt_needs_refine(request);
            params.fhedt_dist2 = request->fhedt_use_dist2;
            if (request->test_hooks != NULL) {
                request->test_hooks->fhedt_table_entry_bytes =
                    params.fhedt_refine ? 2U : 1U;
            }
            fhedt_grid_size = (NSUInteger)request->fhedt_resolution;
            fhedt_grid_size *= fhedt_grid_size * fhedt_grid_size;
            rebuild_fhedt = g_sixel_gpu_fhedt_colors != request->ncolors ||
                g_sixel_gpu_fhedt_resolution != request->fhedt_resolution ||
                g_sixel_gpu_fhedt_refine != request->fhedt_refine ||
                g_sixel_gpu_fhedt_dist2 != request->fhedt_use_dist2 ||
                memcmp(g_sixel_gpu_fhedt_palette, request->palette,
                    palette_length) != 0;
            lookup_buffer = sixel_gpu_fhedt_buffer(&g_sixel_gpu_fhedt_table,
                fhedt_grid_size * (params.fhedt_refine ? 2U : 1U));
            if (rebuild_fhedt &&
                    (sixel_gpu_fhedt_buffer(&g_sixel_gpu_fhedt_first,
                        fhedt_grid_size * 4U) == nil ||
                     sixel_gpu_fhedt_buffer(&g_sixel_gpu_fhedt_second,
                        fhedt_grid_size * 4U) == nil)) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
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
        if (request->has_6delta_accumulation != 0 &&
                request->retained_rgb != NULL) {
            accumulation_buffer = sixel_gpu_metal_buffer(
                request->retained_device, request->retained_rgb);
            accumulation_valid_buffer = sixel_gpu_metal_buffer(
                request->retained_device, request->retained_valid);
            if (accumulation_buffer == nil ||
                    accumulation_valid_buffer == nil ||
                    [accumulation_buffer length] <
                        request->accumulation_pixels_size ||
                    [accumulation_valid_buffer length] <
                        request->accumulation_valid_mask_size) {
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            /* Leases pin these allocations until every range command drains.
             * Canvas stride/origin address the rectangle without packing. */
            accumulation_length = accumulation_valid_length = 0;
        } else if (request->has_6delta_accumulation != 0) {
            accumulation_buffer = sixel_gpu_metal_upload_retained(
                &g_sixel_gpu_metal_accumulation_buffer,
                request->accumulation_pixels, request, 3U);
        } else {
            accumulation_buffer = g_sixel_gpu_metal_dummy_buffer;
        }
        if (accumulation_valid_length != 0U) {
            accumulation_valid_buffer = sixel_gpu_metal_upload_retained(
                &g_sixel_gpu_metal_accumulation_valid_buffer,
                request->accumulation_valid_mask, request, 1U);
        } else if (accumulation_valid_buffer == nil) {
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
            request->test_hooks->rebuild =
                (generic && binding_view.needs_build) ||
                rebuild_grid || rebuild_delaunay ||
                rebuild_fhedt || rebuild_bitlut;
            request->test_hooks->retained_rgb_upload_bytes =
                accumulation_buffer != nil ? accumulation_length : 0U;
            request->test_hooks->retained_valid_upload_bytes =
                accumulation_valid_buffer != nil
                    ? accumulation_valid_length : 0U;
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
                lookup_buffer == nil) {
            sixel_helper_set_additional_message(
                "gpu palette apply: Metal buffer allocation failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }

        /* Direct lookup runs as a matrix product where the device supports
         * simdgroup matrices; both kernels return the same indexes. */
        pipeline = g_sixel_gpu_metal_pipeline;
        if (!generic && !use_bitlut && !use_grid && !use_delaunay &&
                !use_fhedt &&
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
        if (generic) {
            pipeline = [program->compiled->pipelines lastObject];
            workspace = (gpu_native_buffer_t *)binding_view.workspace;
            lookup_buffer = workspace->native;
        }
        if (request->test_hooks != NULL) {
            request->test_hooks->kernel = generic ? 9 : use_bitlut ? 8 :
                use_fhedt ? 7 : use_delaunay ? 6 :
                use_grid ? 4 :
                use_blockcull ? 3 : use_gemm ? 2 : 1;
        }
        if (sixel_trace_topic_is_enabled("gpu_contract")) {
            fprintf(stderr, "LSXGPU2|consumer=palette-apply|kernel=%s\n",
                    generic ? program_view->logical_name :
                    use_bitlut ? "bitlut" : use_fhedt ? "fhedt" :
                    use_delaunay ? "delaunay" :
                    use_grid ? "gridlut" :
                    use_blockcull ? "blockcull" : use_gemm ? "gemm" :
                    request->lut_policy == SIXEL_LUT_POLICY_EYTZINGER ?
                    "eytzinger" : "direct");
        }

        if (use_bitlut && sixel_trace_topic_is_enabled("gpu_contract")) {
            fprintf(stderr, "LSXGPU3|lookup=%dbit|table=%s|shared=1\n",
                bitlut_bits, rebuild_bitlut ? "rebuild" : "reuse");
        }
        if (use_fhedt && sixel_trace_topic_is_enabled("gpu_contract")) {
            fprintf(stderr, "LSXGPU3|lookup=fhedt|table=%s|R=%d|F=%d|D=%d\n",
                rebuild_fhedt ? "rebuild" : "reuse",
                request->fhedt_resolution, request->fhedt_refine,
                request->fhedt_use_dist2);
        }
        if (use_delaunay && sixel_trace_topic_is_enabled("gpu_contract")) {
            fprintf(stderr, "LSXGPU3|lookup=delaunay|table=%s\n",
                    rebuild_delaunay ? "rebuild" : "reuse");
        }

        if (generic && sixel_trace_topic_is_enabled("gpu_contract")) {
            fprintf(stderr, "LSXGPU3|lookup=%s|table=%s\n",
                program_view->logical_name,
                binding_view.needs_build ? "rebuild" : "reuse");
        }

        command_buffer = [g_sixel_gpu_metal_queue commandBuffer];
        encoder = [command_buffer computeCommandEncoder];
        if (command_buffer == nil || encoder == nil) {
            sixel_helper_set_additional_message(
                "gpu palette apply: Metal command encoder creation failed.");
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }

        if (generic && binding_view.needs_build) {
            if (request->test_hooks != NULL) request->test_hooks->rebuild = 1;
            for (pass_index = 0; pass_index < program_view->build_pass_count;
                    ++pass_index) {
                pass = &program_view->build_passes[pass_index];
                [encoder setComputePipelineState:
                    program->compiled->pipelines[pass_index]];
                [encoder setBuffer:palette_buffer offset:0 atIndex:0];
                [encoder setBytes:&program_view->ncolors
                    length:sizeof(uint32_t) atIndex:1];
                [encoder setBuffer:configuration_buffer offset:0 atIndex:2];
                [encoder setBuffer:lookup_buffer offset:0 atIndex:3];
                [encoder dispatchThreadgroups:
                    MTLSizeMake(pass->groups[0], pass->groups[1],
                        pass->groups[2]) threadsPerThreadgroup:
                    MTLSizeMake(pass->threads[0], pass->threads[1],
                        pass->threads[2])];
                /* Encoder boundaries order builder writes before consumers. */
                [encoder endEncoding];
                encoder = [command_buffer computeCommandEncoder];
                if (encoder == nil) {
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
            }
        }
        if (rebuild_bitlut) {
            /* Build and apply share a command buffer. The encoder boundary
             * makes the immutable table visible without a CPU round trip. */
            [encoder endEncoding];
            encoder = nil;
            status = sixel_gpu_bitlut_encode(command_buffer, palette_buffer,
                lookup_buffer, (unsigned int)bitlut_bits, request->ncolors);
            if (SIXEL_FAILED(status)) goto end;
            encoder = [command_buffer computeCommandEncoder];
            if (encoder == nil) {
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
        }
        if (rebuild_fhedt) {
            [encoder endEncoding];
            encoder = nil;
            status = sixel_gpu_fhedt_build(command_buffer, palette_buffer,
                lookup_buffer, request);
            if (SIXEL_FAILED(status)) goto end;
            encoder = [command_buffer computeCommandEncoder];
            if (encoder == nil) {
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
        }
        if (rebuild_delaunay) {
            /* Bounds, compact sorted adjacency and seeds stay on the GPU.
             * Encoder boundaries establish tracked-buffer visibility; apply
             * follows in this command buffer with no intermediate wait. */
            for (build_stage = 0U; build_stage < 3U; ++build_stage) {
                [encoder setComputePipelineState:builders[build_stage]];
                [encoder setBuffer:palette_buffer offset:0 atIndex:2];
                [encoder setBuffer:params_buffer offset:0 atIndex:6];
                [encoder setBuffer:lookup_buffer offset:0 atIndex:15];
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
                encoder = [command_buffer computeCommandEncoder];
                if (encoder == nil) {
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
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
        memset(&dispatch, 0, sizeof(dispatch));
        dispatch.pipeline = pipeline;
        dispatch.generic = generic;
        dispatch.gemm = use_gemm;
        dispatch.blockcull = use_blockcull;
        dispatch.result_direct = result_buffer_direct;
        dispatch.buffers[0] = result_buffer;
        dispatch.buffers[1] = pixel_buffer;
        dispatch.buffers[2] = palette_buffer;
        dispatch.buffers[3] = transparent_buffer;
        dispatch.buffers[4] = blue_noise_buffer;
        dispatch.buffers[5] = gradient_buffer;
        dispatch.buffers[6] = params_buffer;
        dispatch.buffers[7] = accumulation_buffer;
        dispatch.buffers[8] = accumulation_valid_buffer;
        dispatch.buffers[9] = accumulation_result_buffer;
        dispatch.buffers[10] = eytzinger_rank_buffer;
        dispatch.buffers[11] = eytzinger_sorted_buffer;
        dispatch.buffers[12] = eytzinger_params_buffer;
        dispatch.buffers[13] = gemm_tiles_buffer;
        dispatch.buffers[14] = grid_buffer;
        dispatch.buffers[15] = lookup_buffer;
        dispatch.buffers[16] = configuration_buffer;
        dispatch.buffers[17] = invalid_buffer;
        status = sixel_gpu_metal_apply_ranges(request, &dispatch,
            command_buffer, encoder);
        encoder = nil;
        if (status != SIXEL_OK) goto end;

        if (rebuild_bitlut) {
            memcpy(g_sixel_gpu_bitlut_palette, request->palette,
                palette_length);
            g_sixel_gpu_bitlut_colors = request->ncolors;
            g_sixel_gpu_bitlut_bits = bitlut_bits;
        }
        if (rebuild_fhedt) {
            memcpy(g_sixel_gpu_fhedt_palette, request->palette,
                palette_length);
            g_sixel_gpu_fhedt_colors = request->ncolors;
            g_sixel_gpu_fhedt_resolution = request->fhedt_resolution;
            g_sixel_gpu_fhedt_refine = request->fhedt_refine;
            g_sixel_gpu_fhedt_dist2 = request->fhedt_use_dist2;
        }
        if (rebuild_delaunay) {
            memcpy(g_sixel_gpu_metal_delaunay_palette, request->palette,
                   palette_length);
            g_sixel_gpu_metal_delaunay_colors = request->ncolors;
        }
        if (rebuild_grid) {
            memcpy(g_sixel_gpu_metal_grid_palette, request->palette,
                   palette_length);
            g_sixel_gpu_metal_grid_colors = request->ncolors;
        }
        if (request->test_hooks != NULL) {
            request->test_hooks->published = 1;
        }
        status = SIXEL_OK;

    end:
        if (encoder != nil) [encoder endEncoding];
        if (binding != NULL) {
            (void)binding->vtbl->finish(binding, status);
            binding->vtbl->unref(binding);
        }
        [configuration_buffer release];
        [invalid_buffer release];
        if (rebuild_bitlut && status != SIXEL_OK) {
            g_sixel_gpu_bitlut_colors = 0;
        }
        if (rebuild_fhedt && status != SIXEL_OK) {
            g_sixel_gpu_fhedt_colors = 0;
        }
        if (rebuild_delaunay && status != SIXEL_OK) {
            g_sixel_gpu_metal_delaunay_colors = 0;
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
