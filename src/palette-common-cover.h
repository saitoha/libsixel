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
 * the nearest-color lookup actually selects.  In the controlled frozen-palette
 * probes used to choose this ladder, the residual error of a flat patch was
 * bimodal: it sat near zero or near the full distance to the nearest entry,
 * with almost nothing in between.  The second case was diffusion never
 * happening at all -- the pixel resolved to one entry and stayed there.
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
 * These are historical design measurements.  Their original generator and
 * exact probe corpus were not retained.  The documented measurement workflow
 * tests the bimodality assumption on broader source-derived probes rather than
 * treating this table as a distribution-independent result.
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
/*
 * Under hard these name points of the RGB cube.  Under soft the geometry is
 * gone -- anchors come from the image -- so the same names are a BUDGET: how
 * many anchors may be bought, 8, 14 or 26.  The spelling, the ordering and the
 * auto ladder are shared, so a configuration means the same amount of effort
 * in either mode.
 */
#define SIXEL_PALETTE_COVER_OFF     0  /* no anchoring */
#define SIXEL_PALETTE_COVER_CORNERS 1  /* 8 cube corners  / 8 anchors  */
#define SIXEL_PALETTE_COVER_FACES   2  /* + 6 face centres / 14 anchors */
#define SIXEL_PALETTE_COVER_EDGES   3  /* + 12 edge mids   / 26 anchors */
#define SIXEL_PALETTE_COVER_AUTO    4  /* choose by palette size */

#define SIXEL_PALETTE_COVER_ANCHOR_MAX 26u

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
 * How the anchors are chosen.
 *
 * HARD places the fixed lattice above and nothing else.  It cannot miss a
 * region of the cube, and it cannot know which regions the image actually
 * uses, so it is right exactly when coverage is needed for colors that are
 * not in this frame at all.
 *
 * SOFT takes the opposite trade: the anchors are colors of the image, so a
 * color the image leans on is protected wherever it sits, and a region of the
 * cube the image never approaches costs nothing.  It cannot protect what the
 * frame does not contain.
 */
#define SIXEL_PALETTE_COVER_MODE_HARD 0
#define SIXEL_PALETTE_COVER_MODE_SOFT 1

/* Anchoring options, resolved from the override, then env, then defaults. */
typedef struct sixel_palette_cover_options {
    int policy_override;
    int policy;  /* SIXEL_PALETTE_COVER_*      */
    int grow_override;
    int grow;    /* anchors may exceed -p N    */
    int mode_override;
    int mode;    /* SIXEL_PALETTE_COVER_MODE_* */
} sixel_palette_cover_options_t;

/*
 * Resolve SIXEL_PALETTE_COVER_AUTO for a palette of ENTRY_COUNT colors, and
 * report how many anchors a resolved policy is worth.
 */
SIXEL_INTERNAL_API int
sixel_palette_cover_resolve_policy(int policy, unsigned int entry_count);

SIXEL_INTERNAL_API unsigned int
sixel_palette_cover_budget(int policy, unsigned int entry_count);

/*
 * Soft anchoring picks its anchors out of the image instead of constructing
 * them.  Two earlier shapes were measured and rejected:
 *
 *   support points -- the sample maximising <d,x> for each lattice direction.
 *   These lie ON the hull, so the convex hull of 26 of them is inscribed and
 *   cuts the corners between sampled directions: 70% of probe colors stayed
 *   unreachable against 82% for no anchoring at all.
 *
 *   the bounding box -- which does contain the hull, and does fix that.  But a
 *   real color cloud is elongated along luminance, so the box is mostly empty:
 *   on a bright outdoor frame containing a dark panel, six of the eight box
 *   corners had no pixel within 156 units.  An anchor on an unoccupied color
 *   is the same trap as a cube corner -- it becomes the only place a residual
 *   can discharge, and error diffusion has to emit it as isolated
 *   full-intensity pixels.  One hot pixel inflated the box and made seven of
 *   eight corners empty, because a componentwise min/max is the least robust
 *   statistic there is.
 *
 * So an anchor must be a color the image actually contains, in quantity.  The
 * candidates are the populous cells of a coarse histogram, ranked by how far
 * they sit from the finished palette, which is exactly the population that
 * error diffusion cannot reach.  Constructed colors never enter the palette,
 * so the artifact class cannot recur.
 */

/* Histogram cell width for candidate mass, in bits per channel. */
#define SIXEL_PALETTE_COVER_CANDIDATE_BITS 4

/*
 * A cell needs this share of the samples to be a candidate.  The samples are a
 * few thousand pixels, so this is a handful of them: enough that a hot pixel,
 * a cursor, or compression ringing cannot nominate a color, and few enough
 * that a scrub bar covering a fraction of a percent still can.
 */
#define SIXEL_PALETTE_COVER_CANDIDATE_SHARE 1024u
#define SIXEL_PALETTE_COVER_CANDIDATE_MIN 2u

/*
 * Collect up to OUT_MAX soft anchors: populous colors of DATA that the current
 * ENTRIES do not already reach, farthest first, each separated from the ones
 * before it.  Returns how many were written.  ALLOCATOR provides the
 * histogram; a format the reader cannot handle returns zero.
 */
/*
 * Whether the candidate reader understands PIXELFORMAT.  Soft has to be able
 * to tell "nothing needs anchoring" from "I cannot read this", because the
 * first is a no-op and the second must fall back to hard.  Collapsing them
 * makes soft silently place the cube lattice on every image it fully covers,
 * which is exactly the content the cube is worst for.
 */
SIXEL_INTERNAL_API int
sixel_palette_cover_candidates_supported(int pixelformat);

SIXEL_INTERNAL_API unsigned int
sixel_palette_cover_collect_candidates(
    void const          /* in */  *data,
    unsigned int        /* in */   length,
    int                 /* in */   pixelformat,
    unsigned char const /* in */  *entries,
    unsigned int        /* in */   entry_count,
    int                 /* in */   depth,
    unsigned char       /* out */ *out,
    unsigned int        /* in */   out_max,
    sixel_allocator_t   /* in */  *allocator);

/*
 * Place ANCHORS into ENTRIES, funding each by merging the closest pair.
 * Iterated to a fixed point: funding one anchor moves a pair of entries to
 * their midpoint, which can pull an entry away from an anchor already judged
 * reachable.
 */
SIXEL_INTERNAL_API void
sixel_palette_cover_place(
    unsigned char       /* in out */ *entries,
    unsigned int        /* in */      entry_count,
    int                 /* in */      depth,
    unsigned char const /* in */     *anchors,
    unsigned int        /* in */      anchor_count);

/*
 * Collect the anchors of POLICY that ENTRY_COUNT colors do not already reach,
 * writing them to OUT as RGB triples.  Returns how many were written.
 *
 */
SIXEL_INTERNAL_API unsigned int
sixel_palette_cover_missing_anchors(
    unsigned char const /* in */  *entries,
    unsigned int        /* in */   entry_count,
    int                 /* in */   depth,
    int                 /* in */   policy,
    unsigned char       /* out */ *out,
    unsigned int        /* in */   out_max);

/*
 * Anchor a finished palette in place, funding each anchor by merging the
 * closest pair of existing entries.  ENTRY_COUNT does not change, so a caller
 * that asked for N colors still gets N.  RGB888 only.
 */
SIXELAPI SIXELSTATUS
sixel_palette_cover_anchor_rgb888(
    unsigned char /* in out */ *entries,
    unsigned int  /* in */      entry_count,
    int           /* in */      depth);

/*
 * Override anchoring from the encoder. Each field is independent; a field
 * without its override flag falls back to the environment and then defaults.
 * OPTIONS may be NULL when clearing.
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
