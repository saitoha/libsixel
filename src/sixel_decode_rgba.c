/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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
/*
 * Shared SIXEL memory decoding helper for RGB/RGBA output.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <sixel.h>

#include "sixel_decode_rgba.h"

#define SIXEL_DECODE_MAX_SIZE 6000

static SIXELSTATUS
sixel_decode_rgba_try(unsigned char *buffer,
                      size_t size,
                      unsigned char **out_pixels,
                      int *out_width,
                      int *out_height,
                      sixel_allocator_t *allocator)
{
    SIXELSTATUS status;

    if (buffer == NULL || out_pixels == NULL || out_width == NULL ||
            out_height == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (size == 0 || size > (size_t)(INT_MAX - 2)) {
        sixel_helper_set_additional_message(
            "sixel_decode_rgba: invalid input size.");
        return SIXEL_BAD_INPUT;
    }

    status = sixel_decode_direct(buffer,
                                 (int)size,
                                 out_pixels,
                                 out_width,
                                 out_height,
                                 allocator);

    return status;
}

static SIXELSTATUS
sixel_decode_rgba_attempts(unsigned char *workbuf,
                           size_t size,
                           unsigned char **out_pixels,
                           int *out_width,
                           int *out_height,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status;

    status = sixel_decode_rgba_try(workbuf,
                                   size,
                                   out_pixels,
                                   out_width,
                                   out_height,
                                   allocator);
    if (status == SIXEL_OK) {
        return status;
    }

    /* Retry with a synthetic BEL terminator for truncated streams. */
    workbuf[size] = 0x07U;
    status = sixel_decode_rgba_try(workbuf,
                                   size + 1U,
                                   out_pixels,
                                   out_width,
                                   out_height,
                                   allocator);
    if (status == SIXEL_OK) {
        return status;
    }

    /* Retry with ESC \ (ST) in case BEL is not accepted. */
    workbuf[size] = 0x1bU;
    workbuf[size + 1U] = '\\';
    status = sixel_decode_rgba_try(workbuf,
                                   size + 2U,
                                   out_pixels,
                                   out_width,
                                   out_height,
                                   allocator);

    return status;
}

static SIXELSTATUS
sixel_decode_rgba_compose_rgb(unsigned char const *rgba,
                              int width,
                              int height,
                              unsigned char const *bgcolor,
                              unsigned char **out_pixels,
                              sixel_allocator_t *allocator)
{
    unsigned char *rgb;
    size_t total;
    size_t i;
    size_t src_index;
    unsigned char bg_r;
    unsigned char bg_g;
    unsigned char bg_b;
    unsigned char const *src;
    unsigned char *dst;
    unsigned int alpha;

    rgb = NULL;
    total = 0U;
    i = 0U;
    src_index = 0U;
    bg_r = bgcolor[0];
    bg_g = bgcolor[1];
    bg_b = bgcolor[2];

    if (rgba == NULL || out_pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (width <= 0 || height <= 0) {
        return SIXEL_BAD_INPUT;
    }

    total = (size_t)width * (size_t)height;
    if (total > SIZE_MAX / 3U) {
        sixel_helper_set_additional_message(
            "sixel_decode_rgba: image is too large to compose.");
        return SIXEL_BAD_ALLOCATION;
    }

    rgb = (unsigned char *)sixel_allocator_malloc(allocator,
                                                  total * 3U);
    if (rgb == NULL) {
        sixel_helper_set_additional_message(
            "sixel_decode_rgba: allocation failed for RGB output.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0U; i < total; ++i) {
        src_index = i * 4U;
        src = rgba + src_index;
        dst = rgb + i * 3U;
        alpha = src[3];

        dst[0] = (unsigned char)((src[0] * alpha + bg_r * (255U - alpha)) /
                                 255U);
        dst[1] = (unsigned char)((src[1] * alpha + bg_g * (255U - alpha)) /
                                 255U);
        dst[2] = (unsigned char)((src[2] * alpha + bg_b * (255U - alpha)) /
                                 255U);
    }

    *out_pixels = rgb;

    return SIXEL_OK;
}

SIXELAPI SIXELSTATUS
sixel_decode_rgba(unsigned char const *data,
                  size_t size,
                  int request_alpha,
                  unsigned char const *bgcolor,
                  unsigned char **out_pixels,
                  int *out_width,
                  int *out_height,
                  int *out_channels,
                  sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_allocator_t *work_allocator;
    unsigned char *workbuf;
    unsigned char *decoded;
    unsigned char *composed;
    unsigned char const default_bg[3] = { 0U, 0U, 0U };
    unsigned char const *bg;
    int width;
    int height;

    status = SIXEL_FALSE;
    work_allocator = allocator;
    workbuf = NULL;
    decoded = NULL;
    composed = NULL;
    bg = default_bg;
    width = 0;
    height = 0;

    if (data == NULL || out_pixels == NULL || out_width == NULL ||
            out_height == NULL || out_channels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (bgcolor != NULL) {
        bg = bgcolor;
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
            "sixel_decode_rgba: allocation failed for input copy.");
        goto cleanup;
    }
    memcpy(workbuf, data, size);

    status = sixel_decode_rgba_attempts(workbuf,
                                        size,
                                        &decoded,
                                        &width,
                                        &height,
                                        work_allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    /* Guard against runaway inputs before exposing the buffer. */
    if (width > SIXEL_DECODE_MAX_SIZE || height > SIXEL_DECODE_MAX_SIZE) {
        status = SIXEL_BAD_INPUT;
        sixel_helper_set_additional_message(
            "sixel_decode_rgba: image exceeds 6000x6000 pixels.");
        goto cleanup;
    }

    if (request_alpha) {
        *out_pixels = decoded;
        *out_channels = 4;
        decoded = NULL;
    } else {
        status = sixel_decode_rgba_compose_rgb(decoded,
                                               width,
                                               height,
                                               bg,
                                               &composed,
                                               work_allocator);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        *out_pixels = composed;
        *out_channels = 3;
        composed = NULL;
    }

    *out_width = width;
    *out_height = height;
    status = SIXEL_OK;

cleanup:
    if (decoded != NULL) {
        sixel_allocator_free(work_allocator, decoded);
        decoded = NULL;
    }
    if (composed != NULL) {
        sixel_allocator_free(work_allocator, composed);
        composed = NULL;
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
