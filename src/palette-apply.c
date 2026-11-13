/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <limits.h>
#include <string.h>
#include "encode-result.h"
#include "dither-policy-private.h"
#include "gpu-palette.h"
#include "dither-policy-gpu.h"
#include "threading.h"
#include "pixelformat.h"
#include "timeline-logger.h"

typedef struct sixel_palette_apply_storage {
    sixel_palette_apply_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_palette_apply_prepare_request_t plan;
    sixel_dither_policy_options_t options;
    sixel_mutex_t row_gate;
    unsigned char *ready;
    unsigned char comparison[SIXEL_PALETTE_MAX * 3];
    sixel_lookup_policy_interface_t *lookup;
    sixel_gpu_dither_plan_interface_t *gpu_dither;
    sixel_palette_apply_deferred_interface_t *deferred;
    int gpu;
    int prepared;
    int running;
    int prefix;
    SIXELSTATUS status;
    sixel_palette_apply_run_request_t const *run;
    sixel_dither_execution_t execution;
    sixel_frame_pixels_view_t pixels;
    sixel_palette_entries_view_t entries;
} sixel_palette_apply_storage_t;

static void
sixel_palette_apply_ref(sixel_palette_apply_interface_t *self)
{
    sixel_palette_apply_storage_t *apply;

    apply = (sixel_palette_apply_storage_t *)self;
    if (apply != NULL) {
        (void)sixel_atomic_fetch_add_u32(&apply->ref, 1);
    }
}

static void
sixel_palette_apply_clear_plan(sixel_palette_apply_storage_t *apply)
{
    if (apply->prepared) {
        apply->plan.working_palette->vtbl->unref(apply->plan.working_palette);
        if (apply->lookup != NULL) {
            apply->lookup->vtbl->unref(apply->lookup);
        }
        if (apply->gpu) apply->plan.device->vtbl->unref(apply->plan.device);
        if (apply->plan.dither != NULL) {
            apply->plan.dither->vtbl->unref(apply->plan.dither);
        }
        if (apply->gpu_dither != NULL) {
            apply->gpu_dither->vtbl->unref(apply->gpu_dither);
        }
        if (apply->plan.cpu_lookup != NULL) {
            apply->plan.cpu_lookup->vtbl->unref(apply->plan.cpu_lookup);
        }
        if (apply->plan.gpu_lookup != NULL) {
            apply->plan.gpu_lookup->vtbl->unref(apply->plan.gpu_lookup);
        }
        if (apply->plan.gpu_dither != NULL) {
            apply->plan.gpu_dither->vtbl->unref(apply->plan.gpu_dither);
        }
        sixel_wire_palette_unref(apply->plan.output.palette);
    }
    if (apply->deferred != NULL)
        apply->deferred->vtbl->unref(apply->deferred);
    apply->deferred = NULL;
    apply->gpu_dither = NULL;
    apply->lookup = NULL;
    sixel_allocator_free(apply->allocator, apply->ready);
    apply->ready = NULL;
    apply->prepared = 0;
}

static void
sixel_palette_apply_unref(sixel_palette_apply_interface_t *self)
{
    sixel_palette_apply_storage_t *apply;
    sixel_allocator_t *allocator;

    apply = (sixel_palette_apply_storage_t *)self;
    if (apply == NULL || sixel_atomic_fetch_sub_u32(&apply->ref, 1) != 1) {
        return;
    }
    allocator = apply->allocator;
    sixel_palette_apply_clear_plan(apply);
    sixel_mutex_destroy(&apply->row_gate);
    sixel_allocator_free(allocator, apply);
    sixel_allocator_unref(allocator);
}

/* The deferred facet owns a frozen GPU plan independently of the mutable
 * PaletteApply object. Both embedded interfaces are reserved at prepare;
 * no image or solution allocation is needed on the producer thread. */
typedef struct sixel_apply_deferred {
    sixel_palette_apply_deferred_interface_t iface;
    sixel_palette_apply_solution_interface_t solution;
    sixel_atomic_u32_t ref, solution_ref, busy, consumed;
    sixel_allocator_t *allocator;
    sixel_gpu_dither_deferred_interface_t *gpu;
    sixel_gpu_dither_solution_interface_t *solved;
    sixel_indexed_frame_description_t description;
} sixel_apply_deferred_t;

static void
sixel_apply_deferred_ref(sixel_palette_apply_deferred_interface_t *self)
{
    sixel_apply_deferred_t *owner;

    owner = (sixel_apply_deferred_t *)self;
    (void)sixel_atomic_fetch_add_u32(&owner->ref, 1);
}

static void
sixel_apply_deferred_unref(sixel_palette_apply_deferred_interface_t *self)
{
    sixel_apply_deferred_t *owner;
    sixel_allocator_t *allocator;

    owner = (sixel_apply_deferred_t *)self;
    if (sixel_atomic_fetch_sub_u32(&owner->ref, 1) != 1) return;
    allocator = owner->allocator;
    owner->gpu->vtbl->unref(owner->gpu);
    sixel_wire_palette_unref(owner->description.palette);
    sixel_allocator_free(allocator, owner);
    sixel_allocator_unref(allocator);
}

static sixel_apply_deferred_t *
sixel_apply_solution_owner(sixel_palette_apply_solution_interface_t const *s)
{
    return (sixel_apply_deferred_t *)((char *)s -
        offsetof(sixel_apply_deferred_t, solution));
}

static void
sixel_apply_solution_ref(sixel_palette_apply_solution_interface_t *self)
{
    (void)sixel_atomic_fetch_add_u32(
        &sixel_apply_solution_owner(self)->solution_ref, 1);
}

static void
sixel_apply_solution_unref(sixel_palette_apply_solution_interface_t *self)
{
    sixel_apply_deferred_t *owner;

    owner = sixel_apply_solution_owner(self);
    if (sixel_atomic_fetch_sub_u32(&owner->solution_ref, 1) != 1) return;
    owner->solved->vtbl->unref(owner->solved);
    owner->solved = NULL;
    (void)sixel_atomic_fetch_sub_u32(&owner->busy, 1);
    sixel_apply_deferred_unref(&owner->iface);
}

static SIXELSTATUS
sixel_apply_solution_description(
    sixel_palette_apply_solution_interface_t const *self,
    sixel_indexed_frame_description_t *description)
{
    if (self == NULL || description == NULL) return SIXEL_BAD_ARGUMENT;
    *description = sixel_apply_solution_owner(self)->description;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_apply_deferred_solve(sixel_palette_apply_deferred_interface_t *self,
    sixel_palette_apply_solve_request_t const *request,
    sixel_palette_apply_solution_interface_t **solution)
{
    sixel_apply_deferred_t *owner;
    sixel_gpu_dither_solve_request_t solve;
    SIXELSTATUS status;

    if (self == NULL || request == NULL || solution == NULL)
        return SIXEL_BAD_ARGUMENT;
    *solution = NULL;
    owner = (sixel_apply_deferred_t *)self;
    if (sixel_atomic_fetch_add_u32(&owner->busy, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&owner->busy, 1);
        return SIXEL_LOGIC_ERROR;
    }
    memset(&solve, 0, sizeof(solve));
    solve.source = request->source;
    solve.logger = request->logger;
    solve.cancel = request->cancel;
    solve.cancel_context = request->cancel_context;
    status = owner->gpu->vtbl->solve(owner->gpu, &solve, &owner->solved);
    if (status == SIXEL_OK && owner->solved == NULL)
        status = SIXEL_LOGIC_ERROR;
    if (status == SIXEL_OK) {
        owner->solution_ref = 1;
        owner->consumed = 0;
        sixel_apply_deferred_ref(self);
        *solution = &owner->solution;
    } else {
        (void)sixel_atomic_fetch_sub_u32(&owner->busy, 1);
    }
    return status;
}

static SIXELSTATUS
sixel_apply_solution_resolve(sixel_palette_apply_solution_interface_t *self,
    sixel_palette_apply_resolve_request_t const *request)
{
    sixel_apply_deferred_t *owner;
    sixel_indexed_frame_description_t const *desc, *expected;
    sixel_gpu_dither_resolve_request_t resolve;
    sixel_retained_read_view_t baseline;
    size_t cells;
    SIXELSTATUS status;

    if (self == NULL || request == NULL || request->result == NULL)
        return SIXEL_BAD_ARGUMENT;
    owner = sixel_apply_solution_owner(self);
    if (sixel_atomic_fetch_add_u32(&owner->consumed, 0) != 0)
        return SIXEL_LOGIC_ERROR;
    desc = &request->result->description;
    expected = &owner->description;
    /* Compare members, not padding; no descriptor may relabel solved pixels. */
    if (sixel_indexed_frame_get_state(request->result) !=
            SIXEL_INDEXED_BUILDING ||
            desc->width != expected->width ||
            desc->height != expected->height ||
            desc->palette != expected->palette ||
            desc->keycolor != expected->keycolor ||
            desc->keycolor_reserved != expected->keycolor_reserved ||
            desc->transparent_background != expected->transparent_background ||
            desc->encoded_x != expected->encoded_x ||
            desc->encoded_y != expected->encoded_y ||
            desc->encoded_width != expected->encoded_width ||
            desc->encoded_height != expected->encoded_height ||
            desc->origin_x != expected->origin_x ||
            desc->origin_y != expected->origin_y ||
            desc->plane_width != expected->plane_width ||
            desc->plane_height != expected->plane_height)
        return SIXEL_BAD_ARGUMENT;
    if (request->delta.compare != SIXEL_DELTA_COMPARE_TARGET &&
            request->delta.compare != SIXEL_DELTA_COMPARE_QUERY)
        return SIXEL_BAD_ARGUMENT;
    cells = (size_t)request->delta.selection.columns *
        request->delta.selection.rows;
    if (request->delta.threshold > 255 ||
            (request->delta.enabled && request->baseline != NULL &&
             request->delta.metric != SIXEL_DELTA_METRIC_MAX &&
             request->delta.metric != SIXEL_DELTA_METRIC_RGB241) ||
            request->delta.selection.columns > 64 ||
            request->delta.selection.rows > 64 ||
            ((request->delta.selection.columns == 0) !=
             (request->delta.selection.rows == 0)) ||
            (request->delta.selection.tiled &&
             (request->delta.selection.columns != 64 ||
              request->delta.selection.rows != 64)) ||
            (cells && (request->delta.selection.words == NULL ||
             request->delta.selection.word_count < (cells + 31) / 32)))
        return SIXEL_BAD_ARGUMENT;
    if (request->delta.enabled && request->baseline != NULL) {
        status = sixel_retained_read_get_view(request->baseline, &baseline);
        if (status != SIXEL_OK) return status;
        if (request->result->identity.stream != baseline.stream ||
                request->result->identity.epoch != baseline.epoch ||
                request->result->identity.base_sequence != baseline.sequence ||
                desc->plane_width != baseline.description.width ||
                desc->plane_height != baseline.description.height ||
                desc->keycolor < 0 || baseline.rgb == NULL ||
                baseline.valid == NULL ||
                baseline.rgb_stride < (size_t)desc->plane_width * 3 ||
                baseline.valid_stride < (size_t)desc->plane_width ||
                desc->origin_x < 0 || desc->origin_y < 0 ||
                desc->origin_x > desc->plane_width - desc->width ||
                desc->origin_y > desc->plane_height - desc->height)
            return SIXEL_BAD_ARGUMENT;
    }
    if (sixel_atomic_fetch_add_u32(&owner->consumed, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&owner->consumed, 1);
        return SIXEL_LOGIC_ERROR;
    }
    memset(&resolve, 0, sizeof(resolve));
    resolve.baseline = request->baseline;
    resolve.result = request->result;
    resolve.delta = request->delta;
    resolve.logger = request->logger;
    resolve.cancel = request->cancel;
    resolve.cancel_context = request->cancel_context;
    sixel_indexed_frame_acquire_user(request->result);
    status = owner->solved->vtbl->resolve(owner->solved, &resolve);
    if (status == SIXEL_OK && request->cancel != NULL &&
            request->cancel(request->cancel_context))
        status = SIXEL_INTERRUPTED;
    /* Validate producer indexes before publishing any rows. */
    if (status == SIXEL_OK)
        status = sixel_indexed_frame_publish_applied(request->result,
            desc->height);
    if (status == SIXEL_OK)
        status = sixel_indexed_frame_complete(request->result);
    if (status != SIXEL_OK) sixel_indexed_frame_fail(request->result);
    sixel_indexed_frame_release_user(request->result);
    return status;
}

static sixel_palette_apply_deferred_vtbl_t const sixel_apply_deferred_vtbl = {
    sixel_apply_deferred_ref, sixel_apply_deferred_unref,
    sixel_apply_deferred_solve
};

static sixel_palette_apply_solution_vtbl_t const sixel_apply_solution_vtbl = {
    sixel_apply_solution_ref, sixel_apply_solution_unref,
    sixel_apply_solution_description, sixel_apply_solution_resolve
};

static SIXELSTATUS
sixel_apply_deferred_new(sixel_allocator_t *allocator,
    sixel_gpu_dither_plan_interface_t *plan,
    sixel_indexed_frame_description_t const *description,
    sixel_palette_apply_deferred_interface_t **result)
{
    sixel_apply_deferred_t *owner;
    sixel_gpu_dither_deferred_interface_t *gpu;
    SIXELSTATUS status;

    *result = NULL;
    gpu = NULL;
    if (plan == NULL || plan->vtbl->get_deferred == NULL) return SIXEL_OK;
    status = plan->vtbl->get_deferred(plan, &gpu);
    if (status == SIXEL_FEATURE_ERROR) return SIXEL_OK;
    if (status != SIXEL_OK) return status;
    if (gpu == NULL) return SIXEL_LOGIC_ERROR;
    owner = sixel_allocator_calloc(allocator, 1, sizeof(*owner));
    if (owner == NULL) {
        gpu->vtbl->unref(gpu);
        return SIXEL_BAD_ALLOCATION;
    }
    owner->iface.vtbl = &sixel_apply_deferred_vtbl;
    owner->solution.vtbl = &sixel_apply_solution_vtbl;
    owner->ref = 1;
    owner->allocator = allocator;
    sixel_allocator_ref(allocator);
    owner->gpu = gpu;
    owner->description = *description;
    sixel_wire_palette_ref(description->palette);
    *result = &owner->iface;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_palette_apply_prepare(sixel_palette_apply_interface_t *self,
    sixel_palette_apply_prepare_request_t const *request)
{
    sixel_palette_apply_storage_t *apply;
    sixel_dither_policy_options_t options;
    sixel_dither_policy_prepare_request_t configuration;
    sixel_wire_palette_view_t palette;
    sixel_lookup_execution_request_t execution;
    sixel_lookup_policy_interface_t *lookup;
    sixel_gpu_dither_prepare_request_t dither_request;
    sixel_dither_policy_prepare_request_t gpu_configuration;
    sixel_gpu_dither_plan_interface_t *gpu_dither;
    sixel_palette_apply_deferred_interface_t *deferred;
    unsigned char *ready;
    size_t pixels;
    size_t i;
    int gpu;
    SIXELSTATUS status;

    if (self == NULL || request == NULL || request->working_palette == NULL ||
            (request->dither == NULL && request->gpu_dither == NULL) ||
            request->output.palette == NULL || request->output.width <= 0 ||
            request->output.height <= 0 || request->worker_threads < 1 ||
            request->band_height < 0 || request->band_overlap < 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    /* Other working representations keep their explicit legacy adapter. */
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            request->colorspace != SIXEL_COLORSPACE_GAMMA) {
        return SIXEL_FEATURE_ERROR;
    }
    apply = (sixel_palette_apply_storage_t *)self;
    if (apply->running) {
        return SIXEL_LOGIC_ERROR;
    }
    memset(&configuration, 0, sizeof(configuration));
    status = request->dither != NULL
        ? request->dither->vtbl->get_configuration(request->dither,
            &configuration)
        : request->gpu_dither->vtbl->get_configuration(request->gpu_dither,
            &configuration);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if ((size_t)request->output.height > SIZE_MAX /
            (size_t)request->output.width / 3) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (configuration.pixelformat != request->pixelformat ||
            configuration.method_for_scan != request->scan) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->dither != NULL && request->gpu_dither != NULL) {
        memset(&gpu_configuration, 0, sizeof(gpu_configuration));
        status = request->gpu_dither->vtbl->get_configuration(
            request->gpu_dither, &gpu_configuration);
        if (SIXEL_FAILED(status)) return status;
        if (memcmp(&configuration, &gpu_configuration,
                sizeof(configuration)) != 0) return SIXEL_BAD_ARGUMENT;
    }
    options = configuration.options;
    pixels = (size_t)request->output.width * request->output.height;
    memset(&execution, 0, sizeof(execution));
    execution.working_palette = request->working_palette;
    execution.ncolors = options.ncolors - (options.keycolor_reserved != 0);
    execution.pixelformat = request->pixelformat;
    execution.colorspace = request->colorspace;
    execution.parallel_dither_active = request->worker_threads > 1;
    lookup = NULL;
    gpu_dither = NULL;
    gpu = request->gpu_policy != SIXEL_GPU_POLICY_OFF &&
        !(request->dither != NULL &&
          request->gpu_policy == SIXEL_GPU_POLICY_AUTO &&
          pixels < request->gpu_auto_threshold);
    /* Gradient analysis has its own prepared source map in the legacy
     * adapter. Do not silently discard a requested modulation. */
    if (options.bluenoise_gradient_factor != 0) {
        return SIXEL_FEATURE_ERROR;
    }
    status = sixel_wire_palette_get_view(request->output.palette, &palette);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    ready = sixel_allocator_calloc(apply->allocator,
        (size_t)request->output.height, 1);
    if (ready == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    if (palette.count != (size_t)options.ncolors ||
            options.keycolor_reserved != request->output.keycolor_reserved ||
            (options.keycolor_reserved && (options.ncolors < 2 ||
             request->output.keycolor != options.ncolors - 1))) {
        sixel_allocator_free(apply->allocator, ready);
        return SIXEL_BAD_ARGUMENT;
    }
    status = SIXEL_FEATURE_ERROR;
    if (gpu &&
            request->gpu_lookup != NULL && request->device != NULL) {
        memset(&dither_request, 0, sizeof(dither_request));
        dither_request.working_palette = request->working_palette;
        dither_request.lookup = request->gpu_lookup;
        dither_request.device = request->device;
        dither_request.output = request->output;
        dither_request.pixelformat = request->pixelformat;
        dither_request.colorspace = request->colorspace;
        dither_request.scan = request->scan;
        status = request->gpu_dither != NULL
            ? request->gpu_dither->vtbl->prepare(request->gpu_dither,
                &dither_request, &gpu_dither)
            : sixel_gpu_dither_compat_prepare(apply->allocator,
                &configuration, &dither_request, &gpu_dither);
        if (status == SIXEL_OK && gpu_dither == NULL) {
            status = SIXEL_LOGIC_ERROR;
        }
    }
    if (status == SIXEL_FEATURE_ERROR &&
            request->gpu_policy != SIXEL_GPU_POLICY_FORCE &&
            request->dither != NULL && request->cpu_lookup != NULL) {
        status = request->cpu_lookup->vtbl->prepare(request->cpu_lookup,
            &execution, &lookup);
    }
    if (status == SIXEL_OK && lookup == NULL &&
            gpu_dither == NULL) {
        status = SIXEL_LOGIC_ERROR;
    }
    if (status != SIXEL_OK) {
        sixel_allocator_free(apply->allocator, ready);
        return status;
    }
    status = sixel_apply_deferred_new(apply->allocator, gpu_dither,
        &request->output, &deferred);
    if (status != SIXEL_OK) {
        if (gpu_dither != NULL) gpu_dither->vtbl->unref(gpu_dither);
        if (lookup != NULL) lookup->vtbl->unref(lookup);
        sixel_allocator_free(apply->allocator, ready);
        return status;
    }
    /* Retain capabilities before releasing the previous plan, including
     * when the caller reuses one of that plan's preparation objects. */
    if (request->cpu_lookup != NULL)
        request->cpu_lookup->vtbl->ref(request->cpu_lookup);
    if (request->gpu_lookup != NULL)
        request->gpu_lookup->vtbl->ref(request->gpu_lookup);
    if (request->gpu_dither != NULL)
        request->gpu_dither->vtbl->ref(request->gpu_dither);
    sixel_palette_apply_clear_plan(apply);
    apply->plan = *request;
    apply->lookup = lookup;
    apply->gpu_dither = gpu_dither;
    apply->deferred = deferred;
    apply->options = options;
    apply->gpu = gpu_dither != NULL;
    if (apply->gpu) request->device->vtbl->ref(request->device);
    apply->ready = ready;
    request->working_palette->vtbl->ref(request->working_palette);
    if (request->dither != NULL) request->dither->vtbl->ref(request->dither);
    sixel_wire_palette_ref(request->output.palette);
    for (i = 0; i < palette.count; ++i) {
        memcpy(apply->comparison + i * 3, palette.colors[i].emitted_rgb, 3);
    }
    apply->prepared = 1;
    return SIXEL_OK;
}

/* Publication is serialized even when dither bands complete out of order.
 * Only the completed contiguous prefix becomes visible to body workers. */
static void
sixel_palette_apply_row_ready(void *context, int row)
{
    sixel_palette_apply_storage_t *apply;
    sixel_indexed_frame_t *image;
    int previous;

    apply = context;
    image = apply->run->result;
    if (row < 0 || row >= apply->plan.output.height) {
        return;
    }
    sixel_mutex_lock(&apply->row_gate);
    apply->ready[row] = 1;
    previous = apply->prefix;
    while (apply->prefix < apply->plan.output.height &&
            apply->ready[apply->prefix]) {
        ++apply->prefix;
    }
    if (apply->status == SIXEL_OK && apply->run->cancel != NULL &&
            apply->run->cancel(apply->run->cancel_context)) {
        apply->status = SIXEL_INTERRUPTED;
    }
    if (apply->status == SIXEL_OK && apply->prefix != previous) {
        apply->status = sixel_indexed_frame_publish_applied(image,
            apply->prefix);
        if (apply->status == SIXEL_OK && apply->run->rows_ready != NULL) {
            apply->status = apply->run->rows_ready(image, apply->prefix,
                apply->run->rows_context);
        }
    }
    sixel_mutex_unlock(&apply->row_gate);
}

static SIXELSTATUS
sixel_palette_apply_band(sixel_palette_apply_storage_t *apply,
    int start, int end, int overlap)
{
    sixel_dither_policy_apply_request_t request;
    unsigned char *copy;
    unsigned char *pixels;
    size_t row_bytes;
    int origin;
    int job;
    SIXELSTATUS status;

    origin = start > overlap ? start - overlap : 0;
    row_bytes = (size_t)apply->pixels.width *
        (size_t)sixel_helper_compute_depth(apply->pixels.pixelformat);
    pixels = apply->pixels.pixels + (size_t)origin * row_bytes;
    copy = NULL;
    if (overlap > 0) {
        copy = sixel_allocator_malloc(apply->allocator,
            (size_t)(end - origin) * row_bytes);
        if (copy == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        memcpy(copy, pixels, (size_t)(end - origin) * row_bytes);
        pixels = copy;
    }
    memset(&request, 0, sizeof(request));
    request.result = apply->run->result->indexes +
        (size_t)origin * apply->pixels.width;
    request.data = pixels;
    request.width = apply->pixels.width;
    request.height = end - origin;
    request.band_origin = origin;
    request.output_start = start;
    request.depth = 3;
    request.palette = apply->entries.entries;
    request.lookup_policy = apply->lookup;
    request.execution = &apply->execution;
    job = apply->plan.band_height > 0 ? start / apply->plan.band_height : 0;
    sixel_timeline_logger_logf(apply->run->logger, "worker", "dither",
        "start", job, origin, start, end, origin, end, "prepared");
    status = apply->plan.dither->vtbl->apply(apply->plan.dither, &request);
    sixel_timeline_logger_logf(apply->run->logger, "worker", "dither",
        "finish", job, end - 1, start, end, origin, end, "prepared");
    sixel_allocator_free(apply->allocator, copy);
    return status;
}

static int
sixel_palette_apply_worker(sixel_thread_pool_job_t job, void *context,
    void *workspace)
{
    sixel_palette_apply_storage_t *apply;
    int start;
    int end;

    (void)workspace;
    apply = context;
    start = job.band_index * apply->plan.band_height;
    end = start + apply->plan.band_height;
    if (end > apply->pixels.height) {
        end = apply->pixels.height;
    }
    return sixel_palette_apply_band(apply, start, end,
        apply->plan.band_overlap);
}

static SIXELSTATUS
sixel_palette_apply_cpu(sixel_palette_apply_storage_t *apply)
{
    sixel_thread_pool_create_request_t request;
    sixel_thread_pool_job_t job;
    sixel_threadpool_service_t *service;
    sixel_thread_pool_t *pool;
    void *object;
    int bands;
    int i;
    SIXELSTATUS status;
    SIXELSTATUS finish_status;

    if (apply->plan.worker_threads <= 1 || apply->plan.band_height <= 0 ||
            !apply->plan.dither->vtbl->supports_parallel_bands(
                apply->plan.dither)) {
        return sixel_palette_apply_band(apply, 0, apply->pixels.height, 0);
    }
    object = NULL;
    status = sixel_components_getservice("services/threadpool", &object);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    service = object;
    bands = (apply->pixels.height - 1) / apply->plan.band_height + 1;
    memset(&request, 0, sizeof(request));
    request.threads = apply->plan.worker_threads;
    if (request.threads > bands) {
        request.threads = bands;
    }
    request.queue_size = request.threads;
    request.worker = sixel_palette_apply_worker;
    request.userdata = apply;
    pool = NULL;
    status = service->vtbl->create_pool(service, &request, &pool);
    service->vtbl->unref(service);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    for (i = 0; i < bands; ++i) {
        job.band_index = i;
        status = pool->vtbl->push(pool, job);
        if (SIXEL_FAILED(status)) {
            break;
        }
    }
    pool->vtbl->finish(pool);
    finish_status = pool->vtbl->get_error(pool);
    pool->vtbl->unref(pool);
    return SIXEL_FAILED(status) ? status : finish_status;
}

static SIXELSTATUS
sixel_palette_apply_gpu_rows(void *context, int rows)
{
    sixel_palette_apply_storage_t *apply;
    SIXELSTATUS status;

    apply = context;
    if (apply->run->cancel != NULL &&
            apply->run->cancel(apply->run->cancel_context)) {
        return SIXEL_INTERRUPTED;
    }
    status = apply->plan.output.keycolor < 0 ||
            apply->plan.output.keycolor_reserved
        ? sixel_indexed_frame_publish_gpu_rows(apply->run->result, rows)
        : sixel_indexed_frame_publish_applied(apply->run->result, rows);
    if (status != SIXEL_OK) return status;
    apply->prefix = rows;
    if (apply->run->rows_ready != NULL) {
        return apply->run->rows_ready(apply->run->result, rows,
            apply->run->rows_context);
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_palette_apply_run(sixel_palette_apply_interface_t *self,
    sixel_palette_apply_run_request_t const *request)
{
    sixel_palette_apply_storage_t *apply;
    sixel_indexed_frame_description_t const *desc;
    sixel_frame_transparency_t transparency;
    sixel_gpu_dither_run_request_t gpu_run;
    sixel_retained_read_view_t baseline;
    sixel_dither_execution_t *execution;
    size_t i;
    size_t cells;
    SIXELSTATUS status;

    if (self == NULL || request == NULL || request->source == NULL ||
            request->result == NULL ||
            (request->delta.compare != SIXEL_DELTA_COMPARE_TARGET &&
             request->delta.compare != SIXEL_DELTA_COMPARE_QUERY)) {
        return SIXEL_BAD_ARGUMENT;
    }
    apply = (sixel_palette_apply_storage_t *)self;
    if (!apply->prepared || apply->running ||
            sixel_indexed_frame_get_state(request->result) !=
                SIXEL_INDEXED_BUILDING) {
        return SIXEL_LOGIC_ERROR;
    }
    desc = &request->result->description;
    if (desc->width != apply->plan.output.width ||
            desc->height != apply->plan.output.height ||
            desc->palette != apply->plan.output.palette ||
            desc->keycolor != apply->plan.output.keycolor ||
            desc->keycolor_reserved != apply->plan.output.keycolor_reserved ||
            desc->transparent_background !=
                apply->plan.output.transparent_background ||
            desc->encoded_x != apply->plan.output.encoded_x ||
            desc->encoded_y != apply->plan.output.encoded_y ||
            desc->encoded_width != apply->plan.output.encoded_width ||
            desc->encoded_height != apply->plan.output.encoded_height ||
            desc->origin_x != apply->plan.output.origin_x ||
            desc->origin_y != apply->plan.output.origin_y ||
            desc->plane_width != apply->plan.output.plane_width ||
            desc->plane_height != apply->plan.output.plane_height) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = request->source->vtbl->get_pixels(request->source, &apply->pixels);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (apply->pixels.width != desc->width ||
            apply->pixels.height != desc->height ||
            apply->pixels.pixelformat != apply->plan.pixelformat ||
            apply->pixels.colorspace != apply->plan.colorspace) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = request->source->vtbl->get_transparency(request->source,
        &transparency);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (transparency.transparent_mask != NULL &&
            transparency.transparent_mask_size < request->result->pixels) {
        return SIXEL_BAD_ARGUMENT;
    }
    cells = (size_t)request->delta.selection.columns *
        request->delta.selection.rows;
    if (request->delta.selection.columns > 64 ||
            request->delta.selection.rows > 64 || cells > 4096 ||
            (cells != 0 &&
            (request->delta.selection.words == NULL ||
             request->delta.selection.word_count < (cells + 31) / 32))) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = apply->plan.working_palette->vtbl->get_entries(
        apply->plan.working_palette, &apply->entries);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (apply->entries.entries == NULL || apply->entries.depth != 3 ||
            apply->entries.entry_count != (unsigned)apply->options.ncolors ||
            apply->entries.entries_size < apply->entries.entry_count * 3) {
        return SIXEL_BAD_ARGUMENT;
    }
    execution = &apply->execution;
    memset(execution, 0, sizeof(*execution));
    execution->working_palette = apply->plan.working_palette;
    execution->allocator = apply->allocator;
    execution->indexes = request->result->indexes;
    execution->width = desc->width;
    execution->height = desc->height;
    execution->keycolor = desc->keycolor;
    execution->delta_keycolor = desc->keycolor;
    execution->transparent_mask = transparency.transparent_mask;
    execution->transparent_size = transparency.transparent_mask_size;
    execution->skip = request->result->skip;
    execution->skip_size = request->result->pixels;
    execution->logger = request->logger;
    execution->band_height = apply->plan.band_height;
    execution->row_ready = sixel_palette_apply_row_ready;
    execution->row_context = apply;
    execution->selection = request->delta.selection;
    execution->delta_metric = request->delta.metric;
    execution->delta_compare = request->delta.compare;
    execution->delta_error_mode = request->delta.error_mode;
    execution->delta_threshold = request->delta.threshold;
    /* An explicit emitted table also covers legacy HLS and percentage RGB
     * without perturbing the working palette used for nearest lookup. */
    execution->delta_hls = 1;
    execution->hls_rgb = apply->comparison;
    execution->hls_count = (unsigned int)apply->entries.entry_count;
    if (request->baseline != NULL && request->delta.enabled) {
        status = sixel_retained_read_get_view(request->baseline, &baseline);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (request->result->identity.stream != baseline.stream ||
                request->result->identity.epoch != baseline.epoch ||
                request->result->identity.base_sequence != baseline.sequence ||
                desc->plane_width != baseline.description.width ||
                desc->plane_height != baseline.description.height) {
            return SIXEL_LOGIC_ERROR;
        }
        execution->delta_enabled = 1;
        execution->retained_rgb = baseline.rgb;
        execution->retained_rgb_size = baseline.rgb_stride * desc->plane_height;
        execution->retained_valid = baseline.valid;
        execution->retained_valid_size =
            baseline.valid_stride * desc->plane_height;
        execution->plane_width = desc->plane_width;
        execution->plane_height = desc->plane_height;
        execution->origin_x = desc->origin_x;
        execution->origin_y = desc->origin_y;
    }
    memset(apply->ready, 0, (size_t)desc->height);
    memset(execution->skip, 0, execution->skip_size);
    if (transparency.transparent_mask != NULL) {
        for (i = 0; i < execution->skip_size; ++i) {
            execution->skip[i] = transparency.transparent_mask[i] != 0;
        }
    }
    apply->running = 1;
    apply->run = request;
    apply->prefix = 0;
    apply->status = SIXEL_OK;
    sixel_indexed_frame_acquire_user(request->result);
    if (apply->gpu_dither != NULL) {
        memset(&gpu_run, 0, sizeof(gpu_run));
        gpu_run.source = request->source;
        gpu_run.baseline = request->baseline;
        gpu_run.result = request->result;
        gpu_run.delta = request->delta;
        gpu_run.logger = request->logger;
        gpu_run.cancel = request->cancel;
        gpu_run.cancel_context = request->cancel_context;
        if (request->rows_ready != NULL) {
            gpu_run.rows_ready = sixel_palette_apply_gpu_rows;
            gpu_run.rows_context = apply;
        }
        status = apply->gpu_dither->vtbl->run(apply->gpu_dither, &gpu_run);
        if (status == SIXEL_OK && request->cancel != NULL &&
                request->cancel(request->cancel_context)) {
            status = SIXEL_INTERRUPTED;
        }
        /* A streaming producer has already published its completed prefix.
         * Other producers are validated and published once on completion. */
        if (status == SIXEL_OK && apply->prefix < desc->height) {
            status = sixel_indexed_frame_publish_applied(request->result,
                desc->height);
            if (status == SIXEL_OK && request->rows_ready != NULL)
                status = request->rows_ready(request->result, desc->height,
                    request->rows_context);
        }
    } else {
        status = sixel_palette_apply_cpu(apply);
    }
    if (status == SIXEL_OK) {
        status = apply->status;
    }
    if (status == SIXEL_OK) {
        status = sixel_indexed_frame_complete(request->result);
    }
    if (status != SIXEL_OK) {
        sixel_indexed_frame_fail(request->result);
    }
    sixel_indexed_frame_release_user(request->result);
    apply->running = 0;
    apply->run = NULL;
    memset(execution, 0, sizeof(*execution));
    return status;
}

static SIXELSTATUS
sixel_palette_apply_get_device(sixel_palette_apply_interface_t const *self,
    sixel_gpu_device_interface_t **device)
{
    sixel_palette_apply_storage_t const *apply;

    if (self == NULL || device == NULL) return SIXEL_BAD_ARGUMENT;
    apply = (sixel_palette_apply_storage_t const *)self;
    if (!apply->prepared) return SIXEL_LOGIC_ERROR;
    *device = apply->gpu ? apply->plan.device : NULL;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_palette_apply_get_deferred(sixel_palette_apply_interface_t const *self,
    sixel_palette_apply_deferred_interface_t **deferred)
{
    sixel_palette_apply_storage_t const *apply;

    if (self == NULL || deferred == NULL) return SIXEL_BAD_ARGUMENT;
    *deferred = NULL;
    apply = (sixel_palette_apply_storage_t const *)self;
    if (!apply->prepared) return SIXEL_LOGIC_ERROR;
    if (apply->deferred == NULL) return SIXEL_FEATURE_ERROR;
    apply->deferred->vtbl->ref(apply->deferred);
    *deferred = apply->deferred;
    return SIXEL_OK;
}

static sixel_palette_apply_vtbl_t const g_palette_apply_vtbl = {
    sixel_palette_apply_ref,
    sixel_palette_apply_unref,
    sixel_palette_apply_prepare,
    sixel_palette_apply_run,
    sixel_palette_apply_get_device,
    sixel_palette_apply_get_deferred
};

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_apply_factory_new(sixel_allocator_t *allocator, void **object)
{
    sixel_palette_apply_storage_t *apply;

    if (allocator == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *object = NULL;
    apply = sixel_allocator_calloc(allocator, 1, sizeof(*apply));
    if (apply == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
#if SIXEL_ENABLE_THREADS
    if (sixel_mutex_init(&apply->row_gate) != 0) {
        sixel_allocator_free(allocator, apply);
        return SIXEL_RUNTIME_ERROR;
    }
#endif
    apply->iface.vtbl = &g_palette_apply_vtbl;
    apply->ref = 1;
    apply->allocator = allocator;
    sixel_allocator_ref(allocator);
    *object = &apply->iface;
    return SIXEL_OK;
}
