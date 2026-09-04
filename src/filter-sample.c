/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "filter-sample.h"
#include "filter.h"
#include "frame-factory.h"
#include "frame.h"
#include "loader-common.h"
#include "pixelformat.h"

typedef struct sixel_filter_sample_state {
    sixel_filter_sample_config_t config;
} sixel_filter_sample_state_t;

static SIXELSTATUS
sixel_filter_sample_apply(sixel_filter_t *filter,
      sixel_allocator_t *allocator,
      sixel_timeline_logger_t *logger);

static void
sixel_filter_sample_dispose(sixel_filter_t *filter);

static sixel_filter_vtbl_t const sixel_filter_sample_vtbl = {
    "sample",
    SIXEL_FILTER_KIND_SAMPLE,
    sixel_filter_sample_apply,
    sixel_filter_sample_dispose,
    NULL,
    NULL,
    NULL
};

static size_t
sixel_filter_sample_select_stride(
        sixel_filter_sample_config_t const *config,
        int width,
        int height)
{
    size_t stride;
    size_t base_target;
    size_t color_budget;
    size_t target;
    size_t total;

    stride = 1u;
    base_target = 4096u;
    color_budget = 0u;
    target = base_target;
    total = 0u;

    if (width <= 0 || height <= 0) {
        return stride;
    }

    if (config != NULL && config->palette_sample_override != 0) {
        target = config->palette_sample_target;
    } else {
        if (config != NULL && config->reqcolors > 0) {
            color_budget = (size_t)config->reqcolors * 64u;
            if (color_budget / 64u != (size_t)config->reqcolors) {
                color_budget = base_target;
            }
            if (color_budget > target) {
                target = color_budget;
            }
        }

        if (config != NULL
                && (config->quality_mode == SIXEL_QUALITY_HIGH
                    || config->quality_mode == SIXEL_QUALITY_HIGHCOLOR)) {
            if (target <= SIZE_MAX / 2u) {
                target *= 2u;
            } else {
                target = SIZE_MAX;
            }
        } else if (config != NULL
                && config->quality_mode == SIXEL_QUALITY_FULL) {
            if (target <= SIZE_MAX / 4u) {
                target *= 4u;
            } else {
                target = SIZE_MAX;
            }
        }
    }

    total = (size_t)width * (size_t)height;
    while (stride < total && total / (stride * stride) > target) {
        ++stride;
    }
    sixel_trace_topic_message(
        "palette_contract",
        "LSXSMP1|override=%d|target=%zu|stride=%zu",
        config != NULL ? config->palette_sample_override : 0,
        target,
        stride);

    return stride;
}

static SIXELSTATUS
sixel_filter_sample_create_frame(sixel_allocator_t *allocator,
                                 sixel_frame_interface_t **frame_out)
{
    SIXELSTATUS status;

    status = SIXEL_FALSE;

    if (allocator == NULL || frame_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_frame_create_interface_from_factory(allocator, frame_out);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (*frame_out == NULL || (*frame_out)->vtbl == NULL ||
        (*frame_out)->vtbl->init_pixels == NULL ||
        (*frame_out)->vtbl->set_timeline == NULL ||
        (*frame_out)->vtbl->set_transparency == NULL ||
        (*frame_out)->vtbl->unref == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return SIXEL_OK;
}


/* max |channel difference| from the centre along one axis */
static int
sixel_filter_sample_line_spread(unsigned char const *pixels,
                                int width,
                                int height,
                                int depth,
                                int cx,
                                int cy,
                                int dx,
                                int dy)
{
    unsigned char const *centre;
    int worst;
    int i;
    int c;

    if (cx - SIXEL_SAMPLE_SOLID_HALF * dx < 0
            || cx + SIXEL_SAMPLE_SOLID_HALF * dx >= width
            || cy - SIXEL_SAMPLE_SOLID_HALF * dy < 0
            || cy + SIXEL_SAMPLE_SOLID_HALF * dy >= height) {
        return 255;
    }
    centre = pixels + ((size_t)cy * (size_t)width + (size_t)cx)
                      * (size_t)depth;
    worst = 0;
    for (i = -SIXEL_SAMPLE_SOLID_HALF; i <= SIXEL_SAMPLE_SOLID_HALF; ++i) {
        unsigned char const *px;

        px = pixels + ((size_t)(cy + dy * i) * (size_t)width
                       + (size_t)(cx + dx * i)) * (size_t)depth;
        for (c = 0; c < depth; ++c) {
            int d;

            d = (int)px[c] - (int)centre[c];
            if (d < 0) {
                d = -d;
            }
            if (d > worst) {
                worst = d;
            }
        }
    }

    return worst;
}

static unsigned int
sixel_filter_sample_color_distance_sq(unsigned char const *a,
                                      unsigned char const *b,
                                      int depth)
{
    unsigned int total;
    int c;

    total = 0u;
    for (c = 0; c < depth && c < 3; ++c) {
        int d;

        d = (int)a[c] - (int)b[c];
        total += (unsigned int)(d * d);
    }

    return total;
}

/* One histogram cell of solid colors: how many, and their sum for the mean. */
typedef struct sixel_filter_sample_solid_cell {
    unsigned int count;
    unsigned int sum[4];
} sixel_filter_sample_solid_cell_t;

#define SIXEL_SAMPLE_SOLID_BITS 4
#define SIXEL_SAMPLE_SOLID_CELLS (1u << (SIXEL_SAMPLE_SOLID_BITS * 3u))

SIXEL_INTERNAL_API unsigned int
sixel_filter_sample_solid_colors(unsigned char const *pixels,
                                 int width,
                                 int height,
                                 int depth,
                                 unsigned char const *mask,
                                 int clip_x,
                                 int clip_y,
                                 int clip_width,
                                 int clip_height,
                                 unsigned char *out,
                                 unsigned int out_max,
                                 sixel_allocator_t *allocator)
{
    sixel_filter_sample_solid_cell_t *cells;
    unsigned int const shift = 8u - SIXEL_SAMPLE_SOLID_BITS;
    unsigned int found;
    unsigned int seen;
    int x;
    int y;
    int c;

    found = 0u;
    seen = 0u;
    if (pixels == NULL || out == NULL || allocator == NULL || out_max == 0u
            || depth < 3 || depth > 4
            || clip_width <= 0 || clip_height <= 0) {
        return 0u;
    }
    if (out_max > SIXEL_SAMPLE_SOLID_MAX) {
        out_max = SIXEL_SAMPLE_SOLID_MAX;
    }
    cells = (sixel_filter_sample_solid_cell_t *)sixel_allocator_calloc(
        allocator,
        (size_t)SIXEL_SAMPLE_SOLID_CELLS,
        sizeof(sixel_filter_sample_solid_cell_t));
    if (cells == NULL) {
        return 0u;
    }
    for (y = clip_y; y < clip_y + clip_height; y += SIXEL_SAMPLE_SOLID_STRIDE) {
        for (x = clip_x;
             x < clip_x + clip_width;
             x += SIXEL_SAMPLE_SOLID_STRIDE) {
            unsigned char const *px;
            unsigned int key;

            if (mask != NULL
                    && mask[(size_t)y * (size_t)width + (size_t)x] != 0) {
                continue;
            }
            /*
             * Either axis qualifies.  A horizontal run is a scrub bar or an
             * underline, a vertical run is a scrollbar or a window edge, and
             * requiring both would reject every one of them.
             */
            if (sixel_filter_sample_line_spread(pixels, width, height, depth,
                                                x, y, 1, 0)
                        > SIXEL_SAMPLE_SOLID_TOL
                    && sixel_filter_sample_line_spread(pixels, width, height,
                                                       depth, x, y, 0, 1)
                        > SIXEL_SAMPLE_SOLID_TOL) {
                continue;
            }
            px = pixels + ((size_t)y * (size_t)width + (size_t)x)
                          * (size_t)depth;
            key = ((unsigned int)(px[0] >> shift)
                   << (SIXEL_SAMPLE_SOLID_BITS * 2u))
                | ((unsigned int)(px[1] >> shift) << SIXEL_SAMPLE_SOLID_BITS)
                | (unsigned int)(px[2] >> shift);
            cells[key].count++;
            for (c = 0; c < depth; ++c) {
                cells[key].sum[c] += px[c];
            }
            seen++;
        }
    }
    if (seen == 0u) {
        sixel_allocator_free(allocator, cells);
        return 0u;
    }

    /*
     * Farthest-point selection.  The first pick is the most-seen cell, which
     * is the frame's dominant solid color and belongs in any summary; every
     * pick after it is the qualifying cell furthest from everything already
     * chosen.  That is what keeps a small saturated element: it is far from
     * the background even though the background outnumbers it hugely.
     *
     * Each cell carries its distance to the nearest color chosen so far, so a
     * round is one pass rather than one pass times the number chosen.  Doing
     * it the naive way measured 1.1 ms of the frame at 1472x760, which is
     * most of what this stage costs.
     */
    {
        unsigned int *nearest;
        unsigned int index;

        nearest = (unsigned int *)sixel_allocator_calloc(
            allocator, (size_t)SIXEL_SAMPLE_SOLID_CELLS, sizeof(unsigned int));
        if (nearest == NULL) {
            sixel_allocator_free(allocator, cells);
            return 0u;
        }
        for (index = 0u; index < SIXEL_SAMPLE_SOLID_CELLS; ++index) {
            nearest[index] = ~0u;
        }
        while (found < out_max) {
            unsigned char rgb[4];
            unsigned int best_cell;
            unsigned int best_score;
            int have_best;

            best_cell = 0u;
            best_score = 0u;
            have_best = 0;
            for (index = 0u; index < SIXEL_SAMPLE_SOLID_CELLS; ++index) {
                unsigned int score;

                if (cells[index].count == 0u) {
                    continue;
                }
                if (found == 0u) {
                    score = cells[index].count;
                } else {
                    if (nearest[index]
                            <= (unsigned int)SIXEL_SAMPLE_SOLID_SEPARATION_SQ) {
                        continue;
                    }
                    score = nearest[index];
                }
                if (score > best_score) {
                    best_score = score;
                    best_cell = index;
                    have_best = 1;
                }
            }
            if (!have_best) {
                break;
            }
            for (c = 0; c < depth; ++c) {
                rgb[c] = (unsigned char)(cells[best_cell].sum[c]
                                         / cells[best_cell].count);
                out[(size_t)found * (size_t)depth + (size_t)c] = rgb[c];
            }
            found++;
            for (index = 0u; index < SIXEL_SAMPLE_SOLID_CELLS; ++index) {
                unsigned char other[4];
                unsigned int d;

                if (cells[index].count == 0u) {
                    continue;
                }
                for (c = 0; c < depth; ++c) {
                    other[c] = (unsigned char)(cells[index].sum[c]
                                               / cells[index].count);
                }
                d = sixel_filter_sample_color_distance_sq(other, rgb, depth);
                if (d < nearest[index]) {
                    nearest[index] = d;
                }
            }
        }
        sixel_allocator_free(allocator, nearest);
    }
    sixel_allocator_free(allocator, cells);

    return found;
}

static SIXELSTATUS
sixel_filter_sample_copy_frame(
        sixel_filter_sample_config_t const *config,
        sixel_frame_t *frame,
        sixel_allocator_t *allocator,
        sixel_frame_t **sample_out,
        sixel_timeline_logger_t *logger,
        int *sample_width_out,
        int *sample_height_out)
{
    SIXELSTATUS status;
    sixel_frame_t *sample;
    sixel_frame_interface_t *frame_if;
    sixel_frame_interface_t *sample_if;
    sixel_frame_vtbl_t const *sample_vtbl;
    sixel_frame_pixels_request_t pixels_request;
    sixel_frame_timeline_t timeline;
    sixel_frame_transparency_t src_transparency;
    sixel_frame_transparency_t dst_transparency;
    unsigned char *src_pixels;
    unsigned char *dst_pixels;
    unsigned char const *src_mask;
    unsigned char *dst_mask;
    size_t stride;
    size_t src_pixel_count;
    int clip_x;
    int clip_y;
    int clip_w;
    int clip_h;
    int src_width;
    int src_height;
    int src_pixelformat;
    int normalized_src_pixelformat;
    int width;
    int height;
    int depth;
    int sample_width;
    int sample_height;
    size_t sample_count;
    size_t payload_size;
    size_t mask_size;
    size_t dst_index;
    size_t src_offset;
    int x;
    int y;
    unsigned char *normalized_src_pixels;
    unsigned char solid[SIXEL_SAMPLE_SOLID_MAX * 4u];
    unsigned int solid_count;
    unsigned int solid_weight;
    size_t solid_slots;
    int solid_rows;

    status = SIXEL_FALSE;
    solid_count = 0u;
    solid_weight = 0u;
    solid_slots = 0u;
    solid_rows = 0;
    sample = NULL;
    frame_if = NULL;
    sample_if = NULL;
    sample_vtbl = NULL;
    memset(&pixels_request, 0, sizeof(pixels_request));
    memset(&timeline, 0, sizeof(timeline));
    memset(&src_transparency, 0, sizeof(src_transparency));
    memset(&dst_transparency, 0, sizeof(dst_transparency));
    src_pixels = NULL;
    dst_pixels = NULL;
    src_mask = NULL;
    dst_mask = NULL;
    stride = 1u;
    src_pixel_count = 0u;
    clip_x = 0;
    clip_y = 0;
    clip_w = 0;
    clip_h = 0;
    src_width = 0;
    src_height = 0;
    src_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    normalized_src_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    width = 0;
    height = 0;
    depth = 0;
    sample_width = 0;
    sample_height = 0;
    sample_count = 0u;
    payload_size = 0u;
    mask_size = 0u;
    dst_index = 0u;
    src_offset = 0u;
    x = 0;
    y = 0;
    normalized_src_pixels = NULL;

    if (frame == NULL || sample_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    frame_if = sixel_frame_as_interface(frame);
    src_pixelformat = sixel_frame_get_pixelformat(frame);
    if ((src_pixelformat & SIXEL_FORMATTYPE_PALETTE)) {
        return SIXEL_FEATURE_ERROR;
    }

    src_pixels = sixel_frame_get_pixels(frame);
    src_width = sixel_frame_get_width(frame);
    src_height = sixel_frame_get_height(frame);
    status = frame_if->vtbl->get_transparency(frame_if, &src_transparency);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = frame_if->vtbl->get_timeline(frame_if, &timeline);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    src_mask = src_transparency.transparent_mask;
    src_pixel_count = (size_t)src_width * (size_t)src_height;

    if (src_pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (src_width > 0 && src_height > 0 &&
        src_pixel_count / (size_t)src_height != (size_t)src_width) {
        return SIXEL_RUNTIME_ERROR;
    }

    /*
     * Packed grayscale frames use sub-byte storage, but the sampling loop
     * below advances with byte-based offsets. Normalize packed inputs first
     * so clip/stride calculations cannot read beyond source row boundaries.
     */
    if (src_pixelformat == SIXEL_PIXELFORMAT_G1 ||
            src_pixelformat == SIXEL_PIXELFORMAT_G2 ||
            src_pixelformat == SIXEL_PIXELFORMAT_G4) {
        normalized_src_pixels = (unsigned char *)sixel_allocator_malloc(
            allocator, src_pixel_count);
        if (normalized_src_pixels == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        normalized_src_pixelformat = src_pixelformat;
        status = sixel_helper_normalize_pixelformat(
            normalized_src_pixels,
            &normalized_src_pixelformat,
            src_pixels,
            src_pixelformat,
            src_width,
            src_height);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(allocator, normalized_src_pixels);
            return status;
        }
        src_pixels = normalized_src_pixels;
        src_pixelformat = normalized_src_pixelformat;
    }

    depth = sixel_helper_compute_depth(src_pixelformat);
    if (depth <= 0) {
        sixel_allocator_free(allocator, normalized_src_pixels);
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * The logger is currently unused, but it will be wired once planner
     * driven progress messages are emitted from the filter layer.
     */
    (void)logger;

    if (config != NULL) {
        clip_x = config->clip_x;
        clip_y = config->clip_y;
        clip_w = config->clip_width;
        clip_h = config->clip_height;
    }

    if (clip_w <= 0 || clip_h <= 0) {
        clip_x = 0;
        clip_y = 0;
        clip_w = src_width;
        clip_h = src_height;
    } else {
        if (clip_w + clip_x > src_width) {
            if (clip_x > src_width) {
                clip_w = 0;
            } else {
                clip_w = src_width - clip_x;
            }
        }

        if (clip_h + clip_y > src_height) {
            if (clip_y > src_height) {
                clip_h = 0;
            } else {
                clip_h = src_height - clip_y;
            }
        }

        if (clip_w <= 0 || clip_h <= 0) {
            sixel_allocator_free(allocator, normalized_src_pixels);
            return SIXEL_BAD_ARGUMENT;
        }
    }

    width = clip_w;
    height = clip_h;

    stride = sixel_filter_sample_select_stride(config, width, height);
    sample_width = (width + (int)stride - 1) / (int)stride;
    sample_height = (height + (int)stride - 1) / (int)stride;
    if (sample_width <= 0 || sample_height <= 0) {
        sixel_allocator_free(allocator, normalized_src_pixels);
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * Find the solid regions before sizing the sample, because their colors
     * are appended to it and the extra rows have to be accounted for here.
     */
    solid_count = sixel_filter_sample_solid_colors(src_pixels,
                                                   src_width,
                                                   src_height,
                                                   depth,
                                                   src_mask,
                                                   clip_x,
                                                   clip_y,
                                                   width,
                                                   height,
                                                   solid,
                                                   SIXEL_SAMPLE_SOLID_MAX,
                                                   allocator);
    if (solid_count > 0u) {
        size_t grid_count;

        grid_count = (size_t)sample_width * (size_t)sample_height;
        /*
         * Enough copies of each color that it clears the mass floor the
         * palette's anchoring applies, with margin.  Fewer would let a solid
         * element be dismissed as noise -- which is the discrimination this
         * whole path exists to get right.
         */
        solid_weight = (unsigned int)(grid_count / 512u);
        if (solid_weight < 4u) {
            solid_weight = 4u;
        }
        solid_slots = (size_t)solid_count * (size_t)solid_weight;
        solid_rows = (int)((solid_slots + (size_t)sample_width - 1u)
                           / (size_t)sample_width);
        sample_height += solid_rows;
        /* The row is filled round-robin, so the padding is not one color. */
        solid_slots = (size_t)solid_rows * (size_t)sample_width;
    }

    if (sample_width_out != NULL) {
        *sample_width_out = sample_width;
    }
    if (sample_height_out != NULL) {
        *sample_height_out = sample_height;
    }

    sample_count = (size_t)sample_width * (size_t)sample_height;
    if (sample_count != 0u
            && sample_count / (size_t)sample_height != (size_t)sample_width) {
        sixel_allocator_free(allocator, normalized_src_pixels);
        return SIXEL_RUNTIME_ERROR;
    }

    payload_size = sample_count * (size_t)depth;
    if (sample_count != 0u && payload_size / sample_count != (size_t)depth) {
        sixel_allocator_free(allocator, normalized_src_pixels);
        return SIXEL_RUNTIME_ERROR;
    }

    status = sixel_filter_sample_create_frame(allocator, &sample_if);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, normalized_src_pixels);
        return status;
    }
    if (sample_if == NULL || sample_if->vtbl == NULL) {
        sixel_allocator_free(allocator, normalized_src_pixels);
        return SIXEL_BAD_ARGUMENT;
    }
    sample_vtbl = sample_if->vtbl;
    if (sample_vtbl->init_pixels == NULL ||
        sample_vtbl->set_timeline == NULL ||
        sample_vtbl->set_transparency == NULL ||
        sample_vtbl->unref == NULL) {
        sixel_allocator_free(allocator, normalized_src_pixels);
        return SIXEL_BAD_ARGUMENT;
    }
    sample = (sixel_frame_t *)sample_if;
    dst_pixels = (unsigned char *)sixel_allocator_malloc(allocator,
                                                         payload_size);
    if (dst_pixels == NULL) {
        sample_vtbl->unref(sample_if);
        sixel_allocator_free(allocator, normalized_src_pixels);
        return SIXEL_BAD_ALLOCATION;
    }

    pixels_request.pixels = dst_pixels;
    pixels_request.palette = NULL;
    pixels_request.width = sample_width;
    pixels_request.height = sample_height;
    pixels_request.pixelformat = src_pixelformat;
    pixels_request.colorspace = sixel_frame_get_colorspace(frame);
    pixels_request.ncolors = (-1);
    pixels_request.kind = SIXEL_FRAME_PIXELS_U8;

    status = sample_vtbl->init_pixels(sample_if, &pixels_request);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, dst_pixels);
        sample_vtbl->unref(sample_if);
        sixel_allocator_free(allocator, normalized_src_pixels);
        return status;
    }

    /* Preserve frame context in logs emitted by sampled palette jobs. */
    timeline.handoff_shareable = 0;
    status = sample_vtbl->set_timeline(sample_if, &timeline);
    if (SIXEL_FAILED(status)) {
        sample_vtbl->unref(sample_if);
        sixel_allocator_free(allocator, normalized_src_pixels);
        return status;
    }

    dst_transparency.transparent = src_transparency.transparent;
    dst_transparency.alpha_zero_is_transparent =
        src_transparency.alpha_zero_is_transparent;
    if (src_mask != NULL &&
        src_transparency.transparent_mask_size >= src_pixel_count) {
        /*
         * sample_count already includes the appended rows, so the mask covers
         * them; they are zero-filled below, which marks them opaque.  Sizing
         * the mask to the grid alone would leave every appended color outside
         * the declared extent.
         */
        mask_size = sample_count;
        dst_mask = (unsigned char *)sixel_allocator_malloc(allocator,
                                                           mask_size);
        if (dst_mask == NULL) {
            sample_vtbl->unref(sample_if);
            sixel_allocator_free(allocator, normalized_src_pixels);
            return SIXEL_BAD_ALLOCATION;
        }
        dst_transparency.transparent_mask = dst_mask;
        dst_transparency.transparent_mask_size = mask_size;
    }
    status = sample_vtbl->set_transparency(sample_if, &dst_transparency);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, dst_mask);
        sample_vtbl->unref(sample_if);
        sixel_allocator_free(allocator, normalized_src_pixels);
        return status;
    }

    dst_index = 0u;
    for (y = 0; y < height; y += (int)stride) {
        for (x = 0; x < width; x += (int)stride) {
            src_offset = ((size_t)(clip_y + y) * (size_t)src_width
                       + (size_t)(clip_x + x))
                       * (size_t)depth;
            memcpy(dst_pixels + dst_index * (size_t)depth,
                   src_pixels + src_offset,
                   (size_t)depth);
            if (dst_mask != NULL) {
                dst_mask[dst_index] =
                    src_mask[(size_t)(clip_y + y) * (size_t)src_width
                             + (size_t)(clip_x + x)];
            }
            ++dst_index;
        }

    }

    /*
     * Append the solid colors, round-robin so the row padding is shared out
     * rather than over-weighting whichever color happened to be last.
     */
    if (solid_count > 0u) {
        size_t slot;

        for (slot = 0u; slot < solid_slots && dst_index < sample_count;
             ++slot) {
            unsigned int which;

            which = (unsigned int)(slot % (size_t)solid_count);
            memcpy(dst_pixels + dst_index * (size_t)depth,
                   solid + (size_t)which * (size_t)depth,
                   (size_t)depth);
            if (dst_mask != NULL) {
                dst_mask[dst_index] = 0;
            }
            ++dst_index;
        }
    }

    *sample_out = sample;
    sixel_allocator_free(allocator, normalized_src_pixels);

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_filter_sample_apply(sixel_filter_t *filter,
                          sixel_allocator_t *allocator,
                          sixel_timeline_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_sample_state_t *state;
    sixel_frame_t *input_frame;
    sixel_frame_t *sample;
    int sample_height;

    if (filter == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_sample_state_t *)filter->userdata;
    if (state == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (filter->input.slot == NULL || filter->output.slot == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    input_frame = *(filter->input.slot);
    if (input_frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (*(filter->output.slot) != NULL) {
        sixel_frame_unref(*(filter->output.slot));
        *(filter->output.slot) = NULL;
    }

    sample_height = 0;
    status = sixel_filter_sample_copy_frame(&state->config, input_frame,
                                            allocator, &sample, logger,
                                            NULL, &sample_height);
    if (SIXEL_SUCCEEDED(status)) {
        *(filter->output.slot) = sample;
        if (sample_height > 0) {
            filter->progress.total_units = sample_height;
            filter->progress.completed_units = sample_height;
            sixel_filter_update_progress(filter, sample_height);
        }
    }

    return status;
}

static void
sixel_filter_sample_dispose(sixel_filter_t *filter)
{
    sixel_filter_sample_state_t *state;

    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_sample_state_t *)filter->userdata;
    if (state != NULL) {
        free(state);
    }
}

SIXELSTATUS
sixel_filter_sample_init(sixel_filter_t *filter,
                         const sixel_filter_sample_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_sample_state_t *state;

    if (filter == NULL || config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_sample_state_t *)malloc(sizeof(*state));
    if (state == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    state->config = *config;

    status = sixel_filter_init_with_vtbl(
        filter,
        &sixel_filter_sample_vtbl,
        state);
    if (SIXEL_FAILED(status)) {
        free(state);
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
