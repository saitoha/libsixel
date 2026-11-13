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
#include "dither-policy-gpu.h"
#include "options.h"
#include "dither-common-pipeline.h"
#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
#include "gpu-palette-vulkan.h"
#define SIXEL_GPU_PALETTE_USE_VULKAN 1
#endif

#if defined(HAVE_METAL) || defined(SIXEL_GPU_PALETTE_USE_VULKAN)
SIXELSTATUS
sixel_gpu_lookup_compat_apply(sixel_gpu_palette_request_t const *request);
#endif
#if defined(HAVE_METAL)
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
    return sixel_gpu_dither_byte_program(method) != NULL;
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
    /* Decide before probing the device: a lookup policy without its own
     * Vulkan program never runs there, so AUTO must not pay for instance
     * creation. */
    if (!sixel_gpu_palette_policy_is_force(gpu_policy) &&
            !sixel_gpu_lookup_has_program(lut_policy,
                "vulkan.lookup-rgb8.v1")) {
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
    if (request->program == NULL &&
            !sixel_gpu_lookup_has_program(request->lut_policy,
                "vulkan.lookup-rgb8.v1")) {
        if (sixel_gpu_palette_policy_is_force(request->policy)) {
            sixel_helper_set_additional_message(
                "gpu palette apply: this lookup policy has no Vulkan "
                "program yet.");
            return SIXEL_FEATURE_ERROR;
        }
        return SIXEL_FALSE;
    }
#endif
    effective = *request;
    effective.dither_program = sixel_gpu_dither_byte_program(
        request->method_for_diffuse);
    status = effective.dither_program->resolve(&effective, request);
    if (status != SIXEL_OK) {
        return sixel_gpu_palette_policy_is_force(request->policy)
            ? status : SIXEL_FALSE;
    }
    effective.dither_program = sixel_gpu_dither_byte_program(
        request->method_for_diffuse);

    if (!sixel_gpu_palette_has_engine()) {
        if (sixel_gpu_palette_policy_is_force(request->policy)) {
            sixel_helper_set_additional_message(
                "gpu palette apply: no GPU engine is available.");
            return SIXEL_FEATURE_ERROR;
        }
        return SIXEL_FALSE;
    }

#if defined(HAVE_METAL) || defined(SIXEL_GPU_PALETTE_USE_VULKAN)
    status = sixel_gpu_lookup_compat_apply(&effective);
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
    /* Both Metal and Vulkan now carry every registered lookup program.
     * Keep the device probe so unavailable hardware still skips GPU tests. */
    return sixel_gpu_palette_has_engine();
}

SIXEL_INTERNAL_API int
sixel_gpu_palette_lookup_available(int kind)
{
    sixel_gpu_device_interface_t *device;
    int served;

    device = NULL;
    if (sixel_gpu_device_acquire(&device) != SIXEL_OK || device == NULL) {
        return 0;
    }
    served = sixel_gpu_lookup_has_program(kind,
        device->vtbl->program_format(device));
    device->vtbl->unref(device);
    return served;
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
#if defined(HAVE_METAL) || defined(SIXEL_GPU_PALETTE_USE_VULKAN)
    return sixel_gpu_lookup_compat_apply(request);
#else
    return SIXEL_FEATURE_ERROR;
#endif
}

#if !defined(HAVE_METAL) && !defined(SIXEL_GPU_PALETTE_USE_VULKAN)
SIXELSTATUS
sixel_gpu_device_acquire(sixel_gpu_device_interface_t **device)
{
    if (device == NULL) return SIXEL_BAD_ARGUMENT;
    *device = NULL;
    return SIXEL_FEATURE_ERROR;
}

SIXELSTATUS
sixel_gpu_program_prepare(sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t *plan,
    sixel_gpu_dither_program_t const *dither, sixel_gpu_program_t **program)
{
    (void)device;
    (void)plan;
    (void)dither;
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
