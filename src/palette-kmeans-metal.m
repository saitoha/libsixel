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
#include "compat_stub.h"
#include "options.h"
#include "timer.h"

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
    uint32_t repairs;
} sixel_km_metal_state_t;

/* An integer atomic failure flag detects collapsed nonzero distances and
 * residuals without racing assignment threads. Rejection is request-wide;
 * no partial result is published and AUTO retries the unchanged CPU seeds.
 * Separate literals stay below C99's 4095-byte translation limit. The first
 * pass uses a full center scan. The next two passes perform deterministic
 * segmented sums without floating-point atomics or an M-by-K distance table.
 * All iterations are submitted together: no CPU readback occurs between
 * assignment, reduction, empty-cluster repair and convergence decisions. */
static char const * const sixel_km_metal_source[] = {
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
" constant Params &p [[buffer(8)]],\n"
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
" if (atomic_load_explicit(&s.failed, memory_order_relaxed)) {\n"
"  s.done = 1; return;\n"
" }\n"
" for (uint c = 0; c < p.centers; ++c) {\n"
"  if (sums[c].w > 0) continue;\n"
"  ++s.repairs;\n"
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
"  if (!all(isfinite(sums[c]))) {\n"
"   atomic_store_explicit(&s.failed, 1u, memory_order_relaxed);\n"
"   s.done = 1; return;\n"
"  }\n"
"  if (sums[c].w <= 0) continue;\n"
"  float3 next = sums[c].xyz / sums[c].w;\n"
"  if (p.round_centers) next = floor(clamp(next, 0.0f, 255.0f) + 0.5f);\n"
"  float3 d = next - float3(centers[c]);\n"
"  delta += d.x*d.x + d.y*d.y + d.z*d.z; centers[c] = packed_float3(next);\n"
" }\n"
" ++s.iterations; s.delta = delta;\n"
" if (!isfinite(delta)) {\n"
"  atomic_store_explicit(&s.failed, 1u, memory_order_relaxed); s.done = 1;\n"
" }\n"
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
    sixel_km_metal_initialized = 1;
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
    char const *phase_names[9] = { "prepare", "allocate", "upload",
        "encode", "submit", "wait", "validate", "publish", "release" };
    char const *stage_names[4] = { "assign", "partial", "reduce", "update" };
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
    lengths[SIXEL_KM_PARTIALS] = (NSUInteger)params.groups * params.centers *
        4u * sizeof(float);
    lengths[SIXEL_KM_SUMS] = (NSUInteger)params.centers * 4u * sizeof(float);
    lengths[SIXEL_KM_STATE] = sizeof(*state);
    counts[0] = params.points;
    counts[1] = (NSUInteger)params.groups * params.centers;
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
            values[index] = request->weights == NULL ? 1.0f :
                (float)request->weights[index];
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
            if (command == nil) goto cleanup;
        }
        if (trace) phase_ms[3] += (sixel_timer_now() - mark) * 1000.0;
        for (iteration = 0u; iteration < request->max_iterations;
                ++iteration) {
            for (stage = 0u; stage < 4u; ++stage) {
                command_index = iteration * 4u + stage;
                mark = trace ? sixel_timer_now() : 0.0;
                if (split) {
                    command = [sixel_km_metal_queue commandBuffer];
                    if (command == nil) goto cleanup;
                }
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
            if (trace) phase_ms[4] = (sixel_timer_now() - mark) * 1000.0;
        }
        mark = trace ? sixel_timer_now() : 0.0;
        [command waitUntilCompleted];
        completed = 1;
        if (trace) phase_ms[5] = (sixel_timer_now() - mark) * 1000.0;
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
            margin = fmax(maximum[channel] - minimum[channel],
                fmax(fabs(minimum[channel]), fabs(maximum[channel]))) *
                0.0001;
            if (request->round_centers) margin += 0.5;
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
            memcpy(request->membership,
                [buffers[SIXEL_KM_MEMBERSHIP] contents],
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
            if (trace) phase_ms[5] += (sixel_timer_now() - mark) * 1000.0;
        }
        mark = trace ? sixel_timer_now() : 0.0;
        for (index = 0u; index < SIXEL_KM_BUFFERS; ++index) {
            [buffers[index] release];
        }
        if (trace) {
            phase_ms[8] = (sixel_timer_now() - mark) * 1000.0;
            total_ms = (sixel_timer_now() - start) * 1000.0;
            fprintf(stderr, "LSXKMTIME1|mode=%s|M=%u|K=%u|limit=%u|"
                "iterations=%u|repairs=%u|commands=%u|status=0x%04x|"
                "wall_ms=%.6f|device=%s|\n",
                split ? "stages" : "batch", params.points, params.centers,
                request->max_iterations, finished_iterations, repairs,
                command_count, (unsigned int)status, total_ms,
                [[sixel_km_metal_device name] UTF8String] ?: "unavailable");
            for (index = 0u; index < 9u; ++index) {
                fprintf(stderr, "LSXKMPHASE1|phase=%s|cpu_ms=%.6f|\n",
                    phase_names[index], phase_ms[index]);
            }
            for (index = 0u; index < command_count; ++index) {
                /* GPU timestamps are valid only after completion. Zero
                 * means unavailable, not a zero-cost GPU dispatch. */
                gpu_start = [commands[index] GPUStartTime];
                gpu_end = [commands[index] GPUEndTime];
                fprintf(stderr, "LSXKMSTAGE1|mode=%s|phase=%s|iteration=%u|"
                    "noop=%d|gpu_valid=%d|gpu_start_s=%.9f|gpu_end_s=%.9f|"
                    "gpu_ms=%.6f|encode_ms=%.6f|submit_ms=%.6f|\n",
                    split ? "stages" : "batch",
                    split ? stage_names[index % 4u] : "batch",
                    split ? (unsigned int)(index / 4u + 1u) : 0u,
                    split && index / 4u >= finished_iterations,
                    gpu_start > 0.0 && gpu_end >= gpu_start,
                    gpu_start, gpu_end,
                    gpu_start > 0.0 && gpu_end >= gpu_start
                        ? (gpu_end - gpu_start) * 1000.0 : -1.0,
                    split ? encode_ms[index] : phase_ms[3],
                    split ? submit_ms[index] : phase_ms[4]);
            }
        }
    }
    return status;
}
#endif /* HAVE_METAL */
