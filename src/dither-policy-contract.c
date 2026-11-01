/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "dither-policy-private.h"
#include "dither-common-pipeline.h"
#include "options.h"

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_policy_get_configuration(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_prepare_request_t *configuration)
{
    sixel_dither_policy_object_t const *object;

    if (policy == NULL || configuration == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    object = (sixel_dither_policy_object_t const *)policy;
    if (!object->prepared) {
        return SIXEL_LOGIC_ERROR;
    }
    configuration->options = object->options;
    configuration->method_for_scan = object->method_for_scan;
    configuration->pixelformat = object->pixelformat;
    return SIXEL_OK;
}

SIXEL_INTERNAL_API void
sixel_dither_policy_capture_options(sixel_dither_t const *dither,
    sixel_dither_policy_options_t *options)
{
    options->method_for_diffuse = dither->method_for_diffuse;
    options->ncolors = dither->ncolors;
    options->keycolor_reserved = dither->keycolor_reserved;
    options->use_rgb101_palette = dither->use_rgb101_palette;
    options->clamp_policy = dither->clamp_policy;
    options->clamp_threshold = dither->clamp_threshold;
    options->diffusion_deadzone = dither->diffusion_deadzone;
    options->diffusion_perturb = dither->diffusion_perturb;
    options->diffusion_perturb_seed = dither->diffusion_perturb_seed;
    options->a_dither_strength = dither->a_dither_strength;
    options->x_dither_strength = dither->x_dither_strength;
    options->bluenoise_strength = dither->bluenoise_strength;
    options->bluenoise_phase_x = dither->bluenoise_phase_x;
    options->bluenoise_phase_y = dither->bluenoise_phase_y;
    options->bluenoise_channel_rgb = dither->bluenoise_channel_rgb;
    options->bluenoise_size = dither->bluenoise_size;
    options->bluenoise_gradient_factor = dither->bluenoise_gradient_factor;
    if (!dither->a_dither_strength_override) {
        options->a_dither_strength = sixel_dither_positional_auto_strength(
            SIXEL_DIFFUSE_A_DITHER, sixel_dither_lookup_ncolors(dither));
        (void)sixel_option_resolve_registered_float_binding(
            SIXEL_OPTION_SCHEMA_DIFFUSION, "a_dither",
            SIXEL_SUBOPTION_BINDING_ID_2(a_dither_strength,
                a_dither_strength_override), &options->a_dither_strength);
    }
    if (!dither->x_dither_strength_override) {
        options->x_dither_strength = sixel_dither_positional_auto_strength(
            SIXEL_DIFFUSE_X_DITHER, sixel_dither_lookup_ncolors(dither));
        (void)sixel_option_resolve_registered_float_binding(
            SIXEL_OPTION_SCHEMA_DIFFUSION, "x_dither",
            SIXEL_SUBOPTION_BINDING_ID_2(x_dither_strength,
                x_dither_strength_override), &options->x_dither_strength);
    }
    sixel_bluenoise_prepare_options(dither, options);
}

SIXEL_INTERNAL_API void
sixel_dither_policy_apply_options(sixel_dither_t *dither,
    sixel_dither_policy_options_t const *options)
{
    dither->method_for_diffuse = options->method_for_diffuse;
    dither->ncolors = options->ncolors;
    dither->keycolor_reserved = options->keycolor_reserved;
    dither->use_rgb101_palette = options->use_rgb101_palette;
    dither->clamp_policy = options->clamp_policy;
    dither->clamp_threshold = options->clamp_threshold;
    dither->diffusion_deadzone = options->diffusion_deadzone;
    dither->diffusion_perturb = options->diffusion_perturb;
    dither->diffusion_perturb_seed = options->diffusion_perturb_seed;
    dither->a_dither_strength = options->a_dither_strength;
    dither->x_dither_strength = options->x_dither_strength;
    dither->bluenoise_strength = options->bluenoise_strength;
    dither->bluenoise_phase_x = options->bluenoise_phase_x;
    dither->bluenoise_phase_y = options->bluenoise_phase_y;
    dither->bluenoise_channel_rgb = options->bluenoise_channel_rgb;
    dither->bluenoise_size = options->bluenoise_size;
    dither->bluenoise_gradient_factor = options->bluenoise_gradient_factor;
    dither->a_dither_strength_override = 1;
    dither->x_dither_strength_override = 1;
    dither->bluenoise_strength_override = 1;
    dither->bluenoise_gradient_factor_override = 1;
    dither->bluenoise_channel_override = 1;
    dither->bluenoise_phase_override = 1;
    dither->bluenoise_size_override = 1;
}

SIXEL_INTERNAL_API void
sixel_dither_policy_capture_execution(sixel_dither_t const *dither,
    sixel_dither_execution_t *execution)
{
    memset(execution, 0, sizeof(*execution));
    execution->working_palette = dither->palette;
    execution->allocator = dither->allocator;
    execution->indexes = dither->pipeline_index_buffer;
    execution->width = dither->pipeline_image_width;
    execution->height = dither->pipeline_image_height;
    execution->transparent_mask = dither->pipeline_transparent_mask;
    execution->transparent_size = dither->pipeline_transparent_mask_size;
    execution->keycolor = dither->pipeline_transparent_keycolor;
    execution->retained_rgb = dither->pipeline_accumulation_pixels;
    execution->retained_rgb_size = dither->pipeline_accumulation_pixels_size;
    execution->retained_valid = dither->pipeline_accumulation_valid_mask;
    execution->retained_valid_size =
        dither->pipeline_accumulation_valid_mask_size;
    execution->plane_width = dither->pipeline_accumulation_width;
    execution->plane_height = dither->pipeline_accumulation_height;
    execution->origin_x = dither->pipeline_accumulation_origin_x;
    execution->origin_y = dither->pipeline_accumulation_origin_y;
    execution->delta_enabled = dither->pipeline_6delta_enabled;
    execution->delta_threshold = dither->pipeline_6delta_threshold;
    execution->delta_metric = dither->pipeline_6delta_metric;
    execution->delta_error_mode = dither->pipeline_6delta_error_mode;
    execution->delta_keycolor = dither->pipeline_accumulation_keycolor;
    execution->delta_rgb101 = dither->pipeline_6delta_rgb101;
    execution->delta_hls = dither->pipeline_6delta_hls;
    execution->hls_rgb = dither->pipeline_6delta_hls_bytes;
    execution->hls_count = dither->pipeline_6delta_hls_count;
    execution->skip = dither->pipeline_accumulation_result_mask;
    execution->skip_size = dither->pipeline_accumulation_result_mask_size;
    execution->gradient = dither->bluenoise_gradient_map;
    execution->gradient_size = dither->bluenoise_gradient_map_size;
    execution->gradient_width = dither->bluenoise_gradient_width;
    execution->gradient_height = dither->bluenoise_gradient_height;
    execution->row_ready = dither->pipeline_row_callback;
    execution->row_context = dither->pipeline_row_priv;
    execution->logger = dither->pipeline_logger;
    execution->band_height = dither->pipeline_band_height;
    execution->selection.words = dither->pipeline_residual_mask.cells;
    execution->selection.word_count = 128;
    execution->selection.columns = dither->pipeline_residual_mask.columns;
    execution->selection.rows = dither->pipeline_residual_mask.rows;
    execution->selection.width = dither->pipeline_residual_mask.width;
    execution->selection.height = dither->pipeline_residual_mask.height;
    execution->selection.tiled = dither->pipeline_residual_mask.tiled;
    execution->selection.selected_threshold =
        dither->pipeline_residual_mask.threshold;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_dither_policy_bind(sixel_dither_policy_object_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    sixel_dither_policy_effective_t *effective)
{
    sixel_dither_execution_t const *execution;
    sixel_dither_t *dither;
    size_t cells;

    if (!policy->prepared || request->execution == NULL) {
        return SIXEL_LOGIC_ERROR;
    }
    execution = request->execution;
    if (execution->working_palette == NULL || execution->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    cells = (size_t)execution->selection.columns * execution->selection.rows;
    if (cells > 4096 || (cells != 0 &&
            (execution->selection.words == NULL ||
             execution->selection.word_count < (cells + 31) / 32))) {
        return SIXEL_BAD_ARGUMENT;
    }
    memset(effective, 0, sizeof(*effective));
    effective->result = request->result;
    effective->data = request->data;
    effective->width = request->width;
    effective->height = request->height;
    effective->band_origin = request->band_origin;
    effective->output_start = request->output_start;
    effective->depth = request->depth;
    effective->palette = request->palette;
    effective->lookup_policy = request->lookup_policy;
    effective->method_for_scan = policy->method_for_scan;
    effective->pixelformat = policy->pixelformat;
    dither = &effective->context;
    effective->dither = dither;
    sixel_dither_policy_apply_options(dither, &policy->options);
    dither->pixelformat = policy->pixelformat;
    dither->lookup_policy = request->lookup_policy;
    dither->palette = execution->working_palette;
    dither->allocator = execution->allocator;
    dither->pipeline_index_buffer = execution->indexes;
    dither->pipeline_image_width = execution->width;
    dither->pipeline_image_height = execution->height;
    dither->pipeline_accumulation_frame_width = execution->width;
    dither->pipeline_accumulation_frame_height = execution->height;
    dither->pipeline_transparent_mask = execution->transparent_mask;
    dither->pipeline_transparent_mask_size = execution->transparent_size;
    dither->pipeline_transparent_keycolor = execution->keycolor;
    dither->pipeline_accumulation_keycolor = execution->delta_keycolor;
    dither->pipeline_accumulation_pixels = execution->retained_rgb;
    dither->pipeline_accumulation_pixels_size = execution->retained_rgb_size;
    dither->pipeline_accumulation_valid_mask = execution->retained_valid;
    dither->pipeline_accumulation_valid_mask_size =
        execution->retained_valid_size;
    dither->pipeline_accumulation_width = execution->plane_width;
    dither->pipeline_accumulation_height = execution->plane_height;
    dither->pipeline_accumulation_origin_x = execution->origin_x;
    dither->pipeline_accumulation_origin_y = execution->origin_y;
    dither->pipeline_6delta_enabled = execution->delta_enabled;
    dither->pipeline_6delta_threshold = execution->delta_threshold;
    dither->pipeline_6delta_metric = execution->delta_metric;
    dither->pipeline_6delta_error_mode = execution->delta_error_mode;
    dither->pipeline_6delta_rgb101 = execution->delta_rgb101;
    dither->pipeline_6delta_hls = execution->delta_hls;
    dither->pipeline_6delta_hls_bytes = execution->hls_rgb;
    dither->pipeline_6delta_hls_count = execution->hls_count;
    dither->pipeline_accumulation_result_mask = execution->skip;
    dither->pipeline_accumulation_result_mask_size = execution->skip_size;
    dither->bluenoise_gradient_map = (unsigned char *)execution->gradient;
    dither->bluenoise_gradient_map_size = execution->gradient_size;
    dither->bluenoise_gradient_width = execution->gradient_width;
    dither->bluenoise_gradient_height = execution->gradient_height;
    dither->pipeline_row_callback = execution->row_ready;
    dither->pipeline_row_priv = execution->row_context;
    dither->pipeline_logger = execution->logger;
    dither->pipeline_band_height = execution->band_height;
    if (cells != 0) {
        memcpy(dither->pipeline_residual_mask.cells, execution->selection.words,
            ((cells + 31) / 32) * sizeof(uint32_t));
    }
    dither->pipeline_residual_mask.columns = execution->selection.columns;
    dither->pipeline_residual_mask.rows = execution->selection.rows;
    dither->pipeline_residual_mask.width = execution->selection.width;
    dither->pipeline_residual_mask.height = execution->selection.height;
    dither->pipeline_residual_mask.tiled = execution->selection.tiled;
    dither->pipeline_residual_mask.threshold =
        execution->selection.selected_threshold;
    return SIXEL_OK;
}
