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
#include "chunk-view.h"
#include "frompsd-internal.h"
#include "loader-common.h"

int
sixel_builtin_psd_should_try_multilayer_fallback(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info);

static unsigned int
sixel_builtin_psd_hdr_read_u16be(unsigned char const *p);

static size_t
sixel_builtin_psd_hdr_read_u32be_size(unsigned char const *p);

static int
sixel_builtin_psd_has_supported_signature(unsigned char const *buffer,
                                          int *pis_psb_alias)
{
    if (pis_psb_alias != NULL) {
        *pis_psb_alias = 0;
    }
    if (buffer == NULL) {
        return 0;
    }
    if (memcmp(buffer, "8BPS", 4u) == 0) {
        return 1;
    }
    if (memcmp(buffer, "8BPB", 4u) == 0) {
        if (pis_psb_alias != NULL) {
            *pis_psb_alias = 1;
        }
        return 1;
    }
    return 0;
}

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
    int is_psb_alias;

    offset = 0u;
    section_length = 0u;
    resource_end = 0u;
    name_length = 0u;
    data_length = 0u;
    resource_id = 0u;
    is_psb_alias = 0;

    if (buffer == NULL || profile == NULL || profile_length == NULL ||
        size < 34u) {
        return SIXEL_BUILTIN_ICC_EXTRACT_ABSENT;
    }
    *profile = NULL;
    *profile_length = 0u;
    if (!sixel_builtin_psd_has_supported_signature(buffer, &is_psb_alias)) {
        return SIXEL_BUILTIN_ICC_EXTRACT_ABSENT;
    }
    if (is_psb_alias != 0 && sixel_builtin_psd_hdr_read_u16be(buffer + 4u) != 2u) {
        return SIXEL_BUILTIN_ICC_EXTRACT_MALFORMED;
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
sixel_builtin_psd_hdr_read_u16be(unsigned char const *p)
{
    return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

static size_t
sixel_builtin_psd_hdr_read_u32be_size(unsigned char const *p)
{
    return ((size_t)p[0] << 24) |
           ((size_t)p[1] << 16) |
           ((size_t)p[2] << 8) |
           (size_t)p[3];
}

static int
sixel_builtin_psd_hdr_read_u64be_size_checked(unsigned char const *p, size_t *out_value)
{
    uint64_t value;
    uint64_t max_size_value;

    value = 0u;
    max_size_value = 0u;

    if (p == NULL || out_value == NULL) {
        return 0;
    }
    value = ((uint64_t)p[0] << 56) |
            ((uint64_t)p[1] << 48) |
            ((uint64_t)p[2] << 40) |
            ((uint64_t)p[3] << 32) |
            ((uint64_t)p[4] << 24) |
            ((uint64_t)p[5] << 16) |
            ((uint64_t)p[6] << 8) |
            (uint64_t)p[7];
    max_size_value = (uint64_t)SIZE_MAX;
    if (value > max_size_value) {
        return 0;
    }
    *out_value = (size_t)value;
    return 1;
}

static size_t
sixel_builtin_psd_layer_mask_length_field_size(sixel_builtin_psd_info_t const *info)
{
    if (info != NULL && info->version == 2u) {
        return 8u;
    }
    return 4u;
}

static size_t
sixel_builtin_psd_hdr_layer_info_length_field_size(sixel_builtin_psd_info_t const *info)
{
    if (info != NULL && info->version == 2u) {
        return 8u;
    }
    return 4u;
}

static int
sixel_builtin_psd_hdr_read_length_field_checked(unsigned char const *p,
                                            size_t field_size,
                                            size_t *out_value)
{
    if (p == NULL || out_value == NULL) {
        return 0;
    }
    if (field_size == 4u) {
        *out_value = sixel_builtin_psd_hdr_read_u32be_size(p);
        return 1;
    }
    if (field_size == 8u) {
        return sixel_builtin_psd_hdr_read_u64be_size_checked(p, out_value);
    }
    return 0;
}

static size_t
sixel_builtin_psd_hdr_rle_row_length_field_size(sixel_builtin_psd_info_t const *info)
{
    if (info != NULL && info->version == 2u) {
        return 4u;
    }
    return 2u;
}

static int16_t
sixel_builtin_psd_hdr_read_i16be(unsigned char const *p)
{
    return (int16_t)sixel_builtin_psd_hdr_read_u16be(p);
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

static int
sixel_builtin_psd_hdr_is_psb(sixel_builtin_psd_info_t const *info)
{
    return info != NULL && info->version == 2u;
}

static char const *
sixel_builtin_psd_hdr_malformed_layer_mask_length_message(
    sixel_builtin_psd_info_t const *info)
{
    if (sixel_builtin_psd_hdr_is_psb(info)) {
        return "builtin PSD: malformed PSB layer/mask length";
    }
    return "builtin PSD: malformed layer/mask section";
}

static char const *
sixel_builtin_psd_hdr_malformed_layer_info_length_message(
    sixel_builtin_psd_info_t const *info)
{
    if (sixel_builtin_psd_hdr_is_psb(info)) {
        return "builtin PSD: malformed PSB layer info length";
    }
    return "builtin PSD: malformed layer record table";
}

static int
sixel_builtin_psd_hdr_get_layer_info_window(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    size_t *player_info_offset,
    size_t *player_info_length,
    size_t *player_info_end,
    int emit_message)
{
    size_t layer_info_length_field_size;
    size_t section_offset;
    size_t section_end;
    size_t layer_info_offset;
    size_t layer_info_length;

    layer_info_length_field_size = 0u;
    section_offset = 0u;
    section_end = 0u;
    layer_info_offset = 0u;
    layer_info_length = 0u;
    if (chunk == NULL || info == NULL || sixel_chunk_get_buffer(chunk) == NULL ||
        player_info_offset == NULL || player_info_length == NULL ||
        player_info_end == NULL) {
        return 0;
    }

    layer_info_length_field_size =
        sixel_builtin_psd_hdr_layer_info_length_field_size(info);
    if (layer_info_length_field_size == 0u ||
        info->layer_mask_length < layer_info_length_field_size + 2u ||
        info->layer_mask_offset > sixel_chunk_get_size(chunk) ||
        info->layer_mask_length > sixel_chunk_get_size(chunk) - info->layer_mask_offset) {
        if (emit_message != 0) {
            sixel_helper_set_additional_message(
                sixel_builtin_psd_hdr_malformed_layer_mask_length_message(info));
        }
        return 0;
    }
    section_offset = info->layer_mask_offset;
    section_end = section_offset + info->layer_mask_length;
    if (!sixel_builtin_psd_hdr_read_length_field_checked(
            sixel_chunk_get_buffer(chunk) + section_offset,
            layer_info_length_field_size,
            &layer_info_length)) {
        if (emit_message != 0) {
            sixel_helper_set_additional_message(
                sixel_builtin_psd_hdr_malformed_layer_info_length_message(info));
        }
        return 0;
    }
    layer_info_offset = section_offset + layer_info_length_field_size;
    if (layer_info_length < 2u ||
        layer_info_length > section_end - layer_info_offset ||
        layer_info_offset + layer_info_length > sixel_chunk_get_size(chunk)) {
        if (emit_message != 0) {
            sixel_helper_set_additional_message(
                sixel_builtin_psd_hdr_malformed_layer_info_length_message(info));
        }
        return 0;
    }
    *player_info_offset = layer_info_offset;
    *player_info_length = layer_info_length;
    *player_info_end = layer_info_offset + layer_info_length;
    return 1;
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
#define SIXEL_BUILTIN_PSD_LAYER_RECORDS_MALFORMED_MASK (-1)
#define SIXEL_BUILTIN_PSD_LAYER_RECORDS_MALFORMED_INFO (-2)
    size_t layer_info_length;
    size_t layer_info_length_field_size;

    layer_info_length = 0u;
    layer_info_length_field_size = 0u;
    if (chunk == NULL || info == NULL || sixel_chunk_get_buffer(chunk) == NULL) {
        return SIXEL_BUILTIN_PSD_LAYER_RECORDS_MALFORMED_MASK;
    }
    layer_info_length_field_size =
        sixel_builtin_psd_layer_mask_length_field_size(info);
    if (layer_info_length_field_size == 0u) {
        return SIXEL_BUILTIN_PSD_LAYER_RECORDS_MALFORMED_MASK;
    }
    if (info->layer_mask_length == 0u) {
        return 0;
    }
    if (info->layer_mask_offset > sixel_chunk_get_size(chunk) ||
        info->layer_mask_length > sixel_chunk_get_size(chunk) - info->layer_mask_offset) {
        return SIXEL_BUILTIN_PSD_LAYER_RECORDS_MALFORMED_MASK;
    }
    if (info->layer_mask_length < layer_info_length_field_size) {
        return 0;
    }
    if (!sixel_builtin_psd_hdr_read_length_field_checked(
            sixel_chunk_get_buffer(chunk) + info->layer_mask_offset,
            layer_info_length_field_size,
            &layer_info_length)) {
        return SIXEL_BUILTIN_PSD_LAYER_RECORDS_MALFORMED_INFO;
    }
    if (layer_info_length >
        info->layer_mask_length - layer_info_length_field_size) {
        return SIXEL_BUILTIN_PSD_LAYER_RECORDS_MALFORMED_INFO;
    }

    return layer_info_length > 0u ? 1 : 0;
#undef SIXEL_BUILTIN_PSD_LAYER_RECORDS_MALFORMED_MASK
#undef SIXEL_BUILTIN_PSD_LAYER_RECORDS_MALFORMED_INFO
}

static int
sixel_builtin_psd_hdr_compute_row_bytes(sixel_builtin_psd_info_t const *info,
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

static int
sixel_builtin_psd_layer_fallback_supported_by_header(
    sixel_builtin_psd_info_t const *info)
{
    if (info == NULL) {
        return 0;
    }
    if (info->depth != 8u && info->depth != 16u && info->depth != 32u) {
        return 0;
    }
    if ((info->color_mode == 3u && info->channels >= 3u) ||
        ((info->color_mode == 1u || info->color_mode == 8u) &&
         info->channels >= 1u) ||
        (info->color_mode == 9u && info->channels >= 3u) ||
        (info->color_mode == 4u && info->channels >= 4u) ||
        (info->color_mode == 7u &&
         (info->channels == 3u || info->channels == 4u))) {
        return 1;
    }
    return 0;
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
    int allow_layer_fallback;
    size_t row_bytes;
    size_t plane_bytes;
    size_t total_bytes;
    size_t table_entries;
    size_t table_bytes;
    size_t row_length_field_size;
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
    allow_layer_fallback = 0;
    row_bytes = 0u;
    plane_bytes = 0u;
    total_bytes = 0u;
    table_entries = 0u;
    table_bytes = 0u;
    row_length_field_size = 2u;
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

    if (chunk == NULL || info == NULL || sixel_chunk_get_buffer(chunk) == NULL) {
        sixel_builtin_psd_set_message(
            message,
            message_size,
            "builtin PSD: malformed header/metadata");
        return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
    }

    if (info->version != 1u && info->version != 2u) {
        nwrite = snprintf(message,
                          message_size,
                          "builtin PSD: unsupported version (%u; expected 1 or 2)",
                          info->version);
        (void)nwrite;
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

    if (info->image_data_offset > sixel_chunk_get_size(chunk)) {
        sixel_builtin_psd_set_message(
            message,
            message_size,
            "builtin PSD: malformed image data offset");
        return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
    }
    image_data_length = sixel_chunk_get_size(chunk) - info->image_data_offset;
    if (image_data_length == 0u) {
        layer_state = sixel_builtin_psd_has_layer_records(chunk, info);
        if (layer_state < 0) {
            if (layer_state == -2) {
                sixel_builtin_psd_set_message(
                    message,
                    message_size,
                    sixel_builtin_psd_hdr_malformed_layer_info_length_message(info));
            } else {
                sixel_builtin_psd_set_message(
                    message,
                    message_size,
                    sixel_builtin_psd_hdr_malformed_layer_mask_length_message(info));
            }
            return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
        }
        if (layer_state > 0) {
            if (sixel_builtin_psd_layer_fallback_supported_by_header(info)) {
                allow_layer_fallback = 1;
            } else {
                sixel_builtin_psd_set_message(
                    message,
                    message_size,
                    "builtin PSD: unsupported file without "
                    "merged/composite image");
                return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
            }
        }
        if (!allow_layer_fallback) {
            sixel_builtin_psd_set_message(
                message,
                message_size,
                "builtin PSD: malformed image data section");
            return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
        }
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
        } else if (info->depth == 16u) {
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_16BIT;
            skip_icc_conversion = 1;
            colorspace = SIXEL_COLORSPACE_LINEAR;
        } else if (info->depth == 32u) {
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
        if (info->channels == 3u) {
            min_channels = 3u;
            if (info->depth == 8u) {
                decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_RGB_8BIT;
            } else if (info->depth == 16u) {
                decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_RGB_16BIT;
            } else if (info->depth == 32u) {
                decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_RGB_32BIT;
            } else {
                nwrite = snprintf(
                    message,
                    message_size,
                    "builtin PSD: unsupported bit depth (%u) for Multichannel (3ch->RGB)",
                    info->depth);
                (void)nwrite;
                return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
            }
        } else if (info->channels == 4u) {
            min_channels = 4u;
            if (info->depth == 8u) {
                decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_8BIT;
                skip_icc_conversion = 1;
            } else if (info->depth == 16u) {
                decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_16BIT;
                skip_icc_conversion = 1;
                colorspace = SIXEL_COLORSPACE_LINEAR;
            } else if (info->depth == 32u) {
                decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_CMYK_32BIT;
                skip_icc_conversion = 1;
                colorspace = SIXEL_COLORSPACE_LINEAR;
            } else {
                nwrite = snprintf(
                    message,
                    message_size,
                    "builtin PSD: unsupported bit depth (%u) for Multichannel (4ch->CMYK)",
                    info->depth);
                (void)nwrite;
                return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
            }
        } else {
            nwrite = snprintf(
                message,
                message_size,
                "builtin PSD: unsupported Multichannel channel count (%u; expected 3 or 4)",
                info->channels);
            (void)nwrite;
            return SIXEL_BUILTIN_PSD_VALIDATE_UNSUPPORTED;
        }
        break;
    case 9u:
        min_channels = 3u;
        if (info->depth == 8u) {
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_8BIT;
            colorspace = SIXEL_COLORSPACE_CIELAB;
        } else if (info->depth == 16u) {
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_16BIT;
            colorspace = SIXEL_COLORSPACE_CIELAB;
        } else if (info->depth == 32u) {
            decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_LAB_32BIT;
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

    if (!sixel_builtin_psd_hdr_compute_row_bytes(info, &row_bytes) ||
        row_bytes > SIZE_MAX / (size_t)info->height) {
        sixel_builtin_psd_set_message(
            message,
            message_size,
            "builtin PSD: malformed dimensions/depth overflow");
        return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
    }
    plane_bytes = row_bytes * (size_t)info->height;

    if (!allow_layer_fallback) {
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
                layer_state = sixel_builtin_psd_has_layer_records(chunk, info);
                if (layer_state < 0) {
                    if (layer_state == -2) {
                        sixel_builtin_psd_set_message(
                            message,
                            message_size,
                            sixel_builtin_psd_hdr_malformed_layer_info_length_message(info));
                    } else {
                        sixel_builtin_psd_set_message(
                            message,
                            message_size,
                            sixel_builtin_psd_hdr_malformed_layer_mask_length_message(info));
                    }
                    return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
                }
                if (layer_state > 0 &&
                    sixel_builtin_psd_layer_fallback_supported_by_header(info) &&
                    sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
                    allow_layer_fallback = 1;
                } else {
                    sixel_builtin_psd_set_message(
                        message,
                        message_size,
                        "builtin PSD: malformed raw channel stream (too short)");
                    return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
                }
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
            row_length_field_size = sixel_builtin_psd_hdr_rle_row_length_field_size(
                info);
            if (row_length_field_size == 0u ||
                table_entries > SIZE_MAX / row_length_field_size) {
                sixel_builtin_psd_set_message(
                    message,
                    message_size,
                    "builtin PSD: malformed RLE row table overflow");
                return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
            }
            table_bytes = table_entries * row_length_field_size;
            if (table_bytes > image_data_length) {
                layer_state = sixel_builtin_psd_has_layer_records(chunk, info);
                if (layer_state < 0) {
                    if (layer_state == -2) {
                        sixel_builtin_psd_set_message(
                            message,
                            message_size,
                            sixel_builtin_psd_hdr_malformed_layer_info_length_message(info));
                    } else {
                        sixel_builtin_psd_set_message(
                            message,
                            message_size,
                            sixel_builtin_psd_hdr_malformed_layer_mask_length_message(info));
                    }
                    return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
                }
                if (layer_state > 0 &&
                    sixel_builtin_psd_layer_fallback_supported_by_header(info) &&
                    sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
                    allow_layer_fallback = 1;
                } else {
                    sixel_builtin_psd_set_message(
                        message,
                        message_size,
                        "builtin PSD: malformed RLE row table (too short)");
                    return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
                }
            }
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
                             sixel_allocator_t *allocator,
                             sixel_builtin_psd_info_t *info)
{
    unsigned char const *buffer;
    size_t size;
    size_t offset;
    size_t section_length;
    size_t layer_mask_length_field_size;
    int is_psb_alias;

    buffer = NULL;
    size = 0u;
    offset = 0u;
    section_length = 0u;
    layer_mask_length_field_size = 0u;
    is_psb_alias = 0;
    sixel_builtin_psd_trace_reset();
    if (chunk == NULL || allocator == NULL ||
        sixel_chunk_get_buffer(chunk) == NULL || info == NULL ||
        sixel_chunk_get_size(chunk) < 30u) {
        return 0;
    }
    buffer = sixel_chunk_get_buffer(chunk);
    size = sixel_chunk_get_size(chunk);
    if (!sixel_builtin_psd_has_supported_signature(buffer, &is_psb_alias)) {
        return 0;
    }

    memset(info, 0, sizeof(*info));
    info->allocator = allocator;
    info->version = sixel_builtin_psd_hdr_read_u16be(buffer + 4u);
    if (is_psb_alias != 0 && info->version != 2u) {
        return 0;
    }
    info->channels = sixel_builtin_psd_hdr_read_u16be(buffer + 12u);
    info->height = (unsigned int)sixel_builtin_psd_hdr_read_u32be_size(buffer + 14u);
    info->width = (unsigned int)sixel_builtin_psd_hdr_read_u32be_size(buffer + 18u);
    info->depth = sixel_builtin_psd_hdr_read_u16be(buffer + 22u);
    info->color_mode = sixel_builtin_psd_hdr_read_u16be(buffer + 24u);
    layer_mask_length_field_size =
        sixel_builtin_psd_layer_mask_length_field_size(info);

    offset = 26u;
    if (offset + 4u > size) {
        return 0;
    }
    section_length = sixel_builtin_psd_hdr_read_u32be_size(buffer + offset);
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
    section_length = sixel_builtin_psd_hdr_read_u32be_size(buffer + offset);
    offset += 4u;
    if (section_length > size - offset) {
        return 0;
    }
    info->image_resources_offset = offset;
    info->image_resources_length = section_length;
    offset += section_length;

    if (offset + layer_mask_length_field_size > size) {
        if (is_psb_alias != 0 && info->version == 2u) {
            sixel_helper_set_additional_message(
                sixel_builtin_psd_hdr_malformed_layer_mask_length_message(info));
        }
        return 0;
    }
    if (!sixel_builtin_psd_hdr_read_length_field_checked(
            buffer + offset,
            layer_mask_length_field_size,
            &section_length)) {
        if (is_psb_alias != 0 && info->version == 2u) {
            sixel_helper_set_additional_message(
                sixel_builtin_psd_hdr_malformed_layer_mask_length_message(info));
        }
        return 0;
    }
    offset += layer_mask_length_field_size;
    if (section_length > size - offset) {
        if (is_psb_alias != 0 && info->version == 2u) {
            sixel_helper_set_additional_message(
                sixel_builtin_psd_hdr_malformed_layer_mask_length_message(info));
        }
        return 0;
    }
    info->layer_mask_offset = offset;
    info->layer_mask_length = section_length;
    offset += section_length;

    if (offset + 2u > size) {
        return 0;
    }
    info->compression = sixel_builtin_psd_hdr_read_u16be(buffer + offset);
    info->image_data_offset = offset + 2u;

    return 1;
}

int
sixel_builtin_psd_should_try_multilayer_fallback(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info)
{
    size_t layer_info_offset;
    size_t layer_info_length;
    size_t layer_info_end;
    size_t min_layer_count;
    int16_t layer_count_raw;
    size_t layer_count;

    layer_info_offset = 0u;
    layer_info_length = 0u;
    layer_info_end = 0u;
    min_layer_count = 2u;
    layer_count_raw = 0;
    layer_count = 0u;

    if (chunk == NULL || info == NULL || sixel_chunk_get_buffer(chunk) == NULL) {
        return 0;
    }
    if (info->version == 2u) {
        min_layer_count = 1u;
    }
    if (!sixel_builtin_psd_hdr_get_layer_info_window(
            chunk,
            info,
            &layer_info_offset,
            &layer_info_length,
            &layer_info_end,
            0)) {
        return 0;
    }
    layer_count_raw = sixel_builtin_psd_hdr_read_i16be(sixel_chunk_get_buffer(chunk) + layer_info_offset);
    if (layer_count_raw < 0) {
        layer_count = (size_t)(-(int)layer_count_raw);
    } else {
        layer_count = (size_t)layer_count_raw;
    }
    if (layer_count < min_layer_count) {
        return 0;
    }
    if (info->version != 2u) {
        if (layer_count > (SIZE_MAX - 2u) / 18u) {
            return 0;
        }
        if (layer_info_end <= layer_info_offset + 2u + layer_count * 18u) {
            return 0;
        }
    }
    return 1;
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
