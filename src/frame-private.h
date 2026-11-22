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

/* Byte layouts use their legacy packing codes. All float triplets share
 * RGBFLOAT32 storage; coordinates alone distinguish RGB, Lab and opponents.
 * An explicit RGB definition is owned by the frame and takes precedence
 * over the default RGB primaries and transfer. */
struct sixel_frame_pixel_format {
    int layout;
    int coordinates;
    struct sixel_color_def *rgb;
};

/* Private frame object storage. Keep src/frame.h storage-opaque. */
struct sixel_frame {
    sixel_frame_interface_t frame_interface; /* IFrame dispatch header */
    sixel_atomic_u32_t ref;         /* reference counter */
    union {
        unsigned char *u8ptr;       /* loaded pixel data (byte) */
        float *f32ptr;              /* loaded pixel data (float32) */
    } pixels;
    /* In-place crops keep backing storage beyond the logical rectangle.
     * Zero means the current layout describes the entire plane. Coverage
     * is either a binary mask or a straight alpha plane. */
    size_t pixel_extent;            /* retained pixel bytes */
    size_t coverage_extent;         /* retained coverage bytes */
    unsigned char *palette;         /* loaded palette data */
    int owns_pixels;                /* frame releases pixel storage */
    int owns_palette;               /* frame releases palette storage */
    int width;                      /* frame width */
    int height;                     /* frame height */
    int palette_extent;             /* extra addressable raw SIXEL slots */
    int ncolors;                    /* palette colors */
    struct sixel_frame_pixel_format format;
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
    float *alpha;                  /* owned straight fractional coverage */
    size_t alpha_count;            /* one finite [0,1] sample per pixel */
    sixel_allocator_t *allocator;   /* allocator object */
};

#ifdef __cplusplus
extern "C" {
#endif

/* Project the storage and coordinates to a historical API format. */
SIXEL_INTERNAL_API int
sixel_frame_format(sixel_frame_t const *frame);

SIXEL_INTERNAL_API void
sixel_frame_assign_format(sixel_frame_t *frame, int pixelformat);

/* In-place RGBA compaction shortens each pixel but retains the same
 * allocation. A previous crop may already have retained more bytes. */
static inline void
sixel_frame_finish_rgba_compaction(sixel_frame_t *frame)
{
    size_t bytes = (size_t)frame->width * (size_t)frame->height * 4u;

    if (frame->pixel_extent < bytes) {
        frame->pixel_extent = bytes;
    }
    /* Removing alpha changes the stride, not the RGB coordinates. */
    frame->format.layout = SIXEL_PIXELFORMAT_RGB888;
}

/* Resolve straight linear RGB plus continuous alpha at a delivery boundary.
 * A NULL background retains RGB and only marks exact alpha-zero samples.
 * Failure leaves all frame storage and metadata unchanged. */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_frame_resolve_alpha(sixel_frame_t *frame,
                          float const *background, int preserve_zero);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_FRAME_PRIVATE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
