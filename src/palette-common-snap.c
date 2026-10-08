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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stddef.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "colorspace.h"
#include "loader-common.h"
#include "options.h"
#include "palette-common-snap.h"
#include "pixelformat.h"

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
static SIXELSTATUS
sixel_palette_snap_float_entries(float *entries, unsigned int colors,
                                  int pixelformat);

#if defined(_MSC_VER)
# define SIXEL_SNAP_TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
    && !defined(__PCC__)
# define SIXEL_SNAP_TLS _Thread_local
#elif (defined(__GNUC__) || defined(__clang__)) && !defined(__PCC__)
# define SIXEL_SNAP_TLS __thread
#else
# define SIXEL_SNAP_TLS
#endif

static SIXEL_SNAP_TLS sixel_palette_snap_options_t g_snap_options;

#undef SIXEL_SNAP_TLS

void
sixel_set_palette_snap_override(
    sixel_palette_snap_options_t const *options)
{
    char const *policy_name;
    char const *timing_name;

    policy_name = "none";
    timing_name = "once";
    if (options == NULL) {
        memset(&g_snap_options, 0, sizeof(g_snap_options));
        return;
    }
    g_snap_options = *options;
    if (options->policy == SIXEL_PALETTE_SNAP_POLICY_NEAREST) {
        policy_name = "nearest";
    } else if (options->policy
               == SIXEL_PALETTE_SNAP_POLICY_REVERSIBLE) {
        policy_name = "reversible";
    }
    switch (options->timing) {
    case SIXEL_PALETTE_SNAP_TIMING_POLISH:
        timing_name = "polish";
        break;
    case SIXEL_PALETTE_SNAP_TIMING_MERGE:
        timing_name = "merge";
        break;
    case SIXEL_PALETTE_SNAP_TIMING_RESOLVE:
        timing_name = "resolve";
        break;
    case SIXEL_PALETTE_SNAP_TIMING_ALL:
        timing_name = "all";
        break;
    case SIXEL_PALETTE_SNAP_TIMING_ONCE:
    default:
        break;
    }
    if (options->policy_override) {
        sixel_trace_topic_message(
            "palette_contract",
            "LSXSNP1|policy=%s",
            policy_name);
    }
    if (options->timing_override) {
        sixel_trace_topic_message(
            "palette_contract",
            "LSXSNP1|timing=%s",
            timing_name);
    }
    if (options->approach_rate_override) {
        sixel_trace_topic_message(
            "palette_contract",
            "LSXSNP1|approach=%.17g",
            options->approach_rate);
    }
    if (options->channel_factor_l_override) {
        sixel_trace_topic_message(
            "palette_contract",
            "LSXSNP1|channel_l=%.17g",
            options->channel_factor_l);
    }
}

static enum sixel_palette_snap_policy
sixel_palette_get_snap_policy(void)
{
    int value;

    value = SIXEL_PALETTE_SNAP_POLICY_NONE;
    if (g_snap_options.policy_override) {
        return (enum sixel_palette_snap_policy)g_snap_options.policy;
    }
    (void)sixel_option_resolve_registered_base_environment(
        SIXEL_OPTION_SCHEMA_SNAP_POLICY,
        SIXEL_OPTION_SCOPE_ENCODER,
        &value);
    return (enum sixel_palette_snap_policy)value;
}

int
sixel_palette_snap_is_enabled(void)
{
    return sixel_palette_get_snap_policy()
        != SIXEL_PALETTE_SNAP_POLICY_NONE
        && sixel_palette_get_snap_approach_rate() > 0.0;
}

static enum sixel_palette_snap_timing_policy
sixel_palette_get_snap_timing(void)
{
    int value;

    value = SIXEL_PALETTE_SNAP_TIMING_ONCE;
    if (g_snap_options.timing_override) {
        return (enum sixel_palette_snap_timing_policy)g_snap_options.timing;
    }
    (void)sixel_option_resolve_registered_int_binding(
        SIXEL_OPTION_SCHEMA_SNAP_POLICY,
        NULL,
        SIXEL_SUBOPTION_BINDING_ID_2(
            snap_policy_timing,
            snap_policy_timing_override),
        &value);
    return (enum sixel_palette_snap_timing_policy)value;
}

static double
sixel_palette_get_snap_approach_rate(void)
{
    double value;

    value = 1.0;
    if (g_snap_options.approach_rate_override) {
        return g_snap_options.approach_rate;
    }
    (void)sixel_option_resolve_registered_double_binding(
        SIXEL_OPTION_SCHEMA_SNAP_POLICY,
        NULL,
        SIXEL_SUBOPTION_BINDING_ID_2(
            snap_policy_approach_rate,
            snap_policy_approach_rate_override),
        &value);
    return value;
}

static double
sixel_palette_get_snap_channel_factor(void)
{
    double value;

    value = 0.85;
    if (g_snap_options.channel_factor_l_override) {
        return g_snap_options.channel_factor_l;
    }
    (void)sixel_option_resolve_registered_double_binding(
        SIXEL_OPTION_SCHEMA_SNAP_POLICY,
        NULL,
        SIXEL_SUBOPTION_BINDING_ID_2(
            snap_policy_channel_factor_l,
            snap_policy_channel_factor_l_override),
        &value);
    return value;
}

int
sixel_palette_snap_is_exact(void)
{
    return sixel_palette_snap_is_enabled()
        && sixel_palette_get_snap_approach_rate() >= 1.0;
}

/*
 * Typed safe targets can drift during an approximate colorspace round trip.
 * Resolve the actual SIXEL percentage once, then expose that same fixed point
 * to byte exports and float wire emission. Partial approach rates never call
 * this helper, and fixed palettes never acquire the generated-palette flag.
 */
void
sixel_palette_snap_output(unsigned char *entries,
                         float *entries_float32,
                         unsigned int colors)
{
    size_t index;
    size_t count;
    int percent;
    float value;

    count = (size_t)colors * 3U;
    for (index = 0U; index < count; ++index) {
        if (entries_float32 != NULL) {
            value = sixel_pixelformat_float_channel_clamp(
                SIXEL_PIXELFORMAT_RGBFLOAT32, (int)(index % 3U),
                entries_float32[index]);
            percent = (int)(value * 100.0f + 0.5f);
        } else {
            percent = (entries[index] * 100 + 127) / 255;
        }
        entries[index] = (unsigned char)((percent * 255 + 50) / 100);
        if (entries_float32 != NULL) {
            entries_float32[index] = (float)entries[index] / 255.0f;
        }
    }
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
    size_t channels;

    status = SIXEL_OK;
    working = NULL;
    palette_bytes = 0U;
    color_index = 0U;
    depth = sixel_helper_compute_depth(pixelformat);
    channel = 0;
    colorspace = sixel_palette_determine_colorspace(pixelformat);
    approach = sixel_palette_get_snap_approach_rate();
    channels = 0U;
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
    channels = (size_t)depth;
    if (channels > 4U) {
        return;
    }
    if ((size_t)colors > SIZE_MAX / channels) {
        return;
    }

    /*
     * Snap in gamma-corrected sRGB space.  Byte palettes that already live in
     * gamma space are processed in-place; others are converted to sRGB, snapped
     * on the safe-tone grid, then converted back to the original colorspace.
     */
    palette_bytes = (size_t)colors * channels;
    working = (unsigned char *)malloc(palette_bytes);
    if (working == NULL) {
        return;
    }
    memcpy(working, palette, palette_bytes);
    if (colorspace != SIXEL_COLORSPACE_GAMMA) {
        status = sixel_helper_convert_colorspace(working,
                                                 palette_bytes,
                                                 pixelformat,
                                                 colorspace,
                                                 SIXEL_COLORSPACE_GAMMA);
        if (SIXEL_FAILED(status)) {
            free(working);
            return;
        }
    }

    /*
     * Blend toward the reversible grid in float space so the approach rate is
     * effective even when the palette storage is 8bit.
     */
    for (color_index = 0U; color_index < (size_t)colors; ++color_index) {
        float original[4];
        float target[4];

        for (channel = 0; channel < (int)channels; ++channel) {
            size_t index;
            unsigned char snapped_byte;
            float snapped_float;

            index = color_index * channels + (size_t)channel;
            if (index >= palette_bytes) {
                free(working);
                return;
            }
            original[channel] = sixel_pixelformat_byte_to_float(
                pixelformat, channel, working[index]);
            snapped_byte = sixel_palette_reversible_value(working[index]);
            snapped_float = sixel_pixelformat_byte_to_float(
                pixelformat, channel, snapped_byte);
            target[channel] = original[channel]
                              + (snapped_float - original[channel])
                                    * (float)approach;
        }
        for (channel = 0; channel < (int)channels; ++channel) {
            size_t index;
            unsigned char blended_byte;

            index = color_index * channels + (size_t)channel;
            if (index >= palette_bytes) {
                free(working);
                return;
            }
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
    }
    memcpy(palette, working, palette_bytes);
    free(working);
}

void
sixel_palette_reversible_palette_float(float *palette,
                                       unsigned int colors,
                                       int pixelformat)
{
    /* The final hook and the native builder share target selection and
     * interpolation. Timing only controls earlier calls, not this boundary.
     * This legacy void interface leaves the palette unchanged on failure. */
    (void)sixel_palette_snap_float_entries(palette, colors, pixelformat);
}

/*
 * Byte entries in a perceptual palette are packed coordinates, not RGB codes.
 * Snap the authoritative typed view and repack it so later float/byte
 * consistency checks retain the selected safe RGB target and its precision.
 * The caller owns enablement; this is the single final snap, including rate.
 */
void
sixel_palette_snap_entries(unsigned char *entries,
                          float *entries_float32,
                          unsigned int colors,
                          int pixelformat)
{
    size_t index;
    size_t count;
    int channel;

    if (entries == NULL || colors == 0U) {
        return;
    }
    if (entries_float32 == NULL
            || !SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        sixel_palette_reversible_palette(entries, colors,
                                         SIXEL_PIXELFORMAT_RGB888);
        return;
    }
    sixel_palette_reversible_palette_float(entries_float32, colors,
                                            pixelformat);
    count = (size_t)colors * 3U;
    for (index = 0U; index < count; ++index) {
        channel = (int)(index % 3U);
        entries[index] = sixel_pixelformat_float_channel_to_byte(
            pixelformat, channel, entries_float32[index]);
    }
}

/* One typed target search for final palettes and early solver hooks.
 * Only safe-tone candidates are bounded. Rank in double precision and blend
 * from the original coordinates, including negative and extended values. */
static SIXELSTATUS
sixel_palette_snap_coordinates(double *result, double const *source,
                                 int pixelformat)
{
    double gamma[3];
    double corners[3][2];
    double candidate[3];
    double target[3];
    double best[3];
    double weights[3];
    double rate;
    double tone;
    double bounded;
    double distance;
    double delta;
    double blended;
    int counts[3];
    int have_best;
    int space;
    int policy;
    int channel;
    int index;
    int byte;
    int c0;
    int c1;
    int c2;

    space = sixel_palette_determine_colorspace(pixelformat);
    policy = sixel_palette_get_snap_policy();
    rate = sixel_palette_get_snap_approach_rate();
    weights[0] = weights[1] = weights[2] = 1.0;
    if (space == SIXEL_COLORSPACE_OKLAB || space == SIXEL_COLORSPACE_CIELAB ||
            space == SIXEL_COLORSPACE_DIN99D) {
        weights[0] = sixel_palette_get_snap_channel_factor();
        weights[1] = weights[2] = (1.0 - weights[0]) * 0.5;
    }
    if (!sixel_color_convert(gamma, source, space,
            SIXEL_COLORSPACE_GAMMA)) {
        return SIXEL_BAD_INPUT;
    }
    for (channel = 0; channel < 3; ++channel) {
        if (policy == SIXEL_PALETTE_SNAP_POLICY_REVERSIBLE) {
            bounded = gamma[channel] < 0.0 ? 0.0 : gamma[channel];
            bounded = bounded > 1.0 ? 1.0 : bounded;
            byte = (int)(bounded * 255.0 + 0.5);
            corners[channel][0] = sixel_safe_tones[byte] / 255.0;
            corners[channel][1] = corners[channel][0];
            continue;
        }
        corners[channel][0] = 0.0;
        corners[channel][1] = 1.0;
        for (index = 0; index < 256; ++index) {
            tone = sixel_safe_tones[index] / 255.0;
            if (tone <= gamma[channel]) {
                corners[channel][0] = tone;
            }
            if (tone >= gamma[channel]) {
                corners[channel][1] = tone;
                break;
            }
        }
    }
    for (channel = 0; channel < 3; ++channel) {
        counts[channel] = corners[channel][0] == corners[channel][1]
            ? 1 : 2;
    }
    have_best = 0;
    memset(best, 0, sizeof(best));
    for (c0 = 0; c0 < counts[0]; ++c0) {
        for (c1 = 0; c1 < counts[1]; ++c1) {
            for (c2 = 0; c2 < counts[2]; ++c2) {
                candidate[0] = corners[0][c0];
                candidate[1] = corners[1][c1];
                candidate[2] = corners[2][c2];
                if (!sixel_color_convert(target, candidate,
                        SIXEL_COLORSPACE_GAMMA, space)) {
                    return SIXEL_BAD_INPUT;
                }
                distance = 0.0;
                for (channel = 0; channel < 3; ++channel) {
                    /* Compare squared distances after cancelling their
                     * common source^2 term. Equal target components add
                     * exactly zero, even beside a huge source component. */
                    delta = target[channel] - best[channel];
                    distance += weights[channel] * delta *
                        (target[channel] + best[channel]
                            - 2.0 * source[channel]);
                }
                if (!isfinite(distance)) {
                    return SIXEL_BAD_INPUT;
                }
                if (!have_best || distance < 0.0) {
                    have_best = 1;
                    memcpy(best, target, sizeof(best));
                }
            }
        }
    }
    for (channel = 0; channel < 3; ++channel) {
        /* Weighted endpoints avoid x + (target - x) losing the target
         * to cancellation at rate=1 for very large finite coordinates. */
        blended = (1.0 - rate) * source[channel] + rate * best[channel];
        if (!isfinite(blended) || fabs(blended) > FLT_MAX) {
            return SIXEL_BAD_INPUT;
        }
        result[channel] = blended;
    }
    return SIXEL_OK;
}

/* Stage the complete palette before publication: a later invalid entry must
 * not leave an earlier entry snapped. Inactive operations preserve storage. */
static SIXELSTATUS
sixel_palette_snap_float_entries(float *entries, unsigned int colors,
                                  int pixelformat)
{
    float result[SIXEL_PALETTE_MAX * 3];
    double source[3];
    double snapped[3];
    SIXELSTATUS status;
    unsigned int color;
    int channel;

    if (entries == NULL || colors == 0 || colors > SIXEL_PALETTE_MAX ||
            (pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32 &&
             pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 &&
             pixelformat != SIXEL_PIXELFORMAT_OKLABFLOAT32 &&
             pixelformat != SIXEL_PIXELFORMAT_CIELABFLOAT32 &&
             pixelformat != SIXEL_PIXELFORMAT_DIN99DFLOAT32)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_palette_snap_is_enabled()) {
        return SIXEL_OK;
    }
    for (color = 0; color < colors; ++color) {
        for (channel = 0; channel < 3; ++channel) {
            source[channel] = entries[color * 3u + channel];
            if (!isfinite(source[channel])) {
                return SIXEL_BAD_INPUT;
            }
        }
        status = sixel_palette_snap_coordinates(snapped, source, pixelformat);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        for (channel = 0; channel < 3; ++channel) {
            result[color * 3u + channel] = (float)snapped[channel];
        }
    }
    memcpy(entries, result, colors * 3u * sizeof(float));
    return SIXEL_OK;
}

/* The native builder still admits only final-only timing. Sharing arithmetic
 * with early hooks does not implement their solver ordering or binning. */
SIXELSTATUS
sixel_palette_snap_native(float *entries, unsigned int colors,
                           int pixelformat)
{
    if (sixel_palette_snap_is_enabled() &&
            sixel_palette_get_snap_timing() != SIXEL_PALETTE_SNAP_TIMING_ONCE) {
        return SIXEL_BAD_ARGUMENT;
    }
    return sixel_palette_snap_float_entries(entries, colors, pixelformat);
}

void
sixel_palette_snap_triple(double *components,
                          int use_reversible,
                          int pixelformat,
                          enum sixel_palette_snap_stage stage)
{
    double working[3];
    double snapped[3];
    unsigned char byte_value;
    int active;
    int channel;

    if (components == NULL) {
        return;
    }
    active = use_reversible && sixel_palette_snap_is_enabled() &&
        sixel_palette_should_snap(stage);
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        /* An inactive stage must not narrow solver doubles to float32,
         * clamp nominal channel ranges, or even change signed zero. */
        if (!active) {
            return;
        }
        if (SIXEL_SUCCEEDED(sixel_palette_snap_coordinates(snapped,
                components, pixelformat))) {
            memcpy(components, snapped, sizeof(snapped));
        }
        return;
    }

    /* Byte solvers retain their bounded, rounded coordinate contract even
     * at an inactive stage. Their work does not promote the owning palette. */
    for (channel = 0; channel < 3; ++channel) {
        if (!isfinite(components[channel]) || components[channel] < 0.0) {
            byte_value = 0;
        } else if (components[channel] > 255.0) {
            byte_value = 255;
        } else {
            byte_value = (unsigned char)(components[channel] + 0.5);
        }
        working[channel] = (double)byte_value / 255.0;
    }
    if (active) {
        if (SIXEL_FAILED(sixel_palette_snap_coordinates(snapped, working,
                SIXEL_PIXELFORMAT_RGBFLOAT32))) {
            return;
        }
        memcpy(working, snapped, sizeof(working));
    }
    for (channel = 0; channel < 3; ++channel) {
        components[channel] = sixel_pixelformat_float_channel_to_byte(
            SIXEL_PIXELFORMAT_RGBFLOAT32, channel, (float)working[channel]);
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
