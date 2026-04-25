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
    size_t pixel_count;
    size_t expected_size;
    size_t i;
    unsigned char control;
    unsigned int compression_method;
    unsigned int filter_method;
    unsigned int preprocess_method;
    unsigned int reserved_bits;

    pixel_count = 0u;
    expected_size = 0u;
    i = 0u;
    control = 0u;
    compression_method = 0u;
    filter_method = 0u;
    preprocess_method = 0u;
    reserved_bits = 0u;
    (void)allocator;

    if (rgba == NULL || payload == NULL) {
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
    filter_method = (unsigned int)((control >> 2u) & 0x03u);
    preprocess_method = (unsigned int)((control >> 4u) & 0x03u);
    reserved_bits = (unsigned int)((control >> 6u) & 0x03u);

    if (reserved_bits != 0u) {
        return sixel_webp_vp8_alpha_fail(
            SIXEL_BAD_INPUT,
            "builtin webp: VP8 ALPHA reserved bits are non-zero.");
    }
    if (compression_method != 0u ||
        filter_method != 0u ||
        preprocess_method != 0u) {
        return sixel_webp_vp8_alpha_fail(
            SIXEL_NOT_IMPLEMENTED,
            "builtin webp: VP8 ALPHA mode is not supported yet.");
    }

    expected_size = pixel_count + 1u;
    if (payload_size != expected_size) {
        return sixel_webp_vp8_alpha_fail(
            SIXEL_BAD_INPUT,
            "builtin webp: VP8 ALPHA payload size mismatch.");
    }

    for (i = 0u; i < pixel_count; ++i) {
        rgba[i * 4u + 3u] = payload[i + 1u];
    }
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
