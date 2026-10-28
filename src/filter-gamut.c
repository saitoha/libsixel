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

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif
#include <limits.h>
#include <stdlib.h>
#include "filter-gamut.h"

/* This component has no geometry or color-conversion side effects. It maps
 * only the visible samples of an owned application or palette-sample plane.
 * A tile borrows the same immutable mapper as an ordinary full-frame run. */
static SIXELSTATUS
sixel_filter_gamut_apply(sixel_filter_t *filter,
    sixel_allocator_t *allocator, sixel_timeline_logger_t *logger)
{
    sixel_filter_gamut_config_t const *config;
    sixel_frame_t *frame;
    sixel_frame_interface_t *object;
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t transparency;
    SIXELSTATUS status;
    size_t count;

    (void)allocator;
    (void)logger;
    config = filter->userdata;
    frame = filter->input.slot != NULL ? *filter->input.slot
        : filter->input.sample_stream != NULL
            ? filter->input.sample_stream->frame : NULL;
    if (frame == NULL || config == NULL || config->mapper == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    object = (sixel_frame_interface_t *)frame;
    status = object->vtbl->get_pixels(object, &view);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (!SIXEL_PIXELFORMAT_IS_FLOAT32(view.pixelformat) ||
            view.pixels_float32 == NULL || view.width <= 0 ||
            view.height <= 0 || (size_t)view.width >
                SIZE_MAX / (size_t)view.height) {
        return SIXEL_BAD_INPUT;
    }
    status = object->vtbl->get_transparency(object, &transparency);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    count = (size_t)view.width * (size_t)view.height;
    status = sixel_gamut_apply_float32(config->mapper, view.pixels_float32,
        count, view.colorspace, transparency.transparent_mask,
        transparency.transparent_mask_size);
    if (SIXEL_SUCCEEDED(status)) {
        if (filter->output.slot != NULL) {
            *filter->output.slot = frame;
        }
        filter->progress.total_units = view.height;
        sixel_filter_update_progress(filter, view.height);
    }
    return status;
}

static void
sixel_filter_gamut_dispose(sixel_filter_t *filter)
{
    free(filter->userdata);
}

static sixel_filter_vtbl_t const sixel_filter_gamut_vtbl = {
    "gamut", SIXEL_FILTER_KIND_GAMUT,
    sixel_filter_gamut_apply, sixel_filter_gamut_dispose, NULL, NULL, NULL
};

SIXELSTATUS
sixel_filter_gamut_init(sixel_filter_t *filter,
    sixel_filter_gamut_config_t const *config)
{
    sixel_filter_gamut_config_t *state;
    SIXELSTATUS status;

    if (filter == NULL || config == NULL || config->mapper == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    state = malloc(sizeof(*state));
    if (state == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    *state = *config;
    status = sixel_filter_init_with_vtbl(filter,
        &sixel_filter_gamut_vtbl, state);
    if (SIXEL_FAILED(status)) {
        free(state);
        return status;
    }
    filter->flags |= SIXEL_FILTER_FLAG_FLOAT32_TILES;
    return SIXEL_OK;
}
