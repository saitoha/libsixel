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

#include <stddef.h>

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IDL (internal contract)
 *
 * interface IFrame {
 *   ref();
 *   unref();
 *   init_pixels(request);
 *   get_pixels(view);
 *   set_pixelformat(pixelformat);
 *   get_timeline(timeline);
 *   set_timeline(timeline);
 *   get_transparency(transparency);
 *   set_transparency(transparency);
 *   strip_alpha(bgcolor);
 *   resize(width, height, method);
 *   resize_float32(width, height, method);
 *   clip(x, y, width, height);
 *   allocator();
 * }
 *
 * Ownership/lifetime:
 * - Existing public sixel_frame_new() returns refcount=1 frame objects.
 * - init_pixels() takes ownership of request->pixels and request->palette,
 *   matching sixel_frame_init().
 * - set_transparency() takes ownership of request->transparent_mask.
 *
 * Encapsulation path:
 * - src/frame-private.h owns the concrete storage layout.
 * - src/frame.h exposes only the object contract and request/view DTOs.
 * - Existing direct-field callers should migrate toward this interface before
 *   they drop their temporary private-header dependency.
 */

typedef struct sixel_frame_interface sixel_frame_interface_t;

typedef enum sixel_frame_pixels_kind {
    SIXEL_FRAME_PIXELS_U8 = 0,
    SIXEL_FRAME_PIXELS_FLOAT32 = 1
} sixel_frame_pixels_kind_t;

typedef struct sixel_frame_pixels_request {
    void *pixels;
    unsigned char *palette;
    int width;
    int height;
    int pixelformat;
    int colorspace;                 /* negative means infer from pixelformat */
    int ncolors;
    sixel_frame_pixels_kind_t kind;
} sixel_frame_pixels_request_t;

typedef struct sixel_frame_pixels_view {
    unsigned char *pixels;
    float *pixels_float32;
    unsigned char *palette;
    int width;
    int height;
    int pixelformat;
    int colorspace;
    int ncolors;
} sixel_frame_pixels_view_t;

typedef struct sixel_frame_timeline {
    int delay;
    int frame_no;
    int loop_count;
    int multiframe;
    int handoff_shareable;
} sixel_frame_timeline_t;

typedef struct sixel_frame_transparency {
    int transparent;
    int alpha_zero_is_transparent;
    unsigned char *transparent_mask;
    size_t transparent_mask_size;
} sixel_frame_transparency_t;

typedef struct sixel_frame_vtbl {
    void (*ref)(sixel_frame_interface_t *frame);
    void (*unref)(sixel_frame_interface_t *frame);
    SIXELSTATUS (*init_pixels)(
        sixel_frame_interface_t *frame,
        sixel_frame_pixels_request_t const *request);
    SIXELSTATUS (*get_pixels)(
        sixel_frame_interface_t *frame,
        sixel_frame_pixels_view_t *view);
    SIXELSTATUS (*set_pixelformat)(sixel_frame_interface_t *frame,
                                   int pixelformat);
    SIXELSTATUS (*get_timeline)(sixel_frame_interface_t *frame,
                                sixel_frame_timeline_t *timeline);
    SIXELSTATUS (*set_timeline)(
        sixel_frame_interface_t *frame,
        sixel_frame_timeline_t const *timeline);
    SIXELSTATUS (*get_transparency)(
        sixel_frame_interface_t *frame,
        sixel_frame_transparency_t *transparency);
    SIXELSTATUS (*set_transparency)(
        sixel_frame_interface_t *frame,
        sixel_frame_transparency_t const *transparency);
    SIXELSTATUS (*strip_alpha)(sixel_frame_interface_t *frame,
                               unsigned char *bgcolor);
    SIXELSTATUS (*resize)(sixel_frame_interface_t *frame,
                          int width,
                          int height,
                          int method_for_resampling);
    SIXELSTATUS (*resize_float32)(sixel_frame_interface_t *frame,
                                  int width,
                                  int height,
                                  int method_for_resampling);
    SIXELSTATUS (*clip)(sixel_frame_interface_t *frame,
                        int x,
                        int y,
                        int width,
                        int height);
    sixel_allocator_t *(*allocator)(sixel_frame_interface_t *frame);
} sixel_frame_vtbl_t;

struct sixel_frame_interface {
    sixel_frame_vtbl_t const *vtbl;
};

SIXEL_INTERNAL_API sixel_frame_interface_t *
sixel_frame_get_interface(sixel_frame_t *frame);

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
