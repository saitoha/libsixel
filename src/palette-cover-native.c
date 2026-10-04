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

/* Extended RGB cover repair for the selected native float palette path. */
#include "config.h"
#include <float.h>
#include <math.h>
#include <string.h>
#include <sixel.h>
#include "colorspace.h"
#include "pixelformat.h"
#include "palette-common-cover.h"

typedef struct sixel_cover_native_cell {
    unsigned int count;
    double rgb[3];
    double gap;
} sixel_cover_native_cell_t;

static double
sixel_cover_native_distance(double const *a, double const *b)
{
    double r;
    double g;
    double blue;

    r = a[0] - b[0];
    g = a[1] - b[1];
    blue = a[2] - b[2];
    return r * r + g * g + blue * blue;
}

static double
sixel_cover_native_nearest(double const *rgb, double const entries[][3],
                           unsigned int count)
{
    double best;
    double distance;
    unsigned int i;

    best = DBL_MAX;
    for (i = 0; i < count; ++i) {
        distance = sixel_cover_native_distance(rgb, entries[i]);
        if (distance < best) {
            best = distance;
        }
    }
    return best;
}

static int
sixel_cover_native_rgb(double rgb[3], float const *input, int space)
{
    double native[3];

    native[0] = input[0];
    native[1] = input[1];
    native[2] = input[2];
    return sixel_color_convert(rgb, native, space, SIXEL_COLORSPACE_GAMMA);
}

/* Keep distance in nominal gamma RGB, independently of -X and histogram
 * bounds. Expanding an address range must not downweight that channel. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_cover_float(float *entries, unsigned int *entry_count,
                           float const *data, unsigned int length,
                           int pixelformat, int policy, int mode, int grow,
                           unsigned int capacity,
                           sixel_allocator_t *allocator)
{
    unsigned int const side = 1u << SIXEL_PALETTE_COVER_CANDIDATE_BITS;
    unsigned int const cells_count = 1u <<
        (3u * SIXEL_PALETTE_COVER_CANDIDATE_BITS);
    double const near_sq = SIXEL_PALETTE_COVER_NEAR_SQ / (255.0 * 255.0);
    double palette[SIXEL_PALETTE_MAX][3];
    double anchors[SIXEL_PALETTE_COVER_ANCHOR_MAX][3];
    float result[SIXEL_PALETTE_MAX * 3];
    unsigned char changed[SIXEL_PALETTE_MAX];
    double low[3] = { 0, 0, 0 };
    double high[3] = { 1, 1, 1 };
    double rgb[3];
    double native[3];
    double distance;
    double best;
    double gap;
    double position;
    sixel_cover_native_cell_t *cells;
    unsigned char const *anchor;
    SIXELSTATUS status;
    unsigned int count;
    unsigned int samples;
    unsigned int budget;
    unsigned int threshold;
    unsigned int found;
    unsigned int chosen;
    unsigned int key;
    unsigned int bin;
    unsigned int i;
    unsigned int j;
    unsigned int left;
    unsigned int right;
    unsigned int round;
    unsigned int placed;
    int channel;
    int space;

    if (entries == NULL || entry_count == NULL || data == NULL ||
            allocator == NULL || *entry_count == 0 ||
            capacity == 0 || capacity > SIXEL_PALETTE_MAX ||
            *entry_count > capacity || length == 0 ||
            length % (3u * sizeof(float)) != 0 ||
            !SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat) ||
            (mode != SIXEL_PALETTE_COVER_MODE_SOFT &&
             mode != SIXEL_PALETTE_COVER_MODE_HARD)) {
        return SIXEL_BAD_ARGUMENT;
    }
    count = *entry_count;
    budget = sixel_palette_cover_float_budget(policy, count);
    if (budget == 0) {
        return SIXEL_OK;
    }
    space = sixel_pixelformat_get_colorspace(pixelformat);
    samples = length / (3u * sizeof(float));
    for (i = 0; i < count; ++i) {
        if (!sixel_cover_native_rgb(palette[i], entries + i * 3u, space)) {
            return SIXEL_BAD_INPUT;
        }
    }
    cells = NULL;
    status = SIXEL_BAD_INPUT;
    found = 0;
    if (mode == SIXEL_PALETTE_COVER_MODE_HARD) {
        /* Hard anchors are the historical nominal sRGB lattice, even for
         * P3 output. The face/edge coordinate is 128/255, not 1/2. Neither
         * source extrema nor output primaries redefine this policy. Only
         * candidate selection differs from soft; placement stays shared. */
        for (i = 0; i < budget; ++i) {
            anchor = sixel_palette_cover_fixed_anchor(i);
            for (channel = 0; channel < 3; ++channel) {
                rgb[channel] = anchor[channel] / 255.0;
            }
            if (sixel_cover_native_nearest(rgb, palette, count) > near_sq) {
                memcpy(anchors[found++], rgb, sizeof(rgb));
            }
        }
        goto place;
    }
    /* Only histogram addresses are bounded. Keep source coordinates intact,
     * including negative and above-one values supplied by loader CMS. */
    for (i = 0; i < samples; ++i) {
        if (!sixel_cover_native_rgb(rgb, data + i * 3u, space)) {
            return SIXEL_BAD_INPUT;
        }
        for (channel = 0; channel < 3; ++channel) {
            if (rgb[channel] < low[channel]) {
                low[channel] = rgb[channel];
            }
            if (rgb[channel] > high[channel]) {
                high[channel] = rgb[channel];
            }
        }
    }
    cells = sixel_allocator_calloc(allocator, cells_count, sizeof(*cells));
    if (cells == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    status = SIXEL_BAD_INPUT;
    for (i = 0; i < samples; ++i) {
        if (!sixel_cover_native_rgb(rgb, data + i * 3u, space)) {
            goto end;
        }
        key = 0;
        for (channel = 0; channel < 3; ++channel) {
            position = (rgb[channel] - low[channel]) /
                (high[channel] - low[channel]) * side;
            bin = position >= side ? side - 1u : (unsigned int)position;
            key = key * side + bin;
        }
        cells[key].count++;
        for (channel = 0; channel < 3; ++channel) {
            cells[key].rgb[channel] += rgb[channel];
        }
    }
    threshold = samples / SIXEL_PALETTE_COVER_CANDIDATE_SHARE;
    if (threshold < SIXEL_PALETTE_COVER_CANDIDATE_MIN) {
        threshold = SIXEL_PALETTE_COVER_CANDIDATE_MIN;
    }
    for (i = 0; i < cells_count; ++i) {
        if (cells[i].count < threshold) {
            continue;
        }
        for (channel = 0; channel < 3; ++channel) {
            cells[i].rgb[channel] /= cells[i].count;
        }
        cells[i].gap = sixel_cover_native_nearest(cells[i].rgb,
            palette, count);
    }
    /* Cache nearest distances: each new anchor needs one distance per cell,
     * not another scan of the whole palette. Ties retain histogram order. */
    found = 0;
    while (found < budget) {
        best = near_sq;
        chosen = cells_count;
        for (i = 0; i < cells_count; ++i) {
            if (cells[i].count >= threshold && cells[i].gap > best) {
                chosen = i;
                best = cells[i].gap;
            }
        }
        if (chosen == cells_count) {
            break;
        }
        memcpy(anchors[found], cells[chosen].rgb, sizeof(anchors[found]));
        for (i = 0; i < cells_count; ++i) {
            if (cells[i].count < threshold) {
                continue;
            }
            distance = sixel_cover_native_distance(cells[i].rgb,
                anchors[found]);
            if (distance < cells[i].gap) {
                cells[i].gap = distance;
            }
        }
        found++;
    }
place:
    memset(changed, 0, sizeof(changed));
    for (i = 0; grow && i < found && count < capacity; ++i) {
        memcpy(palette[count], anchors[i], sizeof(palette[count]));
        changed[count++] = 1;
    }
    for (round = 0; round < SIXEL_PALETTE_COVER_MAX_ROUNDS; ++round) {
        placed = 0;
        for (i = 0; i < found; ++i) {
            gap = sixel_cover_native_nearest(anchors[i], palette, count);
            if (gap <= near_sq || count < 2) {
                continue;
            }
            best = DBL_MAX;
            left = 0;
            right = 1;
            for (j = 0; j + 1u < count; ++j) {
                for (key = j + 1u; key < count; ++key) {
                    distance = sixel_cover_native_distance(palette[j],
                        palette[key]);
                    if (distance < best) {
                        best = distance;
                        left = j;
                        right = key;
                    }
                }
            }
            if (best > gap / SIXEL_PALETTE_COVER_MERGE_MARGIN) {
                continue;
            }
            for (channel = 0; channel < 3; ++channel) {
                palette[left][channel] = palette[left][channel] * .5 +
                    palette[right][channel] * .5;
            }
            memcpy(palette[right], anchors[i], sizeof(palette[right]));
            changed[left] = changed[right] = 1;
            placed++;
        }
        if (placed == 0) {
            break;
        }
    }
    /* Do not round-trip untouched native entries. Convert every changed slot
     * before committing any result so a failed conversion is atomic. */
    memcpy(result, entries, *entry_count * 3u * sizeof(float));
    for (i = 0; i < count; ++i) {
        if (!changed[i]) {
            continue;
        }
        if (!sixel_color_convert(native, palette[i], SIXEL_COLORSPACE_GAMMA,
                space)) {
            goto end;
        }
        for (channel = 0; channel < 3; ++channel) {
            if (!isfinite(native[channel]) ||
                    fabs(native[channel]) > FLT_MAX) {
                goto end;
            }
            result[i * 3u + channel] = (float)native[channel];
        }
    }
    memcpy(entries, result, count * 3u * sizeof(float));
    *entry_count = count;
    status = SIXEL_OK;
end:
    sixel_allocator_free(allocator, cells);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "native cover: invalid color conversion.");
    }
    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
