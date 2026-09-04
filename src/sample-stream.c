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

#include <sixel.h>

#include "sample-stream.h"

static int
sixel_sample_stream_policy_is_valid(
    sixel_palette_sampling_policy_t policy)
{
    return policy == SIXEL_PALETTE_SAMPLING_FULL_FRAME ||
        policy == SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID;
}

static int
sixel_sample_stream_source_is_valid(
    sixel_palette_sampling_source_t source)
{
    return source == SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME ||
        source == SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME;
}

void
sixel_sample_stream_init(sixel_sample_stream_t *stream)
{
    if (stream == NULL) {
        return;
    }

    stream->frame = NULL;
    stream->storage = SIXEL_SAMPLE_STREAM_EMPTY;
    stream->policy = SIXEL_PALETTE_SAMPLING_AUTO;
    stream->source = SIXEL_PALETTE_SAMPLING_SOURCE_NONE;
    stream->point_count = 0u;
    stream->width = 0;
    stream->height = 0;
    stream->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    stream->colorspace = SIXEL_COLORSPACE_GAMMA;
}

static SIXELSTATUS
sixel_sample_stream_bind(
    sixel_sample_stream_t *stream,
    sixel_frame_t *frame,
    sixel_sample_stream_storage_t storage,
    sixel_palette_sampling_policy_t policy,
    sixel_palette_sampling_source_t source)
{
    size_t width;
    size_t height;

    if (stream == NULL || frame == NULL ||
            !sixel_sample_stream_policy_is_valid(policy) ||
            !sixel_sample_stream_source_is_valid(source)) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (stream->storage != SIXEL_SAMPLE_STREAM_EMPTY) {
        return SIXEL_LOGIC_ERROR;
    }

    stream->width = sixel_frame_get_width(frame);
    stream->height = sixel_frame_get_height(frame);
    if (stream->width <= 0 || stream->height <= 0) {
        sixel_sample_stream_init(stream);
        return SIXEL_BAD_ARGUMENT;
    }
    width = (size_t)stream->width;
    height = (size_t)stream->height;
    if (width > SIZE_MAX / height) {
        sixel_sample_stream_init(stream);
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    stream->frame = frame;
    stream->storage = storage;
    stream->policy = policy;
    stream->source = source;
    stream->point_count = width * height;
    stream->pixelformat = sixel_frame_get_pixelformat(frame);
    stream->colorspace = sixel_frame_get_colorspace(frame);
    return SIXEL_OK;
}

SIXELSTATUS
sixel_sample_stream_bind_borrowed(
    sixel_sample_stream_t *stream,
    sixel_frame_t *frame,
    sixel_palette_sampling_policy_t policy,
    sixel_palette_sampling_source_t source)
{
    return sixel_sample_stream_bind(stream,
                                    frame,
                                    SIXEL_SAMPLE_STREAM_BORROWED_FRAME,
                                    policy,
                                    source);
}

SIXELSTATUS
sixel_sample_stream_take_owned(
    sixel_sample_stream_t *stream,
    sixel_frame_t **frame_slot,
    sixel_palette_sampling_policy_t policy,
    sixel_palette_sampling_source_t source)
{
    SIXELSTATUS status;

    if (frame_slot == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = sixel_sample_stream_bind(stream,
                                      *frame_slot,
                                      SIXEL_SAMPLE_STREAM_OWNED_FRAME,
                                      policy,
                                      source);
    if (SIXEL_SUCCEEDED(status)) {
        *frame_slot = NULL;
    }
    return status;
}

SIXELSTATUS
sixel_sample_stream_refresh(sixel_sample_stream_t *stream)
{
    size_t width;
    size_t height;

    if (stream == NULL || stream->frame == NULL ||
            stream->storage == SIXEL_SAMPLE_STREAM_EMPTY) {
        return SIXEL_BAD_ARGUMENT;
    }

    stream->width = sixel_frame_get_width(stream->frame);
    stream->height = sixel_frame_get_height(stream->frame);
    if (stream->width <= 0 || stream->height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    width = (size_t)stream->width;
    height = (size_t)stream->height;
    if (width > SIZE_MAX / height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    stream->point_count = width * height;
    stream->pixelformat = sixel_frame_get_pixelformat(stream->frame);
    stream->colorspace = sixel_frame_get_colorspace(stream->frame);
    return SIXEL_OK;
}

void
sixel_sample_stream_dispose(sixel_sample_stream_t *stream)
{
    if (stream == NULL) {
        return;
    }
    if (stream->storage == SIXEL_SAMPLE_STREAM_OWNED_FRAME &&
            stream->frame != NULL) {
        sixel_frame_unref(stream->frame);
    }
    sixel_sample_stream_init(stream);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
