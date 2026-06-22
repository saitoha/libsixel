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
/*
 * Shared SIXEL memory decoding helper for packed pixel output.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stddef.h>
#include <string.h>
#if HAVE_STDINT_H
# include <stdint.h>
#endif  /* HAVE_STDINT_H */

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif  /* SIZE_MAX */

#include <sixel.h>

#include "sixel_decode_pixels.h"

#define SIXEL_DECODE_MAX_SIZE 6000

static int
sixel_decode_pixels_depth(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
    case SIXEL_PIXELFORMAT_BGR888:
        return 3;
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
    case SIXEL_PIXELFORMAT_XRGB8888:
    case SIXEL_PIXELFORMAT_RGBX8888:
    case SIXEL_PIXELFORMAT_XBGR8888:
    case SIXEL_PIXELFORMAT_BGRX8888:
        return 4;
    default:
        break;
    }

    return 0;
}

static int
sixel_decode_pixels_format_has_alpha(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGBA8888:
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_BGRA8888:
    case SIXEL_PIXELFORMAT_ABGR8888:
        return 1;
    default:
        break;
    }

    return 0;
}

static int
sixel_decode_pixels_alpha_is_opaque(unsigned char const *rgba,
                                    size_t total)
{
    size_t i;

    for (i = 0U; i < total; ++i) {
        if (rgba[i * 4U + 3U] != 0xffU) {
            return 0;
        }
    }

    return 1;
}

static unsigned char
sixel_decode_pixels_blend(unsigned char src,
                          unsigned char bg,
                          unsigned int alpha)
{
    return (unsigned char)(((unsigned int)src * alpha +
                            (unsigned int)bg * (255U - alpha)) / 255U);
}

static void
sixel_decode_pixels_store(unsigned char *dst,
                          unsigned char const *src,
                          int pixelformat,
                          unsigned char const *bg)
{
    unsigned int alpha;
    unsigned char r;
    unsigned char g;
    unsigned char b;

    alpha = src[3];
    if (!sixel_decode_pixels_format_has_alpha(pixelformat)) {
        r = sixel_decode_pixels_blend(src[0], bg[0], alpha);
        g = sixel_decode_pixels_blend(src[1], bg[1], alpha);
        b = sixel_decode_pixels_blend(src[2], bg[2], alpha);
    } else {
        r = src[0];
        g = src[1];
        b = src[2];
    }

    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
        dst[0] = r;
        dst[1] = g;
        dst[2] = b;
        break;
    case SIXEL_PIXELFORMAT_BGR888:
        dst[0] = b;
        dst[1] = g;
        dst[2] = r;
        break;
    case SIXEL_PIXELFORMAT_RGBA8888:
        dst[0] = r;
        dst[1] = g;
        dst[2] = b;
        dst[3] = src[3];
        break;
    case SIXEL_PIXELFORMAT_ARGB8888:
        dst[0] = src[3];
        dst[1] = r;
        dst[2] = g;
        dst[3] = b;
        break;
    case SIXEL_PIXELFORMAT_BGRA8888:
        dst[0] = b;
        dst[1] = g;
        dst[2] = r;
        dst[3] = src[3];
        break;
    case SIXEL_PIXELFORMAT_ABGR8888:
        dst[0] = src[3];
        dst[1] = b;
        dst[2] = g;
        dst[3] = r;
        break;
    case SIXEL_PIXELFORMAT_XRGB8888:
        dst[0] = 0xffU;
        dst[1] = r;
        dst[2] = g;
        dst[3] = b;
        break;
    case SIXEL_PIXELFORMAT_RGBX8888:
        dst[0] = r;
        dst[1] = g;
        dst[2] = b;
        dst[3] = 0xffU;
        break;
    case SIXEL_PIXELFORMAT_XBGR8888:
        dst[0] = 0xffU;
        dst[1] = b;
        dst[2] = g;
        dst[3] = r;
        break;
    case SIXEL_PIXELFORMAT_BGRX8888:
        dst[0] = b;
        dst[1] = g;
        dst[2] = r;
        dst[3] = 0xffU;
        break;
    default:
        break;
    }
}

static SIXELSTATUS
sixel_decode_pixels_convert(unsigned char *decoded,
                            int width,
                            int height,
                            int pixelformat,
                            unsigned char const *bg,
                            unsigned char **out_pixels,
                            unsigned int *result_flags,
                            sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned char *converted;
    unsigned char const *src;
    unsigned char *dst;
    size_t total;
    size_t i;
    int depth;
    int alpha_opaque;

    status = SIXEL_FALSE;
    converted = NULL;
    src = NULL;
    dst = NULL;
    total = 0U;
    i = 0U;
    depth = 0;
    alpha_opaque = 0;

    if (decoded == NULL || out_pixels == NULL || result_flags == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (width <= 0 || height <= 0) {
        return SIXEL_BAD_INPUT;
    }

    depth = sixel_decode_pixels_depth(pixelformat);
    if (depth == 0) {
        sixel_helper_set_additional_message(
            "sixel_decode_pixels: unsupported output pixelformat.");
        return SIXEL_BAD_ARGUMENT;
    }

    total = (size_t)width * (size_t)height;
    if (total > SIZE_MAX / (size_t)depth) {
        sixel_helper_set_additional_message(
            "sixel_decode_pixels: image is too large to convert.");
        return SIXEL_BAD_ALLOCATION;
    }

    alpha_opaque = sixel_decode_pixels_alpha_is_opaque(decoded, total);
    if (!sixel_decode_pixels_format_has_alpha(pixelformat) || alpha_opaque) {
        *result_flags |= SIXEL_DECODE_PIXELS_RESULT_ALPHA_OPAQUE;
    }

    if (pixelformat == SIXEL_PIXELFORMAT_RGBA8888) {
        *out_pixels = decoded;
        return SIXEL_OK;
    }

    converted = (unsigned char *)sixel_allocator_malloc(
        allocator, total * (size_t)depth);
    if (converted == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decode_pixels: allocation failed for pixel output.");
        return SIXEL_BAD_ALLOCATION;
    }

    src = decoded;
    dst = converted;
    for (i = 0U; i < total; ++i) {
        sixel_decode_pixels_store(dst, src, pixelformat, bg);
        src += 4;
        dst += depth;
    }

    *out_pixels = converted;
    status = SIXEL_OK;

    return status;
}

static SIXELSTATUS
sixel_decode_pixels_try(unsigned char *buffer,
                        size_t size,
                        unsigned int decode_flags,
                        unsigned char **out_pixels,
                        int *out_width,
                        int *out_height,
                        unsigned int *result_flags,
                        sixel_allocator_t *allocator)
{
    SIXELSTATUS status;

    if (buffer == NULL || out_pixels == NULL || out_width == NULL ||
            out_height == NULL || result_flags == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (size == 0 || size > (size_t)(INT_MAX - 2)) {
        sixel_helper_set_additional_message(
            "sixel_decode_pixels: invalid input size.");
        return SIXEL_BAD_INPUT;
    }

    status = sixel_decode_direct_with_options(buffer,
                                              (int)size,
                                              decode_flags,
                                              out_pixels,
                                              out_width,
                                              out_height,
                                              result_flags,
                                              allocator);

    return status;
}

static SIXELSTATUS
sixel_decode_pixels_attempts(unsigned char *workbuf,
                             size_t size,
                             unsigned int decode_flags,
                             unsigned char **out_pixels,
                             int *out_width,
                             int *out_height,
                             unsigned int *result_flags,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    unsigned int first_flags;
    unsigned int second_flags;
    unsigned int third_flags;

    first_flags = 0U;
    second_flags = 0U;
    third_flags = 0U;

    status = sixel_decode_pixels_try(workbuf,
                                     size,
                                     decode_flags,
                                     out_pixels,
                                     out_width,
                                     out_height,
                                     &first_flags,
                                     allocator);
    if (status == SIXEL_OK) {
        *result_flags = first_flags;
        return status;
    }

    /* Retry with a synthetic BEL terminator for truncated streams. */
    workbuf[size] = 0x07U;
    status = sixel_decode_pixels_try(workbuf,
                                     size + 1U,
                                     decode_flags,
                                     out_pixels,
                                     out_width,
                                     out_height,
                                     &second_flags,
                                     allocator);
    if (status == SIXEL_OK) {
        *result_flags = second_flags;
        return status;
    }

    /* Retry with ESC \ (ST) in case BEL is not accepted. */
    workbuf[size] = 0x1bU;
    workbuf[size + 1U] = '\\';
    status = sixel_decode_pixels_try(workbuf,
                                     size + 2U,
                                     decode_flags,
                                     out_pixels,
                                     out_width,
                                     out_height,
                                     &third_flags,
                                     allocator);
    if (status == SIXEL_OK) {
        *result_flags = third_flags;
    }

    return status;
}

SIXELAPI SIXELSTATUS
sixel_decode_pixels(unsigned char const *data,
                    size_t size,
                    sixel_decode_options_t const *options,
                    sixel_decode_result_t *result,
                    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_allocator_t *work_allocator;
    unsigned char *workbuf;
    unsigned char *decoded;
    unsigned char *converted;
    unsigned char const default_bg[3] = { 0U, 0U, 0U };
    unsigned char const *bg;
    unsigned int decode_flags;
    unsigned int result_flags;
    int width;
    int height;
    int pixelformat;
    int depth;

    status = SIXEL_FALSE;
    work_allocator = allocator;
    workbuf = NULL;
    decoded = NULL;
    converted = NULL;
    bg = default_bg;
    decode_flags = 0U;
    result_flags = 0U;
    width = 0;
    height = 0;
    pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    depth = 0;

    if (data == NULL || result == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    result->pixels = NULL;
    result->width = 0;
    result->height = 0;
    result->pixelformat = 0;
    result->stride = 0;
    result->flags = 0U;

    if (options != NULL) {
        decode_flags = options->flags;
        if (options->preferred_pixelformat != 0) {
            pixelformat = options->preferred_pixelformat;
        }
        bg = options->bgcolor;
    }

    depth = sixel_decode_pixels_depth(pixelformat);
    if (depth == 0) {
        sixel_helper_set_additional_message(
            "sixel_decode_pixels: unsupported output pixelformat.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (size == 0 || size > (size_t)(INT_MAX - 2) ||
            size > (SIZE_MAX - 2U)) {
        sixel_helper_set_additional_message(
            "sixel_decode_pixels: invalid input size.");
        return SIXEL_BAD_INPUT;
    }

    if (work_allocator != NULL) {
        sixel_allocator_ref(work_allocator);
    } else {
        status = sixel_allocator_new(&work_allocator,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL);
        if (SIXEL_FAILED(status)) {
            work_allocator = NULL;
            goto cleanup;
        }
    }

    workbuf = (unsigned char *)sixel_allocator_malloc(work_allocator,
                                                      size + 2U);
    if (workbuf == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "sixel_decode_pixels: allocation failed for input copy.");
        goto cleanup;
    }
    memcpy(workbuf, data, size);

    status = sixel_decode_pixels_attempts(workbuf,
                                          size,
                                          decode_flags,
                                          &decoded,
                                          &width,
                                          &height,
                                          &result_flags,
                                          work_allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    /* Guard against runaway inputs before exposing the buffer. */
    if (width > SIXEL_DECODE_MAX_SIZE || height > SIXEL_DECODE_MAX_SIZE) {
        status = SIXEL_BAD_INPUT;
        sixel_helper_set_additional_message(
            "sixel_decode_pixels: image exceeds 6000x6000 pixels.");
        goto cleanup;
    }

    status = sixel_decode_pixels_convert(decoded,
                                         width,
                                         height,
                                         pixelformat,
                                         bg,
                                         &converted,
                                         &result_flags,
                                         work_allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (converted == decoded) {
        decoded = NULL;
    }

    result->pixels = converted;
    result->width = width;
    result->height = height;
    result->pixelformat = pixelformat;
    result->stride = width * depth;
    result->flags = result_flags;
    converted = NULL;
    status = SIXEL_OK;

cleanup:
    if (decoded != NULL) {
        sixel_allocator_free(work_allocator, decoded);
        decoded = NULL;
    }
    if (converted != NULL) {
        sixel_allocator_free(work_allocator, converted);
        converted = NULL;
    }
    if (workbuf != NULL) {
        sixel_allocator_free(work_allocator, workbuf);
        workbuf = NULL;
    }
    if (work_allocator != NULL) {
        sixel_allocator_unref(work_allocator);
    }

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
