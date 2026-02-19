/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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
 *
 * libwebp-backed loader helpers extracted from loader.c to keep WebP decoding
 * isolated from unrelated translation units.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_WEBP

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
#include <webp/decode.h>
#include <webp/demux.h>

#include <sixel.h>

#include "allocator.h"
#include "chunk.h"
#include "frame.h"
#include "loader-common.h"
#include "loader-libwebp.h"
#include "logger.h"

/*
 * Decode a WebP buffer into an RGB(A) pixel buffer managed by libsixel.
 *
 * The steps are:
 *   1) Probe the WebP bitstream for dimensions and alpha flags.
 *   2) Allocate the output buffer from the sixel allocator.
 *   3) Decode into RGB or RGBA depending on the alpha information.
 */
static SIXELSTATUS
load_webp(unsigned char **result,
          unsigned char *data,
          size_t datasize,
          int *pwidth,
          int *pheight,
          int *ppixelformat,
          sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    WebPBitstreamFeatures features;
    int bytes_per_pixel;
    size_t stride;
    size_t size;

    status = SIXEL_BAD_INPUT;

    if (WebPGetFeatures(data, datasize, &features) != VP8_STATUS_OK) {
        sixel_helper_set_additional_message(
            "load_webp: WebPGetFeatures failed.");
        return status;
    }

    if (features.width <= 0 || features.height <= 0) {
        sixel_helper_set_additional_message(
            "load_webp: invalid image dimensions.");
        return status;
    }

    if (features.width > INT_MAX || features.height > INT_MAX) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    *pwidth = features.width;
    *pheight = features.height;

    bytes_per_pixel = features.has_alpha ? 4 : 3;
    *ppixelformat = features.has_alpha ?
        SIXEL_PIXELFORMAT_RGBA8888 : SIXEL_PIXELFORMAT_RGB888;

    if ((size_t)*pwidth > SIZE_MAX / (size_t)bytes_per_pixel) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    stride = (size_t)*pwidth * (size_t)bytes_per_pixel;
    if ((size_t)*pheight > 0 && stride > SIZE_MAX / (size_t)*pheight) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    size = stride * (size_t)*pheight;
    *result = (unsigned char *)sixel_allocator_malloc(allocator, size);
    if (*result == NULL) {
        sixel_helper_set_additional_message(
            "load_webp: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (features.has_alpha) {
        if (WebPDecodeRGBAInto(data, datasize, *result, size,
                               (int)stride) == NULL) {
            sixel_helper_set_additional_message(
                "load_webp: WebPDecodeRGBAInto failed.");
            return SIXEL_BAD_INPUT;
        }
    } else {
        if (WebPDecodeRGBInto(data, datasize, *result, size,
                              (int)stride) == NULL) {
            sixel_helper_set_additional_message(
                "load_webp: WebPDecodeRGBInto failed.");
            return SIXEL_BAD_INPUT;
        }
    }

    status = SIXEL_OK;

    return status;
}

/*
 * Dedicated libwebp loader wiring minimal pipeline.
 *
 *    +------------+     +-------------------+     +--------------------+
 *    | WebP chunk | --> | libwebp decode    | --> | sixel frame emit   |
 *    +------------+     +-------------------+     +--------------------+
 */
SIXELSTATUS
load_with_libwebp(
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
    WebPData webp_data;
    WebPAnimDecoderOptions decoder_options;
    WebPAnimDecoder *decoder;
    WebPAnimInfo anim_info;
    uint8_t *decoded_frame;
    int timestamp;
    int previous_timestamp;
    size_t frame_bytes;
    int next_delay;

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;
    webp_data = (WebPData){ 0 };
    decoder = NULL;
    anim_info = (WebPAnimInfo){ 0 };
    decoded_frame = NULL;
    timestamp = 0;
    previous_timestamp = 0;
    frame_bytes = 0;
    next_delay = 0;

    (void)fuse_palette;
    (void)reqcolors;

    webp_data.bytes = pchunk->buffer;
    webp_data.size = pchunk->size;

    if (!WebPAnimDecoderOptionsInit(&decoder_options)) {
        sixel_helper_set_additional_message(
            "load_with_libwebp: WebPAnimDecoderOptionsInit failed.");
        status = SIXEL_WEBP_ERROR;
        goto end;
    }
    decoder_options.color_mode = MODE_RGBA;
    decoder = WebPAnimDecoderNew(&webp_data, &decoder_options);
    if (decoder == NULL) {
        sixel_helper_set_additional_message(
            "load_with_libwebp: WebPAnimDecoderNew failed.");
        status = SIXEL_WEBP_ERROR;
        goto end;
    }

    if (!WebPAnimDecoderGetInfo(decoder, &anim_info)) {
        sixel_helper_set_additional_message(
            "load_with_libwebp: WebPAnimDecoderGetInfo failed.");
        status = SIXEL_WEBP_ERROR;
        goto end;
    }

    if (anim_info.frame_count <= 1) {
        WebPAnimDecoderDelete(decoder);
        decoder = NULL;

        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = load_webp(&pixels,
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
        goto end;
    }

    if (fstatic) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        if (!WebPAnimDecoderHasMoreFrames(decoder)) {
            sixel_helper_set_additional_message(
                "load_with_libwebp: no frames in animated WebP stream.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (!WebPAnimDecoderGetNext(decoder, &decoded_frame, &timestamp)) {
            sixel_helper_set_additional_message(
                "load_with_libwebp: WebPAnimDecoderGetNext failed.");
            status = SIXEL_WEBP_ERROR;
            goto end;
        }

        frame->width = (int)anim_info.canvas_width;
        frame->height = (int)anim_info.canvas_height;
        frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        frame->multiframe = 0;
        frame->loop_count = 0;
        frame->frame_no = 0;
        frame->delay = timestamp / 10;

        if (frame->width <= 0 || frame->height <= 0) {
            sixel_helper_set_additional_message(
                "load_with_libwebp: invalid canvas dimensions.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if ((size_t)frame->width > SIZE_MAX / 4 ||
            (size_t)frame->height > SIZE_MAX / ((size_t)frame->width * 4)) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        frame_bytes = (size_t)frame->width * (size_t)frame->height * 4;

        pixels = (unsigned char *)sixel_allocator_malloc(pchunk->allocator,
                                                         frame_bytes);
        if (pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_with_libwebp: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        memcpy(pixels, decoded_frame, frame_bytes);
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
        goto end;
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    /*
     * Decode WebP animation as fully composited RGBA canvases.
     *
     *   outer loop : logical animation loop
     *   inner loop : frame traversal inside a single loop
     */
    frame->width = (int)anim_info.canvas_width;
    frame->height = (int)anim_info.canvas_height;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    frame->colorspace = SIXEL_COLORSPACE_GAMMA;
    frame->multiframe = 1;
    frame->loop_count = 0;

    if (frame->width <= 0 || frame->height <= 0) {
        sixel_helper_set_additional_message(
            "load_with_libwebp: invalid canvas dimensions.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if ((size_t)frame->width > SIZE_MAX / 4 ||
        (size_t)frame->height > SIZE_MAX / ((size_t)frame->width * 4)) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }
    frame_bytes = (size_t)frame->width * (size_t)frame->height * 4;

    for (;;) {
        frame->frame_no = 0;
        previous_timestamp = 0;

        while (WebPAnimDecoderHasMoreFrames(decoder)) {
            if (!WebPAnimDecoderGetNext(decoder,
                                        &decoded_frame,
                                        &timestamp)) {
                sixel_helper_set_additional_message(
                    "load_with_libwebp: WebPAnimDecoderGetNext failed.");
                status = SIXEL_WEBP_ERROR;
                goto end;
            }

            pixels = (unsigned char *)sixel_allocator_malloc(
                pchunk->allocator,
                frame_bytes);
            if (pixels == NULL) {
                sixel_helper_set_additional_message(
                    "load_with_libwebp: sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }

            memcpy(pixels, decoded_frame, frame_bytes);
            sixel_frame_set_pixels(frame, pixels);

            next_delay = timestamp - previous_timestamp;
            if (next_delay < 0) {
                next_delay = 0;
            }
            frame->delay = next_delay / 10;

            status = sixel_frame_strip_alpha(frame, bgcolor);
            if (SIXEL_FAILED(status)) {
                goto end;
            }

            status = fn_load(frame, context);
            if (SIXEL_FAILED(status)) {
                goto end;
            }

            pixels = sixel_frame_get_pixels(frame);
            if (pixels != NULL) {
                sixel_allocator_free(pchunk->allocator, pixels);
                sixel_frame_set_pixels(frame, NULL);
                pixels = NULL;
            }

            previous_timestamp = timestamp;
            frame->frame_no++;
        }

        frame->loop_count++;

        if (loop_control == SIXEL_LOOP_DISABLE || frame->frame_no == 1) {
            break;
        }
        if (loop_control == SIXEL_LOOP_AUTO &&
            anim_info.loop_count > 0 &&
            (unsigned int)frame->loop_count >= anim_info.loop_count) {
            break;
        }

        WebPAnimDecoderReset(decoder);
    }

    status = SIXEL_OK;

end:
    if (decoder != NULL) {
        WebPAnimDecoderDelete(decoder);
    }
    sixel_frame_unref(frame);

    return status;
}

int
loader_can_try_libwebp(sixel_chunk_t const *chunk)
{
    if (chunk == NULL) {
        return 0;
    }

    return chunk_is_webp(chunk);
}

#else  /* !HAVE_WEBP */

/*
 * Keep a harmless placeholder around so pedantic builds skip the empty unit
 * warning when libwebp is not part of the build.
 */
enum { sixel_loader_libwebp_placeholder = 0 };

#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_LIBWEBP_PLACEHOLDER_UNUSED __attribute__((unused))
#else
# define SIXEL_LIBWEBP_PLACEHOLDER_UNUSED
#endif

static void
sixel_loader_libwebp_placeholder_function(void)
    SIXEL_LIBWEBP_PLACEHOLDER_UNUSED;

static void
sixel_loader_libwebp_placeholder_function(void)
{
    /*
     * The placeholder ties the enum to a symbol so MSVC does not warn about
     * an empty translation unit when libwebp support is disabled.
     */
    (void)sixel_loader_libwebp_placeholder;
}

#undef SIXEL_LIBWEBP_PLACEHOLDER_UNUSED

#endif  /* HAVE_WEBP */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
