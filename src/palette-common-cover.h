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

#ifndef LIBSIXEL_PALETTE_COMMON_COVER_H
#define LIBSIXEL_PALETTE_COMMON_COVER_H

#include <stddef.h>

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The eight corners of the RGB cube. */
#define SIXEL_PALETTE_COVER_ANCHOR_COUNT 8u

/*
 * An anchor closer than this to an existing entry is already reachable, so
 * placing it would spend a slot without enclosing anything new.
 */
#define SIXEL_PALETTE_COVER_NEAR_SQ (24 * 24 * 3)

/*
 * An anchor is funded by merging the closest pair of entries, and that only
 * pays off while the pair is closer together than the anchor is to the whole
 * palette: the merge costs about the distance between the pair, the anchor
 * buys about the distance it closes.  Comparing the two directly makes the
 * budget scale with the palette instead of assuming one.  A fixed threshold
 * cannot: k-center deliberately spreads its entries, so any constant tuned for
 * a median-cut palette rejects every merge and leaves it unanchored.
 */
#define SIXEL_PALETTE_COVER_MERGE_MARGIN 2u

/*
 * Anchor a finished palette to the gamut corners.
 *
 * Error diffusion reproduces a color the palette lacks by mixing entries that
 * surround it, and that works for anything strictly inside the palette hull:
 * a flat (200,60,40) region renders as (203,61,42) even when no entry is
 * close.  It breaks down at the edge of the gamut.  There the diffused error
 * points out of the RGB cube, the diffusion step clamps it away, and every
 * pixel of the region makes the identical wrong choice -- a saturated scrub
 * bar or status indicator comes out flat and wrong, and shifts color whenever
 * the surrounding image moves the cluster it was folded into.
 *
 * The solvers cannot fix this by tuning.  They minimize total error, so a
 * region worth a fraction of a percent of the image is always cheaper to
 * merge away than to keep.  Nor can a content-adaptive rescue work here: the
 * solvers see a sample of a few thousand pixels, in which such a region is
 * indistinguishable from noise.
 *
 * So the corners are placed from a fixed list rather than earned from the
 * image.  Each is funded by merging the closest pair of existing entries, so
 * what it costs is a near-duplicate; anchors the palette already reaches are
 * skipped, and anchoring stops once even the cheapest merge would cost real
 * error.  Because the list does not depend on image content, the anchor set is
 * the same from frame to frame, which matters as much as the color itself: an
 * anchor that came and went would reintroduce the flicker it exists to remove.
 *
 * Removing the diffusion clamp instead does not work, and was measured: with
 * no entry near the color the rendered result is unchanged -- no distribution
 * of palette entries can average outside their hull -- while the accumulated
 * error grows past ten times full scale and smears into neighbouring regions.
 *
 * ENTRIES is updated in place and ENTRY_COUNT does not change.  RGB888 only.
 */
SIXELAPI SIXELSTATUS
sixel_palette_cover_anchor_rgb888(
    unsigned char /* in out */ *entries,
    unsigned int  /* in */      entry_count,
    int           /* in */      depth);

/* Non-zero unless SIXEL_PALETTE_COVER=0 disables anchoring. */
SIXEL_INTERNAL_API int
sixel_palette_cover_repair_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_COMMON_COVER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
