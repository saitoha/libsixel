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

#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#include "compat_stub.h"
#include "fromwebp-vp8-private.h"

static unsigned int
sixel_webp_vp8_parse_read_u16le(unsigned char const *p)
{
    if (p == NULL) {
        return 0u;
    }
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int
sixel_webp_vp8_parse_read_u24le(unsigned char const *p)
{
    if (p == NULL) {
        return 0u;
    }
    return (unsigned int)p[0]
        | ((unsigned int)p[1] << 8)
        | ((unsigned int)p[2] << 16);
}

SIXELSTATUS
sixel_webp_vp8_validate_dimensions(int width,
                                   int height)
{
    size_t pixel_count;

    pixel_count = 0u;
    if (width <= 0 || height <= 0) {
        sixel_helper_set_additional_message(
            "builtin webp: invalid VP8 image dimensions.");
        return SIXEL_BAD_INPUT;
    }
    if (width > SIXEL_WEBP_MAX_DIMENSION ||
        height > SIXEL_WEBP_MAX_DIMENSION) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIXEL_WEBP_MAX_IMAGE_PIXELS) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    return SIXEL_OK;
}

SIXELSTATUS
sixel_webp_vp8_parse_header(unsigned char const *payload,
                            size_t payload_size,
                            sixel_webp_vp8_frame_header_t *header)
{
    SIXELSTATUS status;
    unsigned int frame_tag;
    unsigned int width_raw;
    unsigned int height_raw;

    status = SIXEL_OK;
    frame_tag = 0u;
    width_raw = 0u;
    height_raw = 0u;
    if (payload == NULL || header == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(header, 0, sizeof(*header));

    if (payload_size < 10u) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 payload is truncated.");
        return SIXEL_BAD_INPUT;
    }

    frame_tag = sixel_webp_vp8_parse_read_u24le(payload);
    if ((frame_tag & 1u) != 0u) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 interframes are not supported yet.");
        return SIXEL_NOT_IMPLEMENTED;
    }

    header->version = (int)((frame_tag >> 1) & 0x7u);
    if (header->version > 3) {
        sixel_helper_set_additional_message(
            "builtin webp: unsupported VP8 profile version.");
        return SIXEL_NOT_IMPLEMENTED;
    }

    header->show_frame = (int)((frame_tag >> 4) & 1u);
    if (header->show_frame == 0) {
        sixel_helper_set_additional_message(
            "builtin webp: hidden VP8 frame is not supported.");
        return SIXEL_NOT_IMPLEMENTED;
    }

    header->first_partition_size = (size_t)(frame_tag >> 5);
    if (header->first_partition_size > payload_size - 10u) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 first partition is truncated.");
        return SIXEL_BAD_INPUT;
    }

    if (payload[3] != 0x9du || payload[4] != 0x01u ||
        payload[5] != 0x2au) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 start code is invalid.");
        return SIXEL_BAD_INPUT;
    }

    width_raw = sixel_webp_vp8_parse_read_u16le(payload + 6u);
    height_raw = sixel_webp_vp8_parse_read_u16le(payload + 8u);
    header->width = (int)(width_raw & 0x3fffu);
    header->height = (int)(height_raw & 0x3fffu);

    status = sixel_webp_vp8_validate_dimensions(header->width,
                                                header->height);
    if (SIXEL_FAILED(status)) {
        return status;
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
