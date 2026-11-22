/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_WEBGPU)
#include <emscripten.h>
#include <math.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "dither-diffusion-webgpu.h"
#include "gpu-palette.h"
#include "gpu-statistics.h"
#include "encode-result.h"
#include "palette.h"
#include "sixel_atomic.h"
#include "bluenoise_64x64.h"

static char const *const diffusion_point_main[] = {
#include <dither-diffusion-point-wgsl.inc.h>
};
static char const *const diffusion_batch_main[] = {
#include <dither-diffusion-batch-wgsl.inc.h>
};

/* Run modes: a monolithic run, a history-free solve, or its resolve. */
enum {
    DIFFUSION_RUN = 0,
    DIFFUSION_SOLVE = 1,
    DIFFUSION_RESOLVE_ONLY = 2
};

typedef struct diffusion_params {
    uint32_t width, height, ncolors;
    uint32_t keycolor, delta, threshold, metric;
    uint32_t columns, rows, tiled, selected_threshold;
    uint32_t selection[128];
    uint32_t serpentine, clamp_hard;
    float deadzone, clamp_lower, clamp_upper;
    uint32_t variable;
    uint32_t levels[1024];
} diffusion_params_t;

typedef struct diffusion_plan {
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
    char const *const *chunks;
    size_t chunk_count;
    float *weights;
    float *solved;
    sixel_indexed_frame_description_t output;
    diffusion_params_t params;
} diffusion_plan_t;

/* Record a separate pass for each phase. Uniform offsets select immutable
 * parameters for the whole submission. No heap view survives an await. */
EM_ASYNC_JS(int, diffusion_dispatch, (char const *const *lookup, int nl,
    char const *const *kernel, int nk, int w, int h, int serpentine,
    int hard, float deadzone, float lower, float upper,
    unsigned char const *rgb, unsigned char const *mask,
    unsigned char const *pal, unsigned int n, unsigned char const *phase,
    float const *weights, void const *constants, unsigned int bytes,
    float *solution, unsigned int ws,
    sixel_gpu_lookup_build_pass_t const *passes, unsigned int nbuild,
    char const *const *entry, int nentry), {
    const state = Module['sixelWebGPU'];
    if (!state) return 0;
    const device = state.device, lim = device.limits;
    const size = w * h * 16;
    if (Math.max(bytes, size) > lim.maxStorageBufferBindingSize ||
        Math.max(bytes, size + 4) > lim.maxBufferSize ||
        Math.ceil(Math.max(w, h) / 8) > lim.maxComputeWorkgroupsPerDimension)
        return 0;
    const buffers = [];
    let scoped = false, table;
    try {
        const pipe = await state.getPipeline(lookup, nl, kernel, nk,
                                             entry, nentry);
        table = await state.table(lookup, nl, pal, n, constants, bytes,
                                        ws, passes, nbuild);
        if (!rgb) return 1;
        const U = GPUBufferUsage;
        function alloc(size, usage) {
            const b = device.createBuffer({size, usage});
            buffers.push(b);
            return b;
        }
        function upload(ptr, flags, count) {
            const data = new Uint32Array(count);
            for (let i = 0; i < count; ++i) {
                const p = (ptr >>> 0) + 3 * i;
                data[i] = HEAPU8[p] | HEAPU8[p + 1] << 8 |
                    HEAPU8[p + 2] << 16 |
                    ((flags && HEAPU8[(flags >>> 0) + i]) ? 1 << 24 : 0);
            }
            const b = alloc(data.byteLength, U.STORAGE | U.COPY_DST);
            device.queue.writeBuffer(b, 0, data);
            return b;
        }
        device.pushErrorScope('out-of-memory');
        device.pushErrorScope('validation');
        scoped = true;
        const input = upload(rgb, mask, w * h), palette = upload(pal, 0, n);
        const a = alloc(size, U.STORAGE), b = alloc(size, U.STORAGE|U.COPY_SRC);
        // Each binding needs all 32 bytes, even on tighter alignments.
        const stride = Math.max(32, lim.minUniformBufferOffsetAlignment);
        const uniforms = alloc(10 * stride, U.UNIFORM | U.COPY_DST);
        const tables = alloc(4096 + (weights ? 8192 : 0), U.STORAGE|U.COPY_DST);
        state.writeBytes(tables, 0, phase, 4096);
        if (weights) state.writeBytes(tables, 4096, weights, 8192);
        const invalid = alloc(4, U.STORAGE | U.COPY_SRC);
        const stage = alloc(size + 4, U.MAP_READ | U.COPY_DST);
        const entries = [input, palette, a, b, uniforms, tables, invalid].map(
            (buffer, binding) => ({binding, resource: {buffer}}));
        if (table) entries.push({binding:8,resource:{buffer:table.buffer}});
        if (bytes) {
            const buffer = alloc(bytes, U.STORAGE | U.COPY_DST);
            state.writeBytes(buffer, 0, constants, bytes);
            entries.push({binding: 7, resource: {buffer}});
        }
        const enc = device.createCommandEncoder();
        for (let i = 0; i < 10; ++i) {
            const u = new Uint32Array([w,h,i,serpentine,0,0,0,hard]);
            new Float32Array(u.buffer).set([deadzone,lower,upper], 4);
            device.queue.writeBuffer(uniforms, i * stride, u);
            entries[4].resource = {buffer:uniforms,offset:i * stride,size:32};
            const group = device.createBindGroup({
                layout: pipe.getBindGroupLayout(0), entries});
            const pass = enc.beginComputePass();
            pass.setPipeline(pipe);
            pass.setBindGroup(0, group);
            pass.dispatchWorkgroups(Math.ceil(w / 8), Math.ceil(h / 8));
            pass.end();
        }
        enc.copyBufferToBuffer(b, 0, stage, 0, size);
        enc.copyBufferToBuffer(invalid, 0, stage, size, 4);
        device.queue.submit([enc.finish()]);
        const valid = device.popErrorScope(), oom = device.popErrorScope();
        scoped = false;
        const [v, e] = await Promise.all([valid, oom,
            stage.mapAsync(GPUMapMode.READ)]);
        if (v || e) return 0;
        const mapped = stage.getMappedRange();
        if (new Uint32Array(mapped, size, 1)[0]) return 0;
        HEAPU8.set(new Uint8Array(mapped, 0, size), solution >>> 0);
        stage.unmap();
        return 1;
    } catch (e) {
        return 0;
    } finally {
        if (scoped) {
            await device.popErrorScope();
            await device.popErrorScope();
        }
        table?.release();
        for (const b of buffers) b.destroy();
    }
})

static void
diffusion_plan_ref(sixel_gpu_dither_plan_interface_t *self)
{
    diffusion_plan_t *plan;

    plan = (diffusion_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
diffusion_plan_unref(sixel_gpu_dither_plan_interface_t *self)
{
    diffusion_plan_t *plan;
    sixel_allocator_t *allocator;

    plan = (diffusion_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) {
        return;
    }
    allocator = plan->allocator;
    sixel_allocator_free(allocator, plan->weights);
    sixel_allocator_free(allocator, plan->solved);
    if (plan->lookup != NULL) {
        plan->lookup->vtbl->unref(plan->lookup);
    }
    if (plan->device != NULL) {
        plan->device->vtbl->unref(plan->device);
    }
    if (plan->output.palette != NULL) {
        sixel_wire_palette_unref(plan->output.palette);
    }
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

/* Validate without touching scratch so bad arguments do not consume a
 * ready solution. The caller must keep the borrowed request stable. */
static SIXELSTATUS
diffusion_validate(diffusion_plan_t *plan,
    sixel_gpu_dither_run_request_t const *request,
    diffusion_params_t *resolved)
{
    diffusion_params_t params;
    sixel_retained_read_view_t baseline;
    sixel_indexed_frame_description_t const *description;
    size_t cells;
    SIXELSTATUS status;

    if (request == NULL || request->result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    description = &request->result->description;
    if (sixel_indexed_frame_get_state(request->result) !=
            SIXEL_INDEXED_BUILDING ||
            description->width != plan->output.width ||
            description->height != plan->output.height ||
            description->palette != plan->output.palette ||
            description->keycolor != plan->output.keycolor ||
            description->keycolor_reserved !=
                plan->output.keycolor_reserved ||
            description->transparent_background !=
                plan->output.transparent_background ||
            description->encoded_x != plan->output.encoded_x ||
            description->encoded_y != plan->output.encoded_y ||
            description->encoded_width != plan->output.encoded_width ||
            description->encoded_height != plan->output.encoded_height ||
            description->origin_x != plan->output.origin_x ||
            description->origin_y != plan->output.origin_y ||
            description->plane_width != plan->output.plane_width ||
            description->plane_height != plan->output.plane_height) {
        return SIXEL_BAD_ARGUMENT;
    }
    params = plan->params;
    params.delta = request->delta.enabled && request->baseline != NULL;
    params.threshold = request->delta.threshold;
    params.metric = request->delta.metric;
    if (params.delta && (params.threshold > 255 ||
            (params.metric != SIXEL_DELTA_METRIC_MAX &&
             params.metric != SIXEL_DELTA_METRIC_RGB241))) {
        return SIXEL_BAD_ARGUMENT;
    }
    cells = (size_t)request->delta.selection.columns *
        request->delta.selection.rows;
    if ((request->delta.selection.cell_thresholds != NULL &&
            request->delta.selection.cell_threshold_count < cells) ||
        request->delta.selection.columns > 64 ||
            request->delta.selection.rows > 64 ||
            ((request->delta.selection.columns == 0) !=
             (request->delta.selection.rows == 0)) ||
            (cells != 0 && (request->delta.selection.words == NULL ||
                request->delta.selection.word_count < (cells + 31) / 32))) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (cells != 0) {
        params.columns = request->delta.selection.columns;
        params.rows = request->delta.selection.rows;
        params.tiled = request->delta.selection.tiled;
        params.selected_threshold =
            request->delta.selection.selected_threshold;
        memcpy(params.selection, request->delta.selection.words,
            ((cells + 31) / 32) * sizeof(uint32_t));
        params.variable = request->delta.selection.cell_thresholds != NULL;
        if (params.variable)
            sixel_delta_pack_levels(params.levels,
                request->delta.selection.cell_thresholds, cells);
    }
    if ((params.columns == 0) != (params.rows == 0) ||
            (params.tiled && (params.columns != 64 || params.rows != 64))) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (params.delta) {
        status = sixel_retained_read_get_view(request->baseline, &baseline);
        if (status != SIXEL_OK) {
            return status;
        }
        if (baseline.rgb == NULL || baseline.valid == NULL ||
                baseline.rgb_stride < (size_t)baseline.description.width * 3 ||
                baseline.valid_stride < (size_t)baseline.description.width ||
                request->result->identity.stream != baseline.stream ||
                request->result->identity.epoch != baseline.epoch ||
                request->result->identity.base_sequence !=
                    baseline.sequence ||
                plan->output.plane_width != baseline.description.width ||
                plan->output.plane_height != baseline.description.height ||
                plan->output.origin_x < 0 || plan->output.origin_y < 0 ||
                plan->output.origin_x > baseline.description.width ||
                plan->output.origin_y > baseline.description.height ||
                plan->output.width >
                    baseline.description.width - plan->output.origin_x ||
                plan->output.height >
                    baseline.description.height - plan->output.origin_y ||
                plan->output.keycolor < 0) {
            return SIXEL_BAD_ARGUMENT;
        }
    }
    *resolved = params;
    return SIXEL_OK;
}

/* The retained frame can arrive after the GPU solve. Its pitched planes
 * are borrowed only during resolve, including for cropped animation frames. */
static unsigned int
diffusion_threshold(diffusion_params_t const *p, size_t x, size_t y,
    float const *gap)
{
    size_t cell;
    unsigned int level, flags;

    if (!p->columns) return p->threshold;
    cell = p->tiled ? (y % 64U) * 64U + x % 64U :
        ((y + 1U) * p->rows - 1U) / p->height * p->columns +
        ((x + 1U) * p->columns - 1U) / p->width;
    level = p->variable ?
        (p->levels[cell / 4U] >> (8U * (cell % 4U))) & 255U :
        p->selected_threshold;
    if (p->variable && (p->tiled == 2U || p->tiled == 3U)) {
        if (level == 255U) return p->threshold;
        flags = level & 192U;
        level &= 63U;
        cell = ((y + 1U) * 64U - 1U) / p->height * 64U +
            ((x + 1U) * 64U - 1U) / p->width;
        if (p->selection[cell / 32U] & (1U << (cell % 32U))) {
            if (flags & 128U) level = 8U;
            if ((flags & 64U) && (p->tiled == 2U ||
                    fabsf(54 * gap[0] + 183 * gap[1] + 19 * gap[2]) <= 1024))
                level = 16U;
        }
    } else if (!(p->selection[cell / 32U] & (1U << (cell % 32U)))) {
        return p->threshold;
    }
    return level < p->threshold ? level : p->threshold;
}

static unsigned char
diffusion_classify(diffusion_params_t const *p, size_t x, size_t y,
    float const *query, unsigned char const *prior, unsigned char const *color)
{
    float gap[3], d, old, fresh, threshold;
    int c, within;

    old = fresh = 0.0f;
    for (c = 0; c < 3; ++c) {
        gap[c] = query[c] - prior[c];
        old += gap[c] * gap[c];
        d = query[c] - color[c];
        fresh += d * d;
    }
    threshold = (float)diffusion_threshold(p, x, y, gap);
    within = p->metric == SIXEL_DELTA_METRIC_RGB241 ?
        2 * gap[0] * gap[0] + 4 * gap[1] * gap[1] + gap[2] * gap[2]
            <= 7 * threshold * threshold :
        fabsf(gap[0]) <= threshold && fabsf(gap[1]) <= threshold &&
            fabsf(gap[2]) <= threshold;
    return within ? (old == 0.0f ? 1 : 3) : (old <= fresh);
}

static int
diffusion_submit(diffusion_plan_t *plan, unsigned char const *rgb,
    unsigned char const *mask)
{
    diffusion_params_t const *p;

    p = &plan->params;
    return sixel_webgpu_available() &&
        diffusion_dispatch(plan->view.chunks, (int)plan->view.chunk_count,
            plan->chunks, (int)plan->chunk_count, p->width, p->height,
            p->serpentine, p->clamp_hard, p->deadzone, p->clamp_lower,
            p->clamp_upper, rgb, mask, plan->view.palette, p->ncolors,
            sixel_bn64, plan->weights, plan->view.constants,
            (unsigned int)plan->view.constants_size, plan->solved,
            plan->view.workspace_size, plan->view.build_passes,
            plan->view.build_pass_count,
            plan->view.batch ? diffusion_batch_main : diffusion_point_main,
            plan->view.batch ? sizeof(diffusion_batch_main) / sizeof(char *) :
                sizeof(diffusion_point_main) / sizeof(char *));
}

static SIXELSTATUS
diffusion_run_impl(diffusion_plan_t *plan,
    sixel_gpu_dither_run_request_t const *request, int mode)
{
    sixel_frame_pixels_view_t source;
    sixel_frame_transparency_t transparency;
    sixel_retained_read_view_t baseline;
    sixel_gpu_lookup_binding_interface_t *binding;
    diffusion_params_t params;
    size_t pixels, i, x, y, bx, by;
    unsigned int packed, best;
    float const *q;
    unsigned char code;
    SIXELSTATUS status, finish;

    if (request == NULL || (mode != DIFFUSION_RESOLVE_ONLY &&
            request->source == NULL)) {
        return SIXEL_BAD_ARGUMENT;
    }
    params = plan->params;
    if (mode != DIFFUSION_SOLVE) {
        status = diffusion_validate(plan, request, &params);
        if (status != SIXEL_OK) {
            return status;
        }
    }
    if (request->cancel != NULL && request->cancel(request->cancel_context)) {
        return SIXEL_INTERRUPTED;
    }
    pixels = (uint64_t)params.width * params.height;
    memset(&source, 0, sizeof(source));
    memset(&transparency, 0, sizeof(transparency));
    if (mode != DIFFUSION_RESOLVE_ONLY) {
        status = request->source->vtbl->get_pixels(request->source, &source);
        if (status != SIXEL_OK) {
            return status;
        }
        if (source.pixels == NULL || source.width != (int)params.width ||
                source.height != (int)params.height ||
                source.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
                source.colorspace != SIXEL_COLORSPACE_GAMMA) {
            return SIXEL_BAD_ARGUMENT;
        }
        status = request->source->vtbl->get_transparency(request->source,
            &transparency);
        if (status != SIXEL_OK) {
            return status;
        }
        if (transparency.transparent_mask != NULL &&
                (transparency.transparent_mask_size < pixels ||
                 plan->output.keycolor < 0)) {
            return SIXEL_BAD_ARGUMENT;
        }
    }
    if (mode != DIFFUSION_RESOLVE_ONLY) {
        binding = NULL;
        status = plan->lookup->vtbl->begin(plan->lookup, &binding);
        if (status != SIXEL_OK) return status;
        if (binding == NULL) return SIXEL_LOGIC_ERROR;
        status = diffusion_submit(plan, source.pixels,
            transparency.transparent_mask) ? SIXEL_OK : SIXEL_FEATURE_ERROR;
        finish = binding->vtbl->finish(binding, status);
        binding->vtbl->unref(binding);
        if (status != SIXEL_OK) return status;
        if (finish != SIXEL_OK) return finish;
        sixel_gpu_statistics_record(0);
    }
    if (request->cancel != NULL && request->cancel(request->cancel_context))
        return SIXEL_INTERRUPTED;
    if (mode == DIFFUSION_SOLVE) return SIXEL_OK;
    if (params.delta) {
        status = sixel_retained_read_get_view(request->baseline, &baseline);
        if (status != SIXEL_OK) return status;
    }
    for (i = 0; i < pixels; ++i) {
        q = plan->solved + 4U * i;
        packed = (unsigned int)q[3];
        best = packed & 255U;
        code = (unsigned char)(packed >> 16);
        if (!code && params.delta) {
            x = i % params.width;
            y = i / params.width;
            bx = x + plan->output.origin_x;
            by = y + plan->output.origin_y;
            if (baseline.valid[by * baseline.valid_stride + bx]) {
                best = (packed >> 8) & 255U;
                if (best >= params.ncolors) return SIXEL_LOGIC_ERROR;
                code = diffusion_classify(&params, x, y, q,
                    baseline.rgb + by * baseline.rgb_stride + 3 * bx,
                    plan->view.palette + 3 * best);
            }
        }
        if (best >= params.ncolors) return SIXEL_LOGIC_ERROR;
        request->result->indexes[i] =
            (unsigned char)(code ? params.keycolor : best);
        request->result->skip[i] = code;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
diffusion_run(sixel_gpu_dither_plan_interface_t *self,
    sixel_gpu_dither_run_request_t const *request)
{
    diffusion_plan_t *plan;
    SIXELSTATUS status;

    plan = (diffusion_plan_t *)self;
    if (sixel_atomic_fetch_add_u32(&plan->busy, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
        return SIXEL_LOGIC_ERROR;
    }
    status = diffusion_run_impl(plan, request, DIFFUSION_RUN);
    (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
    return status;
}

/* Facets share the frozen plan but have independent reference ownership.
 * A live solution holds busy until its final release, even after resolve. */
static diffusion_plan_t *
diffusion_from_deferred(sixel_gpu_dither_deferred_interface_t const *self)
{
    return (diffusion_plan_t *)((char *)self -
        offsetof(diffusion_plan_t, deferred));
}

static diffusion_plan_t *
diffusion_from_solution(sixel_gpu_dither_solution_interface_t const *self)
{
    return (diffusion_plan_t *)((char *)self -
        offsetof(diffusion_plan_t, solution));
}

static void
diffusion_deferred_ref(sixel_gpu_dither_deferred_interface_t *self)
{
    diffusion_plan_ref(&diffusion_from_deferred(self)->iface);
}

static void
diffusion_deferred_unref(sixel_gpu_dither_deferred_interface_t *self)
{
    diffusion_plan_unref(&diffusion_from_deferred(self)->iface);
}

static void
diffusion_solution_ref(sixel_gpu_dither_solution_interface_t *self)
{
    (void)sixel_atomic_fetch_add_u32(
        &diffusion_from_solution(self)->solution_ref, 1);
}

static void
diffusion_solution_unref(sixel_gpu_dither_solution_interface_t *self)
{
    diffusion_plan_t *plan;

    plan = diffusion_from_solution(self);
    if (sixel_atomic_fetch_sub_u32(&plan->solution_ref, 1) != 1) {
        return;
    }
    (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
    diffusion_plan_unref(&plan->iface);
}

static SIXELSTATUS
diffusion_solution_description(
    sixel_gpu_dither_solution_interface_t const *self,
    sixel_indexed_frame_description_t *description)
{
    if (self == NULL || description == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *description = diffusion_from_solution(self)->output;
    return SIXEL_OK;
}

static SIXELSTATUS
diffusion_solve(sixel_gpu_dither_deferred_interface_t *self,
    sixel_gpu_dither_solve_request_t const *request,
    sixel_gpu_dither_solution_interface_t **solution)
{
    diffusion_plan_t *plan;
    sixel_gpu_dither_run_request_t run;
    SIXELSTATUS status;

    if (self == NULL || request == NULL || solution == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *solution = NULL;
    plan = diffusion_from_deferred(self);
    if (sixel_atomic_fetch_add_u32(&plan->busy, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
        return SIXEL_LOGIC_ERROR;
    }
    memset(&run, 0, sizeof(run));
    run.source = request->source;
    run.logger = request->logger;
    run.cancel = request->cancel;
    run.cancel_context = request->cancel_context;
    status = diffusion_run_impl(plan, &run, DIFFUSION_SOLVE);
    if (status == SIXEL_OK) {
        plan->solution_ref = 1;
        plan->consumed = 0;
        diffusion_plan_ref(&plan->iface);
        *solution = &plan->solution;
    } else {
        (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
    }
    return status;
}

static SIXELSTATUS
diffusion_resolve(sixel_gpu_dither_solution_interface_t *self,
    sixel_gpu_dither_resolve_request_t const *request)
{
    diffusion_plan_t *plan;
    sixel_gpu_dither_run_request_t run;
    diffusion_params_t params;
    SIXELSTATUS status;

    if (self == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    plan = diffusion_from_solution(self);
    if (sixel_atomic_fetch_add_u32(&plan->consumed, 0) != 0) {
        return SIXEL_LOGIC_ERROR;
    }
    memset(&run, 0, sizeof(run));
    run.result = request->result;
    run.baseline = request->baseline;
    run.delta = request->delta;
    run.logger = request->logger;
    run.cancel = request->cancel;
    run.cancel_context = request->cancel_context;
    status = diffusion_validate(plan, &run, &params);
    if (status != SIXEL_OK) {
        return status;
    }
    if (sixel_atomic_fetch_add_u32(&plan->consumed, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&plan->consumed, 1);
        return SIXEL_LOGIC_ERROR;
    }
    return diffusion_run_impl(plan, &run, DIFFUSION_RESOLVE_ONLY);
}

static SIXELSTATUS
diffusion_get_deferred(sixel_gpu_dither_plan_interface_t const *self,
    sixel_gpu_dither_deferred_interface_t **deferred)
{
    diffusion_plan_t *plan;

    if (self == NULL || deferred == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    plan = (diffusion_plan_t *)self;
    diffusion_plan_ref(&plan->iface);
    *deferred = &plan->deferred;
    return SIXEL_OK;
}

static sixel_gpu_dither_deferred_vtbl_t const diffusion_deferred_vtbl = {
    diffusion_deferred_ref, diffusion_deferred_unref,
    diffusion_solve
};

static sixel_gpu_dither_solution_vtbl_t const diffusion_solution_vtbl = {
    diffusion_solution_ref, diffusion_solution_unref,
    diffusion_solution_description, diffusion_resolve
};

static sixel_gpu_dither_plan_vtbl_t const diffusion_plan_vtbl = {
    diffusion_plan_ref, diffusion_plan_unref, diffusion_run,
    diffusion_get_deferred
};

SIXELSTATUS
sixel_diffusion_webgpu_prepare(sixel_allocator_t *allocator,
    sixel_dither_policy_prepare_request_t const *configuration,
    sixel_gpu_dither_prepare_request_t const *request,
    sixel_gpu_dither_plan_interface_t **result, char const *const *chunks,
    size_t chunk_count, sixel_diffusion_weights_fn weights)
{
    diffusion_plan_t *plan;
    sixel_palette_t *palette;
    sixel_palette_entries_request_t entries;
    sixel_wire_palette_view_t wire;
    sixel_lookup_execution_request_t lookup_request;
    unsigned char colors[768];
    double margin;
    uint64_t pixels;
    size_t i;
    SIXELSTATUS status;

    if (allocator == NULL || configuration == NULL || request == NULL ||
            result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *result = NULL;
    if (request->device == NULL || request->lookup == NULL ||
            request->output.palette == NULL || request->output.width < 1 ||
            request->output.height < 1 || request->working_palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            request->colorspace != SIXEL_COLORSPACE_GAMMA ||
            (request->scan != SIXEL_SCAN_SERPENTINE &&
             request->scan != SIXEL_SCAN_RASTER) ||
            strcmp(request->device->vtbl->program_format(request->device),
                "webgpu.lookup-rgb8.v1") != 0) {
        return SIXEL_FEATURE_ERROR;
    }
    pixels = (uint64_t)request->output.width * request->output.height;
    if (pixels > UINT32_MAX / 16U) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_wire_palette_get_view(request->output.palette, &wire);
    if (status != SIXEL_OK) {
        return status;
    }
    if (wire.count != (size_t)configuration->options.ncolors ||
            wire.count > 256 ||
            wire.count <= (size_t)request->output.keycolor_reserved ||
            request->output.keycolor_reserved !=
                configuration->options.keycolor_reserved ||
            (request->output.keycolor_reserved &&
             request->output.keycolor != (int)wire.count - 1)) {
        return SIXEL_BAD_ARGUMENT;
    }
    for (i = 0; i < wire.count; ++i) {
        memcpy(colors + 3 * i, wire.colors[i].emitted_rgb, 3);
    }
    plan = sixel_allocator_calloc(allocator, 1, sizeof(*plan));
    if (plan == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    plan->iface.vtbl = &diffusion_plan_vtbl;
    plan->deferred.vtbl = &diffusion_deferred_vtbl;
    plan->solution.vtbl = &diffusion_solution_vtbl;
    plan->ref = 1;
    plan->allocator = allocator;
    sixel_allocator_ref(allocator);
    plan->device = request->device;
    plan->device->vtbl->ref(plan->device);
    plan->output = request->output;
    sixel_wire_palette_ref(plan->output.palette);
    palette = NULL;
    status = sixel_palette_factory_new(allocator, (void **)&palette);
    if (status != SIXEL_OK) {
        goto end;
    }
    entries.entries = colors;
    entries.colors = (unsigned int)wire.count;
    entries.depth = 3;
    status = palette->vtbl->init_entries(palette, &entries);
    if (status != SIXEL_OK) {
        goto end;
    }
    /* The reserved transparency entry never answers a lookup. */
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
    if (status != SIXEL_OK) {
        goto end;
    }
    if (plan->lookup == NULL) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }
    status = plan->lookup->vtbl->describe(plan->lookup, &plan->view);
    if (status != SIXEL_OK) {
        goto end;
    }
    status = sixel_webgpu_check_lookup(&plan->view,
        SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL);
    if (status != SIXEL_OK) goto end;
    if (plan->view.ncolors != lookup_request.ncolors ||
            memcmp(plan->view.palette, colors,
                lookup_request.ncolors * 3U) != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    plan->params.width = (uint32_t)request->output.width;
    plan->params.height = (uint32_t)request->output.height;
    plan->params.ncolors = lookup_request.ncolors;
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

    plan->chunks = chunks;
    plan->chunk_count = chunk_count;
    if (!diffusion_submit(plan, NULL, NULL)) {
        sixel_helper_set_additional_message(
            "WebGPU diffusion pipeline or frame capacity unavailable.");
        status = SIXEL_FEATURE_ERROR;
        goto end;
    }
    if (weights != NULL) {
        plan->weights = sixel_allocator_malloc(allocator,
            256U * 8U * sizeof(float));
        if (plan->weights == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        weights(configuration, plan->weights);
    }
    plan->solved = sixel_allocator_malloc(allocator,
        (size_t)pixels * 4U * sizeof(float));
    if (plan->solved == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    sixel_gpu_statistics_set_device("webgpu", "WebGPU adapter");
    *result = &plan->iface;

end:
    if (palette != NULL) {
        palette->vtbl->unref(palette);
    }
    if (status != SIXEL_OK) {
        diffusion_plan_unref(&plan->iface);
    }
    return status;
}
#endif
