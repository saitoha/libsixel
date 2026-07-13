/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "compat_stub.h"
#include "gpu-palette.h"
#include "bluenoise_64x64.h"

#define SIXEL_GPU_PALETTE_AUTO_THRESHOLD_DEFAULT 262144U
#define SIXEL_GPU_PALETTE_THRESHOLD_ENVVAR \
    "SIXEL_GPU_PALETTE_THRESHOLD"

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
sixel_gpu_palette_metal_apply(sixel_gpu_palette_request_t const *request);

int
sixel_gpu_palette_metal_is_available(void);
#endif

static int
sixel_gpu_palette_policy_is_force(int policy)
{
    return policy == SIXEL_GPU_POLICY_FORCE;
}

static int
sixel_gpu_palette_lut_policy_is_supported(
    sixel_gpu_palette_request_t const *request)
{
    if (request == NULL) {
        return 0;
    }

    /*
     * The Metal kernel performs an exact direct palette scan, so it does not
     * need any of the CPU LUT implementations.  Keep AUTO conservative:
     * threshold-based GPU selection should not silently change output when the
     * caller requested 5bit, 6bit, or another CPU lookup policy.  FORCE is an
     * explicit request to use the GPU path, so treat the requested lookup
     * policy as a CPU implementation detail and bypass it.
     */
    if (request->lut_policy == SIXEL_LUT_POLICY_NONE) {
        return 1;
    }
    return sixel_gpu_palette_policy_is_force(request->policy);
}

static int
sixel_gpu_palette_accumulation_is_supported(
    sixel_gpu_palette_request_t const *request)
{
    size_t expected_size;

    expected_size = 0U;
    if (request == NULL) {
        return 0;
    }

    /*
     * 6delta accumulation is a byte-size optimization layered on top of
     * transparent-policy=keep.  The GPU path can participate only when the
     * caller gives it the retained RGB plane and the optional result mask
     * that the encoder later uses to keep the retained plane honest.
     */
    if (request->has_6delta_accumulation == 0) {
        return 1;
    }
    if (request->method_for_diffuse != SIXEL_DIFFUSE_NONE) {
        return 0;
    }
    if (request->accumulation_pixels == NULL ||
            request->accumulation_keycolor < 0 ||
            request->accumulation_keycolor >= SIXEL_PALETTE_MAX ||
            request->sixdelta_threshold > 255U) {
        return 0;
    }
    if (request->pixel_count > SIZE_MAX / 3U) {
        return 0;
    }
    expected_size = request->pixel_count * 3U;
    if (request->accumulation_pixels_size < expected_size) {
        return 0;
    }
    if (request->accumulation_valid_mask != NULL &&
            request->accumulation_valid_mask_size < request->pixel_count) {
        return 0;
    }
    if (request->accumulation_result_mask != NULL &&
            request->accumulation_result_mask_size < request->pixel_count) {
        return 0;
    }

    return 1;
}

static int
sixel_gpu_palette_parse_float_env(char const *text, float *out_value)
{
    char *endptr;
    double value;

    endptr = NULL;
    value = 0.0;
    if (text == NULL || out_value == NULL || text[0] == '\0') {
        return 0;
    }

    value = strtod(text, &endptr);
    if (endptr == text || *endptr != '\0') {
        return 0;
    }

    *out_value = (float)value;
    return 1;
}

static int
sixel_gpu_palette_parse_int_env(char const *text, int *out_value)
{
    char *endptr;
    long value;

    endptr = NULL;
    value = 0L;
    if (text == NULL || out_value == NULL || text[0] == '\0') {
        return 0;
    }

    value = strtol(text, &endptr, 10);
    if (endptr == text || *endptr != '\0') {
        return 0;
    }

    *out_value = (int)value;
    return 1;
}

static int
sixel_gpu_palette_parse_phase_env(char const *text, int *out_x, int *out_y)
{
    char *endptr;
    int x;
    int y;

    endptr = NULL;
    x = 0;
    y = 0;
    if (text == NULL || out_x == NULL || out_y == NULL || text[0] == '\0') {
        return 0;
    }

    x = (int)strtol(text, &endptr, 10);
    if (endptr == text || endptr == NULL || *endptr != ',') {
        return 0;
    }

    text = endptr + 1;
    y = (int)strtol(text, &endptr, 10);
    if (endptr == text || *endptr != '\0') {
        return 0;
    }

    *out_x = x;
    *out_y = y;
    return 1;
}

static int
sixel_gpu_palette_channel_is_rgb(char const *text)
{
    char value0;
    char value1;
    char value2;
    char value3;

    value0 = '\0';
    value1 = '\0';
    value2 = '\0';
    value3 = '\0';
    if (text == NULL) {
        return 0;
    }

    value0 = (char)tolower((unsigned char)text[0]);
    value1 = (char)tolower((unsigned char)text[1]);
    value2 = (char)tolower((unsigned char)text[2]);
    value3 = text[3];
    return value0 == 'r' && value1 == 'g' && value2 == 'b' &&
        value3 == '\0';
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
sixel_gpu_palette_bluenoise_conf_init(sixel_gpu_bluenoise_conf_t *conf)
{
    char const *text;
    int value;
    unsigned int hash;

    text = NULL;
    value = 0;
    hash = 0U;
    if (conf == NULL) {
        return;
    }

    conf->strength = 0.055f;
    conf->gradient_factor = 0.0f;
    conf->phase_x = 0;
    conf->phase_y = 0;
    conf->channel_rgb = 0;
    conf->size = SIXEL_BN_W;

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_STRENGTH");
    if (text != NULL) {
        (void)sixel_gpu_palette_parse_float_env(text, &conf->strength);
    }

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR");
    if (text != NULL) {
        (void)sixel_gpu_palette_parse_float_env(text,
                                                &conf->gradient_factor);
        if (conf->gradient_factor < 0.0f) {
            conf->gradient_factor = 0.0f;
        }
    }

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_CHANNEL");
    if (text != NULL) {
        conf->channel_rgb = sixel_gpu_palette_channel_is_rgb(text);
    }

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_SIZE");
    if (text != NULL &&
            sixel_gpu_palette_parse_int_env(text, &value) != 0 &&
            value == SIXEL_BN_W) {
        conf->size = value;
    }

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_PHASE");
    if (text != NULL &&
            sixel_gpu_palette_parse_phase_env(text,
                                              &conf->phase_x,
                                              &conf->phase_y) != 0) {
        return;
    }

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_SEED");
    if (text != NULL &&
            sixel_gpu_palette_parse_int_env(text, &value) != 0) {
        hash = sixel_gpu_palette_hash32((unsigned int)value);
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

static size_t
sixel_gpu_palette_auto_threshold(void)
{
    char const *text;
    char *endptr;
    unsigned long value;

    text = sixel_compat_getenv(SIXEL_GPU_PALETTE_THRESHOLD_ENVVAR);
    endptr = NULL;
    value = 0UL;
    if (text == NULL || text[0] == '\0') {
        return (size_t)SIXEL_GPU_PALETTE_AUTO_THRESHOLD_DEFAULT;
    }

    value = strtoul(text, &endptr, 10);
    if (endptr == text || *endptr != '\0') {
        return (size_t)SIXEL_GPU_PALETTE_AUTO_THRESHOLD_DEFAULT;
    }

    return (size_t)value;
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
            request->width <= 0 || request->height <= 0 ||
            request->pixel_count == 0U) {
        return 0;
    }
    if (request->ncolors <= 0 || request->ncolors > SIXEL_PALETTE_MAX ||
            request->palette_depth != 3) {
        return 0;
    }
    if (!sixel_gpu_palette_lut_policy_is_supported(request)) {
        return 0;
    }
    required_palette_size =
        (size_t)request->ncolors * (size_t)request->palette_depth;
    if (request->palette_size < required_palette_size) {
        return 0;
    }
    if (request->method_for_diffuse != SIXEL_DIFFUSE_NONE &&
            request->method_for_diffuse !=
            SIXEL_DIFFUSE_BLUENOISE_DITHER) {
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
    memset(&bluenoise_conf, 0, sizeof(bluenoise_conf));
    sixel_gpu_palette_bluenoise_conf_init(&bluenoise_conf);
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
#else
    return 0;
#endif
}

static int
sixel_gpu_palette_policy_may_apply(int policy, size_t pixel_count)
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
    if (pixel_count < sixel_gpu_palette_auto_threshold()) {
        return 0;
    }

    return sixel_gpu_palette_has_engine();
}

SIXEL_INTERNAL_API int
sixel_gpu_palette_policy_claims_apply_stage(int gpu_policy,
                                            int lut_policy,
                                            int method_for_diffuse,
                                            int method_for_scan,
                                            size_t pixel_count)
{
    int effective_scan;

    effective_scan = SIXEL_SCAN_AUTO;
    if (!sixel_gpu_palette_policy_may_apply(gpu_policy, pixel_count)) {
        return 0;
    }
    if (sixel_gpu_palette_policy_is_force(gpu_policy)) {
        return 1;
    }
    if (lut_policy != SIXEL_LUT_POLICY_NONE) {
        return 0;
    }
    if (method_for_diffuse != SIXEL_DIFFUSE_NONE &&
            method_for_diffuse != SIXEL_DIFFUSE_BLUENOISE_DITHER) {
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
            request->pixel_count < sixel_gpu_palette_auto_threshold()) {
        return SIXEL_FALSE;
    }
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
    status = sixel_gpu_palette_metal_apply(&effective);
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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
