/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <float.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "compat_stub.h"
#include "gpu-palette.h"
#include "lookup-policy-gpu.h"
#include "bluenoise_64x64.h"
#include "options.h"
#include "dither-common-pipeline.h"
#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
#include "gpu-palette-vulkan.h"
#define SIXEL_GPU_PALETTE_USE_VULKAN 1
#endif

typedef struct sixel_gpu_bluenoise_conf {
    float strength;
    float gradient_factor;
    int phase_x;
    int phase_y;
    int channel_rgb;
    int size;
} sixel_gpu_bluenoise_conf_t;

#if defined(HAVE_METAL)
SIXELSTATUS
sixel_gpu_lookup_compat_apply(sixel_gpu_palette_request_t const *request);

int
sixel_gpu_palette_metal_is_available(void);
#endif

static int
sixel_gpu_palette_policy_is_force(int policy)
{
    return policy == SIXEL_GPU_POLICY_FORCE;
}

void
sixel_gpu_palette_set_lookup_configuration(sixel_gpu_palette_request_t *request,
    sixel_lookup_policy_configuration_t const *configuration)
{
    request->fhedt_configured = configuration->kind == SIXEL_LUT_POLICY_FHEDT
        && configuration->pixelformat == SIXEL_PIXELFORMAT_RGB888;
    request->fhedt_resolution = configuration->fhedt_resolution;
    request->fhedt_refine = configuration->fhedt_refine;
    request->fhedt_use_dist2 = configuration->fhedt_use_dist2;
    request->fhedt_use_cache = configuration->fhedt_use_cache;
    memcpy(request->fhedt_weights, configuration->fhedt_weights,
        sizeof(request->fhedt_weights));
}

int
sixel_gpu_palette_fhedt_is_supported(
    sixel_gpu_palette_request_t const *request)
{
    return request != NULL && request->fhedt_configured &&
        (request->fhedt_resolution == 64 ||
         request->fhedt_resolution == 128 ||
         request->fhedt_resolution == 256) &&
        (request->fhedt_refine == 0 || request->fhedt_refine == 1) &&
        (request->fhedt_use_dist2 == 0 || request->fhedt_use_dist2 == 1) &&
        request->fhedt_use_cache == 0 &&
        request->fhedt_weights[0] > 0.0 &&
        request->fhedt_weights[0] <= DBL_MAX &&
        request->fhedt_weights[0] == request->fhedt_weights[1] &&
        request->fhedt_weights[0] == request->fhedt_weights[2];
}

static int
sixel_gpu_palette_lut_policy_is_supported(
    sixel_gpu_palette_request_t const *request)
{
    if (request == NULL) {
        return 0;
    }

    /*
     * The Metal kernels preserve each admitted lookup mode. NONE keeps the
     * direct scan, while EYTZINGER uses the one-dimensional projected search.
     * BLOCKCULL culls each pixel block's candidates before the same exact
     * scan; GRIDLUT reuses fixed-cell candidates and ORDERED scans
     * distance-sorted rows. DELAUNAY walks closed-cell adjacency. The byte
     * 5bit/6bit GPU contract uses immutable cell-center tables, deliberately
     * independent of the CPU first-query memo's traversal history.
     * Keep AUTO conservative for other policies because
     * the GPU path does not reproduce their output semantics.
     */
    if (request->lut_policy == SIXEL_LUT_POLICY_FHEDT)
        return sixel_gpu_palette_fhedt_is_supported(request);
    if (sixel_gpu_lookup_registered(request->lut_policy) ||
            request->lut_policy == SIXEL_LUT_POLICY_5BIT ||
            request->lut_policy == SIXEL_LUT_POLICY_6BIT ||
            request->lut_policy == SIXEL_LUT_POLICY_NONE ||
            request->lut_policy == SIXEL_LUT_POLICY_EYTZINGER ||
            request->lut_policy == SIXEL_LUT_POLICY_BLOCKCULL ||
            request->lut_policy == SIXEL_LUT_POLICY_GRIDLUT ||
            request->lut_policy == SIXEL_LUT_POLICY_ORDERED ||
            request->lut_policy == SIXEL_LUT_POLICY_DELAUNAY) {
        return 1;
    }
    return sixel_gpu_palette_policy_is_force(request->policy);
}

/* All admitted methods select independently at each pixel. */
static int
sixel_gpu_palette_method_is_supported(int method)
{
    return method == SIXEL_DIFFUSE_NONE ||
        method == SIXEL_DIFFUSE_A_DITHER ||
        method == SIXEL_DIFFUSE_X_DITHER ||
        method == SIXEL_DIFFUSE_BLUENOISE_DITHER;
}

static int
sixel_gpu_palette_accumulation_is_supported(
    sixel_gpu_palette_request_t const *request)
{
    size_t expected_size;
    size_t plane_pixels;
    int width;
    int height;

    expected_size = 0U;
    if (request == NULL) {
        return 0;
    }

    /*
     * 6delta accumulation is a byte-size optimization layered on top of
     * alpha-policy=keep.  The GPU path can participate only
     * when the caller gives it the retained RGB plane and the optional
     * result mask that the encoder later uses to keep the retained plane
     * honest.
     */
    if (request->has_6delta_accumulation == 0) {
        /*
         * All admitted GPU policies implement the keep gate. Missing retained
         * history cannot be interpreted as permission to paint every pixel.
         */
        if (request->sixdelta_enabled != 0 &&
                sixel_gpu_palette_method_is_supported(
                    request->method_for_diffuse)) {
            return 0;
        }
        return 1;
    }
    /*
     * A retained plane describes capability, while sixdelta_enabled carries
     * the caller's intent.  Reject contradictory internal requests instead of
     * letting the Metal kernel turn availability into an unsolicited keep.
     */
    if (request->sixdelta_enabled == 0) {
        return 0;
    }
    if (!sixel_gpu_palette_method_is_supported(request->method_for_diffuse)) {
        return 0;
    }
    if (request->accumulation_pixels == NULL ||
            request->accumulation_keycolor < 0 ||
            request->accumulation_keycolor >= SIXEL_PALETTE_MAX ||
            request->sixdelta_threshold > 255U) {
        return 0;
    }
    width = request->accumulation_width;
    height = request->accumulation_height;
    if (width == 0 && height == 0) {
        width = request->width;
        height = request->height;
    }
    /* Check subtraction before computing row offsets or upload sizes. The
     * canvas buffers include every row, while result masks stay frame-local. */
    if (width < request->width || height < request->height ||
            request->accumulation_origin_x < 0 ||
            request->accumulation_origin_y < 0 ||
            request->accumulation_origin_x > width - request->width ||
            request->accumulation_origin_y > height - request->height ||
            (size_t)height > SIZE_MAX / 3U / (size_t)width) {
        return 0;
    }
    plane_pixels = (size_t)width * (size_t)height;
    if (request->sixdelta_compare != SIXEL_DELTA_COMPARE_TARGET &&
            request->sixdelta_compare != SIXEL_DELTA_COMPARE_QUERY) return 0;
    if (request->sixdelta_metric < SIXEL_DELTA_METRIC_AUTO ||
            request->sixdelta_metric > SIXEL_DELTA_METRIC_RGB241) return 0;
    if (request->residual_mask != NULL &&
            request->residual_mask->columns != 0U &&
            (request->residual_mask->columns > 64U ||
             request->residual_mask->rows == 0U ||
             request->residual_mask->rows > 64U ||
             request->residual_mask->width != request->width ||
             request->residual_mask->height != request->height ||
             request->residual_mask->threshold > 255U)) return 0;
    expected_size = plane_pixels * 3U;
    if (request->accumulation_pixels_size < expected_size) {
        return 0;
    }
    if (request->accumulation_valid_mask != NULL &&
            request->accumulation_valid_mask_size < plane_pixels) {
        return 0;
    }
    if (request->accumulation_result_mask != NULL &&
            request->accumulation_result_mask_size < request->pixel_count) {
        return 0;
    }

    return 1;
}

static unsigned int
sixel_gpu_palette_hash32(unsigned int value)
{
    value ^= value >> 16;
    value = (unsigned int)((unsigned long long)value * 0x7feb352dU);
    value ^= value >> 15;
    value = (unsigned int)((unsigned long long)value * 0x846ca68bU);
    value ^= value >> 16;
    return value;
}

static void
sixel_gpu_palette_bluenoise_conf_init(sixel_gpu_bluenoise_conf_t *conf,
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
        hash = sixel_gpu_palette_hash32((unsigned int)seed);
        conf->phase_x = (int)(hash & 63U);
        conf->phase_y = (int)((hash >> 8) & 63U);
    }
}

static void
sixel_gpu_palette_apply_bluenoise_overrides(
    sixel_gpu_bluenoise_conf_t *conf,
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
        hash = sixel_gpu_palette_hash32(
            (unsigned int)request->bluenoise_seed);
        conf->phase_x = (int)(hash & 63U);
        conf->phase_y = (int)((hash >> 8) & 63U);
    }
}

SIXEL_INTERNAL_API size_t
sixel_gpu_palette_auto_threshold(void)
{
    size_t value;

    value = (size_t)SIXEL_GPU_PALETTE_AUTO_THRESHOLD_DEFAULT;
    (void)sixel_option_resolve_registered_size_binding(
        SIXEL_OPTION_SCHEMA_GPU_POLICY,
        NULL,
        SIXEL_SUBOPTION_BINDING_ID_2(
            gpu_palette_threshold,
            gpu_palette_threshold_override),
        &value);
    return value;
}

static int
sixel_gpu_palette_request_is_supported(
    sixel_gpu_palette_request_t const *request)
{
    size_t required_palette_size;

    required_palette_size = 0U;
    if (request == NULL || request->dest == NULL ||
            request->pixels == NULL || request->palette == NULL) {
        return 0;
    }
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            request->rows_per_chunk < 0 ||
            request->width <= 0 || request->height <= 0 ||
            request->pixel_count == 0U ||
            request->pixel_count > UINT32_MAX) {
        return 0;
    }
    if ((size_t)request->height > SIZE_MAX / 3U / (size_t)request->width ||
            request->pixel_count !=
                (size_t)request->width * (size_t)request->height) {
        return 0;
    }
    if (request->ncolors <= 0 || request->ncolors > SIXEL_PALETTE_MAX ||
            request->palette_depth != 3) {
        return 0;
    }
    if (request->program == NULL &&
            !sixel_gpu_palette_lut_policy_is_supported(request)) {
        return 0;
    }
    required_palette_size =
        (size_t)request->ncolors * (size_t)request->palette_depth;
    if (request->palette_size < required_palette_size) {
        return 0;
    }
    if (request->sixdelta_comparison_palette != NULL &&
            request->sixdelta_comparison_palette_size <
                required_palette_size) {
        return 0;
    }
    if (!sixel_gpu_palette_method_is_supported(request->method_for_diffuse)) {
        return 0;
    }
    if (request->method_for_scan != SIXEL_SCAN_RASTER &&
            request->method_for_scan != SIXEL_SCAN_SERPENTINE) {
        return 0;
    }
    if (request->transparent_mask != NULL &&
            request->transparent_mask_size < request->pixel_count) {
        return 0;
    }
    if (!sixel_gpu_palette_accumulation_is_supported(request)) {
        return 0;
    }

    return 1;
}

static void
sixel_gpu_palette_prepare_effective_request(
    sixel_gpu_palette_request_t *effective,
    sixel_gpu_palette_request_t const *request)
{
    sixel_gpu_bluenoise_conf_t bluenoise_conf;

    if (effective == NULL || request == NULL) {
        return;
    }

    *effective = *request;
    /* Match CPU precedence: paintable-count amplitude, registry environment,
     * then the encoder-bound numeric override. Never reuse blue-noise knobs
     * for the arithmetic patterns. */
    if (request->method_for_diffuse == SIXEL_DIFFUSE_A_DITHER ||
            request->method_for_diffuse == SIXEL_DIFFUSE_X_DITHER) {
        effective->positional_strength = sixel_dither_positional_auto_strength(
            request->method_for_diffuse, request->ncolors);
        if (request->method_for_diffuse == SIXEL_DIFFUSE_A_DITHER) {
            (void)sixel_option_resolve_registered_float_binding(
                SIXEL_OPTION_SCHEMA_DIFFUSION, "a_dither",
                SIXEL_SUBOPTION_BINDING_ID_2(
                    a_dither_strength, a_dither_strength_override),
                &effective->positional_strength);
        } else {
            (void)sixel_option_resolve_registered_float_binding(
                SIXEL_OPTION_SCHEMA_DIFFUSION, "x_dither",
                SIXEL_SUBOPTION_BINDING_ID_2(
                    x_dither_strength, x_dither_strength_override),
                &effective->positional_strength);
        }
        if (request->positional_strength_override) {
            effective->positional_strength = request->positional_strength;
        }
        return;
    }
    memset(&bluenoise_conf, 0, sizeof(bluenoise_conf));
    sixel_gpu_palette_bluenoise_conf_init(&bluenoise_conf, request->ncolors);
    sixel_gpu_palette_apply_bluenoise_overrides(&bluenoise_conf, request);
    effective->bluenoise_strength = bluenoise_conf.strength;
    effective->bluenoise_gradient_factor = bluenoise_conf.gradient_factor;
    effective->bluenoise_phase_x = bluenoise_conf.phase_x;
    effective->bluenoise_phase_y = bluenoise_conf.phase_y;
    effective->bluenoise_channel_rgb = bluenoise_conf.channel_rgb;
    effective->bluenoise_size = bluenoise_conf.size;
}

static int
sixel_gpu_palette_has_engine(void)
{
#if defined(HAVE_METAL)
    return sixel_gpu_palette_metal_is_available();
#elif defined(SIXEL_GPU_PALETTE_USE_VULKAN)
    return sixel_gpu_palette_vulkan_is_available();
#else
    return 0;
#endif
}

static int
sixel_gpu_palette_policy_may_apply(int policy,
                                   size_t auto_threshold,
                                   size_t pixel_count)
{
    if (policy == SIXEL_GPU_POLICY_OFF) {
        return 0;
    }
    if (sixel_gpu_palette_policy_is_force(policy)) {
        return 1;
    }
    if (policy != SIXEL_GPU_POLICY_AUTO) {
        return 0;
    }
    if (pixel_count < auto_threshold) {
        return 0;
    }

    return sixel_gpu_palette_has_engine();
}

SIXEL_INTERNAL_API int
sixel_gpu_palette_policy_claims_apply_stage(int gpu_policy,
                                            size_t auto_threshold,
                                            int lut_policy,
                                            int method_for_diffuse,
                                            int method_for_scan,
                                            size_t pixel_count)
{
    int effective_scan;

    effective_scan = SIXEL_SCAN_AUTO;
#if defined(SIXEL_GPU_PALETTE_USE_VULKAN)
    /* Decide before probing the device: an unported lookup never runs on
     * Vulkan, so AUTO must not pay for instance creation. */
    if (!sixel_gpu_palette_policy_is_force(gpu_policy) &&
            !sixel_gpu_palette_vulkan_supports_lut(lut_policy)) {
        return 0;
    }
#endif
    if (!sixel_gpu_palette_policy_may_apply(gpu_policy,
                                            auto_threshold,
                                            pixel_count)) {
        return 0;
    }
    if (sixel_gpu_palette_policy_is_force(gpu_policy)) {
        return 1;
    }
    if (!sixel_gpu_lookup_registered(lut_policy) &&
            lut_policy != SIXEL_LUT_POLICY_5BIT &&
            lut_policy != SIXEL_LUT_POLICY_6BIT &&
            lut_policy != SIXEL_LUT_POLICY_NONE &&
            lut_policy != SIXEL_LUT_POLICY_EYTZINGER &&
            lut_policy != SIXEL_LUT_POLICY_BLOCKCULL &&
            lut_policy != SIXEL_LUT_POLICY_GRIDLUT &&
            lut_policy != SIXEL_LUT_POLICY_ORDERED &&
            lut_policy != SIXEL_LUT_POLICY_DELAUNAY &&
            lut_policy != SIXEL_LUT_POLICY_FHEDT) {
        return 0;
    }
    if (!sixel_gpu_palette_method_is_supported(method_for_diffuse)) {
        return 0;
    }

    effective_scan = method_for_scan;
    if (effective_scan == SIXEL_SCAN_AUTO) {
        effective_scan = SIXEL_SCAN_RASTER;
    }
    if (effective_scan != SIXEL_SCAN_RASTER &&
            effective_scan != SIXEL_SCAN_SERPENTINE) {
        return 0;
    }

    return 1;
}

SIXELSTATUS
sixel_gpu_palette_apply(sixel_gpu_palette_request_t const *request)
{
    SIXELSTATUS status;
    sixel_gpu_palette_request_t effective;

    status = SIXEL_FALSE;
    memset(&effective, 0, sizeof(effective));
    if (request == NULL) {
        sixel_helper_set_additional_message(
            "gpu palette apply: request is null.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->policy == SIXEL_GPU_POLICY_OFF) {
        return SIXEL_FALSE;
    }
    if (!sixel_gpu_palette_request_is_supported(request)) {
        if (sixel_gpu_palette_policy_is_force(request->policy)) {
            sixel_helper_set_additional_message(
                "gpu palette apply: request shape is not supported.");
            return SIXEL_BAD_ARGUMENT;
        }
        return SIXEL_FALSE;
    }
    if (request->policy == SIXEL_GPU_POLICY_AUTO &&
            request->pixel_count < request->auto_threshold) {
        return SIXEL_FALSE;
    }
#if defined(SIXEL_GPU_PALETTE_USE_VULKAN)
    if (request->program != NULL ||
            !sixel_gpu_palette_vulkan_supports_lut(request->lut_policy)) {
        if (sixel_gpu_palette_policy_is_force(request->policy)) {
            sixel_helper_set_additional_message(
                "gpu palette apply: this lookup policy has no Vulkan "
                "program yet.");
            return SIXEL_FEATURE_ERROR;
        }
        return SIXEL_FALSE;
    }
#endif
    if (request->method_for_diffuse == SIXEL_DIFFUSE_BLUENOISE_DITHER) {
        sixel_gpu_palette_prepare_effective_request(&effective, request);
        if (effective.bluenoise_size != SIXEL_BN_W) {
            if (sixel_gpu_palette_policy_is_force(request->policy)) {
                sixel_helper_set_additional_message(
                    "gpu palette apply: blue-noise size is not supported.");
                return SIXEL_BAD_ARGUMENT;
            }
            return SIXEL_FALSE;
        }
    } else if (request->method_for_diffuse == SIXEL_DIFFUSE_A_DITHER ||
            request->method_for_diffuse == SIXEL_DIFFUSE_X_DITHER) {
        sixel_gpu_palette_prepare_effective_request(&effective, request);
    } else {
        effective = *request;
    }

    if (!sixel_gpu_palette_has_engine()) {
        if (sixel_gpu_palette_policy_is_force(request->policy)) {
            sixel_helper_set_additional_message(
                "gpu palette apply: no GPU engine is available.");
            return SIXEL_FEATURE_ERROR;
        }
        return SIXEL_FALSE;
    }

#if defined(HAVE_METAL)
    status = sixel_gpu_lookup_compat_apply(&effective);
    if (status == SIXEL_OK) {
        return SIXEL_OK;
    }
    if (sixel_gpu_palette_policy_is_force(request->policy)) {
        return status;
    }
#elif defined(SIXEL_GPU_PALETTE_USE_VULKAN)
    status = sixel_gpu_palette_vulkan_apply(&effective);
    if (status == SIXEL_OK) {
        return SIXEL_OK;
    }
    if (sixel_gpu_palette_policy_is_force(request->policy)) {
        return status;
    }
#else
    status = SIXEL_FALSE;
#endif

    (void)status;
    return SIXEL_FALSE;
}



SIXEL_INTERNAL_API int
sixel_gpu_palette_plan_available(void)
{
    /* Prepared lookup plans need a gpu_device and program compiler. The
     * Vulkan port owns only the direct apply stage so far. */
#if defined(HAVE_METAL)
    return sixel_gpu_palette_has_engine();
#else
    return 0;
#endif
}

SIXEL_INTERNAL_API int
sixel_gpu_palette_rows_per_chunk(int width, int height)
{
    int bands;

    if (width <= 0 || height < 12 ||
            (size_t)width < (SIXEL_GPU_PALETTE_AUTO_THRESHOLD_DEFAULT +
                (size_t)height - 1U) / (size_t)height) {
        return 0;
    }
    /* Aim for six groups, aligned to the consumer's six-row bands. Geometry
     * scales with the raster rather than fixing a video-specific row count. */
    bands = height / 6 + (height % 6 != 0);
    return ((bands + 5) / 6) * 6;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_gpu_palette_apply_prepared(sixel_gpu_palette_request_t const *request)
{
    if (!sixel_gpu_palette_request_is_supported(request)) {
        return SIXEL_BAD_ARGUMENT;
    }
#if defined(HAVE_METAL)
    return sixel_gpu_lookup_compat_apply(request);
#elif defined(SIXEL_GPU_PALETTE_USE_VULKAN)
    return sixel_gpu_palette_vulkan_apply(request);
#else
    return SIXEL_FEATURE_ERROR;
#endif
}

#if !defined(HAVE_METAL)
SIXELSTATUS
sixel_gpu_device_acquire(sixel_gpu_device_interface_t **device)
{
    if (device == NULL) return SIXEL_BAD_ARGUMENT;
    *device = NULL;
    return SIXEL_FEATURE_ERROR;
}

SIXELSTATUS
sixel_gpu_program_prepare(sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t *plan, sixel_gpu_program_t **program)
{
    (void)device;
    (void)plan;
    if (program == NULL) return SIXEL_BAD_ARGUMENT;
    *program = NULL;
    return SIXEL_FEATURE_ERROR;
}

void
sixel_gpu_program_destroy(sixel_gpu_program_t *program)
{
    (void)program;
}
#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
