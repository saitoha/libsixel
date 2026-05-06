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

#include <limits.h>
#include <stddef.h>

#include "compat_stub.h"
#include "fromwebp-vp8-private.h"

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

SIXELSTATUS
sixel_webp_decode_vp8_payload_into(unsigned char const *payload,
                                   size_t payload_size,
                                   unsigned char *rgba,
                                   size_t rgba_size,
                                   unsigned char **prgba,
                                   int *pwidth,
                                   int *pheight,
                                   sixel_allocator_t *allocator)
{
    return sixel_webp_decode_vp8_payload_into_with_workspace(payload,
                                                             payload_size,
                                                             rgba,
                                                             rgba_size,
                                                             prgba,
                                                             pwidth,
                                                             pheight,
                                                             NULL,
                                                             allocator);
}

SIXELSTATUS
sixel_webp_decode_vp8_payload_into_with_workspace(
    unsigned char const *payload,
    size_t payload_size,
    unsigned char *rgba,
    size_t rgba_size,
    unsigned char **prgba,
    int *pwidth,
    int *pheight,
    sixel_webp_vp8_workspace_t *workspace,
    sixel_allocator_t *allocator)
{
    return sixel_webp_decode_vp8_payload_into_strided_with_workspace(
        payload,
        payload_size,
        rgba,
        rgba_size,
        0u,
        0,
        0,
        prgba,
        pwidth,
        pheight,
        workspace,
        allocator);
}

SIXELSTATUS
sixel_webp_decode_vp8_payload_into_strided_with_workspace(
    unsigned char const *payload,
    size_t payload_size,
    unsigned char *rgba,
    size_t rgba_size,
    size_t rgba_stride,
    int rgba_width,
    int rgba_height,
    unsigned char **prgba,
    int *pwidth,
    int *pheight,
    sixel_webp_vp8_workspace_t *workspace,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_webp_vp8_frame_header_t header;
    unsigned char *decoded_rgba;

    status = SIXEL_OK;
    header.width = 0;
    header.height = 0;
    header.version = 0;
    header.show_frame = 0;
    header.first_partition_size = 0u;
    decoded_rgba = NULL;
    if (payload == NULL || payload_size == 0u || prgba == NULL ||
        pwidth == NULL || pheight == NULL || allocator == NULL ||
        rgba == NULL || rgba_size == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }

    *prgba = NULL;
    *pwidth = 0;
    *pheight = 0;

    status = sixel_webp_vp8_parse_header(payload, payload_size, &header);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_webp_vp8_decode_native_payload_strided(payload,
                                                          payload_size,
                                                          &header,
                                                          rgba,
                                                          rgba_size,
                                                          rgba_stride,
                                                          rgba_width,
                                                          rgba_height,
                                                          prgba,
                                                          pwidth,
                                                          pheight,
                                                          workspace,
                                                          allocator);
    decoded_rgba = *prgba;
    if (SIXEL_SUCCEEDED(status) && decoded_rgba != rgba) {
        return SIXEL_LOGIC_ERROR;
    }
    return status;
}

SIXELSTATUS
sixel_webp_decode_vp8_payload(unsigned char const *payload,
                              size_t payload_size,
                              unsigned char **prgba,
                              int *pwidth,
                              int *pheight,
                              sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_webp_vp8_frame_header_t header;
    unsigned char *rgba;
    size_t pixel_count;
    size_t rgba_size;

    status = SIXEL_OK;
    header.width = 0;
    header.height = 0;
    header.version = 0;
    header.show_frame = 0;
    header.first_partition_size = 0u;
    rgba = NULL;
    pixel_count = 0u;
    rgba_size = 0u;
    if (payload == NULL || payload_size == 0u || prgba == NULL ||
        pwidth == NULL || pheight == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *prgba = NULL;
    *pwidth = 0;
    *pheight = 0;

    status = sixel_webp_vp8_parse_header(payload, payload_size, &header);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = sixel_webp_vp8_validate_dimensions(header.width, header.height);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if ((size_t)header.width > SIZE_MAX / (size_t)header.height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)header.width * (size_t)header.height;
    if (pixel_count > SIZE_MAX / 4u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    rgba_size = pixel_count * 4u;
    rgba = (unsigned char *)sixel_allocator_malloc(allocator, rgba_size);
    if (rgba == NULL) {
        sixel_helper_set_additional_message(
            "builtin webp: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    status = sixel_webp_vp8_decode_native_payload_strided(payload,
                                                          payload_size,
                                                          &header,
                                                          rgba,
                                                          rgba_size,
                                                          0u,
                                                          0,
                                                          0,
                                                          prgba,
                                                          pwidth,
                                                          pheight,
                                                          NULL,
                                                          allocator);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, rgba);
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
