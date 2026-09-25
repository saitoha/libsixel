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

#ifndef LIBSIXEL_WRITER_H
#define LIBSIXEL_WRITER_H

#include <sixel.h>
#include "cms.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NULL means legacy untagged output. A request selects either the PNG sRGB
 * chunk or an RGB definition to serialize as ICC, never both. It labels
 * encoded PAL8/RGB888/RGBA8888 samples; it does not convert working pixels. */
typedef struct sixel_writer_color {
    int srgb;
    sixel_color_def_t const *definition;
} sixel_writer_color_t;

SIXEL_INTERNAL_API SIXELSTATUS
sixel_write_image_color(unsigned char *data, int width, int height,
                         unsigned char *palette, int pixelformat,
                         char const *filename, int imageformat,
                         sixel_writer_color_t const *color,
                         sixel_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_WRITER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
