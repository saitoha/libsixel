/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_METAL)
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include "dither-policy-jajuni-gpu.h"
#include "gpu-runtime-metal.h"
#include "encode-result.h"
#include "palette.h"
#include "sixel_atomic.h"
#include "delta-residual.h"
#include "bluenoise_64x64.h"
#include "gpu-statistics.h"

static char const *const jajuni_gpu_chunks[] = {
#include <generated-metal/dither-policy-jajuni-gpu-shader.inc.h>
};

typedef struct jajuni_gpu_params {
    uint32_t width, height, ncolors, step;
    uint32_t tiles_x, keycolor, delta, threshold;
    uint32_t metric, columns, rows, tiled, selected_threshold;
    uint32_t selection[128];
    uint32_t batch_stage;
    uint32_t serpentine, clamp_hard;
    float deadzone, clamp_lower, clamp_upper;
} jajuni_gpu_params_t;

typedef struct jajuni_gpu_plan {
    sixel_gpu_dither_plan_interface_t iface;
    sixel_gpu_dither_deferred_interface_t deferred;
    sixel_gpu_dither_solution_interface_t solution;
    sixel_atomic_u32_t solution_ref;
    sixel_atomic_u32_t consumed;
    sixel_atomic_u32_t ref;
    sixel_atomic_u32_t busy;
    sixel_allocator_t *allocator;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_lookup_plan_interface_t *lookup;
    sixel_gpu_lookup_program_view_t view;
    sixel_indexed_frame_description_t output;
    id<MTLCommandQueue> queue;
    NSArray *pipelines;
    id<MTLBuffer> buffers[18];
    jajuni_gpu_params_t params;
} jajuni_gpu_plan_t;

/* Idle storage is local to this dither, never shared by live plans. Reuse
 * avoids the first-touch cost of two large state planes on every frame.
 * Two bundles, each at most 128 MiB, bound retained memory. Initialization
 * rewrites every readable state value before use; no error crosses images. */
typedef struct jajuni_gpu_idle {
    id<MTLCommandQueue> queue;
    id<MTLBuffer> buffers[18];
} jajuni_gpu_idle_t;

static pthread_mutex_t jajuni_gpu_scratch_gate = PTHREAD_MUTEX_INITIALIZER;
static jajuni_gpu_idle_t jajuni_gpu_idle[2];
static unsigned int jajuni_gpu_idle_next;

static void jajuni_gpu_recycle(jajuni_gpu_plan_t *plan)
{
    jajuni_gpu_idle_t old;
    size_t i, bytes;
    unsigned int slot;

    bytes = 0;
    for (i = 0; i < 18; ++i)
        bytes += [plan->buffers[i] length];
    if (plan->queue == nil || bytes > 128U * 1024U * 1024U)
        return;
    (void)pthread_mutex_lock(&jajuni_gpu_scratch_gate);
    slot = jajuni_gpu_idle_next++ % 2;
    old = jajuni_gpu_idle[slot];
    jajuni_gpu_idle[slot].queue = plan->queue;
    plan->queue = nil;
    for (i = 0; i < 18; ++i) {
        jajuni_gpu_idle[slot].buffers[i] = plan->buffers[i];
        plan->buffers[i] = nil;
    }
    (void)pthread_mutex_unlock(&jajuni_gpu_scratch_gate);
    for (i = 0; i < 18; ++i)
        [old.buffers[i] release];
    [old.queue release];
}

static void jajuni_gpu_borrow(jajuni_gpu_plan_t *plan, id<MTLDevice> device,
                              size_t const *lengths)
{
    size_t i, slot;
    int fits;

    (void)pthread_mutex_lock(&jajuni_gpu_scratch_gate);
    for (slot = 0; slot < 2; ++slot) {
        fits = jajuni_gpu_idle[slot].queue != nil &&
               [jajuni_gpu_idle[slot].queue device] == device;
        for (i = 0; fits && i < 18; ++i)
            fits = [jajuni_gpu_idle[slot].buffers[i] length] >= lengths[i];
        if (!fits)
            continue;
        plan->queue = jajuni_gpu_idle[slot].queue;
        jajuni_gpu_idle[slot].queue = nil;
        for (i = 0; i < 18; ++i) {
            plan->buffers[i] = jajuni_gpu_idle[slot].buffers[i];
            jajuni_gpu_idle[slot].buffers[i] = nil;
        }
        break;
    }
    (void)pthread_mutex_unlock(&jajuni_gpu_scratch_gate);
}

/* Code is cached independently of palettes and scratch. A bounded cache
 * avoids recompiling the same injected lookup on every video frame. Live
 * plans retain their pipeline array even when a cache entry is evicted. */
static pthread_mutex_t jajuni_gpu_code_gate = PTHREAD_MUTEX_INITIALIZER;
static NSMutableDictionary *jajuni_gpu_code_cache;

static SIXELSTATUS
jajuni_gpu_compile(id<MTLDevice> device,
                   sixel_gpu_lookup_program_view_t const *view,
                   NSArray **result)
{
    NSMutableString *source, *key;
    NSMutableArray *pipelines;
    NSString *entry;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    id<MTLComputePipelineState> pipeline;
    MTLCompileOptions *options;
    NSError *error;
    MTLSize limits;
    size_t i, j, threads;
    SIXELSTATUS status;

    *result = nil;
    source = [NSMutableString string];
    key = [NSMutableString string];
    pipelines = nil;
    library = nil;
    options = nil;
    error = nil;
    status = SIXEL_FEATURE_ERROR;
    [source appendFormat:@"#define JAJUNI_GPU_BATCH %d\n", view->batch != NULL];
    if (view->batch != NULL &&
        (view->batch->entry_point == NULL || view->batch->tile[0] == 0 ||
         view->batch->tile[1] == 0 || view->batch->tile[0] > UINT32_MAX ||
         view->batch->tile[1] > UINT32_MAX))
        return SIXEL_BAD_ARGUMENT;
    for (i = 0; i < view->chunk_count; ++i) {
        if (view->chunks[i] == NULL)
            return SIXEL_BAD_ARGUMENT;
        [source appendString:@(view->chunks[i])];
    }
    for (i = 0; i < sizeof(jajuni_gpu_chunks) / sizeof(*jajuni_gpu_chunks);
         ++i) {
        [source appendString:@(jajuni_gpu_chunks[i])];
    }
    [key appendFormat:@"%p:%@", (void *)device, source];
    for (i = 0; i < view->build_pass_count; ++i) {
        if (view->build_passes[i].entry_point == NULL)
            return SIXEL_BAD_ARGUMENT;
        [key appendFormat:@"|%s", view->build_passes[i].entry_point];
    }
    if (view->batch != NULL)
        [key appendFormat:@"|batch:%s", view->batch->entry_point];
    (void)pthread_mutex_lock(&jajuni_gpu_code_gate);
    if (jajuni_gpu_code_cache == nil) {
        jajuni_gpu_code_cache = [[NSMutableDictionary alloc] init];
    }
    pipelines = jajuni_gpu_code_cache[key];
    if (pipelines == nil) {
        options = [[MTLCompileOptions alloc] init];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        options.fastMathEnabled = NO;
#pragma clang diagnostic pop
        library = [device newLibraryWithSource:source
                                       options:options
                                         error:&error];
        if (library == nil) {
            sixel_helper_set_additional_message(
                error != nil ? [[error localizedDescription] UTF8String]
                             : "jajuni gpu: shader compilation failed");
            goto end;
        }
        pipelines = [NSMutableArray array];
        for (i = 0; i < view->build_pass_count + 5 + (view->batch != NULL);
             ++i) {
            entry = i == view->build_pass_count + 5
                        ? @(view->batch->entry_point)
                    : i < view->build_pass_count
                        ? @(view->build_passes[i].entry_point)
                        : @[
                              @"jajuni_gpu_initialize", @"jajuni_gpu_iterate",
                              @"jajuni_gpu_finish", @"jajuni_gpu_solve_tail",
                              @"jajuni_gpu_resolve"
                          ][i - view->build_pass_count];
            function = [library newFunctionWithName:entry];
            if (function == nil)
                goto end;
            pipeline = [device newComputePipelineStateWithFunction:function
                                                             error:&error];
            [function release];
            if (pipeline == nil)
                goto end;
            [pipelines addObject:pipeline];
            [pipeline release];
        }
        if ([jajuni_gpu_code_cache count] >= 8)
            [jajuni_gpu_code_cache removeAllObjects];
        jajuni_gpu_code_cache[key] = pipelines;
    }
    limits = [device maxThreadsPerThreadgroup];
    for (i = 0; i < view->build_pass_count; ++i) {
        threads = 1;
        for (j = 0; j < 3; ++j) {
            if (view->build_passes[i].threads[j] == 0 ||
                view->build_passes[i].groups[j] == 0 ||
                view->build_passes[i].threads[j] > SIZE_MAX / threads)
                goto end;
            threads *= view->build_passes[i].threads[j];
        }
        pipeline = pipelines[i];
        if (threads > [pipeline maxTotalThreadsPerThreadgroup] ||
            view->build_passes[i].threads[0] > limits.width ||
            view->build_passes[i].threads[1] > limits.height ||
            view->build_passes[i].threads[2] > limits.depth)
            goto end;
    }
    if (view->batch != NULL) {
        threads = 1;
        for (j = 0; j < 3; ++j) {
            if (view->batch->threads[j] == 0 ||
                view->batch->threads[j] > SIZE_MAX / threads)
                goto end;
            threads *= view->batch->threads[j];
        }
        pipeline = pipelines[view->build_pass_count + 5];
        if (threads > [pipeline maxTotalThreadsPerThreadgroup] ||
            view->batch->threads[0] > limits.width ||
            view->batch->threads[1] > limits.height ||
            view->batch->threads[2] > limits.depth)
            goto end;
    }
    *result = [pipelines retain];
    status = SIXEL_OK;
end:
    [options release];
    [library release];
    (void)pthread_mutex_unlock(&jajuni_gpu_code_gate);
    return status;
}

static void jajuni_gpu_plan_ref(sixel_gpu_dither_plan_interface_t *self)
{
    jajuni_gpu_plan_t *plan;

    plan = (jajuni_gpu_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void jajuni_gpu_plan_unref(sixel_gpu_dither_plan_interface_t *self)
{
    jajuni_gpu_plan_t *plan;
    sixel_allocator_t *allocator;
    size_t i;

    plan = (jajuni_gpu_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1)
        return;
    allocator = plan->allocator;
    if (plan->lookup != NULL)
        plan->lookup->vtbl->unref(plan->lookup);
    if (plan->device != NULL)
        plan->device->vtbl->unref(plan->device);
    if (plan->output.palette != NULL)
        sixel_wire_palette_unref(plan->output.palette);
    jajuni_gpu_recycle(plan);
    for (i = 0; i < 18; ++i)
        [plan->buffers[i] release];
    [plan->pipelines release];
    [plan->queue release];
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

/* Validate without touching scratch so bad arguments do not consume a
 * ready solution. The caller must keep the borrowed request stable. */
static SIXELSTATUS
jajuni_gpu_validate(jajuni_gpu_plan_t *plan,
                    sixel_gpu_dither_run_request_t const *request,
                    jajuni_gpu_params_t *resolved)
{
    jajuni_gpu_params_t params;
    sixel_retained_read_view_t baseline;
    size_t cells;
    SIXELSTATUS status;

    if (request == NULL || request->result == NULL)
        return SIXEL_BAD_ARGUMENT;
    if (sixel_indexed_frame_get_state(request->result) !=
            SIXEL_INDEXED_BUILDING ||
        request->result->description.width != plan->output.width ||
        request->result->description.height != plan->output.height ||
        request->result->description.palette != plan->output.palette ||
        request->result->description.keycolor != plan->output.keycolor ||
        request->result->description.keycolor_reserved !=
            plan->output.keycolor_reserved ||
        request->result->description.transparent_background !=
            plan->output.transparent_background ||
        request->result->description.encoded_x != plan->output.encoded_x ||
        request->result->description.encoded_y != plan->output.encoded_y ||
        request->result->description.encoded_width !=
            plan->output.encoded_width ||
        request->result->description.encoded_height !=
            plan->output.encoded_height ||
        request->result->description.origin_x != plan->output.origin_x ||
        request->result->description.origin_y != plan->output.origin_y ||
        request->result->description.plane_width != plan->output.plane_width ||
        request->result->description.plane_height != plan->output.plane_height)
        return SIXEL_BAD_ARGUMENT;
    params = plan->params;
    params.delta = request->delta.enabled && request->baseline != NULL;
    params.threshold = request->delta.threshold;
    params.metric = request->delta.metric;
    if (params.delta && (params.threshold > 255 ||
                         (params.metric != SIXEL_DELTA_METRIC_MAX &&
                          params.metric != SIXEL_DELTA_METRIC_RGB241)))
        return SIXEL_BAD_ARGUMENT;
    cells = (size_t)request->delta.selection.columns *
            request->delta.selection.rows;
    if (request->delta.selection.columns > 64 ||
        request->delta.selection.rows > 64 ||
        ((request->delta.selection.columns == 0) !=
         (request->delta.selection.rows == 0)) ||
        (cells != 0 &&
         (request->delta.selection.words == NULL ||
          request->delta.selection.word_count < (cells + 31) / 32)))
        return SIXEL_BAD_ARGUMENT;
    if (cells != 0) {
        params.columns = request->delta.selection.columns;
        params.rows = request->delta.selection.rows;
        params.tiled = request->delta.selection.tiled;
        params.selected_threshold = request->delta.selection.selected_threshold;
        memcpy(params.selection, request->delta.selection.words,
               ((cells + 31) / 32) * sizeof(uint32_t));
    }
    if ((params.columns == 0) != (params.rows == 0) ||
        (params.tiled && (params.columns != 64 || params.rows != 64)))
        return SIXEL_BAD_ARGUMENT;
    if (params.delta) {
        status = sixel_retained_read_get_view(request->baseline, &baseline);
        if (status != SIXEL_OK)
            return status;
        if (baseline.rgb == NULL || baseline.valid == NULL ||
            baseline.rgb_stride < (size_t)baseline.description.width * 3 ||
            baseline.valid_stride < (size_t)baseline.description.width ||
            request->result->identity.stream != baseline.stream ||
            request->result->identity.epoch != baseline.epoch ||
            request->result->identity.base_sequence != baseline.sequence ||
            plan->output.plane_width != baseline.description.width ||
            plan->output.plane_height != baseline.description.height ||
            plan->output.origin_x < 0 || plan->output.origin_y < 0 ||
            plan->output.origin_x > baseline.description.width ||
            plan->output.origin_y > baseline.description.height ||
            plan->output.width >
                baseline.description.width - plan->output.origin_x ||
            plan->output.height >
                baseline.description.height - plan->output.origin_y ||
            plan->output.keycolor < 0)
            return SIXEL_BAD_ARGUMENT;
    }
    *resolved = params;
    return SIXEL_OK;
}

/* A batch lookup borrows only corrected queries and its output indexes.
 * Separate encoders order query writes, collective lookup and state writes;
 * the point path continues to use a single fused dispatch per subpass. */
static SIXELSTATUS jajuni_gpu_encode_batch(jajuni_gpu_plan_t *plan,
                                           id<MTLCommandBuffer> commands,
                                           id<MTLBuffer> workspace)
{
    sixel_gpu_lookup_batch_dispatch_t const *batch;
    id<MTLComputeCommandEncoder> encoder;
    uint32_t extent[2], count;

    batch = plan->view.batch;
    count = plan->params.ncolors;
    extent[0] = plan->params.width;
    extent[1] = plan->params.height;
    encoder = [commands computeCommandEncoder];
    if (encoder == nil)
        return SIXEL_BAD_ALLOCATION;
    [encoder
        setComputePipelineState:plan->pipelines[plan->view.build_pass_count +
                                                5]];
    [encoder setBuffer:plan->buffers[1] offset:0 atIndex:0];
    [encoder setBytes:&count length:sizeof(count) atIndex:1];
    [encoder setBuffer:plan->buffers[12] offset:0 atIndex:2];
    [encoder setBuffer:workspace offset:0 atIndex:3];
    [encoder setBuffer:plan->buffers[16] offset:0 atIndex:4];
    [encoder setBuffer:plan->buffers[17] offset:0 atIndex:5];
    [encoder setBytes:extent length:sizeof(extent) atIndex:6];
    [encoder
         dispatchThreadgroups:MTLSizeMake(
                                  1 + ((size_t)extent[0] - 1) / batch->tile[0],
                                  1 + ((size_t)extent[1] - 1) / batch->tile[1],
                                  1)
        threadsPerThreadgroup:MTLSizeMake(batch->threads[0], batch->threads[1],
                                          batch->threads[2])];
    [encoder endEncoding];
    return SIXEL_OK;
}

/* Both deferred calls drain their commands. Only solve owns a lookup
 * lease; resolve uses the completed private planes and current history. */
static SIXELSTATUS
jajuni_gpu_run_impl(jajuni_gpu_plan_t *plan,
                    sixel_gpu_dither_run_request_t const *request, int mode)
{
    sixel_frame_pixels_view_t source;
    sixel_frame_transparency_t transparency;
    sixel_retained_read_view_t baseline;
    sixel_gpu_lookup_binding_interface_t *binding;
    sixel_gpu_lookup_binding_view_t binding_view;
    sixel_gpu_lookup_build_pass_t const *pass;
    id<MTLBuffer> workspace;
    id<MTLCommandBuffer> commands;
    id<MTLComputeCommandEncoder> encoder;
    id<MTLComputePipelineState> pipeline;
    jajuni_gpu_params_t params;
    size_t pixels, i, y, offset, count, threads, stage, stages;
    uint32_t ncolors, zero;
    SIXELSTATUS status, finish;

    if (request == NULL || (mode != 2 && request->source == NULL))
        return SIXEL_BAD_ARGUMENT;
    params = plan->params;
    if (mode != 1) {
        status = jajuni_gpu_validate(plan, request, &params);
        if (status != SIXEL_OK)
            return status;
    }
    if (request->cancel != NULL && request->cancel(request->cancel_context))
        return SIXEL_INTERRUPTED;
    pixels = (size_t)params.width * params.height;
    if (mode != 2) {
        status = request->source->vtbl->get_pixels(request->source, &source);
        if (status != SIXEL_OK)
            return status;
        if (source.pixels == NULL || source.width != (int)params.width ||
            source.height != (int)params.height ||
            source.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            source.colorspace != SIXEL_COLORSPACE_GAMMA)
            return SIXEL_BAD_ARGUMENT;
        status = request->source->vtbl->get_transparency(request->source,
                                                         &transparency);
        if (status != SIXEL_OK)
            return status;
        if (transparency.transparent_mask != NULL &&
            (transparency.transparent_mask_size < pixels ||
             plan->output.keycolor < 0))
            return SIXEL_BAD_ARGUMENT;
        memcpy([plan->buffers[0] contents], source.pixels, pixels * 3);
        if (transparency.transparent_mask != NULL) {
            memcpy([plan->buffers[8] contents], transparency.transparent_mask,
                   pixels);
        } else {
            memset([plan->buffers[8] contents], 0, pixels);
        }
    }
    if (params.delta) {
        status = sixel_retained_read_get_view(request->baseline, &baseline);
        if (status != SIXEL_OK)
            return status;
        for (y = 0; y < params.height; ++y) {
            offset = y + (size_t)plan->output.origin_y;
            memcpy((unsigned char *)[plan->buffers[9] contents] +
                       y * params.width * 3,
                   baseline.rgb + offset * baseline.rgb_stride +
                       (size_t)plan->output.origin_x * 3,
                   params.width * 3);
            memcpy((unsigned char *)[plan->buffers[10] contents] +
                       y * params.width,
                   baseline.valid + offset * baseline.valid_stride +
                       plan->output.origin_x,
                   params.width);
        }
    }
    zero = 0;
    memcpy([plan->buffers[14] contents], &zero, sizeof(zero));
    binding = NULL;
    commands = nil;
    workspace = nil;
    memset(&binding_view, 0, sizeof(binding_view));
    if (mode != 2) {
        status = plan->lookup->vtbl->begin(plan->lookup, &binding);
        if (status != SIXEL_OK)
            return status;
        if (binding == NULL)
            return SIXEL_LOGIC_ERROR;
        status = binding->vtbl->get_view(binding, &binding_view);
        if (status != SIXEL_OK)
            goto end;
        workspace =
            sixel_gpu_metal_buffer(plan->device, binding_view.workspace);
        if (workspace == nil ||
            [workspace length] < plan->view.workspace_size) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
    }
    commands = [plan->queue commandBuffer];
    if (commands == nil) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    ncolors = params.ncolors;
    for (i = 0; binding_view.needs_build && i < plan->view.build_pass_count;
         ++i) {
        pass = &plan->view.build_passes[i];
        encoder = [commands computeCommandEncoder];
        if (encoder == nil) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        [encoder setComputePipelineState:plan->pipelines[i]];
        [encoder setBuffer:plan->buffers[1] offset:0 atIndex:0];
        [encoder setBytes:&ncolors length:sizeof(ncolors) atIndex:1];
        [encoder setBuffer:plan->buffers[12] offset:0 atIndex:2];
        [encoder setBuffer:workspace offset:0 atIndex:3];
        [encoder
             dispatchThreadgroups:MTLSizeMake(pass->groups[0], pass->groups[1],
                                              pass->groups[2])
            threadsPerThreadgroup:MTLSizeMake(pass->threads[0],
                                              pass->threads[1],
                                              pass->threads[2])];
        [encoder endEncoding];
    }
    for (i = mode == 2 ? 9 : 0; i < 10; ++i) {
        pipeline = plan->pipelines[plan->view.build_pass_count +
                                   (i == 0   ? 0
                                    : i == 9 ? (mode == 0   ? 2
                                                : mode == 1 ? 3
                                                            : 4)
                                             : 1)];
        params.step = i == 0 ? 0 : (uint32_t)(i - 1);
        stages = plan->view.batch != NULL && mode != 2 ? 2 : 1;
        for (stage = 0; stage < stages; ++stage) {
            params.batch_stage = stages == 2 ? (uint32_t)stage + 1 : 0;
            encoder = [commands computeCommandEncoder];
            if (encoder == nil) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            [encoder setComputePipelineState:pipeline];
            for (y = 0; y < 18; ++y) {
                if (y == 4) {
                    [encoder setBytes:&params length:sizeof(params) atIndex:y];
                } else if (y == 13) {
                    [encoder setBuffer:workspace offset:0 atIndex:y];
                } else {
                    [encoder setBuffer:plan->buffers[y] offset:0 atIndex:y];
                }
            }
            count = i == 0 || i == 9 || plan->view.batch != NULL
                        ? pixels
                        : (size_t)params.tiles_x * ((params.height + 63) / 64) *
                              1024;
            threads = [pipeline maxTotalThreadsPerThreadgroup];
            if (threads > 128)
                threads = 128;
            [encoder dispatchThreads:MTLSizeMake(count, 1, 1)
                threadsPerThreadgroup:MTLSizeMake(threads, 1, 1)];
            [encoder endEncoding];
            if (stages == 2 && stage == 0) {
                status = jajuni_gpu_encode_batch(plan, commands, workspace);
                if (status != SIXEL_OK)
                    goto end;
            }
        }
    }
    [commands commit];
    [commands waitUntilCompleted];
    sixel_gpu_statistics_record_metal(commands);
    status = [commands status] == MTLCommandBufferStatusCompleted
                 ? SIXEL_OK
                 : SIXEL_RUNTIME_ERROR;
    if (status == SIXEL_OK && *(uint32_t *)[plan->buffers[14] contents])
        status = SIXEL_BAD_ARGUMENT;
    if (status == SIXEL_OK && request->cancel != NULL &&
        request->cancel(request->cancel_context))
        status = SIXEL_INTERRUPTED;
    if (status == SIXEL_OK && mode != 1) {
        memcpy(request->result->indexes, [plan->buffers[11] contents], pixels);
        memcpy(request->result->skip, [plan->buffers[15] contents], pixels);
    }
end:
    /* Uncommitted command buffers have no GPU work to drain. Submitted work
     * is always drained above before releasing the exclusive lookup lease. */
    finish = SIXEL_OK;
    if (binding != NULL) {
        finish = binding->vtbl->finish(binding, status);
        binding->vtbl->unref(binding);
    }
    return status != SIXEL_OK ? status : finish;
}

static SIXELSTATUS jajuni_gpu_run(sixel_gpu_dither_plan_interface_t *self,
                                  sixel_gpu_dither_run_request_t const *request)
{
    jajuni_gpu_plan_t *plan;
    SIXELSTATUS status;

    plan = (jajuni_gpu_plan_t *)self;
    if (sixel_atomic_fetch_add_u32(&plan->busy, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
        return SIXEL_LOGIC_ERROR;
    }
    @autoreleasepool {
        status = jajuni_gpu_run_impl(plan, request, 0);
    }
    (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
    return status;
}

/* Facets share the frozen plan but have independent reference ownership.
 * A live solution holds busy until its final release, even after resolve. */
static jajuni_gpu_plan_t *
jajuni_gpu_from_deferred(sixel_gpu_dither_deferred_interface_t const *self)
{
    return (jajuni_gpu_plan_t *)((char *)self -
                                 offsetof(jajuni_gpu_plan_t, deferred));
}

static jajuni_gpu_plan_t *
jajuni_gpu_from_solution(sixel_gpu_dither_solution_interface_t const *self)
{
    return (jajuni_gpu_plan_t *)((char *)self -
                                 offsetof(jajuni_gpu_plan_t, solution));
}

static void jajuni_gpu_deferred_ref(sixel_gpu_dither_deferred_interface_t *self)
{
    jajuni_gpu_plan_ref(&jajuni_gpu_from_deferred(self)->iface);
}

static void
jajuni_gpu_deferred_unref(sixel_gpu_dither_deferred_interface_t *self)
{
    jajuni_gpu_plan_unref(&jajuni_gpu_from_deferred(self)->iface);
}

static void jajuni_gpu_solution_ref(sixel_gpu_dither_solution_interface_t *self)
{
    (void)sixel_atomic_fetch_add_u32(
        &jajuni_gpu_from_solution(self)->solution_ref, 1);
}

static void
jajuni_gpu_solution_unref(sixel_gpu_dither_solution_interface_t *self)
{
    jajuni_gpu_plan_t *plan;

    plan = jajuni_gpu_from_solution(self);
    if (sixel_atomic_fetch_sub_u32(&plan->solution_ref, 1) != 1)
        return;
    (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
    jajuni_gpu_plan_unref(&plan->iface);
}

static SIXELSTATUS jajuni_gpu_solution_description(
    sixel_gpu_dither_solution_interface_t const *self,
    sixel_indexed_frame_description_t *description)
{
    if (self == NULL || description == NULL)
        return SIXEL_BAD_ARGUMENT;
    *description = jajuni_gpu_from_solution(self)->output;
    return SIXEL_OK;
}

static SIXELSTATUS
jajuni_gpu_solve(sixel_gpu_dither_deferred_interface_t *self,
                 sixel_gpu_dither_solve_request_t const *request,
                 sixel_gpu_dither_solution_interface_t **solution)
{
    jajuni_gpu_plan_t *plan;
    sixel_gpu_dither_run_request_t run;
    SIXELSTATUS status;

    if (self == NULL || request == NULL || solution == NULL)
        return SIXEL_BAD_ARGUMENT;
    *solution = NULL;
    plan = jajuni_gpu_from_deferred(self);
    if (sixel_atomic_fetch_add_u32(&plan->busy, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
        return SIXEL_LOGIC_ERROR;
    }
    memset(&run, 0, sizeof(run));
    run.source = request->source;
    run.logger = request->logger;
    run.cancel = request->cancel;
    run.cancel_context = request->cancel_context;
    @autoreleasepool {
        status = jajuni_gpu_run_impl(plan, &run, 1);
    }
    if (status == SIXEL_OK) {
        plan->solution_ref = 1;
        plan->consumed = 0;
        jajuni_gpu_plan_ref(&plan->iface);
        *solution = &plan->solution;
    } else {
        (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
    }
    return status;
}

static SIXELSTATUS
jajuni_gpu_resolve(sixel_gpu_dither_solution_interface_t *self,
                   sixel_gpu_dither_resolve_request_t const *request)
{
    jajuni_gpu_plan_t *plan;
    sixel_gpu_dither_run_request_t run;
    jajuni_gpu_params_t params;
    SIXELSTATUS status;

    if (self == NULL || request == NULL)
        return SIXEL_BAD_ARGUMENT;
    plan = jajuni_gpu_from_solution(self);
    if (sixel_atomic_fetch_add_u32(&plan->consumed, 0) != 0)
        return SIXEL_LOGIC_ERROR;
    memset(&run, 0, sizeof(run));
    run.result = request->result;
    run.baseline = request->baseline;
    run.delta = request->delta;
    run.logger = request->logger;
    run.cancel = request->cancel;
    run.cancel_context = request->cancel_context;
    status = jajuni_gpu_validate(plan, &run, &params);
    if (status != SIXEL_OK)
        return status;
    if (sixel_atomic_fetch_add_u32(&plan->consumed, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&plan->consumed, 1);
        return SIXEL_LOGIC_ERROR;
    }
    @autoreleasepool {
        status = jajuni_gpu_run_impl(plan, &run, 2);
    }
    return status;
}

static SIXELSTATUS
jajuni_gpu_get_deferred(sixel_gpu_dither_plan_interface_t const *self,
                        sixel_gpu_dither_deferred_interface_t **deferred)
{
    jajuni_gpu_plan_t *plan;

    if (self == NULL || deferred == NULL)
        return SIXEL_BAD_ARGUMENT;
    plan = (jajuni_gpu_plan_t *)self;
    jajuni_gpu_plan_ref(&plan->iface);
    *deferred = &plan->deferred;
    return SIXEL_OK;
}

static sixel_gpu_dither_deferred_vtbl_t const jajuni_gpu_deferred_vtbl = {
    jajuni_gpu_deferred_ref, jajuni_gpu_deferred_unref, jajuni_gpu_solve};

static sixel_gpu_dither_solution_vtbl_t const jajuni_gpu_solution_vtbl = {
    jajuni_gpu_solution_ref, jajuni_gpu_solution_unref,
    jajuni_gpu_solution_description, jajuni_gpu_resolve};

static sixel_gpu_dither_plan_vtbl_t const jajuni_gpu_plan_vtbl = {
    jajuni_gpu_plan_ref, jajuni_gpu_plan_unref, jajuni_gpu_run,
    jajuni_gpu_get_deferred};

static SIXELSTATUS jajuni_gpu_prepare_impl(
    sixel_allocator_t *allocator,
    sixel_dither_policy_prepare_request_t const *configuration,
    sixel_gpu_dither_prepare_request_t const *request,
    sixel_gpu_dither_plan_interface_t **result)
{
    jajuni_gpu_plan_t *plan;
    sixel_palette_t *palette;
    sixel_palette_entries_request_t entries;
    sixel_wire_palette_view_t wire;
    sixel_lookup_execution_request_t lookup_request;
    sixel_gpu_lookup_binding_interface_t *warm_binding;
    id<MTLDevice> device;
    unsigned char colors[768], phase[4096];
    uint32_t positions[4096], counts[4];
    double margin;
    size_t lengths[18], pixels, i, x, y, bucket;
    SIXELSTATUS status;

    if (request->device == NULL || request->lookup == NULL ||
        request->output.palette == NULL || request->output.width < 1 ||
        request->output.height < 1 || request->working_palette == NULL)
        return SIXEL_BAD_ARGUMENT;
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
        request->colorspace != SIXEL_COLORSPACE_GAMMA ||
        (request->scan != SIXEL_SCAN_SERPENTINE &&
         request->scan != SIXEL_SCAN_RASTER))
        return SIXEL_FEATURE_ERROR;
    device = sixel_gpu_metal_device(request->device);
    if (device == nil)
        return SIXEL_FEATURE_ERROR;
    pixels = (size_t)request->output.width * request->output.height;
    if (pixels > UINT32_MAX || pixels > SIZE_MAX / 16 ||
        pixels * 16 > [device maxBufferLength])
        return SIXEL_BAD_ARGUMENT;
    status = sixel_wire_palette_get_view(request->output.palette, &wire);
    if (status != SIXEL_OK)
        return status;
    if (wire.count != (size_t)configuration->options.ncolors ||
        wire.count > 256 ||
        wire.count <= (size_t)request->output.keycolor_reserved ||
        request->output.keycolor_reserved !=
            configuration->options.keycolor_reserved ||
        (request->output.keycolor_reserved &&
         request->output.keycolor != (int)wire.count - 1))
        return SIXEL_BAD_ARGUMENT;
    for (i = 0; i < wire.count; ++i)
        memcpy(colors + 3 * i, wire.colors[i].emitted_rgb, 3);
    plan = sixel_allocator_calloc(allocator, 1, sizeof(*plan));
    if (plan == NULL)
        return SIXEL_BAD_ALLOCATION;
    plan->iface.vtbl = &jajuni_gpu_plan_vtbl;
    plan->deferred.vtbl = &jajuni_gpu_deferred_vtbl;
    plan->solution.vtbl = &jajuni_gpu_solution_vtbl;
    plan->ref = 1;
    plan->allocator = allocator;
    sixel_allocator_ref(allocator);
    plan->device = request->device;
    plan->device->vtbl->ref(plan->device);
    plan->output = request->output;
    sixel_wire_palette_ref(plan->output.palette);
    palette = NULL;
    status = sixel_palette_factory_new(allocator, (void **)&palette);
    if (status != SIXEL_OK)
        goto end;
    entries.entries = colors;
    entries.colors = (unsigned int)wire.count;
    entries.depth = 3;
    status = palette->vtbl->init_entries(palette, &entries);
    if (status != SIXEL_OK)
        goto end;
    memset(&lookup_request, 0, sizeof(lookup_request));
    lookup_request.working_palette = palette;
    lookup_request.ncolors =
        (unsigned int)wire.count - (request->output.keycolor_reserved != 0);
    lookup_request.pixelformat = request->pixelformat;
    lookup_request.colorspace = request->colorspace;
    lookup_request.query_kind = SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL;
    lookup_request.allow_batch = 1;
    status = request->lookup->vtbl->prepare(request->lookup, &lookup_request,
                                            request->device, &plan->lookup);
    if (status != SIXEL_OK)
        goto end;
    if (plan->lookup == NULL) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }
    status = plan->lookup->vtbl->describe(plan->lookup, &plan->view);
    if (status != SIXEL_OK)
        goto end;
    if (plan->view.program_format == NULL ||
        strcmp(plan->view.program_format, "metal.lookup-rgb8.v1") ||
        plan->view.query_kind != SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL) {
        status = SIXEL_FEATURE_ERROR;
        goto end;
    }
    if (plan->view.chunks == NULL || plan->view.chunk_count == 0 ||
        plan->view.ncolors != lookup_request.ncolors ||
        plan->view.palette_size < lookup_request.ncolors * 3 ||
        plan->view.palette == NULL || plan->view.workspace_size == 0 ||
        plan->view.palette_size > [device maxBufferLength] ||
        plan->view.workspace_size > [device maxBufferLength] ||
        plan->view.constants_size > [device maxBufferLength] ||
        (plan->view.constants_size && plan->view.constants == NULL) ||
        (plan->view.build_pass_count && plan->view.build_passes == NULL)) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (memcmp(plan->view.palette, colors, lookup_request.ncolors * 3) != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    status = jajuni_gpu_compile(device, &plan->view, &plan->pipelines);
    if (status != SIXEL_OK)
        goto end;
    /* Reserve lookup capacity during preparation. No builder is submitted,
     * so an unbuilt cache must not be marked valid by this temporary lease. */
    warm_binding = NULL;
    status = plan->lookup->vtbl->begin(plan->lookup, &warm_binding);
    if (status != SIXEL_OK)
        goto end;
    if (warm_binding == NULL) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }
    status = warm_binding->vtbl->finish(warm_binding, SIXEL_INTERRUPTED);
    warm_binding->vtbl->unref(warm_binding);
    if (status != SIXEL_OK)
        goto end;
    memset(lengths, 0, sizeof(lengths));
    lengths[0] = pixels * 3;
    lengths[1] = plan->view.palette_size;
    lengths[2] = lengths[3] = pixels * 16;
    lengths[5] = sizeof(phase);
    lengths[6] = sizeof(positions);
    lengths[8] = pixels;
    lengths[9] = pixels * 3;
    lengths[10] = lengths[11] = pixels;
    lengths[12] = plan->view.constants_size ? plan->view.constants_size : 1;
    lengths[14] = 4;
    lengths[15] = pixels;
    if (plan->view.batch != NULL) {
        lengths[16] = pixels * 16;
        lengths[17] = pixels * 4;
    }
    jajuni_gpu_borrow(plan, device, lengths);
    if (plan->queue == nil)
        plan->queue = [device newCommandQueue];
    for (i = 0; i < 18; ++i) {
        if (lengths[i] == 0 || plan->buffers[i] != nil)
            continue;
        plan->buffers[i] =
            [device newBufferWithLength:lengths[i]
                                options:MTLResourceStorageModeShared];
        if (plan->buffers[i] == nil) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    }
    if (plan->queue == nil) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memset(counts, 0, sizeof(counts));
    for (y = 0; y < 64; ++y) {
        for (x = 0; x < 64; ++x) {
            bucket = sixel_bn64[((y + 7) & 63) * 64 + ((x + 43) & 63)] / 64;
            phase[y * 64 + x] = (unsigned char)bucket;
            if (counts[bucket] >= 1024) {
                status = SIXEL_LOGIC_ERROR;
                goto end;
            }
            positions[bucket * 1024 + counts[bucket]++] =
                (uint32_t)(y * 64 + x);
        }
    }
    memcpy([plan->buffers[1] contents], plan->view.palette,
           plan->view.palette_size);
    memcpy([plan->buffers[5] contents], phase, sizeof(phase));
    memcpy([plan->buffers[6] contents], positions, sizeof(positions));
    if (plan->view.constants_size)
        memcpy([plan->buffers[12] contents], plan->view.constants,
               plan->view.constants_size);
    plan->params.width = (uint32_t)request->output.width;
    plan->params.height = (uint32_t)request->output.height;
    plan->params.ncolors = lookup_request.ncolors;
    plan->params.tiles_x = (plan->params.width + 63) / 64;
    plan->params.keycolor = (uint32_t)request->output.keycolor;
    plan->params.serpentine = request->scan == SIXEL_SCAN_SERPENTINE;
    /* The CPU limiter keeps (threshold - 1) * 255 of headroom on both
     * sides of the byte range. The deadzone discards small corrections
     * before limiting. */
    margin = (configuration->options.clamp_threshold - 1.0) * 255.0;
    plan->params.clamp_hard =
        configuration->options.clamp_policy == SIXEL_DITHER_CLAMP_HARD;
    plan->params.deadzone = (float)configuration->options.diffusion_deadzone;
    plan->params.clamp_lower = (float)-margin;
    plan->params.clamp_upper = (float)(255.0 + margin);
    *result = &plan->iface;
    status = SIXEL_OK;
end:
    if (palette != NULL)
        palette->vtbl->unref(palette);
    if (status != SIXEL_OK)
        jajuni_gpu_plan_unref(&plan->iface);
    return status;
}

SIXELSTATUS
sixel_dither_jajuni_gpu_metal_prepare(
    sixel_allocator_t *allocator,
    sixel_dither_policy_prepare_request_t const *configuration,
    sixel_gpu_dither_prepare_request_t const *request,
    sixel_gpu_dither_plan_interface_t **result)
{
    SIXELSTATUS status;

    @autoreleasepool {
        status =
            jajuni_gpu_prepare_impl(allocator, configuration, request, result);
    }
    return status;
}
#endif /* defined(HAVE_METAL) */
