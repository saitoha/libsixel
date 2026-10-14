/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_METAL)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <float.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "progressive-gpu.h"

/* Pipelines may be shared, but no surface image or speculative result is
 * process-global. The context owner serializes calls on its own cache. */
static pthread_mutex_t sixel_repair_metal_lock = PTHREAD_MUTEX_INITIALIZER;
static id<MTLDevice> sixel_repair_metal_device = nil;
static id<MTLCommandQueue> sixel_repair_metal_queue = nil;
static id<MTLComputePipelineState> sixel_repair_metal_scan_pipeline = nil;
static id<MTLComputePipelineState> sixel_repair_metal_reduce_pipeline = nil;
static id<MTLComputePipelineState> sixel_repair_metal_sample_pipeline = nil;
static id<MTLComputePipelineState> sixel_repair_metal_lookup_pipeline = nil;
static int sixel_repair_metal_ready = 0;
static SIXELSTATUS sixel_repair_metal_failure = SIXEL_FEATURE_ERROR;

enum sixel_repair_metal_buffer {
    REPAIR_GPU_SOURCE, REPAIR_GPU_HISTORY, REPAIR_GPU_DOMAIN,
    REPAIR_GPU_VALID, REPAIR_GPU_POSITIONS, REPAIR_GPU_ERRORS,
    REPAIR_GPU_FLAGS, REPAIR_GPU_GROUPS, REPAIR_GPU_SUMMARY,
    REPAIR_GPU_SAMPLES, REPAIR_GPU_FILTER, REPAIR_GPU_SCAN_BUFFERS,
    REPAIR_GPU_QUERIES = REPAIR_GPU_SCAN_BUFFERS,
    REPAIR_GPU_PALETTE, REPAIR_GPU_MATCHES, REPAIR_GPU_BUFFERS
};

typedef struct sixel_repair_metal_cache {
    id<MTLBuffer> buffers[REPAIR_GPU_BUFFERS];
    sixel_repair_gpu_view_t view;
    int bound;
} sixel_repair_metal_cache_t;

typedef struct sixel_repair_metal_params {
    uint64_t first;
    uint32_t count;
    uint32_t sparse;
    uint32_t width;
    uint32_t plane_width;
    uint32_t origin_x;
    uint32_t origin_y;
    float stop;
    float threshold;
    uint32_t sample_limit;
    uint32_t filtered;
    uint64_t sample_population;
    uint64_t sample_offset;
    uint32_t sample_total;
    uint32_t sample_first;
    uint32_t preselected;
    uint32_t reserved;
} sixel_repair_metal_params_t;

/* The shared structs contain only explicitly sized scalars. packed_float3
 * reads the CPU's contiguous three-float representation without a 16-byte
 * float3 stride. Disable fast math when compiling these proposal kernels. */
static char const * const sixel_repair_metal_source[] = {
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct Params {\n"
" ulong first; uint count, sparse, width, plane_width, origin_x, origin_y;\n"
" float stop, threshold; uint sample_limit, filtered;\n"
" ulong sample_population, sample_offset;\n"
" uint sample_total, sample_first, preselected, reserved;\n"
"};\n"
"struct Summary {\n"
" uint domain_count, valid_count, invalid_count, eligible_count, nonfinite;\n"
" float maximum, sum; uint sample_count; ulong maximum_position;\n"
"};\n"
"struct Sample {\n"
" ulong position, plane_position; float x, y, z, error;\n"
" ulong population;\n"
"};\n"
"static ulong position_at(constant Params &p,\n"
" device const ulong *positions, uint index) {\n"
" return p.sparse ? positions[index] : p.first + index;\n"
"}\n"
"static ulong stratum(constant Params &p, uint i) {\n"
" ulong n = p.sample_total, total = p.sample_population;\n"
" return i * (total / n) + (ulong(i) * (total % n)) / n;\n"
"}\n"
"static ulong sample_rank(constant Params &p, uint i) {\n"
" ulong begin = stratum(p, i), end = stratum(p, i + 1);\n"
" return begin + (end - begin - 1) / 2;\n"
"}\n"
"kernel void repair_scan(\n"
" device const packed_float3 *source [[buffer(0)]],\n"
" device const packed_float3 *history [[buffer(1)]],\n"
" device const uchar *domain [[buffer(2)]],\n"
" device const uchar *valid [[buffer(3)]],\n"
" device const ulong *positions [[buffer(4)]],\n"
" device float *errors [[buffer(5)]],\n"
" device uint *flags [[buffer(6)]],\n"
" device Summary *groups [[buffer(7)]],\n"
" device const uchar *filter [[buffer(10)]],\n"
" constant Params &p [[buffer(11)]],\n"
" uint gid [[thread_position_in_grid]],\n"
" uint tid [[thread_index_in_threadgroup]],\n"
" uint group [[threadgroup_position_in_grid]]) {\n"
" threadgroup float local_errors[256];\n"
" threadgroup uint local_flags[256];\n"
" float error = 0; uint flag = 0;\n"
" if (gid < p.count) {\n"
"  ulong position = position_at(p, positions, gid);\n"
"  if (domain[position]) {\n"
"   flag = 1;\n"
"   if (valid[position]) {\n"
"    float3 a = float3(source[position]);\n"
"    float3 b = float3(history[position]);\n"
"    float3 d = a - b;\n"
"    error = d.x * d.x + d.y * d.y + d.z * d.z;\n"
"    flag |= 2;\n"
"    if (!all(isfinite(a)) || !all(isfinite(b)) || !isfinite(error)) {\n"
"     flag |= 8; error = 0;\n"
"    } else if ((!p.filtered || filter[gid]) &&\n"
"        (p.preselected || (error > p.stop && error >= p.threshold))) {\n"
"     flag |= 4;\n"
"    }\n"
"   }\n"
"  }\n"
"  errors[gid] = error; flags[gid] = flag;\n"
" }\n"
" local_errors[tid] = error; local_flags[tid] = flag;\n"
" threadgroup_barrier(mem_flags::mem_threadgroup);\n"
" if (tid == 0) {\n"
"  Summary s = {};\n"
"  for (uint i = 0; i < 256; ++i) {\n"
"   uint f = local_flags[i]; float e = local_errors[i];\n"
"   s.domain_count += (f & 1) != 0;\n"
"   s.invalid_count += (f & 1) && !(f & 2);\n"
"   s.eligible_count += (f & 4) != 0; s.nonfinite |= f & 8;\n"
"   if (f & 2) {\n"
"    if (!s.valid_count || e > s.maximum) {\n"
"     s.maximum = e;\n"
"     s.maximum_position = position_at(p, positions, group * 256 + i);\n"
"    }\n"
"    ++s.valid_count; s.sum += e;\n"
"   }\n"
"  }\n"
"  if (!isfinite(s.sum)) s.nonfinite = 1;\n"
"  groups[group] = s;\n"
" }\n"
"}\n",
"kernel void repair_reduce(\n"
" device const Summary *groups [[buffer(7)]],\n"
" device Summary *result [[buffer(8)]],\n"
" constant Params &p [[buffer(11)]]) {\n"
" Summary s = {};\n"
" for (uint i = 0; i < (p.count + 255) / 256; ++i) {\n"
"  Summary g = groups[i];\n"
"  if (g.valid_count && (!s.valid_count || g.maximum > s.maximum)) {\n"
"   s.maximum = g.maximum; s.maximum_position = g.maximum_position;\n"
"  }\n"
"  s.domain_count += g.domain_count; s.valid_count += g.valid_count;\n"
"  s.invalid_count += g.invalid_count;\n"
"  s.eligible_count += g.eligible_count; s.sum += g.sum;\n"
"  s.nonfinite |= g.nonfinite;\n"
" }\n"
" if (!isfinite(s.sum)) s.nonfinite = 1;\n"
" s.sample_count = min(p.sample_limit, s.eligible_count);\n"
" if (p.sample_total) {\n"
"  s.sample_count = 0;\n"
"  while (p.sample_first + s.sample_count < p.sample_total &&\n"
"      s.sample_count < p.sample_limit) {\n"
"   ulong rank = sample_rank(p, p.sample_first + s.sample_count);\n"
"   if (rank >= p.sample_offset + s.eligible_count) break;\n"
"   if (rank < p.sample_offset) { s.nonfinite = 1; break; }\n"
"   ++s.sample_count;\n"
"  }\n"
" }\n"
" result[0] = s;\n"
"}\n"
"kernel void repair_sample(\n"
" device const packed_float3 *source [[buffer(0)]],\n"
" device const ulong *positions [[buffer(4)]],\n"
" device const float *errors [[buffer(5)]],\n"
" device const uint *flags [[buffer(6)]],\n"
" device const Summary *groups [[buffer(7)]],\n"
" device const Summary *result [[buffer(8)]],\n"
" device Sample *samples [[buffer(9)]],\n"
" constant Params &p [[buffer(11)]], uint gid [[thread_position_in_grid]]) {\n"
" uint n = result[0].sample_count;\n"
" if (gid >= n || result[0].nonfinite) return;\n"
" uint population = result[0].eligible_count;\n"
" uint begin = gid * population / n;\n"
" uint end = (gid + 1) * population / n;\n"
" uint ordinal = begin + (end - begin) / 2;\n"
" ulong weight = end - begin;\n"
" if (p.sample_total) {\n"
"  uint index = p.sample_first + gid;\n"
"  ordinal = uint(sample_rank(p, index) - p.sample_offset);\n"
"  weight = stratum(p, index + 1) - stratum(p, index);\n"
" }\n"
" uint group = 0;\n"
" while (ordinal >= groups[group].eligible_count) {\n"
"  ordinal -= groups[group].eligible_count; ++group;\n"
" }\n"
" for (uint i = group * 256; i < min(p.count, (group + 1) * 256); ++i) {\n"
"  if (!(flags[i] & 4)) continue;\n"
"  if (ordinal) { --ordinal; continue; }\n"
"  ulong position = position_at(p, positions, i);\n"
"  float3 color = float3(source[position]);\n"
"  Sample s; s.position = position;\n"
"  s.plane_position = (ulong(p.origin_y) + position / p.width) *\n"
"      p.plane_width + p.origin_x + position % p.width;\n"
"  s.x = color.x; s.y = color.y; s.z = color.z; s.error = errors[i];\n"
"  s.population = weight; samples[gid] = s;\n"
"  break;\n"
" }\n"
"}\n",
"struct Lookup { uint count, colors; };\n"
"struct Match { uint index; float distance; };\n"
"kernel void repair_lookup(\n"
" device const packed_float3 *queries [[buffer(0)]],\n"
" device const packed_float3 *palette [[buffer(1)]],\n"
" device Match *matches [[buffer(2)]],\n"
" constant Lookup &p [[buffer(3)]],\n"
" uint gid [[thread_position_in_grid]]) {\n"
" if (gid >= p.count) return;\n"
" float3 q = float3(queries[gid]);\n"
" Match best; best.index = 0; best.distance = INFINITY;\n"
" bool invalid = !all(isfinite(q));\n"
" for (uint i = 0; i < p.colors; ++i) {\n"
"  float3 c = float3(palette[i]); float3 d = q - c;\n"
"  float e = d.x * d.x + d.y * d.y + d.z * d.z;\n"
"  invalid |= !all(isfinite(c)) || !isfinite(e);\n"
"  if (e < best.distance) { best.index = i; best.distance = e; }\n"
" }\n"
" if (invalid) best.index = 0xffffffffu;\n"
" matches[gid] = best;\n"
"}\n"
};

static SIXELSTATUS
sixel_repair_metal_prepare(void)
{
    NSMutableString *source;
    MTLCompileOptions *options;
    NSError *error;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    size_t i;
    SIXELSTATUS status;

    source = nil;
    options = nil;
    library = nil;
    function = nil;
    error = nil;
    (void)pthread_mutex_lock(&sixel_repair_metal_lock);
    if (sixel_repair_metal_ready != 0) goto end;
    sixel_repair_metal_ready = -1;
    sixel_repair_metal_device = MTLCreateSystemDefaultDevice();
    if (sixel_repair_metal_device == nil) goto end;
    sixel_repair_metal_failure = SIXEL_BAD_ALLOCATION;
    sixel_repair_metal_queue = [sixel_repair_metal_device newCommandQueue];
    if (sixel_repair_metal_queue == nil) goto end;
    source = [[NSMutableString alloc] init];
    if (source == nil) goto end;
    for (i = 0; i < sizeof(sixel_repair_metal_source) /
            sizeof(sixel_repair_metal_source[0]); ++i) {
        [source appendString:[NSString
            stringWithUTF8String:sixel_repair_metal_source[i]]];
    }
    options = [[MTLCompileOptions alloc] init];
    if (options == nil) goto end;
    /* Keep finite checks valid with the API shared by supported SDKs. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [options setFastMathEnabled:NO];
#pragma clang diagnostic pop
    sixel_repair_metal_failure = SIXEL_RUNTIME_ERROR;
    library = [sixel_repair_metal_device newLibraryWithSource:source
        options:options error:&error];
    if (library == nil) goto end;
    function = [library newFunctionWithName:@"repair_scan"];
    if (function == nil) goto end;
    sixel_repair_metal_scan_pipeline = [sixel_repair_metal_device
        newComputePipelineStateWithFunction:function error:&error];
    [function release];
    function = [library newFunctionWithName:@"repair_reduce"];
    if (function == nil) goto end;
    sixel_repair_metal_reduce_pipeline = [sixel_repair_metal_device
        newComputePipelineStateWithFunction:function error:&error];
    [function release];
    function = [library newFunctionWithName:@"repair_sample"];
    if (function == nil) goto end;
    sixel_repair_metal_sample_pipeline = [sixel_repair_metal_device
        newComputePipelineStateWithFunction:function error:&error];
    [function release];
    function = [library newFunctionWithName:@"repair_lookup"];
    if (function == nil) goto end;
    sixel_repair_metal_lookup_pipeline = [sixel_repair_metal_device
        newComputePipelineStateWithFunction:function error:&error];
    if (sixel_repair_metal_scan_pipeline != nil &&
            sixel_repair_metal_reduce_pipeline != nil &&
            sixel_repair_metal_sample_pipeline != nil &&
            sixel_repair_metal_lookup_pipeline != nil &&
            [sixel_repair_metal_scan_pipeline
                maxTotalThreadsPerThreadgroup] >= 256) {
        sixel_repair_metal_ready = 1;
    }
end:
    [function release];
    [library release];
    [options release];
    [source release];
    status = sixel_repair_metal_ready == 1 ? SIXEL_OK
        : sixel_repair_metal_failure;
    (void)pthread_mutex_unlock(&sixel_repair_metal_lock);
    return status;
}

void
sixel_repair_gpu_metal_free(void *backend)
{
    sixel_repair_metal_cache_t *cache;
    unsigned int i;

    cache = backend;
    if (cache == NULL) return;
    for (i = 0; i < REPAIR_GPU_BUFFERS; ++i) [cache->buffers[i] release];
    free(cache);
}

static int
sixel_repair_metal_buffer(sixel_repair_metal_cache_t *cache,
    unsigned int slot, size_t bytes)
{
    id<MTLBuffer> replacement;

    if (cache->buffers[slot] != nil &&
            [cache->buffers[slot] length] >= bytes) return 1;
    replacement = [sixel_repair_metal_device newBufferWithLength:bytes
        options:MTLResourceStorageModeShared];
    if (replacement == nil) return 0;
    [cache->buffers[slot] release];
    cache->buffers[slot] = replacement;
    return 1;
}

static void
sixel_repair_metal_upload(sixel_repair_metal_cache_t *cache,
    unsigned int slot, void const *data, size_t bytes,
    sixel_repair_gpu_result_t *result)
{
    memcpy([cache->buffers[slot] contents], data, bytes);
    result->uploaded_bytes += bytes;
}

SIXELSTATUS
sixel_repair_gpu_metal_scan(void **backend,
    sixel_repair_gpu_request_t const *request,
    sixel_repair_gpu_result_t *result)
{
    sixel_repair_metal_cache_t *cache;
    sixel_repair_gpu_view_t const *view;
    sixel_repair_gpu_view_t const *old;
    sixel_repair_metal_params_t params;
    id<MTLCommandBuffer> command;
    id<MTLComputeCommandEncoder> encoder;
    id<MTLComputePipelineState> pipeline;
    MTLSize grid;
    MTLSize group;
    size_t sizes[REPAIR_GPU_BUFFERS];
    size_t groups;
    unsigned int i;
    unsigned int stage;
    int changed;
    SIXELSTATUS status;

    status = SIXEL_FEATURE_ERROR;
    @autoreleasepool {
        status = sixel_repair_metal_prepare();
        if (status != SIXEL_OK) return status;
        cache = *backend;
        if (cache == NULL) {
            cache = calloc(1, sizeof(*cache));
            if (cache == NULL) return SIXEL_BAD_ALLOCATION;
            *backend = cache;
        }
        view = &request->view;
        old = &cache->view;
        groups = (request->count + 255) / 256;
        sizes[REPAIR_GPU_SOURCE] = sizes[REPAIR_GPU_HISTORY] =
            view->count * 3 * sizeof(float);
        sizes[REPAIR_GPU_DOMAIN] = sizes[REPAIR_GPU_VALID] = view->count;
        sizes[REPAIR_GPU_POSITIONS] = request->count * sizeof(uint64_t);
        sizes[REPAIR_GPU_ERRORS] = request->count * sizeof(float);
        sizes[REPAIR_GPU_FLAGS] = request->count * sizeof(uint32_t);
        sizes[REPAIR_GPU_GROUPS] = groups *
            sizeof(sixel_repair_gpu_summary_t);
        sizes[REPAIR_GPU_SUMMARY] = sizeof(sixel_repair_gpu_summary_t);
        sizes[REPAIR_GPU_SAMPLES] = sizeof(result->samples);
        sizes[REPAIR_GPU_FILTER] = request->count;
        for (i = 0; i < REPAIR_GPU_SCAN_BUFFERS; ++i) {
            if (!sixel_repair_metal_buffer(cache, i, sizes[i])) {
                return SIXEL_BAD_ALLOCATION;
            }
        }
        for (i = 0; i < REPAIR_GPU_BUFFERS; ++i) {
            if ([cache->buffers[i] length] >
                    SIZE_MAX - result->resident_bytes) {
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
            result->resident_bytes += [cache->buffers[i] length];
        }
        changed = !cache->bound || old->count != view->count ||
            old->width != view->width || old->height != view->height ||
            old->plane_width != view->plane_width ||
            old->plane_height != view->plane_height ||
            old->origin_x != view->origin_x ||
            old->origin_y != view->origin_y ||
            old->workspace != view->workspace ||
            old->configuration_revision != view->configuration_revision;
        if (changed || old->source_generation != view->source_generation ||
                old->source != view->source || old->domain != view->domain) {
            sixel_repair_metal_upload(cache, REPAIR_GPU_SOURCE, view->source,
                sizes[REPAIR_GPU_SOURCE], result);
            sixel_repair_metal_upload(cache, REPAIR_GPU_DOMAIN, view->domain,
                sizes[REPAIR_GPU_DOMAIN], result);
        }
        if (changed || old->history_revision != view->history_revision ||
                old->retained != view->retained || old->valid != view->valid) {
            sixel_repair_metal_upload(cache, REPAIR_GPU_HISTORY,
                view->retained, sizes[REPAIR_GPU_HISTORY], result);
            sixel_repair_metal_upload(cache, REPAIR_GPU_VALID, view->valid,
                sizes[REPAIR_GPU_VALID], result);
        }
        if (request->positions != NULL) {
            sixel_repair_metal_upload(cache, REPAIR_GPU_POSITIONS,
                request->positions, sizes[REPAIR_GPU_POSITIONS], result);
        }
        if (request->filter != NULL) {
            sixel_repair_metal_upload(cache, REPAIR_GPU_FILTER,
                request->filter, sizes[REPAIR_GPU_FILTER], result);
        }
        memset(&params, 0, sizeof(params));
        params.first = request->first;
        params.count = (uint32_t)request->count;
        params.sparse = request->positions != NULL;
        params.width = (uint32_t)view->width;
        params.plane_width = (uint32_t)view->plane_width;
        params.origin_x = (uint32_t)view->origin_x;
        params.origin_y = (uint32_t)view->origin_y;
        params.stop = request->stop_squared > FLT_MAX ? INFINITY
            : (float)request->stop_squared;
        params.threshold = request->selection_squared > FLT_MAX ? INFINITY
            : (float)request->selection_squared;
        params.sample_limit = request->sample_limit;
        params.filtered = request->filter != NULL;
        params.preselected = request->preselected;
        params.sample_population = request->sample_population;
        params.sample_offset = request->sample_offset;
        params.sample_total = request->sample_total;
        params.sample_first = request->sample_first;
        command = [sixel_repair_metal_queue commandBuffer];
        if (command == nil) return SIXEL_BAD_ALLOCATION;
        /* Separate encoders order resource hazards between scan, reduction
         * and sampling. Read results only after command completion. */
        for (stage = 0; stage < 3; ++stage) {
            pipeline = stage == 0 ? sixel_repair_metal_scan_pipeline :
                stage == 1 ? sixel_repair_metal_reduce_pipeline :
                sixel_repair_metal_sample_pipeline;
            encoder = [command computeCommandEncoder];
            if (encoder == nil) return SIXEL_BAD_ALLOCATION;
            [encoder setComputePipelineState:pipeline];
            for (i = 0; i < REPAIR_GPU_SCAN_BUFFERS; ++i) {
                [encoder setBuffer:cache->buffers[i] offset:0 atIndex:i];
            }
            [encoder setBytes:&params length:sizeof(params) atIndex:11];
            grid = MTLSizeMake(stage == 0 ? groups : 1, 1, 1);
            group = MTLSizeMake(stage == 0 ? 256 : stage == 1 ? 1 : 128,
                1, 1);
            [encoder dispatchThreadgroups:grid threadsPerThreadgroup:group];
            [encoder endEncoding];
        }
        [command commit];
        /* Keep the owner's operation gate until device reads and writes
         * finish, including failures. A pending update/rebind/destruction
         * may replace these buffers immediately after the gate opens. */
        [command waitUntilCompleted];
        if ([command status] != MTLCommandBufferStatusCompleted) {
            return SIXEL_RUNTIME_ERROR;
        }
        memcpy(&result->summary,
            [cache->buffers[REPAIR_GPU_SUMMARY] contents],
            sizeof(result->summary));
        if (result->summary.nonfinite) return SIXEL_BAD_INPUT;
        if (result->summary.sample_count > request->sample_limit ||
                result->summary.eligible_count > request->count) {
            return SIXEL_LOGIC_ERROR;
        }
        memcpy(result->samples, [cache->buffers[REPAIR_GPU_SAMPLES] contents],
            result->summary.sample_count * sizeof(result->samples[0]));
        result->downloaded_bytes = sizeof(result->summary) +
            result->summary.sample_count * sizeof(result->samples[0]);
        cache->view = *view;
        cache->bound = 1;
        status = SIXEL_OK;
    }
    return status;
}

SIXELSTATUS
sixel_repair_gpu_metal_lookup(void **backend,
    sixel_repair_gpu_lookup_t const *request,
    sixel_repair_gpu_match_t *matches)
{
    sixel_repair_metal_cache_t *cache;
    sixel_repair_gpu_match_t const *result;
    id<MTLCommandBuffer> command;
    id<MTLComputeCommandEncoder> encoder;
    uint32_t params[2];
    size_t sizes[3];
    size_t i;
    size_t threads;
    SIXELSTATUS status;

    @autoreleasepool {
        status = sixel_repair_metal_prepare();
        if (status != SIXEL_OK) return status;
        cache = *backend;
        if (cache == NULL) {
            cache = calloc(1, sizeof(*cache));
            if (cache == NULL) return SIXEL_BAD_ALLOCATION;
            *backend = cache;
        }
        sizes[0] = request->count * 3 * sizeof(float);
        sizes[1] = request->colors * 3 * sizeof(float);
        sizes[2] = request->count * sizeof(*matches);
        for (i = 0; i < 3; ++i) {
            if (!sixel_repair_metal_buffer(cache, REPAIR_GPU_QUERIES + i,
                    sizes[i])) return SIXEL_BAD_ALLOCATION;
        }
        /* These scratch buffers are overwritten on every dispatch. They
         * cannot reuse queries from another source, palette or noise phase. */
        memcpy([cache->buffers[REPAIR_GPU_QUERIES] contents],
            request->queries, sizes[0]);
        memcpy([cache->buffers[REPAIR_GPU_PALETTE] contents],
            request->palette, sizes[1]);
        params[0] = (uint32_t)request->count;
        params[1] = request->colors;
        command = [sixel_repair_metal_queue commandBuffer];
        if (command == nil) return SIXEL_BAD_ALLOCATION;
        encoder = [command computeCommandEncoder];
        if (encoder == nil) return SIXEL_BAD_ALLOCATION;
        [encoder setComputePipelineState:sixel_repair_metal_lookup_pipeline];
        for (i = 0; i < 3; ++i) {
            [encoder setBuffer:cache->buffers[REPAIR_GPU_QUERIES + i]
                offset:0 atIndex:i];
        }
        [encoder setBytes:params length:sizeof(params) atIndex:3];
        threads = [sixel_repair_metal_lookup_pipeline threadExecutionWidth];
        [encoder dispatchThreadgroups:
            MTLSizeMake((request->count + threads - 1) / threads, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(threads, 1, 1)];
        [encoder endEncoding];
        [command commit];
        /* The owner may reuse lookup scratch or evict a failed cache as
         * soon as we return. No submitted command may outlive this call. */
        [command waitUntilCompleted];
        if ([command status] != MTLCommandBufferStatusCompleted) {
            return SIXEL_RUNTIME_ERROR;
        }
        result = [cache->buffers[REPAIR_GPU_MATCHES] contents];
        for (i = 0; i < request->count; ++i) {
            if (result[i].index >= request->colors ||
                    !isfinite(result[i].distance) || result[i].distance < 0) {
                return SIXEL_BAD_INPUT;
            }
        }
        /* Publish only a completely validated batch. AUTO can retry the
         * same CPU cursor after failure without consuming partial results. */
        memcpy(matches, result, sizes[2]);
    }
    return SIXEL_OK;
}
#endif
