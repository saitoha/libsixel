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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "allocator.h"
#include "cms.h"
#include "frompsd.h"
#include "loader-common.h"
#include "pixelformat.h"

#define SIXEL_FROMPSD_MAX_CHANNELS 56u
#define SIXEL_FROMPSD_MAX_DIMENSION 300000u

int stbi_zlib_decode_buffer(char *obuffer,
                            int olen,
                            char const *ibuffer,
                            int ilen);

static unsigned int
sixel_builtin_read_u16be(unsigned char const *p);

static SIXELSTATUS
sixel_builtin_psd_rgb_bgcolor_to_cielab(unsigned char const *bgcolor,
                                        float out_lab[3]);

static SIXELSTATUS
sixel_builtin_psd_rgb_bgcolor_to_linear(unsigned char const *bgcolor,
                                        float out_linear[3]);

static float
sixel_builtin_psd_lab_decode_l(unsigned char value);

static float
sixel_builtin_psd_lab_decode_ab(unsigned char value);

static float
sixel_builtin_psd_lab_clamp_ab(float value);

static float
sixel_builtin_psd_lab_decode_l32(float value);

static float
sixel_builtin_psd_lab_decode_l16(uint16_t value);

static float
sixel_builtin_psd_lab_decode_ab16(uint16_t value);

static float
sixel_builtin_psd_lab_decode_ab32(float value);

static float
sixel_builtin_psd_clamp_alpha_float32(float alpha);

static float
sixel_builtin_psd_decode_cmyk_f32(float value);

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
    if (is_psb_alias != 0 && sixel_builtin_read_u16be(buffer + 4u) != 2u) {
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
sixel_builtin_read_u16be(unsigned char const *p)
{
    return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

static uint16_t
sixel_builtin_read_u16be_as_u16(unsigned char const *p)
{
    return (uint16_t)sixel_builtin_read_u16be(p);
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

static int
sixel_builtin_read_u64be_size_checked(unsigned char const *p, size_t *out_value)
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
sixel_builtin_psd_layer_info_length_field_size(sixel_builtin_psd_info_t const *info)
{
    if (info != NULL && info->version == 2u) {
        return 8u;
    }
    return 4u;
}

static size_t
sixel_builtin_psd_layer_channel_length_field_size(sixel_builtin_psd_info_t const *info)
{
    if (info != NULL && info->version == 2u) {
        return 8u;
    }
    return 4u;
}

static int
sixel_builtin_psd_read_length_field_checked(unsigned char const *p,
                                            size_t field_size,
                                            size_t *out_value)
{
    if (p == NULL || out_value == NULL) {
        return 0;
    }
    if (field_size == 4u) {
        *out_value = sixel_builtin_read_u32be_size(p);
        return 1;
    }
    if (field_size == 8u) {
        return sixel_builtin_read_u64be_size_checked(p, out_value);
    }
    return 0;
}

static size_t
sixel_builtin_psd_rle_row_length_field_size(sixel_builtin_psd_info_t const *info)
{
    if (info != NULL && info->version == 2u) {
        return 4u;
    }
    return 2u;
}

static size_t
sixel_builtin_psd_read_rle_row_length(
    sixel_builtin_psd_info_t const *info,
    unsigned char const *row_table,
    size_t row_index)
{
    if (row_table == NULL) {
        return 0u;
    }
    if (sixel_builtin_psd_rle_row_length_field_size(info) == 4u) {
        return sixel_builtin_read_u32be_size(row_table + row_index);
    }
    return (size_t)sixel_builtin_read_u16be(row_table + row_index);
}

static int16_t
sixel_builtin_read_i16be(unsigned char const *p)
{
    return (int16_t)sixel_builtin_read_u16be(p);
}

static int32_t
sixel_builtin_read_i32be(unsigned char const *p)
{
    return (int32_t)sixel_builtin_read_u32be(p);
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

static int
sixel_builtin_psd_is_psb(sixel_builtin_psd_info_t const *info)
{
    return info != NULL && info->version == 2u;
}

static char const *
sixel_builtin_psd_malformed_layer_mask_length_message(
    sixel_builtin_psd_info_t const *info)
{
    if (sixel_builtin_psd_is_psb(info)) {
        return "builtin PSD: malformed PSB layer/mask length";
    }
    return "builtin PSD: malformed layer/mask section";
}

static char const *
sixel_builtin_psd_malformed_layer_info_length_message(
    sixel_builtin_psd_info_t const *info)
{
    if (sixel_builtin_psd_is_psb(info)) {
        return "builtin PSD: malformed PSB layer info length";
    }
    return "builtin PSD: malformed layer record table";
}

static char const *
sixel_builtin_psd_malformed_layer_channel_length_message(
    sixel_builtin_psd_info_t const *info)
{
    if (sixel_builtin_psd_is_psb(info)) {
        return "builtin PSD: malformed PSB layer channel length";
    }
    return "builtin PSD: malformed layer channel length";
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

static void
sixel_builtin_psd_commit_transparent_mask_output(
    sixel_allocator_t *allocator,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    unsigned char **ptransparent_mask_buffer,
    size_t pixel_count,
    int preserve_alpha)
{
    if (ptransparent_mask != NULL &&
        ptransparent_mask_buffer != NULL &&
        *ptransparent_mask_buffer != NULL) {
        *ptransparent_mask = *ptransparent_mask_buffer;
        *ptransparent_mask_buffer = NULL;
    }
    if (ptransparent_mask_size != NULL && preserve_alpha != 0) {
        *ptransparent_mask_size = pixel_count;
    }
    if (allocator != NULL &&
        ptransparent_mask_buffer != NULL &&
        *ptransparent_mask_buffer != NULL) {
        sixel_allocator_free(allocator, *ptransparent_mask_buffer);
        *ptransparent_mask_buffer = NULL;
    }
}

static void
sixel_builtin_psd_set_decode_output(
    unsigned char **ppixels,
    int *pwidth,
    int *pheight,
    int *ppixelformat,
    unsigned char *pixels,
    sixel_builtin_psd_info_t const *info,
    int pixelformat)
{
    if (info == NULL) {
        return;
    }
    if (ppixels != NULL) {
        *ppixels = pixels;
    }
    if (pwidth != NULL) {
        *pwidth = (int)info->width;
    }
    if (pheight != NULL) {
        *pheight = (int)info->height;
    }
    if (ppixelformat != NULL) {
        *ppixelformat = pixelformat;
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
#define SIXEL_BUILTIN_PSD_LAYER_RECORDS_MALFORMED_MASK (-1)
#define SIXEL_BUILTIN_PSD_LAYER_RECORDS_MALFORMED_INFO (-2)
    size_t layer_info_length;
    size_t layer_info_length_field_size;

    layer_info_length = 0u;
    layer_info_length_field_size = 0u;
    if (chunk == NULL || info == NULL || chunk->buffer == NULL) {
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
    if (info->layer_mask_offset > chunk->size ||
        info->layer_mask_length > chunk->size - info->layer_mask_offset) {
        return SIXEL_BUILTIN_PSD_LAYER_RECORDS_MALFORMED_MASK;
    }
    if (info->layer_mask_length < layer_info_length_field_size) {
        return 0;
    }
    if (!sixel_builtin_psd_read_length_field_checked(
            chunk->buffer + info->layer_mask_offset,
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

    if (chunk == NULL || info == NULL || chunk->buffer == NULL) {
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
            if (layer_state == -2) {
                sixel_builtin_psd_set_message(
                    message,
                    message_size,
                    sixel_builtin_psd_malformed_layer_info_length_message(info));
            } else {
                sixel_builtin_psd_set_message(
                    message,
                    message_size,
                    sixel_builtin_psd_malformed_layer_mask_length_message(info));
            }
            return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
        }
        if (layer_state > 0) {
            if (((info->depth == 8u ||
                  info->depth == 16u ||
                  info->depth == 32u) &&
                 ((info->color_mode == 3u && info->channels >= 3u) ||
                  ((info->color_mode == 1u || info->color_mode == 8u) &&
                   info->channels >= 1u) ||
                  (info->color_mode == 9u && info->channels >= 3u) ||
                  (info->color_mode == 4u &&
                   (info->depth == 8u ||
                    info->depth == 16u ||
                    info->depth == 32u) &&
                   info->channels >= 4u) ||
                  (info->color_mode == 7u &&
                   ((info->channels == 3u) ||
                    ((info->depth == 8u ||
                      info->depth == 16u ||
                      info->depth == 32u) &&
                     info->channels == 4u)))))) {
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

    if (!sixel_builtin_psd_compute_row_bytes(info, &row_bytes) ||
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
            row_length_field_size = sixel_builtin_psd_rle_row_length_field_size(
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
                sixel_builtin_psd_set_message(
                    message,
                    message_size,
                    "builtin PSD: malformed RLE row table (too short)");
                return SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED;
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
    if (chunk == NULL || chunk->buffer == NULL || info == NULL ||
        chunk->size < 30u) {
        return 0;
    }
    buffer = chunk->buffer;
    size = chunk->size;
    if (!sixel_builtin_psd_has_supported_signature(buffer, &is_psb_alias)) {
        return 0;
    }

    memset(info, 0, sizeof(*info));
    info->version = sixel_builtin_read_u16be(buffer + 4u);
    if (is_psb_alias != 0 && info->version != 2u) {
        return 0;
    }
    info->channels = sixel_builtin_read_u16be(buffer + 12u);
    info->height = (unsigned int)sixel_builtin_read_u32be_size(buffer + 14u);
    info->width = (unsigned int)sixel_builtin_read_u32be_size(buffer + 18u);
    info->depth = sixel_builtin_read_u16be(buffer + 22u);
    info->color_mode = sixel_builtin_read_u16be(buffer + 24u);
    layer_mask_length_field_size =
        sixel_builtin_psd_layer_mask_length_field_size(info);

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

    if (offset + layer_mask_length_field_size > size) {
        if (is_psb_alias != 0 && info->version == 2u) {
            sixel_helper_set_additional_message(
                sixel_builtin_psd_malformed_layer_mask_length_message(info));
        }
        return 0;
    }
    if (!sixel_builtin_psd_read_length_field_checked(
            buffer + offset,
            layer_mask_length_field_size,
            &section_length)) {
        if (is_psb_alias != 0 && info->version == 2u) {
            sixel_helper_set_additional_message(
                sixel_builtin_psd_malformed_layer_mask_length_message(info));
        }
        return 0;
    }
    offset += layer_mask_length_field_size;
    if (section_length > size - offset) {
        if (is_psb_alias != 0 && info->version == 2u) {
            sixel_helper_set_additional_message(
                sixel_builtin_psd_malformed_layer_mask_length_message(info));
        }
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

typedef struct sixel_builtin_psd_layer_channel_entry {
    int16_t channel_id;
    size_t length;
    size_t data_offset;
} sixel_builtin_psd_layer_channel_entry_t;

typedef enum sixel_builtin_psd_layer_blend_mode {
    SIXEL_BUILTIN_PSD_BLEND_NORMAL = 0,
    SIXEL_BUILTIN_PSD_BLEND_DISSOLVE,
    SIXEL_BUILTIN_PSD_BLEND_DARKEN,
    SIXEL_BUILTIN_PSD_BLEND_MULTIPLY,
    SIXEL_BUILTIN_PSD_BLEND_COLOR_BURN,
    SIXEL_BUILTIN_PSD_BLEND_LINEAR_BURN,
    SIXEL_BUILTIN_PSD_BLEND_DARKER_COLOR,
    SIXEL_BUILTIN_PSD_BLEND_LIGHTEN,
    SIXEL_BUILTIN_PSD_BLEND_SCREEN,
    SIXEL_BUILTIN_PSD_BLEND_COLOR_DODGE,
    SIXEL_BUILTIN_PSD_BLEND_LINEAR_DODGE,
    SIXEL_BUILTIN_PSD_BLEND_LIGHTER_COLOR,
    SIXEL_BUILTIN_PSD_BLEND_OVERLAY,
    SIXEL_BUILTIN_PSD_BLEND_SOFT_LIGHT,
    SIXEL_BUILTIN_PSD_BLEND_HARD_LIGHT,
    SIXEL_BUILTIN_PSD_BLEND_VIVID_LIGHT,
    SIXEL_BUILTIN_PSD_BLEND_LINEAR_LIGHT,
    SIXEL_BUILTIN_PSD_BLEND_PIN_LIGHT,
    SIXEL_BUILTIN_PSD_BLEND_HARD_MIX,
    SIXEL_BUILTIN_PSD_BLEND_DIFFERENCE,
    SIXEL_BUILTIN_PSD_BLEND_EXCLUSION,
    SIXEL_BUILTIN_PSD_BLEND_SUBTRACT,
    SIXEL_BUILTIN_PSD_BLEND_DIVIDE,
    SIXEL_BUILTIN_PSD_BLEND_HUE,
    SIXEL_BUILTIN_PSD_BLEND_SATURATION,
    SIXEL_BUILTIN_PSD_BLEND_COLOR,
    SIXEL_BUILTIN_PSD_BLEND_LUMINOSITY
} sixel_builtin_psd_layer_blend_mode_t;

typedef enum sixel_builtin_psd_multilayer_output_kind {
    SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_RGB888 = 0,
    SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_RGBFLOAT32,
    SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_LINEARRGBFLOAT32,
    SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_CIELABFLOAT32,
    SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_CMYK8_DYNAMIC
} sixel_builtin_psd_multilayer_output_kind_t;

typedef enum sixel_builtin_psd_fill_kind {
    SIXEL_BUILTIN_PSD_FILL_NONE = 0,
    SIXEL_BUILTIN_PSD_FILL_SOCO,
    SIXEL_BUILTIN_PSD_FILL_GDFL,
    SIXEL_BUILTIN_PSD_FILL_PTFL
} sixel_builtin_psd_fill_kind_t;

enum {
    SIXEL_BUILTIN_PSD_FILL_STOP_MAX = 8
};

typedef struct sixel_builtin_psd_layer_record {
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
    unsigned int width;
    unsigned int height;
    unsigned int channel_count;
    sixel_builtin_psd_layer_channel_entry_t
        channels[SIXEL_FROMPSD_MAX_CHANNELS];
    char blend_key[5];
    unsigned char opacity;
    unsigned char clipping;
    unsigned char flags;
    int visible;
    int red_channel_index;
    int green_channel_index;
    int blue_channel_index;
    int gray_channel_index;
    int c_channel_index;
    int m_channel_index;
    int y_channel_index;
    int k_channel_index;
    int alpha_channel_index;
    int user_mask_channel_index;
    int real_mask_channel_index;
    int has_non_pixel_payload;
    int has_fill_payload;
    int has_malformed_fill_payload;
    int has_vector_mask;
    int has_layer_effects;
    int has_knockout;
    sixel_builtin_psd_fill_kind_t fill_kind;
    float fill_solid_rgb[3];
    unsigned char fill_gradient_type;
    float fill_gradient_angle_deg;
    float fill_gradient_scale;
    int fill_gradient_reverse;
    size_t fill_gradient_stop_count;
    float fill_gradient_stop_pos[SIXEL_BUILTIN_PSD_FILL_STOP_MAX];
    float fill_gradient_stop_rgb[SIXEL_BUILTIN_PSD_FILL_STOP_MAX][3];
    float fill_gradient_stop_alpha[SIXEL_BUILTIN_PSD_FILL_STOP_MAX];
    unsigned int fill_pattern_tile;
    float fill_pattern_fg_rgb[3];
    float fill_pattern_bg_rgb[3];
} sixel_builtin_psd_layer_record_t;

typedef struct sixel_builtin_psd_layer_model {
    sixel_builtin_psd_layer_record_t *layers;
    size_t layer_count;
} sixel_builtin_psd_layer_model_t;

typedef struct sixel_builtin_psd_layer_buffers {
    float *rgb_linear;
    float *alpha;
    size_t pixel_count;
} sixel_builtin_psd_layer_buffers_t;

static int
sixel_builtin_psd_layer_has_decodable_pixel_channels(
    sixel_builtin_psd_info_t const *info,
    sixel_builtin_psd_layer_record_t const *layer)
{
    if (info == NULL || layer == NULL) {
        return 0;
    }
    if (info->color_mode == 3u ||
        (info->color_mode == 7u && info->channels == 3u)) {
        return layer->red_channel_index >= 0 &&
            layer->green_channel_index >= 0 &&
            layer->blue_channel_index >= 0;
    }
    if (info->color_mode == 1u || info->color_mode == 8u) {
        return layer->gray_channel_index >= 0;
    }
    if (info->color_mode == 4u ||
        (info->color_mode == 7u && info->channels == 4u)) {
        return layer->c_channel_index >= 0 &&
            layer->m_channel_index >= 0 &&
            layer->y_channel_index >= 0 &&
            layer->k_channel_index >= 0;
    }
    if (info->color_mode == 9u) {
        return layer->red_channel_index >= 0 &&
            layer->green_channel_index >= 0 &&
            layer->blue_channel_index >= 0;
    }
    return 0;
}

static void
sixel_builtin_psd_layer_model_init(sixel_builtin_psd_layer_model_t *model)
{
    if (model == NULL) {
        return;
    }
    model->layers = NULL;
    model->layer_count = 0u;
}

static void
sixel_builtin_psd_layer_model_destroy(sixel_allocator_t *allocator,
                                      sixel_builtin_psd_layer_model_t *model)
{
    if (allocator == NULL || model == NULL) {
        return;
    }
    if (model->layers != NULL) {
        sixel_allocator_free(allocator, model->layers);
        model->layers = NULL;
    }
    model->layer_count = 0u;
}

static SIXELSTATUS
sixel_builtin_psd_decode_layer_plane_rgb8(
    unsigned char const *buffer,
    sixel_builtin_psd_layer_channel_entry_t const *channels,
    int channel_index,
    unsigned int width,
    unsigned int height,
    unsigned char *dst);

static SIXELSTATUS
sixel_builtin_psd_decode_layer_plane_rgb8_with_row_length(
    unsigned char const *buffer,
    sixel_builtin_psd_layer_channel_entry_t const *channels,
    int channel_index,
    unsigned int width,
    unsigned int height,
    size_t row_length_field_size,
    unsigned char *dst);

static SIXELSTATUS
sixel_builtin_psd_decode_layer_plane_rgb16(
    unsigned char const *buffer,
    sixel_builtin_psd_layer_channel_entry_t const *channels,
    int channel_index,
    unsigned int width,
    unsigned int height,
    uint16_t *dst);

static SIXELSTATUS
sixel_builtin_psd_decode_layer_plane_rgb16_with_row_length(
    unsigned char const *buffer,
    sixel_builtin_psd_layer_channel_entry_t const *channels,
    int channel_index,
    unsigned int width,
    unsigned int height,
    size_t row_length_field_size,
    uint16_t *dst);

static SIXELSTATUS
sixel_builtin_psd_decode_layer_plane_rgb32(
    unsigned char const *buffer,
    sixel_builtin_psd_layer_channel_entry_t const *channels,
    int channel_index,
    unsigned int width,
    unsigned int height,
    float *dst);

static SIXELSTATUS
sixel_builtin_psd_decode_layer_plane_rgb32_with_row_length(
    unsigned char const *buffer,
    sixel_builtin_psd_layer_channel_entry_t const *channels,
    int channel_index,
    unsigned int width,
    unsigned int height,
    size_t row_length_field_size,
    float *dst);

static float
sixel_builtin_psd_clamp01(float value)
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

static float
sixel_builtin_psd_gamma_to_linear(float value)
{
    value = sixel_builtin_psd_clamp01(value);
    if (value <= 0.04045f) {
        return value / 12.92f;
    }
    return powf((value + 0.055f) / 1.055f, 2.4f);
}

static float
sixel_builtin_psd_linear_to_gamma(float value)
{
    value = sixel_builtin_psd_clamp01(value);
    if (value <= 0.0031308f) {
        return value * 12.92f;
    }
    return 1.055f * powf(value, 1.0f / 2.4f) - 0.055f;
}

static int
sixel_builtin_psd_linear_to_byte(float value)
{
    int scaled;

    scaled = (int)(sixel_builtin_psd_linear_to_gamma(value) * 255.0f + 0.5f);
    if (scaled < 0) {
        return 0;
    }
    if (scaled > 255) {
        return 255;
    }
    return scaled;
}

static int
sixel_builtin_psd_layer_is_non_pixel_key(char const key[5])
{
    /* Common Photoshop adjustment/text/smart-object payload keys. */
    static char const * const keys[] = {
        "TySh", "SoCo", "GdFl", "PtFl", "vibA", "curv", "le\x76l",
        "expA", "hue2", "blnc", "selc", "phfl", "mixr", "clrL",
        "nvrt", "post", "thrs", "grdm", "brit", "blwh", "chnl",
        "lsct", "Lr16", "Lr32", "PlLd", "SoLd", "lnkD", "lnk2"
    };
    size_t i;

    if (key == NULL) {
        return 0;
    }
    for (i = 0u; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        if (memcmp(key, keys[i], 4u) == 0) {
            return 1;
        }
    }
    return 0;
}

static void
sixel_builtin_psd_fill_set_rgb_from_u8(float dst_rgb[3],
                                       unsigned char r,
                                       unsigned char g,
                                       unsigned char b)
{
    if (dst_rgb == NULL) {
        return;
    }
    dst_rgb[0] = sixel_builtin_psd_gamma_to_linear((float)r / 255.0f);
    dst_rgb[1] = sixel_builtin_psd_gamma_to_linear((float)g / 255.0f);
    dst_rgb[2] = sixel_builtin_psd_gamma_to_linear((float)b / 255.0f);
}

static double
sixel_builtin_read_f64be(unsigned char const *p)
{
    union {
        uint64_t u;
        double d;
    } value;

    if (p == NULL) {
        return 0.0;
    }
    value.u = ((uint64_t)p[0] << 56) |
              ((uint64_t)p[1] << 48) |
              ((uint64_t)p[2] << 40) |
              ((uint64_t)p[3] << 32) |
              ((uint64_t)p[4] << 24) |
              ((uint64_t)p[5] << 16) |
              ((uint64_t)p[6] << 8) |
              (uint64_t)p[7];
    return value.d;
}

static int
sixel_builtin_psd_descriptor_skip_unicode_string(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor)
{
    size_t cursor;
    size_t length_u16;

    cursor = 0u;
    length_u16 = 0u;
    if (data == NULL || pcursor == NULL) {
        return 0;
    }
    cursor = *pcursor;
    if (cursor + 4u > data_length) {
        return 0;
    }
    length_u16 = sixel_builtin_read_u32be_size(data + cursor);
    cursor += 4u;
    if (length_u16 > (data_length - cursor) / 2u) {
        return 0;
    }
    cursor += length_u16 * 2u;
    *pcursor = cursor;
    return 1;
}

static int
sixel_builtin_psd_descriptor_read_key4(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    char out_key[5])
{
    size_t cursor;
    size_t key_length;
    size_t copy_length;

    cursor = 0u;
    key_length = 0u;
    copy_length = 0u;
    if (data == NULL || pcursor == NULL || out_key == NULL) {
        return 0;
    }
    cursor = *pcursor;
    if (cursor + 4u > data_length) {
        return 0;
    }
    key_length = sixel_builtin_read_u32be_size(data + cursor);
    cursor += 4u;
    out_key[0] = ' ';
    out_key[1] = ' ';
    out_key[2] = ' ';
    out_key[3] = ' ';
    out_key[4] = '\0';
    if (key_length == 0u) {
        if (cursor + 4u > data_length) {
            return 0;
        }
        memcpy(out_key, data + cursor, 4u);
        cursor += 4u;
        *pcursor = cursor;
        return 1;
    }
    if (key_length > data_length - cursor) {
        return 0;
    }
    copy_length = key_length < 4u ? key_length : 4u;
    if (copy_length > 0u) {
        memcpy(out_key, data + cursor, copy_length);
    }
    cursor += key_length;
    *pcursor = cursor;
    return 1;
}

static int
sixel_builtin_psd_descriptor_read_type(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    char out_type[5])
{
    size_t cursor;

    cursor = 0u;
    if (data == NULL || pcursor == NULL || out_type == NULL) {
        return 0;
    }
    cursor = *pcursor;
    if (cursor + 4u > data_length) {
        return 0;
    }
    memcpy(out_type, data + cursor, 4u);
    out_type[4] = '\0';
    cursor += 4u;
    *pcursor = cursor;
    return 1;
}

static int
sixel_builtin_psd_descriptor_skip_value(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    char const type[5],
    unsigned int depth);

static int
sixel_builtin_psd_descriptor_skip_object(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    unsigned int depth)
{
    size_t cursor;
    size_t item_count;
    size_t i;
    char item_key[5];
    char item_type[5];

    cursor = 0u;
    item_count = 0u;
    i = 0u;
    item_key[0] = '\0';
    item_key[1] = '\0';
    item_key[2] = '\0';
    item_key[3] = '\0';
    item_key[4] = '\0';
    item_type[0] = '\0';
    item_type[1] = '\0';
    item_type[2] = '\0';
    item_type[3] = '\0';
    item_type[4] = '\0';
    if (data == NULL || pcursor == NULL || depth > 8u) {
        return 0;
    }
    cursor = *pcursor;
    if (!sixel_builtin_psd_descriptor_skip_unicode_string(data,
                                                          data_length,
                                                          &cursor)) {
        return 0;
    }
    if (!sixel_builtin_psd_descriptor_read_key4(data,
                                                data_length,
                                                &cursor,
                                                item_key)) {
        return 0;
    }
    if (cursor + 4u > data_length) {
        return 0;
    }
    item_count = sixel_builtin_read_u32be_size(data + cursor);
    cursor += 4u;
    for (i = 0u; i < item_count; ++i) {
        if (!sixel_builtin_psd_descriptor_read_key4(data,
                                                    data_length,
                                                    &cursor,
                                                    item_key)) {
            return 0;
        }
        if (!sixel_builtin_psd_descriptor_read_type(data,
                                                    data_length,
                                                    &cursor,
                                                    item_type)) {
            return 0;
        }
        if (!sixel_builtin_psd_descriptor_skip_value(data,
                                                     data_length,
                                                     &cursor,
                                                     item_type,
                                                     depth + 1u)) {
            return 0;
        }
    }
    *pcursor = cursor;
    return 1;
}

static int
sixel_builtin_psd_descriptor_skip_value(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    char const type[5],
    unsigned int depth)
{
    size_t cursor;
    size_t count;
    size_t length_value;
    size_t i;
    char list_item_type[5];

    cursor = 0u;
    count = 0u;
    length_value = 0u;
    i = 0u;
    list_item_type[0] = '\0';
    list_item_type[1] = '\0';
    list_item_type[2] = '\0';
    list_item_type[3] = '\0';
    list_item_type[4] = '\0';
    if (data == NULL || pcursor == NULL || type == NULL || depth > 8u) {
        return 0;
    }
    cursor = *pcursor;
    if (memcmp(type, "Objc", 4u) == 0 || memcmp(type, "GlbO", 4u) == 0) {
        if (!sixel_builtin_psd_descriptor_skip_object(data,
                                                      data_length,
                                                      &cursor,
                                                      depth + 1u)) {
            return 0;
        }
        *pcursor = cursor;
        return 1;
    }
    if (memcmp(type, "VlLs", 4u) == 0) {
        if (cursor + 4u > data_length) {
            return 0;
        }
        count = sixel_builtin_read_u32be_size(data + cursor);
        cursor += 4u;
        for (i = 0u; i < count; ++i) {
            if (!sixel_builtin_psd_descriptor_read_type(data,
                                                        data_length,
                                                        &cursor,
                                                        list_item_type)) {
                return 0;
            }
            if (!sixel_builtin_psd_descriptor_skip_value(data,
                                                         data_length,
                                                         &cursor,
                                                         list_item_type,
                                                         depth + 1u)) {
                return 0;
            }
        }
        *pcursor = cursor;
        return 1;
    }
    if (memcmp(type, "doub", 4u) == 0) {
        if (cursor + 8u > data_length) {
            return 0;
        }
        cursor += 8u;
        *pcursor = cursor;
        return 1;
    }
    if (memcmp(type, "long", 4u) == 0) {
        if (cursor + 4u > data_length) {
            return 0;
        }
        cursor += 4u;
        *pcursor = cursor;
        return 1;
    }
    if (memcmp(type, "bool", 4u) == 0) {
        if (cursor + 1u > data_length) {
            return 0;
        }
        cursor += 1u;
        *pcursor = cursor;
        return 1;
    }
    if (memcmp(type, "enum", 4u) == 0) {
        char enum_key[5];
        enum_key[0] = '\0';
        enum_key[1] = '\0';
        enum_key[2] = '\0';
        enum_key[3] = '\0';
        enum_key[4] = '\0';
        if (!sixel_builtin_psd_descriptor_read_key4(data,
                                                    data_length,
                                                    &cursor,
                                                    enum_key) ||
            !sixel_builtin_psd_descriptor_read_key4(data,
                                                    data_length,
                                                    &cursor,
                                                    enum_key)) {
            return 0;
        }
        *pcursor = cursor;
        return 1;
    }
    if (memcmp(type, "UntF", 4u) == 0) {
        if (cursor + 12u > data_length) {
            return 0;
        }
        cursor += 12u;
        *pcursor = cursor;
        return 1;
    }
    if (memcmp(type, "TEXT", 4u) == 0) {
        if (!sixel_builtin_psd_descriptor_skip_unicode_string(data,
                                                              data_length,
                                                              &cursor)) {
            return 0;
        }
        *pcursor = cursor;
        return 1;
    }
    if (memcmp(type, "tdta", 4u) == 0 ||
        memcmp(type, "alis", 4u) == 0) {
        if (cursor + 4u > data_length) {
            return 0;
        }
        length_value = sixel_builtin_read_u32be_size(data + cursor);
        cursor += 4u;
        if (length_value > data_length - cursor) {
            return 0;
        }
        cursor += length_value;
        *pcursor = cursor;
        return 1;
    }
    return 0;
}

static int
sixel_builtin_psd_descriptor_read_numeric_value(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    char const type[5],
    double *out_value)
{
    size_t cursor;
    unsigned char const *value_ptr;

    cursor = 0u;
    value_ptr = NULL;
    if (data == NULL || pcursor == NULL || type == NULL || out_value == NULL) {
        return 0;
    }
    cursor = *pcursor;
    if (memcmp(type, "doub", 4u) == 0) {
        if (cursor + 8u > data_length) {
            return 0;
        }
        value_ptr = data + cursor;
        cursor += 8u;
        *out_value = sixel_builtin_read_f64be(value_ptr);
        *pcursor = cursor;
        return 1;
    }
    if (memcmp(type, "UntF", 4u) == 0) {
        if (cursor + 12u > data_length) {
            return 0;
        }
        value_ptr = data + cursor + 4u;
        cursor += 12u;
        *out_value = sixel_builtin_read_f64be(value_ptr);
        *pcursor = cursor;
        return 1;
    }
    if (memcmp(type, "long", 4u) == 0) {
        if (cursor + 4u > data_length) {
            return 0;
        }
        *out_value = (double)(int32_t)sixel_builtin_read_u32be(data + cursor);
        cursor += 4u;
        *pcursor = cursor;
        return 1;
    }
    return 0;
}

static int
sixel_builtin_psd_descriptor_read_bool_value(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    char const type[5],
    int *out_value)
{
    size_t cursor;

    cursor = 0u;
    if (data == NULL || pcursor == NULL || type == NULL || out_value == NULL) {
        return 0;
    }
    if (memcmp(type, "bool", 4u) != 0) {
        return 0;
    }
    cursor = *pcursor;
    if (cursor + 1u > data_length) {
        return 0;
    }
    *out_value = data[cursor] != 0u ? 1 : 0;
    cursor += 1u;
    *pcursor = cursor;
    return 1;
}

static int
sixel_builtin_psd_descriptor_read_enum_value(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    char const type[5],
    char out_value[5])
{
    size_t cursor;
    char enum_type[5];

    cursor = 0u;
    enum_type[0] = '\0';
    enum_type[1] = '\0';
    enum_type[2] = '\0';
    enum_type[3] = '\0';
    enum_type[4] = '\0';
    if (data == NULL || pcursor == NULL || type == NULL || out_value == NULL) {
        return 0;
    }
    if (memcmp(type, "enum", 4u) != 0) {
        return 0;
    }
    cursor = *pcursor;
    if (!sixel_builtin_psd_descriptor_read_key4(data,
                                                data_length,
                                                &cursor,
                                                enum_type) ||
        !sixel_builtin_psd_descriptor_read_key4(data,
                                                data_length,
                                                &cursor,
                                                out_value)) {
        return 0;
    }
    *pcursor = cursor;
    return 1;
}

static int
sixel_builtin_psd_descriptor_parse_color_object(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    float out_rgb[3])
{
    size_t cursor;
    size_t item_count;
    size_t i;
    char key[5];
    char type[5];
    char class_key[5];
    double red;
    double green;
    double blue;
    double cyan;
    double magenta;
    double yellow;
    double black;
    double gray;
    double hue;
    double saturation;
    double brightness;
    double lab_l;
    double lab_a;
    double lab_b;
    double component_max;
    int has_red;
    int has_green;
    int has_blue;
    int has_cyan;
    int has_magenta;
    int has_yellow;
    int has_black;
    int has_gray;
    int has_hue;
    int has_saturation;
    int has_brightness;
    int has_lab_l;
    int has_lab_a;
    int has_lab_b;
    int is_rgb_class;
    int is_gray_class;
    int is_hsb_class;
    int is_lab_class;
    SIXELSTATUS status;

    cursor = 0u;
    item_count = 0u;
    i = 0u;
    class_key[0] = '\0';
    class_key[1] = '\0';
    class_key[2] = '\0';
    class_key[3] = '\0';
    class_key[4] = '\0';
    key[0] = '\0';
    key[1] = '\0';
    key[2] = '\0';
    key[3] = '\0';
    key[4] = '\0';
    type[0] = '\0';
    type[1] = '\0';
    type[2] = '\0';
    type[3] = '\0';
    type[4] = '\0';
    red = 0.0;
    green = 0.0;
    blue = 0.0;
    cyan = 0.0;
    magenta = 0.0;
    yellow = 0.0;
    black = 0.0;
    gray = 0.0;
    hue = 0.0;
    saturation = 0.0;
    brightness = 0.0;
    lab_l = 0.0;
    lab_a = 0.0;
    lab_b = 0.0;
    component_max = 0.0;
    has_red = 0;
    has_green = 0;
    has_blue = 0;
    has_cyan = 0;
    has_magenta = 0;
    has_yellow = 0;
    has_black = 0;
    has_gray = 0;
    has_hue = 0;
    has_saturation = 0;
    has_brightness = 0;
    has_lab_l = 0;
    has_lab_a = 0;
    has_lab_b = 0;
    is_rgb_class = 0;
    is_gray_class = 0;
    is_hsb_class = 0;
    is_lab_class = 0;
    status = SIXEL_FALSE;
    if (data == NULL || pcursor == NULL || out_rgb == NULL) {
        return 0;
    }

    cursor = *pcursor;
    if (!sixel_builtin_psd_descriptor_skip_unicode_string(data,
                                                          data_length,
                                                          &cursor) ||
        !sixel_builtin_psd_descriptor_read_key4(data,
                                                data_length,
                                                &cursor,
                                                class_key)) {
        return 0;
    }
    if (memcmp(class_key, "RGBC", 4u) == 0) {
        is_rgb_class = 1;
    } else if (memcmp(class_key, "Grsc", 4u) == 0) {
        is_gray_class = 1;
    } else if (memcmp(class_key, "HSBC", 4u) == 0 ||
               memcmp(class_key, "HSB ", 4u) == 0) {
        is_hsb_class = 1;
    } else if (memcmp(class_key, "LbCl", 4u) == 0 ||
               memcmp(class_key, "LABC", 4u) == 0 ||
               memcmp(class_key, "LabC", 4u) == 0) {
        is_lab_class = 1;
    } else if (memcmp(class_key, "CMYC", 4u) == 0 ||
               memcmp(class_key, "CMYK", 4u) == 0) {
        is_rgb_class = 0;
    } else {
        return 0;
    }
    if (cursor + 4u > data_length) {
        return 0;
    }
    item_count = sixel_builtin_read_u32be_size(data + cursor);
    cursor += 4u;
    for (i = 0u; i < item_count; ++i) {
        double numeric;
        numeric = 0.0;
        if (!sixel_builtin_psd_descriptor_read_key4(data,
                                                    data_length,
                                                    &cursor,
                                                    key) ||
            !sixel_builtin_psd_descriptor_read_type(data,
                                                    data_length,
                                                    &cursor,
                                                    type)) {
            return 0;
        }
        if (is_rgb_class != 0) {
            if ((memcmp(key, "Rd  ", 4u) == 0 ||
                 memcmp(key, "Grn ", 4u) == 0 ||
                 memcmp(key, "Bl  ", 4u) == 0) &&
                sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                                data_length,
                                                                &cursor,
                                                                type,
                                                                &numeric)) {
                if (memcmp(key, "Rd  ", 4u) == 0) {
                    red = numeric;
                    has_red = 1;
                } else if (memcmp(key, "Grn ", 4u) == 0) {
                    green = numeric;
                    has_green = 1;
                } else {
                    blue = numeric;
                    has_blue = 1;
                }
                continue;
            }
        } else if (is_gray_class != 0) {
            if ((memcmp(key, "Gry ", 4u) == 0 ||
                 memcmp(key, "gray", 4u) == 0) &&
                sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                                data_length,
                                                                &cursor,
                                                                type,
                                                                &numeric)) {
                gray = numeric;
                has_gray = 1;
                continue;
            }
        } else if (is_hsb_class != 0) {
            if ((memcmp(key, "H   ", 4u) == 0 ||
                 memcmp(key, "hue ", 4u) == 0) &&
                sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                                data_length,
                                                                &cursor,
                                                                type,
                                                                &numeric)) {
                hue = numeric;
                has_hue = 1;
                continue;
            }
            if ((memcmp(key, "Strt", 4u) == 0 ||
                 memcmp(key, "sat ", 4u) == 0) &&
                sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                                data_length,
                                                                &cursor,
                                                                type,
                                                                &numeric)) {
                saturation = numeric;
                has_saturation = 1;
                continue;
            }
            if ((memcmp(key, "Brgh", 4u) == 0 ||
                 memcmp(key, "bri ", 4u) == 0) &&
                sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                                data_length,
                                                                &cursor,
                                                                type,
                                                                &numeric)) {
                brightness = numeric;
                has_brightness = 1;
                continue;
            }
        } else if (is_lab_class != 0) {
            if ((memcmp(key, "Lmnc", 4u) == 0 ||
                 memcmp(key, "L   ", 4u) == 0) &&
                sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                                data_length,
                                                                &cursor,
                                                                type,
                                                                &numeric)) {
                lab_l = numeric;
                has_lab_l = 1;
                continue;
            }
            if ((memcmp(key, "A   ", 4u) == 0 ||
                 memcmp(key, "a   ", 4u) == 0) &&
                sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                                data_length,
                                                                &cursor,
                                                                type,
                                                                &numeric)) {
                lab_a = numeric;
                has_lab_a = 1;
                continue;
            }
            if ((memcmp(key, "B   ", 4u) == 0 ||
                 memcmp(key, "b   ", 4u) == 0) &&
                sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                                data_length,
                                                                &cursor,
                                                                type,
                                                                &numeric)) {
                lab_b = numeric;
                has_lab_b = 1;
                continue;
            }
        } else {
            if ((memcmp(key, "Cyn ", 4u) == 0 ||
                 memcmp(key, "Mgnt", 4u) == 0 ||
                 memcmp(key, "Ylw ", 4u) == 0 ||
                 memcmp(key, "Blck", 4u) == 0) &&
                sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                                data_length,
                                                                &cursor,
                                                                type,
                                                                &numeric)) {
                if (memcmp(key, "Cyn ", 4u) == 0) {
                    cyan = numeric;
                    has_cyan = 1;
                } else if (memcmp(key, "Mgnt", 4u) == 0) {
                    magenta = numeric;
                    has_magenta = 1;
                } else if (memcmp(key, "Ylw ", 4u) == 0) {
                    yellow = numeric;
                    has_yellow = 1;
                } else {
                    black = numeric;
                    has_black = 1;
                }
                continue;
            }
        }
        if (!sixel_builtin_psd_descriptor_skip_value(data,
                                                     data_length,
                                                     &cursor,
                                                     type,
                                                     0u)) {
            return 0;
        }
    }
    if (is_rgb_class != 0) {
        if (has_red == 0 || has_green == 0 || has_blue == 0) {
            return 0;
        }
        component_max = fmax(red, fmax(green, blue));
        if (component_max <= 1.0) {
            red *= 255.0;
            green *= 255.0;
            blue *= 255.0;
        }
        if (red < 0.0) {
            red = 0.0;
        } else if (red > 255.0) {
            red = 255.0;
        }
        if (green < 0.0) {
            green = 0.0;
        } else if (green > 255.0) {
            green = 255.0;
        }
        if (blue < 0.0) {
            blue = 0.0;
        } else if (blue > 255.0) {
            blue = 255.0;
        }
    } else if (is_gray_class != 0) {
        if (has_gray == 0) {
            return 0;
        }
        if (gray <= 1.0) {
            gray *= 255.0;
        } else if (gray <= 100.0) {
            gray *= 2.55;
        }
        if (gray < 0.0) {
            gray = 0.0;
        } else if (gray > 255.0) {
            gray = 255.0;
        }
        red = gray;
        green = gray;
        blue = gray;
    } else if (is_hsb_class != 0) {
        double h_norm;
        double s_norm;
        double v_norm;
        double chroma;
        double h_prime;
        double x;
        double m;
        double r1;
        double g1;
        double b1;

        if (has_hue == 0 || has_saturation == 0 || has_brightness == 0) {
            return 0;
        }
        if (hue <= 1.0 && hue >= 0.0) {
            h_norm = hue * 360.0;
        } else {
            h_norm = hue;
        }
        while (h_norm < 0.0) {
            h_norm += 360.0;
        }
        while (h_norm >= 360.0) {
            h_norm -= 360.0;
        }

        if (saturation <= 1.0 && saturation >= 0.0) {
            s_norm = saturation;
        } else if (saturation <= 100.0) {
            s_norm = saturation / 100.0;
        } else if (saturation <= 255.0) {
            s_norm = saturation / 255.0;
        } else {
            s_norm = 1.0;
        }
        if (brightness <= 1.0 && brightness >= 0.0) {
            v_norm = brightness;
        } else if (brightness <= 100.0) {
            v_norm = brightness / 100.0;
        } else if (brightness <= 255.0) {
            v_norm = brightness / 255.0;
        } else {
            v_norm = 1.0;
        }
        if (s_norm < 0.0) {
            s_norm = 0.0;
        } else if (s_norm > 1.0) {
            s_norm = 1.0;
        }
        if (v_norm < 0.0) {
            v_norm = 0.0;
        } else if (v_norm > 1.0) {
            v_norm = 1.0;
        }

        chroma = v_norm * s_norm;
        h_prime = h_norm / 60.0;
        x = chroma * (1.0 - fabs(fmod(h_prime, 2.0) - 1.0));
        m = v_norm - chroma;
        r1 = 0.0;
        g1 = 0.0;
        b1 = 0.0;
        if (h_prime < 1.0) {
            r1 = chroma;
            g1 = x;
        } else if (h_prime < 2.0) {
            r1 = x;
            g1 = chroma;
        } else if (h_prime < 3.0) {
            g1 = chroma;
            b1 = x;
        } else if (h_prime < 4.0) {
            g1 = x;
            b1 = chroma;
        } else if (h_prime < 5.0) {
            r1 = x;
            b1 = chroma;
        } else {
            r1 = chroma;
            b1 = x;
        }
        red = (r1 + m) * 255.0;
        green = (g1 + m) * 255.0;
        blue = (b1 + m) * 255.0;
    } else if (is_lab_class != 0) {
        float lab_pixel[3];

        if (has_lab_l == 0 || has_lab_a == 0 || has_lab_b == 0) {
            return 0;
        }

        if (lab_l >= 0.0 && lab_l <= 1.0) {
            lab_l *= 100.0;
        } else if (lab_l > 100.0 && lab_l <= 255.0) {
            lab_l = (lab_l / 255.0) * 100.0;
        }
        if (lab_l < 0.0) {
            lab_l = 0.0;
        } else if (lab_l > 100.0) {
            lab_l = 100.0;
        }

        if (lab_a >= -1.5 && lab_a <= 1.5) {
            lab_a *= 128.0;
        } else if (lab_a >= 128.0 && lab_a <= 255.0) {
            lab_a = lab_a - 128.0;
        }
        if (lab_b >= -1.5 && lab_b <= 1.5) {
            lab_b *= 128.0;
        } else if (lab_b >= 128.0 && lab_b <= 255.0) {
            lab_b = lab_b - 128.0;
        }
        if (lab_a < -192.0) {
            lab_a = -192.0;
        } else if (lab_a > 192.0) {
            lab_a = 192.0;
        }
        if (lab_b < -192.0) {
            lab_b = -192.0;
        } else if (lab_b > 192.0) {
            lab_b = 192.0;
        }

        lab_pixel[0] = (float)(lab_l / 100.0);
        lab_pixel[1] = sixel_builtin_psd_lab_clamp_ab((float)(lab_a / 128.0));
        lab_pixel[2] = sixel_builtin_psd_lab_clamp_ab((float)(lab_b / 128.0));
        status = sixel_helper_convert_colorspace((unsigned char *)lab_pixel,
                                                 sizeof(lab_pixel),
                                                 SIXEL_PIXELFORMAT_CIELABFLOAT32,
                                                 SIXEL_COLORSPACE_CIELAB,
                                                 SIXEL_COLORSPACE_GAMMA);
        if (SIXEL_FAILED(status)) {
            return 0;
        }
        red = (double)lab_pixel[0] * 255.0;
        green = (double)lab_pixel[1] * 255.0;
        blue = (double)lab_pixel[2] * 255.0;
        if (red < 0.0) {
            red = 0.0;
        } else if (red > 255.0) {
            red = 255.0;
        }
        if (green < 0.0) {
            green = 0.0;
        } else if (green > 255.0) {
            green = 255.0;
        }
        if (blue < 0.0) {
            blue = 0.0;
        } else if (blue > 255.0) {
            blue = 255.0;
        }
    } else {
        double red01;
        double green01;
        double blue01;

        if (has_cyan == 0 || has_magenta == 0 || has_yellow == 0 || has_black == 0) {
            return 0;
        }
        component_max = fmax(cyan,
                             fmax(magenta,
                                  fmax(yellow, black)));
        if (component_max > 1.0) {
            if (component_max <= 100.0) {
                cyan /= 100.0;
                magenta /= 100.0;
                yellow /= 100.0;
                black /= 100.0;
            } else {
                cyan /= 255.0;
                magenta /= 255.0;
                yellow /= 255.0;
                black /= 255.0;
            }
        }
        if (cyan < 0.0) {
            cyan = 0.0;
        } else if (cyan > 1.0) {
            cyan = 1.0;
        }
        if (magenta < 0.0) {
            magenta = 0.0;
        } else if (magenta > 1.0) {
            magenta = 1.0;
        }
        if (yellow < 0.0) {
            yellow = 0.0;
        } else if (yellow > 1.0) {
            yellow = 1.0;
        }
        if (black < 0.0) {
            black = 0.0;
        } else if (black > 1.0) {
            black = 1.0;
        }
        red01 = (1.0 - cyan) * (1.0 - black);
        green01 = (1.0 - magenta) * (1.0 - black);
        blue01 = (1.0 - yellow) * (1.0 - black);
        red = red01 * 255.0;
        green = green01 * 255.0;
        blue = blue01 * 255.0;
    }

    sixel_builtin_psd_fill_set_rgb_from_u8(out_rgb,
                                           (unsigned char)(red + 0.5),
                                           (unsigned char)(green + 0.5),
                                           (unsigned char)(blue + 0.5));
    *pcursor = cursor;
    return 1;
}

static float
sixel_builtin_psd_descriptor_normalize_gradient_position(double value)
{
    float normalized;

    normalized = 0.0f;
    if (value != value) {
        return 0.0f;
    }
    if (value <= 1.0) {
        normalized = (float)value;
    } else if (value <= 100.0) {
        normalized = (float)(value / 100.0);
    } else if (value <= 4096.0) {
        normalized = (float)(value / 4096.0);
    } else {
        normalized = (float)(value / 65535.0);
    }
    return sixel_builtin_psd_clamp01(normalized);
}

static float
sixel_builtin_psd_descriptor_normalize_gradient_alpha(double value)
{
    float normalized;

    normalized = 0.0f;
    if (value != value) {
        return 1.0f;
    }
    if (value <= 1.0) {
        normalized = (float)value;
    } else if (value <= 100.0) {
        normalized = (float)(value / 100.0);
    } else if (value <= 255.0) {
        normalized = (float)(value / 255.0);
    } else {
        normalized = 1.0f;
    }
    return sixel_builtin_psd_clamp_alpha_float32(normalized);
}

static int
sixel_builtin_psd_parse_gdfl_descriptor_stop_object(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    float *out_pos,
    float out_rgb[3],
    float *out_alpha)
{
    size_t cursor;
    size_t item_count;
    size_t i;
    char key[5];
    char type[5];
    double location;
    double alpha;
    int has_location;
    int has_color;

    cursor = 0u;
    item_count = 0u;
    i = 0u;
    key[0] = '\0';
    key[1] = '\0';
    key[2] = '\0';
    key[3] = '\0';
    key[4] = '\0';
    type[0] = '\0';
    type[1] = '\0';
    type[2] = '\0';
    type[3] = '\0';
    type[4] = '\0';
    location = 0.0;
    alpha = 1.0;
    has_location = 0;
    has_color = 0;
    if (data == NULL || pcursor == NULL || out_pos == NULL ||
        out_rgb == NULL || out_alpha == NULL) {
        return 0;
    }

    cursor = *pcursor;
    if (!sixel_builtin_psd_descriptor_skip_unicode_string(data,
                                                          data_length,
                                                          &cursor) ||
        !sixel_builtin_psd_descriptor_read_key4(data,
                                                data_length,
                                                &cursor,
                                                key)) {
        return 0;
    }
    if (cursor + 4u > data_length) {
        return 0;
    }
    item_count = sixel_builtin_read_u32be_size(data + cursor);
    cursor += 4u;
    for (i = 0u; i < item_count; ++i) {
        if (!sixel_builtin_psd_descriptor_read_key4(data,
                                                    data_length,
                                                    &cursor,
                                                    key) ||
            !sixel_builtin_psd_descriptor_read_type(data,
                                                    data_length,
                                                    &cursor,
                                                    type)) {
            return 0;
        }
        if (memcmp(key, "Lctn", 4u) == 0 &&
            sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                            data_length,
                                                            &cursor,
                                                            type,
                                                            &location)) {
            has_location = 1;
            continue;
        }
        if (memcmp(key, "Opct", 4u) == 0 &&
            sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                            data_length,
                                                            &cursor,
                                                            type,
                                                            &alpha)) {
            continue;
        }
        if (memcmp(key, "Clr ", 4u) == 0 &&
            memcmp(type, "Objc", 4u) == 0) {
            if (!sixel_builtin_psd_descriptor_parse_color_object(data,
                                                                 data_length,
                                                                 &cursor,
                                                                 out_rgb)) {
                return 0;
            }
            has_color = 1;
            continue;
        }
        if (!sixel_builtin_psd_descriptor_skip_value(data,
                                                     data_length,
                                                     &cursor,
                                                     type,
                                                     0u)) {
            return 0;
        }
    }
    if (has_color == 0) {
        return 0;
    }
    if (has_location == 0) {
        location = 0.0;
    }
    *out_pos = sixel_builtin_psd_descriptor_normalize_gradient_position(location);
    *out_alpha = sixel_builtin_psd_descriptor_normalize_gradient_alpha(alpha);
    *pcursor = cursor;
    return 1;
}

static unsigned char
sixel_builtin_psd_descriptor_gradient_type_from_enum(char const enum_value[5])
{
    if (enum_value == NULL) {
        return 0u;
    }
    if (memcmp(enum_value, "Rdl ", 4u) == 0) {
        return 1u;
    }
    if (memcmp(enum_value, "Angl", 4u) == 0) {
        return 2u;
    }
    if (memcmp(enum_value, "Rflc", 4u) == 0) {
        return 3u;
    }
    if (memcmp(enum_value, "Dmnd", 4u) == 0) {
        return 4u;
    }
    return 0u;
}

static int
sixel_builtin_psd_parse_gdfl_descriptor_stop_list(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    sixel_builtin_psd_layer_record_t *layer,
    size_t *pstop_count)
{
    size_t cursor;
    size_t list_count;
    size_t j;
    char list_type[5];
    size_t stop_count;

    cursor = 0u;
    list_count = 0u;
    j = 0u;
    list_type[0] = '\0';
    list_type[1] = '\0';
    list_type[2] = '\0';
    list_type[3] = '\0';
    list_type[4] = '\0';
    stop_count = 0u;
    if (data == NULL || pcursor == NULL || layer == NULL || pstop_count == NULL) {
        return 0;
    }
    cursor = *pcursor;
    stop_count = *pstop_count;
    if (cursor + 4u > data_length) {
        return 0;
    }
    list_count = sixel_builtin_read_u32be_size(data + cursor);
    cursor += 4u;
    for (j = 0u; j < list_count; ++j) {
        if (!sixel_builtin_psd_descriptor_read_type(data,
                                                    data_length,
                                                    &cursor,
                                                    list_type)) {
            return 0;
        }
        if (memcmp(list_type, "Objc", 4u) == 0) {
            float stop_pos;
            float stop_rgb[3];
            float stop_alpha;
            stop_pos = 0.0f;
            stop_rgb[0] = 0.0f;
            stop_rgb[1] = 0.0f;
            stop_rgb[2] = 0.0f;
            stop_alpha = 1.0f;
            if (!sixel_builtin_psd_parse_gdfl_descriptor_stop_object(
                    data,
                    data_length,
                    &cursor,
                    &stop_pos,
                    stop_rgb,
                    &stop_alpha)) {
                return 0;
            }
            if (stop_count < SIXEL_BUILTIN_PSD_FILL_STOP_MAX) {
                layer->fill_gradient_stop_pos[stop_count] = stop_pos;
                layer->fill_gradient_stop_rgb[stop_count][0] = stop_rgb[0];
                layer->fill_gradient_stop_rgb[stop_count][1] = stop_rgb[1];
                layer->fill_gradient_stop_rgb[stop_count][2] = stop_rgb[2];
                layer->fill_gradient_stop_alpha[stop_count] = stop_alpha;
                ++stop_count;
            }
        } else if (!sixel_builtin_psd_descriptor_skip_value(data,
                                                            data_length,
                                                            &cursor,
                                                            list_type,
                                                            0u)) {
            return 0;
        }
    }
    *pstop_count = stop_count;
    *pcursor = cursor;
    return 1;
}

static int
sixel_builtin_psd_parse_gdfl_descriptor_grad_object(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    unsigned char *pgradient_type,
    size_t *pstop_count,
    sixel_builtin_psd_layer_record_t *layer)
{
    size_t cursor;
    size_t item_count;
    size_t i;
    char key[5];
    char type[5];

    cursor = 0u;
    item_count = 0u;
    i = 0u;
    key[0] = '\0';
    key[1] = '\0';
    key[2] = '\0';
    key[3] = '\0';
    key[4] = '\0';
    type[0] = '\0';
    type[1] = '\0';
    type[2] = '\0';
    type[3] = '\0';
    type[4] = '\0';
    if (data == NULL || pcursor == NULL || pgradient_type == NULL ||
        pstop_count == NULL || layer == NULL) {
        return 0;
    }

    cursor = *pcursor;
    if (!sixel_builtin_psd_descriptor_skip_unicode_string(data,
                                                          data_length,
                                                          &cursor) ||
        !sixel_builtin_psd_descriptor_read_key4(data,
                                                data_length,
                                                &cursor,
                                                key)) {
        return 0;
    }
    if (cursor + 4u > data_length) {
        return 0;
    }
    item_count = sixel_builtin_read_u32be_size(data + cursor);
    cursor += 4u;
    for (i = 0u; i < item_count; ++i) {
        if (!sixel_builtin_psd_descriptor_read_key4(data,
                                                    data_length,
                                                    &cursor,
                                                    key) ||
            !sixel_builtin_psd_descriptor_read_type(data,
                                                    data_length,
                                                    &cursor,
                                                    type)) {
            return 0;
        }
        if (memcmp(key, "Type", 4u) == 0) {
            char enum_value[5];
            enum_value[0] = '\0';
            enum_value[1] = '\0';
            enum_value[2] = '\0';
            enum_value[3] = '\0';
            enum_value[4] = '\0';
            if (!sixel_builtin_psd_descriptor_read_enum_value(data,
                                                              data_length,
                                                              &cursor,
                                                              type,
                                                              enum_value)) {
                return 0;
            }
            *pgradient_type =
                sixel_builtin_psd_descriptor_gradient_type_from_enum(enum_value);
            continue;
        }
        if (memcmp(key, "Clrs", 4u) == 0 &&
            memcmp(type, "VlLs", 4u) == 0) {
            if (!sixel_builtin_psd_parse_gdfl_descriptor_stop_list(data,
                                                                   data_length,
                                                                   &cursor,
                                                                   layer,
                                                                   pstop_count)) {
                return 0;
            }
            continue;
        }
        if (!sixel_builtin_psd_descriptor_skip_value(data,
                                                     data_length,
                                                     &cursor,
                                                     type,
                                                     0u)) {
            return 0;
        }
    }
    *pcursor = cursor;
    return 1;
}

static void
sixel_builtin_psd_sort_gradient_stops(sixel_builtin_psd_layer_record_t *layer,
                                      size_t stop_count)
{
    size_t a;

    if (layer == NULL || stop_count <= 1u) {
        return;
    }
    for (a = 0u; a + 1u < stop_count; ++a) {
        size_t b;
        for (b = a + 1u; b < stop_count; ++b) {
            if (layer->fill_gradient_stop_pos[b] < layer->fill_gradient_stop_pos[a]) {
                float tmp_pos;
                float tmp_alpha;
                float tmp_rgb0;
                float tmp_rgb1;
                float tmp_rgb2;
                tmp_pos = layer->fill_gradient_stop_pos[a];
                tmp_alpha = layer->fill_gradient_stop_alpha[a];
                tmp_rgb0 = layer->fill_gradient_stop_rgb[a][0];
                tmp_rgb1 = layer->fill_gradient_stop_rgb[a][1];
                tmp_rgb2 = layer->fill_gradient_stop_rgb[a][2];
                layer->fill_gradient_stop_pos[a] = layer->fill_gradient_stop_pos[b];
                layer->fill_gradient_stop_alpha[a] = layer->fill_gradient_stop_alpha[b];
                layer->fill_gradient_stop_rgb[a][0] = layer->fill_gradient_stop_rgb[b][0];
                layer->fill_gradient_stop_rgb[a][1] = layer->fill_gradient_stop_rgb[b][1];
                layer->fill_gradient_stop_rgb[a][2] = layer->fill_gradient_stop_rgb[b][2];
                layer->fill_gradient_stop_pos[b] = tmp_pos;
                layer->fill_gradient_stop_alpha[b] = tmp_alpha;
                layer->fill_gradient_stop_rgb[b][0] = tmp_rgb0;
                layer->fill_gradient_stop_rgb[b][1] = tmp_rgb1;
                layer->fill_gradient_stop_rgb[b][2] = tmp_rgb2;
            }
        }
    }
}

static int
sixel_builtin_psd_parse_gdfl_descriptor_payload(
    unsigned char const *data,
    size_t key_length,
    sixel_builtin_psd_layer_record_t *layer)
{
    size_t cursor;
    size_t item_count;
    size_t i;
    char key[5];
    char type[5];
    unsigned char gradient_type;
    int gradient_reverse;
    float gradient_angle_deg;
    float gradient_scale;
    size_t stop_count;

    cursor = 0u;
    item_count = 0u;
    i = 0u;
    key[0] = '\0';
    key[1] = '\0';
    key[2] = '\0';
    key[3] = '\0';
    key[4] = '\0';
    type[0] = '\0';
    type[1] = '\0';
    type[2] = '\0';
    type[3] = '\0';
    type[4] = '\0';
    gradient_type = 0u;
    gradient_reverse = 0;
    gradient_angle_deg = 0.0f;
    gradient_scale = 1.0f;
    stop_count = 0u;
    if (data == NULL || layer == NULL) {
        return 0;
    }

    if (!sixel_builtin_psd_descriptor_skip_unicode_string(data,
                                                          key_length,
                                                          &cursor) ||
        !sixel_builtin_psd_descriptor_read_key4(data,
                                                key_length,
                                                &cursor,
                                                key)) {
        return 0;
    }
    if (cursor + 4u > key_length) {
        return 0;
    }
    item_count = sixel_builtin_read_u32be_size(data + cursor);
    cursor += 4u;
    for (i = 0u; i < item_count; ++i) {
        if (!sixel_builtin_psd_descriptor_read_key4(data,
                                                    key_length,
                                                    &cursor,
                                                    key) ||
            !sixel_builtin_psd_descriptor_read_type(data,
                                                    key_length,
                                                    &cursor,
                                                    type)) {
            return 0;
        }
        if (memcmp(key, "Type", 4u) == 0) {
            char enum_value[5];
            enum_value[0] = '\0';
            enum_value[1] = '\0';
            enum_value[2] = '\0';
            enum_value[3] = '\0';
            enum_value[4] = '\0';
            if (!sixel_builtin_psd_descriptor_read_enum_value(data,
                                                              key_length,
                                                              &cursor,
                                                              type,
                                                              enum_value)) {
                return 0;
            }
            gradient_type =
                sixel_builtin_psd_descriptor_gradient_type_from_enum(enum_value);
            continue;
        }
        if (memcmp(key, "Rvrs", 4u) == 0) {
            if (!sixel_builtin_psd_descriptor_read_bool_value(data,
                                                              key_length,
                                                              &cursor,
                                                              type,
                                                              &gradient_reverse)) {
                return 0;
            }
            continue;
        }
        if (memcmp(key, "Angl", 4u) == 0) {
            double numeric;
            numeric = 0.0;
            if (!sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                                 key_length,
                                                                 &cursor,
                                                                 type,
                                                                 &numeric)) {
                return 0;
            }
            gradient_angle_deg = (float)numeric;
            continue;
        }
        if (memcmp(key, "Scl ", 4u) == 0) {
            double numeric;
            numeric = 0.0;
            if (!sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                                 key_length,
                                                                 &cursor,
                                                                 type,
                                                                 &numeric)) {
                return 0;
            }
            if (numeric > 4.0) {
                numeric /= 100.0;
            }
            if (numeric > 0.0) {
                gradient_scale = (float)numeric;
            }
            continue;
        }
        if (memcmp(key, "Clrs", 4u) == 0 &&
            memcmp(type, "VlLs", 4u) == 0) {
            if (!sixel_builtin_psd_parse_gdfl_descriptor_stop_list(data,
                                                                   key_length,
                                                                   &cursor,
                                                                   layer,
                                                                   &stop_count)) {
                return 0;
            }
            continue;
        }
        if (memcmp(key, "Grad", 4u) == 0 &&
            memcmp(type, "Objc", 4u) == 0) {
            if (!sixel_builtin_psd_parse_gdfl_descriptor_grad_object(data,
                                                                     key_length,
                                                                     &cursor,
                                                                     &gradient_type,
                                                                     &stop_count,
                                                                     layer)) {
                return 0;
            }
            continue;
        }
        if (!sixel_builtin_psd_descriptor_skip_value(data,
                                                     key_length,
                                                     &cursor,
                                                     type,
                                                     0u)) {
            return 0;
        }
    }
    if (stop_count == 0u) {
        return 0;
    }

    sixel_builtin_psd_sort_gradient_stops(layer, stop_count);

    layer->fill_kind = SIXEL_BUILTIN_PSD_FILL_GDFL;
    layer->fill_gradient_type = gradient_type;
    layer->fill_gradient_reverse = gradient_reverse;
    layer->fill_gradient_angle_deg = gradient_angle_deg;
    layer->fill_gradient_scale = gradient_scale > 0.0f ? gradient_scale : 1.0f;
    layer->fill_gradient_stop_count = stop_count;
    return 1;
}

static int
sixel_builtin_psd_parse_ptfl_descriptor_payload(
    unsigned char const *data,
    size_t key_length,
    sixel_builtin_psd_layer_record_t *layer)
{
    size_t cursor;
    size_t item_count;
    size_t i;
    char key[5];
    char type[5];
    int has_fg;
    int has_bg;
    unsigned int tile;

    cursor = 0u;
    item_count = 0u;
    i = 0u;
    key[0] = '\0';
    key[1] = '\0';
    key[2] = '\0';
    key[3] = '\0';
    key[4] = '\0';
    type[0] = '\0';
    type[1] = '\0';
    type[2] = '\0';
    type[3] = '\0';
    type[4] = '\0';
    has_fg = 0;
    has_bg = 0;
    tile = 4u;
    if (data == NULL || layer == NULL) {
        return 0;
    }

    if (!sixel_builtin_psd_descriptor_skip_unicode_string(data,
                                                          key_length,
                                                          &cursor) ||
        !sixel_builtin_psd_descriptor_read_key4(data,
                                                key_length,
                                                &cursor,
                                                key)) {
        return 0;
    }
    if (cursor + 4u > key_length) {
        return 0;
    }
    item_count = sixel_builtin_read_u32be_size(data + cursor);
    cursor += 4u;
    for (i = 0u; i < item_count; ++i) {
        if (!sixel_builtin_psd_descriptor_read_key4(data,
                                                    key_length,
                                                    &cursor,
                                                    key) ||
            !sixel_builtin_psd_descriptor_read_type(data,
                                                    key_length,
                                                    &cursor,
                                                    type)) {
            return 0;
        }
        if (memcmp(key, "Sz  ", 4u) == 0) {
            double numeric;
            numeric = 0.0;
            if (!sixel_builtin_psd_descriptor_read_numeric_value(data,
                                                                 key_length,
                                                                 &cursor,
                                                                 type,
                                                                 &numeric)) {
                return 0;
            }
            if (numeric < 1.0) {
                tile = 1u;
            } else if (numeric > 65535.0) {
                tile = 65535u;
            } else {
                tile = (unsigned int)(numeric + 0.5);
            }
            continue;
        }
        if (memcmp(key, "FgCl", 4u) == 0 &&
            memcmp(type, "Objc", 4u) == 0) {
            if (!sixel_builtin_psd_descriptor_parse_color_object(data,
                                                                 key_length,
                                                                 &cursor,
                                                                 layer->fill_pattern_fg_rgb)) {
                return 0;
            }
            has_fg = 1;
            continue;
        }
        if (memcmp(key, "BgCl", 4u) == 0 &&
            memcmp(type, "Objc", 4u) == 0) {
            if (!sixel_builtin_psd_descriptor_parse_color_object(data,
                                                                 key_length,
                                                                 &cursor,
                                                                 layer->fill_pattern_bg_rgb)) {
                return 0;
            }
            has_bg = 1;
            continue;
        }
        if (!sixel_builtin_psd_descriptor_skip_value(data,
                                                     key_length,
                                                     &cursor,
                                                     type,
                                                     0u)) {
            return 0;
        }
    }
    if (has_fg == 0 || has_bg == 0) {
        return 0;
    }
    if (tile < 1u) {
        tile = 1u;
    }
    layer->fill_kind = SIXEL_BUILTIN_PSD_FILL_PTFL;
    layer->fill_pattern_tile = tile;
    return 1;
}

static int
sixel_builtin_psd_parse_soco_descriptor_payload(
    unsigned char const *data,
    size_t key_length,
    sixel_builtin_psd_layer_record_t *layer)
{
    size_t cursor;
    size_t item_count;
    size_t i;
    char key[5];
    char type[5];

    cursor = 0u;
    item_count = 0u;
    i = 0u;
    key[0] = '\0';
    key[1] = '\0';
    key[2] = '\0';
    key[3] = '\0';
    key[4] = '\0';
    type[0] = '\0';
    type[1] = '\0';
    type[2] = '\0';
    type[3] = '\0';
    type[4] = '\0';
    if (data == NULL || layer == NULL) {
        return 0;
    }
    if (!sixel_builtin_psd_descriptor_skip_unicode_string(data,
                                                          key_length,
                                                          &cursor) ||
        !sixel_builtin_psd_descriptor_read_key4(data,
                                                key_length,
                                                &cursor,
                                                key)) {
        return 0;
    }
    if (cursor + 4u > key_length) {
        return 0;
    }
    item_count = sixel_builtin_read_u32be_size(data + cursor);
    cursor += 4u;
    for (i = 0u; i < item_count; ++i) {
        if (!sixel_builtin_psd_descriptor_read_key4(data,
                                                    key_length,
                                                    &cursor,
                                                    key) ||
            !sixel_builtin_psd_descriptor_read_type(data,
                                                    key_length,
                                                    &cursor,
                                                    type)) {
            return 0;
        }
        if (memcmp(key, "Clr ", 4u) == 0 &&
            memcmp(type, "Objc", 4u) == 0) {
            size_t obj_cursor;
            float fill_rgb[3];

            obj_cursor = cursor;
            fill_rgb[0] = 0.0f;
            fill_rgb[1] = 0.0f;
            fill_rgb[2] = 0.0f;
            if (sixel_builtin_psd_descriptor_parse_color_object(data,
                                                                key_length,
                                                                &obj_cursor,
                                                                fill_rgb)) {
                layer->fill_kind = SIXEL_BUILTIN_PSD_FILL_SOCO;
                layer->fill_solid_rgb[0] = fill_rgb[0];
                layer->fill_solid_rgb[1] = fill_rgb[1];
                layer->fill_solid_rgb[2] = fill_rgb[2];
                return 1;
            }
            if (!sixel_builtin_psd_descriptor_skip_value(data,
                                                         key_length,
                                                         &cursor,
                                                         type,
                                                         0u)) {
                return 0;
            }
            continue;
        }
        if (!sixel_builtin_psd_descriptor_skip_value(data,
                                                     key_length,
                                                     &cursor,
                                                     type,
                                                     0u)) {
            return 0;
        }
    }
    return 0;
}

static int
sixel_builtin_psd_parse_soco_descriptor_payload_loose(
    unsigned char const *data,
    size_t key_length,
    sixel_builtin_psd_layer_record_t *layer)
{
    size_t cursor;
    size_t obj_cursor;
    float fill_rgb[3];
    static unsigned char const clr_objc_key0[12] = {
        0x00u, 0x00u, 0x00u, 0x00u,
        0x43u, 0x6cu, 0x72u, 0x20u,
        0x4fu, 0x62u, 0x6au, 0x63u
    };
    static unsigned char const clr_objc_key4[12] = {
        0x00u, 0x00u, 0x00u, 0x04u,
        0x43u, 0x6cu, 0x72u, 0x20u,
        0x4fu, 0x62u, 0x6au, 0x63u
    };

    cursor = 0u;
    obj_cursor = 0u;
    fill_rgb[0] = 0.0f;
    fill_rgb[1] = 0.0f;
    fill_rgb[2] = 0.0f;
    if (data == NULL || layer == NULL || key_length < 12u) {
        return 0;
    }
    for (cursor = 0u; cursor + 12u <= key_length; ++cursor) {
        if (memcmp(data + cursor, clr_objc_key0, 12u) != 0 &&
            memcmp(data + cursor, clr_objc_key4, 12u) != 0) {
            continue;
        }
        obj_cursor = cursor + 12u;
        fill_rgb[0] = 0.0f;
        fill_rgb[1] = 0.0f;
        fill_rgb[2] = 0.0f;
        if (!sixel_builtin_psd_descriptor_parse_color_object(data,
                                                             key_length,
                                                             &obj_cursor,
                                                             fill_rgb)) {
            continue;
        }
        layer->fill_kind = SIXEL_BUILTIN_PSD_FILL_SOCO;
        layer->fill_solid_rgb[0] = fill_rgb[0];
        layer->fill_solid_rgb[1] = fill_rgb[1];
        layer->fill_solid_rgb[2] = fill_rgb[2];
        return 1;
    }
    return 0;
}

static int
sixel_builtin_psd_ascii_is_space(unsigned char value)
{
    return value == (unsigned char)' ' ||
           value == (unsigned char)'\t' ||
           value == (unsigned char)'\n' ||
           value == (unsigned char)'\r' ||
           value == (unsigned char)'\f' ||
           value == (unsigned char)'\v';
}

static int
sixel_builtin_psd_ascii_is_numeric_char(unsigned char value)
{
    return (value >= (unsigned char)'0' && value <= (unsigned char)'9') ||
           value == (unsigned char)'+' ||
           value == (unsigned char)'-' ||
           value == (unsigned char)'.' ||
           value == (unsigned char)'e' ||
           value == (unsigned char)'E';
}

static int
sixel_builtin_psd_parse_ascii_float(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    double *out_value)
{
    size_t cursor;
    size_t start;
    size_t token_length;
    char token[64];
    char *endp;
    double parsed;

    cursor = 0u;
    start = 0u;
    token_length = 0u;
    endp = NULL;
    parsed = 0.0;
    if (data == NULL || pcursor == NULL || out_value == NULL) {
        return 0;
    }
    cursor = *pcursor;
    while (cursor < data_length && sixel_builtin_psd_ascii_is_space(data[cursor])) {
        ++cursor;
    }
    start = cursor;
    while (cursor < data_length &&
           sixel_builtin_psd_ascii_is_numeric_char(data[cursor])) {
        ++cursor;
    }
    if (cursor == start) {
        return 0;
    }
    token_length = cursor - start;
    if (token_length == 0u || token_length >= sizeof(token)) {
        return 0;
    }
    memcpy(token, data + start, token_length);
    token[token_length] = '\0';
    parsed = strtod(token, &endp);
    if (endp == NULL || *endp != '\0') {
        return 0;
    }
    *out_value = parsed;
    *pcursor = cursor;
    return 1;
}

static float
sixel_builtin_psd_normalize_rgb_engine_component(double value)
{
    if (value != value) {
        return 0.0f;
    }
    if (value <= 1.0 && value >= 0.0) {
        return sixel_builtin_psd_clamp01((float)value);
    }
    if (value <= 255.0 && value >= 0.0) {
        return sixel_builtin_psd_clamp01((float)(value / 255.0));
    }
    return value < 0.0 ? 0.0f : 1.0f;
}

static float
sixel_builtin_psd_normalize_gray_engine_component(double value)
{
    if (value != value) {
        return 0.0f;
    }
    if (value <= 1.0 && value >= 0.0) {
        return sixel_builtin_psd_clamp01((float)value);
    }
    if (value <= 100.0 && value >= 0.0) {
        return sixel_builtin_psd_clamp01((float)(value / 100.0));
    }
    if (value <= 255.0 && value >= 0.0) {
        return sixel_builtin_psd_clamp01((float)(value / 255.0));
    }
    return value < 0.0 ? 0.0f : 1.0f;
}

static float
sixel_builtin_psd_normalize_cmyk_engine_component(double value)
{
    if (value != value) {
        return 0.0f;
    }
    if (value <= 1.0 && value >= 0.0) {
        return sixel_builtin_psd_clamp01((float)value);
    }
    if (value <= 100.0 && value >= 0.0) {
        return sixel_builtin_psd_clamp01((float)(value / 100.0));
    }
    if (value <= 255.0 && value >= 0.0) {
        return sixel_builtin_psd_clamp01((float)(value / 255.0));
    }
    return value < 0.0 ? 0.0f : 1.0f;
}

typedef enum sixel_builtin_psd_tysh_engine_fillcolor_kind {
    SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_UNKNOWN = 0,
    SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_GRAY = 1,
    SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_RGB = 2,
    SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_HSB = 3,
    SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_CMYK = 4,
    SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_LAB = 5
} sixel_builtin_psd_tysh_engine_fillcolor_kind_t;

static int
sixel_builtin_psd_contains_ascii_token_in_range(
    unsigned char const *data,
    size_t begin,
    size_t end,
    char const *token,
    size_t token_length)
{
    size_t i;

    i = 0u;
    if (data == NULL || token == NULL || token_length == 0u ||
        begin > end || end - begin < token_length) {
        return 0;
    }
    for (i = begin; i + token_length <= end; ++i) {
        if (memcmp(data + i, token, token_length) == 0) {
            return 1;
        }
    }
    return 0;
}

static sixel_builtin_psd_tysh_engine_fillcolor_kind_t
sixel_builtin_psd_detect_tysh_engine_fillcolor_kind(
    unsigned char const *data,
    size_t begin,
    size_t end)
{
    static char const color_space_token[] = "/ColorSpace";
    size_t i;
    size_t cursor;
    size_t token_begin;
    size_t token_end;

    i = 0u;
    cursor = 0u;
    token_begin = 0u;
    token_end = 0u;
    if (data == NULL || begin >= end) {
        return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_UNKNOWN;
    }
    if (end - begin >= sizeof(color_space_token) - 1u) {
        for (i = begin; i + sizeof(color_space_token) - 1u <= end; ++i) {
            if (memcmp(data + i, color_space_token, sizeof(color_space_token) - 1u) != 0) {
                continue;
            }
            cursor = i + sizeof(color_space_token) - 1u;
            while (cursor < end && sixel_builtin_psd_ascii_is_space(data[cursor])) {
                ++cursor;
            }
            if (cursor >= end || data[cursor] != (unsigned char)'/') {
                continue;
            }
            token_begin = cursor;
            ++cursor;
            while (cursor < end &&
                   !sixel_builtin_psd_ascii_is_space(data[cursor]) &&
                   data[cursor] != (unsigned char)'/' &&
                   data[cursor] != (unsigned char)'[' &&
                   data[cursor] != (unsigned char)']' &&
                   data[cursor] != (unsigned char)'<' &&
                   data[cursor] != (unsigned char)'>') {
                ++cursor;
            }
            token_end = cursor;
            if (token_end <= token_begin) {
                continue;
            }
            if (token_end - token_begin == 4u &&
                memcmp(data + token_begin, "/Lab", 4u) == 0) {
                return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_LAB;
            }
            if (token_end - token_begin == 7u &&
                memcmp(data + token_begin, "/CIELab", 7u) == 0) {
                return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_LAB;
            }
            if (token_end - token_begin == 4u &&
                memcmp(data + token_begin, "/RGB", 4u) == 0) {
                return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_RGB;
            }
            if (token_end - token_begin == 10u &&
                memcmp(data + token_begin, "/DeviceRGB", 10u) == 0) {
                return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_RGB;
            }
            if (token_end - token_begin == 4u &&
                memcmp(data + token_begin, "/HSB", 4u) == 0) {
                return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_HSB;
            }
            if (token_end - token_begin == 5u &&
                memcmp(data + token_begin, "/CMYK", 5u) == 0) {
                return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_CMYK;
            }
            if (token_end - token_begin == 11u &&
                memcmp(data + token_begin, "/DeviceCMYK", 11u) == 0) {
                return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_CMYK;
            }
            if (token_end - token_begin == 5u &&
                memcmp(data + token_begin, "/Gray", 5u) == 0) {
                return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_GRAY;
            }
            if (token_end - token_begin == 11u &&
                memcmp(data + token_begin, "/DeviceGray", 11u) == 0) {
                return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_GRAY;
            }
        }
    }

    if (sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/Lab",
            4u) ||
        sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/LAB",
            4u) ||
        sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/CIELab",
            7u) ||
        sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/LbCl",
            5u)) {
        return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_LAB;
    }
    if (sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/CMYK",
            5u) ||
        sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/CMYC",
            5u) ||
        sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/DeviceCMYK",
            11u) ||
        sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/Cmyk",
            5u)) {
        return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_CMYK;
    }
    if (sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/Gray",
            5u) ||
        sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/Gry",
            4u) ||
        sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/DeviceGray",
            11u) ||
        sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/Grsc",
            5u)) {
        return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_GRAY;
    }
    if (sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/HSB",
            4u) ||
        sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/HSBC",
            5u) ||
        sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/Hsb",
            4u)) {
        return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_HSB;
    }
    if (sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/RGB",
            4u) ||
        sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/DeviceRGB",
            10u) ||
        sixel_builtin_psd_contains_ascii_token_in_range(
            data,
            begin,
            end,
            "/RGBC",
            5u)) {
        return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_RGB;
    }
    return SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_UNKNOWN;
}

static int
sixel_builtin_psd_find_ascii_dict_end(
    unsigned char const *data,
    size_t data_length,
    size_t dict_begin,
    size_t *pdict_end)
{
    size_t cursor;
    unsigned int depth;

    cursor = 0u;
    depth = 0u;
    if (data == NULL || pdict_end == NULL ||
        dict_begin + 1u >= data_length ||
        data[dict_begin] != (unsigned char)'<' ||
        data[dict_begin + 1u] != (unsigned char)'<') {
        return 0;
    }
    cursor = dict_begin + 2u;
    depth = 1u;
    while (cursor + 1u < data_length) {
        if (data[cursor] == (unsigned char)'<' &&
            data[cursor + 1u] == (unsigned char)'<') {
            ++depth;
            cursor += 2u;
            continue;
        }
        if (data[cursor] == (unsigned char)'>' &&
            data[cursor + 1u] == (unsigned char)'>') {
            if (depth == 0u) {
                return 0;
            }
            --depth;
            if (depth == 0u) {
                *pdict_end = cursor;
                return 1;
            }
            cursor += 2u;
            continue;
        }
        ++cursor;
    }
    return 0;
}

static int
sixel_builtin_psd_apply_tysh_fillcolor_engine_lab(
    sixel_builtin_psd_layer_record_t *layer,
    double lab_l,
    double lab_a,
    double lab_b)
{
    SIXELSTATUS status;
    float lab_pixel[3];
    float rgb_gamma[3];

    status = SIXEL_FALSE;
    lab_pixel[0] = 0.0f;
    lab_pixel[1] = 0.0f;
    lab_pixel[2] = 0.0f;
    rgb_gamma[0] = 0.0f;
    rgb_gamma[1] = 0.0f;
    rgb_gamma[2] = 0.0f;
    if (layer == NULL) {
        return 0;
    }

    if (lab_l >= 0.0 && lab_l <= 1.0) {
        lab_l *= 100.0;
    } else if (lab_l > 100.0 && lab_l <= 255.0) {
        lab_l = (lab_l / 255.0) * 100.0;
    }
    if (lab_l < 0.0) {
        lab_l = 0.0;
    } else if (lab_l > 100.0) {
        lab_l = 100.0;
    }

    if (lab_a >= -1.5 && lab_a <= 1.5) {
        lab_a *= 128.0;
    } else if (lab_a >= 128.0 && lab_a <= 255.0) {
        lab_a = lab_a - 128.0;
    }
    if (lab_b >= -1.5 && lab_b <= 1.5) {
        lab_b *= 128.0;
    } else if (lab_b >= 128.0 && lab_b <= 255.0) {
        lab_b = lab_b - 128.0;
    }
    if (lab_a < -192.0) {
        lab_a = -192.0;
    } else if (lab_a > 192.0) {
        lab_a = 192.0;
    }
    if (lab_b < -192.0) {
        lab_b = -192.0;
    } else if (lab_b > 192.0) {
        lab_b = 192.0;
    }

    lab_pixel[0] = (float)(lab_l / 100.0);
    lab_pixel[1] = sixel_builtin_psd_lab_clamp_ab((float)(lab_a / 128.0));
    lab_pixel[2] = sixel_builtin_psd_lab_clamp_ab((float)(lab_b / 128.0));
    status = sixel_helper_convert_colorspace((unsigned char *)lab_pixel,
                                             sizeof(lab_pixel),
                                             SIXEL_PIXELFORMAT_CIELABFLOAT32,
                                             SIXEL_COLORSPACE_CIELAB,
                                             SIXEL_COLORSPACE_GAMMA);
    if (SIXEL_FAILED(status)) {
        return 0;
    }
    rgb_gamma[0] = sixel_builtin_psd_clamp01(lab_pixel[0]);
    rgb_gamma[1] = sixel_builtin_psd_clamp01(lab_pixel[1]);
    rgb_gamma[2] = sixel_builtin_psd_clamp01(lab_pixel[2]);

    layer->fill_kind = SIXEL_BUILTIN_PSD_FILL_SOCO;
    layer->fill_solid_rgb[0] = sixel_builtin_psd_gamma_to_linear(rgb_gamma[0]);
    layer->fill_solid_rgb[1] = sixel_builtin_psd_gamma_to_linear(rgb_gamma[1]);
    layer->fill_solid_rgb[2] = sixel_builtin_psd_gamma_to_linear(rgb_gamma[2]);
    return 1;
}

static int
sixel_builtin_psd_parse_tysh_fillcolor_engine_array(
    unsigned char const *data,
    size_t data_length,
    size_t *pcursor,
    double components[4],
    size_t *pcount)
{
    size_t cursor;
    size_t token_cursor;
    size_t count;

    cursor = 0u;
    token_cursor = 0u;
    count = 0u;
    if (data == NULL || pcursor == NULL || components == NULL || pcount == NULL) {
        return 0;
    }
    cursor = *pcursor;
    if (cursor >= data_length || data[cursor] != (unsigned char)'[') {
        return 0;
    }
    ++cursor;
    while (cursor < data_length && count < 4u) {
        while (cursor < data_length && sixel_builtin_psd_ascii_is_space(data[cursor])) {
            ++cursor;
        }
        if (cursor >= data_length) {
            break;
        }
        if (data[cursor] == (unsigned char)']') {
            ++cursor;
            break;
        }
        if (!sixel_builtin_psd_parse_ascii_float(data,
                                                 data_length,
                                                 &cursor,
                                                 &components[count])) {
            /* TySh EngineData can prefix values arrays with names such as
             * [/CMYK ...]. Skip non-numeric tokens leniently and keep
             * collecting numeric components.
             */
            token_cursor = cursor;
            if (token_cursor < data_length &&
                data[token_cursor] == (unsigned char)'/') {
                ++token_cursor;
            }
            while (token_cursor < data_length &&
                   !sixel_builtin_psd_ascii_is_space(data[token_cursor]) &&
                   data[token_cursor] != (unsigned char)']' &&
                   data[token_cursor] != (unsigned char)'[' &&
                   data[token_cursor] != (unsigned char)'<' &&
                   data[token_cursor] != (unsigned char)'>') {
                ++token_cursor;
            }
            if (token_cursor == cursor) {
                ++cursor;
            } else {
                cursor = token_cursor;
            }
            continue;
        }
        ++count;
        while (cursor < data_length && sixel_builtin_psd_ascii_is_space(data[cursor])) {
            ++cursor;
        }
        if (cursor < data_length && data[cursor] == (unsigned char)']') {
            ++cursor;
            break;
        }
    }
    if (count == 0u) {
        return 0;
    }
    *pcursor = cursor;
    *pcount = count;
    return 1;
}

static void
sixel_builtin_psd_hsb_engine_components_to_rgb(
    double hue,
    double saturation,
    double brightness,
    float *pr,
    float *pg,
    float *pb)
{
    double h_norm;
    double s_norm;
    double v_norm;
    double chroma;
    double h_prime;
    double x;
    double m;
    double r1;
    double g1;
    double b1;

    h_norm = 0.0;
    s_norm = 0.0;
    v_norm = 0.0;
    chroma = 0.0;
    h_prime = 0.0;
    x = 0.0;
    m = 0.0;
    r1 = 0.0;
    g1 = 0.0;
    b1 = 0.0;

    if (hue == hue && hue >= 0.0 && hue <= 1.0) {
        h_norm = hue * 360.0;
    } else if (hue == hue) {
        h_norm = hue;
    }
    while (h_norm < 0.0) {
        h_norm += 360.0;
    }
    while (h_norm >= 360.0) {
        h_norm -= 360.0;
    }

    if (saturation == saturation && saturation >= 0.0 && saturation <= 1.0) {
        s_norm = saturation;
    } else if (saturation == saturation && saturation <= 100.0) {
        s_norm = saturation / 100.0;
    } else if (saturation == saturation && saturation <= 255.0) {
        s_norm = saturation / 255.0;
    } else if (saturation == saturation) {
        s_norm = 1.0;
    }
    if (s_norm < 0.0) {
        s_norm = 0.0;
    } else if (s_norm > 1.0) {
        s_norm = 1.0;
    }

    if (brightness == brightness && brightness >= 0.0 && brightness <= 1.0) {
        v_norm = brightness;
    } else if (brightness == brightness && brightness <= 100.0) {
        v_norm = brightness / 100.0;
    } else if (brightness == brightness && brightness <= 255.0) {
        v_norm = brightness / 255.0;
    } else if (brightness == brightness) {
        v_norm = 1.0;
    }
    if (v_norm < 0.0) {
        v_norm = 0.0;
    } else if (v_norm > 1.0) {
        v_norm = 1.0;
    }

    chroma = v_norm * s_norm;
    h_prime = h_norm / 60.0;
    x = chroma * (1.0 - fabs(fmod(h_prime, 2.0) - 1.0));
    m = v_norm - chroma;
    if (h_prime < 1.0) {
        r1 = chroma;
        g1 = x;
    } else if (h_prime < 2.0) {
        r1 = x;
        g1 = chroma;
    } else if (h_prime < 3.0) {
        g1 = chroma;
        b1 = x;
    } else if (h_prime < 4.0) {
        g1 = x;
        b1 = chroma;
    } else if (h_prime < 5.0) {
        r1 = x;
        b1 = chroma;
    } else {
        r1 = chroma;
        b1 = x;
    }

    if (pr != NULL) {
        *pr = sixel_builtin_psd_clamp01((float)(r1 + m));
    }
    if (pg != NULL) {
        *pg = sixel_builtin_psd_clamp01((float)(g1 + m));
    }
    if (pb != NULL) {
        *pb = sixel_builtin_psd_clamp01((float)(b1 + m));
    }
}

static int
sixel_builtin_psd_apply_tysh_fillcolor_engine_components(
    sixel_builtin_psd_layer_record_t *layer,
    double const components[4],
    size_t count,
    sixel_builtin_psd_tysh_engine_fillcolor_kind_t color_kind)
{
    float r;
    float g;
    float b;
    float c;
    float m;
    float y;
    float k;

    r = 0.0f;
    g = 0.0f;
    b = 0.0f;
    c = 0.0f;
    m = 0.0f;
    y = 0.0f;
    k = 0.0f;
    if (layer == NULL || components == NULL || count == 0u) {
        return 0;
    }

    if (color_kind == SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_LAB &&
        count >= 3u) {
        return sixel_builtin_psd_apply_tysh_fillcolor_engine_lab(layer,
                                                                  components[0],
                                                                  components[1],
                                                                  components[2]);
    }

    if ((color_kind == SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_CMYK &&
         count >= 4u) ||
        (color_kind == SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_UNKNOWN &&
         count >= 4u)) {
        c = sixel_builtin_psd_normalize_cmyk_engine_component(components[0]);
        m = sixel_builtin_psd_normalize_cmyk_engine_component(components[1]);
        y = sixel_builtin_psd_normalize_cmyk_engine_component(components[2]);
        k = sixel_builtin_psd_normalize_cmyk_engine_component(components[3]);
        r = (1.0f - c) * (1.0f - k);
        g = (1.0f - m) * (1.0f - k);
        b = (1.0f - y) * (1.0f - k);
    } else if (color_kind == SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_HSB &&
               count >= 3u) {
        sixel_builtin_psd_hsb_engine_components_to_rgb(components[0],
                                                       components[1],
                                                       components[2],
                                                       &r,
                                                       &g,
                                                       &b);
    } else if ((color_kind == SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_RGB &&
                count >= 3u) ||
               (color_kind == SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_UNKNOWN &&
                count >= 3u)) {
        r = sixel_builtin_psd_normalize_rgb_engine_component(components[0]);
        g = sixel_builtin_psd_normalize_rgb_engine_component(components[1]);
        b = sixel_builtin_psd_normalize_rgb_engine_component(components[2]);
    } else {
        r = sixel_builtin_psd_normalize_gray_engine_component(components[0]);
        g = r;
        b = r;
    }

    layer->fill_kind = SIXEL_BUILTIN_PSD_FILL_SOCO;
    layer->fill_solid_rgb[0] = sixel_builtin_psd_gamma_to_linear(
        sixel_builtin_psd_clamp01(r));
    layer->fill_solid_rgb[1] = sixel_builtin_psd_gamma_to_linear(
        sixel_builtin_psd_clamp01(g));
    layer->fill_solid_rgb[2] = sixel_builtin_psd_gamma_to_linear(
        sixel_builtin_psd_clamp01(b));
    return 1;
}

static int
sixel_builtin_psd_parse_tysh_fillcolor_enginedata_dict(
    unsigned char const *data,
    size_t dict_begin,
    size_t dict_end,
    sixel_builtin_psd_layer_record_t *layer)
{
    static char const values_token[] = "/Values";
    size_t j;
    size_t values_cursor;
    size_t count;
    double components[4];
    sixel_builtin_psd_tysh_engine_fillcolor_kind_t color_kind;

    j = 0u;
    values_cursor = 0u;
    count = 0u;
    components[0] = 0.0;
    components[1] = 0.0;
    components[2] = 0.0;
    components[3] = 0.0;
    color_kind = SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_UNKNOWN;
    if (data == NULL || layer == NULL || dict_begin > dict_end) {
        return 0;
    }

    color_kind = sixel_builtin_psd_detect_tysh_engine_fillcolor_kind(
        data,
        dict_begin,
        dict_end);
    for (j = dict_begin; j + sizeof(values_token) - 1u <= dict_end; ++j) {
        if (memcmp(data + j, values_token, sizeof(values_token) - 1u) != 0) {
            continue;
        }
        values_cursor = j + sizeof(values_token) - 1u;
        while (values_cursor < dict_end &&
               sixel_builtin_psd_ascii_is_space(data[values_cursor])) {
            ++values_cursor;
        }
        count = 0u;
        if (sixel_builtin_psd_parse_tysh_fillcolor_engine_array(
                data,
                dict_end,
                &values_cursor,
                components,
                &count)) {
            return sixel_builtin_psd_apply_tysh_fillcolor_engine_components(
                layer,
                components,
                count,
                color_kind);
        }
    }

    values_cursor = dict_begin;
    while (values_cursor < dict_end && data[values_cursor] != (unsigned char)'[') {
        ++values_cursor;
    }
    count = 0u;
    if (values_cursor < dict_end &&
        sixel_builtin_psd_parse_tysh_fillcolor_engine_array(
            data,
            dict_end,
            &values_cursor,
            components,
            &count)) {
        return sixel_builtin_psd_apply_tysh_fillcolor_engine_components(
            layer,
            components,
            count,
            color_kind);
    }
    return 0;
}

static int
sixel_builtin_psd_parse_tysh_fillcolor_enginedata_at(
    unsigned char const *data,
    size_t key_length,
    size_t token_offset,
    sixel_builtin_psd_layer_record_t *layer)
{
    static char const token[] = "/FillColor";
    size_t cursor;
    size_t dict_end;
    size_t count;
    double components[4];

    cursor = 0u;
    dict_end = 0u;
    count = 0u;
    components[0] = 0.0;
    components[1] = 0.0;
    components[2] = 0.0;
    components[3] = 0.0;
    if (data == NULL || layer == NULL ||
        token_offset + sizeof(token) > key_length) {
        return 0;
    }
    cursor = token_offset + sizeof(token) - 1u;
    while (cursor < key_length && sixel_builtin_psd_ascii_is_space(data[cursor])) {
        ++cursor;
    }
    if (cursor >= key_length) {
        return 0;
    }

    count = 0u;
    if (sixel_builtin_psd_parse_tysh_fillcolor_engine_array(
            data,
            key_length,
            &cursor,
            components,
            &count)) {
        return sixel_builtin_psd_apply_tysh_fillcolor_engine_components(
            layer,
            components,
            count,
            SIXEL_BUILTIN_PSD_TYSH_ENGINE_FILLCOLOR_UNKNOWN);
    }

    if (cursor + 1u >= key_length ||
        data[cursor] != (unsigned char)'<' ||
        data[cursor + 1u] != (unsigned char)'<') {
        return 0;
    }
    if (!sixel_builtin_psd_find_ascii_dict_end(data,
                                               key_length,
                                               cursor,
                                               &dict_end)) {
        return 0;
    }
    return sixel_builtin_psd_parse_tysh_fillcolor_enginedata_dict(
        data,
        cursor + 2u,
        dict_end,
        layer);
}

static int
sixel_builtin_psd_parse_tysh_fillcolor_enginedata_scope(
    unsigned char const *data,
    size_t begin,
    size_t end,
    sixel_builtin_psd_layer_record_t *layer)
{
    static char const token[] = "/FillColor";
    size_t i;

    i = 0u;
    if (data == NULL || layer == NULL ||
        begin > end || end - begin < sizeof(token) - 1u) {
        return 0;
    }
    for (i = begin; i + sizeof(token) - 1u <= end; ++i) {
        if (memcmp(data + i, token, sizeof(token) - 1u) != 0) {
            continue;
        }
        if (sixel_builtin_psd_parse_tysh_fillcolor_enginedata_at(
                data,
                end,
                i,
                layer)) {
            return 1;
        }
    }
    return 0;
}

static int
sixel_builtin_psd_parse_tysh_fillcolor_enginedata_stylesheet(
    unsigned char const *data,
    size_t key_length,
    sixel_builtin_psd_layer_record_t *layer)
{
    static char const token[] = "/StyleSheetData";
    size_t i;
    size_t cursor;
    size_t dict_end;

    i = 0u;
    cursor = 0u;
    dict_end = 0u;
    if (data == NULL || layer == NULL || key_length < sizeof(token)) {
        return 0;
    }
    for (i = 0u; i + sizeof(token) - 1u <= key_length; ++i) {
        if (memcmp(data + i, token, sizeof(token) - 1u) != 0) {
            continue;
        }
        cursor = i + sizeof(token) - 1u;
        while (cursor < key_length && sixel_builtin_psd_ascii_is_space(data[cursor])) {
            ++cursor;
        }
        if (cursor + 1u >= key_length ||
            data[cursor] != (unsigned char)'<' ||
            data[cursor + 1u] != (unsigned char)'<') {
            continue;
        }
        if (!sixel_builtin_psd_find_ascii_dict_end(data,
                                                   key_length,
                                                   cursor,
                                                   &dict_end)) {
            continue;
        }
        if (sixel_builtin_psd_parse_tysh_fillcolor_enginedata_scope(
                data,
                cursor + 2u,
                dict_end,
                layer)) {
            return 1;
        }
    }
    return 0;
}

static int
sixel_builtin_psd_parse_tysh_fillcolor_enginedata(
    unsigned char const *data,
    size_t key_length,
    sixel_builtin_psd_layer_record_t *layer)
{
    if (data == NULL || layer == NULL || key_length == 0u) {
        return 0;
    }
    if (sixel_builtin_psd_parse_tysh_fillcolor_enginedata_scope(
            data,
            0u,
            key_length,
            layer)) {
        return 1;
    }
    return sixel_builtin_psd_parse_tysh_fillcolor_enginedata_stylesheet(
        data,
        key_length,
        layer);
}

static int
sixel_builtin_psd_parse_tysh_payload(
    unsigned char const *data,
    size_t key_length,
    sixel_builtin_psd_layer_record_t *layer)
{
    size_t cursor;
    size_t descriptor_start;
    size_t descriptor_end;

    cursor = 0u;
    descriptor_start = 0u;
    descriptor_end = 0u;
    if (data == NULL || layer == NULL) {
        return 0;
    }

    /* Backward-compatible path used by existing minimal fixtures. */
    if (sixel_builtin_psd_parse_soco_descriptor_payload(data, key_length, layer)) {
        return 1;
    }

    /* TySh common structure:
     * version(4) + transform(6*8) + text_desc_version(4) + descriptor...
     */
    if (key_length < 56u) {
        return 0;
    }
    if (sixel_builtin_read_u32be_size(data) != 1u) {
        return 0;
    }
    cursor = 4u + 48u;  /* version + matrix */
    if (cursor + 4u > key_length) {
        return 0;
    }
    cursor += 4u;  /* text descriptor version */
    descriptor_start = cursor;
    if (sixel_builtin_psd_descriptor_skip_object(data, key_length, &cursor, 0u)) {
        descriptor_end = cursor;
        if (descriptor_end > descriptor_start &&
            sixel_builtin_psd_parse_soco_descriptor_payload(
                data + descriptor_start,
                descriptor_end - descriptor_start,
                layer)) {
            return 1;
        }
    }

    /* Optional warp descriptor follows in many TySh payloads.
     * Parse opportunistically, but keep this path non-fatal.
     */
    if (descriptor_end > descriptor_start &&
        descriptor_end + 8u <= key_length) {
        cursor = descriptor_end + 8u;  /* warp version + warp descriptor version */
        descriptor_start = cursor;
        if (sixel_builtin_psd_descriptor_skip_object(data,
                                                     key_length,
                                                     &cursor,
                                                     0u)) {
            descriptor_end = cursor;
            if (descriptor_end > descriptor_start &&
                sixel_builtin_psd_parse_soco_descriptor_payload(
                    data + descriptor_start,
                    descriptor_end - descriptor_start,
                    layer)) {
                return 1;
            }
        }
    }

    /* Lenient fallback for TySh wrappers that carry additional/unknown items:
     * scan for Clr/Objc signatures and parse only the color object.
     */
    descriptor_start = 4u + 48u + 4u;
    if (descriptor_start < key_length &&
        sixel_builtin_psd_parse_soco_descriptor_payload_loose(
            data + descriptor_start,
            key_length - descriptor_start,
            layer)) {
        return 1;
    }
    if (descriptor_start < key_length &&
        sixel_builtin_psd_parse_tysh_fillcolor_enginedata(
            data + descriptor_start,
            key_length - descriptor_start,
            layer)) {
        return 1;
    }
    return 0;
}

static int
sixel_builtin_psd_tysh_payload_looks_wrapped(
    unsigned char const *data,
    size_t key_length)
{
    size_t cursor;

    cursor = 0u;
    if (data == NULL || key_length < 56u) {
        return 0;
    }
    if (sixel_builtin_read_u32be_size(data) != 1u) {
        return 0;
    }
    cursor = 4u + 48u;  /* version + matrix */
    if (cursor + 4u > key_length) {
        return 0;
    }
    return 1;
}

static int
sixel_builtin_psd_parse_fill_payload_sxfl(
    char const key[5],
    unsigned char const *data,
    size_t key_length,
    sixel_builtin_psd_layer_record_t *layer)
{
    size_t i;
    size_t cursor;
    size_t stop_count;
    int angle_i16;

    i = 0u;
    cursor = 0u;
    stop_count = 0u;
    angle_i16 = 0;
    if (key == NULL || data == NULL || layer == NULL) {
        return 0;
    }
    if (key_length < 6u ||
        memcmp(data, "SXFL", 4u) != 0 ||
        data[4] != 1u) {
        return 0;
    }
    if (memcmp(key, "SoCo", 4u) == 0) {
        if (data[5] != 1u || key_length < 11u) {
            return 0;
        }
        layer->fill_kind = SIXEL_BUILTIN_PSD_FILL_SOCO;
        sixel_builtin_psd_fill_set_rgb_from_u8(layer->fill_solid_rgb,
                                               data[8],
                                               data[9],
                                               data[10]);
        return 1;
    }
    if (memcmp(key, "GdFl", 4u) == 0) {
        if (data[5] != 2u || key_length < 13u) {
            return 0;
        }
        layer->fill_kind = SIXEL_BUILTIN_PSD_FILL_GDFL;
        layer->fill_gradient_type = data[6];
        layer->fill_gradient_reverse = data[7] ? 1 : 0;
        angle_i16 = (int)((int16_t)sixel_builtin_read_u16be(data + 8u));
        layer->fill_gradient_angle_deg = (float)angle_i16 / 10.0f;
        layer->fill_gradient_scale =
            (float)sixel_builtin_read_u16be(data + 10u) / 100.0f;
        if (layer->fill_gradient_scale <= 0.0f) {
            layer->fill_gradient_scale = 1.0f;
        }
        stop_count = (size_t)data[12];
        cursor = 13u;
        if (stop_count > SIXEL_BUILTIN_PSD_FILL_STOP_MAX) {
            stop_count = SIXEL_BUILTIN_PSD_FILL_STOP_MAX;
        }
        for (i = 0u; i < stop_count; ++i) {
            float pos;
            if (cursor + 6u > key_length) {
                break;
            }
            pos = (float)sixel_builtin_read_u16be(data + cursor) / 65535.0f;
            layer->fill_gradient_stop_pos[i] = sixel_builtin_psd_clamp01(pos);
            sixel_builtin_psd_fill_set_rgb_from_u8(
                layer->fill_gradient_stop_rgb[i],
                data[cursor + 2u],
                data[cursor + 3u],
                data[cursor + 4u]);
            layer->fill_gradient_stop_alpha[i] =
                sixel_builtin_psd_clamp_alpha_float32(
                    (float)data[cursor + 5u] / 255.0f);
            cursor += 6u;
        }
        layer->fill_gradient_stop_count = i;
        if (layer->fill_gradient_stop_count == 0u) {
            layer->fill_gradient_stop_count = 2u;
            layer->fill_gradient_stop_pos[0] = 0.0f;
            layer->fill_gradient_stop_pos[1] = 1.0f;
            layer->fill_gradient_stop_rgb[0][0] = 0.0f;
            layer->fill_gradient_stop_rgb[0][1] = 0.0f;
            layer->fill_gradient_stop_rgb[0][2] = 0.0f;
            layer->fill_gradient_stop_rgb[1][0] = 1.0f;
            layer->fill_gradient_stop_rgb[1][1] = 1.0f;
            layer->fill_gradient_stop_rgb[1][2] = 1.0f;
            layer->fill_gradient_stop_alpha[0] = 1.0f;
            layer->fill_gradient_stop_alpha[1] = 1.0f;
        }
        return 1;
    }
    if (memcmp(key, "PtFl", 4u) == 0) {
        if (data[5] != 3u || key_length < 13u) {
            return 0;
        }
        layer->fill_kind = SIXEL_BUILTIN_PSD_FILL_PTFL;
        layer->fill_pattern_tile = (unsigned int)data[6];
        if (layer->fill_pattern_tile < 1u) {
            layer->fill_pattern_tile = 1u;
        }
        sixel_builtin_psd_fill_set_rgb_from_u8(layer->fill_pattern_fg_rgb,
                                               data[7],
                                               data[8],
                                               data[9]);
        sixel_builtin_psd_fill_set_rgb_from_u8(layer->fill_pattern_bg_rgb,
                                               data[10],
                                               data[11],
                                               data[12]);
        return 1;
    }
    return 0;
}

static int
sixel_builtin_psd_parse_fill_payload(
    char const key[5],
    unsigned char const *data,
    size_t key_length,
    sixel_builtin_psd_layer_record_t *layer)
{
    if (key == NULL || data == NULL || layer == NULL) {
        return 0;
    }
    if (sixel_builtin_psd_parse_fill_payload_sxfl(key,
                                                  data,
                                                  key_length,
                                                  layer)) {
        return 1;
    }
    if (memcmp(key, "SoCo", 4u) == 0) {
        return sixel_builtin_psd_parse_soco_descriptor_payload(data,
                                                               key_length,
                                                               layer);
    } else if (memcmp(key, "GdFl", 4u) == 0) {
        return sixel_builtin_psd_parse_gdfl_descriptor_payload(data,
                                                               key_length,
                                                               layer);
    } else if (memcmp(key, "PtFl", 4u) == 0) {
        return sixel_builtin_psd_parse_ptfl_descriptor_payload(data,
                                                               key_length,
                                                               layer);
    }
    return 0;
}

static int
sixel_builtin_psd_layer_blend_mode_from_key(
    char const key[5],
    sixel_builtin_psd_layer_blend_mode_t *mode)
{
    if (key == NULL || mode == NULL) {
        return 0;
    }
    if (memcmp(key, "norm", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_NORMAL;
    } else if (memcmp(key, "diss", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_DISSOLVE;
    } else if (memcmp(key, "dark", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_DARKEN;
    } else if (memcmp(key, "mul ", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_MULTIPLY;
    } else if (memcmp(key, "idiv", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_COLOR_BURN;
    } else if (memcmp(key, "lbrn", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_LINEAR_BURN;
    } else if (memcmp(key, "dkCl", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_DARKER_COLOR;
    } else if (memcmp(key, "lite", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_LIGHTEN;
    } else if (memcmp(key, "scrn", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_SCREEN;
    } else if (memcmp(key, "div ", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_COLOR_DODGE;
    } else if (memcmp(key, "lddg", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_LINEAR_DODGE;
    } else if (memcmp(key, "lgCl", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_LIGHTER_COLOR;
    } else if (memcmp(key, "over", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_OVERLAY;
    } else if (memcmp(key, "sLit", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_SOFT_LIGHT;
    } else if (memcmp(key, "hLit", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_HARD_LIGHT;
    } else if (memcmp(key, "vLit", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_VIVID_LIGHT;
    } else if (memcmp(key, "lLit", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_LINEAR_LIGHT;
    } else if (memcmp(key, "pLit", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_PIN_LIGHT;
    } else if (memcmp(key, "hMix", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_HARD_MIX;
    } else if (memcmp(key, "diff", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_DIFFERENCE;
    } else if (memcmp(key, "smud", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_EXCLUSION;
    } else if (memcmp(key, "fsub", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_SUBTRACT;
    } else if (memcmp(key, "fdiv", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_DIVIDE;
    } else if (memcmp(key, "hue ", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_HUE;
    } else if (memcmp(key, "sat ", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_SATURATION;
    } else if (memcmp(key, "colr", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_COLOR;
    } else if (memcmp(key, "lum ", 4u) == 0) {
        *mode = SIXEL_BUILTIN_PSD_BLEND_LUMINOSITY;
    } else {
        return 0;
    }
    return 1;
}

static int
sixel_builtin_psd_skip_pascal_string_padded4(
    unsigned char const *buffer,
    size_t limit,
    size_t *pcursor)
{
    size_t cursor;
    size_t start;
    size_t name_length;

    if (buffer == NULL || pcursor == NULL) {
        return 0;
    }
    cursor = *pcursor;
    if (cursor >= limit) {
        return 0;
    }
    start = cursor;
    name_length = (size_t)buffer[cursor];
    ++cursor;
    if (name_length > limit - cursor) {
        return 0;
    }
    cursor += name_length;
    while (((cursor - start) & 3u) != 0u) {
        if (cursor >= limit) {
            return 0;
        }
        ++cursor;
    }
    *pcursor = cursor;
    return 1;
}

static float
sixel_builtin_psd_rgb_luma(float r, float g, float b)
{
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

static void
sixel_builtin_psd_rgb_to_hsl(float r,
                             float g,
                             float b,
                             float *ph,
                             float *ps,
                             float *pl)
{
    float maxv;
    float minv;
    float d;
    float h;
    float s;
    float l;

    maxv = fmaxf(r, fmaxf(g, b));
    minv = fminf(r, fminf(g, b));
    d = maxv - minv;
    l = 0.5f * (maxv + minv);
    h = 0.0f;
    s = 0.0f;
    if (d > 1.0e-8f) {
        if (l < 0.5f) {
            s = d / (maxv + minv);
        } else {
            s = d / (2.0f - maxv - minv);
        }
        if (maxv == r) {
            h = (g - b) / d + (g < b ? 6.0f : 0.0f);
        } else if (maxv == g) {
            h = (b - r) / d + 2.0f;
        } else {
            h = (r - g) / d + 4.0f;
        }
        h /= 6.0f;
    }
    if (ph != NULL) {
        *ph = h;
    }
    if (ps != NULL) {
        *ps = s;
    }
    if (pl != NULL) {
        *pl = l;
    }
}

static float
sixel_builtin_psd_hue_to_rgb(float p, float q, float t)
{
    if (t < 0.0f) {
        t += 1.0f;
    }
    if (t > 1.0f) {
        t -= 1.0f;
    }
    if (t < 1.0f / 6.0f) {
        return p + (q - p) * 6.0f * t;
    }
    if (t < 1.0f / 2.0f) {
        return q;
    }
    if (t < 2.0f / 3.0f) {
        return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    }
    return p;
}

static void
sixel_builtin_psd_hsl_to_rgb(float h,
                             float s,
                             float l,
                             float *pr,
                             float *pg,
                             float *pb)
{
    float r;
    float g;
    float b;
    float q;
    float p;

    r = l;
    g = l;
    b = l;
    if (s > 1.0e-8f) {
        q = (l < 0.5f) ? (l * (1.0f + s)) : (l + s - l * s);
        p = 2.0f * l - q;
        r = sixel_builtin_psd_hue_to_rgb(p, q, h + 1.0f / 3.0f);
        g = sixel_builtin_psd_hue_to_rgb(p, q, h);
        b = sixel_builtin_psd_hue_to_rgb(p, q, h - 1.0f / 3.0f);
    }
    if (pr != NULL) {
        *pr = sixel_builtin_psd_clamp01(r);
    }
    if (pg != NULL) {
        *pg = sixel_builtin_psd_clamp01(g);
    }
    if (pb != NULL) {
        *pb = sixel_builtin_psd_clamp01(b);
    }
}

static float
sixel_builtin_psd_blend_channel(float cb,
                                float cs,
                                sixel_builtin_psd_layer_blend_mode_t mode)
{
    float result;
    float d;
    float t;
    float s2;

    cb = sixel_builtin_psd_clamp01(cb);
    cs = sixel_builtin_psd_clamp01(cs);
    result = cs;
    d = 0.0f;
    t = 0.0f;
    s2 = 0.0f;
    switch (mode) {
    case SIXEL_BUILTIN_PSD_BLEND_NORMAL:
    case SIXEL_BUILTIN_PSD_BLEND_DISSOLVE:
        result = cs;
        break;
    case SIXEL_BUILTIN_PSD_BLEND_DARKEN:
        result = fminf(cb, cs);
        break;
    case SIXEL_BUILTIN_PSD_BLEND_MULTIPLY:
        result = cb * cs;
        break;
    case SIXEL_BUILTIN_PSD_BLEND_COLOR_BURN:
        result = (cs <= 0.0f) ? 0.0f : 1.0f - fminf(1.0f, (1.0f - cb) / cs);
        break;
    case SIXEL_BUILTIN_PSD_BLEND_LINEAR_BURN:
        result = fmaxf(0.0f, cb + cs - 1.0f);
        break;
    case SIXEL_BUILTIN_PSD_BLEND_LIGHTEN:
        result = fmaxf(cb, cs);
        break;
    case SIXEL_BUILTIN_PSD_BLEND_SCREEN:
        result = cb + cs - cb * cs;
        break;
    case SIXEL_BUILTIN_PSD_BLEND_COLOR_DODGE:
        result = (cs >= 1.0f) ? 1.0f : fminf(1.0f, cb / (1.0f - cs));
        break;
    case SIXEL_BUILTIN_PSD_BLEND_LINEAR_DODGE:
        result = fminf(1.0f, cb + cs);
        break;
    case SIXEL_BUILTIN_PSD_BLEND_OVERLAY:
        result = (cb <= 0.5f)
            ? (2.0f * cb * cs)
            : (1.0f - 2.0f * (1.0f - cb) * (1.0f - cs));
        break;
    case SIXEL_BUILTIN_PSD_BLEND_SOFT_LIGHT:
        if (cs <= 0.5f) {
            result = cb - (1.0f - 2.0f * cs) * cb * (1.0f - cb);
        } else {
            if (cb <= 0.25f) {
                d = ((16.0f * cb - 12.0f) * cb + 4.0f) * cb;
            } else {
                d = sqrtf(cb);
            }
            result = cb + (2.0f * cs - 1.0f) * (d - cb);
        }
        break;
    case SIXEL_BUILTIN_PSD_BLEND_HARD_LIGHT:
        result = (cs <= 0.5f)
            ? (2.0f * cb * cs)
            : (1.0f - 2.0f * (1.0f - cb) * (1.0f - cs));
        break;
    case SIXEL_BUILTIN_PSD_BLEND_VIVID_LIGHT:
        if (cs < 0.5f) {
            s2 = 2.0f * cs;
            result = (s2 <= 0.0f)
                ? 0.0f
                : 1.0f - fminf(1.0f, (1.0f - cb) / s2);
        } else {
            s2 = 2.0f * (cs - 0.5f);
            result = (s2 >= 1.0f)
                ? 1.0f
                : fminf(1.0f, cb / (1.0f - s2));
        }
        break;
    case SIXEL_BUILTIN_PSD_BLEND_LINEAR_LIGHT:
        result = fmaxf(0.0f, fminf(1.0f, cb + 2.0f * cs - 1.0f));
        break;
    case SIXEL_BUILTIN_PSD_BLEND_PIN_LIGHT:
        if (cs < 0.5f) {
            result = fminf(cb, 2.0f * cs);
        } else {
            result = fmaxf(cb, 2.0f * cs - 1.0f);
        }
        break;
    case SIXEL_BUILTIN_PSD_BLEND_HARD_MIX:
        t = fmaxf(0.0f, fminf(1.0f, cb + 2.0f * cs - 1.0f));
        result = t < 0.5f ? 0.0f : 1.0f;
        break;
    case SIXEL_BUILTIN_PSD_BLEND_DIFFERENCE:
        result = fabsf(cb - cs);
        break;
    case SIXEL_BUILTIN_PSD_BLEND_EXCLUSION:
        result = cb + cs - 2.0f * cb * cs;
        break;
    case SIXEL_BUILTIN_PSD_BLEND_SUBTRACT:
        result = fmaxf(0.0f, cb - cs);
        break;
    case SIXEL_BUILTIN_PSD_BLEND_DIVIDE:
        result = (cs <= 0.0f) ? 1.0f : fminf(1.0f, cb / cs);
        break;
    case SIXEL_BUILTIN_PSD_BLEND_DARKER_COLOR:
    case SIXEL_BUILTIN_PSD_BLEND_LIGHTER_COLOR:
    case SIXEL_BUILTIN_PSD_BLEND_HUE:
    case SIXEL_BUILTIN_PSD_BLEND_SATURATION:
    case SIXEL_BUILTIN_PSD_BLEND_COLOR:
    case SIXEL_BUILTIN_PSD_BLEND_LUMINOSITY:
        result = cs;
        break;
    default:
        result = cs;
        break;
    }
    return sixel_builtin_psd_clamp01(result);
}

static void
sixel_builtin_psd_blend_rgb(float cb_r,
                            float cb_g,
                            float cb_b,
                            float cs_r,
                            float cs_g,
                            float cs_b,
                            sixel_builtin_psd_layer_blend_mode_t mode,
                            float *po_r,
                            float *po_g,
                            float *po_b)
{
    float hb;
    float sb;
    float lb;
    float hs;
    float ss;
    float ls;
    float lum_cb;
    float lum_cs;
    float use_source;
    float out_r;
    float out_g;
    float out_b;

    hb = 0.0f;
    sb = 0.0f;
    lb = 0.0f;
    hs = 0.0f;
    ss = 0.0f;
    ls = 0.0f;
    lum_cb = 0.0f;
    lum_cs = 0.0f;
    use_source = 0.0f;
    out_r = cs_r;
    out_g = cs_g;
    out_b = cs_b;

    if (mode == SIXEL_BUILTIN_PSD_BLEND_DARKER_COLOR ||
        mode == SIXEL_BUILTIN_PSD_BLEND_LIGHTER_COLOR) {
        lum_cb = sixel_builtin_psd_rgb_luma(cb_r, cb_g, cb_b);
        lum_cs = sixel_builtin_psd_rgb_luma(cs_r, cs_g, cs_b);
        use_source = (mode == SIXEL_BUILTIN_PSD_BLEND_DARKER_COLOR)
            ? (lum_cs <= lum_cb ? 1.0f : 0.0f)
            : (lum_cs >= lum_cb ? 1.0f : 0.0f);
        out_r = (use_source > 0.5f) ? cs_r : cb_r;
        out_g = (use_source > 0.5f) ? cs_g : cb_g;
        out_b = (use_source > 0.5f) ? cs_b : cb_b;
    } else if (mode == SIXEL_BUILTIN_PSD_BLEND_HUE ||
               mode == SIXEL_BUILTIN_PSD_BLEND_SATURATION ||
               mode == SIXEL_BUILTIN_PSD_BLEND_COLOR ||
               mode == SIXEL_BUILTIN_PSD_BLEND_LUMINOSITY) {
        sixel_builtin_psd_rgb_to_hsl(cb_r, cb_g, cb_b, &hb, &sb, &lb);
        sixel_builtin_psd_rgb_to_hsl(cs_r, cs_g, cs_b, &hs, &ss, &ls);
        if (mode == SIXEL_BUILTIN_PSD_BLEND_HUE) {
            sixel_builtin_psd_hsl_to_rgb(hs, sb, lb, &out_r, &out_g, &out_b);
        } else if (mode == SIXEL_BUILTIN_PSD_BLEND_SATURATION) {
            sixel_builtin_psd_hsl_to_rgb(hb, ss, lb, &out_r, &out_g, &out_b);
        } else if (mode == SIXEL_BUILTIN_PSD_BLEND_COLOR) {
            sixel_builtin_psd_hsl_to_rgb(hs, ss, lb, &out_r, &out_g, &out_b);
        } else {
            sixel_builtin_psd_hsl_to_rgb(hb, sb, ls, &out_r, &out_g, &out_b);
        }
    } else {
        out_r = sixel_builtin_psd_blend_channel(cb_r, cs_r, mode);
        out_g = sixel_builtin_psd_blend_channel(cb_g, cs_g, mode);
        out_b = sixel_builtin_psd_blend_channel(cb_b, cs_b, mode);
    }

    if (po_r != NULL) {
        *po_r = sixel_builtin_psd_clamp01(out_r);
    }
    if (po_g != NULL) {
        *po_g = sixel_builtin_psd_clamp01(out_g);
    }
    if (po_b != NULL) {
        *po_b = sixel_builtin_psd_clamp01(out_b);
    }
}

static void
sixel_builtin_psd_layer_record_init(sixel_builtin_psd_layer_record_t *layer)
{
    if (layer == NULL) {
        return;
    }
    memset(layer, 0, sizeof(*layer));
    layer->blend_key[0] = 'n';
    layer->blend_key[1] = 'o';
    layer->blend_key[2] = 'r';
    layer->blend_key[3] = 'm';
    layer->blend_key[4] = '\0';
    layer->opacity = 255u;
    layer->visible = 1;
    layer->red_channel_index = -1;
    layer->green_channel_index = -1;
    layer->blue_channel_index = -1;
    layer->gray_channel_index = -1;
    layer->c_channel_index = -1;
    layer->m_channel_index = -1;
    layer->y_channel_index = -1;
    layer->k_channel_index = -1;
    layer->alpha_channel_index = -1;
    layer->user_mask_channel_index = -1;
    layer->real_mask_channel_index = -1;
    layer->fill_kind = SIXEL_BUILTIN_PSD_FILL_NONE;
    layer->has_fill_payload = 0;
    layer->has_malformed_fill_payload = 0;
    layer->fill_solid_rgb[0] = 0.0f;
    layer->fill_solid_rgb[1] = 0.0f;
    layer->fill_solid_rgb[2] = 0.0f;
    layer->fill_gradient_type = 0u;
    layer->fill_gradient_angle_deg = 0.0f;
    layer->fill_gradient_scale = 100.0f;
    layer->fill_gradient_reverse = 0;
    layer->fill_gradient_stop_count = 0u;
    layer->fill_pattern_tile = 4u;
    layer->fill_pattern_fg_rgb[0] = 1.0f;
    layer->fill_pattern_fg_rgb[1] = 1.0f;
    layer->fill_pattern_fg_rgb[2] = 1.0f;
    layer->fill_pattern_bg_rgb[0] = 0.0f;
    layer->fill_pattern_bg_rgb[1] = 0.0f;
    layer->fill_pattern_bg_rgb[2] = 0.0f;
}

static int
sixel_builtin_psd_is_layer_hidden(unsigned char flags)
{
    /* Layer record flag bit1: 1 means hidden. */
    return (flags & 0x02u) != 0u;
}

static SIXELSTATUS
sixel_builtin_psd_parse_layer_extra_data(
    unsigned char const *buffer,
    size_t extra_begin,
    size_t extra_end,
    sixel_builtin_psd_layer_record_t *layer)
{
    size_t cursor;
    size_t block_length;
    size_t key_length;
    int parsed_fill;
    char key[5];

    cursor = 0u;
    block_length = 0u;
    key_length = 0u;
    parsed_fill = 0;
    key[0] = '\0';
    key[1] = '\0';
    key[2] = '\0';
    key[3] = '\0';
    key[4] = '\0';

    if (buffer == NULL || layer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (extra_begin > extra_end) {
        return SIXEL_STBI_ERROR;
    }
    cursor = extra_begin;
    if (cursor + 4u > extra_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }

    block_length = sixel_builtin_read_u32be_size(buffer + cursor);
    cursor += 4u;
    if (block_length > extra_end - cursor) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }
    cursor += block_length;

    if (cursor + 4u > extra_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }
    block_length = sixel_builtin_read_u32be_size(buffer + cursor);
    cursor += 4u;
    if (block_length > extra_end - cursor) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }
    cursor += block_length;

    if (!sixel_builtin_psd_skip_pascal_string_padded4(buffer, extra_end, &cursor)) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }

    while (cursor + 12u <= extra_end) {
        if (memcmp(buffer + cursor, "8BIM", 4u) != 0 &&
            memcmp(buffer + cursor, "8B64", 4u) != 0) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer extra data");
            return SIXEL_STBI_ERROR;
        }
        memcpy(key, buffer + cursor + 4u, 4u);
        key[4] = '\0';
        key_length = sixel_builtin_read_u32be_size(buffer + cursor + 8u);
        cursor += 12u;
        if (key_length > extra_end - cursor) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer extra data");
            return SIXEL_STBI_ERROR;
        }
        if (sixel_builtin_psd_layer_is_non_pixel_key(key)) {
            layer->has_non_pixel_payload = 1;
        }
        if (memcmp(key, "TySh", 4u) == 0) {
            parsed_fill = sixel_builtin_psd_parse_tysh_payload(
                buffer + cursor,
                key_length,
                layer);
            if (parsed_fill != 0) {
                layer->has_fill_payload = 1;
            } else if (sixel_builtin_psd_tysh_payload_looks_wrapped(
                           buffer + cursor,
                           key_length)) {
                layer->has_fill_payload = 1;
                layer->has_malformed_fill_payload = 1;
            }
        } else if (memcmp(key, "SoCo", 4u) == 0 ||
                   memcmp(key, "GdFl", 4u) == 0 ||
                   memcmp(key, "PtFl", 4u) == 0) {
            layer->has_fill_payload = 1;
            parsed_fill = sixel_builtin_psd_parse_fill_payload(key,
                                                               buffer + cursor,
                                                               key_length,
                                                               layer);
            if (parsed_fill == 0) {
                layer->has_malformed_fill_payload = 1;
            }
        }
        if (memcmp(key, "vmsk", 4u) == 0 ||
            memcmp(key, "vsms", 4u) == 0) {
            layer->has_vector_mask = 1;
        } else if (memcmp(key, "lrFX", 4u) == 0 ||
                   memcmp(key, "lfx2", 4u) == 0) {
            layer->has_layer_effects = 1;
        } else if (memcmp(key, "knko", 4u) == 0) {
            layer->has_knockout = 1;
        }
        cursor += key_length;
        if ((key_length & 1u) != 0u && cursor < extra_end) {
            ++cursor;
        }
    }
    if (cursor != extra_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }
    return SIXEL_OK;
}

static int
sixel_builtin_psd_should_try_multilayer_fallback(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info)
{
    size_t section_offset;
    size_t section_end;
    size_t layer_info_length_field_size;
    size_t layer_info_length;
    size_t layer_info_offset;
    size_t min_layer_count;
    int16_t layer_count_raw;
    size_t layer_count;

    section_offset = 0u;
    section_end = 0u;
    layer_info_length_field_size = 0u;
    layer_info_length = 0u;
    layer_info_offset = 0u;
    min_layer_count = 2u;
    layer_count_raw = 0;
    layer_count = 0u;

    if (chunk == NULL || info == NULL || chunk->buffer == NULL) {
        return 0;
    }
    if (info->version == 2u) {
        min_layer_count = 1u;
    }
    layer_info_length_field_size =
        sixel_builtin_psd_layer_info_length_field_size(info);
    if (layer_info_length_field_size == 0u ||
        info->layer_mask_length < layer_info_length_field_size + 2u ||
        info->layer_mask_offset > chunk->size ||
        info->layer_mask_length > chunk->size - info->layer_mask_offset) {
        return 0;
    }
    section_offset = info->layer_mask_offset;
    section_end = section_offset + info->layer_mask_length;
    if (!sixel_builtin_psd_read_length_field_checked(
            chunk->buffer + section_offset,
            layer_info_length_field_size,
            &layer_info_length)) {
        return 0;
    }
    layer_info_offset = section_offset + layer_info_length_field_size;
    if (layer_info_length < 2u ||
        layer_info_length > section_end - layer_info_offset ||
        layer_info_offset + layer_info_length > chunk->size) {
        return 0;
    }
    layer_count_raw = sixel_builtin_read_i16be(chunk->buffer + layer_info_offset);
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
        if (layer_info_length <= 2u + layer_count * 18u) {
            return 0;
        }
    }
    return 1;
}

static SIXELSTATUS
sixel_builtin_psd_parse_layer_model(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    sixel_builtin_psd_layer_model_t *model)
{
    unsigned char const *buffer;
    size_t section_offset;
    size_t section_end;
    size_t layer_info_length_field_size;
    size_t layer_channel_length_field_size;
    size_t channel_entry_bytes;
    size_t layer_info_length;
    size_t cursor;
    size_t layer_info_end;
    int16_t layer_count_raw;
    size_t layer_count;
    size_t i;
    size_t c;
    size_t extra_data_length;
    size_t extra_data_begin;
    size_t extra_data_end;

    buffer = NULL;
    section_offset = 0u;
    section_end = 0u;
    layer_info_length_field_size = 0u;
    layer_channel_length_field_size = 0u;
    channel_entry_bytes = 0u;
    layer_info_length = 0u;
    cursor = 0u;
    layer_info_end = 0u;
    layer_count_raw = 0;
    layer_count = 0u;
    i = 0u;
    c = 0u;
    extra_data_length = 0u;
    extra_data_begin = 0u;
    extra_data_end = 0u;

    if (chunk == NULL || info == NULL || model == NULL ||
        chunk->buffer == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    layer_info_length_field_size =
        sixel_builtin_psd_layer_info_length_field_size(info);
    layer_channel_length_field_size =
        sixel_builtin_psd_layer_channel_length_field_size(info);
    if (layer_info_length_field_size == 0u ||
        layer_channel_length_field_size == 0u ||
        layer_channel_length_field_size > SIZE_MAX - 2u) {
        sixel_helper_set_additional_message(
            sixel_builtin_psd_malformed_layer_mask_length_message(info));
        return SIXEL_STBI_ERROR;
    }
    channel_entry_bytes = 2u + layer_channel_length_field_size;
    sixel_builtin_psd_layer_model_init(model);
    if (info->layer_mask_length < layer_info_length_field_size + 2u ||
        info->layer_mask_offset > chunk->size ||
        info->layer_mask_length > chunk->size - info->layer_mask_offset) {
        sixel_helper_set_additional_message(
            sixel_builtin_psd_malformed_layer_mask_length_message(info));
        return SIXEL_STBI_ERROR;
    }
    buffer = chunk->buffer;
    section_offset = info->layer_mask_offset;
    section_end = section_offset + info->layer_mask_length;
    if (!sixel_builtin_psd_read_length_field_checked(buffer + section_offset,
                                                     layer_info_length_field_size,
                                                     &layer_info_length)) {
        sixel_helper_set_additional_message(
            sixel_builtin_psd_malformed_layer_info_length_message(info));
        return SIXEL_STBI_ERROR;
    }
    cursor = section_offset + layer_info_length_field_size;
    if (layer_info_length < 2u ||
        layer_info_length > section_end - cursor ||
        cursor + layer_info_length > chunk->size) {
        sixel_helper_set_additional_message(
            sixel_builtin_psd_malformed_layer_info_length_message(info));
        return SIXEL_STBI_ERROR;
    }
    layer_info_end = cursor + layer_info_length;

    if (cursor + 2u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record header");
        return SIXEL_STBI_ERROR;
    }
    layer_count_raw = sixel_builtin_read_i16be(buffer + cursor);
    cursor += 2u;
    if (layer_count_raw < 0) {
        layer_count = (size_t)(-(int)layer_count_raw);
    } else {
        layer_count = (size_t)layer_count_raw;
    }
    if (layer_count == 0u) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }
    if (layer_count > SIZE_MAX / sizeof(sixel_builtin_psd_layer_record_t)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    model->layers = (sixel_builtin_psd_layer_record_t *)sixel_allocator_malloc(
        chunk->allocator,
        layer_count * sizeof(sixel_builtin_psd_layer_record_t));
    if (model->layers == NULL) {
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    model->layer_count = layer_count;
    for (i = 0u; i < layer_count; ++i) {
        sixel_builtin_psd_layer_record_init(&model->layers[i]);
    }

    for (i = 0u; i < layer_count; ++i) {
        sixel_builtin_psd_layer_record_t *layer;

        layer = &model->layers[i];
        if (cursor + 18u > layer_info_end) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer record geometry");
            return SIXEL_STBI_ERROR;
        }
        layer->top = sixel_builtin_read_i32be(buffer + cursor + 0u);
        layer->left = sixel_builtin_read_i32be(buffer + cursor + 4u);
        layer->bottom = sixel_builtin_read_i32be(buffer + cursor + 8u);
        layer->right = sixel_builtin_read_i32be(buffer + cursor + 12u);
        layer->channel_count = sixel_builtin_read_u16be(buffer + cursor + 16u);
        cursor += 18u;

        if (layer->bottom < layer->top || layer->right < layer->left) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer record geometry");
            return SIXEL_STBI_ERROR;
        }
        layer->width = (unsigned int)(layer->right - layer->left);
        layer->height = (unsigned int)(layer->bottom - layer->top);
        if (layer->channel_count > SIXEL_FROMPSD_MAX_CHANNELS) {
            sixel_helper_set_additional_message(
                "builtin PSD: unsupported layer fallback channels");
            return SIXEL_STBI_ERROR;
        }
        if (cursor > layer_info_end ||
            (size_t)layer->channel_count > (layer_info_end - cursor) / channel_entry_bytes) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel table");
            return SIXEL_STBI_ERROR;
        }
        for (c = 0u; c < (size_t)layer->channel_count; ++c) {
            int16_t channel_id;
            size_t channel_length;

            channel_id = sixel_builtin_read_i16be(buffer + cursor);
            if (!sixel_builtin_psd_read_length_field_checked(
                    buffer + cursor + 2u,
                    layer_channel_length_field_size,
                    &channel_length)) {
                sixel_helper_set_additional_message(
                    sixel_builtin_psd_malformed_layer_channel_length_message(info));
                return SIXEL_STBI_ERROR;
            }
            layer->channels[c].channel_id = channel_id;
            layer->channels[c].length = channel_length;
            layer->channels[c].data_offset = 0u;
            if (channel_length < 2u) {
                sixel_helper_set_additional_message(
                    sixel_builtin_psd_malformed_layer_channel_length_message(info));
                return SIXEL_STBI_ERROR;
            }
            if (channel_id == 0 && layer->red_channel_index < 0) {
                layer->red_channel_index = (int)c;
            } else if (channel_id == 1 && layer->green_channel_index < 0) {
                layer->green_channel_index = (int)c;
            } else if (channel_id == 2 && layer->blue_channel_index < 0) {
                layer->blue_channel_index = (int)c;
            } else if (channel_id == 3 && layer->k_channel_index < 0) {
                layer->k_channel_index = (int)c;
            } else if (channel_id == -1 && layer->alpha_channel_index < 0) {
                layer->alpha_channel_index = (int)c;
            } else if (channel_id == -2 && layer->user_mask_channel_index < 0) {
                layer->user_mask_channel_index = (int)c;
            } else if (channel_id == -3 && layer->real_mask_channel_index < 0) {
                layer->real_mask_channel_index = (int)c;
            }
            cursor += channel_entry_bytes;
        }
        layer->gray_channel_index = layer->red_channel_index;
        layer->c_channel_index = layer->red_channel_index;
        layer->m_channel_index = layer->green_channel_index;
        layer->y_channel_index = layer->blue_channel_index;

        if (cursor + 16u > layer_info_end) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer blend block");
            return SIXEL_STBI_ERROR;
        }
        if (memcmp(buffer + cursor, "8BIM", 4u) != 0 &&
            memcmp(buffer + cursor, "8B64", 4u) != 0) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer blend block");
            return SIXEL_STBI_ERROR;
        }
        memcpy(layer->blend_key, buffer + cursor + 4u, 4u);
        layer->blend_key[4] = '\0';
        layer->opacity = buffer[cursor + 8u];
        layer->clipping = buffer[cursor + 9u];
        layer->flags = buffer[cursor + 10u];
        layer->visible = sixel_builtin_psd_is_layer_hidden(layer->flags) ? 0 : 1;
        extra_data_length = sixel_builtin_read_u32be_size(buffer + cursor + 12u);
        cursor += 16u;

        if (extra_data_length > layer_info_end - cursor) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer extra data");
            return SIXEL_STBI_ERROR;
        }
        extra_data_begin = cursor;
        extra_data_end = cursor + extra_data_length;
        if (SIXEL_FAILED(sixel_builtin_psd_parse_layer_extra_data(
                buffer,
                extra_data_begin,
                extra_data_end,
                layer))) {
            return SIXEL_STBI_ERROR;
        }
        cursor = extra_data_end;
    }

    for (i = 0u; i < layer_count; ++i) {
        sixel_builtin_psd_layer_record_t *layer;

        layer = &model->layers[i];
        for (c = 0u; c < (size_t)layer->channel_count; ++c) {
            if (layer->channels[c].length > layer_info_end - cursor) {
                sixel_helper_set_additional_message(
                    sixel_builtin_psd_malformed_layer_channel_length_message(info));
                return SIXEL_STBI_ERROR;
            }
            layer->channels[c].data_offset = cursor;
            cursor += layer->channels[c].length;
        }
    }
    if (cursor > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel stream");
        return SIXEL_STBI_ERROR;
    }

    return SIXEL_OK;
}

static void
sixel_builtin_psd_layer_buffers_destroy(sixel_allocator_t *allocator,
                                        sixel_builtin_psd_layer_buffers_t *buffers)
{
    if (allocator == NULL || buffers == NULL) {
        return;
    }
    if (buffers->rgb_linear != NULL) {
        sixel_allocator_free(allocator, buffers->rgb_linear);
        buffers->rgb_linear = NULL;
    }
    if (buffers->alpha != NULL) {
        sixel_allocator_free(allocator, buffers->alpha);
        buffers->alpha = NULL;
    }
    buffers->pixel_count = 0u;
}

static SIXELSTATUS
sixel_builtin_psd_decode_layer_channel_float(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_layer_record_t const *layer,
    int channel_index,
    unsigned int depth,
    size_t row_length_field_size,
    float *out_values)
{
    size_t pixel_count;
    size_t i;
    SIXELSTATUS status;
    unsigned char *plane8;
    uint16_t *plane16;

    pixel_count = 0u;
    i = 0u;
    status = SIXEL_FALSE;
    plane8 = NULL;
    plane16 = NULL;

    if (chunk == NULL || layer == NULL || out_values == NULL ||
        channel_index < 0 || chunk->allocator == NULL || chunk->buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)layer->width > SIZE_MAX / (size_t)layer->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)layer->width * (size_t)layer->height;
    if (depth == 8u) {
        plane8 = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                         pixel_count);
        if (plane8 == NULL) {
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        status = sixel_builtin_psd_decode_layer_plane_rgb8_with_row_length(
            chunk->buffer,
            layer->channels,
            channel_index,
            layer->width,
            layer->height,
            row_length_field_size,
            plane8);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(chunk->allocator, plane8);
            return status;
        }
        for (i = 0u; i < pixel_count; ++i) {
            out_values[i] = (float)plane8[i] / 255.0f;
        }
        sixel_allocator_free(chunk->allocator, plane8);
        return SIXEL_OK;
    }
    if (depth == 16u) {
        if (pixel_count > SIZE_MAX / sizeof(uint16_t)) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        plane16 = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                     pixel_count * sizeof(uint16_t));
        if (plane16 == NULL) {
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        status = sixel_builtin_psd_decode_layer_plane_rgb16_with_row_length(
            chunk->buffer,
            layer->channels,
            channel_index,
            layer->width,
            layer->height,
            row_length_field_size,
            plane16);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(chunk->allocator, plane16);
            return status;
        }
        for (i = 0u; i < pixel_count; ++i) {
            out_values[i] = (float)plane16[i] / 65535.0f;
        }
        sixel_allocator_free(chunk->allocator, plane16);
        return SIXEL_OK;
    }
    if (depth == 32u) {
        return sixel_builtin_psd_decode_layer_plane_rgb32_with_row_length(
            chunk->buffer,
            layer->channels,
            channel_index,
            layer->width,
            layer->height,
            row_length_field_size,
            out_values);
    }
    return SIXEL_BAD_INPUT;
}

static SIXELSTATUS
sixel_builtin_psd_decode_layer_to_linear(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    sixel_builtin_psd_layer_record_t const *layer,
    unsigned char const *icc_profile,
    size_t icc_profile_length,
    int *pcms_applied,
    sixel_builtin_psd_layer_buffers_t *out_layer)
{
    size_t pixel_count;
    size_t i;
    float *c0;
    float *c1;
    float *c2;
    float *c3;
    float *rgb;
    float *alpha;
    float opacity_scale;
    size_t row_length_field_size;
    SIXELSTATUS status;
    int cms_applied;
    sixel_cms_profile_t *src_profile;
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;

    pixel_count = 0u;
    i = 0u;
    c0 = NULL;
    c1 = NULL;
    c2 = NULL;
    c3 = NULL;
    rgb = NULL;
    alpha = NULL;
    opacity_scale = 1.0f;
    row_length_field_size = 2u;
    status = SIXEL_FALSE;
    cms_applied = 0;
    src_profile = NULL;
    dst_profile = NULL;
    transform = NULL;

    if (chunk == NULL || info == NULL || layer == NULL || out_layer == NULL ||
        chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    row_length_field_size = sixel_builtin_psd_rle_row_length_field_size(info);
    if (row_length_field_size == 0u) {
        return SIXEL_BAD_INPUT;
    }
    out_layer->rgb_linear = NULL;
    out_layer->alpha = NULL;
    out_layer->pixel_count = 0u;
    if ((size_t)layer->width > SIZE_MAX / (size_t)layer->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)layer->width * (size_t)layer->height;
    if (pixel_count == 0u) {
        return SIXEL_BAD_INPUT;
    }
    if (pixel_count > SIZE_MAX / sizeof(float) ||
        pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    alpha = (float *)sixel_allocator_malloc(chunk->allocator,
                                            pixel_count * sizeof(float));
    c0 = (float *)sixel_allocator_malloc(chunk->allocator,
                                         pixel_count * sizeof(float));
    if (alpha == NULL || c0 == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup;
    }
    for (i = 0u; i < pixel_count; ++i) {
        alpha[i] = 1.0f;
    }
    opacity_scale = (float)layer->opacity / 255.0f;
    for (i = 0u; i < pixel_count; ++i) {
        alpha[i] = opacity_scale;
    }

    if (layer->alpha_channel_index >= 0) {
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk,
            layer,
            layer->alpha_channel_index,
            info->depth,
            row_length_field_size,
            c0);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        for (i = 0u; i < pixel_count; ++i) {
            alpha[i] *= sixel_builtin_psd_clamp_alpha_float32(c0[i]);
        }
    }
    if (layer->user_mask_channel_index >= 0) {
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk,
            layer,
            layer->user_mask_channel_index,
            info->depth,
            row_length_field_size,
            c0);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        for (i = 0u; i < pixel_count; ++i) {
            alpha[i] *= sixel_builtin_psd_clamp_alpha_float32(c0[i]);
        }
    }
    if (layer->real_mask_channel_index >= 0) {
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk,
            layer,
            layer->real_mask_channel_index,
            info->depth,
            row_length_field_size,
            c0);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        for (i = 0u; i < pixel_count; ++i) {
            alpha[i] *= sixel_builtin_psd_clamp_alpha_float32(c0[i]);
        }
    }

    rgb = (float *)sixel_allocator_malloc(chunk->allocator,
                                          pixel_count * 3u * sizeof(float));
    if (rgb == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup;
    }

    if (info->color_mode == 3u ||
        (info->color_mode == 7u && info->channels == 3u)) {
        if (layer->red_channel_index < 0 ||
            layer->green_channel_index < 0 ||
            layer->blue_channel_index < 0) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "builtin PSD: unsupported layer fallback layout");
            goto cleanup;
        }
        c1 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * sizeof(float));
        c2 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * sizeof(float));
        if (c1 == NULL || c2 == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup;
        }
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk, layer, layer->red_channel_index, info->depth,
            row_length_field_size, c0);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk, layer, layer->green_channel_index, info->depth,
            row_length_field_size, c1);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk, layer, layer->blue_channel_index, info->depth,
            row_length_field_size, c2);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        for (i = 0u; i < pixel_count; ++i) {
            rgb[i * 3u + 0u] = sixel_builtin_psd_gamma_to_linear(c0[i]);
            rgb[i * 3u + 1u] = sixel_builtin_psd_gamma_to_linear(c1[i]);
            rgb[i * 3u + 2u] = sixel_builtin_psd_gamma_to_linear(c2[i]);
        }
    } else if (info->color_mode == 1u || info->color_mode == 8u) {
        if (layer->gray_channel_index < 0) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "builtin PSD: unsupported layer fallback layout");
            goto cleanup;
        }
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk, layer, layer->gray_channel_index, info->depth,
            row_length_field_size, c0);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        for (i = 0u; i < pixel_count; ++i) {
            float v;
            v = sixel_builtin_psd_gamma_to_linear(c0[i]);
            rgb[i * 3u + 0u] = v;
            rgb[i * 3u + 1u] = v;
            rgb[i * 3u + 2u] = v;
        }
    } else if (info->color_mode == 4u ||
               (info->color_mode == 7u && info->channels == 4u)) {
        if (layer->c_channel_index < 0 || layer->m_channel_index < 0 ||
            layer->y_channel_index < 0 || layer->k_channel_index < 0) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "builtin PSD: unsupported layer fallback layout");
            goto cleanup;
        }
        c1 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * sizeof(float));
        c2 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * sizeof(float));
        c3 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * sizeof(float));
        if (c1 == NULL || c2 == NULL || c3 == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup;
        }
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk, layer, layer->c_channel_index, info->depth,
            row_length_field_size, c0);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk, layer, layer->m_channel_index, info->depth,
            row_length_field_size, c1);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk, layer, layer->y_channel_index, info->depth,
            row_length_field_size, c2);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk, layer, layer->k_channel_index, info->depth,
            row_length_field_size, c3);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        if (icc_profile != NULL && icc_profile_length > 0u) {
            float *cmyk;

            cmyk = (float *)sixel_allocator_malloc(
                chunk->allocator,
                pixel_count * 4u * sizeof(float));
            if (cmyk == NULL) {
                status = SIXEL_BAD_ALLOCATION;
                sixel_helper_set_additional_message(
                    "builtin PSD: sixel_allocator_malloc() failed.");
                goto cleanup;
            }
            for (i = 0u; i < pixel_count; ++i) {
                cmyk[i * 4u + 0u] = sixel_builtin_psd_decode_cmyk_f32(c0[i]);
                cmyk[i * 4u + 1u] = sixel_builtin_psd_decode_cmyk_f32(c1[i]);
                cmyk[i * 4u + 2u] = sixel_builtin_psd_decode_cmyk_f32(c2[i]);
                cmyk[i * 4u + 3u] = sixel_builtin_psd_decode_cmyk_f32(c3[i]);
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
                sixel_cms_do_transform(transform, cmyk, rgb, pixel_count)) {
                cms_applied = 1;
            }
            sixel_allocator_free(chunk->allocator, cmyk);
            cmyk = NULL;
        }
        if (!cms_applied) {
            for (i = 0u; i < pixel_count; ++i) {
                float c;
                float m;
                float y;
                float k;

                c = sixel_builtin_psd_decode_cmyk_f32(c0[i]);
                m = sixel_builtin_psd_decode_cmyk_f32(c1[i]);
                y = sixel_builtin_psd_decode_cmyk_f32(c2[i]);
                k = sixel_builtin_psd_decode_cmyk_f32(c3[i]);
                rgb[i * 3u + 0u] = sixel_builtin_psd_gamma_to_linear((1.0f - c) *
                                                                     (1.0f - k));
                rgb[i * 3u + 1u] = sixel_builtin_psd_gamma_to_linear((1.0f - m) *
                                                                     (1.0f - k));
                rgb[i * 3u + 2u] = sixel_builtin_psd_gamma_to_linear((1.0f - y) *
                                                                     (1.0f - k));
            }
        }
    } else if (info->color_mode == 9u) {
        float *lab;

        lab = NULL;
        if (layer->red_channel_index < 0 || layer->green_channel_index < 0 ||
            layer->blue_channel_index < 0) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "builtin PSD: unsupported layer fallback layout");
            goto cleanup;
        }
        c1 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * sizeof(float));
        c2 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * sizeof(float));
        lab = (float *)sixel_allocator_malloc(chunk->allocator,
                                              pixel_count * 3u * sizeof(float));
        if (c1 == NULL || c2 == NULL || lab == NULL) {
            sixel_allocator_free(chunk->allocator, lab);
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup;
        }
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk, layer, layer->red_channel_index, info->depth,
            row_length_field_size, c0);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(chunk->allocator, lab);
            goto cleanup;
        }
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk, layer, layer->green_channel_index, info->depth,
            row_length_field_size, c1);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(chunk->allocator, lab);
            goto cleanup;
        }
        status = sixel_builtin_psd_decode_layer_channel_float(
            chunk, layer, layer->blue_channel_index, info->depth,
            row_length_field_size, c2);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(chunk->allocator, lab);
            goto cleanup;
        }
        for (i = 0u; i < pixel_count; ++i) {
            if (info->depth == 8u) {
                lab[i * 3u + 0u] = sixel_builtin_psd_lab_decode_l(
                    (unsigned char)(sixel_builtin_psd_clamp01(c0[i]) * 255.0f + 0.5f));
                lab[i * 3u + 1u] = sixel_builtin_psd_lab_decode_ab(
                    (unsigned char)(sixel_builtin_psd_clamp01(c1[i]) * 255.0f + 0.5f));
                lab[i * 3u + 2u] = sixel_builtin_psd_lab_decode_ab(
                    (unsigned char)(sixel_builtin_psd_clamp01(c2[i]) * 255.0f + 0.5f));
            } else if (info->depth == 16u) {
                lab[i * 3u + 0u] = sixel_builtin_psd_lab_decode_l16(
                    (uint16_t)(sixel_builtin_psd_clamp01(c0[i]) * 65535.0f + 0.5f));
                lab[i * 3u + 1u] = sixel_builtin_psd_lab_decode_ab16(
                    (uint16_t)(sixel_builtin_psd_clamp01(c1[i]) * 65535.0f + 0.5f));
                lab[i * 3u + 2u] = sixel_builtin_psd_lab_decode_ab16(
                    (uint16_t)(sixel_builtin_psd_clamp01(c2[i]) * 65535.0f + 0.5f));
            } else {
                lab[i * 3u + 0u] = sixel_builtin_psd_lab_decode_l32(c0[i]);
                lab[i * 3u + 1u] = sixel_builtin_psd_lab_decode_ab32(c1[i]);
                lab[i * 3u + 2u] = sixel_builtin_psd_lab_decode_ab32(c2[i]);
            }
        }
        status = sixel_helper_convert_colorspace(
            (unsigned char *)lab,
            pixel_count * 3u * sizeof(float),
            SIXEL_PIXELFORMAT_CIELABFLOAT32,
            SIXEL_COLORSPACE_CIELAB,
            SIXEL_COLORSPACE_LINEAR);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(chunk->allocator, lab);
            goto cleanup;
        }
        memcpy(rgb, lab, pixel_count * 3u * sizeof(float));
        sixel_allocator_free(chunk->allocator, lab);
    } else {
        status = SIXEL_BAD_INPUT;
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        goto cleanup;
    }

    for (i = 0u; i < pixel_count; ++i) {
        alpha[i] = sixel_builtin_psd_clamp_alpha_float32(alpha[i]);
    }
    out_layer->rgb_linear = rgb;
    out_layer->alpha = alpha;
    out_layer->pixel_count = pixel_count;
    rgb = NULL;
    alpha = NULL;
    if (pcms_applied != NULL && cms_applied != 0) {
        *pcms_applied = 1;
    }
    status = SIXEL_OK;

cleanup:
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(dst_profile);
    sixel_cms_close_profile(src_profile);
    if (rgb != NULL) {
        sixel_allocator_free(chunk->allocator, rgb);
    }
    if (alpha != NULL) {
        sixel_allocator_free(chunk->allocator, alpha);
    }
    if (c3 != NULL) {
        sixel_allocator_free(chunk->allocator, c3);
    }
    if (c2 != NULL) {
        sixel_allocator_free(chunk->allocator, c2);
    }
    if (c1 != NULL) {
        sixel_allocator_free(chunk->allocator, c1);
    }
    if (c0 != NULL) {
        sixel_allocator_free(chunk->allocator, c0);
    }
    return status;
}

static void
sixel_builtin_psd_fill_sample_gradient(
    sixel_builtin_psd_layer_record_t const *layer,
    float t,
    float out_rgb[3],
    float *out_alpha)
{
    size_t i;
    size_t idx;
    float t0;
    float t1;
    float w;

    i = 0u;
    idx = 0u;
    t0 = 0.0f;
    t1 = 1.0f;
    w = 0.0f;
    if (out_rgb == NULL || out_alpha == NULL) {
        return;
    }
    if (layer == NULL || layer->fill_gradient_stop_count == 0u) {
        out_rgb[0] = 0.0f;
        out_rgb[1] = 0.0f;
        out_rgb[2] = 0.0f;
        *out_alpha = 1.0f;
        return;
    }
    t = sixel_builtin_psd_clamp01(t);
    if (layer->fill_gradient_reverse != 0) {
        t = 1.0f - t;
    }
    if (layer->fill_gradient_stop_count == 1u ||
        t <= layer->fill_gradient_stop_pos[0]) {
        out_rgb[0] = layer->fill_gradient_stop_rgb[0][0];
        out_rgb[1] = layer->fill_gradient_stop_rgb[0][1];
        out_rgb[2] = layer->fill_gradient_stop_rgb[0][2];
        *out_alpha = layer->fill_gradient_stop_alpha[0];
        return;
    }
    idx = layer->fill_gradient_stop_count - 1u;
    if (t >= layer->fill_gradient_stop_pos[idx]) {
        out_rgb[0] = layer->fill_gradient_stop_rgb[idx][0];
        out_rgb[1] = layer->fill_gradient_stop_rgb[idx][1];
        out_rgb[2] = layer->fill_gradient_stop_rgb[idx][2];
        *out_alpha = layer->fill_gradient_stop_alpha[idx];
        return;
    }
    for (i = 0u; i + 1u < layer->fill_gradient_stop_count; ++i) {
        if (t >= layer->fill_gradient_stop_pos[i] &&
            t <= layer->fill_gradient_stop_pos[i + 1u]) {
            idx = i;
            break;
        }
    }
    t0 = layer->fill_gradient_stop_pos[idx];
    t1 = layer->fill_gradient_stop_pos[idx + 1u];
    if (t1 <= t0) {
        w = 0.0f;
    } else {
        w = (t - t0) / (t1 - t0);
    }
    w = sixel_builtin_psd_clamp01(w);
    out_rgb[0] = sixel_builtin_psd_clamp01(
        layer->fill_gradient_stop_rgb[idx][0] * (1.0f - w) +
        layer->fill_gradient_stop_rgb[idx + 1u][0] * w);
    out_rgb[1] = sixel_builtin_psd_clamp01(
        layer->fill_gradient_stop_rgb[idx][1] * (1.0f - w) +
        layer->fill_gradient_stop_rgb[idx + 1u][1] * w);
    out_rgb[2] = sixel_builtin_psd_clamp01(
        layer->fill_gradient_stop_rgb[idx][2] * (1.0f - w) +
        layer->fill_gradient_stop_rgb[idx + 1u][2] * w);
    *out_alpha = sixel_builtin_psd_clamp_alpha_float32(
        layer->fill_gradient_stop_alpha[idx] * (1.0f - w) +
        layer->fill_gradient_stop_alpha[idx + 1u] * w);
}

static float
sixel_builtin_psd_fill_gradient_t(
    sixel_builtin_psd_layer_record_t const *layer,
    unsigned int x,
    unsigned int y)
{
    static float const pi = 3.14159265358979323846f;
    float nx;
    float ny;
    float scale;
    float angle_rad;
    float dir_x;
    float dir_y;
    float linear_proj;
    float t;

    nx = 0.0f;
    ny = 0.0f;
    scale = 1.0f;
    angle_rad = 0.0f;
    dir_x = 1.0f;
    dir_y = 0.0f;
    linear_proj = 0.0f;
    t = 0.0f;
    if (layer == NULL || layer->width == 0u || layer->height == 0u) {
        return 0.0f;
    }
    nx = ((float)x + 0.5f) / (float)layer->width - 0.5f;
    ny = ((float)y + 0.5f) / (float)layer->height - 0.5f;
    scale = layer->fill_gradient_scale;
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    angle_rad = layer->fill_gradient_angle_deg * pi / 180.0f;
    dir_x = cosf(angle_rad);
    dir_y = -sinf(angle_rad);
    linear_proj = (nx * dir_x + ny * dir_y) / scale;

    switch (layer->fill_gradient_type) {
    case 1u: /* radial */
        t = sqrtf(nx * nx + ny * ny) / (0.5f * scale);
        break;
    case 2u: /* angle */
        t = atan2f(-ny, nx) / (2.0f * pi) + 0.5f;
        break;
    case 3u: /* reflected */
        t = fabsf(linear_proj) * 2.0f;
        break;
    case 4u: /* diamond */
        t = (fabsf(nx) + fabsf(ny)) / (0.5f * scale);
        break;
    case 0u: /* linear */
    default:
        t = linear_proj + 0.5f;
        break;
    }
    return sixel_builtin_psd_clamp01(t);
}

static SIXELSTATUS
sixel_builtin_psd_generate_non_pixel_fill_layer(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    sixel_builtin_psd_layer_record_t const *layer,
    sixel_builtin_psd_layer_buffers_t *out_layer)
{
    size_t pixel_count;
    size_t i;
    unsigned int x;
    unsigned int y;
    float *rgb;
    float *alpha;
    float *tmp;
    float opacity_scale;
    size_t row_length_field_size;
    unsigned int tile;

    pixel_count = 0u;
    i = 0u;
    x = 0u;
    y = 0u;
    rgb = NULL;
    alpha = NULL;
    tmp = NULL;
    opacity_scale = 1.0f;
    row_length_field_size = 2u;
    tile = 1u;
    if (chunk == NULL || info == NULL || layer == NULL || out_layer == NULL ||
        chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    row_length_field_size = sixel_builtin_psd_rle_row_length_field_size(info);
    if (row_length_field_size == 0u) {
        return SIXEL_BAD_INPUT;
    }
    out_layer->rgb_linear = NULL;
    out_layer->alpha = NULL;
    out_layer->pixel_count = 0u;
    if (layer->fill_kind == SIXEL_BUILTIN_PSD_FILL_NONE) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)layer->width > SIZE_MAX / (size_t)layer->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)layer->width * (size_t)layer->height;
    if (pixel_count == 0u) {
        return SIXEL_BAD_INPUT;
    }
    if (pixel_count > SIZE_MAX / sizeof(float) ||
        pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    rgb = (float *)sixel_allocator_malloc(
        chunk->allocator,
        pixel_count * 3u * sizeof(float));
    alpha = (float *)sixel_allocator_malloc(
        chunk->allocator,
        pixel_count * sizeof(float));
    if (rgb == NULL || alpha == NULL) {
        sixel_allocator_free(chunk->allocator, rgb);
        sixel_allocator_free(chunk->allocator, alpha);
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    opacity_scale = (float)layer->opacity / 255.0f;
    for (i = 0u; i < pixel_count; ++i) {
        alpha[i] = opacity_scale;
    }

    if (layer->alpha_channel_index >= 0 ||
        layer->user_mask_channel_index >= 0 ||
        layer->real_mask_channel_index >= 0) {
        tmp = (float *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count * sizeof(float));
        if (tmp == NULL) {
            sixel_allocator_free(chunk->allocator, rgb);
            sixel_allocator_free(chunk->allocator, alpha);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        if (layer->alpha_channel_index >= 0) {
            if (SIXEL_FAILED(sixel_builtin_psd_decode_layer_channel_float(
                    chunk,
                    layer,
                    layer->alpha_channel_index,
                    info->depth,
                    row_length_field_size,
                    tmp))) {
                sixel_allocator_free(chunk->allocator, tmp);
                sixel_allocator_free(chunk->allocator, rgb);
                sixel_allocator_free(chunk->allocator, alpha);
                return SIXEL_STBI_ERROR;
            }
            for (i = 0u; i < pixel_count; ++i) {
                alpha[i] *= sixel_builtin_psd_clamp_alpha_float32(tmp[i]);
            }
        }
        if (layer->user_mask_channel_index >= 0) {
            if (SIXEL_FAILED(sixel_builtin_psd_decode_layer_channel_float(
                    chunk,
                    layer,
                    layer->user_mask_channel_index,
                    info->depth,
                    row_length_field_size,
                    tmp))) {
                sixel_allocator_free(chunk->allocator, tmp);
                sixel_allocator_free(chunk->allocator, rgb);
                sixel_allocator_free(chunk->allocator, alpha);
                return SIXEL_STBI_ERROR;
            }
            for (i = 0u; i < pixel_count; ++i) {
                alpha[i] *= sixel_builtin_psd_clamp_alpha_float32(tmp[i]);
            }
        }
        if (layer->real_mask_channel_index >= 0) {
            if (SIXEL_FAILED(sixel_builtin_psd_decode_layer_channel_float(
                    chunk,
                    layer,
                    layer->real_mask_channel_index,
                    info->depth,
                    row_length_field_size,
                    tmp))) {
                sixel_allocator_free(chunk->allocator, tmp);
                sixel_allocator_free(chunk->allocator, rgb);
                sixel_allocator_free(chunk->allocator, alpha);
                return SIXEL_STBI_ERROR;
            }
            for (i = 0u; i < pixel_count; ++i) {
                alpha[i] *= sixel_builtin_psd_clamp_alpha_float32(tmp[i]);
            }
        }
        sixel_allocator_free(chunk->allocator, tmp);
        tmp = NULL;
    }

    switch (layer->fill_kind) {
    case SIXEL_BUILTIN_PSD_FILL_SOCO:
        for (i = 0u; i < pixel_count; ++i) {
            rgb[i * 3u + 0u] = layer->fill_solid_rgb[0];
            rgb[i * 3u + 1u] = layer->fill_solid_rgb[1];
            rgb[i * 3u + 2u] = layer->fill_solid_rgb[2];
        }
        break;
    case SIXEL_BUILTIN_PSD_FILL_GDFL:
        for (y = 0u; y < layer->height; ++y) {
            for (x = 0u; x < layer->width; ++x) {
                size_t idx;
                float t;
                float sample_rgb[3];
                float sample_alpha;
                idx = (size_t)y * (size_t)layer->width + (size_t)x;
                t = sixel_builtin_psd_fill_gradient_t(layer, x, y);
                sixel_builtin_psd_fill_sample_gradient(layer,
                                                       t,
                                                       sample_rgb,
                                                       &sample_alpha);
                rgb[idx * 3u + 0u] = sample_rgb[0];
                rgb[idx * 3u + 1u] = sample_rgb[1];
                rgb[idx * 3u + 2u] = sample_rgb[2];
                alpha[idx] *= sample_alpha;
            }
        }
        break;
    case SIXEL_BUILTIN_PSD_FILL_PTFL:
        tile = layer->fill_pattern_tile;
        if (tile < 1u) {
            tile = 1u;
        }
        for (y = 0u; y < layer->height; ++y) {
            for (x = 0u; x < layer->width; ++x) {
                size_t idx;
                int use_fg;
                float const *src_rgb;
                idx = (size_t)y * (size_t)layer->width + (size_t)x;
                use_fg = (((x / tile) + (y / tile)) & 1u) == 0u ? 1 : 0;
                src_rgb = use_fg ? layer->fill_pattern_fg_rgb
                                 : layer->fill_pattern_bg_rgb;
                rgb[idx * 3u + 0u] = src_rgb[0];
                rgb[idx * 3u + 1u] = src_rgb[1];
                rgb[idx * 3u + 2u] = src_rgb[2];
            }
        }
        break;
    default:
        sixel_allocator_free(chunk->allocator, rgb);
        sixel_allocator_free(chunk->allocator, alpha);
        return SIXEL_BAD_INPUT;
    }

    for (i = 0u; i < pixel_count; ++i) {
        alpha[i] = sixel_builtin_psd_clamp_alpha_float32(alpha[i]);
        rgb[i * 3u + 0u] = sixel_builtin_psd_clamp01(rgb[i * 3u + 0u]);
        rgb[i * 3u + 1u] = sixel_builtin_psd_clamp01(rgb[i * 3u + 1u]);
        rgb[i * 3u + 2u] = sixel_builtin_psd_clamp01(rgb[i * 3u + 2u]);
    }

    out_layer->rgb_linear = rgb;
    out_layer->alpha = alpha;
    out_layer->pixel_count = pixel_count;
    return SIXEL_OK;
}

static void
sixel_builtin_psd_composite_layer_over(
    float *canvas_rgb_premul,
    float *canvas_alpha,
    unsigned int canvas_width,
    unsigned int canvas_height,
    sixel_builtin_psd_layer_record_t const *layer,
    sixel_builtin_psd_layer_buffers_t const *src,
    float const *clip_alpha_map,
    sixel_builtin_psd_layer_blend_mode_t mode)
{
    unsigned int x;
    unsigned int y;
    size_t src_index;
    size_t dst_index;
    int dst_x;
    int dst_y;

    x = 0u;
    y = 0u;
    src_index = 0u;
    dst_index = 0u;
    dst_x = 0;
    dst_y = 0;

    if (canvas_rgb_premul == NULL || canvas_alpha == NULL ||
        layer == NULL || src == NULL) {
        return;
    }

    for (y = 0u; y < layer->height; ++y) {
        dst_y = layer->top + (int)y;
        if (dst_y < 0 || dst_y >= (int)canvas_height) {
            continue;
        }
        for (x = 0u; x < layer->width; ++x) {
            float as;
            float ab;
            float cb_r;
            float cb_g;
            float cb_b;
            float cs_r;
            float cs_g;
            float cs_b;
            float blend_r;
            float blend_g;
            float blend_b;
            float out_a;
            float out_r;
            float out_g;
            float out_b;

            dst_x = layer->left + (int)x;
            if (dst_x < 0 || dst_x >= (int)canvas_width) {
                continue;
            }
            src_index = (size_t)y * (size_t)layer->width + (size_t)x;
            dst_index = (size_t)dst_y * (size_t)canvas_width + (size_t)dst_x;

            as = src->alpha[src_index];
            if (clip_alpha_map != NULL) {
                as *= sixel_builtin_psd_clamp_alpha_float32(clip_alpha_map[dst_index]);
            }
            as = sixel_builtin_psd_clamp_alpha_float32(as);
            if (as <= 0.0f) {
                continue;
            }

            cs_r = sixel_builtin_psd_clamp01(src->rgb_linear[src_index * 3u + 0u]);
            cs_g = sixel_builtin_psd_clamp01(src->rgb_linear[src_index * 3u + 1u]);
            cs_b = sixel_builtin_psd_clamp01(src->rgb_linear[src_index * 3u + 2u]);

            ab = sixel_builtin_psd_clamp_alpha_float32(canvas_alpha[dst_index]);
            if (ab > 0.0f) {
                cb_r = canvas_rgb_premul[dst_index * 3u + 0u] / ab;
                cb_g = canvas_rgb_premul[dst_index * 3u + 1u] / ab;
                cb_b = canvas_rgb_premul[dst_index * 3u + 2u] / ab;
            } else {
                cb_r = 0.0f;
                cb_g = 0.0f;
                cb_b = 0.0f;
            }
            cb_r = sixel_builtin_psd_clamp01(cb_r);
            cb_g = sixel_builtin_psd_clamp01(cb_g);
            cb_b = sixel_builtin_psd_clamp01(cb_b);

            sixel_builtin_psd_blend_rgb(cb_r,
                                        cb_g,
                                        cb_b,
                                        cs_r,
                                        cs_g,
                                        cs_b,
                                        mode,
                                        &blend_r,
                                        &blend_g,
                                        &blend_b);

            out_a = as + ab - as * ab;
            out_r = canvas_rgb_premul[dst_index * 3u + 0u] * (1.0f - as)
                + cs_r * as * (1.0f - ab) + blend_r * as * ab;
            out_g = canvas_rgb_premul[dst_index * 3u + 1u] * (1.0f - as)
                + cs_g * as * (1.0f - ab) + blend_g * as * ab;
            out_b = canvas_rgb_premul[dst_index * 3u + 2u] * (1.0f - as)
                + cs_b * as * (1.0f - ab) + blend_b * as * ab;

            canvas_alpha[dst_index] = sixel_builtin_psd_clamp_alpha_float32(out_a);
            canvas_rgb_premul[dst_index * 3u + 0u] = sixel_builtin_psd_clamp01(out_r);
            canvas_rgb_premul[dst_index * 3u + 1u] = sixel_builtin_psd_clamp01(out_g);
            canvas_rgb_premul[dst_index * 3u + 2u] = sixel_builtin_psd_clamp01(out_b);
        }
    }
}

static void
sixel_builtin_psd_build_clip_alpha_map(
    float *clip_alpha_map,
    unsigned int canvas_width,
    unsigned int canvas_height,
    sixel_builtin_psd_layer_record_t const *layer,
    sixel_builtin_psd_layer_buffers_t const *src)
{
    unsigned int x;
    unsigned int y;
    int dst_x;
    int dst_y;
    size_t src_index;
    size_t dst_index;

    x = 0u;
    y = 0u;
    dst_x = 0;
    dst_y = 0;
    src_index = 0u;
    dst_index = 0u;

    if (clip_alpha_map == NULL || layer == NULL || src == NULL) {
        return;
    }
    memset(clip_alpha_map,
           0,
           (size_t)canvas_width * (size_t)canvas_height * sizeof(float));
    for (y = 0u; y < layer->height; ++y) {
        dst_y = layer->top + (int)y;
        if (dst_y < 0 || dst_y >= (int)canvas_height) {
            continue;
        }
        for (x = 0u; x < layer->width; ++x) {
            dst_x = layer->left + (int)x;
            if (dst_x < 0 || dst_x >= (int)canvas_width) {
                continue;
            }
            src_index = (size_t)y * (size_t)layer->width + (size_t)x;
            dst_index = (size_t)dst_y * (size_t)canvas_width + (size_t)dst_x;
            clip_alpha_map[dst_index] = sixel_builtin_psd_clamp_alpha_float32(
                src->alpha[src_index]);
        }
    }
}

static SIXELSTATUS
sixel_builtin_psd_finalize_multilayer_output(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    float *canvas_rgb_premul,
    float *canvas_alpha,
    unsigned char *bgcolor,
    int preserve_alpha,
    int cms_applied,
    int *pcms_applied,
    sixel_builtin_psd_multilayer_output_kind_t output_kind,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    size_t pixel_count;
    size_t i;
    unsigned char *pixels_u8;
    float *pixels_f32;
    unsigned char *transparent_mask;
    float bg_linear[3];
    SIXELSTATUS status;
    int final_pixelformat;
    int actual_output_kind;

    pixel_count = 0u;
    i = 0u;
    pixels_u8 = NULL;
    pixels_f32 = NULL;
    transparent_mask = NULL;
    bg_linear[0] = 0.0f;
    bg_linear[1] = 0.0f;
    bg_linear[2] = 0.0f;
    status = SIXEL_FALSE;
    final_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    actual_output_kind = (int)output_kind;

    if (chunk == NULL || info == NULL || canvas_rgb_premul == NULL ||
        canvas_alpha == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;

    if (actual_output_kind == (int)SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_CMYK8_DYNAMIC) {
        if (cms_applied != 0) {
            actual_output_kind = (int)SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_RGBFLOAT32;
        } else {
            actual_output_kind = (int)SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_RGB888;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                                   pixel_count);
        if (transparent_mask == NULL) {
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        for (i = 0u; i < pixel_count; ++i) {
            transparent_mask[i] = canvas_alpha[i] <= 0.0f ? 1u : 0u;
        }
    } else if (bgcolor != NULL) {
        status = sixel_builtin_psd_rgb_bgcolor_to_linear(bgcolor, bg_linear);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(chunk->allocator, transparent_mask);
            return status;
        }
    }

    if (actual_output_kind == (int)SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_RGB888) {
        if (pixel_count > SIZE_MAX / 3u) {
            sixel_allocator_free(chunk->allocator, transparent_mask);
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        pixels_u8 = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                            pixel_count * 3u);
        if (pixels_u8 == NULL) {
            sixel_allocator_free(chunk->allocator, transparent_mask);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        for (i = 0u; i < pixel_count; ++i) {
            float r;
            float g;
            float b;
            float a;

            r = canvas_rgb_premul[i * 3u + 0u];
            g = canvas_rgb_premul[i * 3u + 1u];
            b = canvas_rgb_premul[i * 3u + 2u];
            a = canvas_alpha[i];
            if (preserve_alpha == 0 && bgcolor != NULL) {
                r += bg_linear[0] * (1.0f - a);
                g += bg_linear[1] * (1.0f - a);
                b += bg_linear[2] * (1.0f - a);
            }
            pixels_u8[i * 3u + 0u] = (unsigned char)sixel_builtin_psd_linear_to_byte(r);
            pixels_u8[i * 3u + 1u] = (unsigned char)sixel_builtin_psd_linear_to_byte(g);
            pixels_u8[i * 3u + 2u] = (unsigned char)sixel_builtin_psd_linear_to_byte(b);
        }
        final_pixelformat = SIXEL_PIXELFORMAT_RGB888;
        *ppixels = pixels_u8;
    } else {
        if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
            sixel_allocator_free(chunk->allocator, transparent_mask);
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        pixels_f32 = (float *)sixel_allocator_malloc(chunk->allocator,
                                                     pixel_count * 3u * sizeof(float));
        if (pixels_f32 == NULL) {
            sixel_allocator_free(chunk->allocator, transparent_mask);
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        for (i = 0u; i < pixel_count; ++i) {
            float r;
            float g;
            float b;
            float a;

            r = canvas_rgb_premul[i * 3u + 0u];
            g = canvas_rgb_premul[i * 3u + 1u];
            b = canvas_rgb_premul[i * 3u + 2u];
            a = canvas_alpha[i];
            if (preserve_alpha == 0 && bgcolor != NULL) {
                r += bg_linear[0] * (1.0f - a);
                g += bg_linear[1] * (1.0f - a);
                b += bg_linear[2] * (1.0f - a);
            }
            pixels_f32[i * 3u + 0u] = sixel_builtin_psd_clamp01(r);
            pixels_f32[i * 3u + 1u] = sixel_builtin_psd_clamp01(g);
            pixels_f32[i * 3u + 2u] = sixel_builtin_psd_clamp01(b);
        }
        if (actual_output_kind ==
            (int)SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_RGBFLOAT32) {
            for (i = 0u; i < pixel_count * 3u; ++i) {
                pixels_f32[i] = sixel_builtin_psd_linear_to_gamma(pixels_f32[i]);
            }
            final_pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
        } else if (actual_output_kind ==
                   (int)SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_LINEARRGBFLOAT32) {
            final_pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        } else {
            status = sixel_helper_convert_colorspace(
                (unsigned char *)pixels_f32,
                pixel_count * 3u * sizeof(float),
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                SIXEL_COLORSPACE_LINEAR,
                SIXEL_COLORSPACE_CIELAB);
            if (SIXEL_FAILED(status)) {
                sixel_allocator_free(chunk->allocator, pixels_f32);
                sixel_allocator_free(chunk->allocator, transparent_mask);
                return status;
            }
            final_pixelformat = SIXEL_PIXELFORMAT_CIELABFLOAT32;
        }
        *ppixels = (unsigned char *)pixels_f32;
    }

    if (pcms_applied != NULL) {
        *pcms_applied = cms_applied != 0 ? 1 : 0;
    }
    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        *ppixels,
                                        info,
                                        final_pixelformat);
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_decode_psd_multilayer_missing_composite(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char const *icc_profile,
    size_t icc_profile_length,
    int *pcms_applied,
    sixel_builtin_psd_multilayer_output_kind_t output_kind,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat)
{
    sixel_builtin_psd_layer_model_t model;
    size_t pixel_count;
    float *canvas_rgb_premul;
    float *canvas_alpha;
    float *clip_alpha_map;
    int clip_alpha_valid;
    int preserve_alpha;
    int cms_applied;
    int has_transparency;
    int i;
    SIXELSTATUS status;

    sixel_builtin_psd_layer_model_init(&model);
    pixel_count = 0u;
    canvas_rgb_premul = NULL;
    canvas_alpha = NULL;
    clip_alpha_map = NULL;
    clip_alpha_valid = 0;
    preserve_alpha = 0;
    cms_applied = 0;
    has_transparency = 0;
    i = 0;
    status = SIXEL_FALSE;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(float) ||
        pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    preserve_alpha = 0;

    status = sixel_builtin_psd_parse_layer_model(chunk, info, &model);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (model.layer_count <= 1u) {
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    canvas_rgb_premul = (float *)sixel_allocator_malloc(
        chunk->allocator,
        pixel_count * 3u * sizeof(float));
    canvas_alpha = (float *)sixel_allocator_malloc(
        chunk->allocator,
        pixel_count * sizeof(float));
    clip_alpha_map = (float *)sixel_allocator_malloc(
        chunk->allocator,
        pixel_count * sizeof(float));
    if (canvas_rgb_premul == NULL || canvas_alpha == NULL || clip_alpha_map == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup;
    }
    memset(canvas_rgb_premul, 0, pixel_count * 3u * sizeof(float));
    memset(canvas_alpha, 0, pixel_count * sizeof(float));
    memset(clip_alpha_map, 0, pixel_count * sizeof(float));

    for (i = (int)model.layer_count - 1; i >= 0; --i) {
        sixel_builtin_psd_layer_record_t const *layer;
        sixel_builtin_psd_layer_buffers_t src_layer;
        sixel_builtin_psd_layer_blend_mode_t blend_mode;
        int synthetic_fill;

        layer = &model.layers[(size_t)i];
        src_layer.rgb_linear = NULL;
        src_layer.alpha = NULL;
        src_layer.pixel_count = 0u;
        synthetic_fill = 0;

        if (layer->has_non_pixel_payload != 0) {
            sixel_trace_topic_message(
                "psd_decode",
                "builtin PSD: ignoring non-pixel payload in layer fallback");
            if (!sixel_builtin_psd_layer_has_decodable_pixel_channels(info,
                                                                       layer)) {
                if (layer->fill_kind != SIXEL_BUILTIN_PSD_FILL_NONE) {
                    sixel_trace_topic_message(
                        "psd_decode",
                        "builtin PSD: rendering non-pixel fill payload "
                        "in layer fallback");
                    status = sixel_builtin_psd_generate_non_pixel_fill_layer(
                        chunk,
                        info,
                        layer,
                        &src_layer);
                    if (SIXEL_FAILED(status)) {
                        goto cleanup;
                    }
                    synthetic_fill = 1;
                }
            }
            if (synthetic_fill == 0 &&
                !sixel_builtin_psd_layer_has_decodable_pixel_channels(info,
                                                                       layer)) {
                if (layer->has_fill_payload != 0 &&
                    layer->has_malformed_fill_payload != 0) {
                    sixel_trace_topic_message(
                        "psd_decode",
                        "builtin PSD: malformed non-pixel fill payload; "
                        "skipping layer");
                }
                if (layer->clipping == 0u) {
                    clip_alpha_valid = 0;
                }
                continue;
            }
        }
        if (layer->has_vector_mask != 0) {
            sixel_trace_topic_message(
                "psd_decode",
                "builtin PSD: ignoring vector mask in layer fallback");
        }
        if (layer->has_layer_effects != 0) {
            sixel_trace_topic_message(
                "psd_decode",
                "builtin PSD: ignoring layer effects in layer fallback");
        }
        if (layer->has_knockout != 0) {
            sixel_trace_topic_message(
                "psd_decode",
                "builtin PSD: ignoring knockout in layer fallback");
        }
        if (!sixel_builtin_psd_layer_blend_mode_from_key(layer->blend_key,
                                                         &blend_mode)) {
            sixel_trace_topic_message(
                "psd_decode",
                "builtin PSD: unknown layer blend mode; "
                "falling back to Normal");
            blend_mode = SIXEL_BUILTIN_PSD_BLEND_NORMAL;
        }

        if (layer->visible == 0) {
            if (layer->clipping == 0u) {
                clip_alpha_valid = 0;
            }
            continue;
        }
        if (layer->clipping != 0u && clip_alpha_valid == 0) {
            sixel_helper_set_additional_message(
                "builtin PSD: unsupported layer fallback layout");
            status = SIXEL_STBI_ERROR;
            goto cleanup;
        }

        if (synthetic_fill == 0) {
            status = sixel_builtin_psd_decode_layer_to_linear(chunk,
                                                              info,
                                                              layer,
                                                              icc_profile,
                                                              icc_profile_length,
                                                              &cms_applied,
                                                              &src_layer);
            if (SIXEL_FAILED(status)) {
                sixel_builtin_psd_layer_buffers_destroy(chunk->allocator, &src_layer);
                goto cleanup;
            }
        }
        sixel_builtin_psd_composite_layer_over(canvas_rgb_premul,
                                               canvas_alpha,
                                               info->width,
                                               info->height,
                                               layer,
                                               &src_layer,
                                               layer->clipping != 0u
                                                   ? clip_alpha_map
                                                   : NULL,
                                               blend_mode);
        if (layer->clipping == 0u) {
            sixel_builtin_psd_build_clip_alpha_map(clip_alpha_map,
                                                   info->width,
                                                   info->height,
                                                   layer,
                                                   &src_layer);
            clip_alpha_valid = 1;
        }
        sixel_builtin_psd_layer_buffers_destroy(chunk->allocator, &src_layer);
    }
    if (bgcolor == NULL) {
        size_t alpha_index;

        for (alpha_index = 0u; alpha_index < pixel_count; ++alpha_index) {
            if (canvas_alpha[alpha_index] < 0.999999f) {
                has_transparency = 1;
                break;
            }
        }
    }
    preserve_alpha = (bgcolor == NULL && has_transparency != 0) ? 1 : 0;

    status = sixel_builtin_psd_finalize_multilayer_output(
        chunk,
        info,
        canvas_rgb_premul,
        canvas_alpha,
        bgcolor,
        preserve_alpha,
        cms_applied,
        pcms_applied,
        output_kind,
        ppixels,
        ptransparent_mask,
        ptransparent_mask_size,
        pwidth,
        pheight,
        ppixelformat);

cleanup:
    if (clip_alpha_map != NULL) {
        sixel_allocator_free(chunk->allocator, clip_alpha_map);
    }
    if (canvas_alpha != NULL) {
        sixel_allocator_free(chunk->allocator, canvas_alpha);
    }
    if (canvas_rgb_premul != NULL) {
        sixel_allocator_free(chunk->allocator, canvas_rgb_premul);
    }
    sixel_builtin_psd_layer_model_destroy(chunk->allocator, &model);
    return status;
}

static int
sixel_builtin_psd_decode_layer_channel_8bit(
    unsigned char const *data,
    size_t length,
    unsigned int width,
    unsigned int height,
    size_t row_length_field_size,
    unsigned char *dst)
{
#define SIXEL_BUILTIN_PSD_LAYER_DECODE_UNSUPPORTED_COMPRESSION (-1)
#define SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_TABLE (-2)
#define SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_LENGTH (-3)
    unsigned int compression;
    size_t payload_offset;
    size_t payload_length;
    size_t pixel_count;
    size_t row;
    size_t row_table_bytes;
    size_t row_data_offset;
    size_t row_length;
    size_t row_bytes;

    compression = 0u;
    payload_offset = 0u;
    payload_length = 0u;
    pixel_count = 0u;
    row = 0u;
    row_table_bytes = 0u;
    row_data_offset = 0u;
    row_length = 0u;
    row_bytes = 0u;

    if (data == NULL || dst == NULL || width == 0u || height == 0u) {
        return 0;
    }
    if (row_length_field_size != 2u && row_length_field_size != 4u) {
        return 0;
    }
    if (length < 2u) {
        return 0;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return 0;
    }
    pixel_count = (size_t)width * (size_t)height;
    compression = sixel_builtin_read_u16be(data);
    payload_offset = 2u;
    payload_length = length - payload_offset;

    if (compression == 0u) {
        if (payload_length < pixel_count) {
            return 0;
        }
        memcpy(dst, data + payload_offset, pixel_count);
        return 1;
    }

    if (compression != 1u) {
        return SIXEL_BUILTIN_PSD_LAYER_DECODE_UNSUPPORTED_COMPRESSION;
    }
    row_bytes = (size_t)width;
    if (payload_length / row_length_field_size < (size_t)height) {
        return SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_TABLE;
    }
    if ((size_t)height > SIZE_MAX / row_length_field_size) {
        return 0;
    }
    row_table_bytes = (size_t)height * row_length_field_size;
    if (payload_length < row_table_bytes) {
        return SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_TABLE;
    }

    row_data_offset = payload_offset + row_table_bytes;
    for (row = 0u; row < (size_t)height; ++row) {
        if (row_length_field_size == 4u) {
            row_length = sixel_builtin_read_u32be_size(
                data + payload_offset + row * row_length_field_size);
        } else {
            row_length = sixel_builtin_read_u16be(
                data + payload_offset + row * row_length_field_size);
        }
        if (row_length > length - row_data_offset) {
            return SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_LENGTH;
        }
        if (!sixel_builtin_psd_unpack_packbits_row(data + row_data_offset,
                                                   row_length,
                                                   dst + row * row_bytes,
                                                   row_bytes)) {
            return 0;
        }
        row_data_offset += row_length;
    }
    if (row_data_offset != length) {
        return 0;
    }

    return 1;
#undef SIXEL_BUILTIN_PSD_LAYER_DECODE_UNSUPPORTED_COMPRESSION
#undef SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_TABLE
#undef SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_LENGTH
}

static int
sixel_builtin_psd_decode_layer_channel_16bit(
    unsigned char const *data,
    size_t length,
    unsigned int width,
    unsigned int height,
    size_t row_length_field_size,
    uint16_t *dst)
{
#define SIXEL_BUILTIN_PSD_LAYER_DECODE_UNSUPPORTED_COMPRESSION (-1)
#define SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_TABLE (-2)
#define SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_LENGTH (-3)
    unsigned int compression;
    size_t payload_offset;
    size_t payload_length;
    size_t pixel_count;
    size_t row;
    size_t row_table_bytes;
    size_t row_data_offset;
    size_t row_length;
    size_t row_bytes;
    size_t i;
    unsigned char *dst_bytes;

    compression = 0u;
    payload_offset = 0u;
    payload_length = 0u;
    pixel_count = 0u;
    row = 0u;
    row_table_bytes = 0u;
    row_data_offset = 0u;
    row_length = 0u;
    row_bytes = 0u;
    i = 0u;
    dst_bytes = NULL;

    if (data == NULL || dst == NULL || width == 0u || height == 0u) {
        return 0;
    }
    if (row_length_field_size != 2u && row_length_field_size != 4u) {
        return 0;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return 0;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / 2u) {
        return 0;
    }
    row_bytes = (size_t)width * 2u;
    dst_bytes = (unsigned char *)dst;

    if (length < 2u) {
        return 0;
    }
    compression = sixel_builtin_read_u16be(data);
    payload_offset = 2u;
    payload_length = length - payload_offset;

    if (compression == 0u) {
        if (payload_length < pixel_count * 2u) {
            return 0;
        }
        for (i = 0u; i < pixel_count; ++i) {
            dst[i] = sixel_builtin_read_u16be_as_u16(
                data + payload_offset + i * 2u);
        }
        return 1;
    }

    if (compression != 1u) {
        return SIXEL_BUILTIN_PSD_LAYER_DECODE_UNSUPPORTED_COMPRESSION;
    }
    if (payload_length / row_length_field_size < (size_t)height) {
        return SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_TABLE;
    }
    if ((size_t)height > SIZE_MAX / row_length_field_size) {
        return 0;
    }
    row_table_bytes = (size_t)height * row_length_field_size;
    if (payload_length < row_table_bytes) {
        return SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_TABLE;
    }

    row_data_offset = payload_offset + row_table_bytes;
    for (row = 0u; row < (size_t)height; ++row) {
        if (row_length_field_size == 4u) {
            row_length = sixel_builtin_read_u32be_size(
                data + payload_offset + row * row_length_field_size);
        } else {
            row_length = sixel_builtin_read_u16be(
                data + payload_offset + row * row_length_field_size);
        }
        if (row_length > length - row_data_offset) {
            return SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_LENGTH;
        }
        if (!sixel_builtin_psd_unpack_packbits_row(data + row_data_offset,
                                                   row_length,
                                                   dst_bytes + row * row_bytes,
                                                   row_bytes)) {
            return 0;
        }
        row_data_offset += row_length;
    }
    if (row_data_offset != length) {
        return 0;
    }

    for (i = 0u; i < pixel_count; ++i) {
        dst[i] = sixel_builtin_read_u16be_as_u16(dst_bytes + i * 2u);
    }

    return 1;
#undef SIXEL_BUILTIN_PSD_LAYER_DECODE_UNSUPPORTED_COMPRESSION
#undef SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_TABLE
#undef SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_LENGTH
}

static int
sixel_builtin_psd_decode_layer_channel_32bit(
    unsigned char const *data,
    size_t length,
    unsigned int width,
    unsigned int height,
    size_t row_length_field_size,
    float *dst)
{
#define SIXEL_BUILTIN_PSD_LAYER_DECODE_UNSUPPORTED_COMPRESSION (-1)
#define SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_TABLE (-2)
#define SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_LENGTH (-3)
    unsigned int compression;
    size_t payload_offset;
    size_t payload_length;
    size_t pixel_count;
    size_t row;
    size_t row_table_bytes;
    size_t row_data_offset;
    size_t row_length;
    size_t row_bytes;
    size_t i;
    unsigned char *dst_bytes;
    uint32_t bits;

    compression = 0u;
    payload_offset = 0u;
    payload_length = 0u;
    pixel_count = 0u;
    row = 0u;
    row_table_bytes = 0u;
    row_data_offset = 0u;
    row_length = 0u;
    row_bytes = 0u;
    i = 0u;
    dst_bytes = NULL;
    bits = 0u;

    if (data == NULL || dst == NULL || width == 0u || height == 0u) {
        return 0;
    }
    if (row_length_field_size != 2u && row_length_field_size != 4u) {
        return 0;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return 0;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / 4u) {
        return 0;
    }
    row_bytes = (size_t)width * 4u;
    dst_bytes = (unsigned char *)dst;

    if (length < 2u) {
        return 0;
    }
    compression = sixel_builtin_read_u16be(data);
    payload_offset = 2u;
    payload_length = length - payload_offset;

    if (compression == 0u) {
        if (payload_length < pixel_count * 4u) {
            return 0;
        }
        for (i = 0u; i < pixel_count; ++i) {
            bits = sixel_builtin_read_u32be(data + payload_offset + i * 4u);
            memcpy(dst + i, &bits, sizeof(bits));
        }
        return 1;
    }

    if (compression != 1u) {
        return SIXEL_BUILTIN_PSD_LAYER_DECODE_UNSUPPORTED_COMPRESSION;
    }
    if (payload_length / row_length_field_size < (size_t)height) {
        return SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_TABLE;
    }
    if ((size_t)height > SIZE_MAX / row_length_field_size) {
        return 0;
    }
    row_table_bytes = (size_t)height * row_length_field_size;
    if (payload_length < row_table_bytes) {
        return SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_TABLE;
    }

    row_data_offset = payload_offset + row_table_bytes;
    for (row = 0u; row < (size_t)height; ++row) {
        if (row_length_field_size == 4u) {
            row_length = sixel_builtin_read_u32be_size(
                data + payload_offset + row * row_length_field_size);
        } else {
            row_length = sixel_builtin_read_u16be(
                data + payload_offset + row * row_length_field_size);
        }
        if (row_length > length - row_data_offset) {
            return SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_LENGTH;
        }
        if (!sixel_builtin_psd_unpack_packbits_row(data + row_data_offset,
                                                   row_length,
                                                   dst_bytes + row * row_bytes,
                                                   row_bytes)) {
            return 0;
        }
        row_data_offset += row_length;
    }
    if (row_data_offset != length) {
        return 0;
    }

    for (i = 0u; i < pixel_count; ++i) {
        bits = sixel_builtin_read_u32be(dst_bytes + i * 4u);
        memcpy(dst + i, &bits, sizeof(bits));
    }

    return 1;
#undef SIXEL_BUILTIN_PSD_LAYER_DECODE_UNSUPPORTED_COMPRESSION
#undef SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_TABLE
#undef SIXEL_BUILTIN_PSD_LAYER_DECODE_MALFORMED_ROW_LENGTH
}

static SIXELSTATUS
sixel_builtin_psd_decode_layer_plane_rgb8(
    unsigned char const *buffer,
    sixel_builtin_psd_layer_channel_entry_t const *channels,
    int channel_index,
    unsigned int width,
    unsigned int height,
    unsigned char *dst)
{
    return sixel_builtin_psd_decode_layer_plane_rgb8_with_row_length(
        buffer,
        channels,
        channel_index,
        width,
        height,
        2u,
        dst);
}

static SIXELSTATUS
sixel_builtin_psd_decode_layer_plane_rgb8_with_row_length(
    unsigned char const *buffer,
    sixel_builtin_psd_layer_channel_entry_t const *channels,
    int channel_index,
    unsigned int width,
    unsigned int height,
    size_t row_length_field_size,
    unsigned char *dst)
{
    int decode_status;

    decode_status = 0;

    if (buffer == NULL || channels == NULL || channel_index < 0 ||
        dst == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    decode_status = sixel_builtin_psd_decode_layer_channel_8bit(
        buffer + channels[(size_t)channel_index].data_offset,
        channels[(size_t)channel_index].length,
        width,
        height,
        row_length_field_size,
        dst);
    if (decode_status > 0) {
        return SIXEL_OK;
    }

    if (decode_status == -1) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer channel compression");
    } else if (decode_status == -2 && row_length_field_size == 4u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed PSB RLE row table (too short)");
    } else if (decode_status == -3 && row_length_field_size == 4u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed PSB RLE row length");
    } else {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel stream");
    }
    return SIXEL_STBI_ERROR;
}

static SIXELSTATUS
sixel_builtin_psd_decode_layer_plane_rgb16(
    unsigned char const *buffer,
    sixel_builtin_psd_layer_channel_entry_t const *channels,
    int channel_index,
    unsigned int width,
    unsigned int height,
    uint16_t *dst)
{
    return sixel_builtin_psd_decode_layer_plane_rgb16_with_row_length(
        buffer,
        channels,
        channel_index,
        width,
        height,
        2u,
        dst);
}

static SIXELSTATUS
sixel_builtin_psd_decode_layer_plane_rgb16_with_row_length(
    unsigned char const *buffer,
    sixel_builtin_psd_layer_channel_entry_t const *channels,
    int channel_index,
    unsigned int width,
    unsigned int height,
    size_t row_length_field_size,
    uint16_t *dst)
{
    int decode_status;

    decode_status = 0;

    if (buffer == NULL || channels == NULL || channel_index < 0 ||
        dst == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    decode_status = sixel_builtin_psd_decode_layer_channel_16bit(
        buffer + channels[(size_t)channel_index].data_offset,
        channels[(size_t)channel_index].length,
        width,
        height,
        row_length_field_size,
        dst);
    if (decode_status > 0) {
        return SIXEL_OK;
    }

    if (decode_status == -1) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer channel compression");
    } else if (decode_status == -2 && row_length_field_size == 4u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed PSB RLE row table (too short)");
    } else if (decode_status == -3 && row_length_field_size == 4u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed PSB RLE row length");
    } else {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel stream");
    }
    return SIXEL_STBI_ERROR;
}

static SIXELSTATUS
sixel_builtin_psd_decode_layer_plane_rgb32(
    unsigned char const *buffer,
    sixel_builtin_psd_layer_channel_entry_t const *channels,
    int channel_index,
    unsigned int width,
    unsigned int height,
    float *dst)
{
    return sixel_builtin_psd_decode_layer_plane_rgb32_with_row_length(
        buffer,
        channels,
        channel_index,
        width,
        height,
        2u,
        dst);
}

static SIXELSTATUS
sixel_builtin_psd_decode_layer_plane_rgb32_with_row_length(
    unsigned char const *buffer,
    sixel_builtin_psd_layer_channel_entry_t const *channels,
    int channel_index,
    unsigned int width,
    unsigned int height,
    size_t row_length_field_size,
    float *dst)
{
    int decode_status;

    decode_status = 0;

    if (buffer == NULL || channels == NULL || channel_index < 0 ||
        dst == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    decode_status = sixel_builtin_psd_decode_layer_channel_32bit(
        buffer + channels[(size_t)channel_index].data_offset,
        channels[(size_t)channel_index].length,
        width,
        height,
        row_length_field_size,
        dst);
    if (decode_status > 0) {
        return SIXEL_OK;
    }

    if (decode_status == -1) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer channel compression");
    } else if (decode_status == -2 && row_length_field_size == 4u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed PSB RLE row table (too short)");
    } else if (decode_status == -3 && row_length_field_size == 4u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed PSB RLE row length");
    } else {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel stream");
    }
    return SIXEL_STBI_ERROR;
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
    size_t row_length_field_size;
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
    row_length_field_size = 2u;
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
        row_length_field_size = sixel_builtin_psd_rle_row_length_field_size(
            info);
        if (row_length_field_size == 0u ||
            table_entries > SIZE_MAX / row_length_field_size) {
            return 0;
        }
        table_bytes = table_entries * row_length_field_size;
        if (info->image_data_offset > chunk->size ||
            table_bytes > chunk->size - info->image_data_offset) {
            return 0;
        }
        row_table = chunk->buffer + info->image_data_offset;
        data_cursor = info->image_data_offset + table_bytes;

        for (channel = 0u; channel < info->channels; ++channel) {
            for (row = 0u; row < info->height; ++row) {
                row_index = ((size_t)channel * (size_t)info->height +
                             (size_t)row) * row_length_field_size;
                row_length = sixel_builtin_psd_read_rle_row_length(
                    info,
                    row_table,
                    row_index);
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
    size_t row_length_field_size;
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
    row_length_field_size = 2u;
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
        row_length_field_size = sixel_builtin_psd_rle_row_length_field_size(
            info);
        if (row_length_field_size == 0u ||
            table_entries > SIZE_MAX / row_length_field_size) {
            goto fail;
        }
        table_bytes = table_entries * row_length_field_size;
        if (info->image_data_offset > chunk->size ||
            table_bytes > chunk->size - info->image_data_offset) {
            goto fail;
        }
        row_table = chunk->buffer + info->image_data_offset;
        data_cursor = info->image_data_offset + table_bytes;

        for (channel = 0u; channel < info->channels; ++channel) {
            for (row = 0u; row < info->height; ++row) {
                row_index = ((size_t)channel * (size_t)info->height +
                             (size_t)row) * row_length_field_size;
                row_length = sixel_builtin_psd_read_rle_row_length(
                    info,
                    row_table,
                    row_index);
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

static uint16_t
sixel_builtin_psd_decode_cmyk_u16(uint16_t value)
{
    /* PSD stores CMYK channels with inverted polarity. */
    return (uint16_t)(0xffffu - value);
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
    size_t table_entries;
    size_t table_bytes;
    size_t row_length_field_size;
    unsigned char const *row_table;
    size_t data_cursor;
    unsigned int channel;
    unsigned int row;
    size_t row_length;
    size_t row_index;
    size_t row_offset;
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
    table_entries = 0u;
    table_bytes = 0u;
    row_length_field_size = 2u;
    row_table = NULL;
    data_cursor = 0u;
    channel = 0u;
    row = 0u;
    row_length = 0u;
    row_index = 0u;
    row_offset = 0u;
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
    } else if (info->compression == 1u) {
        if ((size_t)info->height > SIZE_MAX / (size_t)info->channels) {
            goto fail;
        }
        table_entries = (size_t)info->height * (size_t)info->channels;
        row_length_field_size = sixel_builtin_psd_rle_row_length_field_size(
            info);
        if (row_length_field_size == 0u ||
            table_entries > SIZE_MAX / row_length_field_size) {
            goto fail;
        }
        table_bytes = table_entries * row_length_field_size;
        if (info->image_data_offset > chunk->size ||
            table_bytes > chunk->size - info->image_data_offset) {
            goto fail;
        }
        row_table = chunk->buffer + info->image_data_offset;
        data_cursor = info->image_data_offset + table_bytes;

        for (channel = 0u; channel < info->channels; ++channel) {
            for (row = 0u; row < info->height; ++row) {
                row_index = ((size_t)channel * (size_t)info->height +
                             (size_t)row) * row_length_field_size;
                row_length = sixel_builtin_psd_read_rle_row_length(
                    info,
                    row_table,
                    row_index);
                if (data_cursor > chunk->size ||
                    row_length > chunk->size - data_cursor) {
                    goto fail;
                }
                if (channel == target_channel) {
                    row_offset = (size_t)row * row_bytes;
                    if (!sixel_builtin_psd_unpack_packbits_row(
                            chunk->buffer + data_cursor,
                            row_length,
                            plane_bytes_buffer + row_offset,
                            row_bytes)) {
                        goto fail;
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
    size_t row_length_field_size;
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
    row_length_field_size = 2u;
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
        row_length_field_size = sixel_builtin_psd_rle_row_length_field_size(
            info);
        if (row_length_field_size == 0u ||
            table_entries > SIZE_MAX / row_length_field_size) {
            return 0;
        }
        table_bytes = table_entries * row_length_field_size;
        if (info->image_data_offset > chunk->size ||
            table_bytes > chunk->size - info->image_data_offset) {
            return 0;
        }
        row_table = chunk->buffer + info->image_data_offset;
        data_cursor = info->image_data_offset + table_bytes;

        for (channel = 0u; channel < info->channels; ++channel) {
            for (row = 0u; row < info->height; ++row) {
                row_index = ((size_t)channel * (size_t)info->height +
                             (size_t)row) * row_length_field_size;
                row_length = sixel_builtin_psd_read_rle_row_length(
                    info,
                    row_table,
                    row_index);
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        rgb,
                                        info,
                                        SIXEL_PIXELFORMAT_RGB888);
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_builtin_decode_psd_single_layer_missing_composite_8bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

static SIXELSTATUS
sixel_builtin_decode_psd_single_layer_missing_composite_16bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

static SIXELSTATUS
sixel_builtin_decode_psd_single_layer_missing_composite_32bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

static SIXELSTATUS
sixel_builtin_decode_psd_single_layer_missing_composite_lab_32bit(
    sixel_chunk_t const *chunk,
    sixel_builtin_psd_info_t const *info,
    unsigned char *bgcolor,
    unsigned char **ppixels,
    unsigned char **ptransparent_mask,
    size_t *ptransparent_mask_size,
    int *pwidth,
    int *pheight,
    int *ppixelformat);

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
    SIXELSTATUS layer_status;

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
    layer_status = SIXEL_FALSE;

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
    if (info->color_mode != 2u && info->image_data_offset >= chunk->size) {
        if (sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
            layer_status = sixel_builtin_decode_psd_multilayer_missing_composite(
                chunk,
                info,
                bgcolor,
                NULL,
                0u,
                NULL,
                SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_RGB888,
                ppixels,
                ptransparent_mask,
                ptransparent_mask_size,
                pwidth,
                pheight,
                ppixelformat);
            if (layer_status != SIXEL_BAD_INPUT) {
                return layer_status;
            }
        }
        return sixel_builtin_decode_psd_single_layer_missing_composite_8bit(
            chunk,
            info,
            bgcolor,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        rgb,
                                        info,
                                        SIXEL_PIXELFORMAT_RGB888);
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
    int blend_with_bg;
    float gray_f;
    float alpha_f;
    float one_minus_alpha;
    float bg_r;
    float bg_g;
    float bg_b;
    SIXELSTATUS layer_status;

    plane0 = NULL;
    plane_alpha = NULL;
    rgbf32 = NULL;
    transparent_mask = NULL;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    preserve_alpha = 0;
    blend_with_bg = 0;
    gray_f = 0.0f;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    bg_r = 0.0f;
    bg_g = 0.0f;
    bg_b = 0.0f;
    layer_status = SIXEL_FALSE;

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
    if (info->image_data_offset >= chunk->size) {
        if (sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
            layer_status = sixel_builtin_decode_psd_multilayer_missing_composite(
                chunk,
                info,
                bgcolor,
                NULL,
                0u,
                NULL,
                SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_RGBFLOAT32,
                ppixels,
                ptransparent_mask,
                ptransparent_mask_size,
                pwidth,
                pheight,
                ppixelformat);
            if (layer_status != SIXEL_BAD_INPUT) {
                return layer_status;
            }
        }
        return sixel_builtin_decode_psd_single_layer_missing_composite_16bit(
            chunk,
            info,
            bgcolor,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
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
    blend_with_bg = (want_alpha != 0 && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        sixel_allocator_free(chunk->allocator, plane0);
        return SIXEL_BAD_ARGUMENT;
    }
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

    if (blend_with_bg != 0) {
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        (unsigned char *)rgbf32,
                                        info,
                                        SIXEL_PIXELFORMAT_RGBFLOAT32);
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
    int blend_with_bg;
    float gray_f;
    float alpha_f;
    float one_minus_alpha;
    float bg_r;
    float bg_g;
    float bg_b;
    SIXELSTATUS layer_status;

    plane0 = NULL;
    plane_alpha = NULL;
    rgbf32 = NULL;
    transparent_mask = NULL;
    pixel_count = 0u;
    i = 0u;
    want_alpha = 0;
    preserve_alpha = 0;
    blend_with_bg = 0;
    gray_f = 0.0f;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    bg_r = 0.0f;
    bg_g = 0.0f;
    bg_b = 0.0f;
    layer_status = SIXEL_FALSE;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if ((info->color_mode != 1u && info->color_mode != 8u) ||
        info->depth != 32u ||
        info->compression > 3u ||
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
    if (info->image_data_offset >= chunk->size) {
        if (sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
            layer_status = sixel_builtin_decode_psd_multilayer_missing_composite(
                chunk,
                info,
                bgcolor,
                NULL,
                0u,
                NULL,
                SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_RGBFLOAT32,
                ppixels,
                ptransparent_mask,
                ptransparent_mask_size,
                pwidth,
                pheight,
                ppixelformat);
            if (layer_status != SIXEL_BAD_INPUT) {
                return layer_status;
            }
        }
        return sixel_builtin_decode_psd_single_layer_missing_composite_32bit(
            chunk,
            info,
            bgcolor,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
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
    blend_with_bg = (want_alpha != 0 && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        sixel_allocator_free(chunk->allocator, plane0);
        return SIXEL_BAD_ARGUMENT;
    }
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

    if (blend_with_bg != 0) {
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        (unsigned char *)rgbf32,
                                        info,
                                        SIXEL_PIXELFORMAT_RGBFLOAT32);
    return SIXEL_OK;
}

/* Recover 8-bit CMYK pixels from single-layer PSD without merged data. */
static SIXELSTATUS
sixel_builtin_decode_psd_single_layer_missing_composite_cmyk_8bit(
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
    unsigned char const *buffer;
    size_t section_offset;
    size_t section_end;
    size_t layer_info_length;
    size_t layer_info_offset;
    size_t layer_info_end;
    int16_t layer_count_raw;
    int layer_count;
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
    unsigned int channel_count;
    size_t cursor;
    size_t extra_data_length;
    size_t pixel_count;
    size_t i;
    int c_channel_index;
    int m_channel_index;
    int y_channel_index;
    int k_channel_index;
    int alpha_channel_index;
    int preserve_alpha;
    int blend_with_bg;
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
    unsigned char *plane_c;
    unsigned char *plane_m;
    unsigned char *plane_y;
    unsigned char *plane_k;
    unsigned char *plane_alpha;
    unsigned char *cmyk;
    unsigned char *rgb;
    float *rgbf32;
    unsigned char *transparent_mask;
    sixel_builtin_psd_layer_channel_entry_t
        channels[SIXEL_FROMPSD_MAX_CHANNELS];

    status = SIXEL_FALSE;
    buffer = NULL;
    section_offset = 0u;
    section_end = 0u;
    layer_info_length = 0u;
    layer_info_offset = 0u;
    layer_info_end = 0u;
    layer_count_raw = 0;
    layer_count = 0;
    top = 0;
    left = 0;
    bottom = 0;
    right = 0;
    channel_count = 0u;
    cursor = 0u;
    extra_data_length = 0u;
    pixel_count = 0u;
    i = 0u;
    c_channel_index = -1;
    m_channel_index = -1;
    y_channel_index = -1;
    k_channel_index = -1;
    alpha_channel_index = -1;
    preserve_alpha = 0;
    blend_with_bg = 0;
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
    plane_c = NULL;
    plane_m = NULL;
    plane_y = NULL;
    plane_k = NULL;
    plane_alpha = NULL;
    cmyk = NULL;
    rgb = NULL;
    rgbf32 = NULL;
    transparent_mask = NULL;
    memset(channels, 0, sizeof(channels));

    if (chunk == NULL || info == NULL || pcms_applied == NULL ||
        ppixels == NULL || pwidth == NULL || pheight == NULL ||
        ppixelformat == NULL || chunk->allocator == NULL ||
        chunk->buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *pcms_applied = 0;
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if ((info->color_mode != 4u && info->color_mode != 7u) ||
        info->depth != 8u ||
        info->channels < 4u) {
        return SIXEL_BAD_INPUT;
    }
    if (info->image_data_offset < chunk->size) {
        return SIXEL_BAD_INPUT;
    }
    if (info->layer_mask_length < 4u ||
        info->layer_mask_offset > chunk->size ||
        info->layer_mask_length > chunk->size - info->layer_mask_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer info section");
        return SIXEL_STBI_ERROR;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / 4u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    use_cms = (icc_profile != NULL && icc_profile_length > 0u) ? 1 : 0;

    buffer = chunk->buffer;
    section_offset = info->layer_mask_offset;
    section_end = section_offset + info->layer_mask_length;
    layer_info_length = sixel_builtin_read_u32be_size(
        buffer + section_offset);
    layer_info_offset = section_offset + 4u;
    if (layer_info_length == 0u ||
        layer_info_length > section_end - layer_info_offset ||
        layer_info_offset + layer_info_length > chunk->size) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record table");
        return SIXEL_STBI_ERROR;
    }
    layer_info_end = layer_info_offset + layer_info_length;

    if (layer_info_offset + 2u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record header");
        return SIXEL_STBI_ERROR;
    }
    layer_count_raw = sixel_builtin_read_i16be(buffer + layer_info_offset);
    layer_info_offset += 2u;
    if (layer_count_raw < 0) {
        layer_count = -(int)layer_count_raw;
    } else {
        layer_count = (int)layer_count_raw;
    }
    if (layer_count != 1) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    if (layer_info_offset + 18u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record geometry");
        return SIXEL_STBI_ERROR;
    }
    top = sixel_builtin_read_i32be(buffer + layer_info_offset);
    left = sixel_builtin_read_i32be(buffer + layer_info_offset + 4u);
    bottom = sixel_builtin_read_i32be(buffer + layer_info_offset + 8u);
    right = sixel_builtin_read_i32be(buffer + layer_info_offset + 12u);
    channel_count = sixel_builtin_read_u16be(
        buffer + layer_info_offset + 16u);
    layer_info_offset += 18u;

    if (channel_count < 4u || channel_count > SIXEL_FROMPSD_MAX_CHANNELS) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback channels");
        return SIXEL_STBI_ERROR;
    }
    if (layer_info_offset > layer_info_end ||
        (size_t)channel_count > (layer_info_end - layer_info_offset) / 6u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel table");
        return SIXEL_STBI_ERROR;
    }

    for (i = 0u; i < (size_t)channel_count; ++i) {
        channels[i].channel_id = sixel_builtin_read_i16be(
            buffer + layer_info_offset);
        channels[i].length = sixel_builtin_read_u32be_size(
            buffer + layer_info_offset + 2u);
        channels[i].data_offset = 0u;
        if (channels[i].length < 2u) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel length");
            return SIXEL_STBI_ERROR;
        }
        if (channels[i].channel_id == 0 && c_channel_index < 0) {
            c_channel_index = (int)i;
        } else if (channels[i].channel_id == 1 && m_channel_index < 0) {
            m_channel_index = (int)i;
        } else if (channels[i].channel_id == 2 && y_channel_index < 0) {
            y_channel_index = (int)i;
        } else if (channels[i].channel_id == 3 && k_channel_index < 0) {
            k_channel_index = (int)i;
        } else if (channels[i].channel_id == -1 && alpha_channel_index < 0) {
            alpha_channel_index = (int)i;
        }
        layer_info_offset += 6u;
    }

    if (layer_info_offset + 16u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer blend block");
        return SIXEL_STBI_ERROR;
    }
    extra_data_length = sixel_builtin_read_u32be_size(
        buffer + layer_info_offset + 12u);
    layer_info_offset += 16u;
    if (extra_data_length > layer_info_end - layer_info_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }
    layer_info_offset += extra_data_length;
    cursor = layer_info_offset;

    for (i = 0u; i < (size_t)channel_count; ++i) {
        if (channels[i].length > layer_info_end - cursor) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel stream");
            return SIXEL_STBI_ERROR;
        }
        channels[i].data_offset = cursor;
        cursor += channels[i].length;
    }
    if (cursor > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel stream");
        return SIXEL_STBI_ERROR;
    }

    if (top != 0 || left != 0 ||
        bottom != (int32_t)info->height ||
        right != (int32_t)info->width ||
        c_channel_index < 0 ||
        m_channel_index < 0 ||
        y_channel_index < 0 ||
        k_channel_index < 0) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    plane_c = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_m = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_y = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    plane_k = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count);
    if (plane_c == NULL || plane_m == NULL || plane_y == NULL || plane_k == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_layer_cmyk8;
    }

    status = sixel_builtin_psd_decode_layer_plane_rgb8(
        buffer,
        channels,
        c_channel_index,
        info->width,
        info->height,
        plane_c);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_cmyk8;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb8(
        buffer,
        channels,
        m_channel_index,
        info->width,
        info->height,
        plane_m);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_cmyk8;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb8(
        buffer,
        channels,
        y_channel_index,
        info->width,
        info->height,
        plane_y);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_cmyk8;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb8(
        buffer,
        channels,
        k_channel_index,
        info->width,
        info->height,
        plane_k);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_cmyk8;
    }

    preserve_alpha = (bgcolor == NULL && alpha_channel_index >= 0) ? 1 : 0;
    blend_with_bg = (alpha_channel_index >= 0 && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup_layer_cmyk8;
    }
    if (alpha_channel_index >= 0) {
        plane_alpha = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                              pixel_count);
        if (plane_alpha == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_layer_cmyk8;
        }
        status = sixel_builtin_psd_decode_layer_plane_rgb8(
            buffer,
            channels,
            alpha_channel_index,
            info->width,
            info->height,
            plane_alpha);
        if (SIXEL_FAILED(status)) {
            goto cleanup_layer_cmyk8;
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
            goto cleanup_layer_cmyk8;
        }
    }

    if (use_cms) {
        cmyk = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count * 4u);
        rgbf32 = (float *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * 3u * sizeof(float));
        if (cmyk == NULL || rgbf32 == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_layer_cmyk8;
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
                if (blend_with_bg != 0) {
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
                        if (transparent_mask != NULL) {
                            transparent_mask[i] = alpha_f <= 0.0f ? 1u : 0u;
                        }
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
            *pcms_applied = 1;
            sixel_builtin_psd_commit_transparent_mask_output(
                chunk->allocator,
                ptransparent_mask,
                ptransparent_mask_size,
                &transparent_mask,
                pixel_count,
                preserve_alpha);
            sixel_builtin_psd_set_decode_output(
                ppixels,
                pwidth,
                pheight,
                ppixelformat,
                (unsigned char *)rgbf32,
                info,
                SIXEL_PIXELFORMAT_RGBFLOAT32);
            status = SIXEL_OK;
            rgbf32 = NULL;
            goto cleanup_layer_cmyk8;
        }
    }

    rgb = (unsigned char *)sixel_allocator_malloc(chunk->allocator, pixel_count * 3u);
    if (rgb == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_layer_cmyk8;
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
                if (transparent_mask != NULL) {
                    transparent_mask[i] = alpha == 0 ? 1u : 0u;
                }
            } else if (blend_with_bg != 0) {
                r = (r * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
                g = (g * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
                b = (b * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
            } else {
                r = (r * alpha) >> 8;
                g = (g * alpha) >> 8;
                b = (b * alpha) >> 8;
            }
        }
        rgb[i * 3u + 0u] = (unsigned char)r;
        rgb[i * 3u + 1u] = (unsigned char)g;
        rgb[i * 3u + 2u] = (unsigned char)b;
    }

    *pcms_applied = 0;
    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        rgb,
                                        info,
                                        SIXEL_PIXELFORMAT_RGB888);
    status = SIXEL_OK;
    rgb = NULL;

cleanup_layer_cmyk8:
    sixel_allocator_free(chunk->allocator, rgb);
    sixel_allocator_free(chunk->allocator, rgbf32);
    sixel_allocator_free(chunk->allocator, cmyk);
    sixel_allocator_free(chunk->allocator, transparent_mask);
    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_k);
    sixel_allocator_free(chunk->allocator, plane_y);
    sixel_allocator_free(chunk->allocator, plane_m);
    sixel_allocator_free(chunk->allocator, plane_c);
    return status;
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
    int blend_with_bg;
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
    SIXELSTATUS layer_status;

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
    blend_with_bg = 0;
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
    layer_status = SIXEL_FALSE;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || pcms_applied == NULL ||
        chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *pcms_applied = 0;
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if ((info->color_mode != 4u && info->color_mode != 7u) ||
        info->depth != 8u ||
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
    if ((info->color_mode == 4u ||
         (info->color_mode == 7u && info->channels == 4u)) &&
        info->image_data_offset >= chunk->size) {
        if (sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
            layer_status = sixel_builtin_decode_psd_multilayer_missing_composite(
                chunk,
                info,
                bgcolor,
                icc_profile,
                icc_profile_length,
                pcms_applied,
                SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_CMYK8_DYNAMIC,
                ppixels,
                ptransparent_mask,
                ptransparent_mask_size,
                pwidth,
                pheight,
                ppixelformat);
            if (layer_status != SIXEL_BAD_INPUT) {
                return layer_status;
            }
        }
        return sixel_builtin_decode_psd_single_layer_missing_composite_cmyk_8bit(
            chunk,
            info,
            bgcolor,
            icc_profile,
            icc_profile_length,
            pcms_applied,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
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
    blend_with_bg = (want_alpha != 0 && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        sixel_allocator_free(chunk->allocator, plane_k);
        sixel_allocator_free(chunk->allocator, plane_y);
        sixel_allocator_free(chunk->allocator, plane_m);
        sixel_allocator_free(chunk->allocator, plane_c);
        return SIXEL_BAD_ARGUMENT;
    }
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
                if (blend_with_bg != 0) {
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
            sixel_builtin_psd_commit_transparent_mask_output(
                chunk->allocator,
                ptransparent_mask,
                ptransparent_mask_size,
                &transparent_mask,
                pixel_count,
                preserve_alpha);
            sixel_builtin_psd_set_decode_output(
                ppixels,
                pwidth,
                pheight,
                ppixelformat,
                (unsigned char *)rgbf32,
                info,
                SIXEL_PIXELFORMAT_RGBFLOAT32);
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
            } else if (blend_with_bg != 0) {
                r = (r * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
                g = (g * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
                b = (b * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
            } else {
                r = (r * alpha) >> 8;
                g = (g * alpha) >> 8;
                b = (b * alpha) >> 8;
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        rgb,
                                        info,
                                        SIXEL_PIXELFORMAT_RGB888);
    *pcms_applied = 0;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_builtin_decode_psd_single_layer_missing_composite_cmyk_16bit(
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
    unsigned char const *buffer;
    size_t section_offset;
    size_t section_end;
    size_t layer_info_length;
    size_t layer_info_offset;
    size_t layer_info_end;
    int16_t layer_count_raw;
    int layer_count;
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
    unsigned int channel_count;
    size_t cursor;
    size_t extra_data_length;
    size_t pixel_count;
    size_t i;
    int c_channel_index;
    int m_channel_index;
    int y_channel_index;
    int k_channel_index;
    int alpha_channel_index;
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
    uint16_t *plane_c;
    uint16_t *plane_m;
    uint16_t *plane_y;
    uint16_t *plane_k;
    uint16_t *plane_alpha;
    uint16_t *cmyk16;
    float *rgb_linear;
    unsigned char *transparent_mask;
    sixel_cms_profile_t *src_profile;
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;
    sixel_builtin_psd_layer_channel_entry_t
        channels[SIXEL_FROMPSD_MAX_CHANNELS];

    status = SIXEL_FALSE;
    buffer = NULL;
    section_offset = 0u;
    section_end = 0u;
    layer_info_length = 0u;
    layer_info_offset = 0u;
    layer_info_end = 0u;
    layer_count_raw = 0;
    layer_count = 0;
    top = 0;
    left = 0;
    bottom = 0;
    right = 0;
    channel_count = 0u;
    cursor = 0u;
    extra_data_length = 0u;
    pixel_count = 0u;
    i = 0u;
    c_channel_index = -1;
    m_channel_index = -1;
    y_channel_index = -1;
    k_channel_index = -1;
    alpha_channel_index = -1;
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
    plane_c = NULL;
    plane_m = NULL;
    plane_y = NULL;
    plane_k = NULL;
    plane_alpha = NULL;
    cmyk16 = NULL;
    rgb_linear = NULL;
    transparent_mask = NULL;
    src_profile = NULL;
    dst_profile = NULL;
    transform = NULL;
    memset(channels, 0, sizeof(channels));

    if (chunk == NULL || info == NULL || pcms_applied == NULL ||
        ppixels == NULL || pwidth == NULL || pheight == NULL ||
        ppixelformat == NULL || chunk->allocator == NULL ||
        chunk->buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *pcms_applied = 0;
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if ((info->color_mode != 4u &&
         info->color_mode != 7u) ||
        info->depth != 16u ||
        info->channels < 4u) {
        return SIXEL_BAD_INPUT;
    }
    if (info->image_data_offset < chunk->size) {
        return SIXEL_BAD_INPUT;
    }
    if (info->layer_mask_length < 4u ||
        info->layer_mask_offset > chunk->size ||
        info->layer_mask_length > chunk->size - info->layer_mask_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer info section");
        return SIXEL_STBI_ERROR;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(uint16_t) ||
        pixel_count > SIZE_MAX / (4u * sizeof(uint16_t)) ||
        pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    use_cms = (icc_profile != NULL && icc_profile_length > 0u) ? 1 : 0;

    buffer = chunk->buffer;
    section_offset = info->layer_mask_offset;
    section_end = section_offset + info->layer_mask_length;
    layer_info_length = sixel_builtin_read_u32be_size(buffer + section_offset);
    layer_info_offset = section_offset + 4u;
    if (layer_info_length == 0u ||
        layer_info_length > section_end - layer_info_offset ||
        layer_info_offset + layer_info_length > chunk->size) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record table");
        return SIXEL_STBI_ERROR;
    }
    layer_info_end = layer_info_offset + layer_info_length;

    if (layer_info_offset + 2u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record header");
        return SIXEL_STBI_ERROR;
    }
    layer_count_raw = sixel_builtin_read_i16be(buffer + layer_info_offset);
    layer_info_offset += 2u;
    if (layer_count_raw < 0) {
        layer_count = -(int)layer_count_raw;
    } else {
        layer_count = (int)layer_count_raw;
    }
    if (layer_count != 1) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    if (layer_info_offset + 18u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record geometry");
        return SIXEL_STBI_ERROR;
    }
    top = sixel_builtin_read_i32be(buffer + layer_info_offset);
    left = sixel_builtin_read_i32be(buffer + layer_info_offset + 4u);
    bottom = sixel_builtin_read_i32be(buffer + layer_info_offset + 8u);
    right = sixel_builtin_read_i32be(buffer + layer_info_offset + 12u);
    channel_count = sixel_builtin_read_u16be(buffer + layer_info_offset + 16u);
    layer_info_offset += 18u;

    if (channel_count < 4u || channel_count > SIXEL_FROMPSD_MAX_CHANNELS) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback channels");
        return SIXEL_STBI_ERROR;
    }
    if (layer_info_offset > layer_info_end ||
        (size_t)channel_count > (layer_info_end - layer_info_offset) / 6u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel table");
        return SIXEL_STBI_ERROR;
    }
    for (i = 0u; i < (size_t)channel_count; ++i) {
        channels[i].channel_id = sixel_builtin_read_i16be(
            buffer + layer_info_offset);
        channels[i].length = sixel_builtin_read_u32be_size(
            buffer + layer_info_offset + 2u);
        channels[i].data_offset = 0u;
        if (channels[i].length < 2u) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel length");
            return SIXEL_STBI_ERROR;
        }
        if (channels[i].channel_id == 0 && c_channel_index < 0) {
            c_channel_index = (int)i;
        } else if (channels[i].channel_id == 1 && m_channel_index < 0) {
            m_channel_index = (int)i;
        } else if (channels[i].channel_id == 2 && y_channel_index < 0) {
            y_channel_index = (int)i;
        } else if (channels[i].channel_id == 3 && k_channel_index < 0) {
            k_channel_index = (int)i;
        } else if (channels[i].channel_id == -1 && alpha_channel_index < 0) {
            alpha_channel_index = (int)i;
        }
        layer_info_offset += 6u;
    }

    if (layer_info_offset + 16u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer blend block");
        return SIXEL_STBI_ERROR;
    }
    extra_data_length = sixel_builtin_read_u32be_size(
        buffer + layer_info_offset + 12u);
    layer_info_offset += 16u;
    if (extra_data_length > layer_info_end - layer_info_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }
    layer_info_offset += extra_data_length;
    cursor = layer_info_offset;

    for (i = 0u; i < (size_t)channel_count; ++i) {
        if (channels[i].length > layer_info_end - cursor) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel stream");
            return SIXEL_STBI_ERROR;
        }
        channels[i].data_offset = cursor;
        cursor += channels[i].length;
    }
    if (cursor > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel stream");
        return SIXEL_STBI_ERROR;
    }

    if (top != 0 || left != 0 ||
        bottom != (int32_t)info->height ||
        right != (int32_t)info->width ||
        c_channel_index < 0 || m_channel_index < 0 ||
        y_channel_index < 0 || k_channel_index < 0) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    plane_c = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * sizeof(uint16_t));
    plane_m = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * sizeof(uint16_t));
    plane_y = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * sizeof(uint16_t));
    plane_k = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * sizeof(uint16_t));
    if (plane_c == NULL || plane_m == NULL || plane_y == NULL || plane_k == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_layer_cmyk16;
    }

    status = sixel_builtin_psd_decode_layer_plane_rgb16(buffer,
                                                        channels,
                                                        c_channel_index,
                                                        info->width,
                                                        info->height,
                                                        plane_c);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_cmyk16;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb16(buffer,
                                                        channels,
                                                        m_channel_index,
                                                        info->width,
                                                        info->height,
                                                        plane_m);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_cmyk16;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb16(buffer,
                                                        channels,
                                                        y_channel_index,
                                                        info->width,
                                                        info->height,
                                                        plane_y);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_cmyk16;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb16(buffer,
                                                        channels,
                                                        k_channel_index,
                                                        info->width,
                                                        info->height,
                                                        plane_k);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_cmyk16;
    }

    preserve_alpha = (bgcolor == NULL && alpha_channel_index >= 0) ? 1 : 0;
    if (alpha_channel_index >= 0) {
        plane_alpha = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                         pixel_count *
                                                         sizeof(uint16_t));
        if (plane_alpha == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_layer_cmyk16;
        }
        status = sixel_builtin_psd_decode_layer_plane_rgb16(buffer,
                                                            channels,
                                                            alpha_channel_index,
                                                            info->width,
                                                            info->height,
                                                            plane_alpha);
        if (SIXEL_FAILED(status)) {
            goto cleanup_layer_cmyk16;
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
            goto cleanup_layer_cmyk16;
        }
    }

    rgb_linear = (float *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * 3u * sizeof(float));
    if (rgb_linear == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_layer_cmyk16;
    }

    if (use_cms) {
        cmyk16 = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                    pixel_count * 4u *
                                                    sizeof(uint16_t));
        if (cmyk16 == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_layer_cmyk16;
        }
        for (i = 0u; i < pixel_count; ++i) {
            cmyk16[i * 4u + 0u] = sixel_builtin_psd_decode_cmyk_u16(plane_c[i]);
            cmyk16[i * 4u + 1u] = sixel_builtin_psd_decode_cmyk_u16(plane_m[i]);
            cmyk16[i * 4u + 2u] = sixel_builtin_psd_decode_cmyk_u16(plane_y[i]);
            cmyk16[i * 4u + 3u] = sixel_builtin_psd_decode_cmyk_u16(plane_k[i]);
        }
        src_profile = sixel_cms_open_profile_from_mem(icc_profile,
                                                      icc_profile_length);
        if (src_profile != NULL) {
            dst_profile = sixel_cms_create_linear_srgb_profile();
        }
        if (src_profile != NULL && dst_profile != NULL) {
            transform = sixel_cms_create_transform(
                src_profile,
                SIXEL_CMS_PIXELFORMAT_CMYK_16,
                dst_profile,
                SIXEL_CMS_PIXELFORMAT_RGB_F32,
                SIXEL_CMS_TRANSFORM_DEFAULT);
        }
        if (transform != NULL &&
            sixel_cms_do_transform(transform, cmyk16, rgb_linear, pixel_count)) {
            cms_applied = 1;
        }
    }

    if (!cms_applied) {
        for (i = 0u; i < pixel_count; ++i) {
            c = (float)sixel_builtin_psd_decode_cmyk_u16(plane_c[i]) / 65535.0f;
            m = (float)sixel_builtin_psd_decode_cmyk_u16(plane_m[i]) / 65535.0f;
            y = (float)sixel_builtin_psd_decode_cmyk_u16(plane_y[i]) / 65535.0f;
            k = (float)sixel_builtin_psd_decode_cmyk_u16(plane_k[i]) / 65535.0f;
            rgb_linear[i * 3u + 0u] = (1.0f - c) * (1.0f - k);
            rgb_linear[i * 3u + 1u] = (1.0f - m) * (1.0f - k);
            rgb_linear[i * 3u + 2u] = (1.0f - y) * (1.0f - k);
        }
        if (!sixel_cms_convert_rgbf32_gamma_to_linear(rgb_linear, pixel_count)) {
            status = SIXEL_BAD_INPUT;
            goto cleanup_layer_cmyk16;
        }
    }

    if (plane_alpha != NULL) {
        if (preserve_alpha != 0) {
            for (i = 0u; i < pixel_count; ++i) {
                alpha_f = (float)plane_alpha[i] / 65535.0f;
                rgb_linear[i * 3u + 0u] *= alpha_f;
                rgb_linear[i * 3u + 1u] *= alpha_f;
                rgb_linear[i * 3u + 2u] *= alpha_f;
                if (transparent_mask != NULL) {
                    transparent_mask[i] = alpha_f <= 0.0f ? 1u : 0u;
                }
            }
        } else {
            status = sixel_builtin_psd_rgb_bgcolor_to_linear(bgcolor, bg_linear);
            if (SIXEL_FAILED(status)) {
                goto cleanup_layer_cmyk16;
            }
            for (i = 0u; i < pixel_count; ++i) {
                alpha_f = (float)plane_alpha[i] / 65535.0f;
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
    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(
        ppixels,
        pwidth,
        pheight,
        ppixelformat,
        (unsigned char *)rgb_linear,
        info,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32);
    status = SIXEL_OK;
    rgb_linear = NULL;

cleanup_layer_cmyk16:
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(dst_profile);
    sixel_cms_close_profile(src_profile);
    sixel_allocator_free(chunk->allocator, cmyk16);
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
sixel_builtin_decode_psd_cmyk_16bit(
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
    uint16_t *plane_c;
    uint16_t *plane_m;
    uint16_t *plane_y;
    uint16_t *plane_k;
    uint16_t *plane_alpha;
    uint16_t *cmyk16;
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
    cmyk16 = NULL;
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
    if ((info->color_mode != 4u && info->color_mode != 7u) ||
        info->depth != 16u ||
        info->compression > 3u || info->channels < 4u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(uint16_t) ||
        pixel_count > SIZE_MAX / (4u * sizeof(uint16_t)) ||
        pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if ((info->color_mode == 4u ||
         (info->color_mode == 7u &&
          info->channels == 4u)) &&
        info->channels >= 4u &&
        info->image_data_offset >= chunk->size) {
        if (sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
            status = sixel_builtin_decode_psd_multilayer_missing_composite(
                chunk,
                info,
                bgcolor,
                icc_profile,
                icc_profile_length,
                pcms_applied,
                SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_LINEARRGBFLOAT32,
                ppixels,
                ptransparent_mask,
                ptransparent_mask_size,
                pwidth,
                pheight,
                ppixelformat);
            if (status != SIXEL_BAD_INPUT) {
                return status;
            }
        }
        return sixel_builtin_decode_psd_single_layer_missing_composite_cmyk_16bit(
            chunk,
            info,
            bgcolor,
            icc_profile,
            icc_profile_length,
            pcms_applied,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
    }
    use_cms = (icc_profile != NULL && icc_profile_length > 0u) ? 1 : 0;

    plane_c = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count *
                                                 sizeof(uint16_t));
    plane_m = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count *
                                                 sizeof(uint16_t));
    plane_y = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count *
                                                 sizeof(uint16_t));
    plane_k = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count *
                                                 sizeof(uint16_t));
    if (plane_c == NULL || plane_m == NULL || plane_y == NULL || plane_k == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_cmyk16;
    }

    preserve_alpha = (bgcolor == NULL && info->channels >= 5u) ? 1 : 0;
    want_alpha = info->channels >= 5u ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                         pixel_count *
                                                         sizeof(uint16_t));
        if (plane_alpha == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_cmyk16;
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
            goto cleanup_cmyk16;
        }
    }

    if (!sixel_builtin_decode_psd_16bit_channel(chunk, info, 0u, plane_c) ||
        !sixel_builtin_decode_psd_16bit_channel(chunk, info, 1u, plane_m) ||
        !sixel_builtin_decode_psd_16bit_channel(chunk, info, 2u, plane_y) ||
        !sixel_builtin_decode_psd_16bit_channel(chunk, info, 3u, plane_k) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_16bit_channel(chunk,
                                                 info,
                                                 4u,
                                                 plane_alpha))) {
        status = SIXEL_STBI_ERROR;
        sixel_helper_set_additional_message(
            "builtin PSD: malformed compressed channel stream");
        goto cleanup_cmyk16;
    }

    rgb_linear = (float *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * 3u * sizeof(float));
    if (rgb_linear == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_cmyk16;
    }

    if (use_cms) {
        cmyk16 = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                    pixel_count * 4u *
                                                    sizeof(uint16_t));
        if (cmyk16 == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_cmyk16;
        }
        for (i = 0u; i < pixel_count; ++i) {
            cmyk16[i * 4u + 0u] = sixel_builtin_psd_decode_cmyk_u16(plane_c[i]);
            cmyk16[i * 4u + 1u] = sixel_builtin_psd_decode_cmyk_u16(plane_m[i]);
            cmyk16[i * 4u + 2u] = sixel_builtin_psd_decode_cmyk_u16(plane_y[i]);
            cmyk16[i * 4u + 3u] = sixel_builtin_psd_decode_cmyk_u16(plane_k[i]);
        }

        src_profile = sixel_cms_open_profile_from_mem(icc_profile,
                                                      icc_profile_length);
        if (src_profile != NULL) {
            dst_profile = sixel_cms_create_linear_srgb_profile();
        }
        if (src_profile != NULL && dst_profile != NULL) {
            transform = sixel_cms_create_transform(
                src_profile,
                SIXEL_CMS_PIXELFORMAT_CMYK_16,
                dst_profile,
                SIXEL_CMS_PIXELFORMAT_RGB_F32,
                SIXEL_CMS_TRANSFORM_DEFAULT);
        }
        if (transform != NULL &&
            sixel_cms_do_transform(transform, cmyk16, rgb_linear, pixel_count)) {
            cms_applied = 1;
        }
    }

    if (!cms_applied) {
        for (i = 0u; i < pixel_count; ++i) {
            c = (float)sixel_builtin_psd_decode_cmyk_u16(plane_c[i]) / 65535.0f;
            m = (float)sixel_builtin_psd_decode_cmyk_u16(plane_m[i]) / 65535.0f;
            y = (float)sixel_builtin_psd_decode_cmyk_u16(plane_y[i]) / 65535.0f;
            k = (float)sixel_builtin_psd_decode_cmyk_u16(plane_k[i]) / 65535.0f;
            rgb_linear[i * 3u + 0u] = (1.0f - c) * (1.0f - k);
            rgb_linear[i * 3u + 1u] = (1.0f - m) * (1.0f - k);
            rgb_linear[i * 3u + 2u] = (1.0f - y) * (1.0f - k);
        }
        if (!sixel_cms_convert_rgbf32_gamma_to_linear(rgb_linear, pixel_count)) {
            status = SIXEL_BAD_INPUT;
            goto cleanup_cmyk16;
        }
    }

    if (plane_alpha != NULL) {
        if (preserve_alpha != 0) {
            for (i = 0u; i < pixel_count; ++i) {
                alpha_f = (float)plane_alpha[i] / 65535.0f;
                rgb_linear[i * 3u + 0u] *= alpha_f;
                rgb_linear[i * 3u + 1u] *= alpha_f;
                rgb_linear[i * 3u + 2u] *= alpha_f;
                transparent_mask[i] = alpha_f <= 0.0f ? 1u : 0u;
            }
        } else {
            status = sixel_builtin_psd_rgb_bgcolor_to_linear(bgcolor, bg_linear);
            if (SIXEL_FAILED(status)) {
                goto cleanup_cmyk16;
            }
            for (i = 0u; i < pixel_count; ++i) {
                alpha_f = (float)plane_alpha[i] / 65535.0f;
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
    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(
        ppixels,
        pwidth,
        pheight,
        ppixelformat,
        (unsigned char *)rgb_linear,
        info,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32);
    status = SIXEL_OK;
    rgb_linear = NULL;

cleanup_cmyk16:
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(dst_profile);
    sixel_cms_close_profile(src_profile);
    sixel_allocator_free(chunk->allocator, cmyk16);
    sixel_allocator_free(chunk->allocator, rgb_linear);
    sixel_allocator_free(chunk->allocator, transparent_mask);
    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_k);
    sixel_allocator_free(chunk->allocator, plane_y);
    sixel_allocator_free(chunk->allocator, plane_m);
    sixel_allocator_free(chunk->allocator, plane_c);
    return status;
}

/* Recover 16-bit Lab pixels from single-layer PSD without merged data. */
static SIXELSTATUS
sixel_builtin_decode_psd_single_layer_missing_composite_lab_16bit(
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
    unsigned char const *buffer;
    size_t section_offset;
    size_t section_end;
    size_t layer_info_length;
    size_t layer_info_offset;
    size_t layer_info_end;
    int16_t layer_count_raw;
    int layer_count;
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
    unsigned int channel_count;
    size_t cursor;
    size_t extra_data_length;
    size_t pixel_count;
    size_t i;
    int l_channel_index;
    int a_channel_index;
    int b_channel_index;
    int alpha_channel_index;
    int preserve_alpha;
    int blend_with_bg;
    float alpha_f;
    float one_minus_alpha;
    float l;
    float a;
    float b;
    float bg_lab[3];
    uint16_t *plane_l;
    uint16_t *plane_a;
    uint16_t *plane_b;
    uint16_t *plane_alpha;
    float *lab;
    unsigned char *transparent_mask;
    sixel_builtin_psd_layer_channel_entry_t
        channels[SIXEL_FROMPSD_MAX_CHANNELS];

    status = SIXEL_FALSE;
    buffer = NULL;
    section_offset = 0u;
    section_end = 0u;
    layer_info_length = 0u;
    layer_info_offset = 0u;
    layer_info_end = 0u;
    layer_count_raw = 0;
    layer_count = 0;
    top = 0;
    left = 0;
    bottom = 0;
    right = 0;
    channel_count = 0u;
    cursor = 0u;
    extra_data_length = 0u;
    pixel_count = 0u;
    i = 0u;
    l_channel_index = -1;
    a_channel_index = -1;
    b_channel_index = -1;
    alpha_channel_index = -1;
    preserve_alpha = 0;
    blend_with_bg = 0;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    l = 0.0f;
    a = 0.0f;
    b = 0.0f;
    bg_lab[0] = 0.0f;
    bg_lab[1] = 0.0f;
    bg_lab[2] = 0.0f;
    plane_l = NULL;
    plane_a = NULL;
    plane_b = NULL;
    plane_alpha = NULL;
    lab = NULL;
    transparent_mask = NULL;
    memset(channels, 0, sizeof(channels));

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL ||
        chunk->buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode != 9u || info->depth != 16u || info->channels < 3u) {
        return SIXEL_BAD_INPUT;
    }
    if (info->image_data_offset < chunk->size) {
        return SIXEL_BAD_INPUT;
    }
    if (info->layer_mask_length < 4u ||
        info->layer_mask_offset > chunk->size ||
        info->layer_mask_length > chunk->size - info->layer_mask_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer info section");
        return SIXEL_STBI_ERROR;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(uint16_t) ||
        pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    buffer = chunk->buffer;
    section_offset = info->layer_mask_offset;
    section_end = section_offset + info->layer_mask_length;
    layer_info_length = sixel_builtin_read_u32be_size(
        buffer + section_offset);
    layer_info_offset = section_offset + 4u;
    if (layer_info_length == 0u ||
        layer_info_length > section_end - layer_info_offset ||
        layer_info_offset + layer_info_length > chunk->size) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record table");
        return SIXEL_STBI_ERROR;
    }
    layer_info_end = layer_info_offset + layer_info_length;

    if (layer_info_offset + 2u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record header");
        return SIXEL_STBI_ERROR;
    }
    layer_count_raw = sixel_builtin_read_i16be(buffer + layer_info_offset);
    layer_info_offset += 2u;
    if (layer_count_raw < 0) {
        layer_count = -(int)layer_count_raw;
    } else {
        layer_count = (int)layer_count_raw;
    }
    if (layer_count != 1) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    if (layer_info_offset + 18u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record geometry");
        return SIXEL_STBI_ERROR;
    }
    top = sixel_builtin_read_i32be(buffer + layer_info_offset);
    left = sixel_builtin_read_i32be(buffer + layer_info_offset + 4u);
    bottom = sixel_builtin_read_i32be(buffer + layer_info_offset + 8u);
    right = sixel_builtin_read_i32be(buffer + layer_info_offset + 12u);
    channel_count = sixel_builtin_read_u16be(
        buffer + layer_info_offset + 16u);
    layer_info_offset += 18u;

    if (channel_count < 3u || channel_count > SIXEL_FROMPSD_MAX_CHANNELS) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback channels");
        return SIXEL_STBI_ERROR;
    }
    if (layer_info_offset > layer_info_end ||
        (size_t)channel_count > (layer_info_end - layer_info_offset) / 6u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel table");
        return SIXEL_STBI_ERROR;
    }

    for (i = 0u; i < (size_t)channel_count; ++i) {
        channels[i].channel_id = sixel_builtin_read_i16be(
            buffer + layer_info_offset);
        channels[i].length = sixel_builtin_read_u32be_size(
            buffer + layer_info_offset + 2u);
        channels[i].data_offset = 0u;
        if (channels[i].length < 2u) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel length");
            return SIXEL_STBI_ERROR;
        }
        if (channels[i].channel_id == 0 && l_channel_index < 0) {
            l_channel_index = (int)i;
        } else if (channels[i].channel_id == 1 && a_channel_index < 0) {
            a_channel_index = (int)i;
        } else if (channels[i].channel_id == 2 && b_channel_index < 0) {
            b_channel_index = (int)i;
        } else if (channels[i].channel_id == -1 && alpha_channel_index < 0) {
            alpha_channel_index = (int)i;
        }
        layer_info_offset += 6u;
    }

    if (layer_info_offset + 16u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer blend block");
        return SIXEL_STBI_ERROR;
    }
    extra_data_length = sixel_builtin_read_u32be_size(
        buffer + layer_info_offset + 12u);
    layer_info_offset += 16u;
    if (extra_data_length > layer_info_end - layer_info_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }
    layer_info_offset += extra_data_length;
    cursor = layer_info_offset;

    for (i = 0u; i < (size_t)channel_count; ++i) {
        if (channels[i].length > layer_info_end - cursor) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel stream");
            return SIXEL_STBI_ERROR;
        }
        channels[i].data_offset = cursor;
        cursor += channels[i].length;
    }
    if (cursor > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel stream");
        return SIXEL_STBI_ERROR;
    }

    if (top != 0 || left != 0 ||
        bottom != (int32_t)info->height ||
        right != (int32_t)info->width ||
        l_channel_index < 0 ||
        a_channel_index < 0 ||
        b_channel_index < 0) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    plane_l = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count *
                                                 sizeof(uint16_t));
    plane_a = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count *
                                                 sizeof(uint16_t));
    plane_b = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count *
                                                 sizeof(uint16_t));
    if (plane_l == NULL || plane_a == NULL || plane_b == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_layer_lab16;
    }

    status = sixel_builtin_psd_decode_layer_plane_rgb16(
        buffer,
        channels,
        l_channel_index,
        info->width,
        info->height,
        plane_l);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_lab16;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb16(
        buffer,
        channels,
        a_channel_index,
        info->width,
        info->height,
        plane_a);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_lab16;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb16(
        buffer,
        channels,
        b_channel_index,
        info->width,
        info->height,
        plane_b);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_lab16;
    }

    preserve_alpha = (bgcolor == NULL && alpha_channel_index >= 0) ? 1 : 0;
    blend_with_bg = (alpha_channel_index >= 0 && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup_layer_lab16;
    }
    if (alpha_channel_index >= 0) {
        plane_alpha = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                         pixel_count *
                                                         sizeof(uint16_t));
        if (plane_alpha == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_layer_lab16;
        }
        status = sixel_builtin_psd_decode_layer_plane_rgb16(
            buffer,
            channels,
            alpha_channel_index,
            info->width,
            info->height,
            plane_alpha);
        if (SIXEL_FAILED(status)) {
            goto cleanup_layer_lab16;
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
            goto cleanup_layer_lab16;
        }
    }
    if (plane_alpha != NULL && preserve_alpha == 0) {
        status = sixel_builtin_psd_rgb_bgcolor_to_cielab(bgcolor, bg_lab);
        if (SIXEL_FAILED(status)) {
            goto cleanup_layer_lab16;
        }
    }

    lab = (float *)sixel_allocator_malloc(chunk->allocator,
                                          pixel_count * 3u * sizeof(float));
    if (lab == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_layer_lab16;
    }

    for (i = 0u; i < pixel_count; ++i) {
        l = sixel_builtin_psd_lab_decode_l16(plane_l[i]);
        a = sixel_builtin_psd_lab_decode_ab16(plane_a[i]);
        b = sixel_builtin_psd_lab_decode_ab16(plane_b[i]);
        if (plane_alpha != NULL) {
            alpha_f = (float)plane_alpha[i] / 65535.0f;
            if (preserve_alpha != 0) {
                l *= alpha_f;
                a *= alpha_f;
                b *= alpha_f;
                if (transparent_mask != NULL) {
                    transparent_mask[i] = alpha_f <= 0.0f ? 1u : 0u;
                }
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        (unsigned char *)lab,
                                        info,
                                        SIXEL_PIXELFORMAT_CIELABFLOAT32);
    status = SIXEL_OK;
    lab = NULL;

cleanup_layer_lab16:
    sixel_allocator_free(chunk->allocator, lab);
    sixel_allocator_free(chunk->allocator, transparent_mask);
    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_a);
    sixel_allocator_free(chunk->allocator, plane_l);
    return status;
}

/* Recover 16-bit RGB/Gray pixels from single-layer PSD without merged data. */
static SIXELSTATUS
sixel_builtin_decode_psd_single_layer_missing_composite_16bit(
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
    unsigned char const *buffer;
    size_t section_offset;
    size_t section_end;
    size_t layer_info_length;
    size_t layer_info_offset;
    size_t layer_info_end;
    int16_t layer_count_raw;
    int layer_count;
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
    unsigned int channel_count;
    size_t cursor;
    size_t extra_data_length;
    size_t pixel_count;
    size_t i;
    unsigned int min_channels;
    int decode_rgb;
    int gray_channel_index;
    int green_channel_index;
    int blue_channel_index;
    int alpha_channel_index;
    int decode_status;
    int preserve_alpha;
    int blend_with_bg;
    float r_f;
    float g_f;
    float b_f;
    float alpha_f;
    float one_minus_alpha;
    float bg_r;
    float bg_g;
    float bg_b;
    uint16_t *plane_r;
    uint16_t *plane_g;
    uint16_t *plane_b;
    uint16_t *plane_alpha;
    float *rgbf32;
    unsigned char *transparent_mask;
    sixel_builtin_psd_layer_channel_entry_t
        channels[SIXEL_FROMPSD_MAX_CHANNELS];

    buffer = NULL;
    section_offset = 0u;
    section_end = 0u;
    layer_info_length = 0u;
    layer_info_offset = 0u;
    layer_info_end = 0u;
    layer_count_raw = 0;
    layer_count = 0;
    top = 0;
    left = 0;
    bottom = 0;
    right = 0;
    channel_count = 0u;
    cursor = 0u;
    extra_data_length = 0u;
    pixel_count = 0u;
    i = 0u;
    min_channels = 0u;
    decode_rgb = 0;
    gray_channel_index = -1;
    green_channel_index = -1;
    blue_channel_index = -1;
    alpha_channel_index = -1;
    decode_status = 0;
    preserve_alpha = 0;
    blend_with_bg = 0;
    r_f = 0.0f;
    g_f = 0.0f;
    b_f = 0.0f;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    bg_r = 0.0f;
    bg_g = 0.0f;
    bg_b = 0.0f;
    plane_r = NULL;
    plane_g = NULL;
    plane_b = NULL;
    plane_alpha = NULL;
    rgbf32 = NULL;
    transparent_mask = NULL;
    memset(channels, 0, sizeof(channels));

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL ||
        chunk->buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (info->color_mode == 9u) {
        return sixel_builtin_decode_psd_single_layer_missing_composite_lab_16bit(
            chunk,
            info,
            bgcolor,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode == 3u ||
        (info->color_mode == 7u && info->channels == 3u)) {
        decode_rgb = 1;
        min_channels = 3u;
    } else if (info->color_mode == 1u || info->color_mode == 8u) {
        decode_rgb = 0;
        min_channels = 1u;
    } else {
        return SIXEL_BAD_INPUT;
    }
    if (info->depth != 16u || info->channels < min_channels) {
        return SIXEL_BAD_INPUT;
    }
    if (info->image_data_offset < chunk->size) {
        return SIXEL_BAD_INPUT;
    }
    if (info->layer_mask_length < 4u ||
        info->layer_mask_offset > chunk->size ||
        info->layer_mask_length > chunk->size - info->layer_mask_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer info section");
        return SIXEL_STBI_ERROR;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(uint16_t) ||
        pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    buffer = chunk->buffer;
    section_offset = info->layer_mask_offset;
    section_end = section_offset + info->layer_mask_length;
    layer_info_length = sixel_builtin_read_u32be_size(
        buffer + section_offset);
    layer_info_offset = section_offset + 4u;
    if (layer_info_length == 0u ||
        layer_info_length > section_end - layer_info_offset ||
        layer_info_offset + layer_info_length > chunk->size) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record table");
        return SIXEL_STBI_ERROR;
    }
    layer_info_end = layer_info_offset + layer_info_length;

    if (layer_info_offset + 2u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record header");
        return SIXEL_STBI_ERROR;
    }
    layer_count_raw = sixel_builtin_read_i16be(buffer + layer_info_offset);
    layer_info_offset += 2u;
    if (layer_count_raw < 0) {
        layer_count = -(int)layer_count_raw;
    } else {
        layer_count = (int)layer_count_raw;
    }
    if (layer_count != 1) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    if (layer_info_offset + 18u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record geometry");
        return SIXEL_STBI_ERROR;
    }
    top = sixel_builtin_read_i32be(buffer + layer_info_offset);
    left = sixel_builtin_read_i32be(buffer + layer_info_offset + 4u);
    bottom = sixel_builtin_read_i32be(buffer + layer_info_offset + 8u);
    right = sixel_builtin_read_i32be(buffer + layer_info_offset + 12u);
    channel_count = sixel_builtin_read_u16be(
        buffer + layer_info_offset + 16u);
    layer_info_offset += 18u;

    if (channel_count < min_channels ||
        channel_count > SIXEL_FROMPSD_MAX_CHANNELS) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback channels");
        return SIXEL_STBI_ERROR;
    }
    if (layer_info_offset > layer_info_end ||
        (size_t)channel_count > (layer_info_end - layer_info_offset) / 6u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel table");
        return SIXEL_STBI_ERROR;
    }

    for (i = 0u; i < (size_t)channel_count; ++i) {
        channels[i].channel_id = sixel_builtin_read_i16be(
            buffer + layer_info_offset);
        channels[i].length = sixel_builtin_read_u32be_size(
            buffer + layer_info_offset + 2u);
        channels[i].data_offset = 0u;
        if (channels[i].length < 2u) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel length");
            return SIXEL_STBI_ERROR;
        }
        if (channels[i].channel_id == 0 && gray_channel_index < 0) {
            gray_channel_index = (int)i;
        } else if (decode_rgb != 0 &&
                   channels[i].channel_id == 1 &&
                   green_channel_index < 0) {
            green_channel_index = (int)i;
        } else if (decode_rgb != 0 &&
                   channels[i].channel_id == 2 &&
                   blue_channel_index < 0) {
            blue_channel_index = (int)i;
        } else if (channels[i].channel_id == -1 && alpha_channel_index < 0) {
            alpha_channel_index = (int)i;
        }
        layer_info_offset += 6u;
    }

    if (layer_info_offset + 16u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer blend block");
        return SIXEL_STBI_ERROR;
    }
    extra_data_length = sixel_builtin_read_u32be_size(
        buffer + layer_info_offset + 12u);
    layer_info_offset += 16u;
    if (extra_data_length > layer_info_end - layer_info_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }
    layer_info_offset += extra_data_length;
    cursor = layer_info_offset;

    for (i = 0u; i < (size_t)channel_count; ++i) {
        if (channels[i].length > layer_info_end - cursor) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel stream");
            return SIXEL_STBI_ERROR;
        }
        channels[i].data_offset = cursor;
        cursor += channels[i].length;
    }
    if (cursor > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel stream");
        return SIXEL_STBI_ERROR;
    }

    if (top != 0 || left != 0 ||
        bottom != (int32_t)info->height ||
        right != (int32_t)info->width ||
        gray_channel_index < 0 ||
        (decode_rgb != 0 &&
         (green_channel_index < 0 || blue_channel_index < 0))) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    plane_r = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count *
                                                 sizeof(uint16_t));
    if (decode_rgb != 0) {
        plane_g = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                     pixel_count *
                                                     sizeof(uint16_t));
        plane_b = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                     pixel_count *
                                                     sizeof(uint16_t));
    }
    if (plane_r == NULL ||
        (decode_rgb != 0 && (plane_g == NULL || plane_b == NULL))) {
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        decode_status = SIXEL_BAD_ALLOCATION;
        goto cleanup_layer16;
    }

    decode_status = sixel_builtin_psd_decode_layer_plane_rgb16(
        buffer,
        channels,
        gray_channel_index,
        info->width,
        info->height,
        plane_r);
    if (decode_status != SIXEL_OK) {
        goto cleanup_layer16;
    }
    if (decode_rgb != 0) {
        decode_status = sixel_builtin_psd_decode_layer_plane_rgb16(
            buffer,
            channels,
            green_channel_index,
            info->width,
            info->height,
            plane_g);
        if (decode_status != SIXEL_OK) {
            goto cleanup_layer16;
        }
        decode_status = sixel_builtin_psd_decode_layer_plane_rgb16(
            buffer,
            channels,
            blue_channel_index,
            info->width,
            info->height,
            plane_b);
        if (decode_status != SIXEL_OK) {
            goto cleanup_layer16;
        }
    }

    preserve_alpha = (bgcolor == NULL && alpha_channel_index >= 0) ? 1 : 0;
    blend_with_bg = (alpha_channel_index >= 0 && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        decode_status = SIXEL_BAD_ARGUMENT;
        goto cleanup_layer16;
    }
    if (alpha_channel_index >= 0) {
        plane_alpha = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                         pixel_count *
                                                         sizeof(uint16_t));
        if (plane_alpha == NULL) {
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            decode_status = SIXEL_BAD_ALLOCATION;
            goto cleanup_layer16;
        }
        decode_status = sixel_builtin_psd_decode_layer_plane_rgb16(
            buffer,
            channels,
            alpha_channel_index,
            info->width,
            info->height,
            plane_alpha);
        if (decode_status != SIXEL_OK) {
            goto cleanup_layer16;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            decode_status = SIXEL_BAD_ALLOCATION;
            goto cleanup_layer16;
        }
    }

    rgbf32 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * 3u * sizeof(float));
    if (rgbf32 == NULL) {
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        decode_status = SIXEL_BAD_ALLOCATION;
        goto cleanup_layer16;
    }

    if (blend_with_bg != 0) {
        bg_r = (float)bgcolor[0] / 255.0f;
        bg_g = (float)bgcolor[1] / 255.0f;
        bg_b = (float)bgcolor[2] / 255.0f;
    }
    for (i = 0u; i < pixel_count; ++i) {
        if (decode_rgb != 0) {
            r_f = (float)plane_r[i] / 65535.0f;
            g_f = (float)plane_g[i] / 65535.0f;
            b_f = (float)plane_b[i] / 65535.0f;
        } else {
            r_f = (float)plane_r[i] / 65535.0f;
            g_f = r_f;
            b_f = r_f;
        }
        if (plane_alpha != NULL) {
            alpha_f = (float)plane_alpha[i] / 65535.0f;
            if (preserve_alpha != 0) {
                r_f *= alpha_f;
                g_f *= alpha_f;
                b_f *= alpha_f;
                if (transparent_mask != NULL) {
                    transparent_mask[i] = plane_alpha[i] == 0u ? 1u : 0u;
                }
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        (unsigned char *)rgbf32,
                                        info,
                                        SIXEL_PIXELFORMAT_RGBFLOAT32);
    rgbf32 = NULL;
    decode_status = SIXEL_OK;

cleanup_layer16:
    sixel_allocator_free(chunk->allocator, rgbf32);
    sixel_allocator_free(chunk->allocator, transparent_mask);
    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_g);
    sixel_allocator_free(chunk->allocator, plane_r);
    return decode_status;
}

/* Recover 32-bit Lab pixels from single-layer PSD without merged data. */
static SIXELSTATUS
sixel_builtin_decode_psd_single_layer_missing_composite_lab_32bit(
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
    unsigned char const *buffer;
    size_t section_offset;
    size_t section_end;
    size_t layer_info_length;
    size_t layer_info_offset;
    size_t layer_info_end;
    int16_t layer_count_raw;
    int layer_count;
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
    unsigned int channel_count;
    size_t cursor;
    size_t extra_data_length;
    size_t pixel_count;
    size_t i;
    int l_channel_index;
    int a_channel_index;
    int b_channel_index;
    int alpha_channel_index;
    int preserve_alpha;
    int blend_with_bg;
    float alpha_f;
    float one_minus_alpha;
    float l;
    float a;
    float b;
    float bg_lab[3];
    float *plane_l;
    float *plane_a;
    float *plane_b;
    float *plane_alpha;
    float *lab;
    unsigned char *transparent_mask;
    sixel_builtin_psd_layer_channel_entry_t
        channels[SIXEL_FROMPSD_MAX_CHANNELS];

    status = SIXEL_FALSE;
    buffer = NULL;
    section_offset = 0u;
    section_end = 0u;
    layer_info_length = 0u;
    layer_info_offset = 0u;
    layer_info_end = 0u;
    layer_count_raw = 0;
    layer_count = 0;
    top = 0;
    left = 0;
    bottom = 0;
    right = 0;
    channel_count = 0u;
    cursor = 0u;
    extra_data_length = 0u;
    pixel_count = 0u;
    i = 0u;
    l_channel_index = -1;
    a_channel_index = -1;
    b_channel_index = -1;
    alpha_channel_index = -1;
    preserve_alpha = 0;
    blend_with_bg = 0;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    l = 0.0f;
    a = 0.0f;
    b = 0.0f;
    bg_lab[0] = 0.0f;
    bg_lab[1] = 0.0f;
    bg_lab[2] = 0.0f;
    plane_l = NULL;
    plane_a = NULL;
    plane_b = NULL;
    plane_alpha = NULL;
    lab = NULL;
    transparent_mask = NULL;
    memset(channels, 0, sizeof(channels));

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL ||
        chunk->buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode != 9u || info->depth != 32u || info->channels < 3u) {
        return SIXEL_BAD_INPUT;
    }
    if (info->image_data_offset < chunk->size) {
        return SIXEL_BAD_INPUT;
    }
    if (info->layer_mask_length < 4u ||
        info->layer_mask_offset > chunk->size ||
        info->layer_mask_length > chunk->size - info->layer_mask_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer info section");
        return SIXEL_STBI_ERROR;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(float) ||
        pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    buffer = chunk->buffer;
    section_offset = info->layer_mask_offset;
    section_end = section_offset + info->layer_mask_length;
    layer_info_length = sixel_builtin_read_u32be_size(
        buffer + section_offset);
    layer_info_offset = section_offset + 4u;
    if (layer_info_length == 0u ||
        layer_info_length > section_end - layer_info_offset ||
        layer_info_offset + layer_info_length > chunk->size) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record table");
        return SIXEL_STBI_ERROR;
    }
    layer_info_end = layer_info_offset + layer_info_length;

    if (layer_info_offset + 2u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record header");
        return SIXEL_STBI_ERROR;
    }
    layer_count_raw = sixel_builtin_read_i16be(buffer + layer_info_offset);
    layer_info_offset += 2u;
    if (layer_count_raw < 0) {
        layer_count = -(int)layer_count_raw;
    } else {
        layer_count = (int)layer_count_raw;
    }
    if (layer_count != 1) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    if (layer_info_offset + 18u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record geometry");
        return SIXEL_STBI_ERROR;
    }
    top = sixel_builtin_read_i32be(buffer + layer_info_offset);
    left = sixel_builtin_read_i32be(buffer + layer_info_offset + 4u);
    bottom = sixel_builtin_read_i32be(buffer + layer_info_offset + 8u);
    right = sixel_builtin_read_i32be(buffer + layer_info_offset + 12u);
    channel_count = sixel_builtin_read_u16be(
        buffer + layer_info_offset + 16u);
    layer_info_offset += 18u;

    if (channel_count < 3u || channel_count > SIXEL_FROMPSD_MAX_CHANNELS) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback channels");
        return SIXEL_STBI_ERROR;
    }
    if (layer_info_offset > layer_info_end ||
        (size_t)channel_count > (layer_info_end - layer_info_offset) / 6u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel table");
        return SIXEL_STBI_ERROR;
    }

    for (i = 0u; i < (size_t)channel_count; ++i) {
        channels[i].channel_id = sixel_builtin_read_i16be(
            buffer + layer_info_offset);
        channels[i].length = sixel_builtin_read_u32be_size(
            buffer + layer_info_offset + 2u);
        channels[i].data_offset = 0u;
        if (channels[i].length < 2u) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel length");
            return SIXEL_STBI_ERROR;
        }
        if (channels[i].channel_id == 0 && l_channel_index < 0) {
            l_channel_index = (int)i;
        } else if (channels[i].channel_id == 1 && a_channel_index < 0) {
            a_channel_index = (int)i;
        } else if (channels[i].channel_id == 2 && b_channel_index < 0) {
            b_channel_index = (int)i;
        } else if (channels[i].channel_id == -1 && alpha_channel_index < 0) {
            alpha_channel_index = (int)i;
        }
        layer_info_offset += 6u;
    }

    if (layer_info_offset + 16u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer blend block");
        return SIXEL_STBI_ERROR;
    }
    extra_data_length = sixel_builtin_read_u32be_size(
        buffer + layer_info_offset + 12u);
    layer_info_offset += 16u;
    if (extra_data_length > layer_info_end - layer_info_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }
    layer_info_offset += extra_data_length;
    cursor = layer_info_offset;

    for (i = 0u; i < (size_t)channel_count; ++i) {
        if (channels[i].length > layer_info_end - cursor) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel stream");
            return SIXEL_STBI_ERROR;
        }
        channels[i].data_offset = cursor;
        cursor += channels[i].length;
    }
    if (cursor > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel stream");
        return SIXEL_STBI_ERROR;
    }

    if (top != 0 || left != 0 ||
        bottom != (int32_t)info->height ||
        right != (int32_t)info->width ||
        l_channel_index < 0 ||
        a_channel_index < 0 ||
        b_channel_index < 0) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
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
        goto cleanup_layer_lab32;
    }

    status = sixel_builtin_psd_decode_layer_plane_rgb32(
        buffer,
        channels,
        l_channel_index,
        info->width,
        info->height,
        plane_l);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_lab32;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb32(
        buffer,
        channels,
        a_channel_index,
        info->width,
        info->height,
        plane_a);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_lab32;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb32(
        buffer,
        channels,
        b_channel_index,
        info->width,
        info->height,
        plane_b);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_lab32;
    }

    preserve_alpha = (bgcolor == NULL && alpha_channel_index >= 0) ? 1 : 0;
    blend_with_bg = (alpha_channel_index >= 0 && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup_layer_lab32;
    }
    if (alpha_channel_index >= 0) {
        plane_alpha = (float *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count *
                                                      sizeof(float));
        if (plane_alpha == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_layer_lab32;
        }
        status = sixel_builtin_psd_decode_layer_plane_rgb32(
            buffer,
            channels,
            alpha_channel_index,
            info->width,
            info->height,
            plane_alpha);
        if (SIXEL_FAILED(status)) {
            goto cleanup_layer_lab32;
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
            goto cleanup_layer_lab32;
        }
    }
    if (plane_alpha != NULL && preserve_alpha == 0) {
        status = sixel_builtin_psd_rgb_bgcolor_to_cielab(bgcolor, bg_lab);
        if (SIXEL_FAILED(status)) {
            goto cleanup_layer_lab32;
        }
    }

    lab = (float *)sixel_allocator_malloc(chunk->allocator,
                                          pixel_count * 3u * sizeof(float));
    if (lab == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_layer_lab32;
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
                if (transparent_mask != NULL) {
                    transparent_mask[i] = alpha_f <= 0.0f ? 1u : 0u;
                }
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        (unsigned char *)lab,
                                        info,
                                        SIXEL_PIXELFORMAT_CIELABFLOAT32);
    status = SIXEL_OK;
    lab = NULL;

cleanup_layer_lab32:
    sixel_allocator_free(chunk->allocator, lab);
    sixel_allocator_free(chunk->allocator, transparent_mask);
    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_a);
    sixel_allocator_free(chunk->allocator, plane_l);
    return status;
}

/* Recover 32-bit RGB/Gray pixels from single-layer PSD without merged data. */
static SIXELSTATUS
sixel_builtin_decode_psd_single_layer_missing_composite_32bit(
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
    unsigned char const *buffer;
    size_t section_offset;
    size_t section_end;
    size_t layer_info_length;
    size_t layer_info_offset;
    size_t layer_info_end;
    int16_t layer_count_raw;
    int layer_count;
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
    unsigned int channel_count;
    size_t cursor;
    size_t extra_data_length;
    size_t pixel_count;
    size_t i;
    unsigned int min_channels;
    int decode_rgb;
    int gray_channel_index;
    int green_channel_index;
    int blue_channel_index;
    int alpha_channel_index;
    int decode_status;
    int preserve_alpha;
    int blend_with_bg;
    float r_f;
    float g_f;
    float b_f;
    float alpha_f;
    float one_minus_alpha;
    float bg_r;
    float bg_g;
    float bg_b;
    float *plane_r;
    float *plane_g;
    float *plane_b;
    float *plane_alpha;
    float *rgbf32;
    unsigned char *transparent_mask;
    sixel_builtin_psd_layer_channel_entry_t
        channels[SIXEL_FROMPSD_MAX_CHANNELS];

    buffer = NULL;
    section_offset = 0u;
    section_end = 0u;
    layer_info_length = 0u;
    layer_info_offset = 0u;
    layer_info_end = 0u;
    layer_count_raw = 0;
    layer_count = 0;
    top = 0;
    left = 0;
    bottom = 0;
    right = 0;
    channel_count = 0u;
    cursor = 0u;
    extra_data_length = 0u;
    pixel_count = 0u;
    i = 0u;
    min_channels = 0u;
    decode_rgb = 0;
    gray_channel_index = -1;
    green_channel_index = -1;
    blue_channel_index = -1;
    alpha_channel_index = -1;
    decode_status = 0;
    preserve_alpha = 0;
    blend_with_bg = 0;
    r_f = 0.0f;
    g_f = 0.0f;
    b_f = 0.0f;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    bg_r = 0.0f;
    bg_g = 0.0f;
    bg_b = 0.0f;
    plane_r = NULL;
    plane_g = NULL;
    plane_b = NULL;
    plane_alpha = NULL;
    rgbf32 = NULL;
    transparent_mask = NULL;
    memset(channels, 0, sizeof(channels));

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL ||
        chunk->buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (info->color_mode == 9u) {
        return sixel_builtin_decode_psd_single_layer_missing_composite_lab_32bit(
            chunk,
            info,
            bgcolor,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode == 3u ||
        (info->color_mode == 7u && info->channels == 3u)) {
        decode_rgb = 1;
        min_channels = 3u;
    } else if (info->color_mode == 1u || info->color_mode == 8u) {
        decode_rgb = 0;
        min_channels = 1u;
    } else {
        return SIXEL_BAD_INPUT;
    }
    if (info->depth != 32u || info->channels < min_channels) {
        return SIXEL_BAD_INPUT;
    }
    if (info->image_data_offset < chunk->size) {
        return SIXEL_BAD_INPUT;
    }
    if (info->layer_mask_length < 4u ||
        info->layer_mask_offset > chunk->size ||
        info->layer_mask_length > chunk->size - info->layer_mask_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer info section");
        return SIXEL_STBI_ERROR;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(float) ||
        pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    buffer = chunk->buffer;
    section_offset = info->layer_mask_offset;
    section_end = section_offset + info->layer_mask_length;
    layer_info_length = sixel_builtin_read_u32be_size(
        buffer + section_offset);
    layer_info_offset = section_offset + 4u;
    if (layer_info_length == 0u ||
        layer_info_length > section_end - layer_info_offset ||
        layer_info_offset + layer_info_length > chunk->size) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record table");
        return SIXEL_STBI_ERROR;
    }
    layer_info_end = layer_info_offset + layer_info_length;

    if (layer_info_offset + 2u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record header");
        return SIXEL_STBI_ERROR;
    }
    layer_count_raw = sixel_builtin_read_i16be(buffer + layer_info_offset);
    layer_info_offset += 2u;
    if (layer_count_raw < 0) {
        layer_count = -(int)layer_count_raw;
    } else {
        layer_count = (int)layer_count_raw;
    }
    if (layer_count != 1) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    if (layer_info_offset + 18u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record geometry");
        return SIXEL_STBI_ERROR;
    }
    top = sixel_builtin_read_i32be(buffer + layer_info_offset);
    left = sixel_builtin_read_i32be(buffer + layer_info_offset + 4u);
    bottom = sixel_builtin_read_i32be(buffer + layer_info_offset + 8u);
    right = sixel_builtin_read_i32be(buffer + layer_info_offset + 12u);
    channel_count = sixel_builtin_read_u16be(
        buffer + layer_info_offset + 16u);
    layer_info_offset += 18u;

    if (channel_count < min_channels ||
        channel_count > SIXEL_FROMPSD_MAX_CHANNELS) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback channels");
        return SIXEL_STBI_ERROR;
    }
    if (layer_info_offset > layer_info_end ||
        (size_t)channel_count > (layer_info_end - layer_info_offset) / 6u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel table");
        return SIXEL_STBI_ERROR;
    }

    for (i = 0u; i < (size_t)channel_count; ++i) {
        channels[i].channel_id = sixel_builtin_read_i16be(
            buffer + layer_info_offset);
        channels[i].length = sixel_builtin_read_u32be_size(
            buffer + layer_info_offset + 2u);
        channels[i].data_offset = 0u;
        if (channels[i].length < 2u) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel length");
            return SIXEL_STBI_ERROR;
        }
        if (channels[i].channel_id == 0 && gray_channel_index < 0) {
            gray_channel_index = (int)i;
        } else if (decode_rgb != 0 &&
                   channels[i].channel_id == 1 &&
                   green_channel_index < 0) {
            green_channel_index = (int)i;
        } else if (decode_rgb != 0 &&
                   channels[i].channel_id == 2 &&
                   blue_channel_index < 0) {
            blue_channel_index = (int)i;
        } else if (channels[i].channel_id == -1 && alpha_channel_index < 0) {
            alpha_channel_index = (int)i;
        }
        layer_info_offset += 6u;
    }

    if (layer_info_offset + 16u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer blend block");
        return SIXEL_STBI_ERROR;
    }
    extra_data_length = sixel_builtin_read_u32be_size(
        buffer + layer_info_offset + 12u);
    layer_info_offset += 16u;
    if (extra_data_length > layer_info_end - layer_info_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }
    layer_info_offset += extra_data_length;
    cursor = layer_info_offset;

    for (i = 0u; i < (size_t)channel_count; ++i) {
        if (channels[i].length > layer_info_end - cursor) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel stream");
            return SIXEL_STBI_ERROR;
        }
        channels[i].data_offset = cursor;
        cursor += channels[i].length;
    }
    if (cursor > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel stream");
        return SIXEL_STBI_ERROR;
    }

    if (top != 0 || left != 0 ||
        bottom != (int32_t)info->height ||
        right != (int32_t)info->width ||
        gray_channel_index < 0 ||
        (decode_rgb != 0 &&
         (green_channel_index < 0 || blue_channel_index < 0))) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    plane_r = (float *)sixel_allocator_malloc(chunk->allocator,
                                              pixel_count * sizeof(float));
    if (decode_rgb != 0) {
        plane_g = (float *)sixel_allocator_malloc(chunk->allocator,
                                                  pixel_count * sizeof(float));
        plane_b = (float *)sixel_allocator_malloc(chunk->allocator,
                                                  pixel_count * sizeof(float));
    }
    if (plane_r == NULL || (decode_rgb != 0 && (plane_g == NULL || plane_b == NULL))) {
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        decode_status = SIXEL_BAD_ALLOCATION;
        goto cleanup_layer32;
    }

    decode_status = sixel_builtin_psd_decode_layer_plane_rgb32(
        buffer,
        channels,
        gray_channel_index,
        info->width,
        info->height,
        plane_r);
    if (decode_status != SIXEL_OK) {
        goto cleanup_layer32;
    }
    if (decode_rgb != 0) {
        decode_status = sixel_builtin_psd_decode_layer_plane_rgb32(
            buffer,
            channels,
            green_channel_index,
            info->width,
            info->height,
            plane_g);
        if (decode_status != SIXEL_OK) {
            goto cleanup_layer32;
        }
        decode_status = sixel_builtin_psd_decode_layer_plane_rgb32(
            buffer,
            channels,
            blue_channel_index,
            info->width,
            info->height,
            plane_b);
        if (decode_status != SIXEL_OK) {
            goto cleanup_layer32;
        }
    }

    preserve_alpha = (bgcolor == NULL && alpha_channel_index >= 0) ? 1 : 0;
    blend_with_bg = (alpha_channel_index >= 0 && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        decode_status = SIXEL_BAD_ARGUMENT;
        goto cleanup_layer32;
    }
    if (alpha_channel_index >= 0) {
        plane_alpha = (float *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count *
                                                      sizeof(float));
        if (plane_alpha == NULL) {
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            decode_status = SIXEL_BAD_ALLOCATION;
            goto cleanup_layer32;
        }
        decode_status = sixel_builtin_psd_decode_layer_plane_rgb32(
            buffer,
            channels,
            alpha_channel_index,
            info->width,
            info->height,
            plane_alpha);
        if (decode_status != SIXEL_OK) {
            goto cleanup_layer32;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            decode_status = SIXEL_BAD_ALLOCATION;
            goto cleanup_layer32;
        }
    }

    rgbf32 = (float *)sixel_allocator_malloc(chunk->allocator,
                                             pixel_count * 3u * sizeof(float));
    if (rgbf32 == NULL) {
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        decode_status = SIXEL_BAD_ALLOCATION;
        goto cleanup_layer32;
    }

    if (blend_with_bg != 0) {
        bg_r = (float)bgcolor[0] / 255.0f;
        bg_g = (float)bgcolor[1] / 255.0f;
        bg_b = (float)bgcolor[2] / 255.0f;
    }
    for (i = 0u; i < pixel_count; ++i) {
        if (decode_rgb != 0) {
            r_f = plane_r[i];
            g_f = plane_g[i];
            b_f = plane_b[i];
        } else {
            r_f = plane_r[i];
            g_f = r_f;
            b_f = r_f;
        }
        if (plane_alpha != NULL) {
            alpha_f = sixel_builtin_psd_clamp_alpha_float32(plane_alpha[i]);
            if (preserve_alpha != 0) {
                r_f *= alpha_f;
                g_f *= alpha_f;
                b_f *= alpha_f;
                if (transparent_mask != NULL) {
                    transparent_mask[i] = alpha_f <= 0.0f ? 1u : 0u;
                }
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        (unsigned char *)rgbf32,
                                        info,
                                        SIXEL_PIXELFORMAT_RGBFLOAT32);
    rgbf32 = NULL;
    decode_status = SIXEL_OK;

cleanup_layer32:
    sixel_allocator_free(chunk->allocator, rgbf32);
    sixel_allocator_free(chunk->allocator, transparent_mask);
    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_g);
    sixel_allocator_free(chunk->allocator, plane_r);
    return decode_status;
}

/* Recover 8-bit Lab pixels from single-layer PSD without merged data. */
static SIXELSTATUS
sixel_builtin_decode_psd_single_layer_missing_composite_lab_8bit(
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
    unsigned char const *buffer;
    size_t section_offset;
    size_t section_end;
    size_t layer_info_length;
    size_t layer_info_offset;
    size_t layer_info_end;
    int16_t layer_count_raw;
    int layer_count;
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
    unsigned int channel_count;
    size_t cursor;
    size_t extra_data_length;
    size_t pixel_count;
    size_t i;
    int l_channel_index;
    int a_channel_index;
    int b_channel_index;
    int alpha_channel_index;
    int preserve_alpha;
    int blend_with_bg;
    int alpha;
    float l;
    float a;
    float b;
    float bg_lab[3];
    unsigned char *plane_l;
    unsigned char *plane_a;
    unsigned char *plane_b;
    unsigned char *plane_alpha;
    float *lab;
    unsigned char *transparent_mask;
    sixel_builtin_psd_layer_channel_entry_t
        channels[SIXEL_FROMPSD_MAX_CHANNELS];

    status = SIXEL_FALSE;
    buffer = NULL;
    section_offset = 0u;
    section_end = 0u;
    layer_info_length = 0u;
    layer_info_offset = 0u;
    layer_info_end = 0u;
    layer_count_raw = 0;
    layer_count = 0;
    top = 0;
    left = 0;
    bottom = 0;
    right = 0;
    channel_count = 0u;
    cursor = 0u;
    extra_data_length = 0u;
    pixel_count = 0u;
    i = 0u;
    l_channel_index = -1;
    a_channel_index = -1;
    b_channel_index = -1;
    alpha_channel_index = -1;
    preserve_alpha = 0;
    blend_with_bg = 0;
    alpha = 0;
    l = 0.0f;
    a = 0.0f;
    b = 0.0f;
    bg_lab[0] = 0.0f;
    bg_lab[1] = 0.0f;
    bg_lab[2] = 0.0f;
    plane_l = NULL;
    plane_a = NULL;
    plane_b = NULL;
    plane_alpha = NULL;
    lab = NULL;
    transparent_mask = NULL;
    memset(channels, 0, sizeof(channels));

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL ||
        chunk->buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode != 9u || info->depth != 8u || info->channels < 3u) {
        return SIXEL_BAD_INPUT;
    }
    if (info->image_data_offset < chunk->size) {
        return SIXEL_BAD_INPUT;
    }
    if (info->layer_mask_length < 4u ||
        info->layer_mask_offset > chunk->size ||
        info->layer_mask_length > chunk->size - info->layer_mask_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer info section");
        return SIXEL_STBI_ERROR;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    buffer = chunk->buffer;
    section_offset = info->layer_mask_offset;
    section_end = section_offset + info->layer_mask_length;
    layer_info_length = sixel_builtin_read_u32be_size(
        buffer + section_offset);
    layer_info_offset = section_offset + 4u;
    if (layer_info_length == 0u ||
        layer_info_length > section_end - layer_info_offset ||
        layer_info_offset + layer_info_length > chunk->size) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record table");
        return SIXEL_STBI_ERROR;
    }
    layer_info_end = layer_info_offset + layer_info_length;

    if (layer_info_offset + 2u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record header");
        return SIXEL_STBI_ERROR;
    }
    layer_count_raw = sixel_builtin_read_i16be(buffer + layer_info_offset);
    layer_info_offset += 2u;
    if (layer_count_raw < 0) {
        layer_count = -(int)layer_count_raw;
    } else {
        layer_count = (int)layer_count_raw;
    }
    if (layer_count != 1) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    if (layer_info_offset + 18u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record geometry");
        return SIXEL_STBI_ERROR;
    }
    top = sixel_builtin_read_i32be(buffer + layer_info_offset);
    left = sixel_builtin_read_i32be(buffer + layer_info_offset + 4u);
    bottom = sixel_builtin_read_i32be(buffer + layer_info_offset + 8u);
    right = sixel_builtin_read_i32be(buffer + layer_info_offset + 12u);
    channel_count = sixel_builtin_read_u16be(
        buffer + layer_info_offset + 16u);
    layer_info_offset += 18u;

    if (channel_count < 3u || channel_count > SIXEL_FROMPSD_MAX_CHANNELS) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback channels");
        return SIXEL_STBI_ERROR;
    }
    if (layer_info_offset > layer_info_end ||
        (size_t)channel_count > (layer_info_end - layer_info_offset) / 6u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel table");
        return SIXEL_STBI_ERROR;
    }

    for (i = 0u; i < (size_t)channel_count; ++i) {
        channels[i].channel_id = sixel_builtin_read_i16be(
            buffer + layer_info_offset);
        channels[i].length = sixel_builtin_read_u32be_size(
            buffer + layer_info_offset + 2u);
        channels[i].data_offset = 0u;
        if (channels[i].length < 2u) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel length");
            return SIXEL_STBI_ERROR;
        }
        if (channels[i].channel_id == 0 && l_channel_index < 0) {
            l_channel_index = (int)i;
        } else if (channels[i].channel_id == 1 && a_channel_index < 0) {
            a_channel_index = (int)i;
        } else if (channels[i].channel_id == 2 && b_channel_index < 0) {
            b_channel_index = (int)i;
        } else if (channels[i].channel_id == -1 && alpha_channel_index < 0) {
            alpha_channel_index = (int)i;
        }
        layer_info_offset += 6u;
    }

    if (layer_info_offset + 16u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer blend block");
        return SIXEL_STBI_ERROR;
    }
    extra_data_length = sixel_builtin_read_u32be_size(
        buffer + layer_info_offset + 12u);
    layer_info_offset += 16u;
    if (extra_data_length > layer_info_end - layer_info_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }
    layer_info_offset += extra_data_length;
    cursor = layer_info_offset;

    for (i = 0u; i < (size_t)channel_count; ++i) {
        if (channels[i].length > layer_info_end - cursor) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel stream");
            return SIXEL_STBI_ERROR;
        }
        channels[i].data_offset = cursor;
        cursor += channels[i].length;
    }
    if (cursor > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel stream");
        return SIXEL_STBI_ERROR;
    }

    if (top != 0 || left != 0 ||
        bottom != (int32_t)info->height ||
        right != (int32_t)info->width ||
        l_channel_index < 0 ||
        a_channel_index < 0 ||
        b_channel_index < 0) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    plane_l = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count);
    plane_a = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count);
    plane_b = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count);
    if (plane_l == NULL || plane_a == NULL || plane_b == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_layer_lab8;
    }

    status = sixel_builtin_psd_decode_layer_plane_rgb8(
        buffer,
        channels,
        l_channel_index,
        info->width,
        info->height,
        plane_l);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_lab8;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb8(
        buffer,
        channels,
        a_channel_index,
        info->width,
        info->height,
        plane_a);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_lab8;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb8(
        buffer,
        channels,
        b_channel_index,
        info->width,
        info->height,
        plane_b);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_lab8;
    }

    preserve_alpha = (bgcolor == NULL && alpha_channel_index >= 0) ? 1 : 0;
    blend_with_bg = (alpha_channel_index >= 0 && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup_layer_lab8;
    }
    if (alpha_channel_index >= 0) {
        plane_alpha = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                              pixel_count);
        if (plane_alpha == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_layer_lab8;
        }
        status = sixel_builtin_psd_decode_layer_plane_rgb8(
            buffer,
            channels,
            alpha_channel_index,
            info->width,
            info->height,
            plane_alpha);
        if (SIXEL_FAILED(status)) {
            goto cleanup_layer_lab8;
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
            goto cleanup_layer_lab8;
        }
    }
    if (plane_alpha != NULL && preserve_alpha == 0) {
        status = sixel_builtin_psd_rgb_bgcolor_to_cielab(bgcolor, bg_lab);
        if (SIXEL_FAILED(status)) {
            goto cleanup_layer_lab8;
        }
    }

    lab = (float *)sixel_allocator_malloc(chunk->allocator,
                                          pixel_count * 3u * sizeof(float));
    if (lab == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_layer_lab8;
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
                if (transparent_mask != NULL) {
                    transparent_mask[i] = alpha == 0 ? 1u : 0u;
                }
            } else {
                l = (l * (float)alpha + bg_lab[0]
                     * (255.0f - (float)alpha)) / 255.0f;
                a = (a * (float)alpha + bg_lab[1]
                     * (255.0f - (float)alpha)) / 255.0f;
                b = (b * (float)alpha + bg_lab[2]
                     * (255.0f - (float)alpha)) / 255.0f;
            }
        }
        lab[i * 3u + 0u] = sixel_builtin_psd_clamp_unit_float32(l);
        lab[i * 3u + 1u] = sixel_builtin_psd_lab_clamp_ab(a);
        lab[i * 3u + 2u] = sixel_builtin_psd_lab_clamp_ab(b);
    }

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        (unsigned char *)lab,
                                        info,
                                        SIXEL_PIXELFORMAT_CIELABFLOAT32);
    status = SIXEL_OK;
    lab = NULL;

cleanup_layer_lab8:
    sixel_allocator_free(chunk->allocator, lab);
    sixel_allocator_free(chunk->allocator, transparent_mask);
    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_a);
    sixel_allocator_free(chunk->allocator, plane_l);
    return status;
}

/* Recover 8-bit RGB/Gray pixels from single-layer PSD without merged data. */
static SIXELSTATUS
sixel_builtin_decode_psd_single_layer_missing_composite_8bit(
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
    unsigned char const *buffer;
    size_t section_offset;
    size_t section_end;
    size_t layer_info_length;
    size_t layer_info_offset;
    size_t layer_info_end;
    int16_t layer_count_raw;
    int layer_count;
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
    unsigned int channel_count;
    size_t cursor;
    size_t extra_data_length;
    size_t pixel_count;
    size_t i;
    unsigned int min_channels;
    int decode_rgb;
    int gray_channel_index;
    int green_channel_index;
    int blue_channel_index;
    int alpha_channel_index;
    int decode_status;
    int preserve_alpha;
    int blend_with_bg;
    int alpha;
    int r;
    int g;
    int b;
    unsigned char *plane_r;
    unsigned char *plane_g;
    unsigned char *plane_b;
    unsigned char *plane_alpha;
    unsigned char *rgb;
    unsigned char *transparent_mask;
    sixel_builtin_psd_layer_channel_entry_t
        channels[SIXEL_FROMPSD_MAX_CHANNELS];

    buffer = NULL;
    section_offset = 0u;
    section_end = 0u;
    layer_info_length = 0u;
    layer_info_offset = 0u;
    layer_info_end = 0u;
    layer_count_raw = 0;
    layer_count = 0;
    top = 0;
    left = 0;
    bottom = 0;
    right = 0;
    channel_count = 0u;
    cursor = 0u;
    extra_data_length = 0u;
    pixel_count = 0u;
    i = 0u;
    min_channels = 0u;
    decode_rgb = 0;
    gray_channel_index = -1;
    green_channel_index = -1;
    blue_channel_index = -1;
    alpha_channel_index = -1;
    decode_status = 0;
    preserve_alpha = 0;
    blend_with_bg = 0;
    alpha = 0;
    r = 0;
    g = 0;
    b = 0;
    plane_r = NULL;
    plane_g = NULL;
    plane_b = NULL;
    plane_alpha = NULL;
    rgb = NULL;
    transparent_mask = NULL;
    memset(channels, 0, sizeof(channels));

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL ||
        chunk->buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (info->color_mode == 9u) {
        return sixel_builtin_decode_psd_single_layer_missing_composite_lab_8bit(
            chunk,
            info,
            bgcolor,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode == 3u ||
        (info->color_mode == 7u && info->channels == 3u)) {
        decode_rgb = 1;
        min_channels = 3u;
    } else if (info->color_mode == 1u || info->color_mode == 8u) {
        decode_rgb = 0;
        min_channels = 1u;
    } else {
        return SIXEL_BAD_INPUT;
    }
    if (info->depth != 8u || info->channels < min_channels) {
        return SIXEL_BAD_INPUT;
    }
    if (info->image_data_offset < chunk->size) {
        return SIXEL_BAD_INPUT;
    }
    if (info->layer_mask_length < 4u ||
        info->layer_mask_offset > chunk->size ||
        info->layer_mask_length > chunk->size - info->layer_mask_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer info section");
        return SIXEL_STBI_ERROR;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    buffer = chunk->buffer;
    section_offset = info->layer_mask_offset;
    section_end = section_offset + info->layer_mask_length;
    layer_info_length = sixel_builtin_read_u32be_size(
        buffer + section_offset);
    layer_info_offset = section_offset + 4u;
    if (layer_info_length == 0u ||
        layer_info_length > section_end - layer_info_offset ||
        layer_info_offset + layer_info_length > chunk->size) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record table");
        return SIXEL_STBI_ERROR;
    }
    layer_info_end = layer_info_offset + layer_info_length;

    if (layer_info_offset + 2u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record header");
        return SIXEL_STBI_ERROR;
    }
    layer_count_raw = sixel_builtin_read_i16be(buffer + layer_info_offset);
    layer_info_offset += 2u;
    if (layer_count_raw < 0) {
        layer_count = -(int)layer_count_raw;
    } else {
        layer_count = (int)layer_count_raw;
    }
    if (layer_count != 1) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    if (layer_info_offset + 18u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record geometry");
        return SIXEL_STBI_ERROR;
    }
    top = sixel_builtin_read_i32be(buffer + layer_info_offset);
    left = sixel_builtin_read_i32be(buffer + layer_info_offset + 4u);
    bottom = sixel_builtin_read_i32be(buffer + layer_info_offset + 8u);
    right = sixel_builtin_read_i32be(buffer + layer_info_offset + 12u);
    channel_count = sixel_builtin_read_u16be(
        buffer + layer_info_offset + 16u);
    layer_info_offset += 18u;

    if (channel_count < min_channels ||
        channel_count > SIXEL_FROMPSD_MAX_CHANNELS) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback channels");
        return SIXEL_STBI_ERROR;
    }
    if (layer_info_offset > layer_info_end ||
        (size_t)channel_count > (layer_info_end - layer_info_offset) / 6u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel table");
        return SIXEL_STBI_ERROR;
    }

    for (i = 0u; i < (size_t)channel_count; ++i) {
        channels[i].channel_id = sixel_builtin_read_i16be(
            buffer + layer_info_offset);
        channels[i].length = sixel_builtin_read_u32be_size(
            buffer + layer_info_offset + 2u);
        channels[i].data_offset = 0u;
        if (channels[i].length < 2u) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel length");
            return SIXEL_STBI_ERROR;
        }
        if (channels[i].channel_id == 0 && gray_channel_index < 0) {
            gray_channel_index = (int)i;
        } else if (decode_rgb != 0 &&
                   channels[i].channel_id == 1 &&
                   green_channel_index < 0) {
            green_channel_index = (int)i;
        } else if (decode_rgb != 0 &&
                   channels[i].channel_id == 2 &&
                   blue_channel_index < 0) {
            blue_channel_index = (int)i;
        } else if (channels[i].channel_id == -1 && alpha_channel_index < 0) {
            alpha_channel_index = (int)i;
        }
        layer_info_offset += 6u;
    }

    if (layer_info_offset + 16u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer blend block");
        return SIXEL_STBI_ERROR;
    }
    extra_data_length = sixel_builtin_read_u32be_size(
        buffer + layer_info_offset + 12u);
    layer_info_offset += 16u;
    if (extra_data_length > layer_info_end - layer_info_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }
    layer_info_offset += extra_data_length;
    cursor = layer_info_offset;

    for (i = 0u; i < (size_t)channel_count; ++i) {
        if (channels[i].length > layer_info_end - cursor) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel stream");
            return SIXEL_STBI_ERROR;
        }
        channels[i].data_offset = cursor;
        cursor += channels[i].length;
    }
    if (cursor > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel stream");
        return SIXEL_STBI_ERROR;
    }

    if (top != 0 || left != 0 ||
        bottom != (int32_t)info->height ||
        right != (int32_t)info->width ||
        gray_channel_index < 0 ||
        (decode_rgb != 0 &&
         (green_channel_index < 0 || blue_channel_index < 0))) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    plane_r = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count);
    if (decode_rgb != 0) {
        plane_g = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                          pixel_count);
        plane_b = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                          pixel_count);
    }
    if (plane_r == NULL || (decode_rgb != 0 && (plane_g == NULL || plane_b == NULL))) {
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        decode_status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    decode_status = sixel_builtin_psd_decode_layer_plane_rgb8(
        buffer,
        channels,
        gray_channel_index,
        info->width,
        info->height,
        plane_r);
    if (decode_status != SIXEL_OK) {
        goto cleanup;
    }
    if (decode_rgb != 0) {
        decode_status = sixel_builtin_psd_decode_layer_plane_rgb8(
            buffer,
            channels,
            green_channel_index,
            info->width,
            info->height,
            plane_g);
        if (decode_status != SIXEL_OK) {
            goto cleanup;
        }
        decode_status = sixel_builtin_psd_decode_layer_plane_rgb8(
            buffer,
            channels,
            blue_channel_index,
            info->width,
            info->height,
            plane_b);
        if (decode_status != SIXEL_OK) {
            goto cleanup;
        }
    }

    preserve_alpha = (bgcolor == NULL && alpha_channel_index >= 0) ? 1 : 0;
    blend_with_bg = (alpha_channel_index >= 0 && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        decode_status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    if (alpha_channel_index >= 0) {
        plane_alpha = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                              pixel_count);
        if (plane_alpha == NULL) {
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            decode_status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        decode_status = sixel_builtin_psd_decode_layer_plane_rgb8(
            buffer,
            channels,
            alpha_channel_index,
            info->width,
            info->height,
            plane_alpha);
        if (decode_status != SIXEL_OK) {
            goto cleanup;
        }
    }
    if (preserve_alpha != 0) {
        transparent_mask = (unsigned char *)sixel_allocator_malloc(
            chunk->allocator,
            pixel_count);
        if (transparent_mask == NULL) {
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            decode_status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
    }

    rgb = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                  pixel_count * 3u);
    if (rgb == NULL) {
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        decode_status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    for (i = 0u; i < pixel_count; ++i) {
        if (decode_rgb != 0) {
            r = (int)plane_r[i];
            g = (int)plane_g[i];
            b = (int)plane_b[i];
        } else {
            r = (int)plane_r[i];
            g = r;
            b = r;
        }
        if (plane_alpha != NULL) {
            alpha = (int)plane_alpha[i];
            if (blend_with_bg != 0) {
                r = (r * alpha + bgcolor[0] * (0xff - alpha)) >> 8;
                g = (g * alpha + bgcolor[1] * (0xff - alpha)) >> 8;
                b = (b * alpha + bgcolor[2] * (0xff - alpha)) >> 8;
            } else {
                /*
                 * SIXEL supports key transparency only. Keep alpha==0 in
                 * the mask and pre-multiply color for semi-transparent
                 * pixels.
                 */
                r = (r * alpha) >> 8;
                g = (g * alpha) >> 8;
                b = (b * alpha) >> 8;
                if (transparent_mask != NULL) {
                    transparent_mask[i] = alpha == 0 ? 1u : 0u;
                }
            }
        }
        rgb[i * 3u + 0u] = (unsigned char)r;
        rgb[i * 3u + 1u] = (unsigned char)g;
        rgb[i * 3u + 2u] = (unsigned char)b;
    }

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        rgb,
                                        info,
                                        SIXEL_PIXELFORMAT_RGB888);
    rgb = NULL;
    decode_status = SIXEL_OK;

cleanup:
    sixel_allocator_free(chunk->allocator, rgb);
    sixel_allocator_free(chunk->allocator, transparent_mask);
    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_g);
    sixel_allocator_free(chunk->allocator, plane_r);
    return decode_status;
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
    SIXELSTATUS layer_status;

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
    layer_status = SIXEL_FALSE;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if ((info->color_mode != 3u && info->color_mode != 7u) ||
        info->depth != 8u ||
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
    if (info->image_data_offset >= chunk->size) {
        if (sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
            layer_status = sixel_builtin_decode_psd_multilayer_missing_composite(
                chunk,
                info,
                bgcolor,
                NULL,
                0u,
                NULL,
                SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_RGB888,
                ppixels,
                ptransparent_mask,
                ptransparent_mask_size,
                pwidth,
                pheight,
                ppixelformat);
            if (layer_status != SIXEL_BAD_INPUT) {
                return layer_status;
            }
        }
        return sixel_builtin_decode_psd_single_layer_missing_composite_8bit(
            chunk,
            info,
            bgcolor,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
    }

    plane_r = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count);
    plane_g = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count);
    plane_b = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count);
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        rgb,
                                        info,
                                        SIXEL_PIXELFORMAT_RGB888);
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
    int blend_with_bg;
    float r_f;
    float g_f;
    float b_f;
    float alpha_f;
    float one_minus_alpha;
    float bg_r;
    float bg_g;
    float bg_b;
    SIXELSTATUS layer_status;

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
    blend_with_bg = 0;
    r_f = 0.0f;
    g_f = 0.0f;
    b_f = 0.0f;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    bg_r = 0.0f;
    bg_g = 0.0f;
    bg_b = 0.0f;
    layer_status = SIXEL_FALSE;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if ((info->color_mode != 3u && info->color_mode != 7u) ||
        info->depth != 16u ||
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
    if (info->image_data_offset >= chunk->size) {
        if (sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
            layer_status = sixel_builtin_decode_psd_multilayer_missing_composite(
                chunk,
                info,
                bgcolor,
                NULL,
                0u,
                NULL,
                SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_RGBFLOAT32,
                ppixels,
                ptransparent_mask,
                ptransparent_mask_size,
                pwidth,
                pheight,
                ppixelformat);
            if (layer_status != SIXEL_BAD_INPUT) {
                return layer_status;
            }
        }
        return sixel_builtin_decode_psd_single_layer_missing_composite_16bit(
            chunk,
            info,
            bgcolor,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
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
    blend_with_bg = (want_alpha != 0 && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        return SIXEL_BAD_ARGUMENT;
    }
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

    if (blend_with_bg != 0) {
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        (unsigned char *)rgbf32,
                                        info,
                                        SIXEL_PIXELFORMAT_RGBFLOAT32);
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
    int blend_with_bg;
    float alpha_f;
    float one_minus_alpha;
    float bg_r;
    float bg_g;
    float bg_b;
    SIXELSTATUS layer_status;

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
    blend_with_bg = 0;
    alpha_f = 0.0f;
    one_minus_alpha = 0.0f;
    bg_r = 0.0f;
    bg_g = 0.0f;
    bg_b = 0.0f;
    layer_status = SIXEL_FALSE;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if ((info->color_mode != 3u && info->color_mode != 7u) ||
        info->depth != 32u ||
        info->compression > 3u ||
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
    if (info->image_data_offset >= chunk->size) {
        if (sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
            layer_status = sixel_builtin_decode_psd_multilayer_missing_composite(
                chunk,
                info,
                bgcolor,
                NULL,
                0u,
                NULL,
                SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_RGBFLOAT32,
                ppixels,
                ptransparent_mask,
                ptransparent_mask_size,
                pwidth,
                pheight,
                ppixelformat);
            if (layer_status != SIXEL_BAD_INPUT) {
                return layer_status;
            }
        }
        return sixel_builtin_decode_psd_single_layer_missing_composite_32bit(
            chunk,
            info,
            bgcolor,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
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
    blend_with_bg = (want_alpha != 0 && preserve_alpha == 0) ? 1 : 0;
    if (blend_with_bg != 0 && bgcolor == NULL) {
        sixel_allocator_free(chunk->allocator, plane_b);
        sixel_allocator_free(chunk->allocator, plane_g);
        sixel_allocator_free(chunk->allocator, plane_r);
        return SIXEL_BAD_ARGUMENT;
    }
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

    if (blend_with_bg != 0) {
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        (unsigned char *)rgbf32,
                                        info,
                                        SIXEL_PIXELFORMAT_RGBFLOAT32);
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
sixel_builtin_psd_lab_decode_l16(uint16_t value)
{
    return sixel_builtin_psd_clamp_unit_float32((float)value / 65535.0f);
}

static float
sixel_builtin_psd_lab_decode_ab16(uint16_t value)
{
    float normalized;

    normalized = ((float)value - 32768.0f) / 32768.0f;
    return sixel_builtin_psd_lab_clamp_ab(normalized);
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
    SIXELSTATUS layer_status;

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
    layer_status = SIXEL_FALSE;

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
    if (info->image_data_offset >= chunk->size) {
        if (sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
            layer_status = sixel_builtin_decode_psd_multilayer_missing_composite(
                chunk,
                info,
                bgcolor,
                NULL,
                0u,
                NULL,
                SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_CIELABFLOAT32,
                ppixels,
                ptransparent_mask,
                ptransparent_mask_size,
                pwidth,
                pheight,
                ppixelformat);
            if (layer_status != SIXEL_BAD_INPUT) {
                return layer_status;
            }
        }
        return sixel_builtin_decode_psd_single_layer_missing_composite_8bit(
            chunk,
            info,
            bgcolor,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        (unsigned char *)lab,
                                        info,
                                        SIXEL_PIXELFORMAT_CIELABFLOAT32);
    return SIXEL_OK;
}

SIXELSTATUS
sixel_builtin_decode_psd_lab_16bit(
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
    uint16_t *plane_l;
    uint16_t *plane_a;
    uint16_t *plane_b;
    uint16_t *plane_alpha;
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
    SIXELSTATUS layer_status;

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
    layer_status = SIXEL_FALSE;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode != 9u || info->depth != 16u ||
        info->compression > 3u || info->channels < 3u) {
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(uint16_t) ||
        pixel_count > SIZE_MAX / (3u * sizeof(uint16_t)) ||
        pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if (info->image_data_offset >= chunk->size) {
        if (sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
            layer_status = sixel_builtin_decode_psd_multilayer_missing_composite(
                chunk,
                info,
                bgcolor,
                NULL,
                0u,
                NULL,
                SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_CIELABFLOAT32,
                ppixels,
                ptransparent_mask,
                ptransparent_mask_size,
                pwidth,
                pheight,
                ppixelformat);
            if (layer_status != SIXEL_BAD_INPUT) {
                return layer_status;
            }
        }
        return sixel_builtin_decode_psd_single_layer_missing_composite_16bit(
            chunk,
            info,
            bgcolor,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
    }

    plane_l = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count *
                                                 sizeof(uint16_t));
    plane_a = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count *
                                                 sizeof(uint16_t));
    plane_b = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count *
                                                 sizeof(uint16_t));
    if (plane_l == NULL || plane_a == NULL || plane_b == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_lab16;
    }

    preserve_alpha = (bgcolor == NULL && info->channels >= 4u) ? 1 : 0;
    want_alpha = info->channels >= 4u ? 1 : 0;
    if (want_alpha) {
        plane_alpha = (uint16_t *)sixel_allocator_malloc(chunk->allocator,
                                                         pixel_count *
                                                         sizeof(uint16_t));
        if (plane_alpha == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_lab16;
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
            goto cleanup_lab16;
        }
    }

    if (!sixel_builtin_decode_psd_16bit_channel(chunk, info, 0u, plane_l) ||
        !sixel_builtin_decode_psd_16bit_channel(chunk, info, 1u, plane_a) ||
        !sixel_builtin_decode_psd_16bit_channel(chunk, info, 2u, plane_b) ||
        (plane_alpha != NULL &&
         !sixel_builtin_decode_psd_16bit_channel(chunk,
                                                 info,
                                                 3u,
                                                 plane_alpha))) {
        status = SIXEL_STBI_ERROR;
        sixel_helper_set_additional_message(
            "builtin PSD: malformed compressed channel stream");
        goto cleanup_lab16;
    }

    if (plane_alpha != NULL && preserve_alpha == 0) {
        status = sixel_builtin_psd_rgb_bgcolor_to_cielab(bgcolor, bg_lab);
        if (SIXEL_FAILED(status)) {
            goto cleanup_lab16;
        }
    }

    lab = (float *)sixel_allocator_malloc(chunk->allocator,
                                          pixel_count * 3u * sizeof(float));
    if (lab == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_lab16;
    }

    for (i = 0u; i < pixel_count; ++i) {
        l = sixel_builtin_psd_lab_decode_l16(plane_l[i]);
        a = sixel_builtin_psd_lab_decode_ab16(plane_a[i]);
        b = sixel_builtin_psd_lab_decode_ab16(plane_b[i]);
        if (plane_alpha != NULL) {
            alpha_f = (float)plane_alpha[i] / 65535.0f;
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        (unsigned char *)lab,
                                        info,
                                        SIXEL_PIXELFORMAT_CIELABFLOAT32);
    status = SIXEL_OK;
    lab = NULL;

cleanup_lab16:
    sixel_allocator_free(chunk->allocator, lab);
    sixel_allocator_free(chunk->allocator, transparent_mask);
    sixel_allocator_free(chunk->allocator, plane_alpha);
    sixel_allocator_free(chunk->allocator, plane_b);
    sixel_allocator_free(chunk->allocator, plane_a);
    sixel_allocator_free(chunk->allocator, plane_l);
    return status;
}

SIXELSTATUS
sixel_builtin_decode_psd_single_layer_missing_composite_cmyk_32bit(
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
    unsigned char const *buffer;
    size_t section_offset;
    size_t section_end;
    size_t layer_info_length;
    size_t layer_info_offset;
    size_t layer_info_end;
    int16_t layer_count_raw;
    int layer_count;
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
    unsigned int channel_count;
    size_t cursor;
    size_t extra_data_length;
    size_t pixel_count;
    size_t i;
    int c_channel_index;
    int m_channel_index;
    int y_channel_index;
    int k_channel_index;
    int alpha_channel_index;
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
    sixel_builtin_psd_layer_channel_entry_t
        channels[SIXEL_FROMPSD_MAX_CHANNELS];

    status = SIXEL_FALSE;
    buffer = NULL;
    section_offset = 0u;
    section_end = 0u;
    layer_info_length = 0u;
    layer_info_offset = 0u;
    layer_info_end = 0u;
    layer_count_raw = 0;
    layer_count = 0;
    top = 0;
    left = 0;
    bottom = 0;
    right = 0;
    channel_count = 0u;
    cursor = 0u;
    extra_data_length = 0u;
    pixel_count = 0u;
    i = 0u;
    c_channel_index = -1;
    m_channel_index = -1;
    y_channel_index = -1;
    k_channel_index = -1;
    alpha_channel_index = -1;
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
    memset(channels, 0, sizeof(channels));

    if (chunk == NULL || info == NULL || pcms_applied == NULL ||
        ppixels == NULL || pwidth == NULL || pheight == NULL ||
        ppixelformat == NULL || chunk->allocator == NULL ||
        chunk->buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *pcms_applied = 0;
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if ((info->color_mode != 4u &&
         info->color_mode != 7u) ||
        info->depth != 32u ||
        info->channels < 4u) {
        return SIXEL_BAD_INPUT;
    }
    if (info->image_data_offset < chunk->size) {
        return SIXEL_BAD_INPUT;
    }
    if (info->layer_mask_length < 4u ||
        info->layer_mask_offset > chunk->size ||
        info->layer_mask_length > chunk->size - info->layer_mask_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer info section");
        return SIXEL_STBI_ERROR;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)info->width * (size_t)info->height;
    if (pixel_count > SIZE_MAX / sizeof(float) ||
        pixel_count > SIZE_MAX / (4u * sizeof(float)) ||
        pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    use_cms = (icc_profile != NULL && icc_profile_length > 0u) ? 1 : 0;

    buffer = chunk->buffer;
    section_offset = info->layer_mask_offset;
    section_end = section_offset + info->layer_mask_length;
    layer_info_length = sixel_builtin_read_u32be_size(buffer + section_offset);
    layer_info_offset = section_offset + 4u;
    if (layer_info_length == 0u ||
        layer_info_length > section_end - layer_info_offset ||
        layer_info_offset + layer_info_length > chunk->size) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record table");
        return SIXEL_STBI_ERROR;
    }
    layer_info_end = layer_info_offset + layer_info_length;

    if (layer_info_offset + 2u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record header");
        return SIXEL_STBI_ERROR;
    }
    layer_count_raw = sixel_builtin_read_i16be(buffer + layer_info_offset);
    layer_info_offset += 2u;
    if (layer_count_raw < 0) {
        layer_count = -(int)layer_count_raw;
    } else {
        layer_count = (int)layer_count_raw;
    }
    if (layer_count != 1) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

    if (layer_info_offset + 18u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer record geometry");
        return SIXEL_STBI_ERROR;
    }
    top = sixel_builtin_read_i32be(buffer + layer_info_offset);
    left = sixel_builtin_read_i32be(buffer + layer_info_offset + 4u);
    bottom = sixel_builtin_read_i32be(buffer + layer_info_offset + 8u);
    right = sixel_builtin_read_i32be(buffer + layer_info_offset + 12u);
    channel_count = sixel_builtin_read_u16be(buffer + layer_info_offset + 16u);
    layer_info_offset += 18u;

    if (channel_count < 4u || channel_count > SIXEL_FROMPSD_MAX_CHANNELS) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback channels");
        return SIXEL_STBI_ERROR;
    }
    if (layer_info_offset > layer_info_end ||
        (size_t)channel_count > (layer_info_end - layer_info_offset) / 6u) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel table");
        return SIXEL_STBI_ERROR;
    }
    for (i = 0u; i < (size_t)channel_count; ++i) {
        channels[i].channel_id = sixel_builtin_read_i16be(
            buffer + layer_info_offset);
        channels[i].length = sixel_builtin_read_u32be_size(
            buffer + layer_info_offset + 2u);
        channels[i].data_offset = 0u;
        if (channels[i].length < 2u) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel length");
            return SIXEL_STBI_ERROR;
        }
        if (channels[i].channel_id == 0 && c_channel_index < 0) {
            c_channel_index = (int)i;
        } else if (channels[i].channel_id == 1 && m_channel_index < 0) {
            m_channel_index = (int)i;
        } else if (channels[i].channel_id == 2 && y_channel_index < 0) {
            y_channel_index = (int)i;
        } else if (channels[i].channel_id == 3 && k_channel_index < 0) {
            k_channel_index = (int)i;
        } else if (channels[i].channel_id == -1 && alpha_channel_index < 0) {
            alpha_channel_index = (int)i;
        }
        layer_info_offset += 6u;
    }

    if (layer_info_offset + 16u > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer blend block");
        return SIXEL_STBI_ERROR;
    }
    extra_data_length = sixel_builtin_read_u32be_size(
        buffer + layer_info_offset + 12u);
    layer_info_offset += 16u;
    if (extra_data_length > layer_info_end - layer_info_offset) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer extra data");
        return SIXEL_STBI_ERROR;
    }
    layer_info_offset += extra_data_length;
    cursor = layer_info_offset;

    for (i = 0u; i < (size_t)channel_count; ++i) {
        if (channels[i].length > layer_info_end - cursor) {
            sixel_helper_set_additional_message(
                "builtin PSD: malformed layer channel stream");
            return SIXEL_STBI_ERROR;
        }
        channels[i].data_offset = cursor;
        cursor += channels[i].length;
    }
    if (cursor > layer_info_end) {
        sixel_helper_set_additional_message(
            "builtin PSD: malformed layer channel stream");
        return SIXEL_STBI_ERROR;
    }

    if (top != 0 || left != 0 ||
        bottom != (int32_t)info->height ||
        right != (int32_t)info->width ||
        c_channel_index < 0 || m_channel_index < 0 ||
        y_channel_index < 0 || k_channel_index < 0) {
        sixel_helper_set_additional_message(
            "builtin PSD: unsupported layer fallback layout");
        return SIXEL_STBI_ERROR;
    }

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
        goto cleanup_layer_cmyk32;
    }

    status = sixel_builtin_psd_decode_layer_plane_rgb32(buffer,
                                                        channels,
                                                        c_channel_index,
                                                        info->width,
                                                        info->height,
                                                        plane_c);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_cmyk32;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb32(buffer,
                                                        channels,
                                                        m_channel_index,
                                                        info->width,
                                                        info->height,
                                                        plane_m);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_cmyk32;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb32(buffer,
                                                        channels,
                                                        y_channel_index,
                                                        info->width,
                                                        info->height,
                                                        plane_y);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_cmyk32;
    }
    status = sixel_builtin_psd_decode_layer_plane_rgb32(buffer,
                                                        channels,
                                                        k_channel_index,
                                                        info->width,
                                                        info->height,
                                                        plane_k);
    if (SIXEL_FAILED(status)) {
        goto cleanup_layer_cmyk32;
    }

    preserve_alpha = (bgcolor == NULL && alpha_channel_index >= 0) ? 1 : 0;
    if (alpha_channel_index >= 0) {
        plane_alpha = (float *)sixel_allocator_malloc(chunk->allocator,
                                                      pixel_count *
                                                      sizeof(float));
        if (plane_alpha == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_layer_cmyk32;
        }
        status = sixel_builtin_psd_decode_layer_plane_rgb32(buffer,
                                                            channels,
                                                            alpha_channel_index,
                                                            info->width,
                                                            info->height,
                                                            plane_alpha);
        if (SIXEL_FAILED(status)) {
            goto cleanup_layer_cmyk32;
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
            goto cleanup_layer_cmyk32;
        }
    }

    rgb_linear = (float *)sixel_allocator_malloc(chunk->allocator,
                                                 pixel_count * 3u * sizeof(float));
    if (rgb_linear == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin PSD: sixel_allocator_malloc() failed.");
        goto cleanup_layer_cmyk32;
    }

    if (use_cms) {
        cmyk = (float *)sixel_allocator_malloc(chunk->allocator,
                                               pixel_count * 4u *
                                               sizeof(float));
        if (cmyk == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            sixel_helper_set_additional_message(
                "builtin PSD: sixel_allocator_malloc() failed.");
            goto cleanup_layer_cmyk32;
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
            goto cleanup_layer_cmyk32;
        }
    }

    if (plane_alpha != NULL) {
        if (preserve_alpha != 0) {
            for (i = 0u; i < pixel_count; ++i) {
                alpha_f = sixel_builtin_psd_clamp_alpha_float32(plane_alpha[i]);
                rgb_linear[i * 3u + 0u] *= alpha_f;
                rgb_linear[i * 3u + 1u] *= alpha_f;
                rgb_linear[i * 3u + 2u] *= alpha_f;
                if (transparent_mask != NULL) {
                    transparent_mask[i] = alpha_f <= 0.0f ? 1u : 0u;
                }
            }
        } else {
            status = sixel_builtin_psd_rgb_bgcolor_to_linear(bgcolor, bg_linear);
            if (SIXEL_FAILED(status)) {
                goto cleanup_layer_cmyk32;
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
    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(
        ppixels,
        pwidth,
        pheight,
        ppixelformat,
        (unsigned char *)rgb_linear,
        info,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32);
    status = SIXEL_OK;
    rgb_linear = NULL;

cleanup_layer_cmyk32:
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
    if ((info->color_mode != 4u && info->color_mode != 7u) ||
        info->depth != 32u ||
        info->compression > 3u ||
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
    if ((info->color_mode == 4u ||
         (info->color_mode == 7u &&
          info->channels == 4u)) &&
        info->channels >= 4u &&
        info->image_data_offset >= chunk->size) {
        if (sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
            status = sixel_builtin_decode_psd_multilayer_missing_composite(
                chunk,
                info,
                bgcolor,
                icc_profile,
                icc_profile_length,
                pcms_applied,
                SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_LINEARRGBFLOAT32,
                ppixels,
                ptransparent_mask,
                ptransparent_mask_size,
                pwidth,
                pheight,
                ppixelformat);
            if (status != SIXEL_BAD_INPUT) {
                return status;
            }
        }
        return sixel_builtin_decode_psd_single_layer_missing_composite_cmyk_32bit(
            chunk,
            info,
            bgcolor,
            icc_profile,
            icc_profile_length,
            pcms_applied,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
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
    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(
        ppixels,
        pwidth,
        pheight,
        ppixelformat,
        (unsigned char *)rgb_linear,
        info,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32);
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
    SIXELSTATUS layer_status;

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
    layer_status = SIXEL_FALSE;

    if (chunk == NULL || info == NULL || ppixels == NULL || pwidth == NULL ||
        pheight == NULL || ppixelformat == NULL || chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_builtin_psd_init_transparent_mask_output(
        ptransparent_mask,
        ptransparent_mask_size);
    if (info->color_mode != 9u || info->depth != 32u ||
        info->compression > 3u ||
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
    if (info->image_data_offset >= chunk->size) {
        if (sixel_builtin_psd_should_try_multilayer_fallback(chunk, info)) {
            layer_status = sixel_builtin_decode_psd_multilayer_missing_composite(
                chunk,
                info,
                bgcolor,
                NULL,
                0u,
                NULL,
                SIXEL_BUILTIN_PSD_MULTILAYER_OUTPUT_CIELABFLOAT32,
                ppixels,
                ptransparent_mask,
                ptransparent_mask_size,
                pwidth,
                pheight,
                ppixelformat);
            if (layer_status != SIXEL_BAD_INPUT) {
                return layer_status;
            }
        }
        return sixel_builtin_decode_psd_single_layer_missing_composite_32bit(
            chunk,
            info,
            bgcolor,
            ppixels,
            ptransparent_mask,
            ptransparent_mask_size,
            pwidth,
            pheight,
            ppixelformat);
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

    sixel_builtin_psd_commit_transparent_mask_output(
        chunk->allocator,
        ptransparent_mask,
        ptransparent_mask_size,
        &transparent_mask,
        pixel_count,
        preserve_alpha);
    sixel_builtin_psd_set_decode_output(ppixels,
                                        pwidth,
                                        pheight,
                                        ppixelformat,
                                        (unsigned char *)lab,
                                        info,
                                        SIXEL_PIXELFORMAT_CIELABFLOAT32);
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
