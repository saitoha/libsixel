/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_METAL)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

#include "palette-kmeans-gpu.h"

/* Only immutable pipelines and a thread-safe command queue are shared. Each
 * palette attempt owns all buffers, including its convergence state. */
static pthread_mutex_t sixel_km_metal_lock = PTHREAD_MUTEX_INITIALIZER;
static id<MTLDevice> sixel_km_metal_device = nil;
static id<MTLCommandQueue> sixel_km_metal_queue = nil;
static id<MTLComputePipelineState> sixel_km_metal_pipelines[4];
static int sixel_km_metal_initialized = 0;
static SIXELSTATUS sixel_km_metal_status = SIXEL_FALSE;

enum {
    SIXEL_KM_POINTS, SIXEL_KM_WEIGHTS, SIXEL_KM_CENTERS,
    SIXEL_KM_MEMBERSHIP, SIXEL_KM_DISTANCES, SIXEL_KM_PARTIALS,
    SIXEL_KM_SUMS, SIXEL_KM_STATE, SIXEL_KM_BUFFERS
};

typedef struct sixel_km_metal_params {
    uint32_t points;
    uint32_t centers;
    uint32_t groups;
    uint32_t min_iterations;
    uint32_t round_centers;
    float threshold;
} sixel_km_metal_params_t;

typedef struct sixel_km_metal_state {
    uint32_t done;
    uint32_t iterations;
    uint32_t failed;
    float delta;
} sixel_km_metal_state_t;

/* Separate literals stay below C99's 4095-byte translation limit. The first
 * pass uses a full center scan. The next two passes perform deterministic
 * segmented sums without floating-point atomics or an M-by-K distance table.
 * All iterations are submitted together: no CPU readback occurs between
 * assignment, reduction, empty-cluster repair and convergence decisions. */
static char const * const sixel_km_metal_source[] = {
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct Params { uint points, centers, groups, min_iterations;\n"
" uint round_centers; float threshold; };\n"
"struct State { uint done, iterations, failed; float delta; };\n"
"kernel void km_assign(\n"
" device const packed_float3 *points [[buffer(0)]],\n"
" device const float *weights [[buffer(1)]],\n"
" device const packed_float3 *centers [[buffer(2)]],\n"
" device uint *membership [[buffer(3)]],\n"
" device float *distances [[buffer(4)]],\n"
" device const State &s [[buffer(7)]],\n"
" constant Params &p [[buffer(8)]],\n"
" uint gid [[thread_position_in_grid]]) {\n"
" if (s.done || gid >= p.points) return;\n"
" float weight = weights[gid];\n"
" if (weight <= 0) { membership[gid] = 0; distances[gid] = 0; return; }\n"
" float3 point = float3(points[gid]);\n"
" float3 d = point - float3(centers[0]);\n"
" float best = d.x*d.x + d.y*d.y + d.z*d.z; uint owner = 0;\n"
" for (uint c = 1; c < p.centers; ++c) {\n"
"  d = point - float3(centers[c]);\n"
"  float distance = d.x*d.x + d.y*d.y + d.z*d.z;\n"
"  if (distance < best) { best = distance; owner = c; }\n"
" }\n"
" membership[gid] = owner; distances[gid] = best * weight;\n"
"}\n",
"kernel void km_partial(\n"
" device const packed_float3 *points [[buffer(0)]],\n"
" device const float *weights [[buffer(1)]],\n"
" device const uint *membership [[buffer(3)]],\n"
" device float4 *partials [[buffer(5)]],\n"
" device const State &s [[buffer(7)]],\n"
" constant Params &p [[buffer(8)]],\n"
" uint gid [[thread_position_in_grid]]) {\n"
" if (s.done || gid >= p.groups * p.centers) return;\n"
" uint group = gid / p.centers, center = gid % p.centers;\n"
" uint end = min((group + 1) * 512u, p.points); float4 sum = 0;\n"
" for (uint i = group * 512u; i < end; ++i) {\n"
"  if (membership[i] == center && weights[i] > 0)\n"
"   sum += float4(float3(points[i]) * weights[i], weights[i]);\n"
" }\n"
" partials[gid] = sum;\n"
"}\n"
"kernel void km_reduce(\n"
" device const float4 *partials [[buffer(5)]],\n"
" device float4 *sums [[buffer(6)]],\n"
" device const State &s [[buffer(7)]],\n"
" constant Params &p [[buffer(8)]],\n"
" uint gid [[thread_position_in_grid]]) {\n"
" if (s.done || gid >= p.centers) return;\n"
" float4 sum = 0;\n"
" for (uint g = 0; g < p.groups; ++g) sum += partials[g*p.centers+gid];\n"
" sums[gid] = sum;\n"
"}\n",
"kernel void km_update(\n"
" device const packed_float3 *points [[buffer(0)]],\n"
" device const float *weights [[buffer(1)]],\n"
" device packed_float3 *centers [[buffer(2)]],\n"
" device uint *membership [[buffer(3)]],\n"
" device float *distances [[buffer(4)]],\n"
" device float4 *sums [[buffer(6)]],\n"
" device State &s [[buffer(7)]],\n"
" constant Params &p [[buffer(8)]],\n"
" uint gid [[thread_position_in_grid]]) {\n"
" if (gid != 0 || s.done) return;\n"
" for (uint c = 0; c < p.centers; ++c) {\n"
"  if (sums[c].w > 0) continue;\n"
"  float farthest = -1; uint selected = 0;\n"
"  for (uint i = 0; i < p.points; ++i) {\n"
"   if (distances[i] > farthest) { farthest = distances[i]; selected = i; }\n"
"  }\n"
"  uint old = membership[selected]; float weight = weights[selected];\n"
"  float4 moved = float4(float3(points[selected]) * weight, weight);\n"
"  if (sums[old].w > 0) {\n"
"   float4 remaining = sums[old] - moved;\n"
"   if (p.round_centers) remaining.xyz = max(remaining.xyz, float3(0));\n"
"   remaining.w = max(remaining.w, 0.0f); sums[old] = remaining;\n"
"  }\n"
"  sums[c] = moved; membership[selected] = c; distances[selected] = 0;\n"
" }\n"
" float delta = 0;\n"
" for (uint c = 0; c < p.centers; ++c) {\n"
"  if (!all(isfinite(sums[c]))) { s.failed = 1; s.done = 1; return; }\n"
"  if (sums[c].w <= 0) continue;\n"
"  float3 next = sums[c].xyz / sums[c].w;\n"
"  if (p.round_centers) next = floor(clamp(next, 0.0f, 255.0f) + 0.5f);\n"
"  float3 d = next - float3(centers[c]);\n"
"  delta += d.x*d.x + d.y*d.y + d.z*d.z; centers[c] = packed_float3(next);\n"
" }\n"
" ++s.iterations; s.delta = delta;\n"
" if (!isfinite(delta)) { s.failed = 1; s.done = 1; }\n"
" else if (delta <= p.threshold && s.iterations >= p.min_iterations)\n"
"  s.done = 1;\n"
"}\n"
};

static SIXELSTATUS
sixel_km_metal_prepare(void)
{
    NSMutableString *source;
    NSString *chunk;
    MTLCompileOptions *options;
    NSError *error;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    char const *names[4];
    size_t index;

    if (sixel_km_metal_initialized) {
        return sixel_km_metal_status;
    }
    sixel_km_metal_initialized = 1;
    sixel_km_metal_device = MTLCreateSystemDefaultDevice();
    if (sixel_km_metal_device == nil) {
        return SIXEL_FALSE;
    }
    sixel_km_metal_status = SIXEL_FEATURE_ERROR;
    sixel_km_metal_queue = [sixel_km_metal_device newCommandQueue];
    if (sixel_km_metal_queue == nil) {
        return sixel_km_metal_status;
    }
    source = [[NSMutableString alloc] init];
    if (source == nil) {
        return SIXEL_BAD_ALLOCATION;
    }
    for (index = 0u; index < sizeof(sixel_km_metal_source) /
            sizeof(sixel_km_metal_source[0]); ++index) {
        chunk = [[NSString alloc]
            initWithUTF8String:sixel_km_metal_source[index]];
        if (chunk == nil) {
            [source release];
            return SIXEL_BAD_ALLOCATION;
        }
        [source appendString:chunk];
        [chunk release];
    }
    options = [[MTLCompileOptions alloc] init];
    /* Keep the deployment target compatible with pre-macOS-15 SDKs. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    options.fastMathEnabled = NO;
#pragma clang diagnostic pop
    error = nil;
    library = [sixel_km_metal_device newLibraryWithSource:source
        options:options error:&error];
    [options release];
    [source release];
    if (library == nil) {
        return sixel_km_metal_status;
    }
    names[0] = "km_assign";
    names[1] = "km_partial";
    names[2] = "km_reduce";
    names[3] = "km_update";
    for (index = 0u; index < 4u; ++index) {
        function = [library newFunctionWithName:
            [NSString stringWithUTF8String:names[index]]];
        if (function == nil) {
            [library release];
            return sixel_km_metal_status;
        }
        sixel_km_metal_pipelines[index] = [sixel_km_metal_device
            newComputePipelineStateWithFunction:function error:&error];
        [function release];
        if (sixel_km_metal_pipelines[index] == nil) {
            [library release];
            return sixel_km_metal_status;
        }
    }
    [library release];
    sixel_km_metal_status = SIXEL_OK;
    return SIXEL_OK;
}

int
sixel_kmeans_metal_is_available(void)
{
    id<MTLDevice> device;
    int available;

    available = 0;
    @autoreleasepool {
        device = MTLCreateSystemDefaultDevice();
        available = device != nil;
        [device release];
    }
    return available;
}

SIXELSTATUS
sixel_kmeans_metal_refine(sixel_kmeans_gpu_request_t const *request,
                         unsigned int *iterations)
{
    SIXELSTATUS status;
    sixel_km_metal_params_t params;
    sixel_km_metal_state_t *state;
    id<MTLBuffer> buffers[SIXEL_KM_BUFFERS];
    id<MTLCommandBuffer> command;
    id<MTLComputeCommandEncoder> encoder;
    NSUInteger lengths[SIXEL_KM_BUFFERS];
    NSUInteger counts[4];
    NSUInteger threads;
    NSUInteger count;
    size_t index;
    unsigned int iteration;
    unsigned int stage;
    float *values;

    memset(buffers, 0, sizeof(buffers));
    memset(&params, 0, sizeof(params));
    params.points = request->point_count;
    params.centers = request->center_count;
    params.groups = (params.points + 511u) / 512u;
    params.min_iterations = request->min_iterations;
    params.round_centers = request->round_centers != 0;
    params.threshold = (float)request->threshold;
    lengths[SIXEL_KM_POINTS] = (NSUInteger)params.points * 3u * sizeof(float);
    lengths[SIXEL_KM_WEIGHTS] = (NSUInteger)params.points * sizeof(float);
    lengths[SIXEL_KM_CENTERS] = (NSUInteger)params.centers * 3u * sizeof(float);
    lengths[SIXEL_KM_MEMBERSHIP] = (NSUInteger)params.points * sizeof(uint32_t);
    lengths[SIXEL_KM_DISTANCES] = lengths[SIXEL_KM_WEIGHTS];
    lengths[SIXEL_KM_PARTIALS] = (NSUInteger)params.groups * params.centers *
        4u * sizeof(float);
    lengths[SIXEL_KM_SUMS] = (NSUInteger)params.centers * 4u * sizeof(float);
    lengths[SIXEL_KM_STATE] = sizeof(*state);
    counts[0] = params.points;
    counts[1] = (NSUInteger)params.groups * params.centers;
    counts[2] = params.centers;
    counts[3] = 1u;

    @autoreleasepool {
        pthread_mutex_lock(&sixel_km_metal_lock);
        status = sixel_km_metal_prepare();
        pthread_mutex_unlock(&sixel_km_metal_lock);
        if (status != SIXEL_OK) {
            goto cleanup;
        }
        status = SIXEL_BAD_ALLOCATION;
        for (index = 0u; index < SIXEL_KM_BUFFERS; ++index) {
            buffers[index] = [sixel_km_metal_device
                newBufferWithLength:lengths[index]
                options:MTLResourceStorageModeShared];
            if (buffers[index] == nil) {
                goto cleanup;
            }
        }
        values = [buffers[SIXEL_KM_POINTS] contents];
        for (index = 0u; index < (size_t)params.points * 3u; ++index) {
            values[index] = (float)request->points[index];
        }
        values = [buffers[SIXEL_KM_WEIGHTS] contents];
        for (index = 0u; index < params.points; ++index) {
            values[index] = request->weights == NULL ? 1.0f :
                (float)request->weights[index];
        }
        values = [buffers[SIXEL_KM_CENTERS] contents];
        for (index = 0u; index < (size_t)params.centers * 3u; ++index) {
            values[index] = (float)request->centers[index];
        }
        state = [buffers[SIXEL_KM_STATE] contents];
        memset(state, 0, sizeof(*state));
        command = [sixel_km_metal_queue commandBuffer];
        if (command == nil) {
            goto cleanup;
        }
        for (iteration = 0u; iteration < request->max_iterations;
                ++iteration) {
            for (stage = 0u; stage < 4u; ++stage) {
                encoder = [command computeCommandEncoder];
                if (encoder == nil) {
                    goto cleanup;
                }
                [encoder setComputePipelineState:
                    sixel_km_metal_pipelines[stage]];
                for (index = 0u; index < SIXEL_KM_BUFFERS; ++index) {
                    [encoder setBuffer:buffers[index] offset:0 atIndex:index];
                }
                [encoder setBytes:&params length:sizeof(params)
                    atIndex:SIXEL_KM_BUFFERS];
                threads = stage == 3u ? 1u : 128u;
                if (threads > [sixel_km_metal_pipelines[stage]
                        maxTotalThreadsPerThreadgroup]) {
                    threads = [sixel_km_metal_pipelines[stage]
                        maxTotalThreadsPerThreadgroup];
                }
                count = (counts[stage] + threads - 1u) / threads;
                [encoder dispatchThreadgroups:MTLSizeMake(count, 1, 1)
                    threadsPerThreadgroup:MTLSizeMake(threads, 1, 1)];
                [encoder endEncoding];
            }
        }
        [command commit];
        [command waitUntilCompleted];
        status = SIXEL_RUNTIME_ERROR;
        if ([command status] != MTLCommandBufferStatusCompleted ||
                state->failed || state->iterations == 0u ||
                state->iterations > request->max_iterations) {
            goto cleanup;
        }
        values = [buffers[SIXEL_KM_CENTERS] contents];
        for (index = 0u; index < (size_t)params.centers * 3u; ++index) {
            if (!isfinite(values[index])) {
                goto cleanup;
            }
        }
        /* Publish atomically with respect to fallback: no caller-owned
         * state changes until every output component has been checked. */
        for (index = 0u; index < (size_t)params.centers * 3u; ++index) {
            request->centers[index] = (double)values[index];
        }
        *iterations = state->iterations;
        status = SIXEL_OK;
cleanup:
        for (index = 0u; index < SIXEL_KM_BUFFERS; ++index) {
            [buffers[index] release];
        }
    }
    return status;
}
#endif /* HAVE_METAL */
