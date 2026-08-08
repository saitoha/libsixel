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

/*
 * Anchor sets, in increasing order of coverage.  Each contains the previous.
 *
 * Error diffusion reproduces a color the palette lacks by mixing entries that
 * the nearest-color lookup actually selects, and it either finds that mixture
 * or it does not: measured on a frozen palette, the residual error of a flat
 * patch is bimodal, sitting either at zero or at the full distance to the
 * nearest entry, with almost nothing in between.  The second case is diffusion
 * never happening at all -- the pixel resolves to one entry and stays there.
 *
 * Percentage of probe colors stuck that way, palette held fixed at 64 entries
 * and each set funded by the same merge, so only the choice of anchors differs:
 *
 *                        interior  face  edge  corner   all
 *   no anchors               86%   100%  100%    100%   96%
 *   corners       (8)        34%    74%   15%     12%   47%
 *   + face centres (14)       0%    12%   15%     12%    9%
 *   + edge mids    (26)       0%     4%   10%     12%    5%
 *
 * The face centres are what matter, and they are why this ladder is not the
 * geometric one: anchoring the twelve edge midpoints instead costs six more
 * slots and leaves 26% stuck, because a color with one channel pinned at 0 or
 * 255 needs partners that share that extreme.  The clamp means error in a
 * pinned channel can only push inward, so an edge color can alternate between
 * its two adjacent corners and converge, but a face is two-dimensional and its
 * four corners are too far to be selected -- the lookup takes an entry just
 * inside the face and the region locks onto it.  Only a partner on that same
 * face breaks the lock.
 *
 * The corner column does not move because one of the eight probes is a blind
 * spot no anchor set reaches; anchoring cannot help a color that already is a
 * palette entry.
 */
#define SIXEL_PALETTE_COVER_OFF     0  /* no anchoring */
#define SIXEL_PALETTE_COVER_CORNERS 1  /* 8 cube corners */
#define SIXEL_PALETTE_COVER_FACES   2  /* + 6 face centres = 14 */
#define SIXEL_PALETTE_COVER_EDGES   3  /* + 12 edge midpoints = 26 */
#define SIXEL_PALETTE_COVER_AUTO    4  /* choose by palette size */

#define SIXEL_PALETTE_COVER_ANCHOR_MAX 26u

/*
 * How the anchors are chosen.
 *
 * HARD places the fixed lattice above and nothing else.  It cannot miss a
 * region of the cube, and it cannot know which regions the image actually
 * uses -- the palette is built from a ~4096-pixel subsample, so by the time
 * anchoring runs the content is no longer available to look at.
 *
 * SOFT is reserved for the opposite trade: choose the anchors from the image
 * itself -- the histogram, or a thumbnail -- so that a color the image leans
 * on is protected even when it sits nowhere near a lattice point, and lattice
 * points the image never approaches cost nothing.  It is not implemented; the
 * option layer rejects it rather than quietly running HARD, so that a
 * configuration asking for it does not change meaning when it lands.
 */
#define SIXEL_PALETTE_COVER_MODE_HARD 0
#define SIXEL_PALETTE_COVER_MODE_SOFT 1

/*
 * The per-channel extent of the colors the solver was given.  Soft anchoring
 * places the lattice on this box instead of on the cube.
 *
 * The box is the cheapest set that CONTAINS the sample hull, and containment
 * is the property that matters: the support points themselves lie ON the hull,
 * so the convex hull of 26 of them is inscribed and cuts the corners between
 * sampled directions.  Measured on a tilted cloud, anchoring to the support
 * points left 70% of probe colors unreachable against 82% for no anchoring at
 * all, while anchoring to the box left 4%.
 */
typedef struct sixel_palette_cover_extent {
    unsigned char lo[3];
    unsigned char hi[3];
} sixel_palette_cover_extent_t;

/*
 * Measure the extent of a sample buffer.  Returns non-zero when the extent is
 * usable; a format this cannot read, or an empty buffer, returns zero and the
 * caller falls back to the cube.
 */
SIXEL_INTERNAL_API int
sixel_palette_cover_measure_extent(
    void const                     /* in */  *data,
    unsigned int                   /* in */   length,
    int                            /* in */   pixelformat,
    sixel_palette_cover_extent_t   /* out */ *extent);

/* Anchoring options, resolved from the override, then env, then defaults. */
typedef struct sixel_palette_cover_options {
    int policy;  /* SIXEL_PALETTE_COVER_*      */
    int grow;    /* anchors may exceed -p N    */
    int mode;    /* SIXEL_PALETTE_COVER_MODE_* */
} sixel_palette_cover_options_t;

/*
 * An anchor closer than this to an existing entry is already reachable, so
 * placing it would spend a slot without enclosing anything new.
 */
#define SIXEL_PALETTE_COVER_NEAR_SQ (24 * 24 * 3)

/*
 * An anchor funded by merging only pays off while the closest pair is nearer
 * to each other than the anchor is to the palette: the merge costs about the
 * distance between the pair, the anchor buys about the distance it closes.
 * Comparing the two makes the budget scale with whatever spread the solver
 * chose, which a fixed threshold cannot -- k-center deliberately spreads its
 * entries, so any constant tuned for a median-cut palette rejects every merge.
 */
#define SIXEL_PALETTE_COVER_MERGE_MARGIN 2u

/*
 * How many times placement may sweep the anchor list.  Funding one anchor can
 * un-reach another, so a single sweep is not a fixed point; a sweep that
 * places nothing ends the loop, making this only a backstop.
 */
#define SIXEL_PALETTE_COVER_MAX_ROUNDS 4u

/*
 * Resolve SIXEL_PALETTE_COVER_AUTO for a palette of ENTRY_COUNT colors.
 *
 * The anchors cost a fixed number of slots, so their relative price falls as
 * the palette grows.  Anchoring is never free in mean squared error -- it
 * spends slots to buy reachability, which MSE over a whole photograph barely
 * registers because the colors it rescues occupy little area.  Measured
 * through the encoder and decoder on photographs, MSE against the source with
 * the face set versus no anchoring at all:
 *
 *              -p 64          -p 128        -p 256
 *   autumn     +23%           +6%           +3%
 *   egret      +27%           +7%           +5%
 *
 * So the ladder is a price schedule, not a quality curve: climb it as the
 * palette grows and each anchor costs a smaller share of the whole.  Below 32
 * colors the price is indefensible at any coverage.
 *
 * The cost is also content-dependent in a way this fixed lattice cannot see.
 * An image whose colors never approach the gamut boundary pays for anchors it
 * can never use, and worse: measured on content compressed into r[86,145],
 * cover=faces took MSE from 57.8 to 110.1 and put 749 pixels on screen in a
 * color the source never contained.  That is what cover_mode=soft exists to
 * fix -- anchoring to the support of the image's own colors rather than the
 * cube's, which degrades to a no-op exactly when the content is interior.
 */
SIXEL_INTERNAL_API int
sixel_palette_cover_resolve_policy(int policy, unsigned int entry_count);

/*
 * Collect the anchors of POLICY that ENTRY_COUNT colors do not already reach,
 * writing them to OUT as RGB triples.  Returns how many were written.
 *
 * EXTENT selects where the lattice sits: NULL puts it on the RGB cube (hard),
 * and a measured extent puts it on that box (soft).
 */
SIXEL_INTERNAL_API unsigned int
sixel_palette_cover_missing_anchors(
    unsigned char const                /* in */  *entries,
    unsigned int                       /* in */   entry_count,
    int                                /* in */   depth,
    int                                /* in */   policy,
    sixel_palette_cover_extent_t const /* in */  *extent,
    unsigned char                      /* out */ *out,
    unsigned int                       /* in */   out_max);

/*
 * Anchor a finished palette in place, funding each anchor by merging the
 * closest pair of existing entries.  ENTRY_COUNT does not change, so a caller
 * that asked for N colors still gets N.  RGB888 only.
 */
SIXELAPI SIXELSTATUS
sixel_palette_cover_anchor_rgb888(
    unsigned char                      /* in out */ *entries,
    unsigned int                       /* in */      entry_count,
    int                                /* in */      depth,
    sixel_palette_cover_extent_t const /* in */     *extent);

/*
 * Override anchoring from the encoder so it can be driven as a quantize model
 * suboption.  Clearing the override (ENABLED zero) falls back to the
 * environment and then to the defaults.  OPTIONS may be NULL when clearing.
 */
SIXEL_INTERNAL_API void
sixel_set_palette_cover_override(
    int                                  /* in */  enabled,
    sixel_palette_cover_options_t const  /* in */ *options);

/* Resolved policy: override first, then SIXEL_PALETTE_COVER, then auto. */
SIXEL_INTERNAL_API int
sixel_palette_cover_policy(void);

/* Non-zero when anchors may push the palette past the requested count. */
SIXEL_INTERNAL_API int
sixel_palette_cover_grow_enabled(void);

/*
 * Resolved mode: override first, then SIXEL_PALETTE_COVER_MODE, then soft.
 *
 * Soft is the default because hard cannot be told apart from a defect on the
 * content it is wrong for.  A frame whose colors stay away from the gamut
 * boundary gains nothing from a cube anchor and pays twice for it: the slot,
 * and the fact that the anchor becomes the ONLY place a small residual can
 * discharge.  Measured on a slightly greenish black desktop at -p 32, where
 * the sole green in the palette was the anchor at (0,255,0), error diffusion
 * -- working exactly as specified -- had to emit 6910 pure-green pixels to
 * represent a few units of green excess, and MSE went from 299 to 2055.  The
 * same lattice placed on the sample box emitted none and cost 376.
 *
 * Hard remains right when coverage is needed for colors that are not in this
 * frame at all, which soft cannot see by construction.
 */
SIXEL_INTERNAL_API int
sixel_palette_cover_mode(void);

/* Convenience: policy resolves to something other than off. */
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
