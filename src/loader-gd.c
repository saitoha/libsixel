/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
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
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * libgd-backed loader extracted from loader.c to isolate optional dependencies
 * and keep the central registry lightweight.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_GD

#include <stdio.h>

#if HAVE_STRING_H
# include <string.h>
#endif

#include <gd.h>

#include <sixel.h>

#include "chunk.h"
#include "frame.h"
#include "loader-common.h"
#include "loader-gd.h"

SIXELSTATUS
load_with_gd(
    sixel_chunk_t const       /* in */     *pchunk,     /* image data */
    int                       /* in */     fstatic,     /* static */
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,    /* background */
                                                 /* color */
    int                       /* in */     loop_control,
    sixel_load_image_function /* in */     fn_load,     /* callback */
    void                      /* in/out */ *context     /* private */
                                                 /* data for callback */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;
    gdImagePtr im;
    unsigned char *p;
    int y;
    int x;
    int c;
    int gif;
    int bmp;
    int tiff;
    int *truecolor_row;
    unsigned char *palette_row;

    (void) fstatic;
    (void) fuse_palette;
    (void) reqcolors;
    (void) bgcolor;
    (void) loop_control;

    frame = NULL;
    im = NULL;
    p = NULL;
    gif = gdSupportsFileType(".gif", 0);
    bmp = gdSupportsFileType(".bmp", 0);
    tiff = gdSupportsFileType(".tiff", 0);

    if (gif && chunk_is_gif(pchunk)) {
        im = gdImageCreateFromGifPtr((int)pchunk->size, pchunk->buffer);
    }

    if (im == NULL && chunk_is_png(pchunk)) {
        im = gdImageCreateFromPngPtr((int)pchunk->size, pchunk->buffer);
    }

    if (im == NULL && chunk_is_jpeg(pchunk)) {
        im = gdImageCreateFromJpegPtr((int)pchunk->size, pchunk->buffer);
    }

    if (im == NULL && bmp) {
        im = gdImageCreateFromBmpPtr((int)pchunk->size, pchunk->buffer);
    }

    if (im == NULL && chunk_is_bmp(pchunk)) {
        im = gdImageCreateFromBmpPtr((int)pchunk->size, pchunk->buffer);
    }

    if (im == NULL && tiff) {
        im = gdImageCreateFromTiffPtr((int)pchunk->size, pchunk->buffer);
    }

    if (im == NULL) {
        /*
         * GD could not decode the input. Signal a backend-specific error so
         * the caller can report that GD rejected the buffer after sniffing.
         */
        status = SIXEL_GD_ERROR;
        goto end;
    }

    if (gdImageSX(im) <= 0 || gdImageSY(im) <= 0) {
        /*
         * GD returned a stub image without valid dimensions. Prevent
         * downstream allocations when the frame size is not usable.
         */
        status = SIXEL_GD_ERROR;
        gdImageDestroy(im);
        goto end;
    }

    if (im->trueColor) {
        if (im->tpixels == NULL) {
            /*
             * Some malformed inputs make GD allocate an image shell without
             * tpixels. gdImageTrueColorPixel() would dereference this field,
             * so abort loading before accessing it.
             */
            status = SIXEL_GD_ERROR;
            gdImageDestroy(im);
            goto end;
        }

        for (y = 0; y < gdImageSY(im); y++) {
            truecolor_row = im->tpixels[y];
            if (truecolor_row == NULL) {
                /*
                 * GD sometimes allocates the tpixels array but leaves one or
                 * more row pointers unset. The encoder would crash on such a
                 * row, so reject the image before iterating over its pixels.
                 */
                status = SIXEL_GD_ERROR;
                gdImageDestroy(im);
                goto end;
            }
        }
    } else {
        if (im->pixels == NULL) {
            /*
             * Paletted images also rely on a populated pixel buffer. Reject
             * frames that omit it to avoid null dereferences when accessing
             * palette entries.
             */
            status = SIXEL_GD_ERROR;
            gdImageDestroy(im);
            goto end;
        }

        for (y = 0; y < gdImageSY(im); y++) {
            palette_row = im->pixels[y];
            if (palette_row == NULL) {
                /*
                 * gdImageGetPixel() reads the row pointer directly. Guard
                 * against partially initialised rows before touching them.
                 */
                status = SIXEL_GD_ERROR;
                gdImageDestroy(im);
                goto end;
            }
        }

        status = SIXEL_GD_ERROR;
        gdImageDestroy(im);
        goto end;
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        gdImageDestroy(im);
        goto end;
    }

    frame->width = gdImageSX(im);
    frame->height = gdImageSY(im);
    frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    sixel_frame_set_pixels(frame,
                           sixel_allocator_malloc(
                               pchunk->allocator,
                               (size_t)(frame->width * frame->height * 3)));
    p = sixel_frame_get_pixels(frame);
    if (p == NULL) {
        sixel_helper_set_additional_message(
            "load_with_gd: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        gdImageDestroy(im);
        goto end;
    }
    for (y = 0; y < frame->height; y++) {
        for (x = 0; x < frame->width; x++) {
            c = gdImageTrueColorPixel(im, x, y);
            *p++ = gdTrueColorGetRed(c);
            *p++ = gdTrueColorGetGreen(c);
            *p++ = gdTrueColorGetBlue(c);
        }
    }
    gdImageDestroy(im);

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_frame_unref(frame);

    status = SIXEL_OK;

end:
    return status;
}

#endif  /* HAVE_GD */

#if !HAVE_GD
/*
 * Avoid empty translation unit warnings when GD support is disabled.
 */
typedef int loader_gd_disabled;
#endif

