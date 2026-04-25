/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See AUTHORS.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
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

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

/* STDC_HEADERS */
#include <stddef.h>

#if HAVE_STRING_H
# include <string.h>
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#include "compat_stub.h"
#include "fromwebp-internal.h"

static unsigned char
sixel_webp_vp8_alpha_gradient_predictor(unsigned int left,
                                        unsigned int top,
                                        unsigned int top_left)
{
    int value;

    value = (int)left + (int)top - (int)top_left;
    if (value < 0) {
        return 0u;
    }
    if (value > 255) {
        return 255u;
    }
    return (unsigned char)value;
}

static unsigned char
sixel_webp_vp8_alpha_predictor(unsigned int filter_method,
                               int x,
                               int y,
                               unsigned char const *alpha_plane,
                               int width)
{
    size_t index;
    unsigned int left;
    unsigned int top;
    unsigned int top_left;

    index = 0u;
    left = 0u;
    top = 0u;
    top_left = 0u;
    if (alpha_plane == NULL || width <= 0) {
        return 0u;
    }

    index = (size_t)y * (size_t)width + (size_t)x;
    if (x > 0) {
        left = (unsigned int)alpha_plane[index - 1u];
    }
    if (y > 0) {
        top = (unsigned int)alpha_plane[index - (size_t)width];
    }
    if (x > 0 && y > 0) {
        top_left = (unsigned int)alpha_plane[index - (size_t)width - 1u];
    }

    /* The ALPHA filters use special border handling identical to libwebp. */
    if (filter_method == 0u) {
        return 0u;
    }
    if (filter_method == 1u) {
        if (x == 0) {
            return (unsigned char)top;
        }
        return (unsigned char)left;
    }
    if (filter_method == 2u) {
        if (y == 0) {
            return (unsigned char)left;
        }
        return (unsigned char)top;
    }

    if (y == 0) {
        if (x == 0) {
            return (unsigned char)top;
        }
        return (unsigned char)left;
    }
    if (x == 0) {
        return (unsigned char)top;
    }
    return sixel_webp_vp8_alpha_gradient_predictor(left, top, top_left);
}

static SIXELSTATUS
sixel_webp_vp8_alpha_reconstruct(unsigned char *alpha_plane,
                                 int width,
                                 int height,
                                 unsigned char const *encoded_payload)
{
    size_t pixel_count;
    size_t i;
    unsigned int filter_method;
    unsigned char encoded;
    unsigned char predictor;
    int x;
    int y;

    pixel_count = 0u;
    i = 0u;
    filter_method = 0u;
    encoded = 0u;
    predictor = 0u;
    x = 0;
    y = 0;

    if (alpha_plane == NULL || encoded_payload == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_count = (size_t)width * (size_t)height;
    filter_method = (unsigned int)((encoded_payload[0] >> 2u) & 0x03u);
    for (i = 0u; i < pixel_count; ++i) {
        x = (int)(i % (size_t)width);
        y = (int)(i / (size_t)width);
        predictor = sixel_webp_vp8_alpha_predictor(filter_method,
                                                   x,
                                                   y,
                                                   alpha_plane,
                                                   width);
        encoded = encoded_payload[i + 1u];
        alpha_plane[i] = (unsigned char)((unsigned int)encoded +
                                         (unsigned int)predictor);
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_alpha_fail(SIXELSTATUS status, char const *message)
{
    if (message != NULL) {
        sixel_helper_set_additional_message(message);
    }
    return status;
}

SIXELSTATUS
sixel_webp_apply_vp8_alpha_payload(unsigned char *rgba,
                                   int width,
                                   int height,
                                   unsigned char const *payload,
                                   size_t payload_size,
                                   sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    size_t pixel_count;
    size_t expected_size;
    size_t i;
    unsigned char control;
    unsigned int compression_method;
    unsigned int preprocess_method;
    unsigned int reserved_bits;
    unsigned char *alpha_plane;

    status = SIXEL_OK;
    pixel_count = 0u;
    expected_size = 0u;
    i = 0u;
    control = 0u;
    compression_method = 0u;
    preprocess_method = 0u;
    reserved_bits = 0u;
    alpha_plane = NULL;

    if (rgba == NULL || payload == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (width <= 0 || height <= 0) {
        return sixel_webp_vp8_alpha_fail(
            SIXEL_BAD_INPUT,
            "builtin webp: invalid VP8 dimensions for ALPHA.");
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX - 1u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (pixel_count > SIZE_MAX / 4u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    if (payload_size == 0u) {
        return sixel_webp_vp8_alpha_fail(
            SIXEL_BAD_INPUT,
            "builtin webp: VP8 ALPHA payload is empty.");
    }

    control = payload[0];
    compression_method = (unsigned int)(control & 0x03u);
    preprocess_method = (unsigned int)((control >> 4u) & 0x03u);
    reserved_bits = (unsigned int)((control >> 6u) & 0x03u);

    if (reserved_bits != 0u) {
        return sixel_webp_vp8_alpha_fail(
            SIXEL_BAD_INPUT,
            "builtin webp: VP8 ALPHA reserved bits are non-zero.");
    }
    if (compression_method != 0u) {
        return sixel_webp_vp8_alpha_fail(
            SIXEL_NOT_IMPLEMENTED,
            "builtin webp: VP8 ALPHA compression mode is not supported yet.");
    }
    /*
     * Preprocess bits 0 and 1 are accepted in this phase. The decoded alpha
     * plane path is identical for both and future stages can split behavior.
     */
    if (preprocess_method > 1u) {
        return sixel_webp_vp8_alpha_fail(
            SIXEL_NOT_IMPLEMENTED,
            "builtin webp: VP8 ALPHA preprocessing mode is not supported yet.");
    }

    expected_size = pixel_count + 1u;
    if (payload_size != expected_size) {
        return sixel_webp_vp8_alpha_fail(
            SIXEL_BAD_INPUT,
            "builtin webp: VP8 ALPHA payload size mismatch.");
    }

    alpha_plane = (unsigned char *)sixel_allocator_malloc(allocator,
                                                           pixel_count);
    if (alpha_plane == NULL) {
        return sixel_webp_vp8_alpha_fail(
            SIXEL_BAD_ALLOCATION,
            "builtin webp: sixel_allocator_malloc() failed.");
    }
    status = sixel_webp_vp8_alpha_reconstruct(alpha_plane,
                                              width,
                                              height,
                                              payload);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, alpha_plane);
        return status;
    }
    for (i = 0u; i < pixel_count; ++i) {
        rgba[i * 4u + 3u] = alpha_plane[i];
    }
    sixel_allocator_free(allocator, alpha_plane);
    return SIXEL_OK;
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
