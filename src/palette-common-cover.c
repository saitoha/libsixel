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
 * The anchor points, ordered so that a prefix of the table is a complete
 * anchor set: the eight cube corners, then the six face centres, then the
 * twelve edge midpoints.  Faces come before edges because that is the order
 * measurement puts them in -- see the table in the header.
 */
static unsigned char const
sixel_palette_cover_anchors[SIXEL_PALETTE_COVER_ANCHOR_MAX][3] = {
    { 0x00u, 0x00u, 0x00u }, { 0xffu, 0x00u, 0x00u },
    { 0x00u, 0xffu, 0x00u }, { 0x00u, 0x00u, 0xffu },
    { 0xffu, 0xffu, 0x00u }, { 0xffu, 0x00u, 0xffu },
    { 0x00u, 0xffu, 0xffu }, { 0xffu, 0xffu, 0xffu },

    { 0x00u, 0x80u, 0x80u }, { 0xffu, 0x80u, 0x80u },
    { 0x80u, 0x00u, 0x80u }, { 0x80u, 0xffu, 0x80u },
    { 0x80u, 0x80u, 0x00u }, { 0x80u, 0x80u, 0xffu },

    { 0x80u, 0x00u, 0x00u }, { 0x80u, 0xffu, 0x00u },
    { 0x80u, 0x00u, 0xffu }, { 0x80u, 0xffu, 0xffu },
    { 0x00u, 0x80u, 0x00u }, { 0xffu, 0x80u, 0x00u },
    { 0x00u, 0x80u, 0xffu }, { 0xffu, 0x80u, 0xffu },
    { 0x00u, 0x00u, 0x80u }, { 0xffu, 0x00u, 0x80u },
    { 0x00u, 0xffu, 0x80u }, { 0xffu, 0xffu, 0x80u }
};

static int g_sixel_palette_cover_override_enabled;
static sixel_palette_cover_options_t g_sixel_palette_cover_override;

SIXEL_INTERNAL_API void
sixel_set_palette_cover_override(int enabled,
                                 sixel_palette_cover_options_t const *options)
{
    g_sixel_palette_cover_override_enabled =
        enabled != 0 && options != NULL ? 1 : 0;
    if (options != NULL) {
        g_sixel_palette_cover_override = *options;
    } else {
        g_sixel_palette_cover_override.policy = SIXEL_PALETTE_COVER_AUTO;
        g_sixel_palette_cover_override.grow = 0;
        g_sixel_palette_cover_override.mode = SIXEL_PALETTE_COVER_MODE_HARD;
    }
}

SIXEL_INTERNAL_API int
sixel_palette_cover_mode(void)
{
    /*
     * Soft anchoring does not exist yet, so every channel that cannot report
     * an error resolves to hard.  The option layer refuses it outright, which
     * is where a user who asks for it finds out.
     */
    return SIXEL_PALETTE_COVER_MODE_HARD;
}

SIXEL_INTERNAL_API int
sixel_palette_cover_policy(void)
{
    char const *value;

    if (g_sixel_palette_cover_override_enabled != 0) {
        return g_sixel_palette_cover_override.policy;
    }
    /*
     * The environment stays available for callers that build a palette
     * directly rather than through the encoder's option layer.
     */
    value = getenv("SIXEL_PALETTE_COVER");
    if (value == NULL) {
        return SIXEL_PALETTE_COVER_AUTO;
    }
    if (strcmp(value, "0") == 0 || strcmp(value, "off") == 0) {
        return SIXEL_PALETTE_COVER_OFF;
    }
    if (strcmp(value, "corners") == 0) {
        return SIXEL_PALETTE_COVER_CORNERS;
    }
    if (strcmp(value, "edges") == 0) {
        return SIXEL_PALETTE_COVER_EDGES;
    }
    if (strcmp(value, "faces") == 0) {
        return SIXEL_PALETTE_COVER_FACES;
    }

    return SIXEL_PALETTE_COVER_AUTO;
}

SIXEL_INTERNAL_API int
sixel_palette_cover_grow_enabled(void)
{
    char const *value;

    if (g_sixel_palette_cover_override_enabled != 0) {
        return g_sixel_palette_cover_override.grow;
    }
    value = getenv("SIXEL_PALETTE_COVER_GROW");

    return value != NULL && strcmp(value, "0") != 0 ? 1 : 0;
}

SIXEL_INTERNAL_API int
sixel_palette_cover_repair_enabled(void)
{
    return sixel_palette_cover_policy() != SIXEL_PALETTE_COVER_OFF;
}

SIXEL_INTERNAL_API int
sixel_palette_cover_resolve_policy(int policy, unsigned int entry_count)
{
    if (policy != SIXEL_PALETTE_COVER_AUTO) {
        return policy;
    }
    /*
     * Below 32 colors even the corners would claim a quarter of the palette,
     * and an image rendered in that few colors has bigger problems than an
     * unreachable face.
     */
    if (entry_count < 32u) {
        return SIXEL_PALETTE_COVER_OFF;
    }
    if (entry_count < 64u) {
        return SIXEL_PALETTE_COVER_CORNERS;
    }
    if (entry_count < 256u) {
        return SIXEL_PALETTE_COVER_FACES;
    }

    return SIXEL_PALETTE_COVER_EDGES;
}

static unsigned int
sixel_palette_cover_anchor_count(int policy)
{
    switch (policy) {
    case SIXEL_PALETTE_COVER_CORNERS:
        return 8u;
    case SIXEL_PALETTE_COVER_FACES:
        return 14u;
    case SIXEL_PALETTE_COVER_EDGES:
        return SIXEL_PALETTE_COVER_ANCHOR_MAX;
    default:
        return 0u;
    }
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

SIXEL_INTERNAL_API unsigned int
sixel_palette_cover_missing_anchors(unsigned char const *entries,
                                    unsigned int entry_count,
                                    int depth,
                                    int policy,
                                    unsigned char *out,
                                    unsigned int out_max)
{
    unsigned int wanted;
    unsigned int index;
    unsigned int found;

    found = 0u;
    if (entries == NULL || out == NULL || depth != 3) {
        return 0u;
    }
    wanted = sixel_palette_cover_anchor_count(
        sixel_palette_cover_resolve_policy(policy, entry_count));
    for (index = 0u; index < wanted && found < out_max; ++index) {
        unsigned char const *anchor;

        anchor = sixel_palette_cover_anchors[index];
        /*
         * Skip an anchor the palette already reaches.  The test depends only
         * on the palette, so it cannot make the anchor set flicker from frame
         * to frame on its own.
         */
        if (sixel_palette_cover_nearest_sq(anchor, entries, entry_count, depth)
                <= (unsigned int)SIXEL_PALETTE_COVER_NEAR_SQ) {
            continue;
        }
        memcpy(out + (size_t)found * 3u, anchor, 3u);
        found++;
    }

    return found;
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
     * Only merge while it costs less than the anchor gains.  A palette whose
     * entries are already spread as far apart as the gap being closed has
     * nothing cheap to give, and anchoring it would trade error for error.
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
    unsigned char wanted[SIXEL_PALETTE_COVER_ANCHOR_MAX * 3u];
    unsigned int count;
    unsigned int index;
    unsigned int gap;
    int slot;

    if (entries == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (depth != 3) {
        return SIXEL_OK;
    }
    count = sixel_palette_cover_missing_anchors(
        entries,
        entry_count,
        depth,
        sixel_palette_cover_policy(),
        wanted,
        SIXEL_PALETTE_COVER_ANCHOR_MAX);

    for (index = 0u; index < count; ++index) {
        unsigned char const *anchor;

        anchor = wanted + (size_t)index * 3u;
        /* Re-measure: an earlier anchor may already have covered this one. */
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
         * enough for the next: the anchors are a list, not a ranking.
         */
        if (slot < 0) {
            continue;
        }
        memcpy(entries + (size_t)slot * (size_t)depth, anchor, 3u);
    }

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
