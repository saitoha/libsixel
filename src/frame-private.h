/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_FRAME_PRIVATE_H
#define LIBSIXEL_FRAME_PRIVATE_H

#include "frame.h"
#include "sixel_atomic.h"

/* Private frame object storage. Keep src/frame.h storage-opaque. */
struct sixel_frame {
    sixel_frame_interface_t frame_interface; /* IFrame dispatch header */
    sixel_atomic_u32_t ref;         /* reference counter */
    union {
        unsigned char *u8ptr;       /* loaded pixel data (byte) */
        float *f32ptr;              /* loaded pixel data (float32) */
    } pixels;
    unsigned char *palette;         /* loaded palette data */
    int width;                      /* frame width */
    int height;                     /* frame height */
    int ncolors;                    /* palette colors */
    int pixelformat;                /* one of enum pixelFormat */
    int colorspace;                 /* one of SIXEL_COLORSPACE_* */
    /*
     * Timeline metadata can be updated by the loader while worker threads
     * sample or quantize shared frames. Keep these fields atomic so that
     * by-ref handoff mode remains race-free under sanitizers.
     */
    sixel_atomic_i32_t delay;       /* delay in msec */
    sixel_atomic_i32_t frame_no;    /* frame number */
    sixel_atomic_i32_t loop_count;  /* loop count */
    sixel_atomic_i32_t multiframe;  /* frame belongs to animation sequence */
    int handoff_shareable;          /* safe to pass by ref in handoff queue */
    int transparent;                /* -1 none, >=0 transparent palette index */
    int alpha_zero_is_transparent;  /* alpha=0 pixels are keycolor candidates */
    unsigned char *transparent_mask; /* per-pixel transparency mask */
    size_t transparent_mask_size;   /* entry count for transparent_mask */
    sixel_allocator_t *allocator;   /* allocator object */
};

#endif /* LIBSIXEL_FRAME_PRIVATE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
