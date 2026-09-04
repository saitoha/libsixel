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

#ifndef LIBSIXEL_SAMPLE_STREAM_H
#define LIBSIXEL_SAMPLE_STREAM_H

#include <sixel.h>

#include "palette-plan.h"

typedef enum sixel_sample_stream_storage {
    SIXEL_SAMPLE_STREAM_EMPTY = 0,
    SIXEL_SAMPLE_STREAM_BORROWED_FRAME,
    SIXEL_SAMPLE_STREAM_OWNED_FRAME
} sixel_sample_stream_storage_t;

/*
 * Transitional typed artifact for the palette branch.
 *
 * The frame payload keeps the current contiguous adapter available while the
 * DAG learns to distinguish a sample stream from an ordinary image frame.
 * Metadata is captured when the artifact is bound so later stages do not need
 * to infer sampling provenance from frame dimensions or scheduler state.
 */
typedef struct sixel_sample_stream {
    sixel_frame_t *frame;
    sixel_sample_stream_storage_t storage;
    sixel_palette_sampling_policy_t policy;
    sixel_palette_sampling_source_t source;
    size_t point_count;
    int width;
    int height;
    int pixelformat;
    int colorspace;
} sixel_sample_stream_t;

SIXEL_INTERNAL_API void
sixel_sample_stream_init(sixel_sample_stream_t *stream);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_sample_stream_bind_borrowed(
    sixel_sample_stream_t *stream,
    sixel_frame_t *frame,
    sixel_palette_sampling_policy_t policy,
    sixel_palette_sampling_source_t source);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_sample_stream_take_owned(
    sixel_sample_stream_t *stream,
    sixel_frame_t **frame_slot,
    sixel_palette_sampling_policy_t policy,
    sixel_palette_sampling_source_t source);

SIXEL_INTERNAL_API void
sixel_sample_stream_dispose(sixel_sample_stream_t *stream);

#endif /* LIBSIXEL_SAMPLE_STREAM_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
