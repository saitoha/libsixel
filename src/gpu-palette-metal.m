/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_METAL)

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sixel.h>

#include "bluenoise_64x64.h"
#include "gpu-palette.h"

enum {
    SIXEL_GPU_METAL_MODE_NONE = 0,
    SIXEL_GPU_METAL_MODE_BLUENOISE = 1
};

typedef struct sixel_gpu_metal_params {
    uint32_t pixel_count;
    uint32_t width;
    uint32_t height;
    uint32_t ncolors;
    uint32_t mode;
    uint32_t has_transparent;
    uint32_t transparent_size;
    uint32_t transparent_keycolor;
    float bluenoise_strength;
    float bluenoise_gradient_factor;
    int32_t bluenoise_phase_x;
    int32_t bluenoise_phase_y;
    uint32_t bluenoise_channel_rgb;
    uint32_t gradient_size;
    uint32_t gradient_width;
    uint32_t gradient_height;
} sixel_gpu_metal_params_t;

typedef struct sixel_gpu_metal_cached_buffer {
    id<MTLBuffer> buffer;
    NSUInteger capacity;
} sixel_gpu_metal_cached_buffer_t;

static pthread_mutex_t g_sixel_gpu_metal_lock = PTHREAD_MUTEX_INITIALIZER;
static id<MTLDevice> g_sixel_gpu_metal_device = nil;
static id<MTLCommandQueue> g_sixel_gpu_metal_queue = nil;
static id<MTLComputePipelineState> g_sixel_gpu_metal_pipeline = nil;
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
"    uint mode;\n"
"    uint has_transparent;\n"
"    uint transparent_size;\n"
"    uint transparent_keycolor;\n"
"    float bluenoise_strength;\n"
"    float bluenoise_gradient_factor;\n"
"    int bluenoise_phase_x;\n"
"    int bluenoise_phase_y;\n"
"    uint bluenoise_channel_rgb;\n"
"    uint gradient_size;\n"
"    uint gradient_width;\n"
"    uint gradient_height;\n"
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
"kernel void sixel_gpu_palette_apply(\n"
"    device uchar *result [[buffer(0)]],\n"
"    device const uchar *pixels [[buffer(1)]],\n"
"    device const uchar *palette [[buffer(2)]],\n"
"    device const uchar *transparent_mask [[buffer(3)]],\n"
"    device const uchar *blue_noise [[buffer(4)]],\n"
"    device const uchar *gradient [[buffer(5)]],\n"
"    constant Params& params [[buffer(6)]],\n"
"    uint gid [[thread_position_in_grid]])\n"
"{\n"
"    uint x;\n"
"    uint y;\n"
"    uint best;\n"
"    uint best_dist;\n"
"    uchar q[3];\n"
"    if (gid >= params.pixel_count) {\n"
"        return;\n"
"    }\n"
"    if (params.has_transparent != 0u && gid < params.transparent_size &&\n"
"        transparent_mask[gid] != 0u) {\n"
"        result[gid] = uchar(params.transparent_keycolor);\n"
"        return;\n"
"    }\n"
"    x = gid % params.width;\n"
"    y = gid / params.width;\n"
"    q[0] = pixels[gid * 3u + 0u];\n"
"    q[1] = pixels[gid * 3u + 1u];\n"
"    q[2] = pixels[gid * 3u + 2u];\n"
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
"    best = 0u;\n"
"    best_dist = 0xffffffffu;\n"
"    for (uint i = 0u; i < params.ncolors; ++i) {\n"
"        int dr = int(q[0]) - int(palette[i * 3u + 0u]);\n"
"        int dg = int(q[1]) - int(palette[i * 3u + 1u]);\n"
"        int db = int(q[2]) - int(palette[i * 3u + 2u]);\n"
"        uint dist = uint(dr * dr + dg * dg + db * db);\n"
"        if (dist < best_dist) {\n"
"            best_dist = dist;\n"
"            best = i;\n"
"        }\n"
"    }\n"
"    result[gid] = uchar(best);\n"
"}\n",
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
    unsigned char dummy;

    error = nil;
    source = nil;
    library = nil;
    function = nil;
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
    [library release];
    library = nil;
    if (function == nil) {
        return 0;
    }
    g_sixel_gpu_metal_pipeline =
        [g_sixel_gpu_metal_device newComputePipelineStateWithFunction:function
                                                                 error:&error];
    [function release];
    function = nil;
    if (g_sixel_gpu_metal_pipeline == nil) {
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
    id<MTLCommandBuffer> command_buffer;
    id<MTLComputeCommandEncoder> encoder;
    NSUInteger threads_per_group;
    NSUInteger groups;
    NSUInteger result_length;
    NSUInteger pixel_length;
    NSUInteger palette_length;
    NSUInteger transparent_length;
    NSUInteger gradient_length;
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
    command_buffer = nil;
    encoder = nil;
    threads_per_group = 0U;
    groups = 0U;
    result_length = 0U;
    pixel_length = 0U;
    palette_length = 0U;
    transparent_length = 0U;
    gradient_length = 0U;
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
        transparent_length = request->transparent_mask != NULL ?
            (NSUInteger)request->transparent_mask_size : 1U;
        gradient_length = request->bluenoise_gradient_map != NULL ?
            (NSUInteger)request->bluenoise_gradient_map_size : 1U;

        result_buffer = sixel_gpu_metal_ensure_buffer(
            &g_sixel_gpu_metal_result_buffer,
            result_length);
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
        if (result_buffer == nil || pixel_buffer == nil ||
                palette_buffer == nil || transparent_buffer == nil ||
                blue_noise_buffer == nil || gradient_buffer == nil ||
                params_buffer == nil) {
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

        memcpy(request->dest, [result_buffer contents], result_length);
        status = SIXEL_OK;

    end:
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
