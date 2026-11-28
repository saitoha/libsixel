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
#include <stdio.h>
#include <string.h>

#include "palette-kmeans-gpu.h"
#include "filter-binning.h"
#include "filter-factory.h"
#include "filter-sample.h"
#include "compat_stub.h"
#include "options.h"
#include "timer.h"
#include "gpu-runtime-metal.h"
#include "encoder-body-gpu-metal.h"
#include "palette-common-cover.h"
#include "palette-common-merge.h"
#include "palette-common-snap.h"
#include "pixelformat.h"

/* Only immutable pipelines and a thread-safe command queue are shared. Each
 * palette attempt owns all buffers, including its convergence state. */
static pthread_mutex_t sixel_km_metal_lock = PTHREAD_MUTEX_INITIALIZER;
static id<MTLDevice> sixel_km_metal_device = nil;
static id<MTLCommandQueue> sixel_km_metal_queue = nil;
static id<MTLComputePipelineState> sixel_km_metal_pipelines[4];
static int sixel_km_metal_initialized = 0;
static SIXELSTATUS sixel_km_metal_status = SIXEL_FALSE;

enum {
    SIXEL_KM_POINTS,
    SIXEL_KM_WEIGHTS,
    SIXEL_KM_CENTERS,
    SIXEL_KM_MEMBERSHIP,
    SIXEL_KM_DISTANCES,
    SIXEL_KM_PARTIALS,
    SIXEL_KM_SUMS,
    SIXEL_KM_STATE,
    SIXEL_KM_MAXIMA,
    SIXEL_KM_BUFFERS
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
    uint32_t repairs;
} sixel_km_metal_state_t;

/* An integer atomic failure flag detects collapsed nonzero distances and
 * residuals without racing assignment threads. Rejection is request-wide;
 * no partial result is published and AUTO retries the unchanged CPU seeds.
 * Separate literals stay below C99's 4095-byte translation limit. The first
 * pass uses a full center scan. A cooperative 512-point tile then builds
 * ordered membership lists in 14 KiB of threadgroup storage: every weighted
 * point is added once, in exactly the original ascending index order. This
 * avoids the per-center full tile scan without introducing FP32 atomics or
 * reordering sums. The second sum pass retains tile order.
 * Each tile stores its maximum and a maximum for each 32-point block.
 * Consuming a donor changes one residual. Refresh that block and scan
 * the block maxima in ascending order to retain the lowest index on
 * ties, including when every residual is zero.
 * All iterations are submitted together: no CPU readback occurs between
 * assignment, reduction, empty-cluster repair and convergence decisions. */
/* Accel: KMEANS-LLOYD metal */
static char const *const sixel_km_metal_source[] = {
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct Params { uint points, centers, groups, min_iterations;\n"
    " uint round_centers; float threshold; };\n"
    "struct State { uint done, iterations; atomic_uint failed; float delta;\n"
    " uint repairs; };\n"
    "kernel void km_assign(\n"
    " device const packed_float3 *points [[buffer(0)]],\n"
    " device const float *weights [[buffer(1)]],\n"
    " device const packed_float3 *centers [[buffer(2)]],\n"
    " device uint *membership [[buffer(3)]],\n"
    " device float *distances [[buffer(4)]],\n"
    " device State &s [[buffer(7)]],\n"
    " constant Params &p [[buffer(9)]],\n"
    " uint gid [[thread_position_in_grid]]) {\n"
    " if (s.done || gid >= p.points) return;\n"
    " float weight = weights[gid];\n"
    " if (weight <= 0) { membership[gid] = 0; distances[gid] = 0; return; }\n"
    " float3 point = float3(points[gid]);\n"
    " uint owner = s.iterations == 0 ? 0 : membership[gid];\n"
    " float3 d = point - float3(centers[owner]);\n"
    " float best = d.x*d.x + d.y*d.y + d.z*d.z;\n"
    " if (best == 0 && any(d != 0))\n"
    "  atomic_store_explicit(&s.failed, 1u, memory_order_relaxed);\n"
    " for (uint c = 0; c < p.centers; ++c) {\n"
    "  d = point - float3(centers[c]);\n"
    "  float distance = d.x*d.x + d.y*d.y + d.z*d.z;\n"
    "  if (distance == 0 && any(d != 0))\n"
    "   atomic_store_explicit(&s.failed, 1u, memory_order_relaxed);\n"
    "  if (distance < best) { best = distance; owner = c; }\n"
    " }\n"
    " float residual = best * weight;\n"
    " if (!isfinite(residual) || (residual == 0 && best > 0))\n"
    "  atomic_store_explicit(&s.failed, 1u, memory_order_relaxed);\n"
    " membership[gid] = owner; distances[gid] = residual;\n"
    "}\n",
    "kernel void km_partial(\n"
    " device const packed_float3 *points [[buffer(0)]],\n"
    " device const float *weights [[buffer(1)]],\n"
    " device const uint *membership [[buffer(3)]],\n"
    " device const float *distances [[buffer(4)]],\n"
    " device float4 *partials [[buffer(5)]],\n"
    " device const State &s [[buffer(7)]],\n"
    " device uint *maxima [[buffer(8)]],\n"
    " constant Params &p [[buffer(9)]],\n"
    " uint group [[threadgroup_position_in_grid]],\n"
    " uint lane [[thread_index_in_threadgroup]],\n"
    " uint lanes [[threads_per_threadgroup]]) {\n"
    " threadgroup float4 moments[512];\n"
    " threadgroup uint heads[1024], links[512];\n"
    " uint begin, count, i, c, owner, selected;\n"
    " float weight, farthest;\n"
    " float4 sum;\n"
    " if (s.done) return;\n"
    " begin = group * 512u; count = min(512u, p.points - begin);\n"
    " for (c = lane; c < p.centers; c += lanes) heads[c] = 512u;\n"
    " for (i = lane; i < count; i += lanes) {\n"
    "  weight = weights[begin+i];\n"
    "  moments[i] = float4(float3(points[begin+i]) * weight, weight);\n"
    "  links[i] = membership[begin+i];\n"
    " }\n"
    " threadgroup_barrier(mem_flags::mem_threadgroup);\n"
    " if (lane == 0) {\n"
    "  farthest = -1; selected = begin;\n"
    "  // Prepend in reverse order to preserve ascending sample summation.\n"
    "  for (i = count; i > 0;) {\n"
    "   --i; owner = links[i]; links[i] = heads[owner]; heads[owner] = i;\n"
    "   // Reverse traversal uses >= so equal residuals keep the lowest "
    "index.\n"
    "   if (distances[begin+i] >= farthest) {\n"
    "    farthest = distances[begin+i]; selected = begin+i;\n"
    "   }\n"
    "  }\n"
    "  maxima[group * 17u] = selected;\n"
    " }\n"
    " for (uint block = lane; block < 16u; block += lanes) {\n"
    "  uint first = begin + block * 32u;\n"
    "  uint end = min(first + 32u, begin + count);\n"
    "  if (first >= end) continue;\n"
    "  uint pick = first;\n"
    "  float score = distances[first];\n"
    "  for (uint j = first + 1u; j < end; ++j) {\n"
    "   float candidate = distances[j];\n"
    "   if (candidate > score) { score = candidate; pick = j; }\n"
    "  }\n"
    "  maxima[group * 17u + 1u + block] = pick;\n"
    " }\n"
    " threadgroup_barrier(mem_flags::mem_threadgroup);\n"
    " for (c = lane; c < p.centers; c += lanes) {\n"
    "  sum = 0;\n"
    "  for (i = heads[c]; i != 512u; i = links[i]) {\n"
    "   if (moments[i].w > 0) sum += moments[i];\n"
    "  }\n"
    "  partials[group*p.centers+c] = sum;\n"
    " }\n"
    "}\n"
    "kernel void km_reduce(\n"
    " device const float4 *partials [[buffer(5)]],\n"
    " device float4 *sums [[buffer(6)]],\n"
    " device const State &s [[buffer(7)]],\n"
    " constant Params &p [[buffer(9)]],\n"
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
    " device uint *maxima [[buffer(8)]],\n"
    " constant Params &p [[buffer(9)]],\n"
    " uint gid [[thread_position_in_grid]]) {\n"
    " if (gid != 0 || s.done) return;\n"
    " if (atomic_load_explicit(&s.failed, memory_order_relaxed)) {\n"
    "  s.done = 1; return;\n"
    " }\n"
    " for (uint c = 0; c < p.centers; ++c) {\n"
    "  if (sums[c].w > 0) continue;\n"
    "  ++s.repairs;\n"
    "  float farthest = -1; uint selected = 0;\n"
    "  for (uint g = 0; g < p.groups; ++g) {\n"
    "   uint i = maxima[g * 17u];\n"
    "   if (distances[i] > farthest) { farthest = distances[i]; selected = i; "
    "}\n"
    "  }\n"
    "  uint old = membership[selected]; float weight = weights[selected];\n"
    "  float4 moved = float4(float3(points[selected]) * weight, weight);\n"
    "  if (sums[old].w > 0) {\n"
    "   float4 remaining = sums[old] - moved;\n"
    "   if (p.round_centers) remaining.xyz = max(remaining.xyz, float3(0));\n"
    "   remaining.w = max(remaining.w, 0.0f); sums[old] = remaining;\n"
    "  }\n"
    "  sums[c] = moved; membership[selected] = c; distances[selected] = 0;\n"
    "  uint group = selected / 512u, begin = group * 512u;\n"
    "  uint end = min(begin + 512u, p.points);\n"
    "  uint block = (selected - begin) / 32u;\n"
    "  uint first = begin + block * 32u;\n"
    "  uint last = min(first + 32u, end);\n"
    "  uint base = group * 17u;\n"
    "  farthest = -1; selected = first;\n"
    "  for (uint i = first; i < last; ++i) {\n"
    "   float candidate = distances[i];\n"
    "   if (candidate > farthest) { farthest = candidate; selected = i; }\n"
    "  }\n"
    "  maxima[base + 1u + block] = selected;\n"
    "  farthest = -1;\n"
    "  for (uint b = 0; begin + b * 32u < end; ++b) {\n"
    "   uint i = maxima[base + 1u + b];\n"
    "   float candidate = distances[i];\n"
    "   if (candidate > farthest) { farthest = candidate; selected = i; }\n"
    "  }\n"
    "  maxima[base] = selected;\n"
    " }\n"
    " float delta = 0;\n"
    " for (uint c = 0; c < p.centers; ++c) {\n"
    "  if (!all(isfinite(sums[c]))) {\n"
    "   atomic_store_explicit(&s.failed, 1u, memory_order_relaxed);\n"
    "   s.done = 1; return;\n"
    "  }\n"
    "  if (sums[c].w <= 0) continue;\n"
    "  float3 next = sums[c].xyz / sums[c].w;\n"
    "  if (p.round_centers) next = floor(clamp(next, 0.0f, 255.0f) + 0.5f);\n"
    "  float3 d = next - float3(centers[c]);\n"
    "  delta += d.x*d.x + d.y*d.y + d.z*d.z; centers[c] = "
    "packed_float3(next);\n"
    " }\n"
    " ++s.iterations; s.delta = delta;\n"
    " if (!isfinite(delta)) {\n"
    "  atomic_store_explicit(&s.failed, 1u, memory_order_relaxed); s.done = "
    "1;\n"
    " }\n"
    " else if (delta <= p.threshold && s.iterations >= p.min_iterations)\n"
    "  s.done = 1;\n"
    "}\n"};

static SIXELSTATUS sixel_km_metal_prepare(void)
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
    /* A transient initialization failure must not poison future requests.
     * The caller holds the lock; discard only unpublished partial state. */
    for (index = 0u; index < 4u; ++index) {
        [sixel_km_metal_pipelines[index] release];
        sixel_km_metal_pipelines[index] = nil;
    }
    [sixel_km_metal_queue release];
    sixel_km_metal_queue = nil;
    [sixel_km_metal_device release];
    sixel_km_metal_status = SIXEL_FALSE;
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
                                 sizeof(sixel_km_metal_source[0]);
         ++index) {
        chunk =
            [[NSString alloc] initWithUTF8String:sixel_km_metal_source[index]];
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
                                                  options:options
                                                    error:&error];
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
        function = [library
            newFunctionWithName:[NSString stringWithUTF8String:names[index]]];
        if (function == nil) {
            [library release];
            return sixel_km_metal_status;
        }
        sixel_km_metal_pipelines[index] =
            [sixel_km_metal_device newComputePipelineStateWithFunction:function
                                                                 error:&error];
        [function release];
        if (sixel_km_metal_pipelines[index] == nil) {
            [library release];
            return sixel_km_metal_status;
        }
    }
    [library release];
    sixel_km_metal_initialized = 1;
    sixel_km_metal_status = SIXEL_OK;
    return SIXEL_OK;
}

int sixel_kmeans_metal_is_initialized(void)
{
    int initialized;

    /* The compiler publishes all four pipelines under this lock and never
     * replaces them after success. A concurrent preparation is still cold
     * for AUTO: do not wait for it or speculatively compile for later work. */
    if (pthread_mutex_trylock(&sixel_km_metal_lock) != 0) {
        return 0;
    }
    initialized = sixel_km_metal_initialized;
    pthread_mutex_unlock(&sixel_km_metal_lock);
    return initialized;
}

int sixel_kmeans_metal_is_available(void)
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
    double minimum[3];
    double maximum[3];
    double value;
    double margin;
    unsigned int channel;
    /* Profiling splits dispatches only when explicitly requested. Normal
     * tracing retains the production single-command-buffer submission. */
    id<MTLCommandBuffer> commands[116 * 4];
    double encode_ms[116 * 4];
    double submit_ms[116 * 4];
    double phase_ms[9];
    double start, mark, phase_start, gpu_start, gpu_end, total_ms;
    char const *profile;
    char const *device_name;
    char const *phase_names[9] = {"prepare",  "allocate", "upload",
                                  "encode",   "submit",   "wait",
                                  "validate", "publish",  "release"};
    char const *stage_names[4] = {"assign", "partial", "reduce", "update"};
    unsigned int command_count, finished_iterations, repairs, command_index;
    int trace, split, completed, active_phase;

    trace = sixel_diagnostics_trace_topic_is_enabled("kmeans_backend");
    profile = sixel_compat_getenv("SIXEL_KMEANS_GPU_PROFILE");
    split = trace && profile != NULL && strcmp(profile, "stages") == 0;
    start = trace ? sixel_timer_now() : 0.0;
    phase_start = start;
    active_phase = 0;
    command_count = finished_iterations = repairs = 0u;
    completed = 0;
    command = nil;
    memset(commands, 0, sizeof(commands));
    memset(encode_ms, 0, sizeof(encode_ms));
    memset(submit_ms, 0, sizeof(submit_ms));
    memset(phase_ms, 0, sizeof(phase_ms));
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
    lengths[SIXEL_KM_PARTIALS] =
        (NSUInteger)params.groups * params.centers * 4u * sizeof(float);
    lengths[SIXEL_KM_SUMS] = (NSUInteger)params.centers * 4u * sizeof(float);
    lengths[SIXEL_KM_STATE] = sizeof(*state);
    lengths[SIXEL_KM_MAXIMA] = (NSUInteger)params.groups *
        SIXEL_KMEANS_GPU_MAXIMA_PER_TILE * sizeof(uint32_t);
    counts[0] = params.points;
    counts[1] = params.groups;
    counts[2] = params.centers;
    counts[3] = 1u;

    @autoreleasepool {
        if (request->test_failure == 1) {
            status = SIXEL_FEATURE_ERROR;
            goto cleanup;
        }
        pthread_mutex_lock(&sixel_km_metal_lock);
        status = sixel_km_metal_prepare();
        pthread_mutex_unlock(&sixel_km_metal_lock);
        if (status != SIXEL_OK) {
            goto cleanup;
        }
        if (trace) {
            phase_ms[0] = (sixel_timer_now() - phase_start) * 1000.0;
            phase_start = sixel_timer_now();
        }
        active_phase = 1;
        status = SIXEL_BAD_ALLOCATION;
        for (index = 0u; index < SIXEL_KM_BUFFERS; ++index) {
            buffers[index] = [sixel_km_metal_device
                newBufferWithLength:lengths[index]
                            options:MTLResourceStorageModeShared];
            if (buffers[index] == nil ||
                (request->test_failure == 2 && index == 3u)) {
                goto cleanup;
            }
        }
        if (trace) {
            phase_ms[1] = (sixel_timer_now() - phase_start) * 1000.0;
            phase_start = sixel_timer_now();
        }
        active_phase = 2;
        values = [buffers[SIXEL_KM_POINTS] contents];
        for (index = 0u; index < (size_t)params.points * 3u; ++index) {
            values[index] = (float)request->points[index];
        }
        values = [buffers[SIXEL_KM_WEIGHTS] contents];
        for (index = 0u; index < params.points; ++index) {
            values[index] = request->weights == NULL
                                ? 1.0f
                                : (float)request->weights[index];
        }
        values = [buffers[SIXEL_KM_CENTERS] contents];
        for (index = 0u; index < (size_t)params.centers * 3u; ++index) {
            values[index] = (float)request->centers[index];
        }
        state = [buffers[SIXEL_KM_STATE] contents];
        memset(state, 0, sizeof(*state));
        if (trace) {
            phase_ms[2] = (sixel_timer_now() - phase_start) * 1000.0;
            phase_start = sixel_timer_now();
        }
        active_phase = -1;
        mark = trace ? sixel_timer_now() : 0.0;
        if (!split) {
            command = [sixel_km_metal_queue commandBuffer];
            if (command == nil)
                goto cleanup;
        }
        if (trace)
            phase_ms[3] += (sixel_timer_now() - mark) * 1000.0;
        for (iteration = 0u; iteration < request->max_iterations; ++iteration) {
            for (stage = 0u; stage < 4u; ++stage) {
                command_index = iteration * 4u + stage;
                mark = trace ? sixel_timer_now() : 0.0;
                if (split) {
                    command = [sixel_km_metal_queue commandBuffer];
                    if (command == nil)
                        goto cleanup;
                }
                encoder = [command computeCommandEncoder];
                if (encoder == nil) {
                    goto cleanup;
                }
                [encoder
                    setComputePipelineState:sixel_km_metal_pipelines[stage]];
                for (index = 0u; index < SIXEL_KM_BUFFERS; ++index) {
                    [encoder setBuffer:buffers[index] offset:0 atIndex:index];
                }
                [encoder setBytes:&params
                           length:sizeof(params)
                          atIndex:SIXEL_KM_BUFFERS];
                threads = stage == 3u ? 1u : 128u;
                if (threads > [sixel_km_metal_pipelines[stage]
                                  maxTotalThreadsPerThreadgroup]) {
                    threads = [sixel_km_metal_pipelines[stage]
                        maxTotalThreadsPerThreadgroup];
                }
                /* Partial sums use one cooperative group per 512 points;
                 * the other kernels retain one thread per output item. */
                count = stage == 1u ? counts[stage]
                                    : (counts[stage] + threads - 1u) / threads;
                [encoder dispatchThreadgroups:MTLSizeMake(count, 1, 1)
                        threadsPerThreadgroup:MTLSizeMake(threads, 1, 1)];
                [encoder endEncoding];
                if (trace) {
                    encode_ms[command_index] =
                        (sixel_timer_now() - mark) * 1000.0;
                    phase_ms[3] += encode_ms[command_index];
                }
                if (split) {
                    /* Queue order preserves every dependency. There is no
                     * CPU wait/readback between iterations, even here. */
                    commands[command_count++] = command;
                    mark = sixel_timer_now();
                    [command commit];
                    submit_ms[command_index] =
                        (sixel_timer_now() - mark) * 1000.0;
                    phase_ms[4] += submit_ms[command_index];
                }
            }
        }
        if (!split) {
            commands[command_count++] = command;
            mark = trace ? sixel_timer_now() : 0.0;
            [command commit];
            if (trace)
                phase_ms[4] = (sixel_timer_now() - mark) * 1000.0;
        }
        mark = trace ? sixel_timer_now() : 0.0;
        [command waitUntilCompleted];
        sixel_gpu_statistics_record_metal(command);
        completed = 1;
        if (trace)
            phase_ms[5] = (sixel_timer_now() - mark) * 1000.0;
        phase_start = trace ? sixel_timer_now() : 0.0;
        active_phase = 6;
        status = SIXEL_RUNTIME_ERROR;
        /* A later buffer can complete after an earlier one failed. Check
         * every submission before reading or publishing shared results. */
        for (index = 0u; index < command_count; ++index) {
            if ([commands[index] status] != MTLCommandBufferStatusCompleted)
                goto cleanup;
        }
        if (request->test_failure == 3 || state->failed ||
            state->iterations == 0u ||
            state->iterations > request->max_iterations) {
            goto cleanup;
        }
        finished_iterations = state->iterations;
        repairs = state->repairs;
        /* Means must remain in the convex coordinate envelope. Empty
         * centers may retain their seeds; byte rounding expands by 0.5.
         * This also catches unstable cancellation during donor repair. */
        for (channel = 0u; channel < 3u; ++channel) {
            minimum[channel] = maximum[channel] = request->points[channel];
        }
        for (index = 0u; index < (size_t)params.points * 3u; ++index) {
            channel = (unsigned int)(index % 3u);
            value = request->points[index];
            minimum[channel] = fmin(minimum[channel], value);
            maximum[channel] = fmax(maximum[channel], value);
        }
        for (index = 0u; index < (size_t)params.centers * 3u; ++index) {
            channel = (unsigned int)(index % 3u);
            value = request->centers[index];
            minimum[channel] = fmin(minimum[channel], value);
            maximum[channel] = fmax(maximum[channel], value);
        }
        values = [buffers[SIXEL_KM_CENTERS] contents];
        for (index = 0u; index < (size_t)params.centers * 3u; ++index) {
            channel = (unsigned int)(index % 3u);
            margin =
                fmax(maximum[channel] - minimum[channel],
                     fmax(fabs(minimum[channel]), fabs(maximum[channel]))) *
                0.0001;
            if (request->round_centers)
                margin += 0.5;
            if (!isfinite(values[index]) ||
                values[index] < minimum[channel] - margin ||
                values[index] > maximum[channel] + margin) {
                goto cleanup;
            }
        }
        if (trace) {
            phase_ms[6] = (sixel_timer_now() - phase_start) * 1000.0;
            phase_start = sixel_timer_now();
        }
        active_phase = 7;
        /* Publish atomically with respect to fallback: no caller-owned
         * state changes until every output component has been checked. */
        for (index = 0u; index < (size_t)params.centers * 3u; ++index) {
            request->centers[index] = (double)values[index];
        }
        if (request->membership != NULL) {
            memcpy(request->membership, [buffers[SIXEL_KM_MEMBERSHIP] contents],
                   (size_t)params.points * sizeof(unsigned int));
        }
        *iterations = state->iterations;
        status = SIXEL_OK;
    cleanup:
        if (trace && active_phase >= 0)
            phase_ms[active_phase] +=
                (sixel_timer_now() - phase_start) * 1000.0;
        /* Encoding failure after partial submission must still drain the
         * queue before freeing attempt resources or returning to fallback. */
        if (!completed && command_count > 0u) {
            mark = trace ? sixel_timer_now() : 0.0;
            [commands[command_count - 1u] waitUntilCompleted];
            sixel_gpu_statistics_record_metal(commands[command_count - 1u]);
            if (trace)
                phase_ms[5] += (sixel_timer_now() - mark) * 1000.0;
        }
        mark = trace ? sixel_timer_now() : 0.0;
        for (index = 0u; index < SIXEL_KM_BUFFERS; ++index) {
            [buffers[index] release];
        }
        if (trace) {
            phase_ms[8] = (sixel_timer_now() - mark) * 1000.0;
            total_ms = (sixel_timer_now() - start) * 1000.0;
            device_name = [[sixel_km_metal_device name] UTF8String];
            fprintf(stderr,
                    "LSXKMTIME1|mode=%s|M=%u|K=%u|limit=%u|"
                    "iterations=%u|repairs=%u|commands=%u|status=0x%04x|"
                    "wall_ms=%.6f|device=%s|\n",
                    split ? "stages" : "batch", params.points, params.centers,
                    request->max_iterations, finished_iterations, repairs,
                    command_count, (unsigned int)status, total_ms,
                    device_name != NULL ? device_name : "unavailable");
            for (index = 0u; index < 9u; ++index) {
                fprintf(stderr, "LSXKMPHASE1|phase=%s|cpu_ms=%.6f|\n",
                        phase_names[index], phase_ms[index]);
            }
            for (index = 0u; index < command_count; ++index) {
                /* GPU timestamps are valid only after completion. Zero
                 * means unavailable, not a zero-cost GPU dispatch. */
                gpu_start = [commands[index] GPUStartTime];
                gpu_end = [commands[index] GPUEndTime];
                fprintf(stderr,
                        "LSXKMSTAGE1|mode=%s|phase=%s|iteration=%u|"
                        "noop=%d|gpu_valid=%d|gpu_start_s=%.9f|gpu_end_s=%.9f|"
                        "gpu_ms=%.6f|encode_ms=%.6f|submit_ms=%.6f|\n",
                        split ? "stages" : "batch",
                        split ? stage_names[index % 4u] : "batch",
                        split ? (unsigned int)(index / 4u + 1u) : 0u,
                        split && index / 4u >= finished_iterations,
                        gpu_start > 0.0 && gpu_end >= gpu_start, gpu_start,
                        gpu_end,
                        gpu_start > 0.0 && gpu_end >= gpu_start
                            ? (gpu_end - gpu_start) * 1000.0
                            : -1.0,
                        split ? encode_ms[index] : phase_ms[3],
                        split ? submit_ms[index] : phase_ms[4]);
            }
        }
    }
    return status;
}
/* Reuse the production Lloyd, Heckbert split and 6bit lookup shaders. */
static char const *const sixel_hb_resident_source[] = {
#include <generated-metal/palette-heckbert-gpu-shader.inc.h>
};

static char const *const sixel_km_resident_sources[] = {
#include <generated-metal/lookup-policy-6bit-gpu-shader.inc.h>
#include <generated-metal/lookup-policy-6bit-gpu-query-shader.inc.h>
#include <generated-metal/palette-kmeans-resident-shader.inc.h>
};

static char const *const sixel_ward_resident_sources[] = {
#include <generated-metal/palette-ward-shader.inc.h>
#include <generated-metal/palette-ward-resident-shader.inc.h>
};

enum {
    KM_HISTOGRAM, KM_COUNT, KM_PREFIX, KM_COMPACT, KM_MOMENTS, KM_AXIS,
    KM_TILES, KM_MERGE, KM_SEED, KM_ASSIGN, KM_PARTIAL, KM_REDUCE, KM_UPDATE,
    KM_SCORE, KM_OBJECTIVE, KM_PACK, KM_SNAP, KM_COVER_HIST, KM_COVER_GAPS,
    KM_COVER, KM_LUT, KM_APPLY, HB_FIRST, HB_ACCUMULATE, HB_PREFIX, HB_COMPACT,
    HB_SOLVE, HB_REP, HB_WARD_SOLVE,
    WD_KM_POINTS, WD_KM_INIT, WD_KM_COLLECT, WD_KM_CLUSTERS,
    WD_HB_POINTS, WD_HB_CLUSTERS, WD_SEED, WD_ROWS, WD_MERGE, WD_REPLAY,
    WD_LLOYD_CLEAR, WD_LLOYD_ASSIGN, WD_LLOYD_UPDATE, WD_PACK, KM_STAGE_COUNT
};

enum {
    KM_PARAMS = 9, KM_RGB = 10, KM_HIST = 11, KM_GEOMETRY = 12,
    KM_OFFSETS = 13,
    KM_DISPATCH = 14, KM_FIRST_KEYS = 17, KM_SECOND_KEYS = 18,
    KM_ARGUMENT = 19, KM_PALETTE = 21, KM_TABLE = 22, KM_INDICES = 23,
    KM_POLICY = 24, KM_SNAP_TABLE = 25, KM_COVER_BINS = 26,
    KM_ANCHORS = 28, KM_RESULT = 29, KM_BUFFER_COUNT = 30
};

static pthread_mutex_t sixel_km_resident_lock = PTHREAD_MUTEX_INITIALIZER;
static id<MTLDevice> sixel_km_resident_device;
static id<MTLCommandQueue> sixel_km_resident_queue;
static sixel_body_gpu_t *sixel_km_resident_body;
static id<MTLComputePipelineState> sixel_km_resident_stages[KM_STAGE_COUNT];
static id<MTLBuffer> sixel_km_resident_buffers[KM_BUFFER_COUNT];
enum {
    HB_POINTS, HB_ORDER, HB_SCRATCH, HB_BOXES, HB_PARAMS, HB_SOLVED,
    HB_FIRST_INDEX, HB_MOMENTS, HB_SAMPLES,
    HB_TILES = 9, HB_BUFFER_COUNT = 10, HB_TONES = 20,
    HB_GRID_TILE = 128, HB_GRID_CELLS = 64 * 64 * 64
};
static id<MTLBuffer> sixel_hb_resident_buffers[HB_BUFFER_COUNT];
enum { WD_WORK, WD_MEANS, WD_CACHE, WD_PARAMS, WD_PAIRS, WD_BUFFERS };
static id<MTLBuffer> sixel_ward_resident_buffers[WD_BUFFERS];

typedef struct sixel_hb_resident_params {
    uint32_t count, colors, total;
    float weight0, weight1, weight2;
} sixel_hb_resident_params_t;

typedef struct sixel_km_geometry {
    uint32_t width, height, stride, colors, min_iterations;
    float threshold;
    uint32_t target, heckbert, representative;
} sixel_km_geometry_t;

static SIXELSTATUS
sixel_km_resident_prepare(int heckbert, int ward)
{
    static char const *const names[] = {
        "histogram", "count_bins", "prefix", "compact", "moments", "axis",
        "tiles", "merge_keys", "seed", "km_assign", "km_partial", "km_reduce",
        "km_update", "score", "objective", "pack", "final_snap",
        "cover_histogram", "cover_gaps", "cover_repair", "lookup_6bit_build",
        "apply_palette", "heckbert_histogram_first",
        "heckbert_histogram_accumulate", "heckbert_histogram_prefix",
        "heckbert_histogram_compact", "heckbert_solve",
        "heckbert_representatives", "heckbert_solve",
        "resident_ward_km_points", "resident_ward_km_init",
        "resident_ward_km_collect", "resident_ward_km_clusters",
        "resident_ward_hb_points", "resident_ward_hb_clusters",
        "resident_ward_seed", "ward_rows", "ward_merge", "resident_ward_replay",
        "resident_ward_lloyd_clear", "resident_ward_lloyd_assign",
        "resident_ward_lloyd_update", "resident_ward_pack"
    };
    NSMutableString *source;
    MTLCompileOptions *options;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    NSError *error;
    unsigned int i;

    if (sixel_km_resident_stages[KM_APPLY] != nil &&
            sixel_km_resident_stages[heckbert ? HB_REP : KM_PACK] != nil &&
            (!heckbert || sixel_km_resident_stages[ward ? HB_WARD_SOLVE
                                                         : HB_SOLVE]) &&
            (!ward || sixel_km_resident_stages[heckbert ? WD_HB_CLUSTERS
                                                        : WD_KM_CLUSTERS]))
        return SIXEL_OK;
    if (sixel_km_resident_device == nil)
        sixel_km_resident_device = MTLCreateSystemDefaultDevice();
    if (sixel_km_resident_device == nil)
        return SIXEL_FALSE;
    if (sixel_km_resident_queue == nil)
        sixel_km_resident_queue = [sixel_km_resident_device newCommandQueue];
    if (sixel_km_resident_queue == nil)
        return SIXEL_BAD_ALLOCATION;
    source = [NSMutableString string];
    for (i = 0; i < sizeof(sixel_km_metal_source) /
                    sizeof(sixel_km_metal_source[0]); ++i)
        [source appendString:@(sixel_km_metal_source[i])];
    if (heckbert) {
        [source appendString:@"\n#define SIXEL_HECKBERT_RESIDENT 1\n"];
        if (ward)
            [source appendString:@"#define SIXEL_HECKBERT_WARD_RESIDENT 1\n"];
        for (i = 0; i < sizeof(sixel_hb_resident_source) /
                        sizeof(sixel_hb_resident_source[0]); ++i)
            [source appendString:@(sixel_hb_resident_source[i])];
    }
    for (i = 0; i < sizeof(sixel_km_resident_sources) /
                    sizeof(sixel_km_resident_sources[0]); ++i)
        [source appendString:@(sixel_km_resident_sources[i])];
    if (ward) {
        for (i = 0; i < sizeof(sixel_ward_resident_sources) /
                         sizeof(sixel_ward_resident_sources[0]); ++i)
            [source appendString:@(sixel_ward_resident_sources[i])];
    }
    options = [[MTLCompileOptions alloc] init];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    options.fastMathEnabled = NO;
#pragma clang diagnostic pop
    library = [sixel_km_resident_device newLibraryWithSource:source
                                                 options:options error:&error];
    [options release];
    if (library == nil) {
        NSLog(@"Resident Metal palette: %@", error);
        return SIXEL_FEATURE_ERROR;
    }
    for (i = 0; i < KM_STAGE_COUNT; ++i) {
        if (sixel_km_resident_stages[i] != nil ||
                (heckbert && i < KM_SNAP) ||
                (!heckbert && i >= HB_FIRST && i <= HB_WARD_SOLVE) ||
                (ward && i == HB_SOLVE) ||
                (!ward && i >= HB_WARD_SOLVE) ||
                (heckbert && i >= WD_KM_POINTS && i <= WD_KM_CLUSTERS) ||
                (!heckbert && i >= WD_HB_POINTS && i <= WD_HB_CLUSTERS))
            continue;
        function = [library newFunctionWithName:@(names[i])];
        sixel_km_resident_stages[i] = function == nil ? nil :
            [sixel_km_resident_device newComputePipelineStateWithFunction:
                function error:nil];
        [function release];
        if (sixel_km_resident_stages[i] == nil) {
            NSLog(@"Resident Metal palette stage %s is unavailable", names[i]);
            [library release];
            return SIXEL_FEATURE_ERROR;
        }
    }
    [library release];
    return SIXEL_OK;
}

static int
sixel_resident_buffers_allocate(id<MTLBuffer> *buffers,
    NSUInteger const *lengths, unsigned int count, uint32_t shared)
{
    id<MTLBuffer> buffer;
    MTLResourceOptions mode;
    unsigned int i;

    for (i = 0; i < count; ++i) {
        if (lengths[i] == 0)
            continue;
        mode = (shared & (1u << i)) != 0
            ? MTLResourceStorageModeShared : MTLResourceStorageModePrivate;
        if (buffers[i].length >= lengths[i] &&
                buffers[i].resourceOptions == mode)
            continue;
        buffer = [sixel_km_resident_device newBufferWithLength:lengths[i]
                                                      options:mode];
        if (buffer == nil)
            return 0;
        [buffers[i] release];
        buffers[i] = buffer;
    }
    return 1;
}

static int
sixel_km_resident_storage(unsigned int samples, unsigned int pixels,
                          unsigned int colors, int heckbert, int ward,
                          int spell)
{
    NSUInteger padded, groups;
    NSUInteger lengths[KM_BUFFER_COUNT];
    NSUInteger hb_lengths[HB_BUFFER_COUNT];
    /* WardWork contains fixed byte-grid storage and integer cluster
     * moments. The trailing pad covers its aligned control words. */
    static NSUInteger const ward_lengths[WD_BUFFERS] = {
        24u*32768u + 128u*SIXEL_WARD_MAX_CLUSTERS + 32u,
        16u*SIXEL_WARD_MAX_CLUSTERS, 8u*SIXEL_WARD_MAX_CLUSTERS, 8u,
        4u*SIXEL_WARD_MAX_CLUSTERS
    };
    uint32_t shared;

    padded = (samples + 127u) & ~127u;
    groups = (padded + 511u) / 512u;
    shared = (1u << KM_RGB) | (1u << KM_INDICES) | (1u << KM_SNAP_TABLE) |
             (1u << KM_ANCHORS) | (1u << KM_RESULT);
    if (spell)
        shared &= ~(1u << KM_INDICES);
    memcpy(lengths, (NSUInteger[]){
        12*padded, 4*padded, 12*colors, 4*padded, 4*padded,
        16*colors*groups, 16*colors, 20, 68*groups, 24,
        3*(NSUInteger)pixels, 16*32768, 0, 4*256, 24,
        48*((padded+63)/64), 16, 8*padded, 8*padded, 0, 4,
        768, 262144, pixels, 0, 256, 16*4096, 4*4096, 78, 776
    }, sizeof(lengths));
    if (heckbert) {
        /* The shared palette tail uses only state, parameters and slots
         * 10 onward. The Heckbert split has its own typed buffers. */
        memcpy(hb_lengths, (NSUInteger[]){
            [HB_POINTS] = 16*samples,
            [HB_ORDER] = 4*samples,
            [HB_SCRATCH] = 4*samples,
            [HB_BOXES] = 16*colors,
            [HB_PARAMS] = sizeof(sixel_hb_resident_params_t),
            [HB_SOLVED] = 4,
            [HB_FIRST_INDEX] = 4*HB_GRID_CELLS,
            [HB_MOMENTS] = 16*samples,
            [HB_SAMPLES] = 4*samples,
            [HB_TILES] = 4*((samples + HB_GRID_TILE - 1u) / HB_GRID_TILE)
        }, sizeof(hb_lengths));
        memset(lengths, 0, SIXEL_KM_STATE*sizeof(*lengths));
        lengths[SIXEL_KM_MAXIMA] = 0;
        lengths[KM_HIST] = 0;
        memset(lengths + KM_OFFSETS, 0, (KM_PALETTE-KM_OFFSETS)*
                                            sizeof(*lengths));
        if (!sixel_resident_buffers_allocate(sixel_hb_resident_buffers,
                hb_lengths, HB_BUFFER_COUNT, 0u))
            return 0;
    }
    if (ward && !sixel_resident_buffers_allocate(sixel_ward_resident_buffers,
            ward_lengths, WD_BUFFERS, 0u))
        return 0;
    return sixel_resident_buffers_allocate(sixel_km_resident_buffers,
        lengths, KM_BUFFER_COUNT, shared);
}

static int
sixel_km_resident_dispatch(id<MTLCommandBuffer> command, unsigned int stage,
    unsigned int count, unsigned int lanes, int indirect, unsigned int arg,
    int reverse, sixel_km_geometry_t const *geometry, uint32_t const policy[4])
{
    id<MTLComputeCommandEncoder> encoder;
    id<MTLBuffer> *buffers;
    MTLSize group;
    unsigned int i, slots;

    buffers = sixel_km_resident_buffers;
    if (lanes > sixel_km_resident_stages[stage].maxTotalThreadsPerThreadgroup)
        return 0;
    encoder = [command computeCommandEncoder];
    if (encoder == nil)
        return 0;
    [encoder setComputePipelineState:sixel_km_resident_stages[stage]];
    for (i = 0; i < KM_RESULT; ++i) {
        if (buffers[i] != nil)
            [encoder setBuffer:buffers[i] offset:0 atIndex:i];
    }
    if ((stage >= HB_FIRST && stage <= HB_WARD_SOLVE) ||
            stage == WD_HB_POINTS || stage == WD_HB_CLUSTERS) {
        slots = stage < HB_SOLVE ? HB_BUFFER_COUNT : HB_FIRST_INDEX;
        for (i = 0; i < slots; ++i)
            [encoder setBuffer:sixel_hb_resident_buffers[i] offset:0
                        atIndex:i];
        if (stage == HB_FIRST || stage == HB_COMPACT)
            [encoder setBytes:sixel_safe_tones length:sizeof(sixel_safe_tones)
                      atIndex:HB_TONES];
    }
    if (stage >= WD_KM_POINTS) {
        [encoder setBuffer:sixel_ward_resident_buffers[WD_WORK] offset:0
                    atIndex:30];
        [encoder setBuffer:sixel_ward_resident_buffers[WD_MEANS] offset:0
                    atIndex:29];
        [encoder setBuffer:sixel_ward_resident_buffers[WD_PARAMS] offset:0
                    atIndex:28];
        [encoder setBuffer:sixel_ward_resident_buffers[WD_PAIRS] offset:0
                    atIndex:27];
        if (stage == WD_PACK)
            [encoder setBytes:sixel_safe_tones length:sizeof(sixel_safe_tones)
                      atIndex:HB_TONES];
        if (stage == WD_ROWS || stage == WD_MERGE) {
            [encoder setBuffer:sixel_ward_resident_buffers[WD_MEANS] offset:0
                        atIndex:0];
            [encoder setBuffer:sixel_ward_resident_buffers[WD_CACHE] offset:0
                        atIndex:1];
            [encoder setBuffer:sixel_ward_resident_buffers[WD_PARAMS] offset:0
                        atIndex:2];
            [encoder setBuffer:sixel_ward_resident_buffers[WD_PAIRS] offset:0
                        atIndex:3];
        }
    }
    if (reverse) {
        [encoder setBuffer:buffers[KM_SECOND_KEYS] offset:0
                    atIndex:KM_FIRST_KEYS];
        [encoder setBuffer:buffers[KM_FIRST_KEYS] offset:0
                    atIndex:KM_SECOND_KEYS];
    }
    [encoder setBytes:geometry length:sizeof(*geometry) atIndex:KM_GEOMETRY];
    [encoder setBytes:&arg length:sizeof(arg) atIndex:KM_ARGUMENT];
    [encoder setBytes:policy length:4*sizeof(*policy) atIndex:KM_POLICY];
    if (stage == KM_LUT) {
        [encoder setBuffer:buffers[KM_TABLE] offset:0 atIndex:0];
        [encoder setBuffer:buffers[KM_PALETTE] offset:0 atIndex:1];
        [encoder setBuffer:buffers[9] offset:4 atIndex:2];
    }
    group = MTLSizeMake(lanes, 1, 1);
    if (indirect >= 0) {
        [encoder dispatchThreadgroupsWithIndirectBuffer:buffers[KM_DISPATCH]
            indirectBufferOffset:(NSUInteger)indirect
            threadsPerThreadgroup:group];
    } else {
        [encoder dispatchThreadgroups:MTLSizeMake((count+lanes-1)/lanes, 1, 1)
                 threadsPerThreadgroup:group];
    }
    [encoder endEncoding];
    return 1;
}

typedef struct sixel_metal_binning_context {
    id<MTLCommandBuffer> command;
    sixel_km_geometry_t const *geometry;
    uint32_t const *policy;
    int heckbert;
} sixel_metal_binning_context_t;

static SIXELSTATUS
sixel_metal_record_binning(void *context,
    sixel_filter_sample_grid_t const *grid)
{
    sixel_metal_binning_context_t const *binning;
    unsigned int samples, tiles, lanes;

    binning = context;
    samples = (unsigned int)grid->count;
#define RECORD(stage, count, lanes, arg) \
    if (!sixel_km_resident_dispatch(binning->command, stage, count, lanes, \
             -1, arg, 0, binning->geometry, binning->policy)) \
        return SIXEL_RUNTIME_ERROR
    if (binning->heckbert) {
        tiles = (samples + HB_GRID_TILE - 1u) / HB_GRID_TILE;
        lanes = (unsigned int)
            sixel_km_resident_stages[HB_COMPACT].threadExecutionWidth;
        if (!lanes)
            return SIXEL_RUNTIME_ERROR;

        /* The first pass gathers sampled pixels directly from the RGB
         * buffer and writes the first-seen cell index in the same launch. */
        RECORD(HB_FIRST, samples, HB_GRID_TILE, 0);
        RECORD(HB_ACCUMULATE, samples, HB_GRID_TILE, samples);
        RECORD(HB_PREFIX, 1, 1, samples);
        RECORD(HB_COMPACT, tiles * lanes, lanes, samples);
    } else {
        RECORD(KM_HISTOGRAM, samples, 128, 0);
        RECORD(KM_COUNT, 256, 64, 0);
        RECORD(KM_PREFIX, 1, 1, 0);
        RECORD(KM_COMPACT, 256, 64, 0);
    }
#undef RECORD
    return SIXEL_OK;
}

SIXELSTATUS
sixel_palette_metal_resident(sixel_palette_resident_request_t const *request,
    unsigned char const *rgb, unsigned char *indices,
    unsigned char *palette, unsigned int *colors,
    sixel_palette_resident_body_t *body)
{
    sixel_body_metal_palette_t body_palette;
    sixel_km_geometry_t geometry;
    sixel_filter_sample_grid_t const *grid;
    sixel_filter_binning_config_t binning;
    sixel_metal_binning_context_t binning_context;
    sixel_filter_t binning_filter;
    NSUInteger simd, lanes;
    uint32_t policy[4];
    uint32_t const *result;
    unsigned int samples, pixels, i, run, tiles, split_stage;
    unsigned int ward_rows_width, ward_merge_lanes;
    int reverse, heckbert, ward;
    SIXELSTATUS status;
    id<MTLBuffer> *buffers;
    id<MTLCommandBuffer> command;
    id<MTLBlitCommandEncoder> blit;
    unsigned char *table;
    float value, target;

    if (request == NULL || rgb == NULL ||
            (body == NULL && (!indices || !palette || !colors)) ||
            request->colors == 0 ||
            request->colors > SIXEL_PALETTE_MAX) {
        return SIXEL_BAD_ARGUMENT;
    }
    heckbert = request->quantizer == SIXEL_QUANTIZE_MODEL_MEDIANCUT;
    ward = request->ward_colors != 0;
    ward_rows_width = 0;
    ward_merge_lanes = 0;
    if (ward && (request->ward_colors <= request->colors ||
            request->ward_colors > SIXEL_WARD_MAX_CLUSTERS ||
            request->ward_iterations > 30u))
        return SIXEL_BAD_ARGUMENT;
    if (!heckbert &&
            (request->max_iterations == 0 || request->max_iterations > 116u))
        return SIXEL_BAD_ARGUMENT;
    grid = request->grid;
    if (grid == NULL || !grid->stride || !grid->width || !grid->height ||
            !grid->count || grid->count > 32768u ||
            grid->width > UINT32_MAX / 3u / grid->height)
        return SIXEL_FALSE;
    pixels = grid->width * grid->height;
    samples = (unsigned int)grid->count;
    geometry = (sixel_km_geometry_t){grid->width, grid->height, grid->stride,
        ward ? request->ward_colors : request->colors, request->min_iterations,
        request->threshold, request->colors, heckbert, request->representative};
    tiles = (samples + HB_GRID_TILE - 1u) / HB_GRID_TILE;
    policy[0] = request->snap;
    policy[1] = request->cover;
    policy[2] = request->soft;
    policy[3] = request->grow;
    buffers = sixel_km_resident_buffers;
    pthread_mutex_lock(&sixel_km_resident_lock);
    @autoreleasepool {
        if (body != NULL && sixel_km_resident_body == NULL &&
                sixel_body_metal_resident_new(&sixel_km_resident_body) !=
                    SIXEL_OK) {
            status = SIXEL_FALSE;
            goto end;
        }
        status = sixel_km_resident_prepare(heckbert, ward);
        if (ward && status == SIXEL_FEATURE_ERROR)
            status = SIXEL_FALSE;
        if (status != SIXEL_OK)
            goto end;
        if (ward) {
            ward_rows_width = (unsigned int)
                sixel_km_resident_stages[WD_ROWS].threadExecutionWidth;
            ward_merge_lanes = sixel_ward_metal_merge_lanes(geometry.colors,
                (unsigned int)
                    sixel_km_resident_stages[WD_MERGE].threadExecutionWidth,
                (unsigned int)sixel_km_resident_stages[WD_MERGE]
                    .maxTotalThreadsPerThreadgroup);
            if (!ward_rows_width || !ward_merge_lanes) {
                status = SIXEL_FALSE;
                goto end;
            }
        }
        status = SIXEL_BAD_ALLOCATION;
        if (!sixel_km_resident_storage(samples, pixels, geometry.colors,
                heckbert, ward, body != NULL))
            goto end;
        memcpy(buffers[KM_RGB].contents, rgb, 3*(size_t)pixels);
        if (request->snap) {
            table = buffers[KM_SNAP_TABLE].contents;
            for (i = 0; i < 256; ++i) {
                value = sixel_pixelformat_byte_to_float(
                    SIXEL_PIXELFORMAT_RGB888, 0, (unsigned char)i);
                target = sixel_pixelformat_byte_to_float(
                    SIXEL_PIXELFORMAT_RGB888, 0,
                    sixel_palette_reversible_value(i));
                value += (target-value)*(float)request->snap_rate;
                table[i] = sixel_pixelformat_float_channel_to_byte(
                    SIXEL_PIXELFORMAT_RGB888, 0, value);
            }
        }
        if (request->cover && !request->soft) {
            table = buffers[KM_ANCHORS].contents;
            for (i = 0; i < SIXEL_PALETTE_COVER_ANCHOR_MAX; ++i)
                memcpy(table+3*i, sixel_palette_cover_fixed_anchor(i), 3);
        }

        status = SIXEL_RUNTIME_ERROR;
        command = [sixel_km_resident_queue commandBuffer];
        blit = [command blitCommandEncoder];
        if (blit == nil)
            goto end;
        if (heckbert) {
            [blit fillBuffer:sixel_hb_resident_buffers[HB_FIRST_INDEX]
                       range:NSMakeRange(0,4*HB_GRID_CELLS) value:255];
            [blit fillBuffer:sixel_hb_resident_buffers[HB_MOMENTS]
                       range:NSMakeRange(0,16*samples) value:0];
            [blit fillBuffer:sixel_hb_resident_buffers[HB_TILES]
                       range:NSMakeRange(0,4*tiles) value:0];
        } else {
            [blit fillBuffer:buffers[KM_HIST]
                       range:NSMakeRange(0,16*32768) value:0];
        }
        [blit fillBuffer:buffers[SIXEL_KM_STATE]
                   range:NSMakeRange(0,20) value:0];
        if (request->cover && request->soft)
            [blit fillBuffer:buffers[KM_COVER_BINS]
                       range:NSMakeRange(0,16*4096) value:0];
        [blit endEncoding];

        memset(&binning, 0, sizeof(binning));
        binning_context = (sixel_metal_binning_context_t){
            command, &geometry, policy, heckbert};
        binning.grid_input = grid;
        binning.record = sixel_metal_record_binning;
        binning.record_context = &binning_context;
        status = sixel_filter_factory_init_by_kind(SIXEL_FILTER_KIND_BINNING,
                                                   &binning, &binning_filter);
        if (status != SIXEL_OK)
            goto end;
        status = sixel_filter_run(&binning_filter, NULL, NULL);
        sixel_filter_teardown(&binning_filter);
        if (status != SIXEL_OK)
            goto end;
        status = SIXEL_RUNTIME_ERROR;

#define RUN(stage, count, lanes, indirect, arg, reverse) \
        if (!sixel_km_resident_dispatch(command, stage, count, lanes, \
                 indirect, arg, reverse, &geometry, policy)) goto end
        if (heckbert) {
            split_stage = ward ? HB_WARD_SOLVE : HB_SOLVE;
            simd = sixel_km_resident_stages[split_stage].threadExecutionWidth;
            lanes = sixel_km_resident_stages[split_stage]
                .maxTotalThreadsPerThreadgroup;
            if (!simd || 256u % simd != 0u)
                goto end;
            if (lanes > 1024u)
                lanes = 1024u;
            if (lanes > 32u * simd)
                lanes = 32u * simd;
            lanes -= lanes % simd;
            if (lanes < 256u)
                goto end;
            RUN(split_stage, (unsigned int)lanes, (unsigned int)lanes,
                -1, 0, 0);
            if (ward) {
                RUN(WD_HB_POINTS, samples, 128, -1, 0, 0);
                RUN(WD_HB_CLUSTERS, geometry.colors, 128, -1, 0, 0);
            } else {
                RUN(HB_REP, request->colors, 128, -1, 0, 0);
            }
        } else {
            if (ward)
                RUN(WD_KM_POINTS, 256, 64, -1, 0, 0);
            RUN(KM_MOMENTS, (samples+63)/64, 32, -1, 0, 0);
            RUN(KM_AXIS, 1, 1, -1, 0, 0);
            RUN(KM_TILES, 0, 128, 0, 0, 0);
            reverse = 0;
            for (run = 128; run < samples; run *= 2) {
                RUN(KM_MERGE, 0, 128, 0, run, reverse);
                reverse = !reverse;
            }
            RUN(KM_SEED, 1, 1, -1, 0, reverse);
            for (i = 0; i < request->max_iterations; ++i) {
                RUN(KM_ASSIGN, 0, 128, 0, 0, 0);
                RUN(KM_PARTIAL, 0, 128, 12, 0, 0);
                RUN(KM_REDUCE, geometry.colors, 128, -1, 0, 0);
                RUN(KM_UPDATE, 1, 1, -1, 0, 0);
            }
            if (ward) {
                RUN(WD_KM_INIT, geometry.colors, 128, -1, 0, 0);
                RUN(KM_ASSIGN, 0, 128, 0, 0, 0);
                RUN(WD_KM_COLLECT, 0, 128, 0, 0, 0);
                RUN(WD_KM_CLUSTERS, geometry.colors, 128, -1, 0, 0);
            } else {
                RUN(KM_SCORE, 0, 128, 0, 0, 0);
                RUN(KM_OBJECTIVE, 1, 1, -1, 0, 0);
                RUN(KM_PACK, request->colors, 128, -1, 0, 0);
            }
        }
        if (ward) {
            RUN(WD_SEED, geometry.colors, 128, -1, 0, 0);
            RUN(WD_ROWS, geometry.colors * ward_rows_width, ward_rows_width,
                -1, 0, 0);
            RUN(WD_MERGE, ward_merge_lanes, ward_merge_lanes, -1, 0, 0);
            RUN(WD_REPLAY, 1, 1, -1, 0, 0);
            for (i = 0; i < request->ward_iterations; ++i) {
                RUN(WD_LLOYD_CLEAR, request->colors, 128, -1, 0, 0);
                RUN(WD_LLOYD_ASSIGN, samples, 128, -1, 0, 0);
                RUN(WD_LLOYD_UPDATE, 1, 1, -1, 0, 0);
            }
            RUN(WD_PACK, request->colors, 128, -1, 0, 0);
        }
        if (request->snap) {
            RUN(KM_SNAP, request->colors, 128, -1, 0, 0);
        }
        if (request->cover && request->soft) {
            RUN(KM_COVER_HIST, samples, 128, -1, 0, 0);
            RUN(KM_COVER_GAPS, 4096, 128, -1, samples, 0);
        }
        if (request->cover) {
            RUN(KM_COVER, 256, 256, -1, 0, 0);
        }
        RUN(KM_LUT, 262144, 128, -1, 0, 0);
        RUN(KM_APPLY, pixels, 128, -1, 0, 0);
#undef RUN
        if (body != NULL) {
            body_palette = (sixel_body_metal_palette_t){
                buffers[KM_PALETTE], buffers[KM_PARAMS],
                buffers[SIXEL_KM_STATE],
                offsetof(sixel_km_metal_params_t, centers),
                offsetof(sixel_km_metal_state_t, failed)
            };
            status = sixel_body_metal_record(sixel_km_resident_body,
                command, buffers[KM_INDICES], &body_palette, grid->width,
                grid->height, request->body_gri_limit);
            if (status != SIXEL_OK)
                goto end;
            status = SIXEL_RUNTIME_ERROR;
        } else {
            /* Publish only after the entire indexed result is ready. */
            blit = [command blitCommandEncoder];
            if (blit == nil)
                goto end;
            [blit copyFromBuffer:buffers[KM_PALETTE] sourceOffset:0
                        toBuffer:buffers[KM_RESULT]
                destinationOffset:8 size:768];
            [blit copyFromBuffer:buffers[KM_PARAMS] sourceOffset:4
                        toBuffer:buffers[KM_RESULT] destinationOffset:0 size:4];
            [blit copyFromBuffer:buffers[SIXEL_KM_STATE] sourceOffset:8
                        toBuffer:buffers[KM_RESULT] destinationOffset:4 size:4];
            [blit endEncoding];
        }
        [command commit];
        [command waitUntilCompleted];
        sixel_gpu_statistics_record_metal(command);
        if (command.status != MTLCommandBufferStatusCompleted)
            goto end;
        if (body != NULL) {
            status = sixel_body_metal_copy(sixel_km_resident_body,
                &body->bytes, &body->size);
            goto end;
        }
        result = buffers[KM_RESULT].contents;
        if (result[1] || result[0] == 0 || result[0] > 256) {
            if (ward)
                status = SIXEL_FALSE;
            goto end;
        }
        *colors = result[0];
        memcpy(palette, result+2, 3*result[0]);
        memcpy(indices, buffers[KM_INDICES].contents, pixels);
        status = SIXEL_OK;
end:
        pthread_mutex_unlock(&sixel_km_resident_lock);
    }
    return status;
}
#endif /* HAVE_METAL */
