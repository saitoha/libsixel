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
#include "lookup-common.h"
#include "palette-common-snap.h"
#include "pixelformat.h"

static int
sixel_palette_determine_colorspace(int pixelformat);
static void
sixel_palette_clamp_float_triplet(float *components, int pixelformat);
static SIXELSTATUS
sixel_palette_snap_float_triplet(float *components,
                                 int use_reversible,
                                 int pixelformat);

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
    float *working_palette;
    unsigned char *snapped_bytes;
    size_t palette_channels;
    size_t palette_bytes_len;
    size_t index;
    size_t color_index;
    int colorspace;
    int channel_count;
    int channel;

    status = SIXEL_OK;
    working_palette = NULL;
    snapped_bytes = NULL;
    palette_channels = 0U;
    palette_bytes_len = 0U;
    index = 0U;
    color_index = 0U;
    colorspace = sixel_palette_determine_colorspace(pixelformat);
    channel_count = 0;
    channel = 0;

    if (palette == NULL || colors == 0U) {
        return;
    }

    /*
     * Preserve reversible snapping for OKLab palettes by round-tripping
     * through sRGB gamma before re-encoding onto the 101-tone grid.
     */
    if (colorspace == SIXEL_COLORSPACE_OKLAB) {
        palette_channels = (size_t)colors * 3U;
        if (palette_channels > SIZE_MAX / sizeof(float)) {
            return;
        }
        palette_bytes_len = palette_channels * sizeof(float);
        working_palette = (float *)malloc(palette_bytes_len);
        if (working_palette == NULL) {
            return;
        }
        snapped_bytes = (unsigned char *)malloc(palette_channels);
        if (snapped_bytes == NULL) {
            free(working_palette);
            return;
        }

        memcpy(working_palette, palette, palette_bytes_len);
        sixel_palette_clamp_float_triplet(working_palette, pixelformat);
        status = sixel_helper_convert_colorspace(
            (unsigned char *)working_palette,
            palette_bytes_len,
            SIXEL_PIXELFORMAT_OKLABFLOAT32,
            SIXEL_COLORSPACE_OKLAB,
            SIXEL_COLORSPACE_GAMMA);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }

        for (index = 0U; index < palette_channels; ++index) {
            channel = (int)(index % 3U);
            snapped_bytes[index]
                = sixel_pixelformat_float_channel_to_byte(
                    SIXEL_PIXELFORMAT_RGBFLOAT32,
                    channel,
                    working_palette[index]);
            snapped_bytes[index]
                = sixel_palette_reversible_value(snapped_bytes[index]);
            working_palette[index]
                = sixel_pixelformat_byte_to_float(
                    SIXEL_PIXELFORMAT_RGBFLOAT32,
                    channel,
                    snapped_bytes[index]);
        }

        status = sixel_helper_convert_colorspace(
            (unsigned char *)working_palette,
            palette_bytes_len,
            SIXEL_PIXELFORMAT_RGBFLOAT32,
            SIXEL_COLORSPACE_GAMMA,
            SIXEL_COLORSPACE_OKLAB);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }

        sixel_palette_clamp_float_triplet(working_palette, pixelformat);
        memcpy(palette, working_palette, palette_bytes_len);

cleanup:
        free(snapped_bytes);
        free(working_palette);
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
    unsigned char snapped_bytes[3];
    int colorspace;
    int original_pixelformat;
    int channel;

    status = SIXEL_OK;
    colorspace = sixel_palette_determine_colorspace(pixelformat);
    original_pixelformat = pixelformat;
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

    memcpy(working_palette, components, sizeof(working_palette));
    sixel_palette_clamp_float_triplet(working_palette,
                                      original_pixelformat);
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
