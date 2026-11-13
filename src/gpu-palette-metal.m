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
#include "gpu-statistics.h"

#include <sixel.h>

#include "bluenoise_64x64.h"
#include "gpu-dequant.h"
#include "gpu-palette.h"
#include "gpu-runtime-metal.h"
#include "gpu-lookup-metal.h"
#include "loader-common.h"

typedef struct sixel_gpu_metal_params {
    uint32_t pixel_count;
    uint32_t width;
    uint32_t height;
    uint32_t ncolors;
    uint32_t lookup_parameters[4];
    uint32_t reserved;
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

typedef struct sixel_gpu_metal_cached_buffer {
    id<MTLBuffer> buffer;
    NSUInteger capacity;
} sixel_gpu_metal_cached_buffer_t;

/* A lookup supplies its code and native execution operations. The runtime
 * only compiles, binds, submits and drains commands under its device lock. */
typedef struct gpu_metal_lookup_run {
    id<MTLComputePipelineState> pipeline;
    id<MTLBuffer> buffers[6];
    sixel_gpu_dither_program_t const *dither;
    char const *kernel;
    int kernel_id, rebuild;
    NSUInteger thread_multiple, fixed_threads;
    NSUInteger block_width, block_height, threads_per_block;
} gpu_metal_lookup_run_t;

struct gpu_metal_lookup {
    char const *const *chunks;
    char const *entry;
    SIXELSTATUS (*prepare)(gpu_metal_lookup_run_t *,
        sixel_gpu_palette_request_t const *, sixel_gpu_metal_params_t *);
    SIXELSTATUS (*build)(gpu_metal_lookup_run_t *, id<MTLCommandBuffer>,
        id<MTLBuffer>, id<MTLBuffer>, sixel_gpu_palette_request_t const *);
    void (*finish)(gpu_metal_lookup_run_t *,
        sixel_gpu_palette_request_t const *, SIXELSTATUS);
};

typedef struct gpu_metal_lookup_pipeline {
    char const *const *chunks;
    sixel_gpu_dither_program_t const *dither;
    char const *entry;
    id<MTLComputePipelineState> pipeline;
    struct gpu_metal_lookup_pipeline *next;
} gpu_metal_lookup_pipeline_t;
static gpu_metal_lookup_pipeline_t *gpu_lookup_pipelines;

static pthread_mutex_t g_sixel_gpu_metal_lock = PTHREAD_MUTEX_INITIALIZER;
static id<MTLDevice> g_sixel_gpu_metal_device = nil;
static id<MTLCommandQueue> g_sixel_gpu_metal_queue = nil;
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
static unsigned char g_sixel_gpu_metal_palette_shadow[
    SIXEL_PALETTE_MAX * 3U];
static NSUInteger g_sixel_gpu_metal_palette_shadow_length = 0U;
static int g_sixel_gpu_metal_palette_shadow_valid = 0;

static char const * const g_sixel_gpu_metal_source_chunks[] = {
#include <generated-metal/gpu-palette-legacy-types-shader.inc.h>
#include <generated-metal/gpu-palette-legacy-query-shader.inc.h>
NULL
};
static char const *const gpu_legacy_suffix[] = {
#include <generated-metal/gpu-palette-legacy-pixel-shader.inc.h>
NULL
};
static char const *const gpu_dequant_source[] = {
#include <generated-metal/gpu-palette-legacy-types-shader.inc.h>
#include <generated-metal/gpu-dequant-fast4-shader.inc.h>
NULL
};

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

static id<MTLComputePipelineState>
gpu_metal_lookup_pipeline(char const *const *chunks,
    sixel_gpu_dither_program_t const *dither, char const *entry)
{
    gpu_metal_lookup_pipeline_t *cached;
    NSMutableString *source;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    NSError *error;
    size_t i;

    if (dither == NULL || dither->legacy_metal_count == 0) return nil;
    for (cached = gpu_lookup_pipelines; cached != NULL; cached = cached->next)
        if (cached->chunks == chunks && cached->dither == dither &&
                strcmp(cached->entry, entry) == 0) return cached->pipeline;
    if (!sixel_gpu_palette_metal_prepare_device_locked()) return nil;
    cached = calloc(1, sizeof(*cached));
    if (cached == NULL) return nil;
    cached->chunks = chunks;
    cached->dither = dither;
    cached->entry = entry;
    cached->next = gpu_lookup_pipelines;
    gpu_lookup_pipelines = cached;
    source = [[NSMutableString alloc] init];
    if (source == nil) return nil;
    if (!sixel_gpu_palette_metal_append_chunks(source,
            g_sixel_gpu_metal_source_chunks)) {
        [source release];
        return nil;
    }
    for (i = 0; i < dither->legacy_metal_count; ++i)
        [source appendString:@(dither->legacy_metal[i])];
    if (!sixel_gpu_palette_metal_append_chunks(source, gpu_legacy_suffix) ||
            !sixel_gpu_palette_metal_append_chunks(source, chunks)) {
        [source release];
        return nil;
    }
    error = nil;
    library = [g_sixel_gpu_metal_device newLibraryWithSource:source
        options:nil error:&error];
    [source release];
    if (library == nil) {
        sixel_gpu_palette_metal_set_message(
            "gpu lookup: Metal compilation failed", error);
        return nil;
    }
    function = [library newFunctionWithName:@(entry)];
    if (function != nil) {
        cached->pipeline = [g_sixel_gpu_metal_device
            newComputePipelineStateWithFunction:function error:&error];
        [function release];
    }
    [library release];
    return cached->pipeline;
}

static int
sixel_gpu_dequant_metal_prepare_locked(void)
{
    NSMutableString *source;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    NSError *error;

    if (g_sixel_gpu_metal_dequant_pipeline != nil) return 1;
    if (!sixel_gpu_palette_metal_prepare_device_locked()) return 0;
    error = nil;
    source = [[NSMutableString alloc] init];
    if (source == nil) return 0;
    if (!sixel_gpu_palette_metal_append_chunks(source, gpu_dequant_source)) {
        [source release];
        return 0;
    }
    library = [g_sixel_gpu_metal_device newLibraryWithSource:source
        options:nil error:&error];
    [source release];
    if (library == nil) return 0;
    function = [library newFunctionWithName:@"sixel_gpu_dequant_fast4_rgba"];
    if (function != nil) {
        g_sixel_gpu_metal_dequant_pipeline = [g_sixel_gpu_metal_device
            newComputePipelineStateWithFunction:function error:&error];
        [function release];
    }
    [library release];
    return g_sixel_gpu_metal_dequant_pipeline != nil;
}

int
sixel_gpu_palette_metal_is_available(void)
{
    int available;

    available = 0;
    (void)pthread_mutex_lock(&g_sixel_gpu_metal_lock);
    available = sixel_gpu_palette_metal_prepare_device_locked();
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

#include "lookup-policy-none-gpu-metal.inc.h"
#include "lookup-policy-5bit-gpu-metal.inc.h"
#include "lookup-policy-6bit-gpu-metal.inc.h"
#include "lookup-policy-fhedt-gpu-metal.inc.h"
#include "lookup-policy-eytzinger-gpu-metal.inc.h"
#include "lookup-policy-gridlut-gpu-metal.inc.h"
#include "lookup-policy-delaunay-gpu-metal.inc.h"
#include "lookup-policy-blockcull-gpu-metal.inc.h"

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
    NSUInteger thread_multiple, fixed_threads;
    NSUInteger block_width, block_height, threads_per_block;
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
    if (dispatch->thread_multiple != 0)
        threads -= threads % dispatch->thread_multiple;
    if (dispatch->fixed_threads != 0) threads = dispatch->fixed_threads;
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
            if (dispatch->threads_per_block != 0) {
                count = ((request->width + dispatch->block_width - 1U) /
                    dispatch->block_width) *
                    ((request->height + dispatch->block_height - 1U) /
                    dispatch->block_height) * dispatch->threads_per_block;
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
        sixel_gpu_statistics_record_metal(range->command);
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
        if (!ranges[i].completed) {
            sixel_gpu_statistics_record_metal(ranges[i].command);
        }
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
    gpu_metal_lookup_program_t const *native;
    gpu_metal_lookup_t const *lookup;
    gpu_metal_lookup_run_t lookup_run;
    unsigned int slot;
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
    id<MTLCommandBuffer> command_buffer;
    id<MTLComputeCommandEncoder> encoder;
    NSUInteger result_length;
    NSUInteger pixel_length;
    NSUInteger palette_length;
    NSUInteger transparent_length;
    NSUInteger gradient_length;
    NSUInteger accumulation_length;
    NSUInteger accumulation_valid_length;
    NSUInteger accumulation_result_length;
    id<MTLComputePipelineState> pipeline;
    id<MTLBuffer> lookup_buffer;
    int result_buffer_direct;
    int locked;
    sixel_gpu_metal_dispatch_t dispatch;

    if (input == NULL) return SIXEL_BAD_ARGUMENT;
    if (input->program == NULL) return SIXEL_BAD_ARGUMENT;
    local = *input;
    request = &local;
    program = request->program;
    if (program != NULL && request->dither_program != program->dither)
        return SIXEL_BAD_ARGUMENT;
    binding = NULL;
    memset(&binding_view, 0, sizeof(binding_view));
    program_view = program == NULL ? NULL : &program->view;
    generic = !program->native_lookup;
    lookup = NULL;
    memset(&lookup_run, 0, sizeof(lookup_run));
    lookup_run.dither = request->dither_program;
    configuration_buffer = nil;
    invalid_buffer = nil;
    zero = 0;
    if (program != NULL) {
        request = &local;
        local.palette = program_view->palette;
        local.palette_size = program_view->palette_size;
        local.ncolors = (int)program_view->ncolors;
        local.lut_policy = -1;
        if (program->native_lookup) {
            native = program_view->constants;
            lookup = native->implementation;
            sixel_gpu_palette_set_lookup_configuration(&local,
                native->configuration);
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
    command_buffer = nil;
    encoder = nil;
    result_length = 0U;
    pixel_length = 0U;
    palette_length = 0U;
    transparent_length = 0U;
    gradient_length = 0U;
    accumulation_length = 0U;
    accumulation_valid_length = 0U;
    accumulation_result_length = 0U;
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
        if (!sixel_gpu_palette_metal_prepare_device_locked()) {
            sixel_helper_set_additional_message(
                "gpu palette apply: Metal is not available.");
            status = SIXEL_FEATURE_ERROR;
            goto end;
        }

        if (request->test_hooks != NULL) {
            request->test_hooks->entered = 1;
            request->test_hooks->matrix_supported =
                [g_sixel_gpu_metal_device supportsFamily:MTLGPUFamilyApple7]
                && !request->test_hooks->disable_matrix;
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
        for (slot = 0; slot < 6; ++slot)
            lookup_run.buffers[slot] = g_sixel_gpu_metal_dummy_buffer;
        if (lookup != NULL) {
            lookup_run.pipeline = gpu_metal_lookup_pipeline(lookup->chunks,
                request->dither_program, lookup->entry);
            if (lookup_run.pipeline == nil) {
                status = SIXEL_FEATURE_ERROR;
                goto end;
            }
            status = lookup->prepare(&lookup_run, request, &params);
            if (status != SIXEL_OK) goto end;
        }
        params_buffer = sixel_gpu_metal_upload_buffer(
            &g_sixel_gpu_metal_params_buffer,
            &params,
            sizeof(params));
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
                lookup_run.rebuild;
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
                accumulation_result_buffer == nil) {
            sixel_helper_set_additional_message(
                "gpu palette apply: Metal buffer allocation failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }

        pipeline = lookup_run.pipeline;
        lookup_buffer = lookup_run.buffers[5];
        if (generic) {
            pipeline = [program->compiled->pipelines lastObject];
            workspace = (gpu_native_buffer_t *)binding_view.workspace;
            lookup_buffer = workspace->native;
        }
        if (request->test_hooks != NULL)
            request->test_hooks->kernel = generic ? 9 : lookup_run.kernel_id;
        if (sixel_trace_topic_is_enabled("gpu_contract"))
            fprintf(stderr, "LSXGPU2|consumer=palette-apply|kernel=%s\n",
                generic ? program_view->logical_name : lookup_run.kernel);

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
        if (lookup != NULL && lookup_run.rebuild && lookup->build != NULL) {
            [encoder endEncoding];
            encoder = nil;
            status = lookup->build(&lookup_run, command_buffer,
                palette_buffer, params_buffer, request);
            if (status != SIXEL_OK) goto end;
            encoder = [command_buffer computeCommandEncoder];
            if (encoder == nil) {
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
        }
        memset(&dispatch, 0, sizeof(dispatch));
        dispatch.pipeline = pipeline;
        dispatch.generic = generic;
        dispatch.thread_multiple = lookup_run.thread_multiple;
        dispatch.fixed_threads = lookup_run.fixed_threads;
        dispatch.block_width = lookup_run.block_width;
        dispatch.block_height = lookup_run.block_height;
        dispatch.threads_per_block = lookup_run.threads_per_block;
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
        for (slot = 0; slot < 5; ++slot)
            dispatch.buffers[10 + slot] = lookup_run.buffers[slot];
        dispatch.buffers[15] = lookup_buffer;
        dispatch.buffers[16] = configuration_buffer;
        dispatch.buffers[17] = invalid_buffer;
        status = sixel_gpu_metal_apply_ranges(request, &dispatch,
            command_buffer, encoder);
        encoder = nil;
        if (status != SIXEL_OK) goto end;

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
        if (lookup != NULL && lookup->finish != NULL)
            lookup->finish(&lookup_run, request, status);
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
    available = sixel_gpu_dequant_metal_prepare_locked();
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
        if (!sixel_gpu_dequant_metal_prepare_locked()) {
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
        sixel_gpu_statistics_record_metal(command_buffer);
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

SIXEL_INTERNAL_API void
sixel_gpu_statistics_record_metal(id<MTLCommandBuffer> command)
{
    static dispatch_once_t published;
    CFTimeInterval busy;

    if (command == nil) {
        return;
    }
    /* Every Metal stage shares the default device, so the first completed
     * command names the backend for the whole process. */
    dispatch_once(&published, ^{
        @autoreleasepool {
            sixel_gpu_statistics_set_device("metal",
                [[[command device] name] UTF8String]);
        }
    });
    busy = [command GPUEndTime] - [command GPUStartTime];
    sixel_gpu_statistics_record(busy > 0.0
        ? (unsigned long long)(busy * 1e9) : 0U);
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
