/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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

#ifndef SIXEL_DECODER_IMAGE_H
#define SIXEL_DECODER_IMAGE_H

#include <sixel.h>

#ifndef SIXEL_PALETTE_MAX_DECODER
#define SIXEL_PALETTE_MAX_DECODER 65536
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct image_buffer {
    union {
        void *p;
        unsigned char *in_bytes;
        unsigned short *in_shorts;
    } pixels;
    int width;
    int height;
    int depth;
    int ncolors;
    int palette[SIXEL_PALETTE_MAX_DECODER];
} image_buffer_t;

SIXELSTATUS image_buffer_init(image_buffer_t *image,
                              int width,
                              int height,
                              int bgindex,
                              int depth,
                              sixel_allocator_t *allocator);

SIXELSTATUS image_buffer_resize(image_buffer_t *image,
                                int width,
                                int height,
                                int bgindex,
                                sixel_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif /* SIXEL_DECODER_IMAGE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
