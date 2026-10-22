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

/*
 * Exact block-culled lookup for native float palettes.
 *
 * A run of consecutive samples spans an axis-aligned box. For each color c,
 * maxdist(c) bounds the distance from c to every point of the box, so
 * U = min_c maxdist(c) bounds every sample's nearest distance. A color whose
 * box distance mindist(c) exceeds U is farther than that color at every
 * point and can be skipped. Survivors keep palette order, so the final
 * strict first-minimum scan returns the exhaustive scan's index, ties
 * included.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <string.h>
#include "lookup-blockcull-float32.h"

enum {
    SIXEL_BLOCKCULL_FLOAT_RUN = 32,
    SIXEL_BLOCKCULL_FLOAT_MAX_COLORS = SIXEL_PALETTE_MAX
};

/* Computed distances carry a relative error of a few units in the last
 * place. Keeping colors within this relative slack of U makes every skipped
 * color strictly farther, in the computed metric, than the color that
 * attains U. */
#define SIXEL_BLOCKCULL_FLOAT_SLACK (1.0 + 1e-9)

struct sixel_lookup_blockcull_float32 {
    sixel_allocator_t *allocator;
    int count;
    /* Nonzero when the palette and weights are finite, which the box bounds
     * require; otherwise every lookup scans the palette. */
    int cullable;
    double palette[SIXEL_BLOCKCULL_FLOAT_MAX_COLORS * 3];
    double weights[3];
};

/* The VP-tree float expression: channel order, double differences, squared
 * difference times weight, accumulated from zero. */
static double
sixel_blockcull_float_distance(sixel_lookup_blockcull_float32_t const *lookup,
                               float const *pixel, int index)
{
    double const *entry;
    double distance;
    double diff;
    int c;

    entry = lookup->palette + (size_t)index * 3U;
    distance = 0.0;
    for (c = 0; c < 3; ++c) {
        diff = (double)pixel[c] - entry[c];
        distance += diff * diff * lookup->weights[c];
    }
    return distance;
}

/* NaNs never win a strict comparison, and an all-nonfinite scan returns
 * index zero, as every exhaustive float scan initialized to DBL_MAX. */
static int
sixel_blockcull_float_scan(sixel_lookup_blockcull_float32_t const *lookup,
                           float const *pixel, int const *candidates,
                           int ncandidates)
{
    double distance;
    double minimum;
    int best;
    int i;
    int index;

    best = 0;
    minimum = DBL_MAX;
    for (i = 0; i < ncandidates; ++i) {
        index = candidates != NULL ? candidates[i] : i;
        distance = sixel_blockcull_float_distance(lookup, pixel, index);
        if (distance < minimum) {
            minimum = distance;
            best = index;
        }
    }
    return best;
}

SIXELSTATUS
sixel_lookup_blockcull_float32_create(
    sixel_allocator_t *allocator,
    float const *palette,
    int ncolors,
    float const *weights,
    sixel_lookup_blockcull_float32_t **lookup_out)
{
    sixel_lookup_blockcull_float32_t *lookup;
    int i;

    if (lookup_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *lookup_out = NULL;
    if (allocator == NULL || palette == NULL || weights == NULL
            || ncolors <= 0 || ncolors > SIXEL_BLOCKCULL_FLOAT_MAX_COLORS) {
        return SIXEL_BAD_ARGUMENT;
    }
    lookup = (sixel_lookup_blockcull_float32_t *)sixel_allocator_malloc(
        allocator, sizeof(*lookup));
    if (lookup == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    memset(lookup, 0, sizeof(*lookup));
    lookup->allocator = allocator;
    lookup->count = ncolors;
    lookup->cullable = 1;
    for (i = 0; i < 3; ++i) {
        lookup->weights[i] = weights[i];
        if (!isfinite(lookup->weights[i]) || lookup->weights[i] < 0.0) {
            lookup->cullable = 0;
        }
    }
    for (i = 0; i < ncolors * 3; ++i) {
        lookup->palette[i] = palette[i];
        if (!isfinite(lookup->palette[i])) {
            lookup->cullable = 0;
        }
    }
    *lookup_out = lookup;
    return SIXEL_OK;
}

void
sixel_lookup_blockcull_float32_destroy(
    sixel_lookup_blockcull_float32_t *lookup)
{
    if (lookup != NULL) {
        sixel_allocator_free(lookup->allocator, lookup);
    }
}

int
sixel_lookup_blockcull_float32_map(
    sixel_lookup_blockcull_float32_t const *lookup,
    float const *pixel)
{
    if (lookup == NULL || pixel == NULL) {
        return 0;
    }
    return sixel_blockcull_float_scan(lookup, pixel, NULL, lookup->count);
}

/* Collect the colors that can be nearest to some point of the run's box.
 * Returns -1 when the box bounds are unusable, so the run is scanned. */
static int
sixel_blockcull_float_candidates(
    sixel_lookup_blockcull_float32_t const *lookup,
    float const (*samples)[3], int nsamples, int *candidates)
{
    double low[3];
    double high[3];
    double bound;
    double far;
    double near;
    double a;
    double b;
    double gap;
    double const *entry;
    int count;
    int i;
    int c;

    for (c = 0; c < 3; ++c) {
        low[c] = samples[0][c];
        high[c] = samples[0][c];
    }
    for (i = 0; i < nsamples; ++i) {
        for (c = 0; c < 3; ++c) {
            if (!isfinite(samples[i][c])) {
                return -1;
            }
            if (samples[i][c] < low[c]) {
                low[c] = samples[i][c];
            }
            if (samples[i][c] > high[c]) {
                high[c] = samples[i][c];
            }
        }
    }
    bound = DBL_MAX;
    for (i = 0; i < lookup->count; ++i) {
        entry = lookup->palette + (size_t)i * 3U;
        far = 0.0;
        for (c = 0; c < 3; ++c) {
            a = entry[c] - low[c];
            b = high[c] - entry[c];
            gap = a > b ? a : b;
            far += gap * gap * lookup->weights[c];
        }
        if (far < bound) {
            bound = far;
        }
    }
    bound *= SIXEL_BLOCKCULL_FLOAT_SLACK;
    if (!isfinite(bound)) {
        return -1;
    }
    count = 0;
    for (i = 0; i < lookup->count; ++i) {
        entry = lookup->palette + (size_t)i * 3U;
        near = 0.0;
        for (c = 0; c < 3; ++c) {
            gap = 0.0;
            if (entry[c] < low[c]) {
                gap = low[c] - entry[c];
            } else if (entry[c] > high[c]) {
                gap = entry[c] - high[c];
            }
            near += gap * gap * lookup->weights[c];
        }
        if (near <= bound) {
            candidates[count++] = i;
        }
    }
    return count;
}

void
sixel_lookup_blockcull_float32_map_pixels(
    sixel_lookup_blockcull_float32_t const *lookup,
    unsigned char const *pixels, size_t count, int pixel_stride,
    sixel_index_t *result, int result_stride)
{
    float samples[SIXEL_BLOCKCULL_FLOAT_RUN][3];
    int candidates[SIXEL_BLOCKCULL_FLOAT_MAX_COLORS];
    int ncandidates;
    size_t done;
    size_t i;
    size_t n;

    if (lookup == NULL || count == 0U) {
        return;
    }
    done = 0U;
    while (done < count) {
        n = count - done;
        if (n > SIXEL_BLOCKCULL_FLOAT_RUN) {
            n = SIXEL_BLOCKCULL_FLOAT_RUN;
        }
        for (i = 0; i < n; ++i) {
            memcpy(samples[i], pixels, sizeof(samples[i]));
            /* Do not form a pointer before the buffer after a reverse row. */
            if (done + i + 1U < count) {
                pixels += pixel_stride;
            }
        }
        ncandidates = -1;
        if (lookup->cullable) {
            ncandidates = sixel_blockcull_float_candidates(
                lookup, (float const (*)[3])samples, (int)n, candidates);
        }
        for (i = 0; i < n; ++i) {
            *result = (sixel_index_t)(ncandidates < 0
                ? sixel_blockcull_float_scan(lookup, samples[i], NULL,
                                             lookup->count)
                : sixel_blockcull_float_scan(lookup, samples[i], candidates,
                                             ncandidates));
            if (done + i + 1U < count) {
                result += result_stride;
            }
        }
        done += n;
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
