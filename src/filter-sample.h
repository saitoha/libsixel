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
#if defined(HAVE_VULKAN)
#include "gpu-runtime-vulkan.h"
#endif

/* A deferred grid describes the input frame without gathering its pixels.
 * Binning can fuse the gather while using the same grid as the CPU filter. */
typedef struct sixel_filter_sample_grid {
    unsigned int width, height, stride, columns, rows;
    size_t count;
} sixel_filter_sample_grid_t;

/*
 * Sample filter configuration. The planner fills this before wiring the
 * filter so apply() can copy the requested region with the right density.
 */
typedef struct sixel_filter_sample_config {
    /*
     * An effective policy and its source are required when the output edge
     * is a sample stream.  AUTO remains the compatibility value for callers
     * that request the legacy frame output.
     */
    sixel_palette_sampling_policy_t policy;
    sixel_palette_sampling_source_t source;

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
    /* Non-zero skips solid-region augmentation (sampling solid=off): the
     * adaptive sample is the grid alone. Zero keeps the default scan. */
    int skip_solid;
    /* SIXEL_GPU_POLICY_*: any policy but OFF may scan solid regions on
     * the GPU; the CPU scans when no device takes it. */
    int gpu_policy;
    /* Stage a loaded RGB frame for reuse if the pre-plan leaves it intact. */
    int share_gpu_input;
    /* Set for a deferred grid edge. The filter borrows this configuration
     * and publishes geometry without allocating or reading sample pixels. */
    sixel_filter_sample_grid_t *grid_output;
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

/* Solid colors are counted in cells of their top four bits per channel. */
#define SIXEL_SAMPLE_SOLID_BITS 4
#define SIXEL_SAMPLE_SOLID_CELLS (1u << (SIXEL_SAMPLE_SOLID_BITS * 3u))

/*
 * Scan the RGB888 clip of PIXELS for solid points on the device, leaving
 * in CELLS (SIXEL_SAMPLE_SOLID_CELLS records of a count and four channel
 * sums) exactly the histogram the CPU scan builds. Fails, leaving CELLS
 * unspecified, when no device is available or allocation or dispatch fails.
 */
SIXEL_INTERNAL_API void
sixel_filter_sample_plan_grid(sixel_filter_sample_config_t const *config,
    int width, int height, sixel_filter_sample_grid_t *grid);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_filter_sample_solid_gpu(
    unsigned char const /* in */  *pixels,
    int                 /* in */   width,
    int                 /* in */   height,
    int                 /* in */   clip_x,
    int                 /* in */   clip_y,
    int                 /* in */   clip_width,
    int                 /* in */   clip_height,
    sixel_frame_t       /* in */  *share_owner,
    unsigned int        /* out */ *cells);

#if defined(HAVE_VULKAN)
SIXEL_INTERNAL_API int
sixel_filter_sample_vulkan_choose(int gpu_policy);
/* These leases are protected by the Vulkan runtime lock. A held source
 * frame keeps the key's pixel address unique until the dither consumes it. */
SIXEL_INTERNAL_API int
sixel_filter_sample_vulkan_borrow_locked(
    unsigned char const *pixels, size_t length,
    sixel_vk_buffer_t const **buffer);
SIXEL_INTERNAL_API sixel_frame_t *
sixel_filter_sample_vulkan_release_locked(int slot);
SIXEL_INTERNAL_API void
sixel_filter_sample_vulkan_finish_preplan(sixel_frame_t *owner,
    int unchanged);
#endif

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
 * a 3-pixel bar can be crowded out by background colors in every slot,
 * bringing back the flicker.  The grid pick already represents whatever covers
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
