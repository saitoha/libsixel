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

#ifndef LIBSIXEL_FILTER_CLIP_H
#define LIBSIXEL_FILTER_CLIP_H

#include <sixel.h>

#include "filter.h"
#include "logger.h"

/*
 * Clip filter configuration. The planner passes the region to preserve and the
 * filter trims the frame in-place. A zero or negative width/height disables the
 * operation.
 */
typedef struct sixel_filter_clip_config {
    int clip_x;
    int clip_y;
    int clip_width;
    int clip_height;
} sixel_filter_clip_config_t;

SIXELSTATUS
sixel_filter_clip_init(sixel_filter_t *filter,
                       const sixel_filter_clip_config_t *config);

SIXELSTATUS
sixel_filter_clip_frame(const sixel_filter_clip_config_t *config,
                        sixel_frame_t *frame,
                        sixel_logger_t *logger);

#endif /* LIBSIXEL_FILTER_CLIP_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
