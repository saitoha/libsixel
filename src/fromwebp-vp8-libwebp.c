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

#if HAVE_WEBP

/* STDC_HEADERS */
#include <stddef.h>

#if HAVE_STRING_H
# include <string.h>
#endif

#include <webp/decode.h>

#include "compat_stub.h"
#include "fromwebp-vp8-private.h"

SIXELSTATUS
sixel_webp_vp8_decode_with_libwebp(unsigned char const *riff_data,
                                   size_t riff_size,
                                   sixel_webp_vp8_frame_header_t const *header,
                                   unsigned char **prgba,
                                   int *pwidth,
                                   int *pheight,
                                   sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    WebPDecoderConfig config;
    VP8StatusCode feature_status;
    VP8StatusCode decode_status;
    unsigned char *rgba;
    size_t pixel_count;
    size_t rgba_size;
    int width;
    int height;

    status = SIXEL_OK;
    memset(&config, 0, sizeof(config));
    feature_status = VP8_STATUS_OK;
    decode_status = VP8_STATUS_OK;
    rgba = NULL;
    pixel_count = 0u;
    rgba_size = 0u;
    width = 0;
    height = 0;

    if (riff_data == NULL || header == NULL || prgba == NULL ||
        pwidth == NULL || pheight == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (WebPInitDecoderConfig(&config) == 0) {
        sixel_helper_set_additional_message(
            "builtin webp: WebPInitDecoderConfig() failed.");
        return SIXEL_NOT_IMPLEMENTED;
    }

    feature_status = WebPGetFeatures(riff_data, riff_size, &config.input);
    if (feature_status != VP8_STATUS_OK) {
        if (feature_status == VP8_STATUS_UNSUPPORTED_FEATURE) {
            sixel_helper_set_additional_message(
                "builtin webp: unsupported VP8 feature.");
            return SIXEL_NOT_IMPLEMENTED;
        }
        sixel_helper_set_additional_message(
            "builtin webp: VP8 bitstream parse failed.");
        return SIXEL_BAD_INPUT;
    }

    width = config.input.width;
    height = config.input.height;
    if (width != header->width || height != header->height) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8 header dimensions mismatch.");
        return SIXEL_BAD_INPUT;
    }

    status = sixel_webp_vp8_validate_dimensions(width, height);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    pixel_count = (size_t)width * (size_t)height;
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

    config.output.colorspace = MODE_RGBA;
    config.output.u.RGBA.rgba = rgba;
    config.output.u.RGBA.stride = width * 4;
    config.output.u.RGBA.size = rgba_size;
    config.output.is_external_memory = 1;

    decode_status = WebPDecode(riff_data, riff_size, &config);
    if (decode_status != VP8_STATUS_OK) {
        if (decode_status == VP8_STATUS_UNSUPPORTED_FEATURE) {
            sixel_helper_set_additional_message(
                "builtin webp: unsupported VP8 feature.");
            status = SIXEL_NOT_IMPLEMENTED;
        } else {
            sixel_helper_set_additional_message(
                "builtin webp: VP8 decode failed.");
            status = SIXEL_BAD_INPUT;
        }
        goto cleanup;
    }

    *prgba = rgba;
    *pwidth = width;
    *pheight = height;
    rgba = NULL;
    status = SIXEL_OK;

cleanup:
    sixel_allocator_free(allocator, rgba);
    WebPFreeDecBuffer(&config.output);
    return status;
}

#else
/*
 * Keep a non-empty translation unit when libwebp support is disabled.
 */
typedef int sixel_webp_vp8_libwebp_disabled_marker_t;
#endif  /* HAVE_WEBP */


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
