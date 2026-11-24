/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
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

#ifndef LIBSIXEL_FRAME_H
#define LIBSIXEL_FRAME_H

#include <6cells.h>
#include "pixelformat.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Factory adapter for image/frame.
 *
 * Public sixel_frame_new() keeps its legacy out-parameter order; the
 * generated factory registry expects allocator-first constructors.
 */
/* @classid image/frame */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_factory_new(sixel_allocator_t *allocator, void **object);

/* Project the layout and coordinates for internal working-buffer consumers. */
SIXEL_INTERNAL_API int
sixel_frame_format(sixel_frame_t const *frame);

/* Borrow native samples whose layout and coordinates are known separately. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_borrow_pixels(sixel_frame_t *frame,
                         sixel_frame_pixels_request_t const *request);

/* Adapt public layouts at the boundary to existing work-buffer kernels. */
static inline SIXELSTATUS
sixel_frame_get_work_pixels(sixel_frame_interface_t *frame,
                            sixel_frame_pixels_view_t *view)
{
    SIXELSTATUS status;

    status = frame->vtbl->get_pixels(frame, view);
    if (SIXEL_SUCCEEDED(status) &&
            view->pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32) {
        view->pixelformat =
            sixel_pixelformat_for_colorspace(view->colorspace, 1);
    }
    return status;
}

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_FRAME_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
