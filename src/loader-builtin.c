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
 */

/* Builtin loader covering SIXEL, PNM, GIF, and stb_image fallbacks.  This
 * module keeps the heavyweight stb_image implementation isolated from the
 * registry so other backends avoid pulling in its macros and includes.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_CTYPE_H
# include <ctype.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif

#include <sixel.h>

#include "allocator.h"
#include "chunk.h"
#include "frame.h"
#include "fromgif.h"
#include "frompnm.h"
#include "pixelformat.h"
#include "loader-builtin.h"
#include "loader-common.h"
#include "loader.h"

static sixel_allocator_t *stbi_allocator;

void *
stbi_malloc(size_t n)
{
    return sixel_allocator_malloc(stbi_allocator, n);
}

void *
stbi_realloc(void *p, size_t n)
{
    return sixel_allocator_realloc(stbi_allocator, p, n);
}

void
stbi_free(void *p)
{
    sixel_allocator_free(stbi_allocator, p);
}

#define STBI_MALLOC stbi_malloc
#define STBI_REALLOC stbi_realloc
#define STBI_FREE stbi_free

#define STBI_NO_STDIO 1
#define STB_IMAGE_IMPLEMENTATION 1
#define STBI_FAILURE_USERMSG 1
#if defined(_WIN32)
# define STBI_NO_THREAD_LOCALS 1  /* no tls */
#endif
#define STBI_NO_GIF
#define STBI_NO_PNM
#if !defined(HAVE_EMMINTRIN_H)
# define STBI_NO_SIMD
#endif

#if HAVE_DIAGNOSTIC_SIGN_CONVERSION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsign-conversion"
# endif
#endif
#if HAVE_DIAGNOSTIC_STRICT_OVERFLOW
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wstrict-overflow"
# endif
#endif
#if HAVE_DIAGNOSTIC_SWITCH_DEFAULT
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wswitch-default"
# endif
#endif
#if HAVE_DIAGNOSTIC_SHADOW
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
# endif
#endif
#if HAVE_DIAGNOSTIC_DOUBLE_PROMOTION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdouble-promotion"
# endif
#endif
#if HAVE_DIAGNOSTIC_UNUSED_FUNCTION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-function"
# endif
#endif
#if HAVE_DIAGNOSTIC_UNUSED_BUT_SET_VARIABLE
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-but-set-variable"
# endif
#endif
#include "stb_image.h"
#if HAVE_DIAGNOSTIC_UNUSED_BUT_SET_VARIABLE
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
#if HAVE_DIAGNOSTIC_UNUSED_FUNCTION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
#if HAVE_DIAGNOSTIC_DOUBLE_PROMOTION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
#if HAVE_DIAGNOSTIC_SHADOW
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
#if HAVE_DIAGNOSTIC_SWITCH_DEFAULT
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
#if HAVE_DIAGNOSTIC_STRICT_OVERFLOW
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
#if HAVE_DIAGNOSTIC_SIGN_CONVERSION
# if defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif

static SIXELSTATUS
load_sixel(unsigned char        /* out */ **result,
           unsigned char        /* in */  *buffer,
           int                  /* in */  size,
           int                  /* out */ *psx,
           int                  /* out */ *psy,
           unsigned char        /* out */ **ppalette,
           int                  /* out */ *pncolors,
           int                  /* in */  reqcolors,
           int                  /* out */ *ppixelformat,
           sixel_allocator_t    /* in */  *allocator)
{
    SIXELSTATUS status;
    unsigned char *decoded_pixels;
    unsigned char *decoded_palette;
    size_t image_bytes;

    if (result == NULL || pncolors == NULL || ppixelformat == NULL ||
            allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = SIXEL_FALSE;
    decoded_pixels = NULL;
    decoded_palette = NULL;
    image_bytes = 0;

    status = sixel_decode_raw(buffer,
                              size,
                              &decoded_pixels,
                              psx,
                              psy,
                              &decoded_palette,
                              pncolors,
                              allocator);
    if (loader_trace_is_enabled()) {
        loader_trace_message("load_sixel: sixel_decode_raw -> %d",
                             (int)status);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    image_bytes = (size_t)(*psx) * (size_t)(*psy);

    if (ppalette == NULL ||
            (reqcolors > 0 && *pncolors > reqcolors)) {
        size_t rgb_bytes;
        size_t index;

        if (decoded_palette == NULL) {
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        rgb_bytes = image_bytes * 3u;
        *result = sixel_allocator_malloc(allocator, rgb_bytes);
        if (*result == NULL) {
            sixel_helper_set_additional_message(
                "load_sixel: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (index = 0; index < image_bytes; ++index) {
            size_t palette_index;
            size_t pixel_offset;

            palette_index = decoded_pixels[index] * 3u;
            pixel_offset = index * 3u;
            (*result)[pixel_offset + 0] = decoded_palette[palette_index + 0];
            (*result)[pixel_offset + 1] = decoded_palette[palette_index + 1];
            (*result)[pixel_offset + 2] = decoded_palette[palette_index + 2];
        }
        *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    } else {
        *result = decoded_pixels;
        decoded_pixels = NULL;
        *ppalette = decoded_palette;
        decoded_palette = NULL;
        *ppixelformat = SIXEL_PIXELFORMAT_PAL8;
    }
    status = SIXEL_OK;

end:
    if (decoded_pixels != NULL) {
        sixel_allocator_free(allocator, decoded_pixels);
        decoded_pixels = NULL;
    }
    if (decoded_palette != NULL) {
        sixel_allocator_free(allocator, decoded_palette);
        decoded_palette = NULL;
    }

    return status;
}

static int
chunk_is_sixel(sixel_chunk_t const *chunk)
{
    unsigned char *p;
    unsigned char *end;

    p = chunk->buffer;
    end = p + chunk->size;

    if (chunk->size < 3) {
        return 0;
    }

    p++;
    if (p >= end) {
        return 0;
    }
    if (*(p - 1) == 0x90 || (*(p - 1) == 0x1b && *p == 0x50)) {
        while (p++ < end) {
            if (*p == 0x71) {
                return 1;
            } else if (*p == 0x18 || *p == 0x1a) {
                return 0;
            } else if (*p < 0x20) {
                continue;
            } else if (*p < 0x30) {
                return 0;
            } else if (*p < 0x40) {
                continue;
            }
        }
    }
    return 0;
}

static int
chunk_is_pnm(sixel_chunk_t const *chunk)
{
    if (chunk->size < 2) {
        return 0;
    }
    if (chunk->buffer[0] == 'P' &&
        chunk->buffer[1] >= '1' &&
        chunk->buffer[1] <= '6') {
        return 1;
    }
    return 0;
}

typedef union _fn_pointer {
    sixel_load_image_function fn;
    void *                    p;
} fn_pointer;

SIXELSTATUS
load_with_builtin(
    sixel_chunk_t const *pchunk,
    int fstatic,
    int fuse_palette,
    int reqcolors,
    unsigned char *bgcolor,
    int loop_control,
    sixel_load_image_function fn_load,
    void *context)
{
    SIXELSTATUS status;
    unsigned char *pixels;
    int depth;
    sixel_frame_t *frame;
    fn_pointer fnp;
    stbi__context stb_context;
    char message[80];
    int nwrite;

    status = SIXEL_BAD_INPUT;
    pixels = NULL;
    depth = 0;
    frame = NULL;
    fnp.fn = NULL;
    stb_context = (stbi__context){ 0 };
    nwrite = 0;

    if (pchunk == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (chunk_is_sixel(pchunk)) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        status = load_sixel(&pixels,
                            pchunk->buffer,
                            (int)pchunk->size,
                            &frame->width,
                            &frame->height,
                            fuse_palette ? &frame->palette: NULL,
                            &frame->ncolors,
                            reqcolors,
                            &frame->pixelformat,
                            pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_frame_set_pixels(frame, pixels);
    } else if (chunk_is_pnm(pchunk)) {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        status = load_pnm(pchunk->buffer,
                          (int)pchunk->size,
                          frame->allocator,
                          &pixels,
                          &frame->width,
                          &frame->height,
                          fuse_palette ? &frame->palette: NULL,
                          &frame->ncolors,
                          &frame->pixelformat);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_frame_set_pixels(frame, pixels);
    } else if (chunk_is_gif(pchunk)) {
        fnp.fn = fn_load;
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        status = load_gif(pchunk->buffer,
                          (int)pchunk->size,
                          bgcolor,
                          reqcolors,
                          fuse_palette,
                          fstatic,
                          loop_control,
                          fnp.p,
                          context,
                          pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        goto end;
    } else {
        status = sixel_frame_new(&frame, pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (pchunk->size > INT_MAX) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto end;
        }
        stbi_allocator = pchunk->allocator;
        stbi__start_mem(&stb_context,
                        pchunk->buffer,
                        (int)pchunk->size);
        pixels = stbi__load_and_postprocess_8bit(&stb_context,
                                                 &frame->width,
                                                 &frame->height,
                                                 &depth,
                                                 3);
        if (pixels == NULL) {
            sixel_helper_set_additional_message(stbi_failure_reason());
            status = SIXEL_STBI_ERROR;
            goto end;
        }
        sixel_frame_set_pixels(frame, pixels);
        frame->loop_count = 1;
        switch (depth) {
        case 1:
        case 3:
        case 4:
            frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
            break;
        default:
            nwrite = snprintf(message,
                              sizeof(message),
                              "load_with_builtin() failed.\n"
                              "reason: unknown pixel-format.(depth: %d)\n",
                              depth);
            if (nwrite > 0) {
                sixel_helper_set_additional_message(message);
            }
            status = SIXEL_STBI_ERROR;
            goto end;
        }
    }

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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
