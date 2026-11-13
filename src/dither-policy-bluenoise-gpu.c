/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <string.h>
#include "dither-policy-bluenoise-gpu.h"
#include "gpu-palette.h"
#include "encode-result.h"
#include "palette.h"
#include "sixel_atomic.h"
#include "options.h"
#include "dither-common-pipeline.h"
#include "bluenoise_64x64.h"

#if defined(HAVE_METAL)
static char const *const d_bluenoise_prefix[] = {
#include <generated-metal/dither-policy-bluenoise-gpu-prefix-shader.inc.h>
};
static char const *const d_bluenoise_suffix[] = {
#include <generated-metal/dither-policy-bluenoise-gpu-shader.inc.h>
};
static char const *const d_bluenoise_legacy[] = {
#include <generated-metal/dither-policy-bluenoise-gpu-query-shader.inc.h>
};
#elif defined(HAVE_VULKAN)
static char const *const d_bluenoise_prefix[] = {
#include <generated-vulkan/dither-policy-bluenoise-gpu-prefix-shader.inc.h>
};
static char const *const d_bluenoise_suffix[] = {
#include <generated-vulkan/dither-policy-bluenoise-gpu-shader.inc.h>
};
#else
static char const *const d_bluenoise_prefix[] = { NULL };
static char const *const d_bluenoise_suffix[] = { NULL };
#endif
#if !defined(HAVE_METAL)
static char const *const d_bluenoise_legacy[] = { NULL };
#endif

typedef struct d_bluenoise_conf {
    float strength;
    float gradient_factor;
    int phase_x;
    int phase_y;
    int channel_rgb;
    int size;
} d_bluenoise_conf_t;

static unsigned int
d_bluenoise_hash32(unsigned int value)
{
    value ^= value >> 16;
    value = (unsigned int)((unsigned long long)value * 0x7feb352dU);
    value ^= value >> 15;
    value = (unsigned int)((unsigned long long)value * 0x846ca68bU);
    value ^= value >> 16;
    return value;
}

static void
d_bluenoise_bluenoise_conf_init(d_bluenoise_conf_t *conf,
                                     int ncolors)
{
    int seed;
    unsigned int hash;

    seed = 0;
    hash = 0U;
    if (conf == NULL) {
        return;
    }

    conf->strength = sixel_dither_positional_auto_strength(
        SIXEL_DIFFUSE_BLUENOISE_DITHER, ncolors);
    conf->gradient_factor = 0.0f;
    conf->phase_x = 0;
    conf->phase_y = 0;
    conf->channel_rgb = 0;
    conf->size = SIXEL_BN_W;

    (void)sixel_option_resolve_registered_float_binding(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        "bluenoise",
        SIXEL_SUBOPTION_BINDING_ID_2(
            bluenoise_strength,
            bluenoise_strength_override),
        &conf->strength);

    if (sixel_option_resolve_registered_float_binding(
            SIXEL_OPTION_SCHEMA_DIFFUSION,
            "bluenoise",
            SIXEL_SUBOPTION_BINDING_ID_2(
                bluenoise_gradient_factor,
                bluenoise_gradient_factor_override),
            &conf->gradient_factor)) {
        if (conf->gradient_factor < 0.0f) {
            conf->gradient_factor = 0.0f;
        }
    }

    (void)sixel_option_resolve_registered_int_binding(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        "bluenoise",
        SIXEL_SUBOPTION_BINDING_ID_2(
            bluenoise_channel_rgb,
            bluenoise_channel_override),
        &conf->channel_rgb);

    (void)sixel_option_resolve_registered_int_binding(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        "bluenoise",
        SIXEL_SUBOPTION_BINDING_ID_2(
            bluenoise_size,
            bluenoise_size_override),
        &conf->size);

    if (sixel_option_resolve_registered_int_pair_binding(
            SIXEL_OPTION_SCHEMA_DIFFUSION,
            "bluenoise",
            SIXEL_SUBOPTION_BINDING_ID_3(
                bluenoise_phase_x,
                bluenoise_phase_y,
                bluenoise_phase_override),
            &conf->phase_x,
            &conf->phase_y)) {
        return;
    }

    if (sixel_option_resolve_registered_int_binding(
            SIXEL_OPTION_SCHEMA_DIFFUSION,
            "bluenoise",
            SIXEL_SUBOPTION_BINDING_ID_2(
                bluenoise_seed,
                bluenoise_seed_override),
            &seed)) {
        hash = d_bluenoise_hash32((unsigned int)seed);
        conf->phase_x = (int)(hash & 63U);
        conf->phase_y = (int)((hash >> 8) & 63U);
    }
}

static void
d_bluenoise_apply_bluenoise_overrides(
    d_bluenoise_conf_t *conf,
    sixel_gpu_palette_request_t const *request)
{
    unsigned int hash;

    hash = 0U;
    if (conf == NULL || request == NULL) {
        return;
    }

    if (request->bluenoise_strength_override != 0) {
        conf->strength = request->bluenoise_strength;
    }
    if (request->bluenoise_gradient_factor_override != 0) {
        conf->gradient_factor = request->bluenoise_gradient_factor;
        if (conf->gradient_factor < 0.0f) {
            conf->gradient_factor = 0.0f;
        }
    }
    if (request->bluenoise_channel_override != 0) {
        conf->channel_rgb =
            request->bluenoise_channel_rgb != 0 ? 1 : 0;
    }
    if (request->bluenoise_size_override != 0 &&
            request->bluenoise_size == SIXEL_BN_W) {
        conf->size = request->bluenoise_size;
    }

    if (request->bluenoise_phase_override != 0) {
        conf->phase_x = request->bluenoise_phase_x;
        conf->phase_y = request->bluenoise_phase_y;
    } else if (request->bluenoise_seed_override != 0) {
        hash = d_bluenoise_hash32(
            (unsigned int)request->bluenoise_seed);
        conf->phase_x = (int)(hash & 63U);
        conf->phase_y = (int)((hash >> 8) & 63U);
    }
}

static SIXELSTATUS
d_bluenoise_resolve(sixel_gpu_palette_request_t *effective,
    sixel_gpu_palette_request_t const *request)
{
    d_bluenoise_conf_t conf;

    *effective = *request;
    memset(&conf, 0, sizeof(conf));
    d_bluenoise_bluenoise_conf_init(&conf, request->ncolors);
    d_bluenoise_apply_bluenoise_overrides(&conf, request);
    effective->bluenoise_strength = conf.strength;
    effective->bluenoise_gradient_factor = conf.gradient_factor;
    effective->bluenoise_phase_x = conf.phase_x;
    effective->bluenoise_phase_y = conf.phase_y;
    effective->bluenoise_channel_rgb = conf.channel_rgb;
    effective->bluenoise_size = conf.size;
    if (conf.size != SIXEL_BN_W) {
        sixel_helper_set_additional_message(
            "gpu palette apply: blue-noise size is not supported.");
        return SIXEL_BAD_ARGUMENT;
    }
    return SIXEL_OK;
}

sixel_gpu_dither_program_t const sixel_dither_bluenoise_gpu_program = {
    "bluenoise",
    d_bluenoise_prefix, sizeof(d_bluenoise_prefix) / sizeof(char const *),
    d_bluenoise_suffix, sizeof(d_bluenoise_suffix) / sizeof(char const *),
    d_bluenoise_legacy, sizeof(d_bluenoise_legacy) / sizeof(char const *),
    d_bluenoise_resolve
};

/* Prepared options and lookup code are frozen per plan. The transport
 * only transfers buffers and runs this policy's supplied shader code. */
typedef struct d_bluenoise_plan {
    sixel_gpu_dither_plan_interface_t iface;
    sixel_atomic_u32_t ref, busy;
    sixel_allocator_t *allocator;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_program_t *program;
    sixel_indexed_frame_description_t output;
    sixel_dither_policy_prepare_request_t configuration;
    unsigned char palette[768], comparison[768];
} d_bluenoise_plan_t;

typedef struct d_bluenoise_preparer {
    sixel_gpu_dither_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_dither_policy_prepare_request_t configuration;
    int configured;
} d_bluenoise_preparer_t;

static void
d_bluenoise_plan_ref(sixel_gpu_dither_plan_interface_t *self)
{
    d_bluenoise_plan_t *plan;

    plan = (d_bluenoise_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
d_bluenoise_plan_unref(sixel_gpu_dither_plan_interface_t *self)
{
    d_bluenoise_plan_t *plan;
    sixel_allocator_t *allocator;

    plan = (d_bluenoise_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) return;
    allocator = plan->allocator;
    sixel_gpu_program_destroy(plan->program);
    plan->device->vtbl->unref(plan->device);
    sixel_wire_palette_unref(plan->output.palette);
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
d_bluenoise_execute(d_bluenoise_plan_t *plan,
    sixel_gpu_dither_run_request_t const *run)
{
    sixel_gpu_palette_request_t request;
    sixel_frame_pixels_view_t source;
    sixel_frame_transparency_t transparency;
    sixel_retained_read_view_t baseline;
    sixel_retained_gpu_read_view_t retained;
    sixel_delta_mask_t selection;
    sixel_indexed_frame_description_t const *desc;
    size_t cells;
    SIXELSTATUS status;

    if (run == NULL || run->source == NULL || run->result == NULL)
        return SIXEL_BAD_ARGUMENT;
    if (sixel_indexed_frame_get_state(run->result) != SIXEL_INDEXED_BUILDING)
        return SIXEL_LOGIC_ERROR;
    desc = &run->result->description;
    if (desc->width != plan->output.width ||
            desc->height != plan->output.height ||
            desc->palette != plan->output.palette ||
            desc->keycolor != plan->output.keycolor ||
            desc->keycolor_reserved != plan->output.keycolor_reserved ||
            desc->transparent_background !=
                plan->output.transparent_background ||
            desc->encoded_x != plan->output.encoded_x ||
            desc->encoded_y != plan->output.encoded_y ||
            desc->encoded_width != plan->output.encoded_width ||
            desc->encoded_height != plan->output.encoded_height ||
            desc->origin_x != plan->output.origin_x ||
            desc->origin_y != plan->output.origin_y ||
            desc->plane_width != plan->output.plane_width ||
            desc->plane_height != plan->output.plane_height)
        return SIXEL_BAD_ARGUMENT;
    if (run->cancel != NULL && run->cancel(run->cancel_context))
        return SIXEL_INTERRUPTED;
    status = run->source->vtbl->get_pixels(run->source, &source);
    if (status != SIXEL_OK) return status;
    if (source.width != desc->width || source.height != desc->height ||
            source.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            source.colorspace != SIXEL_COLORSPACE_GAMMA)
        return SIXEL_BAD_ARGUMENT;
    status = run->source->vtbl->get_transparency(run->source, &transparency);
    if (status != SIXEL_OK) return status;
    memset(&request, 0, sizeof(request));
    memset(&selection, 0, sizeof(selection));
    cells = (size_t)run->delta.selection.columns * run->delta.selection.rows;
    if (run->delta.selection.columns > 64 ||
            run->delta.selection.rows > 64 || cells > 4096 ||
            (cells != 0 && (run->delta.selection.words == NULL ||
             run->delta.selection.word_count < (cells + 31) / 32)))
        return SIXEL_BAD_ARGUMENT;
    if (cells != 0) {
        memcpy(selection.cells, run->delta.selection.words,
            ((cells + 31) / 32) * sizeof(uint32_t));
        selection.columns = run->delta.selection.columns;
        selection.rows = run->delta.selection.rows;
        selection.width = run->delta.selection.width;
        selection.height = run->delta.selection.height;
        selection.tiled = run->delta.selection.tiled;
        selection.threshold = run->delta.selection.selected_threshold;
    }
    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.program = plan->program;
    request.dither_program = &sixel_dither_bluenoise_gpu_program;
    request.dest = run->result->indexes;
    request.pixels = source.pixels;
    request.pixel_count = run->result->pixels;
    request.width = source.width;
    request.height = source.height;
    request.pixelformat = source.pixelformat;
    request.palette = plan->palette;
    request.ncolors = plan->configuration.options.ncolors -
        (plan->configuration.options.keycolor_reserved != 0);
    request.palette_size = (size_t)request.ncolors * 3;
    request.palette_depth = 3;
    request.method_for_diffuse = SIXEL_DIFFUSE_BLUENOISE_DITHER;
    request.method_for_scan = plan->configuration.method_for_scan;
    request.cancel = run->cancel;
    request.cancel_context = run->cancel_context;
    request.rows_ready = run->rows_ready;
    request.rows_context = run->rows_context;
    if (run->rows_ready != NULL)
        request.rows_per_chunk = sixel_gpu_palette_rows_per_chunk(
            source.width, source.height);
    request.transparent_mask = transparency.transparent_mask;
    request.transparent_mask_size = transparency.transparent_mask_size;
    request.transparent_keycolor = desc->keycolor;
    if (run->baseline != NULL && run->delta.enabled) {
        status = sixel_retained_read_get_view(run->baseline, &baseline);
        if (status != SIXEL_OK) return status;
        if (run->result->identity.stream != baseline.stream ||
                run->result->identity.epoch != baseline.epoch ||
                run->result->identity.base_sequence != baseline.sequence ||
                desc->plane_width != baseline.description.width ||
                desc->plane_height != baseline.description.height)
            return SIXEL_LOGIC_ERROR;
        request.sixdelta_enabled = request.has_6delta_accumulation = 1;
        request.accumulation_pixels = baseline.rgb;
        request.accumulation_pixels_size = baseline.rgb_stride *
            desc->plane_height;
        request.accumulation_valid_mask = baseline.valid;
        request.accumulation_valid_mask_size = baseline.valid_stride *
            desc->plane_height;
        request.accumulation_width = desc->plane_width;
        request.accumulation_height = desc->plane_height;
        request.accumulation_origin_x = desc->origin_x;
        request.accumulation_origin_y = desc->origin_y;
        status = sixel_retained_read_get_gpu_view(run->baseline,
            plan->device, &retained);
        if (status == SIXEL_OK) {
            request.retained_device = plan->device;
            request.retained_rgb = retained.rgb;
            request.retained_valid = retained.valid;
        } else if (status != SIXEL_FEATURE_ERROR) return status;
    }
    request.accumulation_keycolor = desc->keycolor;
    request.sixdelta_threshold = run->delta.threshold;
    request.sixdelta_metric = run->delta.metric;
    request.sixdelta_compare = run->delta.compare;
    request.residual_mask = &selection;
    request.sixdelta_comparison_palette = plan->comparison;
    request.sixdelta_comparison_palette_size = request.palette_size;
    request.accumulation_result_mask = run->result->skip;
    request.accumulation_result_mask_size = run->result->pixels;
    request.bluenoise_strength_override = 1;
    request.bluenoise_strength = plan->configuration.options.bluenoise_strength;
    request.bluenoise_phase_override = 1;
    request.bluenoise_phase_x = plan->configuration.options.bluenoise_phase_x;
    request.bluenoise_phase_y = plan->configuration.options.bluenoise_phase_y;
    request.bluenoise_channel_override = 1;
    request.bluenoise_channel_rgb =
        plan->configuration.options.bluenoise_channel_rgb;
    request.bluenoise_size_override = 1;
    request.bluenoise_size = plan->configuration.options.bluenoise_size;
    request.bluenoise_gradient_factor_override = 1;
    request.bluenoise_gradient_factor = 0;
    /* Settings are already resolved. No environment lookup is permitted
     * after preparation, and failed GPU output is never retried on CPU. */
    return sixel_gpu_palette_apply_prepared(&request);
}

static SIXELSTATUS
d_bluenoise_run(sixel_gpu_dither_plan_interface_t *self,
    sixel_gpu_dither_run_request_t const *request)
{
    d_bluenoise_plan_t *plan;
    SIXELSTATUS status;

    plan = (d_bluenoise_plan_t *)self;
    if (sixel_atomic_fetch_add_u32(&plan->busy, 1) != 0) {
        (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
        return SIXEL_LOGIC_ERROR;
    }
    status = d_bluenoise_execute(plan, request);
    (void)sixel_atomic_fetch_sub_u32(&plan->busy, 1);
    return status;
}

static SIXELSTATUS
d_bluenoise_deferred(sixel_gpu_dither_plan_interface_t const *self,
    sixel_gpu_dither_deferred_interface_t **result)
{
    (void)self;
    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    return SIXEL_FEATURE_ERROR;
}

static sixel_gpu_dither_plan_vtbl_t const d_bluenoise_plan_vtbl = {
    d_bluenoise_plan_ref, d_bluenoise_plan_unref,
    d_bluenoise_run, d_bluenoise_deferred
};

static void
d_bluenoise_ref(sixel_gpu_dither_preparer_interface_t *self)
{
    d_bluenoise_preparer_t *preparer;

    preparer = (d_bluenoise_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
d_bluenoise_unref(sixel_gpu_dither_preparer_interface_t *self)
{
    d_bluenoise_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (d_bluenoise_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->allocator;
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
d_bluenoise_configure(sixel_gpu_dither_preparer_interface_t *self,
    sixel_dither_policy_prepare_request_t const *request)
{
    d_bluenoise_preparer_t *preparer;

    if (request == NULL ||
            request->options.method_for_diffuse !=
                SIXEL_DIFFUSE_BLUENOISE_DITHER ||
            request->options.ncolors < 1 || request->options.ncolors > 256 ||
            (request->options.keycolor_reserved &&
             request->options.ncolors < 2)) return SIXEL_BAD_ARGUMENT;
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            (request->method_for_scan != SIXEL_SCAN_RASTER &&
             request->method_for_scan != SIXEL_SCAN_SERPENTINE))
        return SIXEL_FEATURE_ERROR;
    if (!isfinite(request->options.bluenoise_strength) ||
            !isfinite(request->options.bluenoise_gradient_factor))
        return SIXEL_BAD_ARGUMENT;
    /* Gradient maps stay in the legacy frame adapter until the typed
     * request can carry that analysis without dropping modulation. */
    if (request->options.bluenoise_size != SIXEL_BN_W ||
            request->options.bluenoise_gradient_factor != 0)
        return SIXEL_FEATURE_ERROR;
    preparer = (d_bluenoise_preparer_t *)self;
    preparer->configuration = *request;
    preparer->configured = 1;
    return SIXEL_OK;
}

static SIXELSTATUS
d_bluenoise_configuration(sixel_gpu_dither_preparer_interface_t const *self,
    sixel_dither_policy_prepare_request_t *request)
{
    d_bluenoise_preparer_t const *preparer;

    if (request == NULL) return SIXEL_BAD_ARGUMENT;
    preparer = (d_bluenoise_preparer_t const *)self;
    if (!preparer->configured) return SIXEL_LOGIC_ERROR;
    *request = preparer->configuration;
    return SIXEL_OK;
}

static SIXELSTATUS
d_bluenoise_prepare(sixel_gpu_dither_preparer_interface_t const *self,
    sixel_gpu_dither_prepare_request_t const *request,
    sixel_gpu_dither_plan_interface_t **result)
{
    d_bluenoise_preparer_t const *preparer;
    d_bluenoise_plan_t *plan;
    sixel_gpu_lookup_plan_interface_t *lookup;
    sixel_lookup_execution_request_t execution;
    sixel_palette_entries_view_t entries;
    sixel_wire_palette_view_t wire;
    size_t i;
    SIXELSTATUS status;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    preparer = (d_bluenoise_preparer_t const *)self;
    if (!preparer->configured) return SIXEL_LOGIC_ERROR;
    if (request == NULL || request->lookup == NULL ||
            request->device == NULL || request->working_palette == NULL ||
            request->output.palette == NULL || request->output.width <= 0 ||
            request->output.height <= 0) return SIXEL_BAD_ARGUMENT;
    if (request->pixelformat != preparer->configuration.pixelformat ||
            request->scan != preparer->configuration.method_for_scan ||
            request->colorspace != SIXEL_COLORSPACE_GAMMA)
        return SIXEL_FEATURE_ERROR;
    if (request->output.keycolor_reserved !=
            preparer->configuration.options.keycolor_reserved ||
            (request->output.keycolor_reserved &&
             request->output.keycolor !=
                preparer->configuration.options.ncolors - 1))
        return SIXEL_BAD_ARGUMENT;
    status = request->working_palette->vtbl->get_entries(
        request->working_palette, &entries);
    if (status != SIXEL_OK) return status;
    status = sixel_wire_palette_get_view(request->output.palette, &wire);
    if (status != SIXEL_OK) return status;
    if (entries.entries == NULL || entries.depth != 3 ||
            entries.entry_count !=
                (unsigned)preparer->configuration.options.ncolors ||
            entries.entries_size < entries.entry_count * 3 ||
            wire.count != entries.entry_count) return SIXEL_BAD_ARGUMENT;
    plan = sixel_allocator_calloc(preparer->allocator, 1, sizeof(*plan));
    if (plan == NULL) return SIXEL_BAD_ALLOCATION;
    plan->iface.vtbl = &d_bluenoise_plan_vtbl;
    plan->ref = 1;
    plan->allocator = preparer->allocator;
    sixel_allocator_ref(plan->allocator);
    plan->device = request->device;
    plan->device->vtbl->ref(plan->device);
    plan->configuration = preparer->configuration;
    plan->output = request->output;
    sixel_wire_palette_ref(plan->output.palette);
    memcpy(plan->palette, entries.entries, entries.entry_count * 3);
    for (i = 0; i < wire.count; ++i)
        memcpy(plan->comparison + i * 3, wire.colors[i].emitted_rgb, 3);
    memset(&execution, 0, sizeof(execution));
    execution.working_palette = request->working_palette;
    execution.ncolors = entries.entry_count -
        (request->output.keycolor_reserved != 0);
    execution.pixelformat = request->pixelformat;
    execution.colorspace = request->colorspace;
    execution.query_kind = SIXEL_GPU_LOOKUP_QUERY_BYTE;
    execution.allow_batch = 1;
    lookup = NULL;
    status = request->lookup->vtbl->prepare(request->lookup, &execution,
        request->device, &lookup);
    if (status == SIXEL_OK && lookup == NULL) status = SIXEL_LOGIC_ERROR;
    if (status == SIXEL_OK)
        status = sixel_gpu_program_prepare(request->device, lookup,
            &sixel_dither_bluenoise_gpu_program, &plan->program);
    if (lookup != NULL) lookup->vtbl->unref(lookup);
    if (status != SIXEL_OK) {
        d_bluenoise_plan_unref(&plan->iface);
        return status;
    }
    *result = &plan->iface;
    return SIXEL_OK;
}

static sixel_gpu_dither_preparer_vtbl_t const d_bluenoise_vtbl = {
    d_bluenoise_ref, d_bluenoise_unref, d_bluenoise_configure,
    d_bluenoise_configuration, d_bluenoise_prepare
};

/* Ownership escapes through the returned component interface. */
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_dither_policy_bluenoise_gpu_new(sixel_allocator_t *allocator,
    void **result)
{
    d_bluenoise_preparer_t *preparer;

    if (allocator == NULL || result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    preparer = sixel_allocator_calloc(allocator, 1, sizeof(*preparer));
    if (preparer == NULL) return SIXEL_BAD_ALLOCATION;
    preparer->iface.vtbl = &d_bluenoise_vtbl;
    preparer->ref = 1;
    preparer->allocator = allocator;
    sixel_allocator_ref(allocator);
    *result = &preparer->iface;
    return SIXEL_OK;
}
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic pop
#endif
