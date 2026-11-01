/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <limits.h>
#include <string.h>
#include "encode-result.h"
#include "dither-policy-private.h"
#include "gpu-palette.h"
#include "threading.h"
#include "pixelformat.h"
#include "timeline-logger.h"

typedef struct sixel_palette_apply_storage {
    sixel_palette_apply_interface_t interface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_palette_apply_prepare_request_t plan;
    sixel_dither_policy_options_t options;
    sixel_mutex_t row_gate;
    unsigned char *ready;
    unsigned char comparison[SIXEL_PALETTE_MAX * 3];
    int lookup_kind;
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
        apply->plan.lookup->vtbl->unref(apply->plan.lookup);
        apply->plan.dither->vtbl->unref(apply->plan.dither);
        sixel_wire_palette_unref(apply->plan.output.palette);
    }
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

static SIXELSTATUS
sixel_palette_apply_prepare(sixel_palette_apply_interface_t *self,
    sixel_palette_apply_prepare_request_t const *request)
{
    sixel_palette_apply_storage_t *apply;
    sixel_dither_policy_options_t options;
    sixel_dither_policy_prepare_request_t configuration;
    sixel_wire_palette_view_t palette;
    unsigned char *ready;
    size_t pixels;
    size_t i;
    int kind;
    int gpu;
    SIXELSTATUS status;

    if (self == NULL || request == NULL || request->working_palette == NULL ||
            request->lookup == NULL || request->dither == NULL ||
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
    status = request->dither->vtbl->get_configuration(request->dither,
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
    options = configuration.options;
    pixels = (size_t)request->output.width * request->output.height;
    kind = request->lookup->vtbl->get_kind != NULL
        ? request->lookup->vtbl->get_kind(request->lookup) : -1;
    gpu = request->pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
        request->output.origin_x == 0 && request->output.origin_y == 0 &&
        request->output.plane_width == request->output.width &&
        request->output.plane_height == request->output.height &&
        options.bluenoise_gradient_factor == 0 &&
        (kind == SIXEL_LUT_POLICY_NONE || kind == SIXEL_LUT_POLICY_EYTZINGER ||
         kind == SIXEL_LUT_POLICY_BLOCKCULL ||
         kind == SIXEL_LUT_POLICY_GRIDLUT ||
         kind == SIXEL_LUT_POLICY_ORDERED) &&
        (options.method_for_diffuse == SIXEL_DIFFUSE_NONE ||
         options.method_for_diffuse == SIXEL_DIFFUSE_A_DITHER ||
         options.method_for_diffuse == SIXEL_DIFFUSE_X_DITHER ||
         options.method_for_diffuse == SIXEL_DIFFUSE_BLUENOISE_DITHER);
    if (request->gpu_policy == SIXEL_GPU_POLICY_OFF ||
            (request->gpu_policy == SIXEL_GPU_POLICY_AUTO &&
             pixels < request->gpu_auto_threshold)) {
        gpu = 0;
    } else if (gpu) {
        gpu = sixel_gpu_palette_plan_available();
    }
    if (!gpu && request->gpu_policy == SIXEL_GPU_POLICY_FORCE) {
        return SIXEL_FEATURE_ERROR;
    }
    /* Gradient analysis has its own prepared source map in the legacy
     * adapter. Do not silently discard a requested modulation. */
    if (options.bluenoise_gradient_factor != 0) {
        return SIXEL_FEATURE_ERROR;
    }
    ready = sixel_allocator_calloc(apply->allocator,
        (size_t)request->output.height, 1);
    if (ready == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_wire_palette_get_view(request->output.palette, &palette);
    if (palette.count != (size_t)options.ncolors ||
            options.keycolor_reserved != request->output.keycolor_reserved ||
            (options.keycolor_reserved && (options.ncolors < 2 ||
             request->output.keycolor != options.ncolors - 1))) {
        sixel_allocator_free(apply->allocator, ready);
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_palette_apply_clear_plan(apply);
    apply->plan = *request;
    apply->options = options;
    apply->lookup_kind = kind;
    apply->gpu = gpu;
    apply->ready = ready;
    request->working_palette->vtbl->ref(request->working_palette);
    request->lookup->vtbl->ref(request->lookup);
    request->dither->vtbl->ref(request->dither);
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
    request.lookup_policy = apply->plan.lookup;
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
sixel_palette_apply_gpu(sixel_palette_apply_storage_t *apply)
{
    sixel_gpu_palette_request_t request;
    sixel_dither_execution_t const *execution;
    sixel_delta_mask_t selection;
    size_t cells;
    SIXELSTATUS status;

    execution = &apply->execution;
    memset(&request, 0, sizeof(request));
    memset(&selection, 0, sizeof(selection));
    cells = (size_t)execution->selection.columns * execution->selection.rows;
    if (cells != 0) {
        memcpy(selection.cells, execution->selection.words,
            ((cells + 31) / 32) * sizeof(uint32_t));
        selection.columns = execution->selection.columns;
        selection.rows = execution->selection.rows;
        selection.width = execution->selection.width;
        selection.height = execution->selection.height;
        selection.tiled = execution->selection.tiled;
        selection.threshold = execution->selection.selected_threshold;
    }
    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.dest = apply->run->result->indexes;
    request.pixels = apply->pixels.pixels;
    request.pixel_count = apply->run->result->pixels;
    request.width = apply->pixels.width;
    request.height = apply->pixels.height;
    request.pixelformat = apply->pixels.pixelformat;
    request.palette = apply->entries.entries;
    request.palette_size = apply->entries.entries_size;
    request.palette_depth = apply->entries.depth;
    request.ncolors = apply->options.ncolors -
        (apply->options.keycolor_reserved != 0);
    request.lut_policy = apply->lookup_kind;
    request.method_for_diffuse = apply->options.method_for_diffuse;
    request.method_for_scan = apply->plan.scan;
    request.transparent_mask = execution->transparent_mask;
    request.transparent_mask_size = execution->transparent_size;
    request.transparent_keycolor = execution->keycolor;
    request.sixdelta_enabled = execution->delta_enabled;
    request.has_6delta_accumulation = execution->delta_enabled;
    request.accumulation_pixels = execution->retained_rgb;
    request.accumulation_pixels_size = execution->retained_rgb_size;
    request.accumulation_valid_mask = execution->retained_valid;
    request.accumulation_valid_mask_size = execution->retained_valid_size;
    request.accumulation_keycolor = execution->delta_keycolor;
    request.sixdelta_threshold = execution->delta_threshold;
    request.sixdelta_metric = execution->delta_metric;
    request.residual_mask = &selection;
    request.sixdelta_comparison_palette = apply->comparison;
    request.sixdelta_comparison_palette_size = apply->entries.entry_count * 3;
    request.accumulation_result_mask = execution->skip;
    request.accumulation_result_mask_size = execution->skip_size;
    request.positional_strength_override = 1;
    request.positional_strength =
        apply->options.method_for_diffuse == SIXEL_DIFFUSE_A_DITHER
            ? apply->options.a_dither_strength
            : apply->options.x_dither_strength;
    request.bluenoise_strength_override = 1;
    request.bluenoise_strength = apply->options.bluenoise_strength;
    request.bluenoise_phase_override = 1;
    request.bluenoise_phase_x = apply->options.bluenoise_phase_x;
    request.bluenoise_phase_y = apply->options.bluenoise_phase_y;
    request.bluenoise_channel_override = 1;
    request.bluenoise_channel_rgb = apply->options.bluenoise_channel_rgb;
    request.bluenoise_size_override = 1;
    request.bluenoise_size = apply->options.bluenoise_size;
    request.bluenoise_gradient_factor_override = 1;
    request.bluenoise_gradient_factor = 0;
    status = sixel_gpu_palette_apply_prepared(&request);
    if (status != SIXEL_OK) {
        return SIXEL_FAILED(status) ? status : SIXEL_RUNTIME_ERROR;
    }
    /* The command buffer completed the whole image. Publish it once instead
     * of emulating CPU row callbacks and repeating their locks and scans. */
    if (apply->run->cancel != NULL &&
            apply->run->cancel(apply->run->cancel_context)) {
        return SIXEL_INTERRUPTED;
    }
    status = apply->plan.output.keycolor < 0 ||
            apply->plan.output.keycolor_reserved
        ? sixel_indexed_frame_publish_gpu(apply->run->result)
        : sixel_indexed_frame_publish_applied(apply->run->result,
            apply->pixels.height);
    if (status == SIXEL_OK && apply->run->rows_ready != NULL) {
        status = apply->run->rows_ready(apply->run->result,
            apply->pixels.height, apply->run->rows_context);
    }
    return status;
}

static SIXELSTATUS
sixel_palette_apply_run(sixel_palette_apply_interface_t *self,
    sixel_palette_apply_run_request_t const *request)
{
    sixel_palette_apply_storage_t *apply;
    sixel_indexed_frame_description_t const *desc;
    sixel_frame_transparency_t transparency;
    sixel_retained_read_view_t baseline;
    sixel_dither_execution_t *execution;
    size_t i;
    size_t cells;
    SIXELSTATUS status;

    if (self == NULL || request == NULL || request->source == NULL ||
            request->result == NULL) {
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
    execution->delta_error_mode = request->delta.error_mode;
    execution->delta_threshold = request->delta.threshold;
    /* An explicit emitted table also covers legacy HLS and percentage RGB
     * without perturbing the working palette used for nearest lookup. */
    execution->delta_hls = 1;
    execution->hls_rgb = apply->comparison;
    execution->hls_count = (unsigned int)apply->entries.entry_count;
    if (request->baseline != NULL && request->delta.enabled) {
        sixel_retained_read_get_view(request->baseline, &baseline);
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
    status = apply->gpu ? sixel_palette_apply_gpu(apply) :
        sixel_palette_apply_cpu(apply);
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

static sixel_palette_apply_vtbl_t const g_palette_apply_vtbl = {
    sixel_palette_apply_ref,
    sixel_palette_apply_unref,
    sixel_palette_apply_prepare,
    sixel_palette_apply_run
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
    apply->interface.vtbl = &g_palette_apply_vtbl;
    apply->ref = 1;
    apply->allocator = allocator;
    sixel_allocator_ref(allocator);
    *object = &apply->interface;
    return SIXEL_OK;
}
