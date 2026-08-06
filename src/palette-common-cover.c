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

#include "config.h"

#include <stdio.h>
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif

#include <sixel.h>

#include "palette-common-cover.h"

/*
 * The eight corners of the RGB cube.  Error diffusion fails exactly at the
 * gamut boundary, so these are the colors worth guaranteeing; everything
 * strictly inside the palette hull is reachable by dithering already.
 */
static unsigned char const
sixel_palette_cover_anchors[SIXEL_PALETTE_COVER_ANCHOR_COUNT][3] = {
    { 0x00u, 0x00u, 0x00u },
    { 0xffu, 0x00u, 0x00u },
    { 0x00u, 0xffu, 0x00u },
    { 0x00u, 0x00u, 0xffu },
    { 0xffu, 0xffu, 0x00u },
    { 0xffu, 0x00u, 0xffu },
    { 0x00u, 0xffu, 0xffu },
    { 0xffu, 0xffu, 0xffu }
};

/*
 * Encoder-supplied override.  File scope matches how the other palette
 * suboptions reach their solvers: the encoder sets it while applying quantize
 * model options and clears it once the frame is done.
 */
static int g_sixel_palette_cover_override_enabled;
static int g_sixel_palette_cover_override_value;

SIXEL_INTERNAL_API void
sixel_set_palette_cover_override(int enabled, int value)
{
    g_sixel_palette_cover_override_enabled = enabled != 0 ? 1 : 0;
    g_sixel_palette_cover_override_value = value != 0 ? 1 : 0;
}

SIXEL_INTERNAL_API int
sixel_palette_cover_repair_enabled(void)
{
    char const *value;

    if (g_sixel_palette_cover_override_enabled != 0) {
        return g_sixel_palette_cover_override_value;
    }
    /*
     * The environment stays available for callers that build a palette
     * directly rather than through the encoder's option layer.
     */
    value = getenv("SIXEL_PALETTE_COVER");
    if (value != NULL && value[0] == '0' && value[1] == '\0') {
        return 0;
    }

    return 1;
}

static unsigned int
sixel_palette_cover_distance_sq(unsigned char const *a, unsigned char const *b)
{
    int dr;
    int dg;
    int db;

    dr = (int)a[0] - (int)b[0];
    dg = (int)a[1] - (int)b[1];
    db = (int)a[2] - (int)b[2];

    return (unsigned int)(dr * dr + dg * dg + db * db);
}

static unsigned int
sixel_palette_cover_nearest_sq(unsigned char const *rgb,
                               unsigned char const *entries,
                               unsigned int entry_count,
                               int depth)
{
    unsigned int best;
    unsigned int index;
    unsigned int candidate;

    best = ~0u;
    for (index = 0u; index < entry_count; ++index) {
        candidate = sixel_palette_cover_distance_sq(
            rgb,
            entries + (size_t)index * (size_t)depth);
        if (candidate < best) {
            best = candidate;
        }
    }

    return best;
}

/*
 * Merge the two closest palette entries into their midpoint and report the
 * slot that fell free.  The pair is chosen purely on distance, so what
 * disappears is a near-duplicate -- the cheapest way to fund an anchor.
 */
static int
sixel_palette_cover_free_slot(unsigned char *entries,
                              unsigned int entry_count,
                              int depth,
                              unsigned int merge_budget_sq)
{
    unsigned int i;
    unsigned int j;
    unsigned int best_distance;
    unsigned int distance;
    unsigned int best_i;
    unsigned int best_j;
    unsigned char *left;
    unsigned char *right;
    int channel;

    if (entry_count < 2u) {
        return -1;
    }
    best_distance = ~0u;
    best_i = 0u;
    best_j = 1u;
    for (i = 0u; i + 1u < entry_count; ++i) {
        for (j = i + 1u; j < entry_count; ++j) {
            distance = sixel_palette_cover_distance_sq(
                entries + (size_t)i * (size_t)depth,
                entries + (size_t)j * (size_t)depth);
            if (distance < best_distance) {
                best_distance = distance;
                best_i = i;
                best_j = j;
            }
        }
    }

    /*
     * Only merge while it costs less than the anchor gains.  A palette holding
     * near-duplicates gives up a slot for almost nothing; one whose entries are
     * already spread as far apart as the gap being closed has nothing cheap to
     * give, and anchoring it would trade error for error.
     */
    if (best_distance > merge_budget_sq) {
        return -1;
    }

    left = entries + (size_t)best_i * (size_t)depth;
    right = entries + (size_t)best_j * (size_t)depth;
    for (channel = 0; channel < depth && channel < 3; ++channel) {
        left[channel] =
            (unsigned char)(((int)left[channel] + (int)right[channel]) / 2);
    }

    return (int)best_j;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_cover_anchor_rgb888(unsigned char *entries,
                                  unsigned int entry_count,
                                  int depth)
{
    unsigned int index;
    unsigned int placed;
    unsigned int gap;
    int slot;

    if (entries == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (depth != 3) {
        return SIXEL_OK;
    }
    /*
     * Below this the palette is too small to give up a quarter of itself, and
     * an image rendered in a handful of colors has bigger problems than an
     * unreachable corner.
     */
    if (entry_count < SIXEL_PALETTE_COVER_ANCHOR_COUNT * 4u) {
        return SIXEL_OK;
    }
    if (!sixel_palette_cover_repair_enabled()) {
        return SIXEL_OK;
    }

    placed = 0u;
    for (index = 0u; index < SIXEL_PALETTE_COVER_ANCHOR_COUNT; ++index) {
        unsigned char const *anchor;

        anchor = sixel_palette_cover_anchors[index];
        /*
         * Skip an anchor the palette already reaches.  This is a pure budget
         * saving: an entry that close changes nothing about which colors are
         * enclosed, and the test depends only on the palette, so it cannot
         * make the anchor set flicker from frame to frame on its own.
         */
        gap = sixel_palette_cover_nearest_sq(anchor,
                                             entries,
                                             entry_count,
                                             depth);
        if (gap <= (unsigned int)SIXEL_PALETTE_COVER_NEAR_SQ) {
            continue;
        }
        slot = sixel_palette_cover_free_slot(
            entries,
            entry_count,
            depth,
            gap / SIXEL_PALETTE_COVER_MERGE_MARGIN);
        /*
         * Nothing cheap enough for this anchor does not mean nothing cheap
         * enough for the next: the anchors are ordered, not ranked, so keep
         * going rather than abandoning the remaining corners.
         */
        if (slot < 0) {
            continue;
        }
        memcpy(entries + (size_t)slot * (size_t)depth, anchor, 3u);
        placed++;
    }
    (void)placed;

    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
