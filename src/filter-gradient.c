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

#if HAVE_MATH_H
# include <math.h>
#endif  /* HAVE_MATH_H */
#if HAVE_STRING_H
# include <string.h>
#endif  /* HAVE_STRING_H */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <sixel.h>

#include "compat_stub.h"
#include "dither.h"
#include "filter-gradient.h"
#include "filter.h"
#include "frame.h"

typedef struct sixel_filter_gradient_state {
    sixel_filter_gradient_config_t config;
} sixel_filter_gradient_state_t;

static SIXELSTATUS
sixel_filter_gradient_apply(sixel_filter_t *filter,
                            sixel_allocator_t *allocator,
                            sixel_timeline_logger_t *logger);

static void
sixel_filter_gradient_dispose(sixel_filter_t *filter);

static sixel_filter_vtbl_t const sixel_filter_gradient_vtbl = {
    "gradient-map",
    SIXEL_FILTER_KIND_GRADIENT,
    sixel_filter_gradient_apply,
    sixel_filter_gradient_dispose,
    NULL,
    NULL,
    NULL
};

static unsigned char
sixel_filter_gradient_luma_from_rgb(unsigned char r,
                                    unsigned char g,
                                    unsigned char b)
{
    unsigned int luma;

    /*
     * Convert to luma in byte range with fixed-point Rec.601 weights.
     * This keeps the Sobel input deterministic across platforms.
     */
    luma = 77u * (unsigned int)r
         + 150u * (unsigned int)g
         + 29u * (unsigned int)b
         + 128u;

    return (unsigned char)(luma >> 8);
}

static float
sixel_filter_gradient_resolve_factor(sixel_dither_t const *dither)
{
    char const *text;
    char *endptr;
    double parsed;
    float resolved;

    text = NULL;
    endptr = NULL;
    parsed = 0.0;
    resolved = 0.0f;

    if (dither == NULL
            || dither->method_for_diffuse != SIXEL_DIFFUSE_BLUENOISE_DITHER) {
        return 0.0f;
    }
    if (dither->bluenoise_gradient_factor_override != 0) {
        if (dither->bluenoise_gradient_factor > 0.0f) {
            return dither->bluenoise_gradient_factor;
        }
        return 0.0f;
    }

    text = sixel_compat_getenv("SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR");
    if (text == NULL || text[0] == '\0') {
        return 0.0f;
    }

    errno = 0;
    parsed = strtod(text, &endptr);
    if (endptr == text
            || endptr == NULL
            || endptr[0] != '\0'
            || errno != 0
            || parsed <= 0.0) {
        return 0.0f;
    }

    resolved = (float)parsed;
    if (resolved <= 0.0f) {
        return 0.0f;
    }
    return resolved;
}

static SIXELSTATUS
sixel_filter_gradient_build_luma_map(
    sixel_frame_t *frame,
    sixel_allocator_t *allocator,
    unsigned char **luma_out)
{
    SIXELSTATUS status;
    unsigned char *luma;
    unsigned char *normalized;
    unsigned char const *palette;
    unsigned char index_u8;
    int normalized_pixelformat;
    int source_pixelformat;
    int ncolors;
    int width;
    int height;
    size_t pixel_count;
    size_t normalized_size;
    size_t index;
    size_t base;

    status = SIXEL_FALSE;
    luma = NULL;
    normalized = NULL;
    palette = NULL;
    index_u8 = 0U;
    normalized_pixelformat = 0;
    source_pixelformat = 0;
    ncolors = 0;
    width = 0;
    height = 0;
    pixel_count = 0U;
    normalized_size = 0U;
    index = 0U;
    base = 0U;

    if (frame == NULL || allocator == NULL || luma_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *luma_out = NULL;

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    if (width <= 0 || height <= 0) {
        return SIXEL_OK;
    }

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INPUT;
    }
    pixel_count = (size_t)width * (size_t)height;

    luma = (unsigned char *)sixel_allocator_malloc(allocator, pixel_count);
    if (luma == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    if (pixel_count > SIZE_MAX / 3u) {
        sixel_allocator_free(allocator, luma);
        return SIXEL_BAD_INPUT;
    }
    normalized_size = pixel_count * 3u;
    normalized = (unsigned char *)sixel_allocator_malloc(allocator,
                                                         normalized_size);
    if (normalized == NULL) {
        sixel_allocator_free(allocator, luma);
        return SIXEL_BAD_ALLOCATION;
    }

    source_pixelformat = sixel_frame_get_pixelformat(frame);
    status = sixel_helper_normalize_pixelformat(normalized,
                                                &normalized_pixelformat,
                                                sixel_frame_get_pixels(frame),
                                                source_pixelformat,
                                                width,
                                                height);
    if (SIXEL_FAILED(status)) {
        /*
         * Unsupported formats should not abort encoding. A zero luma map
         * disables gradient attenuation while preserving legacy output.
         */
        memset(luma, 0, pixel_count);
        status = SIXEL_OK;
        goto end;
    }

    switch (normalized_pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
        for (index = 0U; index < pixel_count; ++index) {
            base = index * 3U;
            luma[index] = sixel_filter_gradient_luma_from_rgb(
                normalized[base + 0U],
                normalized[base + 1U],
                normalized[base + 2U]);
        }
        break;
    case SIXEL_PIXELFORMAT_G8:
        memcpy(luma, normalized, pixel_count);
        break;
    case SIXEL_PIXELFORMAT_PAL8:
        palette = sixel_frame_get_palette(frame);
        ncolors = sixel_frame_get_ncolors(frame);
        for (index = 0U; index < pixel_count; ++index) {
            index_u8 = normalized[index];
            if (palette != NULL
                    && ncolors > 0
                    && (int)index_u8 < ncolors) {
                base = (size_t)index_u8 * 3U;
                luma[index] = sixel_filter_gradient_luma_from_rgb(
                    palette[base + 0U],
                    palette[base + 1U],
                    palette[base + 2U]);
            } else {
                luma[index] = index_u8;
            }
        }
        break;
    default:
        memset(luma, 0, pixel_count);
        break;
    }

end:
    sixel_allocator_free(allocator, normalized);
    if (SIXEL_SUCCEEDED(status)) {
        *luma_out = luma;
    } else if (luma != NULL) {
        sixel_allocator_free(allocator, luma);
    }

    return status;
}

static SIXELSTATUS
sixel_filter_gradient_build_sobel_map(
    unsigned char const *luma,
    int width,
    int height,
    unsigned char *gradient_map)
{
    int x;
    int y;
    int x0;
    int x1;
    int x2;
    int y0;
    int y1;
    int y2;
    int tl;
    int tc;
    int tr;
    int ml;
    int mr;
    int bl;
    int bc;
    int br;
    int gx;
    int gy;
    float magnitude;
    float scaled;
    size_t center;

    x = 0;
    y = 0;
    x0 = 0;
    x1 = 0;
    x2 = 0;
    y0 = 0;
    y1 = 0;
    y2 = 0;
    tl = 0;
    tc = 0;
    tr = 0;
    ml = 0;
    mr = 0;
    bl = 0;
    bc = 0;
    br = 0;
    gx = 0;
    gy = 0;
    magnitude = 0.0f;
    scaled = 0.0f;
    center = 0U;

    if (luma == NULL || gradient_map == NULL || width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (y = 0; y < height; ++y) {
        y0 = (y > 0) ? (y - 1) : y;
        y1 = y;
        y2 = (y + 1 < height) ? (y + 1) : y;
        for (x = 0; x < width; ++x) {
            x0 = (x > 0) ? (x - 1) : x;
            x1 = x;
            x2 = (x + 1 < width) ? (x + 1) : x;

            tl = luma[y0 * width + x0];
            tc = luma[y0 * width + x1];
            tr = luma[y0 * width + x2];
            ml = luma[y1 * width + x0];
            mr = luma[y1 * width + x2];
            bl = luma[y2 * width + x0];
            bc = luma[y2 * width + x1];
            br = luma[y2 * width + x2];

            gx = -tl - (2 * ml) - bl + tr + (2 * mr) + br;
            gy = tl + (2 * tc) + tr - bl - (2 * bc) - br;
            magnitude = sqrtf((float)(gx * gx + gy * gy));
            scaled = magnitude * (255.0f / 1443.0f);
            if (scaled > 255.0f) {
                scaled = 255.0f;
            }

            center = (size_t)y * (size_t)width + (size_t)x;
            gradient_map[center] = (unsigned char)(scaled + 0.5f);
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_filter_gradient_apply(sixel_filter_t *filter,
                            sixel_allocator_t *allocator,
                            sixel_timeline_logger_t *logger)
{
    SIXELSTATUS status;
    sixel_filter_gradient_state_t *state;
    sixel_frame_t *frame;
    unsigned char *luma;
    unsigned char *gradient_map;
    float gradient_factor;
    int width;
    int height;
    size_t map_size;

    status = SIXEL_FALSE;
    state = NULL;
    frame = NULL;
    luma = NULL;
    gradient_map = NULL;
    gradient_factor = 0.0f;
    width = 0;
    height = 0;
    map_size = 0U;

    if (filter == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_gradient_state_t *)filter->userdata;
    if (state == NULL || state->config.dither == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (filter->input.slot == NULL || filter->input.slot[0] == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    frame = filter->input.slot[0];
    gradient_factor = sixel_filter_gradient_resolve_factor(
        state->config.dither);
    if (gradient_factor <= 0.0f) {
        sixel_dither_clear_bluenoise_gradient_map_hint(state->config.dither);
        if (filter->output.slot != NULL) {
            filter->output.slot[0] = frame;
        }
        filter->progress.total_units = 1;
        filter->progress.completed_units = 1;
        (void)sixel_filter_update_progress(filter, 1);
        return SIXEL_OK;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    if (width <= 0 || height <= 0) {
        sixel_dither_clear_bluenoise_gradient_map_hint(state->config.dither);
        if (filter->output.slot != NULL) {
            filter->output.slot[0] = frame;
        }
        filter->progress.total_units = 1;
        filter->progress.completed_units = 1;
        (void)sixel_filter_update_progress(filter, 1);
        return SIXEL_OK;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INPUT;
    }
    map_size = (size_t)width * (size_t)height;

    gradient_map = (unsigned char *)sixel_allocator_malloc(allocator,
                                                           map_size);
    if (gradient_map == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    status = sixel_filter_gradient_build_luma_map(frame, allocator, &luma);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, gradient_map);
        return status;
    }
    if (luma == NULL) {
        memset(gradient_map, 0, map_size);
    } else {
        status = sixel_filter_gradient_build_sobel_map(luma,
                                                       width,
                                                       height,
                                                       gradient_map);
        sixel_allocator_free(allocator, luma);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(allocator, gradient_map);
            return status;
        }
    }

    status = sixel_dither_set_bluenoise_gradient_map_hint(
        state->config.dither,
        gradient_map,
        map_size,
        width,
        height);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, gradient_map);
        return status;
    }

    if (filter->output.slot != NULL) {
        filter->output.slot[0] = frame;
    }

    if (logger != NULL) {
        sixel_timeline_logger_logf(logger,
                          "filter",
                          "worker",
                          "gradient-map",
                          -1,
                          -1,
                          0,
                          0,
                          width,
                          height,
                          "sobel-l2 map generated");
    }

    filter->progress.total_units = height;
    filter->progress.completed_units = height;
    (void)sixel_filter_update_progress(filter, height);

    return SIXEL_OK;
}

static void
sixel_filter_gradient_dispose(sixel_filter_t *filter)
{
    sixel_filter_gradient_state_t *state;

    if (filter == NULL) {
        return;
    }

    state = (sixel_filter_gradient_state_t *)filter->userdata;
    if (state != NULL) {
        free(state);
    }
}

SIXELSTATUS
sixel_filter_gradient_init(sixel_filter_t *filter,
                           const sixel_filter_gradient_config_t *config)
{
    SIXELSTATUS status;
    sixel_filter_gradient_state_t *state;

    status = SIXEL_FALSE;
    state = NULL;

    if (filter == NULL || config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    state = (sixel_filter_gradient_state_t *)calloc(
        1u, sizeof(sixel_filter_gradient_state_t));
    if (state == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    state->config = *config;

    status = sixel_filter_init_with_vtbl(filter,
                                         &sixel_filter_gradient_vtbl,
                                         state);
    if (SIXEL_FAILED(status)) {
        free(state);
        return status;
    }

    filter->progress.total_units = 0;
    filter->progress.completed_units = 0;

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
