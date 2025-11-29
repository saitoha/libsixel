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
#include <strings.h>

#include "colorspace.h"
#include "compat_stub.h"
#include "lookup-common.h"
#include "palette-common-snap.h"
#include "pixelformat.h"

enum sixel_palette_snap_policy {
    SIXEL_PALETTE_SNAP_POLICY_NEAREST = 0,
    SIXEL_PALETTE_SNAP_POLICY_REVERSIBLE
};

static enum sixel_palette_snap_policy
sixel_palette_get_snap_policy(void);
static int
sixel_palette_determine_colorspace(int pixelformat);
static void
sixel_palette_clamp_float_triplet(float *components, int pixelformat);
static SIXELSTATUS
sixel_palette_snap_float_triplet(float *components,
                                 int use_reversible,
                                int pixelformat);

static enum sixel_palette_snap_policy snap_policy_cache
    = SIXEL_PALETTE_SNAP_POLICY_NEAREST;
static int snap_policy_initialized = 0;

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

    if (strcasecmp(policy, "reversible") == 0) {
        snap_policy_cache = SIXEL_PALETTE_SNAP_POLICY_REVERSIBLE;
        return snap_policy_cache;
    }

    snap_policy_cache = SIXEL_PALETTE_SNAP_POLICY_NEAREST;

    return snap_policy_cache;
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

    status = SIXEL_OK;
    working = NULL;
    palette_bytes = 0U;
    color_index = 0U;
    depth = sixel_helper_compute_depth(pixelformat);
    channel = 0;
    colorspace = sixel_palette_determine_colorspace(pixelformat);
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

    for (color_index = 0U; color_index < (size_t)colors; ++color_index) {
        for (channel = 0; channel < depth; ++channel) {
            size_t index;

            index = color_index * (size_t)depth + (size_t)channel;
            working[index]
                = sixel_palette_reversible_value(working[index]);
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

    status = SIXEL_OK;
    index = 0U;
    color_index = 0U;
    colorspace = sixel_palette_determine_colorspace(pixelformat);
    channel_count = 0;
    channel = 0;

    if (palette == NULL || colors == 0U) {
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
            index = color_index * (size_t)channel_count;
            for (channel = 0; channel < 3 && channel < channel_count;
                 ++channel) {
                unsigned char snapped;

                snapped = sixel_pixelformat_float_channel_to_byte(
                    pixelformat,
                    channel,
                    palette[index + (size_t)channel]);
                snapped = sixel_palette_reversible_value(snapped);
                palette[index + (size_t)channel]
                    = sixel_pixelformat_byte_to_float(pixelformat,
                                                      channel,
                                                      snapped);
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
        status = sixel_palette_snap_float_triplet(&palette[index],
                                                  1,
                                                  pixelformat);
        if (SIXEL_FAILED(status)) {
            return;
        }
    }
}

static SIXELSTATUS
sixel_palette_snap_float_triplet(float *components,
                                 int use_reversible,
                                 int pixelformat)
{
    SIXELSTATUS status;
    float working_palette[3];
    float target_original[3];
    float candidate_gamma[3];
    float candidate_target[3];
    unsigned char snapped_bytes[3];
    unsigned char candidate_values[3][3];
    int candidate_counts[3];
    int colorspace;
    int original_pixelformat;
    int snap_policy;
    int channel;

    status = SIXEL_OK;
    colorspace = sixel_palette_determine_colorspace(pixelformat);
    original_pixelformat = pixelformat;
    snap_policy = sixel_palette_get_snap_policy();
    if (components == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (!use_reversible) {
        for (channel = 0; channel < 3; ++channel) {
            components[channel]
                = sixel_pixelformat_float_channel_clamp(pixelformat,
                                                        channel,
                                                        components[channel]);
        }

        return SIXEL_OK;
    }

    if (colorspace == SIXEL_COLORSPACE_GAMMA) {
        for (channel = 0; channel < 3; ++channel) {
            unsigned char snapped;

            snapped = sixel_pixelformat_float_channel_to_byte(
                pixelformat,
                channel,
                components[channel]);
            snapped = sixel_palette_reversible_value(snapped);
            components[channel]
                = sixel_pixelformat_byte_to_float(pixelformat,
                                                  channel,
                                                  snapped);
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

    for (channel = 0; channel < 3; ++channel) {
        snapped_bytes[channel]
            = sixel_pixelformat_float_channel_to_byte(pixelformat,
                                                      channel,
                                                      working_palette[channel]);
        candidate_counts[channel] = 0;
        candidate_values[channel][candidate_counts[channel]++]
            = sixel_palette_reversible_value(snapped_bytes[channel]);
        candidate_values[channel][candidate_counts[channel]++]
            = sixel_palette_reversible_value(snapped_bytes[channel]
                                             > 0U
                                             ? snapped_bytes[channel] - 1U
                                             : 0U);
        candidate_values[channel][candidate_counts[channel]++]
            = sixel_palette_reversible_value(snapped_bytes[channel]
                                             < 255U
                                             ? snapped_bytes[channel] + 1U
                                             : 255U);
        snapped_bytes[channel] = candidate_values[channel][0];
    }

    if (snap_policy == SIXEL_PALETTE_SNAP_POLICY_NEAREST) {
        double best_distance;
        int c0;
        int c1;
        int c2;

        /*
         * Enumerate a 3x3x3 neighborhood in sRGB gamma space and choose the
         * candidate whose projection back into the target colorspace is nearest
         * to the original float components.
         */
        best_distance = 1.0e30;
        for (c0 = 0; c0 < candidate_counts[0]; ++c0) {
            for (c1 = 0; c1 < candidate_counts[1]; ++c1) {
                for (c2 = 0; c2 < candidate_counts[2]; ++c2) {
                    double distance;

                    candidate_gamma[0]
                        = sixel_pixelformat_byte_to_float(
                            SIXEL_PIXELFORMAT_RGBFLOAT32,
                            0,
                            candidate_values[0][c0]);
                    candidate_gamma[1]
                        = sixel_pixelformat_byte_to_float(
                            SIXEL_PIXELFORMAT_RGBFLOAT32,
                            1,
                            candidate_values[1][c1]);
                    candidate_gamma[2]
                        = sixel_pixelformat_byte_to_float(
                            SIXEL_PIXELFORMAT_RGBFLOAT32,
                            2,
                            candidate_values[2][c2]);

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

                    distance = 0.0;
                    for (channel = 0; channel < 3; ++channel) {
                        double diff;

                        diff = (double)candidate_target[channel]
                               - (double)target_original[channel];
                        distance += diff * diff;
                    }
                    if (distance < best_distance) {
                        best_distance = distance;
                        snapped_bytes[0] = candidate_values[0][c0];
                        snapped_bytes[1] = candidate_values[1][c1];
                        snapped_bytes[2] = candidate_values[2][c2];
                    }
                }
            }
        }
    } else {
        for (channel = 0; channel < 3; ++channel) {
            snapped_bytes[channel]
                = sixel_palette_reversible_value(snapped_bytes[channel]);
        }
    }

    for (channel = 0; channel < 3; ++channel) {
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
                          int channel)
{
    double clamped;
    double snapped;
    unsigned char sample;

    clamped = value;
    snapped = value;
    sample = 0U;
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        if (!use_reversible) {
            return (double)sixel_pixelformat_float_channel_clamp(
                pixelformat, channel, (float)value);
        }
        sample = sixel_pixelformat_float_channel_to_byte(pixelformat,
                                                         channel,
                                                         (float)value);
        if (use_reversible) {
            sample = sixel_palette_reversible_value(sample);
        }
        snapped = (double)sixel_pixelformat_byte_to_float(pixelformat,
                                                          channel,
                                                          sample);

        return snapped;
    }
    if (clamped < 0.0) {
        clamped = 0.0;
    }
    if (clamped > 255.0) {
        clamped = 255.0;
    }
    if (!use_reversible) {
        return clamped;
    }
    sample = (unsigned char)(clamped + 0.5);
    snapped = (double)sixel_palette_reversible_value((unsigned int)sample);

    return snapped;
}

void
sixel_palette_snap_triple(double *components,
                          int use_reversible,
                          int pixelformat)
{
    SIXELSTATUS status;
    float working[3];
    unsigned char bytes[3];
    int channel;

    status = SIXEL_OK;
    if (components == NULL) {
        return;
    }

    /*
     * Float palettes are round-tripped through sRGB bytes so the reversible
     * grid logic always works on the canonical 0-255 space.
     */
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        for (channel = 0; channel < 3; ++channel) {
            working[channel] = (float)components[channel];
        }
        status = sixel_palette_snap_float_triplet(
            working, use_reversible, pixelformat);
        if (SIXEL_FAILED(status)) {
            return;
        }
        for (channel = 0; channel < 3; ++channel) {
            components[channel] = (double)working[channel];
        }

        return;
    }

    /*
     * Byte palettes stay in-place; clamp to 0-255 and snap directly on the
     * reversible tone grid without passing through float conversions.
     */
    for (channel = 0; channel < 3; ++channel) {
        if (components[channel] < 0.0) {
            components[channel] = 0.0;
        }
        if (components[channel] > 255.0) {
            components[channel] = 255.0;
        }
        bytes[channel] = (unsigned char)(components[channel] + 0.5);
        if (use_reversible) {
            bytes[channel] = sixel_palette_reversible_value(bytes[channel]);
        }
        components[channel] = (double)bytes[channel];
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
