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

static unsigned int
sixel_palette_cover_anchor_count(int policy);

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
    char const *value;

    if (g_sixel_palette_cover_override_enabled != 0) {
        return g_sixel_palette_cover_override.mode;
    }
    value = getenv("SIXEL_PALETTE_COVER_MODE");
    if (value != NULL && strcmp(value, "hard") == 0) {
        return SIXEL_PALETTE_COVER_MODE_HARD;
    }

    return SIXEL_PALETTE_COVER_MODE_SOFT;
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
    if (strcmp(value, "faces") == 0) {
        return SIXEL_PALETTE_COVER_FACES;
    }
    if (strcmp(value, "edges") == 0) {
        return SIXEL_PALETTE_COVER_EDGES;
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
     * Below 32 colors the anchors would claim a quarter of the palette, and an
     * image rendered in that few colors has bigger problems than an
     * unreachable region.
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

SIXEL_INTERNAL_API unsigned int
sixel_palette_cover_budget(int policy, unsigned int entry_count)
{
    return sixel_palette_cover_anchor_count(
        sixel_palette_cover_resolve_policy(policy, entry_count));
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

SIXEL_INTERNAL_API void
sixel_palette_cover_place(unsigned char *entries,
                          unsigned int entry_count,
                          int depth,
                          unsigned char const *anchors,
                          unsigned int anchor_count)
{
    unsigned int round;
    unsigned int index;
    unsigned int placed;
    unsigned int gap;
    int slot;

    if (entries == NULL || anchors == NULL || depth != 3) {
        return;
    }
    /*
     * Placing is iterated rather than done in one sweep, because funding an
     * anchor moves a pair of entries to their midpoint and that can pull an
     * entry away from an anchor already judged reachable.  Measured on an
     * inset fixture, an anchor sitting 40.8 away -- inside the reach
     * threshold, so skipped -- was left 49.6 away by a merge made for an
     * earlier anchor, and the single sweep never looked at it again.
     *
     * A round that places nothing terminates the loop, so the cap is only a
     * backstop; each round either makes progress or is the last.
     */
    for (round = 0u; round < SIXEL_PALETTE_COVER_MAX_ROUNDS; ++round) {
        placed = 0u;
        for (index = 0u; index < anchor_count; ++index) {
            unsigned char const *anchor;

            anchor = anchors + (size_t)index * 3u;
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
             * Nothing cheap enough for this anchor does not mean nothing
             * cheap enough for the next: the anchors are a list, not a
             * ranking.
             */
            if (slot < 0) {
                continue;
            }
            memcpy(entries + (size_t)slot * (size_t)depth, anchor, 3u);
            placed++;
        }
        if (placed == 0u) {
            break;
        }
    }
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_cover_anchor_rgb888(unsigned char *entries,
                                  unsigned int entry_count,
                                  int depth)
{
    unsigned char wanted[SIXEL_PALETTE_COVER_ANCHOR_MAX * 3u];
    unsigned int count;

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
    sixel_palette_cover_place(entries, entry_count, depth, wanted, count);

    return SIXEL_OK;
}

/*
 * One histogram cell: how many samples fell in it, and their sum so the
 * representative can be their mean.
 *
 * The mean of the samples in a cell is a color the image contains, up to the
 * spread within one cell.  A cell CENTRE would not be -- it is a constructed
 * color again, and constructing colors is what this whole path exists to stop.
 */
typedef struct sixel_palette_cover_cell {
    unsigned int count;
    unsigned int sum[3];
} sixel_palette_cover_cell_t;

/*
 * Only the byte-per-channel formats that actually reach a solver are read.
 * Anything else -- float32, sub-byte packings, paletted input -- is handed to
 * hard, which is the behavior those formats have today.
 */
static int
sixel_palette_cover_reader(int pixelformat,
                           unsigned int *stride,
                           unsigned int *offset)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
    case SIXEL_PIXELFORMAT_BGR888:
        *stride = 3u; *offset = 0u; return 1;
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
        *stride = 4u; *offset = 0u; return 1;
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
        *stride = 4u; *offset = 1u; return 1;
    default:
        break;
    }

    return 0;
}

SIXEL_INTERNAL_API int
sixel_palette_cover_candidates_supported(int pixelformat)
{
    unsigned int stride;
    unsigned int offset;

    return sixel_palette_cover_reader(pixelformat, &stride, &offset);
}

SIXEL_INTERNAL_API unsigned int
sixel_palette_cover_collect_candidates(void const *data,
                                       unsigned int length,
                                       int pixelformat,
                                       unsigned char const *entries,
                                       unsigned int entry_count,
                                       int depth,
                                       unsigned char *out,
                                       unsigned int out_max,
                                       sixel_allocator_t *allocator)
{
    sixel_palette_cover_cell_t *cells;
    unsigned char const *bytes;
    unsigned int const bits = SIXEL_PALETTE_COVER_CANDIDATE_BITS;
    unsigned int const cell_count = 1u << (bits * 3u);
    unsigned int const shift = 8u - bits;
    unsigned int stride;
    unsigned int offset;
    unsigned int pixels;
    unsigned int threshold;
    unsigned int i;
    unsigned int found;
    int channel;

    found = 0u;
    if (data == NULL || out == NULL || entries == NULL || allocator == NULL
            || depth != 3 || out_max == 0u || length == 0u
            || entry_count == 0u) {
        return 0u;
    }
    if (!sixel_palette_cover_reader(pixelformat, &stride, &offset)) {
        return 0u;
    }
    pixels = length / stride;
    if (pixels == 0u) {
        return 0u;
    }
    cells = (sixel_palette_cover_cell_t *)sixel_allocator_calloc(
        allocator,
        (size_t)cell_count,
        sizeof(sixel_palette_cover_cell_t));
    if (cells == NULL) {
        return 0u;
    }
    bytes = (unsigned char const *)data;
    for (i = 0u; i < pixels; ++i) {
        unsigned char const *px;
        unsigned int key;

        px = bytes + (size_t)i * (size_t)stride + offset;
        key = ((unsigned int)(px[0] >> shift) << (bits * 2u))
            | ((unsigned int)(px[1] >> shift) << bits)
            | (unsigned int)(px[2] >> shift);
        cells[key].count++;
        for (channel = 0; channel < 3; ++channel) {
            cells[key].sum[channel] += px[channel];
        }
    }
    threshold = pixels / SIXEL_PALETTE_COVER_CANDIDATE_SHARE;
    if (threshold < SIXEL_PALETTE_COVER_CANDIDATE_MIN) {
        threshold = SIXEL_PALETTE_COVER_CANDIDATE_MIN;
    }

    /*
     * Take the qualifying cell that is farthest from everything chosen so far
     * -- the palette plus the anchors already taken -- and repeat.  Ranking by
     * distance rather than by mass is the point: the most populous colors are
     * exactly the ones the solver already spent entries on, so ranking by mass
     * would nominate colors that are all skipped as already reached.
     */
    while (found < out_max) {
        unsigned int best_cell;
        unsigned int best_gap;
        int have_best;

        best_cell = 0u;
        best_gap = 0u;
        have_best = 0;
        for (i = 0u; i < cell_count; ++i) {
            unsigned char rgb[3];
            unsigned int gap;

            if (cells[i].count < threshold) {
                continue;
            }
            for (channel = 0; channel < 3; ++channel) {
                rgb[channel] =
                    (unsigned char)(cells[i].sum[channel] / cells[i].count);
            }
            gap = sixel_palette_cover_nearest_sq(rgb,
                                                 entries,
                                                 entry_count,
                                                 depth);
            if (found > 0u) {
                unsigned int taken;

                taken = sixel_palette_cover_nearest_sq(rgb, out, found, 3);
                if (taken < gap) {
                    gap = taken;
                }
            }
            if (gap > best_gap) {
                best_gap = gap;
                best_cell = i;
                have_best = 1;
            }
        }
        if (!have_best
                || best_gap <= (unsigned int)SIXEL_PALETTE_COVER_NEAR_SQ) {
            break;
        }
        for (channel = 0; channel < 3; ++channel) {
            out[(size_t)found * 3u + (size_t)channel] =
                (unsigned char)(cells[best_cell].sum[channel]
                                / cells[best_cell].count);
        }
        found++;
    }
    sixel_allocator_free(allocator, cells);

    return found;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
