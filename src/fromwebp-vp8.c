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
#include <limits.h>
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

#if HAVE_WEBP
# include <webp/decode.h>
#endif

#include "compat_stub.h"
#include "fromwebp-internal.h"

typedef struct sixel_webp_vp8_frame_header {
    int width;
    int height;
    int version;
    int show_frame;
    size_t first_partition_size;
} sixel_webp_vp8_frame_header_t;

static unsigned int
sixel_webp_vp8_read_u16le(unsigned char const *p)
{
    if (p == NULL) {
        return 0u;
    }
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int
sixel_webp_vp8_read_u24le(unsigned char const *p)
{
    if (p == NULL) {
        return 0u;
    }
    return (unsigned int)p[0]
        | ((unsigned int)p[1] << 8)
        | ((unsigned int)p[2] << 16);
}

static void
sixel_webp_vp8_write_u32le(unsigned char *p,
                           unsigned int value)
{
    if (p == NULL) {
        return;
    }
    p[0] = (unsigned char)(value & 0xffu);
    p[1] = (unsigned char)((value >> 8) & 0xffu);
    p[2] = (unsigned char)((value >> 16) & 0xffu);
    p[3] = (unsigned char)((value >> 24) & 0xffu);
}

static SIXELSTATUS
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

static SIXELSTATUS
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

    frame_tag = sixel_webp_vp8_read_u24le(payload);
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

    width_raw = sixel_webp_vp8_read_u16le(payload + 6u);
    height_raw = sixel_webp_vp8_read_u16le(payload + 8u);
    header->width = (int)(width_raw & 0x3fffu);
    header->height = (int)(height_raw & 0x3fffu);

    status = sixel_webp_vp8_validate_dimensions(header->width,
                                                header->height);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_wrap_riff_payload(unsigned char const *payload,
                                 size_t payload_size,
                                 unsigned char **priff_data,
                                 size_t *priff_size,
                                 sixel_allocator_t *allocator)
{
    unsigned char *riff_data;
    size_t payload_padded_size;
    size_t riff_size;
    unsigned int riff_size_field;

    riff_data = NULL;
    payload_padded_size = 0u;
    riff_size = 0u;
    riff_size_field = 0u;
    if (payload == NULL || priff_data == NULL || priff_size == NULL ||
        allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *priff_data = NULL;
    *priff_size = 0u;

    if (payload_size > SIZE_MAX - (payload_size & 1u)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    payload_padded_size = payload_size + (payload_size & 1u);

    if (payload_padded_size > SIZE_MAX - 20u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    riff_size = 20u + payload_padded_size;

#if SIZE_MAX > UINT_MAX
    if (riff_size - 8u > (size_t)UINT_MAX) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
#endif
    riff_size_field = (unsigned int)(riff_size - 8u);

    riff_data = (unsigned char *)sixel_allocator_malloc(allocator, riff_size);
    if (riff_data == NULL) {
        sixel_helper_set_additional_message(
            "builtin webp: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memcpy(riff_data, "RIFF", 4u);
    sixel_webp_vp8_write_u32le(riff_data + 4u, riff_size_field);
    memcpy(riff_data + 8u, "WEBP", 4u);
    memcpy(riff_data + 12u, "VP8 ", 4u);
    sixel_webp_vp8_write_u32le(riff_data + 16u, (unsigned int)payload_size);
    memcpy(riff_data + 20u, payload, payload_size);
    if ((payload_size & 1u) != 0u) {
        riff_data[20u + payload_size] = 0u;
    }

    *priff_data = riff_data;
    *priff_size = riff_size;
    return SIXEL_OK;
}

#if HAVE_WEBP
static SIXELSTATUS
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
#endif

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
    unsigned char *riff_data;
    size_t riff_size;

    status = SIXEL_OK;
    memset(&header, 0, sizeof(header));
    riff_data = NULL;
    riff_size = 0u;
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

    status = sixel_webp_vp8_wrap_riff_payload(payload,
                                              payload_size,
                                              &riff_data,
                                              &riff_size,
                                              allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

#if HAVE_WEBP
    status = sixel_webp_vp8_decode_with_libwebp(riff_data,
                                                riff_size,
                                                &header,
                                                prgba,
                                                pwidth,
                                                pheight,
                                                allocator);
#else
    (void)header;
    sixel_helper_set_additional_message(
        "builtin webp: VP8 static decoder is unavailable in this build.");
    status = SIXEL_NOT_IMPLEMENTED;
#endif

cleanup:
    sixel_allocator_free(allocator, riff_data);
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
