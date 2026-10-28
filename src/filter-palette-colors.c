/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "filter-palette-colors.h"
#include "sample-statistics.h"
#include "loader-common.h"

static SIXELSTATUS
sixel_filter_palette_colors_convert(sixel_frame_t *frame,
                                        int target_colorspace)
{
    SIXELSTATUS status;
    int source_colorspace;
    int pixelformat;
    int depth;
    int width;
    int height;
    size_t pixel_total;
    size_t pixel_bytes;
    unsigned char *pixels;
    sixel_frame_pixels_view_t view;
    sixel_frame_interface_t *frame_if;

    status = SIXEL_BAD_ARGUMENT;
    source_colorspace = SIXEL_COLORSPACE_GAMMA;
    pixelformat = SIXEL_PIXELFORMAT_RGB888;
    depth = 0;
    width = 0;
    height = 0;
    pixel_total = 0U;
    pixel_bytes = 0U;
    pixels = NULL;
    memset(&view, 0, sizeof(view));

    if (frame == NULL) {
        return status;
    }

    source_colorspace = sixel_frame_get_colorspace(frame);
    if (source_colorspace == target_colorspace) {
        return SIXEL_OK;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    if (width <= 0 || height <= 0) {
        return SIXEL_BAD_INPUT;
    }

    pixelformat = sixel_frame_get_pixelformat(frame);
    depth = sixel_helper_compute_depth(pixelformat);
    if (depth <= 0) {
        return SIXEL_BAD_INPUT;
    }

    pixel_total = (size_t)width * (size_t)height;
    if (pixel_total / (size_t)width != (size_t)height) {
        return SIXEL_BAD_INPUT;
    }
    if (pixel_total > SIZE_MAX / (size_t)depth) {
        return SIXEL_BAD_INPUT;
    }
    pixel_bytes = pixel_total * (size_t)depth;

    frame_if = sixel_frame_as_interface(frame);
    status = frame_if->vtbl->get_pixels(frame_if, &view);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    pixels = SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)
        ? (unsigned char *)view.pixels_float32
        : view.pixels;

    /* Both precisions use the buffer driver. Float arithmetic preserves
     * extended coordinates, with the common parallel threshold and budget;
     * byte storage retains its separate LUT/SIMD implementation. */
    status = sixel_helper_convert_colorspace(pixels, pixel_bytes,
        pixelformat, source_colorspace, target_colorspace);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    sixel_frame_set_colorspace(frame, target_colorspace);
    return SIXEL_OK;
}

/* The output owns converted storage. Identity output borrows the input
 * payload, but owns its statistics reference independently of filter life. */
static SIXELSTATUS
sixel_filter_palette_colors_apply(sixel_filter_t *filter,
    sixel_allocator_t *allocator, sixel_timeline_logger_t *logger)
{
    sixel_filter_palette_colors_config_t const *config;
    sixel_sample_stream_t *input;
    sixel_sample_stream_t *samples_out;
    sixel_weighted_point_set_t *points;
    sixel_weighted_point_set_t *result;
    sixel_sample_statistics_t *statistics;
    sixel_frame_interface_t *frame_if;
    sixel_frame_interface_t *clone_if;
    sixel_frame_t *clone;
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t transparency;
    sixel_frame_pixels_view_t gamma_view;
    sixel_frame_transparency_t gamma_transparency;
    SIXELSTATUS status;
    double sample[3];
    double gamma_sample[3];
    size_t index;
    size_t count;
    unsigned int channels;
    unsigned int gamma_channels;
    unsigned int axis;
    int floating;
    int visible;
    int colorspace;
    int byte_coordinates;
    int gamma_floating;
    int gamma_visible;
    int reuse_gamma;

    config = filter->userdata;
    input = filter->input.sample_stream;
    samples_out = filter->output.sample_stream;
    points = filter->input.weighted_points;
    result = filter->output.weighted_points;
    statistics = NULL;
    clone_if = NULL;
    clone = NULL;
    reuse_gamma = 0;
    if (config == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (points != NULL) {
        if (input != NULL || result == NULL || points == result ||
                config->convert || config->copy_payload ||
                points->policy != SIXEL_PALETTE_BINNING_NONE ||
                points->ownership == SIXEL_WEIGHTED_POINT_EMPTY ||
                points->weights != NULL ||
                result->ownership != SIXEL_WEIGHTED_POINT_EMPTY) {
            return SIXEL_BAD_ARGUMENT;
        }
        *result = *points;
        result->ownership = SIXEL_WEIGHTED_POINT_BORROWED;
        result->allocator = NULL;
        result->statistics = NULL;
        count = points->point_count;
        colorspace = points->colorspace;
        byte_coordinates = config->byte_coordinates;
    } else {
        if (input == NULL || samples_out == NULL || input == samples_out ||
                input->storage == SIXEL_SAMPLE_STREAM_EMPTY ||
                samples_out->storage != SIXEL_SAMPLE_STREAM_EMPTY) {
            return SIXEL_BAD_ARGUMENT;
        }
        if (config->convert || config->copy_payload) {
            if (input->frame == NULL) {
                return SIXEL_BAD_ARGUMENT;
            }
            /* Keep the transform transaction observable independently of
             * the parallel colorspace driver running inside this filter. */
            if (logger != NULL) {
                sixel_timeline_logger_logf(logger, "filter", "worker",
                    "palette-colorspace-start", -1, -1, 0, 0,
                    input->width, input->height,
                    "src=format%d colorspace%d dst=format%d colorspace%d",
                    input->pixelformat, input->colorspace,
                    config->target_pixelformat, config->target_colorspace);
            }
            frame_if = sixel_frame_as_interface(input->frame);
            status = frame_if->vtbl->clone(frame_if, allocator, &clone_if);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            if (clone_if == NULL) {
                return SIXEL_BAD_ALLOCATION;
            }
            clone = (sixel_frame_t *)clone_if;
            if (config->convert) {
                status = sixel_frame_set_pixelformat(clone,
                    config->target_pixelformat);
                if (SIXEL_SUCCEEDED(status)) {
                    status = sixel_filter_palette_colors_convert(clone,
                        config->target_colorspace);
                }
                if (SIXEL_FAILED(status)) {
                    sixel_frame_unref(clone);
                    return status;
                }
            }
            status = sixel_sample_stream_take_owned(samples_out, &clone,
                input->policy, input->source);
            sixel_frame_unref(clone);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        } else {
            *samples_out = *input;
            if (samples_out->storage == SIXEL_SAMPLE_STREAM_OWNED_FRAME) {
                samples_out->storage = SIXEL_SAMPLE_STREAM_BORROWED_FRAME;
            }
            samples_out->statistics = NULL;
        }
        count = samples_out->point_count;
        colorspace = samples_out->colorspace;
        byte_coordinates =
            !SIXEL_PIXELFORMAT_IS_FLOAT32(samples_out->pixelformat);
    }
    if (config->collect_statistics) {
        status = sixel_sample_statistics_new(&statistics,
            colorspace, byte_coordinates, allocator);
        if (SIXEL_FAILED(status)) {
            goto fail;
        }
        if (points == NULL) {
            status = sixel_sample_stream_get_view(samples_out, &view,
                &transparency, &channels, &floating);
            if (SIXEL_FAILED(status)) {
                goto fail;
            }
            /* Input and output have the same retained sample order. The
             * producer alone owns both color representations, so it can
             * reuse gamma evidence without exposing source pixels or a
             * color-conversion callback to the binning consumer. */
            reuse_gamma = config->convert &&
                input->colorspace == SIXEL_COLORSPACE_GAMMA &&
                samples_out->colorspace != SIXEL_COLORSPACE_GAMMA;
            if (reuse_gamma) {
                status = sixel_sample_stream_get_view(input, &gamma_view,
                    &gamma_transparency, &gamma_channels, &gamma_floating);
                if (SIXEL_FAILED(status)) {
                    goto fail;
                }
            }
        }
        for (index = 0u; index < count; ++index) {
            if (points != NULL) {
                memcpy(sample, points->coordinates + index * 3u,
                    sizeof(sample));
            } else {
                status = sixel_sample_stream_read(&view, &transparency,
                    channels, floating, index, sample, &visible);
                if (SIXEL_FAILED(status)) {
                    goto fail;
                }
                if (!visible) {
                    continue;
                }
            }
            if (reuse_gamma) {
                status = sixel_sample_stream_read(&gamma_view,
                    &gamma_transparency, gamma_channels, gamma_floating,
                    index, gamma_sample, &gamma_visible);
                if (SIXEL_FAILED(status)) {
                    goto fail;
                }
                if (!gamma_visible) {
                    status = SIXEL_LOGIC_ERROR;
                    goto fail;
                }
                if (!gamma_floating) {
                    for (axis = 0u; axis < 3u; ++axis) {
                        gamma_sample[axis] /= 255.0;
                    }
                }
            }
            status = sixel_sample_statistics_observe_gamma(statistics,
                sample, reuse_gamma ? gamma_sample : NULL);
            if (SIXEL_FAILED(status)) {
                goto fail;
            }
        }
    }
    sixel_trace_topic_message("palette_contract",
        "LSXPCS1|convert=%d|statistics=%d|colorspace=%d|points=%lu",
        config->convert, config->collect_statistics, colorspace,
        (unsigned long)(statistics != NULL ? statistics->point_count : count));
    if (points != NULL) {
        result->statistics = statistics;
    } else {
        samples_out->statistics = statistics;
    }
    if (logger != NULL && (config->convert || config->copy_payload)) {
        sixel_timeline_logger_logf(logger, "filter", "worker",
            "palette-colorspace-finish", -1, -1, 0, 0,
            samples_out->width, samples_out->height,
            "result=format%d colorspace%d statistics=%d",
            samples_out->pixelformat, samples_out->colorspace,
            config->collect_statistics);
    }
    filter->progress.total_units = 1;
    return sixel_filter_update_progress(filter, 1);

fail:
    sixel_sample_statistics_unref(statistics);
    if (points != NULL) {
        sixel_weighted_point_set_dispose(result);
    } else {
        sixel_sample_stream_dispose(samples_out);
    }
    return status;
}

static void
sixel_filter_palette_colors_dispose(sixel_filter_t *filter)
{
    free(filter->userdata);
}

static sixel_filter_vtbl_t const sixel_filter_palette_colors_vtbl = {
    "palette-colorspace",
    SIXEL_FILTER_KIND_PALETTE_COLORS,
    sixel_filter_palette_colors_apply,
    sixel_filter_palette_colors_dispose,
    NULL,
    NULL,
    NULL
};

SIXELSTATUS
sixel_filter_palette_colors_init(sixel_filter_t *filter,
    sixel_filter_palette_colors_config_t const *config)
{
    sixel_filter_palette_colors_config_t *state;
    SIXELSTATUS status;

    if (filter == NULL || config == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    state = malloc(sizeof(*state));
    if (state == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    *state = *config;
    status = sixel_filter_init_with_vtbl(filter,
        &sixel_filter_palette_colors_vtbl, state);
    if (SIXEL_FAILED(status)) {
        free(state);
    }
    return status;
}
