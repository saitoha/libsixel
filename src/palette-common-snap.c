/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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

/*
 * Safe-tone snapping helpers for palettes.  The implementation mirrors the
 * reversible tone logic historically embedded inside palette.c but now lives in
 * a dedicated module so other translation units can reuse it without dragging
 * in unrelated merge infrastructure.
 */

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "colorspace.h"
#include "compat_stub.h"
#include "lookup-common.h"
#include "palette-common-snap.h"
#include "pixelformat.h"

enum sixel_palette_snap_policy {
    SIXEL_PALETTE_SNAP_POLICY_NEAREST = 0,
    SIXEL_PALETTE_SNAP_POLICY_REVERSIBLE
};

enum sixel_palette_snap_timing_policy {
    SIXEL_PALETTE_SNAP_TIMING_ONCE = 0,
    SIXEL_PALETTE_SNAP_TIMING_POLISH,
    SIXEL_PALETTE_SNAP_TIMING_MERGE,
    SIXEL_PALETTE_SNAP_TIMING_RESOLVE,
    SIXEL_PALETTE_SNAP_TIMING_ALL
};

static enum sixel_palette_snap_policy
sixel_palette_get_snap_policy(void);
static enum sixel_palette_snap_timing_policy
sixel_palette_get_snap_timing(void);
static double
sixel_palette_get_snap_approach_rate(void);
static double
sixel_palette_get_snap_channel_factor(void);
static int
sixel_palette_determine_colorspace(int pixelformat);
static void
sixel_palette_clamp_float_triplet(float *components, int pixelformat);
static SIXELSTATUS
sixel_palette_snap_float_triplet(float *components,
                                 int use_reversible,
                                 int pixelformat,
                                 enum sixel_palette_snap_stage stage);

static enum sixel_palette_snap_policy snap_policy_cache
    = SIXEL_PALETTE_SNAP_POLICY_NEAREST;
static int snap_policy_initialized = 0;
static enum sixel_palette_snap_timing_policy snap_timing_cache
    = SIXEL_PALETTE_SNAP_TIMING_ONCE;
static int snap_timing_initialized = 0;
static double snap_approach_cache = 1.0;
static int snap_approach_initialized = 0;
static double snap_channel_factor_cache = 0.85;
static int snap_channel_factor_initialized = 0;

static enum sixel_palette_snap_policy
sixel_palette_get_snap_policy(void)
{
    char const *policy;

    /*
     * SIXEL_PALETTE_SNAP_TARGET_POLICY controls whether we snap to the
     * reversible fixed points or choose the nearest fixed point in the current
     * colorspace.  The default "auto" is treated as "nearest".
     */
    if (snap_policy_initialized) {
        return snap_policy_cache;
    }

    snap_policy_initialized = 1;
    policy = sixel_compat_getenv("SIXEL_PALETTE_SNAP_TARGET_POLICY");
    if (policy == NULL || *policy == '\0') {
        snap_policy_cache = SIXEL_PALETTE_SNAP_POLICY_NEAREST;
        return snap_policy_cache;
    }

    if (sixel_compat_strcasecmp(policy, "reversible") == 0) {
        snap_policy_cache = SIXEL_PALETTE_SNAP_POLICY_REVERSIBLE;
        return snap_policy_cache;
    }

    snap_policy_cache = SIXEL_PALETTE_SNAP_POLICY_NEAREST;

    return snap_policy_cache;
}

static enum sixel_palette_snap_timing_policy
sixel_palette_get_snap_timing(void)
{
    char const *policy;

    if (snap_timing_initialized) {
        return snap_timing_cache;
    }

    snap_timing_initialized = 1;
    policy = sixel_compat_getenv("SIXEL_PALETTE_SNAP_TIMING_POLICY");
    if (policy == NULL || *policy == '\0') {
        snap_timing_cache = SIXEL_PALETTE_SNAP_TIMING_ONCE;

        return snap_timing_cache;
    }
    if (sixel_compat_strcasecmp(policy, "polish") == 0) {
        snap_timing_cache = SIXEL_PALETTE_SNAP_TIMING_POLISH;

        return snap_timing_cache;
    }
    if (sixel_compat_strcasecmp(policy, "merge") == 0) {
        snap_timing_cache = SIXEL_PALETTE_SNAP_TIMING_MERGE;

        return snap_timing_cache;
    }
    if (sixel_compat_strcasecmp(policy, "resolve") == 0) {
        snap_timing_cache = SIXEL_PALETTE_SNAP_TIMING_RESOLVE;

        return snap_timing_cache;
    }
    if (sixel_compat_strcasecmp(policy, "all") == 0) {
        snap_timing_cache = SIXEL_PALETTE_SNAP_TIMING_ALL;

        return snap_timing_cache;
    }

    snap_timing_cache = SIXEL_PALETTE_SNAP_TIMING_ONCE;

    return snap_timing_cache;
}

static double
sixel_palette_get_snap_approach_rate(void)
{
    char const *value;
    double parsed;

    if (snap_approach_initialized) {
        return snap_approach_cache;
    }

    snap_approach_initialized = 1;
    value = sixel_compat_getenv("SIXEL_PALETTE_SNAP_APPROACH_RATE");
    if (value == NULL || *value == '\0') {
        snap_approach_cache = 1.0;

        return snap_approach_cache;
    }

    parsed = strtod(value, NULL);
    if (parsed < 0.0) {
        parsed = 0.0;
    }
    if (parsed > 1.0) {
        parsed = 1.0;
    }
    snap_approach_cache = parsed;

    return snap_approach_cache;
}

static double
sixel_palette_get_snap_channel_factor(void)
{
    char const *value;
    double parsed;

    if (snap_channel_factor_initialized) {
        return snap_channel_factor_cache;
    }

    snap_channel_factor_initialized = 1;
    value = sixel_compat_getenv("SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L");
    if (value == NULL || *value == '\0') {
        snap_channel_factor_cache = 0.85;

        return snap_channel_factor_cache;
    }

    parsed = strtod(value, NULL);
    if (parsed < 0.0) {
        parsed = 0.0;
    }
    if (parsed > 1.0) {
        parsed = 1.0;
    }

    snap_channel_factor_cache = parsed;

    return snap_channel_factor_cache;
}

int
sixel_palette_should_snap(enum sixel_palette_snap_stage stage)
{
    enum sixel_palette_snap_timing_policy timing;

    timing = sixel_palette_get_snap_timing();
    if (stage == SIXEL_PALETTE_SNAP_STAGE_FINAL_OUTPUT) {
        return 1;
    }
    if (stage == SIXEL_PALETTE_SNAP_STAGE_FINAL_MERGE_PRE
        && timing >= SIXEL_PALETTE_SNAP_TIMING_POLISH) {
        return 1;
    }
    if (stage == SIXEL_PALETTE_SNAP_STAGE_FINAL_MERGE_ITER
        && timing >= SIXEL_PALETTE_SNAP_TIMING_MERGE) {
        return 1;
    }
    if (stage == SIXEL_PALETTE_SNAP_STAGE_QUANTIZER_ITER
        && timing >= SIXEL_PALETTE_SNAP_TIMING_RESOLVE) {
        return 1;
    }
    if (stage == SIXEL_PALETTE_SNAP_STAGE_INITIAL_SEED
        && timing >= SIXEL_PALETTE_SNAP_TIMING_ALL) {
        return 1;
    }

    return 0;
}

static int
sixel_palette_determine_colorspace(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
        return SIXEL_COLORSPACE_LINEAR;
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
        return SIXEL_COLORSPACE_OKLAB;
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
        return SIXEL_COLORSPACE_CIELAB;
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
        return SIXEL_COLORSPACE_DIN99D;
    default:
        return SIXEL_COLORSPACE_GAMMA;
    }
}

void
sixel_palette_reversible_palette(unsigned char *palette,
                                 unsigned int colors,
                                 int pixelformat)
{
    SIXELSTATUS status;
    unsigned char *working;
    size_t palette_bytes;
    size_t color_index;
    int depth;
    int channel;
    int colorspace;
    double approach;

    status = SIXEL_OK;
    working = NULL;
    palette_bytes = 0U;
    color_index = 0U;
    depth = sixel_helper_compute_depth(pixelformat);
    channel = 0;
    colorspace = sixel_palette_determine_colorspace(pixelformat);
    approach = sixel_palette_get_snap_approach_rate();
    if (!sixel_palette_should_snap(SIXEL_PALETTE_SNAP_STAGE_FINAL_OUTPUT)) {
        return;
    }
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        sixel_palette_reversible_palette_float((float *)palette,
                                               colors,
                                               pixelformat);
        return;
    }
    if (palette == NULL || colors == 0U || depth <= 0) {
        return;
    }

    /*
     * Snap in gamma-corrected sRGB space.  Byte palettes that already live in
     * gamma space are processed in-place; others are converted to sRGB, snapped
     * on the safe-tone grid, then converted back to the original colorspace.
     */
    palette_bytes = (size_t)colors * (size_t)depth;
    if (colorspace != SIXEL_COLORSPACE_GAMMA) {
        working = (unsigned char *)malloc(palette_bytes);
        if (working == NULL) {
            return;
        }
        memcpy(working, palette, palette_bytes);
        status = sixel_helper_convert_colorspace(working,
                                                 palette_bytes,
                                                 pixelformat,
                                                 colorspace,
                                                 SIXEL_COLORSPACE_GAMMA);
        if (SIXEL_FAILED(status)) {
            free(working);
            return;
        }
    } else {
        working = palette;
    }

    /*
     * Blend toward the reversible grid in float space so the approach rate is
     * effective even when the palette storage is 8bit.
     */
    for (color_index = 0U; color_index < (size_t)colors; ++color_index) {
        float original[4];
        float target[4];

        for (channel = 0; channel < depth; ++channel) {
            size_t index;
            unsigned char snapped_byte;
            float snapped_float;

            index = color_index * (size_t)depth + (size_t)channel;
            original[channel] = sixel_pixelformat_byte_to_float(
                pixelformat, channel, working[index]);
            snapped_byte = sixel_palette_reversible_value(working[index]);
            snapped_float = sixel_pixelformat_byte_to_float(
                pixelformat, channel, snapped_byte);
            target[channel] = original[channel]
                              + (snapped_float - original[channel])
                                    * (float)approach;
        }
        for (channel = 0; channel < depth; ++channel) {
            size_t index;
            unsigned char blended_byte;

            index = color_index * (size_t)depth + (size_t)channel;
            blended_byte = sixel_pixelformat_float_channel_to_byte(
                pixelformat, channel, target[channel]);
            working[index] = blended_byte;
        }
    }

    if (colorspace != SIXEL_COLORSPACE_GAMMA) {
        status = sixel_helper_convert_colorspace(working,
                                                 palette_bytes,
                                                 pixelformat,
                                                 SIXEL_COLORSPACE_GAMMA,
                                                 colorspace);
        if (SIXEL_FAILED(status)) {
            free(working);
            return;
        }
        memcpy(palette, working, palette_bytes);
        free(working);
    }
}

void
sixel_palette_reversible_palette_float(float *palette,
                                       unsigned int colors,
                                       int pixelformat)
{
    SIXELSTATUS status;
    size_t index;
    size_t color_index;
    int colorspace;
    int channel_count;
    int channel;
    double approach;

    status = SIXEL_OK;
    index = 0U;
    color_index = 0U;
    colorspace = sixel_palette_determine_colorspace(pixelformat);
    channel_count = 0;
    channel = 0;
    approach = sixel_palette_get_snap_approach_rate();

    if (palette == NULL || colors == 0U) {
        return;
    }
    if (!sixel_palette_should_snap(SIXEL_PALETTE_SNAP_STAGE_FINAL_OUTPUT)) {
        return;
    }

    /*
     * Float palettes in sRGB gamma can snap in place just like byte palettes
     * because the reversible grid already represents the nearest fixed points
     * in that space.  No colorspace round-trip is required here.
     */
    if (colorspace == SIXEL_COLORSPACE_GAMMA) {
        channel_count = sixel_helper_compute_depth(pixelformat)
            / (int)sizeof(float);
        if (channel_count <= 0) {
            return;
        }
        for (color_index = 0U; color_index < (size_t)colors; ++color_index) {
            float original[3];

            index = color_index * (size_t)channel_count;
            for (channel = 0; channel < 3 && channel < channel_count;
                 ++channel) {
                unsigned char snapped;
                float target;

                original[channel] = palette[index + (size_t)channel];
                snapped = sixel_pixelformat_float_channel_to_byte(
                    pixelformat,
                    channel,
                    original[channel]);
                snapped = sixel_palette_reversible_value(snapped);
                target = sixel_pixelformat_byte_to_float(pixelformat,
                                                         channel,
                                                         snapped);
                target = original[channel]
                         + (target - original[channel]) * (float)approach;
                palette[index + (size_t)channel]
                    = sixel_pixelformat_float_channel_clamp(pixelformat,
                                                           channel,
                                                           target);
            }
        }

        return;
    }

    channel_count = sixel_helper_compute_depth(pixelformat)
        / (int)sizeof(float);
    if (channel_count <= 0) {
        return;
    }
    for (color_index = 0U; color_index < (size_t)colors; ++color_index) {
        index = color_index * (size_t)channel_count;
        status = sixel_palette_snap_float_triplet(
            &palette[index],
            1,
            pixelformat,
            SIXEL_PALETTE_SNAP_STAGE_FINAL_OUTPUT);
        if (SIXEL_FAILED(status)) {
            return;
        }
    }
}

static SIXELSTATUS
sixel_palette_snap_float_triplet(float *components,
                                 int use_reversible,
                                 int pixelformat,
                                 enum sixel_palette_snap_stage stage)
{
    SIXELSTATUS status;
    float working_palette[3];
    float target_original[3];
    float candidate_gamma[3];
    float candidate_target[3];
    float candidate_gamma_values[3][2];
    float best_target[3];
    unsigned char snapped_bytes[3];
    int candidate_counts[3];
    int candidate_valid;
    unsigned char lower;
    unsigned char upper;
    unsigned char tone;
    float lower_f;
    float upper_f;
    float tone_f;
    double best_distance;
    int c0;
    int c1;
    int c2;
    int index;
    int colorspace;
    int original_pixelformat;
    int snap_policy;
    int channel;
    double approach;
    double channel_factor;

    status = SIXEL_OK;
    colorspace = sixel_palette_determine_colorspace(pixelformat);
    original_pixelformat = pixelformat;
    snap_policy = sixel_palette_get_snap_policy();
    approach = sixel_palette_get_snap_approach_rate();
    channel_factor = 1.0;
    if (colorspace == SIXEL_COLORSPACE_OKLAB
        || colorspace == SIXEL_COLORSPACE_CIELAB
        || colorspace == SIXEL_COLORSPACE_DIN99D) {
        channel_factor = sixel_palette_get_snap_channel_factor();
    }
    if (components == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (!use_reversible || !sixel_palette_should_snap(stage)) {
        for (channel = 0; channel < 3; ++channel) {
            components[channel]
                = sixel_pixelformat_float_channel_clamp(pixelformat,
                                                        channel,
                                                        components[channel]);
        }

        return SIXEL_OK;
    }

    memcpy(working_palette, components, sizeof(working_palette));
    sixel_palette_clamp_float_triplet(working_palette,
                                      original_pixelformat);
    memcpy(target_original, working_palette, sizeof(target_original));
    if (colorspace != SIXEL_COLORSPACE_GAMMA) {
        status = sixel_helper_convert_colorspace(
            (unsigned char *)working_palette,
            sizeof(working_palette),
            pixelformat,
            colorspace,
            SIXEL_COLORSPACE_GAMMA);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    }

    /*
     * Float snapping enumerates safe-tone corners in sRGB float space instead
     * of collapsing to 8bit beforehand.  This keeps nearest snapping from
     * losing precision when the palette is stored as float.
     */
    candidate_valid = 0;
    for (channel = 0; channel < 3; ++channel) {
        lower = sixel_safe_tones[0];
        upper = sixel_safe_tones[255];
        lower_f = sixel_pixelformat_byte_to_float(
            SIXEL_PIXELFORMAT_RGBFLOAT32, channel, lower);
        upper_f = sixel_pixelformat_byte_to_float(
            SIXEL_PIXELFORMAT_RGBFLOAT32, channel, upper);
        for (index = 0; index < 256; ++index) {
            tone = sixel_safe_tones[index];
            tone_f = sixel_pixelformat_byte_to_float(
                SIXEL_PIXELFORMAT_RGBFLOAT32, channel, tone);
            if (tone_f <= working_palette[channel]) {
                lower = tone;
                lower_f = tone_f;
            }
            if (tone_f >= working_palette[channel]) {
                upper = tone;
                upper_f = tone_f;
                break;
            }
        }

        candidate_counts[channel] = 1;
        candidate_gamma_values[channel][0] = lower_f;
        if (upper != lower) {
            candidate_gamma_values[channel][1] = upper_f;
            candidate_counts[channel] = 2;
        }
        snapped_bytes[channel] = sixel_palette_reversible_value(
            sixel_pixelformat_float_channel_to_byte(pixelformat,
                                                    channel,
                                                    working_palette[channel]));
    }

    if (snap_policy == SIXEL_PALETTE_SNAP_POLICY_NEAREST) {
        best_distance = 1.0e30;
        for (c0 = 0; c0 < candidate_counts[0]; ++c0) {
            for (c1 = 0; c1 < candidate_counts[1]; ++c1) {
                for (c2 = 0; c2 < candidate_counts[2]; ++c2) {
                    double distance;

                    candidate_gamma[0] = candidate_gamma_values[0][c0];
                    candidate_gamma[1] = candidate_gamma_values[1][c1];
                    candidate_gamma[2] = candidate_gamma_values[2][c2];

                    memcpy(candidate_target,
                           candidate_gamma,
                           sizeof(candidate_target));
                    if (colorspace != SIXEL_COLORSPACE_GAMMA) {
                        status = sixel_helper_convert_colorspace(
                            (unsigned char *)candidate_target,
                            sizeof(candidate_target),
                            SIXEL_PIXELFORMAT_RGBFLOAT32,
                            SIXEL_COLORSPACE_GAMMA,
                            colorspace);
                        if (SIXEL_FAILED(status)) {
                            continue;
                        }
                    }

                    /*
                     * Lab-family spaces use a weighted distance so L* can be
                     * emphasized relative to chroma.  The factor is tunable
                     * via SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L.
                     */
                    if (colorspace == SIXEL_COLORSPACE_OKLAB
                        || colorspace == SIXEL_COLORSPACE_CIELAB
                        || colorspace == SIXEL_COLORSPACE_DIN99D) {
                        double ldiff;
                        double adiff;
                        double bdiff;
                        double chroma_weight;

                        ldiff = (double)candidate_target[0]
                                - (double)target_original[0];
                        adiff = (double)candidate_target[1]
                                - (double)target_original[1];
                        bdiff = (double)candidate_target[2]
                                - (double)target_original[2];
                        chroma_weight = 1.0 - channel_factor;
                        distance = channel_factor * ldiff * ldiff
                                   + chroma_weight * 0.5 * adiff * adiff
                                   + chroma_weight * 0.5 * bdiff * bdiff;
                    } else {
                        distance = 0.0;
                        for (channel = 0; channel < 3; ++channel) {
                            double diff;

                            diff = (double)candidate_target[channel]
                                   - (double)target_original[channel];
                            distance += diff * diff;
                        }
                    }
                    if (distance < best_distance) {
                        best_distance = distance;
                        memcpy(best_target,
                               candidate_target,
                               sizeof(best_target));
                        candidate_valid = 1;
                    }
                }
            }
        }
        if (candidate_valid) {
            memcpy(working_palette, best_target, sizeof(working_palette));
        } else {
            memcpy(working_palette,
                   target_original,
                   sizeof(working_palette));
        }
    } else {
        for (channel = 0; channel < 3; ++channel) {
            snapped_bytes[channel]
                = sixel_palette_reversible_value(snapped_bytes[channel]);
            working_palette[channel]
                = sixel_pixelformat_byte_to_float(pixelformat,
                                                  channel,
                                                  snapped_bytes[channel]);
        }

        if (colorspace != SIXEL_COLORSPACE_GAMMA) {
            status = sixel_helper_convert_colorspace(
                (unsigned char *)working_palette,
                sizeof(working_palette),
                SIXEL_PIXELFORMAT_RGBFLOAT32,
                SIXEL_COLORSPACE_GAMMA,
                colorspace);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }
    }

    for (channel = 0; channel < 3; ++channel) {
        working_palette[channel]
            = target_original[channel]
              + (working_palette[channel] - target_original[channel])
                    * (float)approach;
    }
    sixel_palette_clamp_float_triplet(working_palette,
                                      original_pixelformat);
    memcpy(components, working_palette, sizeof(working_palette));

    return SIXEL_OK;
}

static void
sixel_palette_clamp_float_triplet(float *components, int pixelformat)
{
    int channel;

    for (channel = 0; channel < 3; ++channel) {
        components[channel] = sixel_pixelformat_float_channel_clamp(
            pixelformat, channel, components[channel]);
    }
}

double
sixel_palette_snap_double(double value,
                          int use_reversible,
                          int pixelformat,
                          int channel,
                          enum sixel_palette_snap_stage stage)
{
    double clamped;
    double snapped;
    double approach;
    unsigned char sample;

    clamped = value;
    snapped = value;
    approach = sixel_palette_get_snap_approach_rate();
    sample = 0U;
    if (!use_reversible || !sixel_palette_should_snap(stage)) {
        return (double)sixel_pixelformat_float_channel_clamp(pixelformat,
                                                             channel,
                                                             (float)value);
    }
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        float target;

        sample = sixel_pixelformat_float_channel_to_byte(pixelformat,
                                                         channel,
                                                         (float)value);
        sample = sixel_palette_reversible_value(sample);
        target = sixel_pixelformat_byte_to_float(pixelformat,
                                                 channel,
                                                 sample);
        target = (float)(value + (target - value) * approach);

        return (double)sixel_pixelformat_float_channel_clamp(pixelformat,
                                                             channel,
                                                             target);
    }
    if (clamped < 0.0) {
        clamped = 0.0;
    }
    if (clamped > 255.0) {
        clamped = 255.0;
    }
    sample = (unsigned char)(clamped + 0.5);
    snapped = (double)sixel_palette_reversible_value((unsigned int)sample);
    snapped = clamped + (snapped - clamped) * approach;
    if (snapped < 0.0) {
        snapped = 0.0;
    }
    if (snapped > 255.0) {
        snapped = 255.0;
    }

    return snapped;
}

void
sixel_palette_snap_triple(double *components,
                          int use_reversible,
                          int pixelformat,
                          enum sixel_palette_snap_stage stage)
{
    SIXELSTATUS status;
    float working[3];
    unsigned char byte_value;
    int channel;

    status = SIXEL_OK;
    if (components == NULL) {
        return;
    }

    /*
     * Convert incoming components into the current colorspace as float values
     * so the snapping routine can perform colorspace conversion as needed.
     */
    for (channel = 0; channel < 3; ++channel) {
        working[channel] = 0.0f;
    }
    for (channel = 0; channel < 3; ++channel) {
        if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
            working[channel] = (float)components[channel];
            continue;
        }
        byte_value = 0U;
        if (components[channel] < 0.0) {
            byte_value = 0U;
        } else if (components[channel] > 255.0) {
            byte_value = 255U;
        } else {
            byte_value = (unsigned char)(components[channel] + 0.5);
        }
        working[channel] = sixel_pixelformat_byte_to_float(pixelformat,
                                                           channel,
                                                           byte_value);
    }

    status = sixel_palette_snap_float_triplet(working,
                                              use_reversible,
                                              pixelformat,
                                              stage);
    if (SIXEL_FAILED(status)) {
        return;
    }
    for (channel = 0; channel < 3; ++channel) {
        if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
            components[channel] = (double)working[channel];
            continue;
        }
        byte_value = sixel_pixelformat_float_channel_to_byte(pixelformat,
                                                             channel,
                                                             working[channel]);
        components[channel] = (double)byte_value;
    }
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
