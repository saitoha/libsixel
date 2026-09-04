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

#ifndef LIBSIXEL_FILTER_SAMPLE_H
#define LIBSIXEL_FILTER_SAMPLE_H

#include <sixel.h>

#include "filter.h"
#include "timeline-logger.h"

/*
 * Sample filter configuration. The planner fills this before wiring the
 * filter so apply() can copy the requested region with the right density.
 */
typedef struct sixel_filter_sample_config {
    /*
     * Visible region used for palette sampling. When width/height are not
     * positive, the full frame bounds are used instead.
     */
    int clip_x;
    int clip_y;
    int clip_width;
    int clip_height;

    /*
     * Palette sizing knobs carried from encoder options. The override flag
     * mirrors $SIXEL_PALETTE_SAMPLE_TARGET support.
     */
    int reqcolors;
    int quality_mode;
    int palette_sample_override;
    size_t palette_sample_target;
} sixel_filter_sample_config_t;

SIXELSTATUS
sixel_filter_sample_init(sixel_filter_t *filter,
                         const sixel_filter_sample_config_t *config);

/*
 * Solid-region detection.
 *
 * The sample is a grid pick, so a thin element is present or absent depending
 * on where the grid falls: measured on a 1472x760 frame at -p 64, where the
 * stride is 17, a 700x3 bar landed in the sample in only 3 of 17 row phases.
 * In motion the phase changes every frame, and that is the flicker -- the
 * palette gains and loses the color from one frame to the next.  No rule
 * applied to the sample can repair this, because in the other 14 phases the
 * color is simply not there.
 *
 * So solid regions are found in the source and their colors appended to the
 * sample, which makes the sample's content independent of the grid phase.
 *
 * A point counts as solid when the pixels along a short line through it, on
 * either axis, are all within SIXEL_SAMPLE_SOLID_TOL of it.  The test has to
 * be a LINE and not a square: a square kernel of radius r rejects every
 * feature thinner than 2r+1, so a 5x5 kernel discards a 3-pixel bar for
 * exactly the reason it discards a hot pixel.  Measured, a 5x5 kernel found
 * the bar in 0 of 17 phases even scanning every pixel, while the two-axis line
 * test found it in all 17 -- and still rejected a noisy patch and a hot pixel
 * completely.
 *
 * The scan stride must not exceed the thinnest feature worth protecting: a
 * 3-pixel bar is guaranteed to contain a scanned row at stride 3, and at
 * stride 4 detection fell to 13 of 17.  Cost at 1472x760 is 0.27 ms, and it
 * scales with the damaged area rather than the screen.
 */
#define SIXEL_SAMPLE_SOLID_STRIDE 3
#define SIXEL_SAMPLE_SOLID_HALF 2
#define SIXEL_SAMPLE_SOLID_TOL 6
#define SIXEL_SAMPLE_SOLID_MAX 32u

/*
 * Two solid colors nearer than this are the same UI element as far as the
 * palette is concerned, so they share a slot in the collected list.
 */
#define SIXEL_SAMPLE_SOLID_SEPARATION_SQ (12 * 12 * 3)

/*
 * Collect up to OUT_MAX solid colors of PIXELS, writing DEPTH bytes each.
 * MASK, when not NULL, excludes transparent pixels.  Returns how many were
 * written.
 *
 * Selection is farthest-point, not most-seen.  Ranking by how many pixels a
 * color covers puts the background at the top and fills every slot with it:
 * measured on autumn.png, egret.jpg and snake-fs8.png, all 32 slots went to
 * background colors and a 3-pixel bar was crowded out in 17 of 17 phases --
 * the flicker, back again.  The grid pick already represents whatever covers
 * a lot of the frame; what it misses is small, and small is what this has to
 * find.  Maximising the distance to the colors already chosen keeps a
 * saturated bar in a brown photograph, because it is far from everything.
 */
SIXEL_INTERNAL_API unsigned int
sixel_filter_sample_solid_colors(
    unsigned char const /* in */  *pixels,
    int                 /* in */   width,
    int                 /* in */   height,
    int                 /* in */   depth,
    unsigned char const /* in */  *mask,
    int                 /* in */   clip_x,
    int                 /* in */   clip_y,
    int                 /* in */   clip_width,
    int                 /* in */   clip_height,
    unsigned char       /* out */ *out,
    unsigned int        /* in */   out_max,
    sixel_allocator_t   /* in */  *allocator);

#endif /* LIBSIXEL_FILTER_SAMPLE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
