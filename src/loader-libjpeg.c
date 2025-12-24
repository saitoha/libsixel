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
 * libjpeg-backed loader helpers extracted from loader.c to prepare for
 * backend-specific translation units. The functions here remain thin wrappers
 * around the shared callbacks and frame utilities so the registration table
 * can reference them without pulling libjpeg headers into unrelated code.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#if HAVE_JPEG

#include <stdio.h>
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#include <jpeglib.h>

#include <sixel.h>

#include "allocator.h"
#include "chunk.h"
#include "loader-common.h"
#include "frame.h"
#include "loader-libjpeg.h"
#include "logger.h"

/*
 * import from @uobikiemukot's sdump loader.h
 *
 * The helper keeps libjpeg-specific state localized so only this file needs
 * to include jpeglib.h. Callers receive raw RGB buffers and metadata filled
 * through the OUT parameters.
 */
static SIXELSTATUS
load_jpeg(unsigned char **result,
          unsigned char *data,
          size_t datasize,
          int *pwidth,
          int *pheight,
          int *ppixelformat,
          sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    JDIMENSION row_stride;
    size_t size;
    JSAMPARRAY buffer;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr pub;

    status = SIXEL_JPEG_ERROR;
    cinfo.err = jpeg_std_error(&pub);

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, datasize);
    jpeg_read_header(&cinfo, TRUE);

    /* disable colormap (indexed color), grayscale -> rgb */
    cinfo.quantize_colors = FALSE;
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    if (cinfo.output_components != 3) {
        sixel_helper_set_additional_message(
            "load_jpeg: unknown pixel format.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;

    if (cinfo.output_width > INT_MAX || cinfo.output_height > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    *pwidth = (int)cinfo.output_width;
    *pheight = (int)cinfo.output_height;

    size = (size_t)(*pwidth * *pheight * cinfo.output_components);
    *result = (unsigned char *)
        sixel_allocator_malloc(allocator, size);
    if (*result == NULL) {
        sixel_helper_set_additional_message(
            "load_jpeg: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    row_stride = cinfo.output_width * (unsigned int)cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo,
                                        JPOOL_IMAGE,
                                        row_stride,
                                        1);

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        if (cinfo.err->num_warnings > 0) {
            sixel_helper_set_additional_message(
                "jpeg_read_scanlines: error/warining occuered.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        memcpy(*result + (cinfo.output_scanline - 1) * row_stride,
               buffer[0],
               row_stride);
    }

    status = SIXEL_OK;

end:
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return status;
}

/*
 * Dedicated libjpeg loader wiring minimal pipeline.
 *
 *    +------------+     +-------------------+     +--------------------+
 *    | JPEG chunk | --> | libjpeg decode    | --> | sixel frame emit   |
 *    +------------+     +-------------------+     +--------------------+
 */
SIXELSTATUS
load_with_libjpeg(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char *pixels;

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;

    (void)fstatic;
    (void)fuse_palette;
    (void)reqcolors;
    (void)loop_control;

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = load_jpeg(&pixels,
                       pchunk->buffer,
                       pchunk->size,
                       &frame->width,
                       &frame->height,
                       &frame->pixelformat,
                       pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_frame_set_pixels(frame, pixels);

    status = sixel_frame_strip_alpha(frame, bgcolor);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);

    return status;
}

int
loader_can_try_libjpeg(sixel_chunk_t const *chunk)
{
    if (chunk == NULL) {
        return 0;
    }

    return chunk_is_jpeg(chunk);
}

#else  /* !HAVE_JPEG */

/*
 * Keep a harmless placeholder around so pedantic builds skip the empty unit
 * warning when libjpeg is not part of the build.
 */
enum { sixel_loader_libjpeg_placeholder = 0 };

#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_LIBJPEG_PLACEHOLDER_UNUSED __attribute__((unused))
#else
# define SIXEL_LIBJPEG_PLACEHOLDER_UNUSED
#endif

static void
sixel_loader_libjpeg_placeholder_function(void)
    SIXEL_LIBJPEG_PLACEHOLDER_UNUSED;

static void
sixel_loader_libjpeg_placeholder_function(void)
{
    /*
     * The placeholder ties the enum to a symbol so MSVC does not warn about
     * an empty translation unit when libjpeg support is disabled.
     */
    (void)sixel_loader_libjpeg_placeholder;
}

#undef SIXEL_LIBJPEG_PLACEHOLDER_UNUSED

#endif  /* HAVE_JPEG */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
