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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

/* STDC_HEADERS */
#include <limits.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#include <sixel.h>

#include "allocator.h"
#include "chunk.h"
#include "frombmp.h"

#define SIXEL_BMP_DIB_CORE      12u
#define SIXEL_BMP_DIB_INFO      40u
#define SIXEL_BMP_DIB_V3        56u
#define SIXEL_BMP_DIB_V4        108u
#define SIXEL_BMP_DIB_V5        124u
#define SIXEL_BMP_FILE_HEADER_SIZE 14u

#define SIXEL_BMP_COMPRESSION_RGB       SIXEL_FROMBMP_COMPRESSION_RGB
#define SIXEL_BMP_COMPRESSION_RLE8      SIXEL_FROMBMP_COMPRESSION_RLE8
#define SIXEL_BMP_COMPRESSION_RLE4      SIXEL_FROMBMP_COMPRESSION_RLE4
#define SIXEL_BMP_COMPRESSION_BITFIELDS SIXEL_FROMBMP_COMPRESSION_BITFIELDS
#define SIXEL_BMP_COMPRESSION_JPEG      SIXEL_FROMBMP_COMPRESSION_JPEG
#define SIXEL_BMP_COMPRESSION_PNG       SIXEL_FROMBMP_COMPRESSION_PNG
#define SIXEL_BMP_COMPRESSION_ALPHABITFIELDS \
    SIXEL_FROMBMP_COMPRESSION_ALPHABITFIELDS
#define SIXEL_BMP_COMPRESSION_CMYK      SIXEL_FROMBMP_COMPRESSION_CMYK

#define SIXEL_BMP_MAX_PALETTE 256u

#define SIXEL_BMP_V5_CSTYPE_OFFSET        56u
#define SIXEL_BMP_V4_ENDPOINTS_OFFSET     60u
#define SIXEL_BMP_V4_GAMMA_RED_OFFSET     96u
#define SIXEL_BMP_V4_GAMMA_GREEN_OFFSET  100u
#define SIXEL_BMP_V4_GAMMA_BLUE_OFFSET   104u
#define SIXEL_BMP_V5_PROFILE_DATA_OFFSET 112u
#define SIXEL_BMP_V5_PROFILE_SIZE_OFFSET 116u

typedef struct sixel_bmp_decode_info {
    sixel_chunk_t const *chunk;
    int width;
    int height;
    int top_down;
    int bpp;
    int is_cmyk;
    unsigned int compression;
    unsigned int red_mask;
    unsigned int green_mask;
    unsigned int blue_mask;
    unsigned int alpha_mask;
    int has_alpha_mask;
    int palette_count;
    unsigned char palette[SIXEL_BMP_MAX_PALETTE][4];
    size_t pixel_offset;
    size_t payload_size;
    size_t row_stride;
    unsigned char const *payload;
    unsigned char const *icc_profile;
    size_t icc_profile_length;
    int has_calibrated_rgb;
    double calibrated_gamma;
    double white_x;
    double white_y;
    double red_x;
    double red_y;
    double green_x;
    double green_y;
    double blue_x;
    double blue_y;
} sixel_bmp_decode_info_t;

static SIXELSTATUS
sixel_bmp_fail(char const *message)
{
    sixel_helper_set_additional_message(message);
    return SIXEL_STBI_ERROR;
}

static int
sixel_bmp_read_u16le(unsigned char const *buffer,
                     size_t size,
                     size_t offset,
                     unsigned int *value)
{
    if (buffer == NULL || value == NULL ||
        offset > size || size - offset < 2u) {
        return 0;
    }
    *value = (unsigned int)buffer[offset]
        | ((unsigned int)buffer[offset + 1u] << 8u);
    return 1;
}

static int
sixel_bmp_read_u32le(unsigned char const *buffer,
                     size_t size,
                     size_t offset,
                     unsigned int *value)
{
    if (buffer == NULL || value == NULL ||
        offset > size || size - offset < 4u) {
        return 0;
    }
    *value = (unsigned int)buffer[offset]
        | ((unsigned int)buffer[offset + 1u] << 8u)
        | ((unsigned int)buffer[offset + 2u] << 16u)
        | ((unsigned int)buffer[offset + 3u] << 24u);
    return 1;
}

static int
sixel_bmp_read_s32le(unsigned char const *buffer,
                     size_t size,
                     size_t offset,
                     int *value)
{
    unsigned int u32;

    u32 = 0u;
    if (value == NULL) {
        return 0;
    }
    if (!sixel_bmp_read_u32le(buffer, size, offset, &u32)) {
        return 0;
    }
    /* Keep the two's-complement bit pattern when decoding signed fields. */
    *value = (int)(int32_t)u32;
    return 1;
}

static int
sixel_bmp_read_fxpt2dot30(unsigned char const *buffer,
                          size_t size,
                          size_t offset,
                          double *value)
{
    int signed_value;

    signed_value = 0;
    if (value == NULL) {
        return 0;
    }
    if (!sixel_bmp_read_s32le(buffer, size, offset, &signed_value)) {
        return 0;
    }
    *value = (double)signed_value / 1073741824.0;
    return 1;
}

static int
sixel_bmp_read_u16dot16(unsigned char const *buffer,
                        size_t size,
                        size_t offset,
                        double *value)
{
    unsigned int u32_value;

    u32_value = 0u;
    if (value == NULL) {
        return 0;
    }
    if (!sixel_bmp_read_u32le(buffer, size, offset, &u32_value)) {
        return 0;
    }
    *value = (double)u32_value / 65536.0;
    return 1;
}

static int
sixel_bmp_xyz_to_xy(double x,
                    double y,
                    double z,
                    double *x_out,
                    double *y_out)
{
    double sum;

    sum = 0.0;
    if (x_out == NULL || y_out == NULL) {
        return 0;
    }

    sum = x + y + z;
    if (sum <= 0.0) {
        return 0;
    }

    *x_out = x / sum;
    *y_out = y / sum;
    if (*x_out <= 0.0 || *x_out >= 1.0 ||
        *y_out <= 0.0 || *y_out >= 1.0 ||
        *x_out + *y_out >= 1.0) {
        return 0;
    }
    return 1;
}

static int
sixel_bmp_high_bit(unsigned int value)
{
    int bit;

    bit = -1;
    while (value != 0u) {
        value >>= 1u;
        bit++;
    }
    return bit;
}

static int
sixel_bmp_bitcount(unsigned int value)
{
    int count;

    count = 0;
    while (value != 0u) {
        count += (int)(value & 1u);
        value >>= 1u;
    }
    return count;
}

static int
sixel_bmp_shift_signed(unsigned int value, int shift, int bits)
{
    static unsigned int const mul_table[9] = {
        0u,
        0xffu, 0x55u, 0x49u, 0x11u,
        0x21u, 0x41u, 0x81u, 0x01u
    };
    static unsigned int const shift_table[9] = {
        0u, 0u, 0u, 1u, 0u, 2u, 4u, 6u, 0u
    };

    if (bits < 0 || bits > 8) {
        return 0;
    }
    if (shift < 0) {
        value <<= (unsigned int)(-shift);
    } else {
        value >>= (unsigned int)shift;
    }
    value >>= (unsigned int)(8 - bits);
    return (int)((value * mul_table[bits]) >> shift_table[bits]);
}

static void
sixel_bmp_set_default_masks(sixel_bmp_decode_info_t *info)
{
    if (info == NULL) {
        return;
    }
    if (info->bpp == 16) {
        info->red_mask = 31u << 10u;
        info->green_mask = 31u << 5u;
        info->blue_mask = 31u;
        info->alpha_mask = 0u;
    } else if (info->bpp == 32) {
        info->red_mask = 0xffu << 16u;
        info->green_mask = 0xffu << 8u;
        info->blue_mask = 0xffu;
        info->alpha_mask = 0xffu << 24u;
    } else {
        info->red_mask = 0u;
        info->green_mask = 0u;
        info->blue_mask = 0u;
        info->alpha_mask = 0u;
    }
}

static int
sixel_bmp_is_compressed_payload(unsigned int compression)
{
    return compression == SIXEL_BMP_COMPRESSION_JPEG ||
        compression == SIXEL_BMP_COMPRESSION_PNG;
}

static int
sixel_bmp_uses_palette(unsigned int compression, unsigned int bpp)
{
    if (compression == SIXEL_BMP_COMPRESSION_RGB ||
        compression == SIXEL_BMP_COMPRESSION_RLE8 ||
        compression == SIXEL_BMP_COMPRESSION_RLE4) {
        return bpp < 16u;
    }
    return 0;
}

static SIXELSTATUS
sixel_bmp_validate_compressed_payload(sixel_bmp_decode_info_t const *info)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    unsigned char const *payload;
    size_t payload_size;

    payload = NULL;
    payload_size = 0u;
    if (info == NULL || info->payload == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    payload = info->payload;
    payload_size = info->payload_size;
    if (payload_size == 0u) {
        return sixel_bmp_fail("builtin BMP: compressed payload is empty");
    }

    if (info->compression == SIXEL_BMP_COMPRESSION_JPEG) {
        if (payload_size < 2u ||
            payload[0] != 0xffu ||
            payload[1] != 0xd8u) {
            return sixel_bmp_fail("builtin BMP: invalid JPEG payload");
        }
        return SIXEL_OK;
    }
    if (info->compression == SIXEL_BMP_COMPRESSION_PNG) {
        if (payload_size < sizeof(png_signature) ||
            memcmp(payload, png_signature, sizeof(png_signature)) != 0) {
            return sixel_bmp_fail("builtin BMP: invalid PNG payload");
        }
        return SIXEL_OK;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_bmp_compute_row_stride(int width, int bpp, size_t *row_stride)
{
    size_t bits_per_row;
    size_t bytes_per_row;

    bits_per_row = 0u;
    bytes_per_row = 0u;
    if (row_stride == NULL || width <= 0 || bpp <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)width > SIZE_MAX / (size_t)bpp) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    bits_per_row = (size_t)width * (size_t)bpp;
    bytes_per_row = (bits_per_row + 7u) >> 3u;
    if (bytes_per_row > SIZE_MAX - 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    *row_stride = (bytes_per_row + 3u) & ~(size_t)3u;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_bmp_parse_palette(sixel_bmp_decode_info_t *info,
                        size_t palette_offset,
                        size_t entry_size,
                        size_t palette_count)
{
    unsigned char const *buffer;
    size_t size;
    size_t index;
    unsigned char b;
    unsigned char g;
    unsigned char r;

    buffer = NULL;
    size = 0u;
    index = 0u;
    b = 0u;
    g = 0u;
    r = 0u;
    if (info == NULL || info->chunk == NULL || info->chunk->buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (entry_size != 3u && entry_size != 4u) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (palette_count == 0u || palette_count > SIXEL_BMP_MAX_PALETTE) {
        return sixel_bmp_fail("builtin BMP: invalid palette size");
    }

    buffer = info->chunk->buffer;
    size = info->chunk->size;
    if (palette_offset > size ||
        palette_count > (size - palette_offset) / entry_size) {
        return sixel_bmp_fail("builtin BMP: truncated palette");
    }

    for (index = 0u; index < palette_count; ++index) {
        b = buffer[palette_offset + index * entry_size + 0u];
        g = buffer[palette_offset + index * entry_size + 1u];
        r = buffer[palette_offset + index * entry_size + 2u];
        info->palette[index][0] = r;
        info->palette[index][1] = g;
        info->palette[index][2] = b;
        info->palette[index][3] = 0xffu;
    }

    info->palette_count = (int)palette_count;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_bmp_parse_calibrated_v4v5(
    sixel_bmp_decode_info_t *info,
    unsigned char const *buffer,
    size_t size)
{
    static unsigned char const calibrated_signature[4] = {
        0x00u, 0x00u, 0x00u, 0x00u
    };
    size_t dib_offset;
    unsigned char const *cs_type;
    double red_x;
    double red_y;
    double red_z;
    double green_x;
    double green_y;
    double green_z;
    double blue_x;
    double blue_y;
    double blue_z;
    double white_xyz_x;
    double white_xyz_y;
    double white_xyz_z;
    double red_xy_x;
    double red_xy_y;
    double green_xy_x;
    double green_xy_y;
    double blue_xy_x;
    double blue_xy_y;
    double white_xy_x;
    double white_xy_y;
    double gamma_r;
    double gamma_g;
    double gamma_b;
    double gamma_avg;

    dib_offset = 0u;
    cs_type = NULL;
    red_x = 0.0;
    red_y = 0.0;
    red_z = 0.0;
    green_x = 0.0;
    green_y = 0.0;
    green_z = 0.0;
    blue_x = 0.0;
    blue_y = 0.0;
    blue_z = 0.0;
    white_xyz_x = 0.0;
    white_xyz_y = 0.0;
    white_xyz_z = 0.0;
    red_xy_x = 0.0;
    red_xy_y = 0.0;
    green_xy_x = 0.0;
    green_xy_y = 0.0;
    blue_xy_x = 0.0;
    blue_xy_y = 0.0;
    white_xy_x = 0.0;
    white_xy_y = 0.0;
    gamma_r = 0.0;
    gamma_g = 0.0;
    gamma_b = 0.0;
    gamma_avg = 0.0;
    if (info == NULL || buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    dib_offset = SIXEL_BMP_FILE_HEADER_SIZE;
    cs_type = buffer + dib_offset + SIXEL_BMP_V5_CSTYPE_OFFSET;
    if (memcmp(cs_type, calibrated_signature, 4u) != 0) {
        return SIXEL_OK;
    }

    if (!sixel_bmp_read_fxpt2dot30(
            buffer,
            size,
            dib_offset + SIXEL_BMP_V4_ENDPOINTS_OFFSET + 0u,
            &red_x) ||
        !sixel_bmp_read_fxpt2dot30(
            buffer,
            size,
            dib_offset + SIXEL_BMP_V4_ENDPOINTS_OFFSET + 4u,
            &red_y) ||
        !sixel_bmp_read_fxpt2dot30(
            buffer,
            size,
            dib_offset + SIXEL_BMP_V4_ENDPOINTS_OFFSET + 8u,
            &red_z) ||
        !sixel_bmp_read_fxpt2dot30(
            buffer,
            size,
            dib_offset + SIXEL_BMP_V4_ENDPOINTS_OFFSET + 12u,
            &green_x) ||
        !sixel_bmp_read_fxpt2dot30(
            buffer,
            size,
            dib_offset + SIXEL_BMP_V4_ENDPOINTS_OFFSET + 16u,
            &green_y) ||
        !sixel_bmp_read_fxpt2dot30(
            buffer,
            size,
            dib_offset + SIXEL_BMP_V4_ENDPOINTS_OFFSET + 20u,
            &green_z) ||
        !sixel_bmp_read_fxpt2dot30(
            buffer,
            size,
            dib_offset + SIXEL_BMP_V4_ENDPOINTS_OFFSET + 24u,
            &blue_x) ||
        !sixel_bmp_read_fxpt2dot30(
            buffer,
            size,
            dib_offset + SIXEL_BMP_V4_ENDPOINTS_OFFSET + 28u,
            &blue_y) ||
        !sixel_bmp_read_fxpt2dot30(
            buffer,
            size,
            dib_offset + SIXEL_BMP_V4_ENDPOINTS_OFFSET + 32u,
            &blue_z) ||
        !sixel_bmp_read_u16dot16(
            buffer,
            size,
            dib_offset + SIXEL_BMP_V4_GAMMA_RED_OFFSET,
            &gamma_r) ||
        !sixel_bmp_read_u16dot16(
            buffer,
            size,
            dib_offset + SIXEL_BMP_V4_GAMMA_GREEN_OFFSET,
            &gamma_g) ||
        !sixel_bmp_read_u16dot16(
            buffer,
            size,
            dib_offset + SIXEL_BMP_V4_GAMMA_BLUE_OFFSET,
            &gamma_b)) {
        return sixel_bmp_fail("builtin BMP: malformed calibrated RGB fields");
    }

    white_xyz_x = red_x + green_x + blue_x;
    white_xyz_y = red_y + green_y + blue_y;
    white_xyz_z = red_z + green_z + blue_z;
    if (!sixel_bmp_xyz_to_xy(red_x, red_y, red_z, &red_xy_x, &red_xy_y) ||
        !sixel_bmp_xyz_to_xy(green_x,
                             green_y,
                             green_z,
                             &green_xy_x,
                             &green_xy_y) ||
        !sixel_bmp_xyz_to_xy(blue_x,
                             blue_y,
                             blue_z,
                             &blue_xy_x,
                             &blue_xy_y) ||
        !sixel_bmp_xyz_to_xy(white_xyz_x,
                             white_xyz_y,
                             white_xyz_z,
                             &white_xy_x,
                             &white_xy_y)) {
        return SIXEL_OK;
    }
    if (gamma_r <= 0.0 || gamma_g <= 0.0 || gamma_b <= 0.0) {
        return SIXEL_OK;
    }

    /*
     * cms.c profile synthesis takes a single gamma value for RGB.
     * Use the channel average when calibrated gamma is available.
     */
    gamma_avg = (gamma_r + gamma_g + gamma_b) / 3.0;
    if (gamma_avg <= 0.0) {
        return SIXEL_OK;
    }

    info->has_calibrated_rgb = 1;
    info->calibrated_gamma = gamma_avg;
    info->white_x = white_xy_x;
    info->white_y = white_xy_y;
    info->red_x = red_xy_x;
    info->red_y = red_xy_y;
    info->green_x = green_xy_x;
    info->green_y = green_xy_y;
    info->blue_x = blue_xy_x;
    info->blue_y = blue_xy_y;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_bmp_parse_v5_embedded_profile(
    sixel_bmp_decode_info_t *info,
    unsigned char const *buffer,
    size_t size)
{
    size_t dib_offset;
    unsigned char const *cs_type;
    unsigned int profile_data_u32;
    unsigned int profile_size_u32;
    size_t profile_offset;
    size_t profile_size;

    dib_offset = 0u;
    cs_type = NULL;
    profile_data_u32 = 0u;
    profile_size_u32 = 0u;
    profile_offset = 0u;
    profile_size = 0u;
    if (info == NULL || buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    dib_offset = SIXEL_BMP_FILE_HEADER_SIZE;
    cs_type = buffer + dib_offset + SIXEL_BMP_V5_CSTYPE_OFFSET;
    if (memcmp(cs_type, "MBED", 4u) != 0) {
        /*
         * PROFILE_LINKED and other CSType values are not interpreted in the
         * builtin BMP path. Decode pixels and continue without ICC.
         */
        return SIXEL_OK;
    }

    if (!sixel_bmp_read_u32le(buffer,
                              size,
                              dib_offset + SIXEL_BMP_V5_PROFILE_DATA_OFFSET,
                              &profile_data_u32) ||
        !sixel_bmp_read_u32le(buffer,
                              size,
                              dib_offset + SIXEL_BMP_V5_PROFILE_SIZE_OFFSET,
                              &profile_size_u32)) {
        return sixel_bmp_fail("builtin BMP: malformed V5 profile fields");
    }
    if (profile_size_u32 == 0u) {
        return sixel_bmp_fail("builtin BMP: invalid embedded ICC size");
    }

    /*
     * BITMAPV5HEADER stores profile offsets from the beginning of the DIB
     * header in file-backed BMP streams.
     */
    profile_offset = dib_offset + (size_t)profile_data_u32;
    profile_size = (size_t)profile_size_u32;
    if (profile_offset < dib_offset ||
        profile_offset > size ||
        profile_size > size - profile_offset) {
        return sixel_bmp_fail("builtin BMP: embedded ICC range is invalid");
    }

    info->icc_profile = buffer + profile_offset;
    info->icc_profile_length = profile_size;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_bmp_parse_header(sixel_chunk_t const *chunk,
                       sixel_bmp_decode_info_t *info)
{
    unsigned char const *buffer;
    size_t size;
    unsigned int u16_value;
    unsigned int dib_size;
    unsigned int pixel_offset_u32;
    unsigned int planes;
    unsigned int bpp;
    unsigned int compression;
    unsigned int image_size_u32;
    unsigned int colors_used;
    int width_s32;
    int height_s32;
    size_t palette_offset;
    size_t palette_count;
    size_t palette_entry_size;
    size_t max_palette_entries;
    size_t mask_offset;
    size_t payload_size;
    SIXELSTATUS status;

    buffer = NULL;
    size = 0u;
    u16_value = 0u;
    dib_size = 0u;
    pixel_offset_u32 = 0u;
    planes = 0u;
    bpp = 0u;
    compression = 0u;
    image_size_u32 = 0u;
    colors_used = 0u;
    width_s32 = 0;
    height_s32 = 0;
    palette_offset = 0u;
    palette_count = 0u;
    palette_entry_size = 0u;
    max_palette_entries = 0u;
    mask_offset = 0u;
    payload_size = 0u;
    status = SIXEL_OK;
    if (chunk == NULL || info == NULL || chunk->buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /* Parse only fields required for pixel reconstruction. */
    memset(info, 0, sizeof(*info));
    info->chunk = chunk;
    buffer = chunk->buffer;
    size = chunk->size;
    if (size < 14u) {
        return sixel_bmp_fail("builtin BMP: file header is truncated");
    }
    if (buffer[0] != 'B' || buffer[1] != 'M') {
        return sixel_bmp_fail("builtin BMP: invalid file signature");
    }

    if (!sixel_bmp_read_u32le(buffer, size, 10u, &pixel_offset_u32) ||
        !sixel_bmp_read_u32le(buffer, size, 14u, &dib_size)) {
        return sixel_bmp_fail("builtin BMP: malformed header fields");
    }
    if (dib_size != SIXEL_BMP_DIB_CORE &&
        dib_size != SIXEL_BMP_DIB_INFO &&
        dib_size != SIXEL_BMP_DIB_V3 &&
        dib_size != SIXEL_BMP_DIB_V4 &&
        dib_size != SIXEL_BMP_DIB_V5) {
        return sixel_bmp_fail("builtin BMP: unsupported DIB header size");
    }
    if ((size_t)pixel_offset_u32 >= size) {
        return sixel_bmp_fail("builtin BMP: invalid pixel offset");
    }
    if (14u > size - (size_t)dib_size) {
        return sixel_bmp_fail("builtin BMP: truncated DIB header");
    }

    if (dib_size == SIXEL_BMP_DIB_CORE) {
        if (!sixel_bmp_read_u16le(buffer, size, 18u, &u16_value)) {
            return sixel_bmp_fail("builtin BMP: malformed CORE width");
        }
        info->width = (int)u16_value;
        if (!sixel_bmp_read_u16le(buffer, size, 20u, &u16_value)) {
            return sixel_bmp_fail("builtin BMP: malformed CORE height");
        }
        info->height = (int)u16_value;
        if (!sixel_bmp_read_u16le(buffer, size, 22u, &planes) ||
            !sixel_bmp_read_u16le(buffer, size, 24u, &bpp)) {
            return sixel_bmp_fail("builtin BMP: malformed CORE fields");
        }
        compression = SIXEL_BMP_COMPRESSION_RGB;
        image_size_u32 = 0u;
        colors_used = 0u;
        info->top_down = 0;
        palette_entry_size = 3u;
    } else {
        if (!sixel_bmp_read_s32le(buffer, size, 18u, &width_s32) ||
            !sixel_bmp_read_s32le(buffer, size, 22u, &height_s32)) {
            return sixel_bmp_fail("builtin BMP: malformed INFO dimensions");
        }
        if (!sixel_bmp_read_u16le(buffer, size, 26u, &planes) ||
            !sixel_bmp_read_u16le(buffer, size, 28u, &bpp) ||
            !sixel_bmp_read_u32le(buffer, size, 30u, &compression) ||
            !sixel_bmp_read_u32le(buffer, size, 34u, &image_size_u32) ||
            !sixel_bmp_read_u32le(buffer, size, 46u, &colors_used)) {
            return sixel_bmp_fail("builtin BMP: malformed INFO fields");
        }
        if (height_s32 == INT_MIN) {
            return sixel_bmp_fail("builtin BMP: invalid signed height");
        }
        info->top_down = height_s32 < 0 ? 1 : 0;
        info->width = width_s32;
        info->height = height_s32 < 0 ? -height_s32 : height_s32;
        palette_entry_size = 4u;
    }

    if (info->width <= 0 || info->height <= 0) {
        return sixel_bmp_fail("builtin BMP: invalid image dimensions");
    }
    if (planes != 1u) {
        return sixel_bmp_fail("builtin BMP: unsupported plane count");
    }
    if (compression != SIXEL_BMP_COMPRESSION_RGB &&
        compression != SIXEL_BMP_COMPRESSION_RLE8 &&
        compression != SIXEL_BMP_COMPRESSION_RLE4 &&
        compression != SIXEL_BMP_COMPRESSION_BITFIELDS &&
        compression != SIXEL_BMP_COMPRESSION_ALPHABITFIELDS &&
        compression != SIXEL_BMP_COMPRESSION_JPEG &&
        compression != SIXEL_BMP_COMPRESSION_PNG &&
        compression != SIXEL_BMP_COMPRESSION_CMYK) {
        return sixel_bmp_fail("builtin BMP: unsupported compression mode");
    }
    if (!sixel_bmp_is_compressed_payload(compression) &&
        bpp != 1u && bpp != 4u && bpp != 8u &&
        bpp != 16u && bpp != 24u && bpp != 32u) {
        return sixel_bmp_fail("builtin BMP: unsupported bit depth");
    }
    if (compression == SIXEL_BMP_COMPRESSION_RLE8 && bpp != 8u) {
        return sixel_bmp_fail("builtin BMP: RLE8 requires 8bpp");
    }
    if (compression == SIXEL_BMP_COMPRESSION_RLE4 && bpp != 4u) {
        return sixel_bmp_fail("builtin BMP: RLE4 requires 4bpp");
    }
    if (compression == SIXEL_BMP_COMPRESSION_BITFIELDS &&
        bpp != 16u && bpp != 32u) {
        return sixel_bmp_fail(
            "builtin BMP: BI_BITFIELDS requires 16/32bpp");
    }
    if (compression == SIXEL_BMP_COMPRESSION_ALPHABITFIELDS &&
        bpp != 16u && bpp != 32u) {
        return sixel_bmp_fail(
            "builtin BMP: BI_ALPHABITFIELDS requires 16/32bpp");
    }
    if (compression == SIXEL_BMP_COMPRESSION_CMYK &&
        bpp != 32u) {
        return sixel_bmp_fail("builtin BMP: BI_CMYK requires 32bpp");
    }

    info->bpp = (int)bpp;
    info->is_cmyk = compression == SIXEL_BMP_COMPRESSION_CMYK ? 1 : 0;
    info->compression = compression;
    info->pixel_offset = (size_t)pixel_offset_u32;
    info->payload = buffer + info->pixel_offset;
    if (sixel_bmp_is_compressed_payload(compression)) {
        if (image_size_u32 != 0u) {
            payload_size = (size_t)image_size_u32;
            if (payload_size > size - info->pixel_offset) {
                return sixel_bmp_fail(
                    "builtin BMP: compressed payload range is invalid");
            }
        } else {
            payload_size = size - info->pixel_offset;
        }
        info->payload_size = payload_size;
        status = sixel_bmp_validate_compressed_payload(info);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    } else {
        info->payload_size = 0u;
    }

    sixel_bmp_set_default_masks(info);
    if (!sixel_bmp_is_compressed_payload(info->compression) &&
        info->compression != SIXEL_BMP_COMPRESSION_CMYK &&
        (info->bpp == 16 || info->bpp == 32)) {
        if (dib_size == SIXEL_BMP_DIB_INFO) {
            if (compression == SIXEL_BMP_COMPRESSION_BITFIELDS ||
                compression == SIXEL_BMP_COMPRESSION_ALPHABITFIELDS) {
                mask_offset = 14u + SIXEL_BMP_DIB_INFO;
                if (!sixel_bmp_read_u32le(buffer, size, mask_offset + 0u,
                                          &info->red_mask) ||
                    !sixel_bmp_read_u32le(buffer, size, mask_offset + 4u,
                                          &info->green_mask) ||
                    !sixel_bmp_read_u32le(buffer, size, mask_offset + 8u,
                                          &info->blue_mask)) {
                    return sixel_bmp_fail("builtin BMP: truncated masks");
                }
                if (compression == SIXEL_BMP_COMPRESSION_ALPHABITFIELDS) {
                    if (!sixel_bmp_read_u32le(buffer,
                                              size,
                                              mask_offset + 12u,
                                              &info->alpha_mask)) {
                        return sixel_bmp_fail("builtin BMP: truncated masks");
                    }
                } else {
                    info->alpha_mask = 0u;
                    if ((size_t)pixel_offset_u32 >= mask_offset + 16u &&
                        size >= mask_offset + 16u) {
                        (void)sixel_bmp_read_u32le(buffer,
                                                   size,
                                                   mask_offset + 12u,
                                                   &info->alpha_mask);
                    }
                }
            }
        } else if (dib_size == SIXEL_BMP_DIB_V3 ||
                   dib_size == SIXEL_BMP_DIB_V4 ||
                   dib_size == SIXEL_BMP_DIB_V5) {
            mask_offset = 14u + SIXEL_BMP_DIB_INFO;
            if (!sixel_bmp_read_u32le(buffer, size, mask_offset + 0u,
                                      &info->red_mask) ||
                !sixel_bmp_read_u32le(buffer, size, mask_offset + 4u,
                                      &info->green_mask) ||
                !sixel_bmp_read_u32le(buffer, size, mask_offset + 8u,
                                      &info->blue_mask) ||
                !sixel_bmp_read_u32le(buffer, size, mask_offset + 12u,
                                      &info->alpha_mask)) {
                return sixel_bmp_fail("builtin BMP: malformed mask fields");
            }
            if (compression != SIXEL_BMP_COMPRESSION_BITFIELDS &&
                compression != SIXEL_BMP_COMPRESSION_ALPHABITFIELDS) {
                sixel_bmp_set_default_masks(info);
            }
        }
        if (info->red_mask == 0u ||
            info->green_mask == 0u ||
            info->blue_mask == 0u) {
            return sixel_bmp_fail("builtin BMP: invalid color masks");
        }
        if (compression == SIXEL_BMP_COMPRESSION_ALPHABITFIELDS &&
            info->alpha_mask == 0u) {
            return sixel_bmp_fail("builtin BMP: invalid alpha mask");
        }
        info->has_alpha_mask = info->alpha_mask != 0u ? 1 : 0;
    }

    if (sixel_bmp_uses_palette(info->compression, bpp)) {
        palette_offset = 14u + (size_t)dib_size;
        if (palette_offset > (size_t)pixel_offset_u32) {
            return sixel_bmp_fail("builtin BMP: invalid palette offset");
        }
        if (colors_used != 0u) {
            palette_count = (size_t)colors_used;
        } else {
            palette_count = ((size_t)pixel_offset_u32 - palette_offset)
                / palette_entry_size;
            if (palette_count == 0u) {
                palette_count = (size_t)1u << bpp;
            }
        }
        max_palette_entries = (size_t)1u << bpp;
        if (palette_count > max_palette_entries) {
            palette_count = max_palette_entries;
        }
        status = sixel_bmp_parse_palette(info,
                                         palette_offset,
                                         palette_entry_size,
                                         palette_count);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    if (info->compression == SIXEL_BMP_COMPRESSION_RGB ||
        info->compression == SIXEL_BMP_COMPRESSION_BITFIELDS ||
        info->compression == SIXEL_BMP_COMPRESSION_ALPHABITFIELDS ||
        info->compression == SIXEL_BMP_COMPRESSION_CMYK) {
        status = sixel_bmp_compute_row_stride(info->width,
                                              info->bpp,
                                              &info->row_stride);
        if (SIXEL_FAILED(status)) {
            return sixel_bmp_fail("builtin BMP: invalid row stride");
        }
        if ((size_t)info->height > SIZE_MAX / info->row_stride) {
            return sixel_bmp_fail("builtin BMP: image size overflows");
        }
        if (info->pixel_offset > size ||
            info->row_stride * (size_t)info->height
                > size - info->pixel_offset) {
            return sixel_bmp_fail("builtin BMP: truncated pixel data");
        }
    }

    if (dib_size == SIXEL_BMP_DIB_V4 || dib_size == SIXEL_BMP_DIB_V5) {
        status = sixel_bmp_parse_calibrated_v4v5(info, buffer, size);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }
    if (dib_size == SIXEL_BMP_DIB_V5) {
        status = sixel_bmp_parse_v5_embedded_profile(info, buffer, size);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return SIXEL_OK;
}

static int
sixel_bmp_put_index(sixel_bmp_decode_info_t const *info,
                    unsigned char *indices,
                    int *x,
                    int y,
                    unsigned char index)
{
    size_t offset;

    offset = 0u;
    if (info == NULL || indices == NULL || x == NULL) {
        return 0;
    }
    if (y < 0 || y >= info->height || *x < 0 || *x >= info->width) {
        return 0;
    }
    if ((size_t)info->width != 0u &&
        (size_t)y > SIZE_MAX / (size_t)info->width) {
        return 0;
    }
    offset = (size_t)y * (size_t)info->width + (size_t)(*x);
    indices[offset] = index;
    *x += 1;
    return 1;
}

static SIXELSTATUS
sixel_bmp_decode_rle8_indices(sixel_bmp_decode_info_t const *info,
                              unsigned char *indices)
{
    unsigned char const *buffer;
    size_t size;
    size_t offset;
    int x;
    int y;
    int step;
    unsigned int count;
    unsigned int code;
    unsigned int index;
    unsigned int dx;
    unsigned int dy;

    buffer = NULL;
    size = 0u;
    offset = 0u;
    x = 0;
    y = 0;
    step = 0;
    count = 0u;
    code = 0u;
    index = 0u;
    dx = 0u;
    dy = 0u;
    if (info == NULL || indices == NULL || info->chunk == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    buffer = info->chunk->buffer;
    size = info->chunk->size;
    offset = info->pixel_offset;
    x = 0;
    y = info->top_down ? 0 : info->height - 1;
    /*
     * RLE stream coordinates move downward for top-down BMP,
     * upward otherwise.
     */
    step = info->top_down ? 1 : -1;

    while (offset < size) {
        if (size - offset < 2u) {
            return sixel_bmp_fail("builtin BMP: truncated RLE8 stream");
        }
        count = (unsigned int)buffer[offset++];
        code = (unsigned int)buffer[offset++];

        if (count != 0u) {
            for (index = 0u; index < count; ++index) {
                if (code >= (unsigned int)info->palette_count ||
                    !sixel_bmp_put_index(info,
                                         indices,
                                         &x,
                                         y,
                                         (unsigned char)code)) {
                    return sixel_bmp_fail("builtin BMP: RLE8 write overflow");
                }
            }
            continue;
        }

        if (code == 0u) {
            x = 0;
            y += step;
            continue;
        }
        if (code == 1u) {
            return SIXEL_OK;
        }
        if (code == 2u) {
            if (size - offset < 2u) {
                return sixel_bmp_fail("builtin BMP: truncated RLE8 delta");
            }
            dx = (unsigned int)buffer[offset++];
            dy = (unsigned int)buffer[offset++];
            x += (int)dx;
            if (step > 0) {
                y += (int)dy;
            } else {
                y -= (int)dy;
            }
            if (x < 0 || x >= info->width || y < 0 || y >= info->height) {
                return sixel_bmp_fail("builtin BMP: invalid RLE8 delta");
            }
            continue;
        }

        if ((size_t)code > size - offset) {
            return sixel_bmp_fail("builtin BMP: truncated RLE8 absolute");
        }
        for (index = 0u; index < code; ++index) {
            if ((unsigned int)buffer[offset + index] >=
                    (unsigned int)info->palette_count ||
                !sixel_bmp_put_index(info,
                                     indices,
                                     &x,
                                     y,
                                     buffer[offset + index])) {
                return sixel_bmp_fail("builtin BMP: RLE8 absolute overflow");
            }
        }
        offset += (size_t)code;
        if ((code & 1u) != 0u) {
            if (offset >= size) {
                return sixel_bmp_fail("builtin BMP: RLE8 padding overflow");
            }
            offset++;
        }
    }

    return sixel_bmp_fail("builtin BMP: missing RLE8 end marker");
}

static SIXELSTATUS
sixel_bmp_decode_rle4_indices(sixel_bmp_decode_info_t const *info,
                              unsigned char *indices)
{
    unsigned char const *buffer;
    size_t size;
    size_t offset;
    size_t byte_count;
    int x;
    int y;
    int step;
    unsigned int count;
    unsigned int code;
    unsigned int index;
    unsigned int dx;
    unsigned int dy;
    unsigned int nibble;
    unsigned char packed;

    buffer = NULL;
    size = 0u;
    offset = 0u;
    byte_count = 0u;
    x = 0;
    y = 0;
    step = 0;
    count = 0u;
    code = 0u;
    index = 0u;
    dx = 0u;
    dy = 0u;
    nibble = 0u;
    packed = 0u;
    if (info == NULL || indices == NULL || info->chunk == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    buffer = info->chunk->buffer;
    size = info->chunk->size;
    offset = info->pixel_offset;
    x = 0;
    y = info->top_down ? 0 : info->height - 1;
    /*
     * RLE stream coordinates move downward for top-down BMP,
     * upward otherwise.
     */
    step = info->top_down ? 1 : -1;

    while (offset < size) {
        if (size - offset < 2u) {
            return sixel_bmp_fail("builtin BMP: truncated RLE4 stream");
        }
        count = (unsigned int)buffer[offset++];
        code = (unsigned int)buffer[offset++];

        if (count != 0u) {
            for (index = 0u; index < count; ++index) {
                nibble = (index & 1u) == 0u ? (code >> 4u) : (code & 0x0fu);
                if (nibble >= (unsigned int)info->palette_count ||
                    !sixel_bmp_put_index(info,
                                         indices,
                                         &x,
                                         y,
                                         (unsigned char)nibble)) {
                    return sixel_bmp_fail("builtin BMP: RLE4 write overflow");
                }
            }
            continue;
        }

        if (code == 0u) {
            x = 0;
            y += step;
            continue;
        }
        if (code == 1u) {
            return SIXEL_OK;
        }
        if (code == 2u) {
            if (size - offset < 2u) {
                return sixel_bmp_fail("builtin BMP: truncated RLE4 delta");
            }
            dx = (unsigned int)buffer[offset++];
            dy = (unsigned int)buffer[offset++];
            x += (int)dx;
            if (step > 0) {
                y += (int)dy;
            } else {
                y -= (int)dy;
            }
            if (x < 0 || x >= info->width || y < 0 || y >= info->height) {
                return sixel_bmp_fail("builtin BMP: invalid RLE4 delta");
            }
            continue;
        }

        byte_count = ((size_t)code + 1u) >> 1u;
        if (byte_count > size - offset) {
            return sixel_bmp_fail("builtin BMP: truncated RLE4 absolute");
        }
        for (index = 0u; index < code; ++index) {
            packed = buffer[offset + (index >> 1u)];
            nibble = (index & 1u) == 0u
                ? (unsigned int)(packed >> 4u)
                : (unsigned int)(packed & 0x0fu);
            if (nibble >= (unsigned int)info->palette_count ||
                !sixel_bmp_put_index(info,
                                     indices,
                                     &x,
                                     y,
                                     (unsigned char)nibble)) {
                return sixel_bmp_fail("builtin BMP: RLE4 absolute overflow");
            }
        }
        offset += byte_count;
        if ((byte_count & 1u) != 0u) {
            if (offset >= size) {
                return sixel_bmp_fail("builtin BMP: RLE4 padding overflow");
            }
            offset++;
        }
    }

    return sixel_bmp_fail("builtin BMP: missing RLE4 end marker");
}

static SIXELSTATUS
sixel_bmp_convert_indices_to_rgb(sixel_bmp_decode_info_t const *info,
                                 unsigned char const *indices,
                                 unsigned char *pixels)
{
    size_t x;
    size_t y;
    size_t offset;
    unsigned int palette_index;

    x = 0u;
    y = 0u;
    offset = 0u;
    palette_index = 0u;
    if (info == NULL || indices == NULL || pixels == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (y = 0u; y < (size_t)info->height; ++y) {
        for (x = 0u; x < (size_t)info->width; ++x) {
            offset = y * (size_t)info->width + x;
            palette_index = (unsigned int)indices[offset];
            if (palette_index >= (unsigned int)info->palette_count) {
                return sixel_bmp_fail("builtin BMP: palette index overflow");
            }
            pixels[offset * 3u + 0u] = info->palette[palette_index][0];
            pixels[offset * 3u + 1u] = info->palette[palette_index][1];
            pixels[offset * 3u + 2u] = info->palette[palette_index][2];
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_bmp_decode_indexed_uncompressed(sixel_bmp_decode_info_t const *info,
                                      unsigned char *pixels)
{
    unsigned char const *buffer;
    size_t row_offset;
    size_t source_row;
    size_t x;
    size_t y;
    unsigned int palette_index;
    unsigned char packed;

    buffer = NULL;
    row_offset = 0u;
    source_row = 0u;
    x = 0u;
    y = 0u;
    palette_index = 0u;
    packed = 0u;
    if (info == NULL || pixels == NULL || info->chunk == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    buffer = info->chunk->buffer;
    for (y = 0u; y < (size_t)info->height; ++y) {
        source_row = info->top_down
            ? y
            : (size_t)info->height - 1u - y;
        row_offset = info->pixel_offset + source_row * info->row_stride;

        for (x = 0u; x < (size_t)info->width; ++x) {
            if (info->bpp == 8) {
                palette_index = (unsigned int)buffer[row_offset + x];
            } else if (info->bpp == 4) {
                packed = buffer[row_offset + (x >> 1u)];
                if ((x & 1u) == 0u) {
                    palette_index = (unsigned int)(packed >> 4u);
                } else {
                    palette_index = (unsigned int)(packed & 0x0fu);
                }
            } else {
                packed = buffer[row_offset + (x >> 3u)];
                palette_index = (unsigned int)((packed >> (7u - (x & 7u)))
                                               & 0x01u);
            }
            if (palette_index >= (unsigned int)info->palette_count) {
                return sixel_bmp_fail("builtin BMP: palette index overflow");
            }
            pixels[(y * (size_t)info->width + x) * 3u + 0u] =
                info->palette[palette_index][0];
            pixels[(y * (size_t)info->width + x) * 3u + 1u] =
                info->palette[palette_index][1];
            pixels[(y * (size_t)info->width + x) * 3u + 2u] =
                info->palette[palette_index][2];
        }
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_bmp_decode_indexed_rle(sixel_bmp_decode_info_t const *info,
                             unsigned char *pixels)
{
    SIXELSTATUS status;
    unsigned char *indices;
    size_t pixel_count;

    status = SIXEL_FALSE;
    indices = NULL;
    pixel_count = 0u;
    if (info == NULL || pixels == NULL || info->chunk == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)info->width > SIZE_MAX / (size_t)info->height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_count = (size_t)info->width * (size_t)info->height;
    indices = (unsigned char *)sixel_allocator_malloc(info->chunk->allocator,
                                                      pixel_count);
    if (indices == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    memset(indices, 0, pixel_count);

    if (info->compression == SIXEL_BMP_COMPRESSION_RLE8) {
        status = sixel_bmp_decode_rle8_indices(info, indices);
    } else {
        status = sixel_bmp_decode_rle4_indices(info, indices);
    }
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(info->chunk->allocator, indices);
        return status;
    }

    status = sixel_bmp_convert_indices_to_rgb(info, indices, pixels);
    sixel_allocator_free(info->chunk->allocator, indices);
    return status;
}

static SIXELSTATUS
sixel_bmp_decode_cmyk(sixel_bmp_decode_info_t const *info,
                      unsigned char *pixels)
{
    unsigned char const *buffer;
    size_t row_offset;
    size_t source_row;
    size_t x;
    size_t y;
    size_t dst_offset;
    size_t src_offset;

    buffer = NULL;
    row_offset = 0u;
    source_row = 0u;
    x = 0u;
    y = 0u;
    dst_offset = 0u;
    src_offset = 0u;
    if (info == NULL || pixels == NULL || info->chunk == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    buffer = info->chunk->buffer;
    for (y = 0u; y < (size_t)info->height; ++y) {
        source_row = info->top_down ? y : (size_t)info->height - 1u - y;
        row_offset = info->pixel_offset + source_row * info->row_stride;
        for (x = 0u; x < (size_t)info->width; ++x) {
            src_offset = row_offset + x * 4u;
            dst_offset = (y * (size_t)info->width + x) * 4u;
            pixels[dst_offset + 0u] = buffer[src_offset + 0u];
            pixels[dst_offset + 1u] = buffer[src_offset + 1u];
            pixels[dst_offset + 2u] = buffer[src_offset + 2u];
            pixels[dst_offset + 3u] = buffer[src_offset + 3u];
        }
    }
    return SIXEL_OK;
}

static void
sixel_bmp_drop_alpha_inplace(unsigned char *pixels, size_t pixel_count)
{
    size_t index;

    index = 0u;
    if (pixels == NULL) {
        return;
    }
    for (index = 0u; index < pixel_count; ++index) {
        pixels[index * 3u + 0u] = pixels[index * 4u + 0u];
        pixels[index * 3u + 1u] = pixels[index * 4u + 1u];
        pixels[index * 3u + 2u] = pixels[index * 4u + 2u];
    }
}

static SIXELSTATUS
sixel_bmp_decode_truecolor(sixel_bmp_decode_info_t const *info,
                           unsigned char *pixels,
                           int *pcomp)
{
    unsigned char const *buffer;
    size_t row_offset;
    size_t source_row;
    size_t x;
    size_t y;
    size_t dst_offset;
    size_t src_offset;
    unsigned int value;
    unsigned int alpha_or;
    unsigned int alpha_and;
    int rshift;
    int gshift;
    int bshift;
    int ashift;
    int rcount;
    int gcount;
    int bcount;
    int alpha_count;
    int comp;
    size_t pixel_count;
    size_t index;

    buffer = NULL;
    row_offset = 0u;
    source_row = 0u;
    x = 0u;
    y = 0u;
    dst_offset = 0u;
    src_offset = 0u;
    value = 0u;
    alpha_or = 0u;
    alpha_and = 0xffu;
    rshift = 0;
    gshift = 0;
    bshift = 0;
    ashift = 0;
    rcount = 0;
    gcount = 0;
    bcount = 0;
    alpha_count = 0;
    comp = 3;
    pixel_count = 0u;
    index = 0u;
    if (info == NULL || pixels == NULL || pcomp == NULL ||
        info->chunk == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    buffer = info->chunk->buffer;
    comp = info->has_alpha_mask != 0 ? 4 : 3;
    if (info->bpp == 16 || info->bpp == 32) {
        rshift = sixel_bmp_high_bit(info->red_mask) - 7;
        gshift = sixel_bmp_high_bit(info->green_mask) - 7;
        bshift = sixel_bmp_high_bit(info->blue_mask) - 7;
        rcount = sixel_bmp_bitcount(info->red_mask);
        gcount = sixel_bmp_bitcount(info->green_mask);
        bcount = sixel_bmp_bitcount(info->blue_mask);
        if (rcount > 8 || gcount > 8 || bcount > 8) {
            return sixel_bmp_fail("builtin BMP: invalid color mask width");
        }
        if (info->has_alpha_mask != 0) {
            ashift = sixel_bmp_high_bit(info->alpha_mask) - 7;
            alpha_count = sixel_bmp_bitcount(info->alpha_mask);
            if (alpha_count > 8) {
                return sixel_bmp_fail("builtin BMP: invalid alpha mask width");
            }
        }
    }

    for (y = 0u; y < (size_t)info->height; ++y) {
        source_row = info->top_down
            ? y
            : (size_t)info->height - 1u - y;
        row_offset = info->pixel_offset + source_row * info->row_stride;

        for (x = 0u; x < (size_t)info->width; ++x) {
            dst_offset = (y * (size_t)info->width + x) * (size_t)comp;
            if (info->bpp == 24) {
                src_offset = row_offset + x * 3u;
                pixels[dst_offset + 0u] = buffer[src_offset + 2u];
                pixels[dst_offset + 1u] = buffer[src_offset + 1u];
                pixels[dst_offset + 2u] = buffer[src_offset + 0u];
                continue;
            }

            if (info->bpp == 16) {
                src_offset = row_offset + x * 2u;
                value = (unsigned int)buffer[src_offset + 0u]
                    | ((unsigned int)buffer[src_offset + 1u] << 8u);
            } else {
                src_offset = row_offset + x * 4u;
                value = (unsigned int)buffer[src_offset + 0u]
                    | ((unsigned int)buffer[src_offset + 1u] << 8u)
                    | ((unsigned int)buffer[src_offset + 2u] << 16u)
                    | ((unsigned int)buffer[src_offset + 3u] << 24u);
            }

            pixels[dst_offset + 0u] = (unsigned char)sixel_bmp_shift_signed(
                value & info->red_mask,
                rshift,
                rcount);
            pixels[dst_offset + 1u] = (unsigned char)sixel_bmp_shift_signed(
                value & info->green_mask,
                gshift,
                gcount);
            pixels[dst_offset + 2u] = (unsigned char)sixel_bmp_shift_signed(
                value & info->blue_mask,
                bshift,
                bcount);
            if (comp == 4) {
                pixels[dst_offset + 3u] = (unsigned char)sixel_bmp_shift_signed(
                    value & info->alpha_mask,
                    ashift,
                    alpha_count);
                alpha_or |= pixels[dst_offset + 3u];
                alpha_and &= pixels[dst_offset + 3u];
            }
        }
    }

    if (comp == 4) {
        pixel_count = (size_t)info->width * (size_t)info->height;
        /*
         * Legacy BMP files often leave alpha bytes as zero.
         * Treat all-zero alpha as opaque to match stb_image behavior.
         */
        if (alpha_or == 0u) {
            for (index = 0u; index < pixel_count; ++index) {
                pixels[index * 4u + 3u] = 0xffu;
            }
            alpha_and = 0xffu;
        }
        if (alpha_and == 0xffu) {
            sixel_bmp_drop_alpha_inplace(pixels, pixel_count);
            comp = 3;
        }
    }

    *pcomp = comp;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_frombmp_load(sixel_chunk_t const *chunk,
                   unsigned char **ppixels,
                   int *pwidth,
                   int *pheight,
                   int *pcomp,
                   int *pis_cmyk,
                   unsigned char const **picc_profile,
                   size_t *picc_profile_length)
{
    SIXELSTATUS status;
    sixel_bmp_decode_info_t info;
    unsigned char *pixels;
    size_t pixel_count;
    size_t buffer_size;
    int comp;

    status = SIXEL_FALSE;
    memset(&info, 0, sizeof(info));
    pixels = NULL;
    pixel_count = 0u;
    buffer_size = 0u;
    comp = 0;
    if (chunk == NULL ||
        ppixels == NULL ||
        pwidth == NULL ||
        pheight == NULL ||
        pcomp == NULL ||
        pis_cmyk == NULL ||
        chunk->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppixels = NULL;
    *pwidth = 0;
    *pheight = 0;
    *pcomp = 0;
    *pis_cmyk = 0;
    if (picc_profile != NULL) {
        *picc_profile = NULL;
    }
    if (picc_profile_length != NULL) {
        *picc_profile_length = 0u;
    }

    status = sixel_bmp_parse_header(chunk, &info);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (sixel_bmp_is_compressed_payload(info.compression)) {
        return sixel_bmp_fail(
            "builtin BMP: compressed payload requires probe decode path");
    }
    if ((size_t)info.width > SIZE_MAX / (size_t)info.height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_count = (size_t)info.width * (size_t)info.height;
    if (info.is_cmyk != 0) {
        comp = 4;
        if (pixel_count > SIZE_MAX / 4u) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        buffer_size = pixel_count * 4u;
        pixels = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                         buffer_size);
        if (pixels == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        status = sixel_bmp_decode_cmyk(&info, pixels);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(chunk->allocator, pixels);
            return status;
        }
    } else if (info.bpp < 16) {
        comp = 3;
        if (pixel_count > SIZE_MAX / 3u) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        buffer_size = pixel_count * 3u;
        pixels = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                         buffer_size);
        if (pixels == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        if (info.compression == SIXEL_BMP_COMPRESSION_RGB) {
            status = sixel_bmp_decode_indexed_uncompressed(&info, pixels);
        } else {
            status = sixel_bmp_decode_indexed_rle(&info, pixels);
        }
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(chunk->allocator, pixels);
            return status;
        }
    } else if (info.bpp == 24) {
        comp = 3;
        if (pixel_count > SIZE_MAX / 3u) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        buffer_size = pixel_count * 3u;
        pixels = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                         buffer_size);
        if (pixels == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        status = sixel_bmp_decode_truecolor(&info, pixels, &comp);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(chunk->allocator, pixels);
            return status;
        }
    } else {
        if (info.has_alpha_mask != 0) {
            comp = 4;
            if (pixel_count > SIZE_MAX / 4u) {
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
            buffer_size = pixel_count * 4u;
        } else {
            comp = 3;
            if (pixel_count > SIZE_MAX / 3u) {
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
            buffer_size = pixel_count * 3u;
        }
        pixels = (unsigned char *)sixel_allocator_malloc(chunk->allocator,
                                                         buffer_size);
        if (pixels == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        status = sixel_bmp_decode_truecolor(&info, pixels, &comp);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(chunk->allocator, pixels);
            return status;
        }
    }

    *ppixels = pixels;
    *pwidth = info.width;
    *pheight = info.height;
    *pcomp = comp;
    *pis_cmyk = info.is_cmyk;
    if (picc_profile != NULL) {
        *picc_profile = info.icc_profile;
    }
    if (picc_profile_length != NULL) {
        *picc_profile_length = info.icc_profile_length;
    }
    return SIXEL_OK;
}

SIXELSTATUS
sixel_frombmp_probe(
    sixel_chunk_t const *chunk,
    sixel_frombmp_probe_t *probe)
{
    SIXELSTATUS status;
    sixel_bmp_decode_info_t info;

    status = SIXEL_FALSE;
    memset(&info, 0, sizeof(info));
    if (chunk == NULL || probe == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(probe, 0, sizeof(*probe));
    status = sixel_bmp_parse_header(chunk, &info);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    probe->width = info.width;
    probe->height = info.height;
    probe->bpp = info.bpp;
    probe->is_cmyk = info.is_cmyk;
    probe->compression = info.compression;
    probe->payload = info.payload;
    probe->payload_size = info.payload_size;
    probe->icc_profile = info.icc_profile;
    probe->icc_profile_length = info.icc_profile_length;
    probe->has_calibrated_rgb = info.has_calibrated_rgb;
    probe->calibrated_gamma = info.calibrated_gamma;
    probe->white_x = info.white_x;
    probe->white_y = info.white_y;
    probe->red_x = info.red_x;
    probe->red_y = info.red_y;
    probe->green_x = info.green_x;
    probe->green_y = info.green_y;
    probe->blue_x = info.blue_x;
    probe->blue_y = info.blue_y;
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
