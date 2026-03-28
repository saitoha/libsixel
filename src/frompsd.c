/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* STDC_HEADERS */
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <sixel.h>

#include "allocator.h"
#include "cms.h"
#include "frompsd.h"
#include "pixelformat.h"

#define SIXEL_FROMPSD_MAX_CHANNELS 56u
#define SIXEL_FROMPSD_MAX_DIMENSION 300000u

int stbi_zlib_decode_buffer(char *obuffer,
                            int olen,
                            char const *ibuffer,
                            int ilen);

int
sixel_builtin_extract_psd_icc(unsigned char const *buffer,
                              size_t size,
                              unsigned char const **profile,
                              size_t *profile_length)
{
    size_t offset;
    size_t section_length;
    size_t resource_end;
    size_t name_length;
    size_t data_length;
    unsigned int resource_id;

    offset = 0u;
    section_length = 0u;
    resource_end = 0u;
    name_length = 0u;
    data_length = 0u;
    resource_id = 0u;

    if (buffer == NULL || profile == NULL || profile_length == NULL ||
        size < 34u) {
        return SIXEL_BUILTIN_ICC_EXTRACT_ABSENT;
    }
    *profile = NULL;
    *profile_length = 0u;
    if (memcmp(buffer, "8BPS", 4u) != 0) {
        return SIXEL_BUILTIN_ICC_EXTRACT_ABSENT;
    }

    /* color mode data section */
    offset = 26u;
    section_length = ((size_t)buffer[offset + 0u] << 24) |
                     ((size_t)buffer[offset + 1u] << 16) |
                     ((size_t)buffer[offset + 2u] << 8) |
                     (size_t)buffer[offset + 3u];
    offset += 4u;
    if (section_length > size - offset) {
        return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
    }
    offset += section_length;

    /* image resources section */
    if (offset + 4u > size) {
        return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
    }
    section_length = ((size_t)buffer[offset + 0u] << 24) |
                     ((size_t)buffer[offset + 1u] << 16) |
                     ((size_t)buffer[offset + 2u] << 8) |
                     (size_t)buffer[offset + 3u];
    offset += 4u;
    if (section_length > size - offset) {
        return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
    }

    resource_end = offset + section_length;
    while (offset + 12u <= resource_end) {
        if (memcmp(buffer + offset, "8BIM", 4u) != 0) {
            return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
        }
        offset += 4u;

        resource_id = ((unsigned int)buffer[offset + 0u] << 8) |
                      (unsigned int)buffer[offset + 1u];
        offset += 2u;

        name_length = (size_t)buffer[offset];
        ++offset;
        if (name_length > resource_end - offset) {
            return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
        }
        offset += name_length;
        if (((1u + name_length) & 1u) != 0u) {
            if (offset >= resource_end) {
                return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
            }
            ++offset;
        }

        if (offset + 4u > resource_end) {
            return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
        }
        data_length = ((size_t)buffer[offset + 0u] << 24) |
                      ((size_t)buffer[offset + 1u] << 16) |
                      ((size_t)buffer[offset + 2u] << 8) |
                      (size_t)buffer[offset + 3u];
        offset += 4u;
        if (data_length > resource_end - offset) {
            return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
        }

        if (resource_id == 0x040fu) {
            if (data_length == 0u) {
                return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
            }
            *profile = buffer + offset;
            *profile_length = data_length;
            return SIXEL_BUILTIN_ICC_EXTRACT_FOUND;
        }

        offset += data_length;
        if ((data_length & 1u) != 0u) {
            if (offset >= resource_end) {
                return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
            }
            ++offset;
        }
    }

    return SIXEL_BUILTIN_ICC_EXTRACT_ABSENT;
}


static unsigned int
sixel_builtin_read_u16be(unsigned char const *p)
{
    return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

static void
sixel_builtin_write_u16be(unsigned char *p, unsigned int value)
{
    p[0] = (unsigned char)((value >> 8) & 0xffu);
    p[1] = (unsigned char)(value & 0xffu);
}

static size_t
sixel_builtin_read_u32be_size(unsigned char const *p)
{
    return ((size_t)p[0] << 24) |
           ((size_t)p[1] << 16) |
           ((size_t)p[2] << 8) |
           (size_t)p[3];
}

static uint32_t
sixel_builtin_read_u32be(unsigned char const *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static void
sixel_builtin_write_u32be(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)((value >> 24) & 0xffu);
    p[1] = (unsigned char)((value >> 16) & 0xffu);
    p[2] = (unsigned char)((value >> 8) & 0xffu);
    p[3] = (unsigned char)(value & 0xffu);
}

static void
sixel_builtin_psd_set_message(char *message,
                              size_t message_size,
                              char const *text)
{
    if (message == NULL || message_size == 0u) {
        return;
    }
    if (text == NULL) {
        message[0] = '\0';
        return;
    }
    (void)snprintf(message, message_size, "%s", text);
}

static void
sixel_builtin_psd_init_transparent_mask_output(
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size)
{
    if (ptransparent_mask != NULL) {
        *ptransparent_mask = NULL;
    }
    if (ptransparent_mask_size != NULL) {
        *ptransparent_mask_size = 0u;
    }
}

static char const *
sixel_builtin_psd_mode_name(unsigned int color_mode)
{
    switch (color_mode) {
    case 0u:
        return "Bitmap";
    case 1u:
        return "Grayscale";
    case 2u:
        return "Indexed";
    case 3u:
        return "RGB";
    case 4u:
        return "CMYK";
    case 7u:
        return "Multichannel";
    case 8u:
        return "Duotone";
    case 9u:
        return "Lab";
    default:
        return "Unknown";
    }
}

static int
sixel_builtin_psd_has_layer_records(sixel_chunk_t const *chunk,
                                    sixel_builtin_psd_info_t const *info)
{
    size_t layer_info_length;

    layer_info_length = 0u;
    if (chunk == NULL || info == NULL || chunk->buffer == NULL) {
        return -1;
    }
    if (info->layer_mask_length == 0u) {
        return 0;
    }
    if (info->layer_mask_offset > chunk->size ||
        info->layer_mask_length > chunk->size - info->layer_mask_offset) {
        return -1;
    }
    if (info->layer_mask_length < 4u) {
        return 0;
    }

    layer_info_length = sixel_builtin_read_u32be_size(chunk->buffer +
                                                       info->layer_mask_offset);
    if (layer_info_length > info->layer_mask_length - 4u) {
        return -1;
    }

    return layer_info_length > 0u ? 1 : 0;
}

static int
sixel_builtin_psd_compute_row_bytes(sixel_builtin_psd_info_t const *info,
                                    size_t *prow_bytes)
{
    size_t row_bytes;
    size_t bytes_per_sample;

    row_bytes = 0u;
    bytes_per_sample = 0u;
    if (info == NULL || prow_bytes == NULL || info->width == 0u) {
        return 0;
    }

    if (info->depth == 1u) {
        row_bytes = ((size_t)info->width + 7u) / 8u;
        if (row_bytes == 0u) {
            return 0;
        }
        *prow_bytes = row_bytes;
        return 1;
    }

    if (info->depth == 8u) {
        bytes_per_sample = 1u;
    } else if (info->depth == 16u) {
        bytes_per_sample = 2u;
    } else if (info->depth == 32u) {
        bytes_per_sample = 4u;
    } else {
        return 0;
    }

    if ((size_t)info->width > SIZE_MAX / bytes_per_sample) {
        return 0;
    }
    row_bytes = (size_t)info->width * bytes_per_sample;
    if (row_bytes == 0u) {
        return 0;
    }
    *prow_bytes = row_bytes;
    return 1;
}

int
sixel_builtin_validate_psd_info(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    int *pdecode_mode,
    int *pskip_icc_conversion,
    int *pcolorspace,
    char *message,
    size_t message_size)
{
    char ignored_message[2];
    int decode_mode;
    int skip_icc_conversion;
    int colorspace;
    unsigned int min_channels;
    size_t image_data_length;
    int layer_state;
    size_t row_bytes;
    size_t plane_bytes;
    size_t total_bytes;
    size_t table_entries;
    size_t table_bytes;
    int nwrite;

    ignored_message[0] = '\0';
    ignored_message[1] = '\0';
    if (message == NULL || message_size == 0u) {
        message = ignored_message;
        message_size = sizeof(ignored_message);
    }

    decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_NONE;
    skip_icc_conversion = 0;
    colorspace = SIXEL_COLORSPACE_GAMMA;
    min_channels = 1u;
    image_data_length = 0u;
    layer_state = 0;
    row_bytes = 0u;
    plane_bytes = 0u;
    total_bytes = 0u;
    table_entries = 0u;
    table_bytes = 0u;
    nwrite = 0;

    if (pdecode_mode != NULL) {
        *pdecode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_NONE;
    }
    if (pskip_icc_conversion != NULL) {
        *pskip_icc_conversion = 0;
    }
    if (pcolorspace != NULL) {
        *pcolorspace = SIXEL_COLORSPACE_GAMMA;
    }
    message[0] = '\0';

    if (chunk == NULL || info == NULL || chunk->buffer == NULL) {
        sixel_builtin_psd_set_message(
            message,
            message_size,
            "builtin PSD: malformed header/metadata");
        return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
    }

    if (info->version != 1u) {
        sixel_builtin_psd_set_message(
            message,
            message_size,
            "builtin PSD: unsupported version (expected 1)");
        return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
    }

    if (info->channels < 1u || info->channels > SIXEL_FROMPSD_MAX_CHANNELS) {
        nwrite = snprintf(message,
                          message_size,
                          "builtin PSD: malformed channel count (%u; expected 1..%u)",
                          info->channels,
                          SIXEL_FROMPSD_MAX_CHANNELS);
        (void)nwrite;
        return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
    }

    if (info->width < 1u || info->height < 1u ||
        info->width > SIXEL_FROMPSD_MAX_DIMENSION ||
        info->height > SIXEL_FROMPSD_MAX_DIMENSION) {
        nwrite = snprintf(message,
                          message_size,
                          "builtin PSD: malformed dimensions (%ux%u; expected 1..%u)",
                          info->width,
                          info->height,
                          SIXEL_FROMPSD_MAX_DIMENSION);
        (void)nwrite;
        return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
    }

    if (info->compression > 3u) {
        nwrite = snprintf(message,
                          message_size,
                          "builtin PSD: unsupported compression (%u)",
                          info->compression);
        (void)nwrite;
        return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
    }

    if (info->image_data_offset > chunk->size) {
        sixel_builtin_psd_set_message(
            message,
            message_size,
            "builtin PSD: malformed image data offset");
        return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
    }
    image_data_length = chunk->size - info->image_data_offset;
    if (image_data_length == 0u) {
        layer_state = sixel_builtin_psd_has_layer_records(chunk, info);
        if (layer_state < 0) {
            sixel_builtin_psd_set_message(
                message,
                message_size,
                "builtin PSD: malformed layer/mask section");
            return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
        }
        if (layer_state > 0) {
            sixel_builtin_psd_set_message(
                message,
                message_size,
                "builtin PSD: unsupported file without merged/composite image");
            return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
        }
        sixel_builtin_psd_set_message(
            message,
            message_size,
            "builtin PSD: malformed image data section");
        return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
    }

    switch (info->color_mode) {
    case 0u:
        min_channels = 1u;
        if (info->depth != 1u) {
            nwrite = snprintf(message,
                              message_size,
                              "builtin PSD: unsupported bit depth (%u) for Bitmap",
                              info->depth);
            (void)nwrite;
            return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
        }
        if (info->compression == 3u) {
            sixel_builtin_psd_set_message(
                message,
                message_size,
                "builtin PSD: unsupported compression (3) for Bitmap 1-bit");
            return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
        }
        decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_BITMAP_1BIT;
        break;
    case 1u:
    case 8u:
        min_channels = 1u;
        if (info->depth == 8u) {
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_GRAY_INDEXED_8BIT;
        } else if (info->depth == 16u) {
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_GRAY_DUOTONE_16BIT;
        } else if (info->depth == 32u) {
            if (info->compression == 1u) {
                sixel_builtin_psd_set_message(
                    message,
                    message_size,
                    "builtin PSD: unsupported RLE compression for 32-bit Gray/Duotone");
                return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
            }
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_GRAY_DUOTONE_32BIT;
        } else {
            nwrite = snprintf(message,
                              message_size,
                              "builtin PSD: unsupported bit depth (%u) for %s",
                              info->depth,
                              sixel_builtin_psd_mode_name(info->color_mode));
            (void)nwrite;
            return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
        }
        break;
    case 2u:
        min_channels = 1u;
        if (info->depth != 8u) {
            nwrite = snprintf(message,
                              message_size,
                              "builtin PSD: unsupported bit depth (%u) for Indexed",
                              info->depth);
            (void)nwrite;
            return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
        }
        decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_GRAY_INDEXED_8BIT;
        break;
    case 3u:
        min_channels = 3u;
        if (info->depth == 8u) {
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_RGB_8BIT;
        } else if (info->depth == 16u) {
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_RGB_16BIT;
        } else if (info->depth == 32u) {
            if (info->compression == 1u) {
                sixel_builtin_psd_set_message(
                    message,
                    message_size,
                    "builtin PSD: unsupported RLE compression for 32-bit RGB");
                return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
            }
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_RGB_32BIT;
        } else {
            nwrite = snprintf(message,
                              message_size,
                              "builtin PSD: unsupported bit depth (%u) for RGB",
                              info->depth);
            (void)nwrite;
            return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
        }
        break;
    case 4u:
        min_channels = 4u;
        if (info->depth == 8u) {
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_8BIT;
            skip_icc_conversion = 1;
        } else if (info->depth == 32u) {
            if (info->compression == 1u) {
                sixel_builtin_psd_set_message(
                    message,
                    message_size,
                    "builtin PSD: unsupported RLE compression for 32-bit CMYK");
                return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
            }
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_32BIT;
            skip_icc_conversion = 1;
            colorspace = SIXEL_COLORSPACE_LINEAR;
        } else {
            nwrite = snprintf(message,
                              message_size,
                              "builtin PSD: unsupported bit depth (%u) for CMYK",
                              info->depth);
            (void)nwrite;
            return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
        }
        break;
    case 7u:
        sixel_builtin_psd_set_message(
            message,
            message_size,
            "builtin PSD: unsupported color mode (7: Multichannel)");
        return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
    case 9u:
        min_channels = 3u;
        if (info->depth == 8u) {
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_8BIT;
            skip_icc_conversion = 1;
            colorspace = SIXEL_COLORSPACE_CIELAB;
        } else if (info->depth == 32u) {
            if (info->compression == 1u) {
                sixel_builtin_psd_set_message(
                    message,
                    message_size,
                    "builtin PSD: unsupported RLE compression for 32-bit Lab");
                return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
            }
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_32BIT;
            skip_icc_conversion = 1;
            colorspace = SIXEL_COLORSPACE_CIELAB;
        } else {
            nwrite = snprintf(message,
                              message_size,
                              "builtin PSD: unsupported bit depth (%u) for Lab",
                              info->depth);
            (void)nwrite;
            return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
        }
        break;
    default:
        nwrite = snprintf(message,
                          message_size,
                          "builtin PSD: unsupported color mode (%u)",
                          info->color_mode);
        (void)nwrite;
        return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
    }

    if (info->channels < min_channels) {
        nwrite = snprintf(message,
                          message_size,
                          "builtin PSD: malformed channel count (%u) for %s (requires >=%u)",
                          info->channels,
                          sixel_builtin_psd_mode_name(info->color_mode),
                          min_channels);
        (void)nwrite;
        return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
    }

    if (!sixel_builtin_psd_compute_row_bytes(info, &row_bytes) ||
        row_bytes > SIZE_MAX / (size_t)info->height) {
        sixel_builtin_psd_set_message(
            message,
            message_size,
            "builtin PSD: malformed dimensions/depth overflow");
        return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
    }
    plane_bytes = row_bytes * (size_t)info->height;

    if (info->compression == 0u) {
        if (plane_bytes > 0u &&
            (size_t)info->channels > SIZE_MAX / plane_bytes) {
            sixel_builtin_psd_set_message(
                message,
                message_size,
                "builtin PSD: malformed raw plane size overflow");
            return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
        }
        total_bytes = plane_bytes * (size_t)info->channels;
        if (total_bytes > image_data_length) {
            sixel_builtin_psd_set_message(
                message,
                message_size,
                "builtin PSD: malformed raw channel stream (too short)");
            return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
        }
    } else if (info->compression == 1u) {
        if ((size_t)info->height > SIZE_MAX / (size_t)info->channels) {
            sixel_builtin_psd_set_message(
                message,
                message_size,
                "builtin PSD: malformed RLE row table overflow");
            return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
        }
        table_entries = (size_t)info->height * (size_t)info->channels;
        if (table_entries > SIZE_MAX / 2u) {
            sixel_builtin_psd_set_message(
                message,
                message_size,
                "builtin PSD: malformed RLE row table overflow");
            return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
        }
        table_bytes = table_entries * 2u;
        if (table_bytes > image_data_length) {
            sixel_builtin_psd_set_message(
                message,
                message_size,
                "builtin PSD: malformed RLE row table (too short)");
            return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
        }
    }

    if (pdecode_mode != NULL) {
        *pdecode_mode = decode_mode;
    }
    if (pskip_icc_conversion != NULL) {
        *pskip_icc_conversion = skip_icc_conversion;
    }
    if (pcolorspace != NULL) {
        *pcolorspace = colorspace;
    }
    return SIXEL_BUILTIN_PSD_VALIDATE_OK;
}

int
sixel_builtin_parse_psd_info(sixel_chunk_t const *chunk,
                             sixel_builtin_psd_info_t *info)
{
    unsigned char const *buffer;
    size_t size;
    size_t offset;
    size_t section_length;

    buffer = NULL;
    size = 0u;
    offset = 0u;
    section_length = 0u;
    if (chunk == NULL || chunk->buffer == NULL || info == NULL ||
        chunk->size < 30u) {
        return 0;
    }
    buffer = chunk->buffer;
    size = chunk->size;
    if (memcmp(buffer, "8BPS", 4u) != 0) {
        return 0;
    }

    memset(info, 0, sizeof(*info));
    info->version = sixel_builtin_read_u16be(buffer + 4u);
    info->channels = sixel_builtin_read_u16be(buffer + 12u);
    info->height = (unsigned int)sixel_builtin_read_u32be_size(buffer + 14u);
    info->width = (unsigned int)sixel_builtin_read_u32be_size(buffer + 18u);
    info->depth = sixel_builtin_read_u16be(buffer + 22u);
    info->color_mode = sixel_builtin_read_u16be(buffer + 24u);

    offset = 26u;
    if (offset + 4u > size) {
        return 0;
    }
    section_length = sixel_builtin_read_u32be_size(buffer + offset);
    offset += 4u;
    if (section_length > size - offset) {
        return 0;
    }
    info->color_mode_data_offset = offset;
    info->color_mode_data_length = section_length;
    offset += section_length;

    if (offset + 4u > size) {
        return 0;
    }
    section_length = sixel_builtin_read_u32be_size(buffer + offset);
    offset += 4u;
    if (section_length > size - offset) {
        return 0;
    }
    info->image_resources_offset = offset;
    info->image_resources_length = section_length;
    offset += section_length;

    if (offset + 4u > size) {
        return 0;
    }
    section_length = sixel_builtin_read_u32be_size(buffer + offset);
    offset += 4u;
    if (section_length > size - offset) {
        return 0;
    }
    info->layer_mask_offset = offset;
    info->layer_mask_length = section_length;
    offset += section_length;

    if (offset + 2u > size) {
        return 0;
    }
    info->compression = sixel_builtin_read_u16be(buffer + offset);
    info->image_data_offset = offset + 2u;

    return 1;
}


static int
sixel_builtin_psd_unpack_packbits_row(unsigned char const *src,
                                      size_t src_length,
                                      unsigned char *dst,
                                      size_t dst_length)
{
    size_t src_offset;
    size_t dst_offset;
    unsigned int control;
    size_t run;
    unsigned char value;

    src_offset = 0u;
    dst_offset = 0u;
    control = 0u;
    run = 0u;
    value = 0u;
    if (src == NULL || dst == NULL) {
        return 0;
    }

    while (dst_offset < dst_length) {
        if (src_offset >= src_length) {
            return 0;
        }
        control = src[src_offset++];
        if (control <= 127u) {
            run = (size_t)control + 1u;
            if (run > dst_length - dst_offset ||
                run > src_length - src_offset) {
                return 0;
            }
            memcpy(dst + dst_offset, src + src_offset, run);
            src_offset += run;
            dst_offset += run;
        } else if (control == 128u) {
            /* no-op */
        } else {
            run = 257u - (size_t)control;
            if (run > dst_length - dst_offset || src_offset >= src_length) {
                return 0;
            }
            value = src[src_offset++];
            memset(dst + dst_offset, (int)value, run);
            dst_offset += run;
        }
    }

    if (src_offset != src_length) {
        return 0;
    }
    return 1;
}

static void
sixel_builtin_psd_decode_prediction_8bit(unsigned char *data,
                                         unsigned int channels,
                                         unsigned int height,
                                         size_t row_bytes)
{
    unsigned int channel;
    unsigned int row;
    size_t plane_bytes;
    unsigned char *plane;
    unsigned char *row_data;
    size_t x;

    channel = 0u;
    row = 0u;
    plane_bytes = row_bytes * (size_t)height;
    plane = NULL;
    row_data = NULL;
    x = 0u;

    for (channel = 0u; channel < channels; ++channel) {
        plane = data + (size_t)channel * plane_bytes;
        for (row = 0u; row < height; ++row) {
            row_data = plane + (size_t)row * row_bytes;
            for (x = 1u; x < row_bytes; ++x) {
                row_data[x] = (unsigned char)((unsigned int)row_data[x] +
                                              (unsigned int)row_data[x - 1u]);
            }
        }
    }
}

static int
sixel_builtin_psd_decode_prediction_16bit(unsigned char *data,
                                          unsigned int channels,
                                          unsigned int height,
                                          size_t row_bytes)
{
    unsigned int channel;
    unsigned int row;
    size_t row_words;
    size_t plane_bytes;
    unsigned char *plane;
    unsigned char *row_data;
    size_t x;
    unsigned int prev;
    unsigned int curr;

    channel = 0u;
    row = 0u;
    row_words = 0u;
    plane_bytes = 0u;
    plane = NULL;
    row_data = NULL;
    x = 0u;
    prev = 0u;
    curr = 0u;

    if ((row_bytes & 1u) != 0u) {
        return 0;
    }
    row_words = row_bytes / 2u;
    plane_bytes = row_bytes * (size_t)height;

    for (channel = 0u; channel < channels; ++channel) {
        plane = data + (size_t)channel * plane_bytes;
        for (row = 0u; row < height; ++row) {
            row_data = plane + (size_t)row * row_bytes;
            for (x = 1u; x < row_words; ++x) {
                prev = sixel_builtin_read_u16be(row_data + (x - 1u) * 2u);
                curr = sixel_builtin_read_u16be(row_data + x * 2u);
                curr = (curr + prev) & 0xffffu;
                sixel_builtin_write_u16be(row_data + x * 2u, curr);
            }
        }
    }

    return 1;
}

static int
sixel_builtin_psd_decode_prediction_32bit(unsigned char *data,
                                          unsigned int channels,
                                          unsigned int height,
                                          size_t row_bytes)
{
    unsigned int channel;
    unsigned int row;
    size_t row_dwords;
    size_t plane_bytes;
    unsigned char *plane;
    unsigned char *row_data;
    size_t x;
    uint32_t prev;
    uint32_t curr;

    channel = 0u;
    row = 0u;
    row_dwords = 0u;
    plane_bytes = 0u;
    plane = NULL;
    row_data = NULL;
    x = 0u;
    prev = 0u;
    curr = 0u;

    if ((row_bytes & 3u) != 0u) {
        return 0;
    }
    row_dwords = row_bytes / 4u;
    plane_bytes = row_bytes * (size_t)height;

    for (channel = 0u; channel < channels; ++channel) {
        plane = data + (size_t)channel * plane_bytes;
        for (row = 0u; row < height; ++row) {
            row_data = plane + (size_t)row * row_bytes;
            for (x = 1u; x < row_dwords; ++x) {
                prev = sixel_builtin_read_u32be(row_data + (x - 1u) * 4u);
                curr = sixel_builtin_read_u32be(row_data + x * 4u);
                curr += prev;
                sixel_builtin_write_u32be(row_data + x * 4u, curr);
            }
        }
    }

    return 1;
}

static int
sixel_builtin_psd_decode_zip_planar(sixel_chunk_t const *chunk,
                                    sixel_builtin_psd_info_t const *info,
                                    size_t row_bytes,
                                    unsigned char **pdecoded,
                                    size_t *pplane_bytes)
{
    size_t plane_bytes;
    size_t total_bytes;
    size_t compressed_offset;
    size_t compressed_bytes;
    unsigned char *decoded;
    int decoded_bytes;

    plane_bytes = 0u;
    total_bytes = 0u;
    compressed_offset = 0u;
    compressed_bytes = 0u;
    decoded = NULL;
    decoded_bytes = 0;

    if (chunk == NULL || info == NULL || pdecoded == NULL || pplane_bytes == NULL ||
        chunk->buffer == NULL || chunk->allocator == NULL ||
        row_bytes == 0u || info->height == 0u || info->channels == 0u) {
        return 0;
    }
    if (info->compression != 2u && info->compression != 3u) {
        return 0;
    }
    if (row_bytes > SIZE_MAX / (size_t)info->height) {
        return 0;
    }
    plane_bytes = row_bytes * (size_t)info->height;
    if (plane_bytes > 0u &&
        (size_t)info->channels > SIZE_MAX / plane_bytes) {
        return 0;
    }
    total_bytes = plane_bytes * (size_t)info->channels;
    if (total_bytes == 0u) {
        return 0;
    }
    if (info->image_data_offset > chunk->size) {
        return 0;
    }
    compressed_offset = info->image_data_offset;
    compressed_bytes = chunk->size - compressed_offset;
    if (compressed_bytes == 0u) {
        return 0;
    }
    if (total_bytes > (size_t)INT_MAX || compressed_bytes > (size_t)INT_MAX) {
        return 0;
    }

    decoded = (unsigned char *)sixel_allocator_malloc(chunk->allocator, total_bytes);
    if (decoded == NULL) {
        return 0;
    }

    decoded_bytes = stbi_zlib_decode_buffer((char *)decoded,
                                            (int)total_bytes,
                                            (char const *)(chunk->buffer +
                                                           compressed_offset),
                                            (int)compressed_bytes);
    if (decoded_bytes != (int)total_bytes) {
        sixel_allocator_free(chunk->allocator, decoded);
        return 0;
    }

    if (info->compression == 3u) {
        if (info->depth == 8u) {
            sixel_builtin_psd_decode_prediction_8bit(decoded,
                                                     info->channels,
                                                     info->height,
                                                     row_bytes);
        } else if (info->depth == 16u) {
            if (!sixel_builtin_psd_decode_prediction_16bit(decoded,
                                                           info->channels,
                                                           info->height,
                                                           row_bytes)) {
                sixel_allocator_free(chunk->allocator, decoded);
                return 0;
            }
        } else if (info->depth == 32u) {
            if (!sixel_builtin_psd_decode_prediction_32bit(decoded,
                                                           info->channels,
                                                           info->height,
                                                           row_bytes)) {
                sixel_allocator_free(chunk->allocator, decoded);
                return 0;
            }
        } else {
            sixel_allocator_free(chunk->allocator, decoded);
            return 0;
        }
    }

    *pdecoded = decoded;
    *pplane_bytes = plane_bytes;
    return 1;
}

static int
sixel_builtin_decode_psd_8bit_channel(sixel_chunk_t const *chunk,
                                      sixel_builtin_psd_info_t const *info,
                                      unsigned int target_channel,
                                      unsigned char *dst)
{
    size_t pixel_count;
    size_t plane_bytes;
    size_t total_plane_bytes;
    size_t channel_offset;
    size_t offset;
    size_t table_entries;
    size_t table_bytes;
    unsigned char const *row_table;
    size_t data_cursor;
    unsigned int channel;
    unsigned int row;
    size_t row_length;
    size_t row_index;
    size_t row_offset;
    unsigned char *zip_planar;
    size_t zip_plane_bytes;

    pixel_count = 0u;
    plane_bytes = 0u;
    total_plane_bytes = 0u;
    channel_offset = 0u;
    offset = 0u;
    table_entries = 0u;
    table_bytes = 0u;
    row_table = NULL;
    data_cursor = 0u;
    channel = 0u;
    row = 0u;
    row_length = 0u;
    row_index = 0u;
    row_offset = 0u;
    zip_planar = NULL;
    zip_plane_bytes = 0u;

    if (chunk == NULL || info == NULL || dst == NULL ||
        chunk->buffer == NULL || chunk->allocator == NULL ||
        info->channels == 0u || info->width == 0u || info->height == 0u ||
        target_channel >= info->channels) {
        return 0;
    }

    if ((size_t)info->width > SIXEL_FROMPSD_MAX_DIMENSION ||
        (size_t)info->height > SIXEL_FROMPSD_MAX_DIMENSION) {
        return 0;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return 0;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    plane_bytes = pixel_count;

    if (info->compression == 0u) {
        if (plane_bytes > 0u &&
            (size_t)info->channels > (SIZE_MAX - info->image_data_offset) / plane_bytes) {
            return 0;
        }
        total_plane_bytes = (size_t)info->channels * plane_bytes;
        if (info->image_data_offset > chunk->size ||
            total_plane_bytes > chunk->size - info->image_data_offset) {
            return 0;
        }
        channel_offset = (size_t)target_channel * plane_bytes;
        offset = info->image_data_offset + channel_offset;
        memcpy(dst, chunk->buffer + offset, plane_bytes);
        return 1;
    } else if (info->compression == 1u) {
        if ((size_t)info->height > SIZE_MAX / (size_t)info->channels) {
            return 0;
        }
        table_entries = (size_t)info->height * (size_t)info->channels;
        if (table_entries > SIZE_MAX / 2u) {
            return 0;
        }
        table_bytes = table_entries * 2u;
        if (info->image_data_offset > chunk->size ||
            table_bytes > chunk->size - info->image_data_offset) {
            return 0;
        }
        row_table = chunk->buffer + info->image_data_offset;
        data_cursor = info->image_data_offset + table_bytes;

        for (channel = 0u; channel < info->channels; ++channel) {
            for (row = 0u; row < info->height; ++row) {
                row_index = ((size_t)channel * (size_t)info->height + (size_t)row) * 2u;
                row_length = (size_t)sixel_builtin_read_u16be(row_table + row_index);
                if (data_cursor > chunk->size || row_length > chunk->size - data_cursor) {
                    return 0;
                }
                if (channel == target_channel) {
                    row_offset = (size_t)row * (size_t)info->width;
                    if (!sixel_builtin_psd_unpack_packbits_row(
                            chunk->buffer + data_cursor,
                            row_length,
                            dst + row_offset,
                            (size_t)info->width)) {
                        return 0;
                    }
                }
                data_cursor += row_length;
            }
        }
        return 1;
    } else if (info->compression == 2u || info->compression == 3u) {
        if (!sixel_builtin_psd_decode_zip_planar(chunk,
                                                 info,
                                                 (size_t)info->width,
                                                 &zip_planar,
                                                 &zip_plane_bytes)) {
            return 0;
        }
        if (zip_plane_bytes != plane_bytes) {
            sixel_allocator_free(chunk->allocator, zip_planar);
            return 0;
        }
        memcpy(dst,
               zip_planar + (size_t)target_channel * zip_plane_bytes,
               plane_bytes);
        sixel_allocator_free(chunk->allocator, zip_planar);
        return 1;
    }
    return 0;
}

static int
sixel_builtin_decode_psd_16bit_channel(sixel_chunk_t const *chunk,
                                       sixel_builtin_psd_info_t const *info,
                                       unsigned int target_channel,
                                       uint16_t *dst)
{
    unsigned char const *src;
    unsigned char *plane_bytes_buffer;
    size_t pixel_count;
    size_t plane_bytes;
    size_t row_bytes;
    size_t total_plane_bytes;
    size_t channel_offset;
    size_t offset;
    size_t table_entries;
    size_t table_bytes;
    unsigned char const *row_table;
    size_t data_cursor;
    unsigned int channel;
    unsigned int row;
    size_t row_length;
    size_t row_index;
    size_t row_offset;
    size_t i;
    size_t x;
    unsigned char *zip_planar;
    size_t zip_plane_bytes;
    unsigned char *row_fallback8;

    src = NULL;
    plane_bytes_buffer = NULL;
    pixel_count = 0u;
    plane_bytes = 0u;
    row_bytes = 0u;
    total_plane_bytes = 0u;
    channel_offset = 0u;
    offset = 0u;
    table_entries = 0u;
    table_bytes = 0u;
    row_table = NULL;
    data_cursor = 0u;
    channel = 0u;
    row = 0u;
    row_length = 0u;
    row_index = 0u;
    row_offset = 0u;
    i = 0u;
    x = 0u;
    zip_planar = NULL;
    zip_plane_bytes = 0u;
    row_fallback8 = NULL;

    if (chunk == NULL || info == NULL || dst == NULL ||
        chunk->buffer == NULL || chunk->allocator == NULL ||
        info->channels == 0u || info->width == 0u || info->height == 0u ||
        target_channel >= info->channels) {
        return 0;
    }

    if ((size_t)info->width > SIXEL_FROMPSD_MAX_DIMENSION ||
        (size_t)info->height > SIXEL_FROMPSD_MAX_DIMENSION) {
        return 0;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return 0;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / 2u) {
        return 0;
    }
    plane_bytes = pixel_count * 2u;
    row_bytes = (size_t)info->width * 2u;

    plane_bytes_buffer = (unsigned char *)sixel_allocator_malloc(
        chunk->allocator, plane_bytes);
    if (plane_bytes_buffer == NULL) {
        return 0;
    }

    if (info->compression == 0u) {
        if (plane_bytes > 0u &&
            (size_t)info->channels > (SIZE_MAX - info->image_data_offset) / plane_bytes) {
            goto fail;
        }
        total_plane_bytes = (size_t)info->channels * plane_bytes;
        if (info->image_data_offset > chunk->size ||
            total_plane_bytes > chunk->size - info->image_data_offset) {
            goto fail;
        }
        channel_offset = (size_t)target_channel * plane_bytes;
        offset = info->image_data_offset + channel_offset;
        src = chunk->buffer + offset;
        memcpy(plane_bytes_buffer, src, plane_bytes);
    } else if (info->compression == 1u) {
        if ((size_t)info->height > SIZE_MAX / (size_t)info->channels) {
            goto fail;
        }
        table_entries = (size_t)info->height * (size_t)info->channels;
        if (table_entries > SIZE_MAX / 2u) {
            goto fail;
        }
        table_bytes = table_entries * 2u;
        if (info->image_data_offset > chunk->size ||
            table_bytes > chunk->size - info->image_data_offset) {
            goto fail;
        }
        row_table = chunk->buffer + info->image_data_offset;
        data_cursor = info->image_data_offset + table_bytes;

        for (channel = 0u; channel < info->channels; ++channel) {
            for (row = 0u; row < info->height; ++row) {
                row_index = ((size_t)channel * (size_t)info->height + (size_t)row) * 2u;
                row_length = (size_t)sixel_builtin_read_u16be(row_table + row_index);
                if (data_cursor > chunk->size || row_length > chunk->size - data_cursor) {
                    goto fail;
                }
                if (channel == target_channel) {
                    row_offset = (size_t)row * row_bytes;
                    if (!sixel_builtin_psd_unpack_packbits_row(
                            chunk->buffer + data_cursor,
                            row_length,
                            plane_bytes_buffer + row_offset,
                            row_bytes)) {
                        if ((size_t)info->width > 0u) {
                            if (row_fallback8 == NULL) {
                                row_fallback8 = (unsigned char *)
                                    sixel_allocator_malloc(chunk->allocator,
                                                           (size_t)info->width);
                                if (row_fallback8 == NULL) {
                                    goto fail;
                                }
                            }
                            if (!sixel_builtin_psd_unpack_packbits_row(
                                    chunk->buffer + data_cursor,
                                    row_length,
                                    row_fallback8,
                                    (size_t)info->width)) {
                                goto fail;
                            }
                            for (x = 0u; x < (size_t)info->width; ++x) {
                                plane_bytes_buffer[row_offset + x * 2u] =
                                    row_fallback8[x];
                                plane_bytes_buffer[row_offset + x * 2u + 1u] =
                                    0u;
                            }
                        } else {
                            goto fail;
                        }
                    }
                }
                data_cursor += row_length;
            }
        }
    } else if (info->compression == 2u || info->compression == 3u) {
        if (!sixel_builtin_psd_decode_zip_planar(chunk,
                                                 info,
                                                 row_bytes,
                                                 &zip_planar,
                                                 &zip_plane_bytes)) {
            goto fail;
        }
        if (zip_plane_bytes != plane_bytes) {
            sixel_allocator_free(chunk->allocator, zip_planar);
            goto fail;
        }
        src = zip_planar + (size_t)target_channel * zip_plane_bytes;
        memcpy(plane_bytes_buffer, src, plane_bytes);
        sixel_allocator_free(chunk->allocator, zip_planar);
        zip_planar = NULL;
    } else {
        goto fail;
    }

    for (i = 0u; i < pixel_count; ++i) {
        dst[i] = (uint16_t)(((uint16_t)plane_bytes_buffer[i * 2u] << 8) |
                            (uint16_t)plane_bytes_buffer[i * 2u + 1u]);
    }
    sixel_allocator_free(chunk->allocator, row_fallback8);
    sixel_allocator_free(chunk->allocator, plane_bytes_buffer);
    return 1;

fail:
    sixel_allocator_free(chunk->allocator, row_fallback8);
    sixel_allocator_free(chunk->allocator, zip_planar);
    sixel_allocator_free(chunk->allocator, plane_bytes_buffer);
    return 0;
}

static float
sixel_builtin_psd_decode_be_float32(unsigned char const *src)
{
    uint32_t bits;
    float value;

    bits = 0u;
    value = 0.0f;

    bits = sixel_builtin_read_u32be(src);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static float
sixel_builtin_psd_clamp_alpha_float32(float alpha)
{
    if (alpha != alpha) {
        return 0.0f;
    }
    if (alpha < 0.0f) {
        return 0.0f;
    }
    if (alpha > 1.0f) {
        return 1.0f;
    }
    return alpha;
}

static float
sixel_builtin_psd_clamp_unit_float32(float value)
{
    if (value != value) {
        return 0.0f;
    }
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static unsigned char
sixel_builtin_psd_decode_cmyk_u8(unsigned char value)
{
    /* PSD stores CMYK channels with inverted polarity. */
    return (unsigned char)(0xffu - value);
}

static float
sixel_builtin_psd_decode_cmyk_f32(float value)
{
    /* PSD stores CMYK channels with inverted polarity. */
    return 1.0f - sixel_builtin_psd_clamp_unit_float32(value);
}

static int
sixel_builtin_decode_psd_32bit_channel(sixel_chunk_t const *chunk,
                                       sixel_builtin_psd_info_t const *info,
                                       unsigned int target_channel,
                                       float *dst)
{
    unsigned char const *src;
    unsigned char *plane_bytes_buffer;
    size_t pixel_count;
    size_t plane_bytes;
    size_t row_bytes;
    size_t total_plane_bytes;
    size_t channel_offset;
    size_t offset;
    size_t i;
    unsigned char *zip_planar;
    size_t zip_plane_bytes;

    src = NULL;
    plane_bytes_buffer = NULL;
    pixel_count = 0u;
    plane_bytes = 0u;
    row_bytes = 0u;
    total_plane_bytes = 0u;
    channel_offset = 0u;
    offset = 0u;
    i = 0u;
    zip_planar = NULL;
    zip_plane_bytes = 0u;

    if (chunk == NULL || info == NULL || dst == NULL ||
        chunk->buffer == NULL || chunk->allocator == NULL ||
        info->channels == 0u || info->width == 0u || info->height == 0u ||
        target_channel >= info->channels) {
        return 0;
    }

    if ((size_t)info->width > SIXEL_FROMPSD_MAX_DIMENSION ||
        (size_t)info->height > SIXEL_FROMPSD_MAX_DIMENSION) {
        return 0;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return 0;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / 4u) {
        return 0;
    }
    plane_bytes = pixel_count * 4u;
    row_bytes = (size_t)info->width * 4u;

    plane_bytes_buffer = (unsigned char *)sixel_allocator_malloc(
        chunk->allocator, plane_bytes);
    if (plane_bytes_buffer == NULL) {
        return 0;
    }

    if (info->compression == 0u) {
        if (plane_bytes > 0u &&
            (size_t)info->channels > (SIZE_MAX - info->image_data_offset) / plane_bytes) {
            goto fail;
        }
        total_plane_bytes = (size_t)info->channels * plane_bytes;
        if (info->image_data_offset > chunk->size ||
            total_plane_bytes > chunk->size - info->image_data_offset) {
            goto fail;
        }
        channel_offset = (size_t)target_channel * plane_bytes;
        offset = info->image_data_offset + channel_offset;
        src = chunk->buffer + offset;
        memcpy(plane_bytes_buffer, src, plane_bytes);
    } else if (info->compression == 2u || info->compression == 3u) {
        if (!sixel_builtin_psd_decode_zip_planar(chunk,
                                                 info,
                                                 row_bytes,
                                                 &zip_planar,
                                                 &zip_plane_bytes)) {
            goto fail;
        }
        if (zip_plane_bytes != plane_bytes) {
            sixel_allocator_free(chunk->allocator, zip_planar);
            goto fail;
        }
        src = zip_planar + (size_t)target_channel * zip_plane_bytes;
        memcpy(plane_bytes_buffer, src, plane_bytes);
        sixel_allocator_free(chunk->allocator, zip_planar);
        zip_planar = NULL;
    } else {
        goto fail;
    }

    for (i = 0u; i < pixel_count; ++i) {
        dst[i] = sixel_builtin_psd_decode_be_float32(plane_bytes_buffer + i * 4u);
    }
    sixel_allocator_free(chunk->allocator, plane_bytes_buffer);
    return 1;

fail:
    sixel_allocator_free(chunk->allocator, zip_planar);
    sixel_allocator_free(chunk->allocator, plane_bytes_buffer);
    return 0;
}

static int
sixel_builtin_decode_psd_bitmap_channel(sixel_chunk_t const *chunk,
                                        sixel_builtin_psd_info_t const *info,
                                        unsigned int target_channel,
                                        unsigned char *dst,
                                        size_t row_bytes)
{
    size_t plane_bytes;
    size_t total_plane_bytes;
    size_t channel_offset;
    size_t offset;
    size_t table_entries;
    size_t table_bytes;
    unsigned char const *row_table;
    size_t data_cursor;
    unsigned int channel;
    unsigned int row;
    size_t row_length;
    size_t row_index;
    size_t row_offset;
    unsigned char *zip_planar;
    size_t zip_plane_bytes;

    plane_bytes = 0u;
    total_plane_bytes = 0u;
    channel_offset = 0u;
    offset = 0u;
    table_entries = 0u;
    table_bytes = 0u;
    row_table = NULL;
    data_cursor = 0u;
    channel = 0u;
    row = 0u;
    row_length = 0u;
    row_index = 0u;
    row_offset = 0u;
    zip_planar = NULL;
    zip_plane_bytes = 0u;

    if (chunk == NULL || info == NULL || dst == NULL ||
        chunk->buffer == NULL || chunk->allocator == NULL ||
        info->channels == 0u || info->width == 0u || info->height == 0u ||
        target_channel >= info->channels || row_bytes == 0u) {
        return 0;
    }
    if (row_bytes > SIZE_MAX / (size_t)info->height) {
        return 0;
    }
    plane_bytes = row_bytes * (size_t)info->height;

    if (info->compression == 0u) {
        if (plane_bytes > 0u &&
            (size_t)info->channels > (SIZE_MAX - info->image_data_offset) / plane_bytes) {
            return 0;
        }
        total_plane_bytes = (size_t)info->channels * plane_bytes;
        if (info->image_data_offset > chunk->size ||
            total_plane_bytes > chunk->size - info->image_data_offset) {
            return 0;
        }
        channel_offset = (size_t)target_channel * plane_bytes;
        offset = info->image_data_offset + channel_offset;
        memcpy(dst, chunk->buffer + offset, plane_bytes);
        return 1;
    } else if (info->compression == 1u) {
        if ((size_t)info->height > SIZE_MAX / (size_t)info->channels) {
            return 0;
        }
        table_entries = (size_t)info->height * (size_t)info->channels;
        if (table_entries > SIZE_MAX / 2u) {
            return 0;
        }
        table_bytes = table_entries * 2u;
        if (info->image_data_offset > chunk->size ||
            table_bytes > chunk->size - info->image_data_offset) {
            return 0;
        }
        row_table = chunk->buffer + info->image_data_offset;
        data_cursor = info->image_data_offset + table_bytes;

        for (channel = 0u; channel < info->channels; ++channel) {
            for (row = 0u; row < info->height; ++row) {
                row_index = ((size_t)channel * (size_t)info->height + (size_t)row) * 2u;
                row_length = (size_t)sixel_builtin_read_u16be(row_table + row_index);
                if (data_cursor > chunk->size || row_length > chunk->size - data_cursor) {
                    return 0;
                }
                if (channel == target_channel) {
                    row_offset = (size_t)row * row_bytes;
                    if (!sixel_builtin_psd_unpack_packbits_row(
                            chunk->buffer + data_cursor,
                            row_length,
                            dst + row_offset,
                            row_bytes)) {
                        return 0;
                    }
                }
                data_cursor += row_length;
            }
        }
        return 1;
    } else if (info->compression == 2u || info->compression == 3u) {
        if (!sixel_builtin_psd_decode_zip_planar(chunk,
                                                 info,
                                                 row_bytes,
                                                 &zip_planar,
                                                 &zip_plane_bytes)) {
            return 0;
        }
        if (zip_plane_bytes != plane_bytes) {
            sixel_allocator_free(chunk->allocator, zip_planar);
            return 0;
        }
        memcpy(dst,
               zip_planar + (size_t)target_channel * zip_plane_bytes,
               plane_bytes);
        sixel_allocator_free(chunk->allocator, zip_planar);
        return 1;
    }
    return 0;
}

static int
sixel_builtin_decode_psd_8bit_planes(sixel_chunk_t const *chunk,
                                     sixel_builtin_psd_info_t const *info,
                                     int want_alpha,
                                     unsigned char **pplane0,
                                     unsigned char **pplane_alpha)
{
    unsigned char *plane0;
    unsigned char *plane_alpha;
    size_t pixel_count;
    size_t plane_bytes;

    plane0 = NULL;
    plane_alpha = NULL;
    pixel_count = 0u;
    plane_bytes = 0u;
    if (chunk == NULL || info == NULL || pplane0 == NULL || pplane_alpha == NULL ||
        chunk->buffer == NULL || chunk->allocator == NULL ||
        info->channels == 0u || info->width == 0u || info->height == 0u) {
        return 0;
    }

    *pplane0 = NULL;
    *pplane_alpha = NULL;

    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return 0;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    plane_bytes = pixel_count;

    plane0 = (unsigned char *)sixel_allocator_malloc(chunk->allocator, plane_bytes);
    if (plane0 == NULL) {
        return 0;
    }
    if (want_alpha && info->channels >= 2u) {
        plane_alpha = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                              plane_bytes);
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane0);
            return 0;
        }
    }

    if (!sixel_builtin_decode_psd_8bit_channel(chunk, info, 0u, plane0)) {
        goto fail;
    }
    if (plane_alpha != NULL &&
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 1u, plane_alpha)) {
        goto fail;
    }

    *pplane0 = plane0;
    *pplane_alpha = plane_alpha;
    return 1;

fail:
    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane0);
    return 0;
}

SIXELSTATUS
sixel_builtin_decode_psd_bitmap_1bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    unsigned char *plane0;
    unsigned char *plane_alpha;
    unsigned char *rgb;
    unsigned char *transparent_mask;
    size_t row_bytes;
    size_t plane_bytes;
    size_t pixel_count;
    size_t y;
    size_t x;
    size_t row_offset;
    size_t pixel_offset;
    int want_alpha;
    int preserve_alpha;
    int bit;
    int alpha;
    int r;
    int g;
    int b;
    int blend_with_bg;

    plane0 = NULL;
    plane_alpha = NULL;
    rgb = NULL;
    transparent_mask = NULL;
    row_bytes = 0u;
    plane_bytes = 0u;
    pixel_count = 0u;
    y = 0u;
    x = 0u;
    row_offset = 0u;
    pixel_offset = 0u;
    want_alpha = 0;
    preserve_alpha = 0;
    bit = 0;
    alpha = 0;
    r = 0;
    g = 0;
    b = 0;
    blend_with_bg = 0;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode != 0u || info->depth != 1u ||
        info->compression > 3u || info->channels < 1u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    row_bytes = ((size_t)info->width + 7u) / 8u;
    if (row_bytes == 0u || row_bytes > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    plane_bytes = row_bytes * (size_t)info->height;

    plane0 = (unsigned char *)sixel_allocator_malloc(chunk->allocator, plane_bytes);
    if (plane0 == NULL) {
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    preserve_alpha = (bgcolor == NULL && info->channels >= 2u) ? 1 : 0;
    want_alpha = info->channels >= 2u ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                              plane_bytes);
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane0);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane0);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }
    /* Alpha blending requires a concrete background color. */
    blend_with_bg = (plane_alpha != NULL && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane0);
        return SIXEL_BAD_ARGUMENT;
    }

    if (!sixel_builtin_decode_psd_bitmap_channel(chunk,
                                                 info,
                                                 0u,
                                                 plane0,
                                                 row_bytes) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_bitmap_channel(chunk,
                                                  info,
                                                  1u,
                                                  plane_alpha,
                                                  row_bytes))) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane0);
        sixel_helper_set_additional_message(
            "builtin PSD: malformed compressed channel stream");
        return SIXEL_STBI_ERROR;
    }

    rgb = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count * 3u);
    if (rgb == NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane0);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (y = 0u; y < (size_t)info->height; ++y) {
        row_offset = y * row_bytes;
        for (x = 0u; x < (size_t)info->width; ++x) {
            pixel_offset = y * (size_t)info->width + x;
            bit = (plane0[row_offset + (x >> 3u)] &
                   (unsigned char)(1u << (7u - (x & 7u)))) ? 1 : 0;
            /* PSD Bitmap mode stores 0=white, 1=black. */
            r = bit ? 0 : 0xff;
            g = r;
            b = r;
            if (plane_alpha != NULL) {
                alpha = (plane_alpha[row_offset + (x >> 3u)] &
                         (unsigned char)(1u << (7u - (x & 7u)))) ? 0xff : 0;
                if (blend_with_bg != 0) {
                    r = (r * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
                    g = (g * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
                    b = (b * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
                } else {
                    r = (r * alpha) >> 8;
                    g = (g * alpha) >> 8;
                    b = (b * alpha) >> 8;
                    if (transparent_mask != NULL) {
                        transparent_mask[pixel_offset] = alpha == 0 ? 1u : 0u;
                    }
                }
            }
            rgb[pixel_offset * 3u + 0u] = (unsigned char)r;
            rgb[pixel_offset * 3u + 1u] = (unsigned char)g;
            rgb[pixel_offset * 3u + 2u] = (unsigned char)b;
        }
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane0);

    *ppixels = rgb;
    if (transparent_mask != NULL && ptransparent_mask != NULL) {
        *ptransparent_mask = transparent_mask;
        transparent_mask = NULL;
    }
    if (transparent_mask != NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
    }
    if (ptransparent_mask_size != NULL && preserve_alpha != 0) {
        *ptransparent_mask_size = pixel_count;
    }
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_builtin_decode_psd_gray_or_indexed_8bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    unsigned char const *palette_data;
    unsigned char *plane0;
    unsigned char *plane_alpha;
    unsigned char *rgb;
    unsigned char *transparent_mask;
    size_t pixel_count;
    size_t i;
    int want_alpha;
    int preserve_alpha;
    unsigned char idx;
    unsigned char gray;
    unsigned char alpha;
    int r;
    int g;
    int b;
    int blend_with_bg;

    palette_data = NULL;
    plane0 = NULL;
    plane_alpha = NULL;
    rgb = NULL;
    transparent_mask = NULL;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    preserve_alpha = 0;
    idx = 0u;
    gray = 0u;
    alpha = 0u;
    r = 0;
    g = 0;
    b = 0;
    blend_with_bg = 0;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if ((info->color_mode != 1u &&
         info->color_mode != 2u &&
         info->color_mode != 8u) ||
        info->depth != 8u || info->compression > 3u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (info->color_mode == 2u && info->color_mode_data_length < 768u) {
        sixel_helper_set_additional_message(
            "builtin PSD: indexed palette data is too short");
        return SIXEL_STBI_ERROR;
    }

    preserve_alpha = (bgcolor == NULL && info->channels >= 2u) ? 1 : 0;
    want_alpha = info->channels >= 2u ? 1 : 0;
    /* Alpha blending requires a concrete background color. */
    blend_with_bg = (want_alpha != 0 && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_builtin_decode_psd_8bit_planes(chunk,
                                              info,
                                              want_alpha,
                                              &plane0,
                                              &plane_alpha)) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed compressed channel stream");
        return SIXEL_STBI_ERROR;
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane0);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }

    rgb = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count * 3u);
    if (rgb == NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane0);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (info->color_mode == 2u) {
        palette_data = chunk->buffer + info->color_mode_data_offset;
    }

    for (i = 0u; i < pixel_count; ++i) {
        if (info->color_mode == 1u || info->color_mode == 8u) {
            gray = plane0[i];
            r = (int)gray;
            g = (int)gray;
            b = (int)gray;
        } else {
            idx = plane0[i];
            r = (int)palette_data[idx];
            g = (int)palette_data[256u + idx];
            b = (int)palette_data[512u + idx];
        }
        if (plane_alpha != NULL) {
            alpha = plane_alpha[i];
            if (blend_with_bg != 0) {
                r = (r * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
                g = (g * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
                b = (b * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
            } else {
                r = (r * alpha) >> 8;
                g = (g * alpha) >> 8;
                b = (b * alpha) >> 8;
                if (transparent_mask != NULL) {
                    transparent_mask[i] = alpha == 0u ? 1u : 0u;
                }
            }
        }
        rgb[i * 3u + 0u] = (unsigned char)r;
        rgb[i * 3u + 1u] = (unsigned char)g;
        rgb[i * 3u + 2u] = (unsigned char)b;
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane0);

    *ppixels = rgb;
    if (transparent_mask != NULL && ptransparent_mask != NULL) {
        *ptransparent_mask = transparent_mask;
        transparent_mask = NULL;
    }
    if (transparent_mask != NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
    }
    if (ptransparent_mask_size != NULL && preserve_alpha != 0) {
        *ptransparent_mask_size = pixel_count;
    }
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_builtin_decode_psd_gray_or_duotone_16bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    uint16_t *plane0;
    uint16_t *plane_alpha;
    float *rgbf32;
    unsigned char *transparent_mask;
    size_t pixel_count;
    size_t i;
    int want_alpha;
    int preserve_alpha;
    float gray_f;
    float alpha_f;
    float one_minus_alpha;
    float bg_r;
    float bg_g;
    float bg_b;

    plane0 = NULL;
    plane_alpha = NULL;
    rgbf32 = NULL;
    transparent_mask = NULL;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    preserve_alpha = 0;
    gray_f = 0.0f;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    bg_r = 0.0f;
    bg_g = 0.0f;
    bg_b = 0.0f;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if ((info->color_mode != 1u && info->color_mode != 8u) ||
        info->depth != 16u || info->compression > 3u || info->channels < 1u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (pixel_count > SIZE_MAX / sizeof(uint16_t)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    plane0 = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                pixel_count * sizeof(uint16_t));
    if (plane0 == NULL) {
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    preserve_alpha = (bgcolor == NULL && info->channels >= 2u) ? 1 : 0;
    want_alpha = info->channels >= 2u ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                         pixel_count *
                                                         sizeof(uint16_t));
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane0);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane0);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }
    if (!sixel_builtin_decode_psd_16bit_channel(chunk, info, 0u, plane0) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_16bit_channel(chunk,
                                                 info,
                                                 1u,
                                                 plane_alpha))) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane0);
        sixel_helper_set_additional_message(
            "builtin PSD: malformed compressed channel stream");
        return SIXEL_STBI_ERROR;
    }

    rgbf32 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * 3u * sizeof(float));
    if (rgbf32 == NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane0);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (plane_alpha != NULL && preserve_alpha == 0) {
        bg_r = (float)bgcolor[0] / 255.0f;
        bg_g = (float)bgcolor[1] / 255.0f;
        bg_b = (float)bgcolor[2] / 255.0f;
    }
    for (i = 0u; i < pixel_count; ++i) {
        gray_f = (float)plane0[i] / 65535.0f;
        if (plane_alpha != NULL) {
            alpha_f = (float)plane_alpha[i] / 65535.0f;
            if (preserve_alpha != 0) {
                rgbf32[i * 3u + 0u] = gray_f * alpha_f;
                rgbf32[i * 3u + 1u] = gray_f * alpha_f;
                rgbf32[i * 3u + 2u] = gray_f * alpha_f;
                transparent_mask[i] = plane_alpha[i] == 0u ? 1u : 0u;
            } else {
                one_minus_alpha = 1.0f - alpha_f;
                rgbf32[i * 3u + 0u] = gray_f * alpha_f
                    + bg_r * one_minus_alpha;
                rgbf32[i * 3u + 1u] = gray_f * alpha_f
                    + bg_g * one_minus_alpha;
                rgbf32[i * 3u + 2u] = gray_f * alpha_f
                    + bg_b * one_minus_alpha;
            }
        } else {
            rgbf32[i * 3u + 0u] = gray_f;
            rgbf32[i * 3u + 1u] = gray_f;
            rgbf32[i * 3u + 2u] = gray_f;
        }
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane0);

    *ppixels = (unsigned char *)rgbf32;
    if (transparent_mask != NULL && ptransparent_mask != NULL) {
        *ptransparent_mask = transparent_mask;
        transparent_mask = NULL;
    }
    if (transparent_mask != NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
    }
    if (ptransparent_mask_size != NULL && preserve_alpha != 0) {
        *ptransparent_mask_size = pixel_count;
    }
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_builtin_decode_psd_gray_or_duotone_32bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    float *plane0;
    float *plane_alpha;
    float *rgbf32;
    unsigned char *transparent_mask;
    size_t pixel_count;
    size_t i;
    int want_alpha;
    int preserve_alpha;
    float gray_f;
    float alpha_f;
    float one_minus_alpha;
    float bg_r;
    float bg_g;
    float bg_b;

    plane0 = NULL;
    plane_alpha = NULL;
    rgbf32 = NULL;
    transparent_mask = NULL;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    preserve_alpha = 0;
    gray_f = 0.0f;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    bg_r = 0.0f;
    bg_g = 0.0f;
    bg_b = 0.0f;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if ((info->color_mode != 1u && info->color_mode != 8u) ||
        info->depth != 32u ||
        (info->compression != 0u &&
         info->compression != 2u &&
         info->compression != 3u) ||
        info->channels < 1u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(float)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    plane0 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * sizeof(float));
    if (plane0 == NULL) {
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    preserve_alpha = (bgcolor == NULL && info->channels >= 2u) ? 1 : 0;
    want_alpha = info->channels >= 2u ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (float *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count *
                                                      sizeof(float));
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane0);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane0);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }
    if (!sixel_builtin_decode_psd_32bit_channel(chunk, info, 0u, plane0) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_32bit_channel(chunk,
                                                 info,
                                                 1u,
                                                 plane_alpha))) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane0);
        sixel_helper_set_additional_message(
            "builtin PSD: malformed compressed channel stream");
        return SIXEL_STBI_ERROR;
    }

    rgbf32 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * 3u * sizeof(float));
    if (rgbf32 == NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane0);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (plane_alpha != NULL && preserve_alpha == 0) {
        bg_r = (float)bgcolor[0] / 255.0f;
        bg_g = (float)bgcolor[1] / 255.0f;
        bg_b = (float)bgcolor[2] / 255.0f;
    }
    for (i = 0u; i < pixel_count; ++i) {
        gray_f = plane0[i];
        if (plane_alpha != NULL) {
            alpha_f = sixel_builtin_psd_clamp_alpha_float32(plane_alpha[i]);
            if (preserve_alpha != 0) {
                rgbf32[i * 3u + 0u] = gray_f * alpha_f;
                rgbf32[i * 3u + 1u] = gray_f * alpha_f;
                rgbf32[i * 3u + 2u] = gray_f * alpha_f;
                transparent_mask[i] = alpha_f <= 0.0f ? 1u : 0u;
            } else {
                one_minus_alpha = 1.0f - alpha_f;
                rgbf32[i * 3u + 0u] = gray_f * alpha_f
                    + bg_r * one_minus_alpha;
                rgbf32[i * 3u + 1u] = gray_f * alpha_f
                    + bg_g * one_minus_alpha;
                rgbf32[i * 3u + 2u] = gray_f * alpha_f
                    + bg_b * one_minus_alpha;
            }
        } else {
            rgbf32[i * 3u + 0u] = gray_f;
            rgbf32[i * 3u + 1u] = gray_f;
            rgbf32[i * 3u + 2u] = gray_f;
        }
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane0);

    *ppixels = (unsigned char *)rgbf32;
    if (transparent_mask != NULL && ptransparent_mask != NULL) {
        *ptransparent_mask = transparent_mask;
        transparent_mask = NULL;
    }
    if (transparent_mask != NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
    }
    if (ptransparent_mask_size != NULL && preserve_alpha != 0) {
        *ptransparent_mask_size = pixel_count;
    }
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_builtin_decode_psd_cmyk_8bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char const *icc_profile,
    size_t icc_profile_length,
    int *pcms_applied,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    unsigned char *plane_c;
    unsigned char *plane_m;
    unsigned char *plane_y;
    unsigned char *plane_k;
    unsigned char *plane_alpha;
    unsigned char *cmyk;
    unsigned char *rgb;
    float *rgbf32;
    unsigned char *transparent_mask;
    size_t pixel_count;
    size_t i;
    int want_alpha;
    int preserve_alpha;
    int use_cms;
    int cms_applied;
    int c;
    int m;
    int y;
    int k;
    int alpha;
    int r;
    int g;
    int b;

    plane_c = NULL;
    plane_m = NULL;
    plane_y = NULL;
    plane_k = NULL;
    plane_alpha = NULL;
    cmyk = NULL;
    rgb = NULL;
    rgbf32 = NULL;
    transparent_mask = NULL;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    preserve_alpha = 0;
    use_cms = 0;
    cms_applied = 0;
    c = 0;
    m = 0;
    y = 0;
    k = 0;
    alpha = 0;
    r = 0;
    g = 0;
    b = 0;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || pcms_applied == NULL ||
        chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *pcms_applied = 0;
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode != 4u || info->depth != 8u ||
        info->compression > 3u || info->channels < 4u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    use_cms = (icc_profile != NULL && icc_profile_length > 0u) ? 1 : 0;

    plane_c = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_m = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_y = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_k = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    if (plane_c == NULL || plane_m == NULL || plane_y == NULL || plane_k == NULL) {
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        sixel_allocator_free(chunk->allocator, plane_k);
        sixel_allocator_free(chunk->allocator, plane_y);
        sixel_allocator_free(chunk->allocator, plane_m);
        sixel_allocator_free(chunk->allocator, plane_c);
        return SIXEL_BAD_ALLOCATION;
    }

    preserve_alpha = (bgcolor == NULL && info->channels >= 5u) ? 1 : 0;
    want_alpha = info->channels >= 5u ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                              pixel_count);
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane_k);
            sixel_allocator_free(chunk->allocator, plane_y);
            sixel_allocator_free(chunk->allocator, plane_m);
            sixel_allocator_free(chunk->allocator, plane_c);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane_k);
            sixel_allocator_free(chunk->allocator, plane_y);
            sixel_allocator_free(chunk->allocator, plane_m);
            sixel_allocator_free(chunk->allocator, plane_c);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }

    if (!sixel_builtin_decode_psd_8bit_channel(chunk, info, 0u, plane_c) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 1u, plane_m) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 2u, plane_y) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 3u, plane_k) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_8bit_channel(chunk, info, 4u, plane_alpha))) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_k);
        sixel_allocator_free(chunk->allocator, plane_y);
        sixel_allocator_free(chunk->allocator, plane_m);
        sixel_allocator_free(chunk->allocator, plane_c);
        sixel_helper_set_additional_message(
            "builtin PSD: malformed compressed channel stream");
        return SIXEL_STBI_ERROR;
    }

    if (use_cms) {
        if (pixel_count > SIZE_MAX / 4u) {
            sixel_allocator_free(chunk->allocator, transparent_mask);
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane_k);
            sixel_allocator_free(chunk->allocator, plane_y);
            sixel_allocator_free(chunk->allocator, plane_m);
            sixel_allocator_free(chunk->allocator, plane_c);
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        cmyk = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count * 4u);
        rgbf32 = (float *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * 3u * sizeof(float));
        if (cmyk == NULL || rgbf32 == NULL) {
            sixel_allocator_free(chunk->allocator, rgbf32);
            sixel_allocator_free(chunk->allocator, cmyk);
            sixel_allocator_free(chunk->allocator, transparent_mask);
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane_k);
            sixel_allocator_free(chunk->allocator, plane_y);
            sixel_allocator_free(chunk->allocator, plane_m);
            sixel_allocator_free(chunk->allocator, plane_c);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }

        for (i = 0u; i < pixel_count; ++i) {
            cmyk[i * 4u + 0u] = sixel_builtin_psd_decode_cmyk_u8(plane_c[i]);
            cmyk[i * 4u + 1u] = sixel_builtin_psd_decode_cmyk_u8(plane_m[i]);
            cmyk[i * 4u + 2u] = sixel_builtin_psd_decode_cmyk_u8(plane_y[i]);
            cmyk[i * 4u + 3u] = sixel_builtin_psd_decode_cmyk_u8(plane_k[i]);
        }

        {
            sixel_cms_profile_t *src_profile;
            sixel_cms_profile_t *dst_profile;
            sixel_cms_transform_t *transform;

            src_profile = NULL;
            dst_profile = NULL;
            transform = NULL;
            src_profile = sixel_cms_open_profile_from_mem(icc_profile,
                                                          icc_profile_length);
            if (src_profile != NULL) {
                dst_profile = sixel_cms_create_srgb_profile();
                if (dst_profile != NULL) {
                    transform = sixel_cms_create_transform(
                        src_profile,
                        SIXEL_CMS_PIXELFORMAT_CMYK_8,
                        dst_profile,
                        SIXEL_CMS_PIXELFORMAT_RGB_F32,
                        SIXEL_CMS_TRANSFORM_DEFAULT);
                }
                if (transform != NULL &&
                    sixel_cms_do_transform(transform,
                                           cmyk,
                                           rgbf32,
                                           pixel_count)) {
                    cms_applied = 1;
                }
            }
            sixel_cms_delete_transform(transform);
            sixel_cms_close_profile(dst_profile);
            sixel_cms_close_profile(src_profile);
        }
        sixel_allocator_free(chunk->allocator, cmyk);
        cmyk = NULL;

        if (cms_applied) {
            if (plane_alpha != NULL) {
                float bg_r;
                float bg_g;
                float bg_b;
                float alpha_f;
                float one_minus_alpha;

                bg_r = 0.0f;
                bg_g = 0.0f;
                bg_b = 0.0f;
                if (preserve_alpha == 0) {
                    bg_r = (float)bgcolor[0] / 255.0f;
                    bg_g = (float)bgcolor[1] / 255.0f;
                    bg_b = (float)bgcolor[2] / 255.0f;
                }
                for (i = 0u; i < pixel_count; ++i) {
                    alpha_f = (float)plane_alpha[i] / 255.0f;
                    if (preserve_alpha != 0) {
                        rgbf32[i * 3u + 0u] *= alpha_f;
                        rgbf32[i * 3u + 1u] *= alpha_f;
                        rgbf32[i * 3u + 2u] *= alpha_f;
                        transparent_mask[i] = alpha_f <= 0.0f ? 1u : 0u;
                    } else {
                        one_minus_alpha = 1.0f - alpha_f;
                        rgbf32[i * 3u + 0u] = rgbf32[i * 3u + 0u]
                            * alpha_f + bg_r * one_minus_alpha;
                        rgbf32[i * 3u + 1u] = rgbf32[i * 3u + 1u]
                            * alpha_f + bg_g * one_minus_alpha;
                        rgbf32[i * 3u + 2u] = rgbf32[i * 3u + 2u]
                            * alpha_f + bg_b * one_minus_alpha;
                    }
                }
            }
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane_k);
            sixel_allocator_free(chunk->allocator, plane_y);
            sixel_allocator_free(chunk->allocator, plane_m);
            sixel_allocator_free(chunk->allocator, plane_c);

            *pcms_applied = 1;
            *ppixels = (unsigned char *)rgbf32;
            if (transparent_mask != NULL && ptransparent_mask != NULL) {
                *ptransparent_mask = transparent_mask;
                transparent_mask = NULL;
            }
            if (transparent_mask != NULL) {
                sixel_allocator_free(chunk->allocator, transparent_mask);
            }
            if (ptransparent_mask_size != NULL && preserve_alpha != 0) {
                *ptransparent_mask_size = pixel_count;
            }
            *pwidth = (int)info->width;
            *pheight = (int)info->height;
            *ppixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
            return SIXEL_OK;
        }
        sixel_allocator_free(chunk->allocator, rgbf32);
        rgbf32 = NULL;
    }

    rgb = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count * 3u);
    if (rgb == NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_k);
        sixel_allocator_free(chunk->allocator, plane_y);
        sixel_allocator_free(chunk->allocator, plane_m);
        sixel_allocator_free(chunk->allocator, plane_c);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0u; i < pixel_count; ++i) {
        c = (int)sixel_builtin_psd_decode_cmyk_u8(plane_c[i]);
        m = (int)sixel_builtin_psd_decode_cmyk_u8(plane_m[i]);
        y = (int)sixel_builtin_psd_decode_cmyk_u8(plane_y[i]);
        k = (int)sixel_builtin_psd_decode_cmyk_u8(plane_k[i]);
        r = ((0xff - c) * (0xff - k) + 0x7f) / 0xff;
        g = ((0xff - m) * (0xff - k) + 0x7f) / 0xff;
        b = ((0xff - y) * (0xff - k) + 0x7f) / 0xff;
        if (plane_alpha != NULL) {
            alpha = (int)plane_alpha[i];
            if (preserve_alpha != 0) {
                r = (r * alpha) >> 8;
                g = (g * alpha) >> 8;
                b = (b * alpha) >> 8;
                transparent_mask[i] = alpha == 0 ? 1u : 0u;
            } else {
                r = (r * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
                g = (g * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
                b = (b * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
            }
        }
        rgb[i * 3u + 0u] = (unsigned char)r;
        rgb[i * 3u + 1u] = (unsigned char)g;
        rgb[i * 3u + 2u] = (unsigned char)b;
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_k);
    sixel_allocator_free(chunk->allocator, plane_y);
    sixel_allocator_free(chunk->allocator, plane_m);
    sixel_allocator_free(chunk->allocator, plane_c);

    *ppixels = rgb;
    if (transparent_mask != NULL && ptransparent_mask != NULL) {
        *ptransparent_mask = transparent_mask;
        transparent_mask = NULL;
    }
    if (transparent_mask != NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
    }
    if (ptransparent_mask_size != NULL && preserve_alpha != 0) {
        *ptransparent_mask_size = pixel_count;
    }
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    *pcms_applied = 0;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_builtin_decode_psd_rgb_8bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    unsigned char *plane_r;
    unsigned char *plane_g;
    unsigned char *plane_b;
    unsigned char *plane_alpha;
    unsigned char *rgb;
    unsigned char *transparent_mask;
    size_t pixel_count;
    size_t i;
    int want_alpha;
    int preserve_alpha;
    int alpha;
    int r;
    int g;
    int b;

    plane_r = NULL;
    plane_g = NULL;
    plane_b = NULL;
    plane_alpha = NULL;
    rgb = NULL;
    transparent_mask = NULL;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    preserve_alpha = 0;
    alpha = 0;
    r = 0;
    g = 0;
    b = 0;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode != 3u || info->depth != 8u ||
        info->compression > 3u || info->channels < 3u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    plane_r = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_g = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_b = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    if (plane_r == NULL || plane_g == NULL || plane_b == NULL) {
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    preserve_alpha = (bgcolor == NULL && info->channels >= 4u) ? 1 : 0;
    want_alpha = info->channels >= 4u ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                              pixel_count);
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane_b);
            sixel_allocator_free(chunk->allocator, plane_g);
            sixel_allocator_free(chunk->allocator, plane_r);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane_b);
            sixel_allocator_free(chunk->allocator, plane_g);
            sixel_allocator_free(chunk->allocator, plane_r);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }

    if (!sixel_builtin_decode_psd_8bit_channel(chunk, info, 0u, plane_r) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 1u, plane_g) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 2u, plane_b) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_8bit_channel(chunk, info, 3u, plane_alpha))) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        sixel_helper_set_additional_message(
            "builtin PSD: malformed compressed channel stream");
        return SIXEL_STBI_ERROR;
    }

    rgb = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count * 3u);
    if (rgb == NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0u; i < pixel_count; ++i) {
        r = (int)plane_r[i];
        g = (int)plane_g[i];
        b = (int)plane_b[i];
        if (plane_alpha != NULL) {
            alpha = (int)plane_alpha[i];
            if (preserve_alpha != 0) {
                r = (r * alpha) >> 8;
                g = (g * alpha) >> 8;
                b = (b * alpha) >> 8;
                transparent_mask[i] = alpha == 0 ? 1u : 0u;
            } else {
                r = (r * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
                g = (g * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
                b = (b * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
            }
        }
        rgb[i * 3u + 0u] = (unsigned char)r;
        rgb[i * 3u + 1u] = (unsigned char)g;
        rgb[i * 3u + 2u] = (unsigned char)b;
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_g);
    sixel_allocator_free(chunk->allocator, plane_r);

    *ppixels = rgb;
    if (transparent_mask != NULL && ptransparent_mask != NULL) {
        *ptransparent_mask = transparent_mask;
        transparent_mask = NULL;
    }
    if (transparent_mask != NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
    }
    if (ptransparent_mask_size != NULL && preserve_alpha != 0) {
        *ptransparent_mask_size = pixel_count;
    }
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_builtin_decode_psd_rgb_16bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    uint16_t *plane_r;
    uint16_t *plane_g;
    uint16_t *plane_b;
    uint16_t *plane_alpha;
    float *rgbf32;
    unsigned char *transparent_mask;
    size_t pixel_count;
    size_t i;
    int want_alpha;
    int preserve_alpha;
    float r_f;
    float g_f;
    float b_f;
    float alpha_f;
    float one_minus_alpha;
    float bg_r;
    float bg_g;
    float bg_b;

    plane_r = NULL;
    plane_g = NULL;
    plane_b = NULL;
    plane_alpha = NULL;
    rgbf32 = NULL;
    transparent_mask = NULL;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    preserve_alpha = 0;
    r_f = 0.0f;
    g_f = 0.0f;
    b_f = 0.0f;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    bg_r = 0.0f;
    bg_g = 0.0f;
    bg_b = 0.0f;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode != 3u || info->depth != 16u ||
        info->compression > 3u || info->channels < 3u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(uint16_t)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    plane_r = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * sizeof(uint16_t));
    plane_g = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * sizeof(uint16_t));
    plane_b = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * sizeof(uint16_t));
    if (plane_r == NULL || plane_g == NULL || plane_b == NULL) {
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    preserve_alpha = (bgcolor == NULL && info->channels >= 4u) ? 1 : 0;
    want_alpha = info->channels >= 4u ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                         pixel_count *
                                                         sizeof(uint16_t));
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane_b);
            sixel_allocator_free(chunk->allocator, plane_g);
            sixel_allocator_free(chunk->allocator, plane_r);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane_b);
            sixel_allocator_free(chunk->allocator, plane_g);
            sixel_allocator_free(chunk->allocator, plane_r);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }

    if (!sixel_builtin_decode_psd_16bit_channel(chunk, info, 0u, plane_r) ||
        !sixel_builtin_decode_psd_16bit_channel(chunk, info, 1u, plane_g) ||
        !sixel_builtin_decode_psd_16bit_channel(chunk, info, 2u, plane_b) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_16bit_channel(chunk,
                                                 info,
                                                 3u,
                                                 plane_alpha))) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        sixel_helper_set_additional_message(
            "builtin PSD: malformed compressed channel stream");
        return SIXEL_STBI_ERROR;
    }

    rgbf32 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * 3u * sizeof(float));
    if (rgbf32 == NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (plane_alpha != NULL && preserve_alpha == 0) {
        bg_r = (float)bgcolor[0] / 255.0f;
        bg_g = (float)bgcolor[1] / 255.0f;
        bg_b = (float)bgcolor[2] / 255.0f;
    }
    for (i = 0u; i < pixel_count; ++i) {
        r_f = (float)plane_r[i] / 65535.0f;
        g_f = (float)plane_g[i] / 65535.0f;
        b_f = (float)plane_b[i] / 65535.0f;
        if (plane_alpha != NULL) {
            alpha_f = (float)plane_alpha[i] / 65535.0f;
            if (preserve_alpha != 0) {
                r_f *= alpha_f;
                g_f *= alpha_f;
                b_f *= alpha_f;
                transparent_mask[i] = plane_alpha[i] == 0u ? 1u : 0u;
            } else {
                one_minus_alpha = 1.0f - alpha_f;
                r_f = r_f * alpha_f + bg_r * one_minus_alpha;
                g_f = g_f * alpha_f + bg_g * one_minus_alpha;
                b_f = b_f * alpha_f + bg_b * one_minus_alpha;
            }
        }
        rgbf32[i * 3u + 0u] = r_f;
        rgbf32[i * 3u + 1u] = g_f;
        rgbf32[i * 3u + 2u] = b_f;
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_g);
    sixel_allocator_free(chunk->allocator, plane_r);

    *ppixels = (unsigned char *)rgbf32;
    if (transparent_mask != NULL && ptransparent_mask != NULL) {
        *ptransparent_mask = transparent_mask;
        transparent_mask = NULL;
    }
    if (transparent_mask != NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
    }
    if (ptransparent_mask_size != NULL && preserve_alpha != 0) {
        *ptransparent_mask_size = pixel_count;
    }
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_builtin_decode_psd_rgb_32bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    float *plane_r;
    float *plane_g;
    float *plane_b;
    float *plane_alpha;
    float *rgbf32;
    unsigned char *transparent_mask;
    size_t pixel_count;
    size_t i;
    int want_alpha;
    int preserve_alpha;
    float alpha_f;
    float one_minus_alpha;
    float bg_r;
    float bg_g;
    float bg_b;

    plane_r = NULL;
    plane_g = NULL;
    plane_b = NULL;
    plane_alpha = NULL;
    rgbf32 = NULL;
    transparent_mask = NULL;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    preserve_alpha = 0;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    bg_r = 0.0f;
    bg_g = 0.0f;
    bg_b = 0.0f;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode != 3u || info->depth != 32u ||
        (info->compression != 0u &&
         info->compression != 2u &&
         info->compression != 3u) ||
        info->channels < 3u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(float)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    plane_r = (float *)sixel_allocator_malloc(chunk->allocator,
                                              pixel_count * sizeof(float));
    plane_g = (float *)sixel_allocator_malloc(chunk->allocator,
                                              pixel_count * sizeof(float));
    plane_b = (float *)sixel_allocator_malloc(chunk->allocator,
                                              pixel_count * sizeof(float));
    if (plane_r == NULL || plane_g == NULL || plane_b == NULL) {
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    preserve_alpha = (bgcolor == NULL && info->channels >= 4u) ? 1 : 0;
    want_alpha = info->channels >= 4u ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (float *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count *
                                                      sizeof(float));
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane_b);
            sixel_allocator_free(chunk->allocator, plane_g);
            sixel_allocator_free(chunk->allocator, plane_r);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane_b);
            sixel_allocator_free(chunk->allocator, plane_g);
            sixel_allocator_free(chunk->allocator, plane_r);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }

    if (!sixel_builtin_decode_psd_32bit_channel(chunk, info, 0u, plane_r) ||
        !sixel_builtin_decode_psd_32bit_channel(chunk, info, 1u, plane_g) ||
        !sixel_builtin_decode_psd_32bit_channel(chunk, info, 2u, plane_b) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_32bit_channel(chunk,
                                                 info,
                                                 3u,
                                                 plane_alpha))) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        sixel_helper_set_additional_message(
            "builtin PSD: malformed compressed channel stream");
        return SIXEL_STBI_ERROR;
    }

    rgbf32 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * 3u * sizeof(float));
    if (rgbf32 == NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (plane_alpha != NULL && preserve_alpha == 0) {
        bg_r = (float)bgcolor[0] / 255.0f;
        bg_g = (float)bgcolor[1] / 255.0f;
        bg_b = (float)bgcolor[2] / 255.0f;
    }
    for (i = 0u; i < pixel_count; ++i) {
        if (plane_alpha != NULL) {
            alpha_f = sixel_builtin_psd_clamp_alpha_float32(plane_alpha[i]);
            if (preserve_alpha != 0) {
                rgbf32[i * 3u + 0u] = plane_r[i] * alpha_f;
                rgbf32[i * 3u + 1u] = plane_g[i] * alpha_f;
                rgbf32[i * 3u + 2u] = plane_b[i] * alpha_f;
                transparent_mask[i] = alpha_f <= 0.0f ? 1u : 0u;
            } else {
                one_minus_alpha = 1.0f - alpha_f;
                rgbf32[i * 3u + 0u] = plane_r[i] * alpha_f
                    + bg_r * one_minus_alpha;
                rgbf32[i * 3u + 1u] = plane_g[i] * alpha_f
                    + bg_g * one_minus_alpha;
                rgbf32[i * 3u + 2u] = plane_b[i] * alpha_f
                    + bg_b * one_minus_alpha;
            }
        } else {
            rgbf32[i * 3u + 0u] = plane_r[i];
            rgbf32[i * 3u + 1u] = plane_g[i];
            rgbf32[i * 3u + 2u] = plane_b[i];
        }
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_g);
    sixel_allocator_free(chunk->allocator, plane_r);

    *ppixels = (unsigned char *)rgbf32;
    if (transparent_mask != NULL && ptransparent_mask != NULL) {
        *ptransparent_mask = transparent_mask;
        transparent_mask = NULL;
    }
    if (transparent_mask != NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
    }
    if (ptransparent_mask_size != NULL && preserve_alpha != 0) {
        *ptransparent_mask_size = pixel_count;
    }
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    return SIXEL_OK;
}

static float
sixel_builtin_psd_lab_decode_l(unsigned char value)
{
    return (float)value / 255.0f;
}

static float
sixel_builtin_psd_lab_decode_ab(unsigned char value)
{
    return ((float)(int)value - 128.0f) / 128.0f;
}

static float
sixel_builtin_psd_lab_clamp_ab(float value)
{
    if (value < -1.5f) {
        return -1.5f;
    }
    if (value > 1.5f) {
        return 1.5f;
    }
    return value;
}

static SIXELSTATUS
sixel_builtin_psd_rgb_bgcolor_to_cielab(unsigned char const *bgcolor,
                                        float out_lab[3])
{
    SIXELSTATUS status;
    unsigned char lab8[3];

    status = SIXEL_FALSE;
    if (bgcolor == NULL || out_lab == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    lab8[0] = bgcolor[0];
    lab8[1] = bgcolor[1];
    lab8[2] = bgcolor[2];
    status = sixel_helper_convert_colorspace(lab8,
                                             3u,
                                             SIXEL_PIXELFORMAT_RGB888,
                                             SIXEL_COLORSPACE_GAMMA,
                                             SIXEL_COLORSPACE_CIELAB);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    out_lab[0] = sixel_builtin_psd_lab_decode_l(lab8[0]);
    out_lab[1] = sixel_builtin_psd_lab_decode_ab(lab8[1]);
    out_lab[2] = sixel_builtin_psd_lab_decode_ab(lab8[2]);

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_psd_rgb_bgcolor_to_linear(unsigned char const *bgcolor,
                                        float out_linear[3])
{
    SIXELSTATUS status;
    float rgbf32[3];

    status = SIXEL_FALSE;
    rgbf32[0] = 0.0f;
    rgbf32[1] = 0.0f;
    rgbf32[2] = 0.0f;
    if (bgcolor == NULL || out_linear == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    rgbf32[0] = (float)bgcolor[0] / 255.0f;
    rgbf32[1] = (float)bgcolor[1] / 255.0f;
    rgbf32[2] = (float)bgcolor[2] / 255.0f;
    status = sixel_helper_convert_colorspace((unsigned char *)rgbf32,
                                             sizeof(rgbf32),
                                             SIXEL_PIXELFORMAT_RGBFLOAT32,
                                             SIXEL_COLORSPACE_GAMMA,
                                             SIXEL_COLORSPACE_LINEAR);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    out_linear[0] = rgbf32[0];
    out_linear[1] = rgbf32[1];
    out_linear[2] = rgbf32[2];
    return SIXEL_OK;
}

static float
sixel_builtin_psd_lab_decode_l32(float value)
{
    if (value != value) {
        return 0.0f;
    }
    if (value < 0.0f || value > 1.5f) {
        value /= 100.0f;
    }
    return sixel_builtin_psd_clamp_unit_float32(value);
}

static float
sixel_builtin_psd_lab_decode_ab32(float value)
{
    if (value != value) {
        return 0.0f;
    }
    if (value < -1.5f || value > 1.5f) {
        value /= 128.0f;
    }
    return sixel_builtin_psd_lab_clamp_ab(value);
}

SIXELSTATUS
sixel_builtin_decode_psd_lab_8bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    SIXELSTATUS status;
    unsigned char *plane_l;
    unsigned char *plane_a;
    unsigned char *plane_b;
    unsigned char *plane_alpha;
    unsigned char *transparent_mask;
    float *lab;
    float bg_lab[3];
    size_t pixel_count;
    size_t i;
    int want_alpha;
    int preserve_alpha;
    int alpha;
    float l;
    float a;
    float b;

    status = SIXEL_FALSE;
    plane_l = NULL;
    plane_a = NULL;
    plane_b = NULL;
    plane_alpha = NULL;
    transparent_mask = NULL;
    lab = NULL;
    bg_lab[0] = 0.0f;
    bg_lab[1] = 0.0f;
    bg_lab[2] = 0.0f;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    preserve_alpha = 0;
    alpha = 0;
    l = 0.0f;
    a = 0.0f;
    b = 0.0f;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode != 9u || info->depth != 8u ||
        info->compression > 3u || info->channels < 3u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    plane_l = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_a = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_b = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    if (plane_l == NULL || plane_a == NULL || plane_b == NULL) {
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_a);
        sixel_allocator_free(chunk->allocator, plane_l);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    preserve_alpha = (bgcolor == NULL && info->channels >= 4u) ? 1 : 0;
    want_alpha = info->channels >= 4u ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                              pixel_count);
        if (plane_alpha == NULL) {
            sixel_allocator_free(chunk->allocator, plane_b);
            sixel_allocator_free(chunk->allocator, plane_a);
            sixel_allocator_free(chunk->allocator, plane_l);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane_b);
            sixel_allocator_free(chunk->allocator, plane_a);
            sixel_allocator_free(chunk->allocator, plane_l);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }

    if (!sixel_builtin_decode_psd_8bit_channel(chunk, info, 0u, plane_l) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 1u, plane_a) ||
        !sixel_builtin_decode_psd_8bit_channel(chunk, info, 2u, plane_b) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_8bit_channel(chunk, info, 3u, plane_alpha))) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_a);
        sixel_allocator_free(chunk->allocator, plane_l);
        sixel_helper_set_additional_message(
            "builtin PSD: malformed compressed channel stream");
        return SIXEL_STBI_ERROR;
    }

    if (plane_alpha != NULL && preserve_alpha == 0) {
        status = sixel_builtin_psd_rgb_bgcolor_to_cielab(bgcolor, bg_lab);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(chunk->allocator, transparent_mask);
            sixel_allocator_free(chunk->allocator, plane_alpha);
            sixel_allocator_free(chunk->allocator, plane_b);
            sixel_allocator_free(chunk->allocator, plane_a);
            sixel_allocator_free(chunk->allocator, plane_l);
            return status;
        }
    }

    lab = (float *)sixel_allocator_malloc(chunk->allocator, pixel_count * 3u * sizeof(float));
    if (lab == NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
        sixel_allocator_free(chunk->allocator, plane_alpha);
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_a);
        sixel_allocator_free(chunk->allocator, plane_l);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0u; i < pixel_count; ++i) {
        l = sixel_builtin_psd_lab_decode_l(plane_l[i]);
        a = sixel_builtin_psd_lab_decode_ab(plane_a[i]);
        b = sixel_builtin_psd_lab_decode_ab(plane_b[i]);
        if (plane_alpha != NULL) {
            alpha = (int)plane_alpha[i];
            if (preserve_alpha != 0) {
                l = l * (float)alpha / 255.0f;
                a = a * (float)alpha / 255.0f;
                b = b * (float)alpha / 255.0f;
                transparent_mask[i] = alpha == 0 ? 1u : 0u;
            } else {
                l = (l * (float)alpha + bg_lab[0]
                     * (255.0f - (float)alpha)) / 255.0f;
                a = (a * (float)alpha + bg_lab[1]
                     * (255.0f - (float)alpha)) / 255.0f;
                b = (b * (float)alpha + bg_lab[2]
                     * (255.0f - (float)alpha)) / 255.0f;
            }
        }
        lab[i * 3u + 0u] = l;
        lab[i * 3u + 1u] = sixel_builtin_psd_lab_clamp_ab(a);
        lab[i * 3u + 2u] = sixel_builtin_psd_lab_clamp_ab(b);
    }

    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_a);
    sixel_allocator_free(chunk->allocator, plane_l);

    *ppixels = (unsigned char *)lab;
    if (transparent_mask != NULL && ptransparent_mask != NULL) {
        *ptransparent_mask = transparent_mask;
        transparent_mask = NULL;
    }
    if (transparent_mask != NULL) {
        sixel_allocator_free(chunk->allocator, transparent_mask);
    }
    if (ptransparent_mask_size != NULL && preserve_alpha != 0) {
        *ptransparent_mask_size = pixel_count;
    }
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_CIELABFLOAT32;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_builtin_decode_psd_cmyk_32bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char const *icc_profile,
    size_t icc_profile_length,
    int *pcms_applied,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    SIXELSTATUS status;
    float *plane_c;
    float *plane_m;
    float *plane_y;
    float *plane_k;
    float *plane_alpha;
    float *cmyk;
    float *rgb_linear;
    unsigned char *transparent_mask;
    sixel_cms_profile_t *src_profile;
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;
    size_t pixel_count;
    size_t i;
    int want_alpha;
    int preserve_alpha;
    int use_cms;
    int cms_applied;
    float bg_linear[3];
    float alpha_f;
    float one_minus_alpha;
    float c;
    float m;
    float y;
    float k;

    status = SIXEL_FALSE;
    plane_c = NULL;
    plane_m = NULL;
    plane_y = NULL;
    plane_k = NULL;
    plane_alpha = NULL;
    cmyk = NULL;
    rgb_linear = NULL;
    transparent_mask = NULL;
    src_profile = NULL;
    dst_profile = NULL;
    transform = NULL;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    preserve_alpha = 0;
    use_cms = 0;
    cms_applied = 0;
    bg_linear[0] = 0.0f;
    bg_linear[1] = 0.0f;
    bg_linear[2] = 0.0f;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    c = 0.0f;
    m = 0.0f;
    y = 0.0f;
    k = 0.0f;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || pcms_applied == NULL ||
        chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *pcms_applied = 0;
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode != 4u || info->depth != 32u ||
        (info->compression != 0u &&
         info->compression != 2u &&
         info->compression != 3u) ||
        info->channels < 4u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(float) ||
        pixel_count > SIZE_MAX / (3u * sizeof(float)) ||
        pixel_count > SIZE_MAX / (4u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    use_cms = (icc_profile != NULL && icc_profile_length > 0u) ? 1 : 0;

    plane_c = (float *)sixel_allocator_malloc(chunk->allocator,
                                              pixel_count * sizeof(float));
    plane_m = (float *)sixel_allocator_malloc(chunk->allocator,
                                              pixel_count * sizeof(float));
    plane_y = (float *)sixel_allocator_malloc(chunk->allocator,
                                              pixel_count * sizeof(float));
    plane_k = (float *)sixel_allocator_malloc(chunk->allocator,
                                              pixel_count * sizeof(float));
    if (plane_c == NULL || plane_m == NULL || plane_y == NULL || plane_k == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup;
    }

    preserve_alpha = (bgcolor == NULL && info->channels >= 5u) ? 1 : 0;
    want_alpha = info->channels >= 5u ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (float *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count *
                                                      sizeof(float));
        if (plane_alpha == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup;
        }
    }

    if (!sixel_builtin_decode_psd_32bit_channel(chunk, info, 0u, plane_c) ||
        !sixel_builtin_decode_psd_32bit_channel(chunk, info, 1u, plane_m) ||
        !sixel_builtin_decode_psd_32bit_channel(chunk, info, 2u, plane_y) ||
        !sixel_builtin_decode_psd_32bit_channel(chunk, info, 3u, plane_k) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_32bit_channel(chunk,
                                                 info,
                                                 4u,
                                                 plane_alpha))) {
        status = SIXEL_STBI_ERROR;
        sixel_helper_set_additional_message(
            "builtin PSD: malformed compressed channel stream");
        goto cleanup;
    }

    rgb_linear = (float *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * 3u * sizeof(float));
    if (rgb_linear == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup;
    }

    if (use_cms) {
        cmyk = (float *)sixel_allocator_malloc(chunk->allocator,
                                               pixel_count * 4u * sizeof(float));
        if (cmyk == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup;
        }
        for (i = 0u; i < pixel_count; ++i) {
            cmyk[i * 4u + 0u] = sixel_builtin_psd_decode_cmyk_f32(plane_c[i]);
            cmyk[i * 4u + 1u] = sixel_builtin_psd_decode_cmyk_f32(plane_m[i]);
            cmyk[i * 4u + 2u] = sixel_builtin_psd_decode_cmyk_f32(plane_y[i]);
            cmyk[i * 4u + 3u] = sixel_builtin_psd_decode_cmyk_f32(plane_k[i]);
        }

        src_profile = sixel_cms_open_profile_from_mem(icc_profile,
                                                      icc_profile_length);
        if (src_profile != NULL) {
            dst_profile = sixel_cms_create_linear_srgb_profile();
        }
        if (src_profile != NULL && dst_profile != NULL) {
            transform = sixel_cms_create_transform(
                src_profile,
                SIXEL_CMS_PIXELFORMAT_CMYK_F32,
                dst_profile,
                SIXEL_CMS_PIXELFORMAT_RGB_F32,
                SIXEL_CMS_TRANSFORM_DEFAULT);
        }
        if (transform != NULL &&
            sixel_cms_do_transform(transform, cmyk, rgb_linear, pixel_count)) {
            cms_applied = 1;
        }
    }

    if (!cms_applied) {
        for (i = 0u; i < pixel_count; ++i) {
            c = sixel_builtin_psd_decode_cmyk_f32(plane_c[i]);
            m = sixel_builtin_psd_decode_cmyk_f32(plane_m[i]);
            y = sixel_builtin_psd_decode_cmyk_f32(plane_y[i]);
            k = sixel_builtin_psd_decode_cmyk_f32(plane_k[i]);
            rgb_linear[i * 3u + 0u] = (1.0f - c) * (1.0f - k);
            rgb_linear[i * 3u + 1u] = (1.0f - m) * (1.0f - k);
            rgb_linear[i * 3u + 2u] = (1.0f - y) * (1.0f - k);
        }
        if (!sixel_cms_convert_rgbf32_gamma_to_linear(rgb_linear, pixel_count)) {
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }
    }

    if (plane_alpha != NULL) {
        if (preserve_alpha != 0) {
            for (i = 0u; i < pixel_count; ++i) {
                alpha_f = sixel_builtin_psd_clamp_alpha_float32(plane_alpha[i]);
                rgb_linear[i * 3u + 0u] *= alpha_f;
                rgb_linear[i * 3u + 1u] *= alpha_f;
                rgb_linear[i * 3u + 2u] *= alpha_f;
                transparent_mask[i] = alpha_f <= 0.0f ? 1u : 0u;
            }
        } else {
        status = sixel_builtin_psd_rgb_bgcolor_to_linear(bgcolor, bg_linear);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        for (i = 0u; i < pixel_count; ++i) {
            alpha_f = sixel_builtin_psd_clamp_alpha_float32(plane_alpha[i]);
            one_minus_alpha = 1.0f - alpha_f;
            rgb_linear[i * 3u + 0u] =
                rgb_linear[i * 3u + 0u] * alpha_f + bg_linear[0] * one_minus_alpha;
            rgb_linear[i * 3u + 1u] =
                rgb_linear[i * 3u + 1u] * alpha_f + bg_linear[1] * one_minus_alpha;
            rgb_linear[i * 3u + 2u] =
                rgb_linear[i * 3u + 2u] * alpha_f + bg_linear[2] * one_minus_alpha;
        }
        }
    }

    *pcms_applied = cms_applied;
    *ppixels = (unsigned char *)rgb_linear;
    if (transparent_mask != NULL && ptransparent_mask != NULL) {
        *ptransparent_mask = transparent_mask;
        transparent_mask = NULL;
    }
    if (ptransparent_mask_size != NULL && preserve_alpha != 0) {
        *ptransparent_mask_size = pixel_count;
    }
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    status = SIXEL_OK;
    rgb_linear = NULL;

cleanup:
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(dst_profile);
    sixel_cms_close_profile(src_profile);
    sixel_allocator_free(chunk->allocator, cmyk);
    sixel_allocator_free(chunk->allocator, rgb_linear);
    sixel_allocator_free(chunk->allocator, transparent_mask);
    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_k);
    sixel_allocator_free(chunk->allocator, plane_y);
    sixel_allocator_free(chunk->allocator, plane_m);
    sixel_allocator_free(chunk->allocator, plane_c);
    return status;
}

SIXELSTATUS
sixel_builtin_decode_psd_lab_32bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    SIXELSTATUS status;
    float *plane_l;
    float *plane_a;
    float *plane_b;
    float *plane_alpha;
    unsigned char *transparent_mask;
    float *lab;
    float bg_lab[3];
    size_t pixel_count;
    size_t i;
    int want_alpha;
    int preserve_alpha;
    float alpha_f;
    float one_minus_alpha;
    float l;
    float a;
    float b;

    status = SIXEL_FALSE;
    plane_l = NULL;
    plane_a = NULL;
    plane_b = NULL;
    plane_alpha = NULL;
    transparent_mask = NULL;
    lab = NULL;
    bg_lab[0] = 0.0f;
    bg_lab[1] = 0.0f;
    bg_lab[2] = 0.0f;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    preserve_alpha = 0;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    l = 0.0f;
    a = 0.0f;
    b = 0.0f;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode != 9u || info->depth != 32u ||
        (info->compression != 0u &&
         info->compression != 2u &&
         info->compression != 3u) ||
        info->channels < 3u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(float) ||
        pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    plane_l = (float *)sixel_allocator_malloc(chunk->allocator,
                                              pixel_count * sizeof(float));
    plane_a = (float *)sixel_allocator_malloc(chunk->allocator,
                                              pixel_count * sizeof(float));
    plane_b = (float *)sixel_allocator_malloc(chunk->allocator,
                                              pixel_count * sizeof(float));
    if (plane_l == NULL || plane_a == NULL || plane_b == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_lab32;
    }

    preserve_alpha = (bgcolor == NULL && info->channels >= 4u) ? 1 : 0;
    want_alpha = info->channels >= 4u ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (float *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count *
                                                      sizeof(float));
        if (plane_alpha == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_lab32;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_lab32;
        }
    }

    if (!sixel_builtin_decode_psd_32bit_channel(chunk, info, 0u, plane_l) ||
        !sixel_builtin_decode_psd_32bit_channel(chunk, info, 1u, plane_a) ||
        !sixel_builtin_decode_psd_32bit_channel(chunk, info, 2u, plane_b) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_32bit_channel(chunk,
                                                 info,
                                                 3u,
                                                 plane_alpha))) {
        status = SIXEL_STBI_ERROR;
        sixel_helper_set_additional_message(
            "builtin PSD: malformed compressed channel stream");
        goto cleanup_lab32;
    }

    if (plane_alpha != NULL && preserve_alpha == 0) {
        status = sixel_builtin_psd_rgb_bgcolor_to_cielab(bgcolor, bg_lab);
        if (SIXEL_FAILED(status)) {
            goto cleanup_lab32;
        }
    }

    lab = (float *)sixel_allocator_malloc(chunk->allocator,
                                          pixel_count * 3u * sizeof(float));
    if (lab == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_lab32;
    }

    for (i = 0u; i < pixel_count; ++i) {
        l = sixel_builtin_psd_lab_decode_l32(plane_l[i]);
        a = sixel_builtin_psd_lab_decode_ab32(plane_a[i]);
        b = sixel_builtin_psd_lab_decode_ab32(plane_b[i]);
        if (plane_alpha != NULL) {
            alpha_f = sixel_builtin_psd_clamp_alpha_float32(plane_alpha[i]);
            if (preserve_alpha != 0) {
                l *= alpha_f;
                a *= alpha_f;
                b *= alpha_f;
                transparent_mask[i] = alpha_f <= 0.0f ? 1u : 0u;
            } else {
                one_minus_alpha = 1.0f - alpha_f;
                l = l * alpha_f + bg_lab[0] * one_minus_alpha;
                a = a * alpha_f + bg_lab[1] * one_minus_alpha;
                b = b * alpha_f + bg_lab[2] * one_minus_alpha;
            }
        }
        lab[i * 3u + 0u] = sixel_builtin_psd_clamp_unit_float32(l);
        lab[i * 3u + 1u] = sixel_builtin_psd_lab_clamp_ab(a);
        lab[i * 3u + 2u] = sixel_builtin_psd_lab_clamp_ab(b);
    }

    *ppixels = (unsigned char *)lab;
    if (transparent_mask != NULL && ptransparent_mask != NULL) {
        *ptransparent_mask = transparent_mask;
        transparent_mask = NULL;
    }
    if (ptransparent_mask_size != NULL && preserve_alpha != 0) {
        *ptransparent_mask_size = pixel_count;
    }
    *pwidth = (int)info->width;
    *pheight = (int)info->height;
    *ppixelformat = SIXEL_PIXELFORMAT_CIELABFLOAT32;
    status = SIXEL_OK;
    lab = NULL;

cleanup_lab32:
    sixel_allocator_free(chunk->allocator, lab);
    sixel_allocator_free(chunk->allocator, transparent_mask);
    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_a);
    sixel_allocator_free(chunk->allocator, plane_l);
    return status;
}

/*
 * Convert palette entries into RGB triplets while resolving alpha.
 *
 * Notes:
 * - RGB/RGBA palettes keep channel order as-is.
 * - Grayscale palettes (1 component) are expanded to RGB.
 * - RGBA entries are pre-blended against the background color so PAL8
 *   output stays opaque and matches the libpng backend.
 */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
