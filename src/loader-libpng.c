/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
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
 *
 * libpng-backed loader helpers extracted from loader.c. Concentrating PNG
 * handling here keeps the main registry lean and isolates libpng headers from
 * other backends while preserving the existing control flow and diagnostics.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_LIBPNG

#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <setjmp.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STDARG_H
# include <stdarg.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include <png.h>

#include <sixel.h>

#include "allocator.h"
#include "cms.h"
#include "chunk.h"
#include "compat_stub.h"
#include "loader-common.h"
#include "frame.h"
#include "loader.h"
#include "loader-libpng.h"
#include "logger.h"

typedef struct sixel_loader_libpng_component {
    sixel_loader_component_t base;
    sixel_allocator_t *allocator;
    unsigned int ref;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_control;
    int has_bgcolor;
    unsigned char bgcolor[3];
    int has_start_frame_no;
    int start_frame_no;
} sixel_loader_libpng_component_t;

/*
 * Topic-scoped APNG decoder diagnostics.
 *
 * Enabled when SIXEL_TRACE_TOPIC includes "apng_decode".
 * Supported separators follow options.c behavior:
 * comma, colon, semicolon, and ASCII whitespace.
 */
static int
apng_decode_trace_is_enabled(void)
{
    char const *topics;
    char const *cursor;
    char const *token_end;
    size_t topic_length;
    size_t token_length;
    char const *topic;

    topics = NULL;
    cursor = NULL;
    token_end = NULL;
    topic_length = 0u;
    token_length = 0u;
    topic = "apng_decode";

    topic_length = strlen(topic);
    topics = sixel_compat_getenv("SIXEL_TRACE_TOPIC");
    if (topics == NULL || topics[0] == '\0') {
        return 0;
    }

    cursor = topics;
    while (*cursor != '\0') {
        while (*cursor != '\0' &&
               (*cursor == ' ' || *cursor == '\t' || *cursor == ',' ||
                *cursor == ':' || *cursor == ';')) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        token_end = cursor;
        while (*token_end != '\0' &&
               *token_end != ' ' && *token_end != '\t' &&
               *token_end != ',' && *token_end != ':' &&
               *token_end != ';') {
            ++token_end;
        }

        token_length = (size_t)(token_end - cursor);
        if (token_length == topic_length &&
            strncmp(cursor, topic, token_length) == 0) {
            return 1;
        }

        cursor = token_end;
    }

    return 0;
}

static void
apng_decode_trace_message(char const *format, ...)
{
    va_list args;

    if (!apng_decode_trace_is_enabled()) {
        return;
    }

    fprintf(stderr, "libsixel[apng_decode]: ");
    va_start(args, format);
    sixel_compat_vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
}

static void
read_png(png_structp png_ptr,
         png_bytep data,
         png_size_t length)
{
    sixel_chunk_t *pchunk;

    pchunk = (sixel_chunk_t *)png_get_io_ptr(png_ptr);
    if (length > pchunk->size) {
        length = pchunk->size;
    }
    if (length > 0) {
        memcpy(data, pchunk->buffer, length);
        pchunk->buffer += length;
        pchunk->size -= length;
    }
}

static void
read_palette(png_structp png_ptr,
             png_infop info_ptr,
             unsigned char *palette,
             int ncolors,
             png_color *png_palette,
             png_color_16 *pbackground,
             int *transparent)
{
    png_bytep trans;
    int num_trans;
    int i;

    trans = NULL;
    num_trans = 0;
    i = 0;

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, NULL);
    }
    if (num_trans > 0) {
        *transparent = trans[0];
    }
    for (i = 0; i < ncolors; ++i) {
        if (pbackground && i < num_trans) {
            palette[i * 3 + 0] = ((0xff - trans[i]) * pbackground->red
                                   + trans[i] * png_palette[i].red) >> 8;
            palette[i * 3 + 1] = ((0xff - trans[i]) * pbackground->green
                                   + trans[i] * png_palette[i].green) >> 8;
            palette[i * 3 + 2] = ((0xff - trans[i]) * pbackground->blue
                                   + trans[i] * png_palette[i].blue) >> 8;
        } else {
            palette[i * 3 + 0] = png_palette[i].red;
            palette[i * 3 + 1] = png_palette[i].green;
            palette[i * 3 + 2] = png_palette[i].blue;
        }
    }
}

#if HAVE_SETJMP && HAVE_LONGJMP
static jmp_buf jmpbuf;
#endif  /* HAVE_SETJMP && HAVE_LONGJMP */

/* libpng error handler */
static void
png_error_callback(png_structp png_ptr, png_const_charp error_message)
{
    (void) png_ptr;

    sixel_helper_set_additional_message(error_message);
#if HAVE_SETJMP && HAVE_LONGJMP
    longjmp(jmpbuf, 1);
#endif  /* HAVE_SETJMP && HAVE_LONGJMP */
}

#if HAVE_LCMS2
static uint32_t
png_read_be32_chunk(unsigned char const *p)
{
    return ((uint32_t)p[0] << 24u) |
           ((uint32_t)p[1] << 16u) |
           ((uint32_t)p[2] << 8u) |
           (uint32_t)p[3];
}

static int
png_detect_chunk_flags_raw(unsigned char const *buffer,
                           size_t size,
                           int *has_iccp,
                           int *has_srgb,
                           int *has_chrm,
                           int *has_gama)
{
    static unsigned char const signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;

    if (has_iccp == NULL || has_srgb == NULL ||
        has_chrm == NULL || has_gama == NULL) {
        return 0;
    }

    *has_iccp = 0;
    *has_srgb = 0;
    *has_chrm = 0;
    *has_gama = 0;

    if (buffer == NULL || size < 8u) {
        return 0;
    }
    if (memcmp(buffer, signature, sizeof(signature)) != 0) {
        return 0;
    }

    offset = 8u;
    while (offset + 12u <= size) {
        uint32_t chunk_length;
        size_t chunk_total;
        unsigned char const *chunk_type;

        chunk_length = png_read_be32_chunk(buffer + offset);
        chunk_total = 12u + (size_t)chunk_length;
        if (chunk_total > size - offset) {
            return 0;
        }

        chunk_type = buffer + offset + 4u;
        if (memcmp(chunk_type, "iCCP", 4u) == 0) {
            *has_iccp = 1;
        } else if (memcmp(chunk_type, "sRGB", 4u) == 0) {
            *has_srgb = 1;
        } else if (memcmp(chunk_type, "cHRM", 4u) == 0) {
            *has_chrm = 1;
        } else if (memcmp(chunk_type, "gAMA", 4u) == 0) {
            *has_gama = 1;
        } else if (memcmp(chunk_type, "IEND", 4u) == 0) {
            break;
        }

        offset += chunk_total;
    }

    return 1;
}

/*
 * Convert decoded PNG RGB pixels from an embedded ICC profile to sRGB.
 *
 * The iCCP chunk is optional. When it is absent or invalid, the loader keeps
 * the original decoded pixels so behavior remains backward compatible.
 */
static int
png_convert_profile_to_srgb(unsigned char *pixels,
                            int width,
                            int height,
                            int pixelformat,
                            sixel_cms_profile_t * src_profile)
{
    sixel_cms_profile_t * dst_profile;
    sixel_cms_transform_t * transform;
    sixel_cms_color_space_t src_colorspace;
    sixel_cms_pixel_format_t src_type;
    sixel_cms_pixel_format_t dst_type;
    size_t pixel_count;
    unsigned char *gray_in;
    unsigned char *rgb_out;
    unsigned char *rgb_in;
    int converted;
    size_t i;

    dst_profile = NULL;
    transform = NULL;
    src_colorspace = SIXEL_CMS_COLORSPACE_RGB;
    src_type = SIXEL_CMS_PIXELFORMAT_RGB_8;
    dst_type = SIXEL_CMS_PIXELFORMAT_RGB_8;
    pixel_count = 0;
    gray_in = NULL;
    rgb_out = NULL;
    rgb_in = NULL;
    converted = 0;
    i = 0u;

    if (pixels == NULL || width <= 0 || height <= 0 || src_profile == NULL) {
        return 0;
    }
    if (pixelformat != SIXEL_PIXELFORMAT_RGB888 &&
        pixelformat != SIXEL_PIXELFORMAT_G8 &&
        pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32) {
        return 0;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32) {
        src_type = SIXEL_CMS_PIXELFORMAT_RGB_F32;
        dst_type = SIXEL_CMS_PIXELFORMAT_RGB_F32;
    }
    src_colorspace = sixel_cms_get_color_space(src_profile);
    pixel_count = (size_t)width * (size_t)height;

    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        return 0;
    }

    if (src_colorspace == SIXEL_CMS_COLORSPACE_GRAY && pixelformat == SIXEL_PIXELFORMAT_RGB888) {
        gray_in = (unsigned char *)malloc(pixel_count);
        rgb_out = (unsigned char *)malloc(pixel_count * 3u);
        if (gray_in == NULL || rgb_out == NULL) {
            goto cleanup;
        }
        for (i = 0u; i < pixel_count; ++i) {
            gray_in[i] = pixels[i * 3u];
        }
        transform = sixel_cms_create_transform(src_profile,
                                       SIXEL_CMS_PIXELFORMAT_GRAY_8,
                                       dst_profile,
                                       SIXEL_CMS_PIXELFORMAT_RGB_8,
                                       SIXEL_CMS_TRANSFORM_DEFAULT);
        if (transform == NULL) {
            goto cleanup;
        }
        sixel_cms_do_transform(transform, gray_in, rgb_out, pixel_count);
        memcpy(pixels, rgb_out, pixel_count * 3u);
        converted = 1;
        goto cleanup;
    }
    if (pixelformat == SIXEL_PIXELFORMAT_G8) {
        rgb_out = (unsigned char *)malloc(pixel_count * 3u);
        if (rgb_out == NULL) {
            goto cleanup;
        }
        if (src_colorspace == SIXEL_CMS_COLORSPACE_GRAY) {
            gray_in = (unsigned char *)malloc(pixel_count);
            if (gray_in == NULL) {
                goto cleanup;
            }
            memcpy(gray_in, pixels, pixel_count);
            transform = sixel_cms_create_transform(src_profile,
                                           SIXEL_CMS_PIXELFORMAT_GRAY_8,
                                           dst_profile,
                                           SIXEL_CMS_PIXELFORMAT_RGB_8,
                                           SIXEL_CMS_TRANSFORM_DEFAULT);
            if (transform == NULL) {
                goto cleanup;
            }
            sixel_cms_do_transform(transform, gray_in, rgb_out, pixel_count);
        } else {
            rgb_in = (unsigned char *)malloc(pixel_count * 3u);
            if (rgb_in == NULL) {
                goto cleanup;
            }
            for (i = 0u; i < pixel_count; ++i) {
                rgb_in[i * 3u + 0u] = pixels[i];
                rgb_in[i * 3u + 1u] = pixels[i];
                rgb_in[i * 3u + 2u] = pixels[i];
            }
            transform = sixel_cms_create_transform(src_profile,
                                           SIXEL_CMS_PIXELFORMAT_RGB_8,
                                           dst_profile,
                                           SIXEL_CMS_PIXELFORMAT_RGB_8,
                                           SIXEL_CMS_TRANSFORM_DEFAULT);
            if (transform == NULL) {
                goto cleanup;
            }
            sixel_cms_do_transform(transform, rgb_in, rgb_out, pixel_count);
        }
        for (i = 0u; i < pixel_count; ++i) {
            pixels[i] = rgb_out[i * 3u + 0u];
        }
        converted = 1;
        goto cleanup;
    }

    transform = sixel_cms_create_transform(src_profile,
                                   src_type,
                                   dst_profile,
                                   dst_type,
                                   SIXEL_CMS_TRANSFORM_DEFAULT);
    if (transform == NULL) {
        goto cleanup;
    }

    if (sixel_cms_do_transform(transform, pixels, pixels, pixel_count)) {
        converted = 1;
    }

cleanup:
    if (rgb_out != NULL) {
        free(rgb_out);
    }
    if (gray_in != NULL) {
        free(gray_in);
    }
    if (rgb_in != NULL) {
        free(rgb_in);
    }
    if (transform != NULL) {
        sixel_cms_delete_transform(transform);
    }
    if (dst_profile != NULL) {
        sixel_cms_close_profile(dst_profile);
    }
    return converted;
}

static int
png_build_rgb_profile_from_chunks(png_structp png_ptr,
                                  png_infop info_ptr,
                                  sixel_cms_profile_t **profile)
{
    sixel_cms_profile_t *built_profile;
    double file_gamma;
    double white_x;
    double white_y;
    double red_x;
    double red_y;
    double green_x;
    double green_y;
    double blue_x;
    double blue_y;
    int has_chrm;
    int has_gama;

    built_profile = NULL;
    file_gamma = 0.0;
    white_x = 0.0;
    white_y = 0.0;
    red_x = 0.0;
    red_y = 0.0;
    green_x = 0.0;
    green_y = 0.0;
    blue_x = 0.0;
    blue_y = 0.0;
    has_chrm = 0;
    has_gama = 0;

    if (profile == NULL) {
        return 0;
    }
    *profile = NULL;

    has_chrm = png_get_cHRM(png_ptr,
                            info_ptr,
                            &white_x,
                            &white_y,
                            &red_x,
                            &red_y,
                            &green_x,
                            &green_y,
                            &blue_x,
                            &blue_y) == PNG_INFO_cHRM;
    has_gama = png_get_gAMA(png_ptr, info_ptr, &file_gamma) == PNG_INFO_gAMA;

    if (!has_gama) {
        return 0;
    }
    if (!has_chrm) {
        white_x = 0.3127;
        white_y = 0.3290;
        red_x = 0.6400;
        red_y = 0.3300;
        green_x = 0.3000;
        green_y = 0.6000;
        blue_x = 0.1500;
        blue_y = 0.0600;
    }
    if (!has_gama) {
        file_gamma = 1.0 / 2.2;
    }
    if (file_gamma <= 0.0) {
        return 0;
    }

    built_profile = sixel_cms_create_rgb_profile_from_gamma_chrm(file_gamma,
                                                                 white_x,
                                                                 white_y,
                                                                 red_x,
                                                                 red_y,
                                                                 green_x,
                                                                 green_y,
                                                                 blue_x,
                                                                 blue_y);
    if (built_profile == NULL) {
        return 0;
    }

    *profile = built_profile;
    return 1;
}

static int
png_convert_embedded_icc_to_srgb(unsigned char *pixels,
                                 int width,
                                 int height,
                                 int pixelformat,
                                 png_bytep profile,
                                 png_uint_32 profile_length)
{
    sixel_cms_profile_t * src_profile;
    int converted;

    src_profile = NULL;
    converted = 0;
    if (profile == NULL || profile_length == 0u) {
        return 0;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_length);
    if (src_profile == NULL) {
        return 0;
    }

    converted = png_convert_profile_to_srgb(pixels,
                                            width,
                                            height,
                                            pixelformat,
                                            src_profile);
    sixel_cms_close_profile(src_profile);
    return converted;
}
#endif

static SIXELSTATUS
png_convert_rgb16_rows_to_rgbfloat32(unsigned char      /* out */ **result,
                                     unsigned char const /* in */  *rows16,
                                     png_size_t          /* in */  rowbytes,
                                     int                 /* in */  width,
                                     int                 /* in */  height,
                                     sixel_allocator_t   /* in */  *allocator)
{
    SIXELSTATUS status;
    float *dst;
    size_t pixel_count;
    size_t total_bytes;
    size_t y;
    size_t x;
    unsigned char const *src_row;
    size_t src_index;
    size_t dst_index;
    unsigned int value;

    status = SIXEL_FALSE;
    dst = NULL;
    pixel_count = 0u;
    total_bytes = 0u;
    y = 0u;
    x = 0u;
    src_row = NULL;
    src_index = 0u;
    dst_index = 0u;
    value = 0u;

    if (result == NULL || rows16 == NULL || allocator == NULL ||
        width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)rowbytes < (size_t)width * 6u) {
        sixel_helper_set_additional_message(
            "load_png: invalid 16-bit RGB row stride.");
        return SIXEL_BAD_INPUT;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    total_bytes = pixel_count * 3u * sizeof(float);
    dst = (float *)sixel_allocator_malloc(allocator, total_bytes);
    if (dst == NULL) {
        sixel_helper_set_additional_message(
            "load_png: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (y = 0u; y < (size_t)height; ++y) {
        src_row = rows16 + y * (size_t)rowbytes;
        for (x = 0u; x < (size_t)width; ++x) {
            src_index = x * 6u;
            dst_index = (y * (size_t)width + x) * 3u;

            value = ((unsigned int)src_row[src_index + 0u] << 8u)
                | (unsigned int)src_row[src_index + 1u];
            dst[dst_index + 0u] = (float)value / 65535.0f;

            value = ((unsigned int)src_row[src_index + 2u] << 8u)
                | (unsigned int)src_row[src_index + 3u];
            dst[dst_index + 1u] = (float)value / 65535.0f;

            value = ((unsigned int)src_row[src_index + 4u] << 8u)
                | (unsigned int)src_row[src_index + 5u];
            dst[dst_index + 2u] = (float)value / 65535.0f;
        }
    }

    *result = (unsigned char *)dst;
    status = SIXEL_OK;

    return status;
}

static SIXELSTATUS
load_png(unsigned char      /* out */ **result,
         unsigned char      /* in */  *buffer,
         size_t             /* in */  size,
         int                /* out */ *psx,
         int                /* out */ *psy,
         unsigned char      /* out */ **ppalette,
         int                /* out */ *pncolors,
         int                /* in */  reqcolors,
         int                /* out */ *pixelformat,
         unsigned char      /* out */ *bgcolor,
         int                /* out */ *transparent,
         int                /* out */ *cms_applied,
         int                /* in */  enable_cms,
         sixel_allocator_t  /* in */  *allocator)
{
    SIXELSTATUS status;
    sixel_chunk_t read_chunk;
    png_uint_32 bitdepth;
    png_uint_32 png_status;
    png_structp png_ptr;
    png_infop info_ptr;
#ifdef HAVE_DIAGNOSTIC_CLOBBERED
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wclobbered"
#endif
    unsigned char **rows = NULL;
    png_color *png_palette = NULL;
    png_color_16 background;
    png_color_16p default_background;
#if HAVE_LCMS2
    png_charp icc_name;
    int icc_compression_type;
    png_bytep icc_profile;
    png_uint_32 icc_profile_length;
    sixel_cms_profile_t * chunk_profile;
    int has_embedded_icc;
    int has_embedded_icc_raw;
    int has_srgb_chunk;
    int has_chrm_chunk;
    int has_gama_chunk;
    int has_raw_chunk_flags;
    int has_srgb_chunk_raw;
    int has_chrm_chunk_raw;
    int has_gama_chunk_raw;
    int intent;
    double white_x;
    double white_y;
    double red_x;
    double red_y;
    double green_x;
    double green_y;
    double blue_x;
    double blue_y;
    double file_gamma;
#endif
    png_uint_32 width;
    png_uint_32 height;
    png_uint_32 read_bitdepth;
    png_uint_32 read_channels;
    png_size_t rowbytes;
    unsigned char *raw16_pixels = NULL;
    size_t raw16_size = 0u;
    int promote_to_float32;
    int i;
    int depth;
    int cms_converted;

#if HAVE_SETJMP && HAVE_LONGJMP
    if (setjmp(jmpbuf) != 0) {
        sixel_allocator_free(allocator, *result);
        *result = NULL;
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }
#endif  /* HAVE_SETJMP && HAVE_LONGJMP */

    status = SIXEL_FALSE;
    *result = NULL;
    png_ptr = NULL;
    info_ptr = NULL;
    read_bitdepth = 0u;
    read_channels = 0u;
    rowbytes = 0u;
    promote_to_float32 = 0;
    cms_converted = 0;
#if HAVE_LCMS2
    icc_name = NULL;
    icc_compression_type = 0;
    icc_profile = NULL;
    icc_profile_length = 0u;
    chunk_profile = NULL;
    has_embedded_icc = 0;
    has_embedded_icc_raw = 0;
    has_srgb_chunk = 0;
    has_chrm_chunk = 0;
    has_gama_chunk = 0;
    has_raw_chunk_flags = 0;
    has_srgb_chunk_raw = 0;
    has_chrm_chunk_raw = 0;
    has_gama_chunk_raw = 0;
    intent = 0;
    white_x = 0.0;
    white_y = 0.0;
    red_x = 0.0;
    red_y = 0.0;
    green_x = 0.0;
    green_y = 0.0;
    blue_x = 0.0;
    blue_y = 0.0;
    file_gamma = 0.0;
#else
    (void)enable_cms;
    (void)cms_applied;
#endif
    if (cms_applied != NULL) {
        *cms_applied = 0;
    }

    png_ptr = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, NULL, &png_error_callback, NULL);
    if (!png_ptr) {
        sixel_helper_set_additional_message(
            "png_create_read_struct() failed.");
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }

    /*
     * The minimum valid PNG is 67 bytes.
     * https://garethrees.org/2007/11/14/pngcrush/
     */
    if (size < 67) {
        sixel_helper_set_additional_message("PNG data too small to be valid!");
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }

#if HAVE_SETJMP
    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
        sixel_allocator_free(allocator, *result);
        *result = NULL;
        status = SIXEL_PNG_ERROR;
        goto cleanup;
    }
#endif  /* HAVE_SETJMP */

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        sixel_helper_set_additional_message(
            "png_create_info_struct() failed.");
        status = SIXEL_PNG_ERROR;
        png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
        goto cleanup;
    }
    read_chunk.buffer = buffer;
    read_chunk.size = size;

    png_set_read_fn(png_ptr,(png_voidp)&read_chunk, read_png);
    png_read_info(png_ptr, info_ptr);
#if HAVE_LCMS2
    if (png_get_iCCP(png_ptr,
                     info_ptr,
                     &icc_name,
                     &icc_compression_type,
                     &icc_profile,
                     &icc_profile_length) == PNG_INFO_iCCP) {
        (void)icc_name;
        (void)icc_compression_type;
        has_embedded_icc = 1;
    } else {
        icc_profile = NULL;
        icc_profile_length = 0u;
        has_embedded_icc = 0;
    }
    has_srgb_chunk = png_get_sRGB(png_ptr, info_ptr, &intent) == PNG_INFO_sRGB;
    has_chrm_chunk = png_get_cHRM(png_ptr,
                                  info_ptr,
                                  &white_x,
                                  &white_y,
                                  &red_x,
                                  &red_y,
                                  &green_x,
                                  &green_y,
                                  &blue_x,
                                  &blue_y) == PNG_INFO_cHRM;
    has_gama_chunk = png_get_gAMA(png_ptr, info_ptr, &file_gamma) == PNG_INFO_gAMA;
    has_raw_chunk_flags = png_detect_chunk_flags_raw(buffer,
                                                     size,
                                                     &has_embedded_icc_raw,
                                                     &has_srgb_chunk_raw,
                                                     &has_chrm_chunk_raw,
                                                     &has_gama_chunk_raw);
    if (!has_raw_chunk_flags) {
        has_srgb_chunk_raw = has_srgb_chunk;
        has_chrm_chunk_raw = has_chrm_chunk;
        has_gama_chunk_raw = has_gama_chunk;
    }
    (void)has_embedded_icc_raw;
#endif

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);

    if (width > INT_MAX || height > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }

    *psx = (int)width;
    *psy = (int)height;

    bitdepth = png_get_bit_depth(png_ptr, info_ptr);
    if (bitdepth == 16) {
#  if HAVE_DEBUG
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "preserving 16bit for float32 conversion...\n");
#  endif
        promote_to_float32 = 1;
    }

    if (bgcolor) {
#  if HAVE_DEBUG
        fprintf(stderr,
                "background color is specified [%02x, %02x, %02x]\n",
                bgcolor[0], bgcolor[1], bgcolor[2]);
#  endif
        if (bitdepth == 16) {
            /*
             * png_set_background() expects background samples in the source
             * image precision. 16-bit PNG paths therefore require 8-bit CLI
             * colors to be expanded into 0..65535 before alpha composition.
             */
            background.red = (png_uint_16)(bgcolor[0] * 257u);
            background.green = (png_uint_16)(bgcolor[1] * 257u);
            background.blue = (png_uint_16)(bgcolor[2] * 257u);
            background.gray = (png_uint_16)(
                ((unsigned int)bgcolor[0]
                 + (unsigned int)bgcolor[1]
                 + (unsigned int)bgcolor[2]) * 257u / 3u);
        } else {
            background.red = bgcolor[0];
            background.green = bgcolor[1];
            background.blue = bgcolor[2];
            background.gray = (bgcolor[0] + bgcolor[1] + bgcolor[2]) / 3;
        }
    } else if (png_get_bKGD(png_ptr, info_ptr, &default_background)
            == PNG_INFO_bKGD) {
        memcpy(&background, default_background, sizeof(background));
#  if HAVE_DEBUG
        fprintf(stderr,
                "background color is found [%02x, %02x, %02x]\n",
                background.red, background.green, background.blue);
#  endif
    } else {
        background.red = 0;
        background.green = 0;
        background.blue = 0;
        background.gray = 0;
    }

    switch (png_get_color_type(png_ptr, info_ptr)) {
    case PNG_COLOR_TYPE_PALETTE:
#  if HAVE_DEBUG
        fprintf(stderr, "paletted PNG(PNG_COLOR_TYPE_PALETTE)\n");
#  endif
        png_status = png_get_PLTE(png_ptr, info_ptr,
                                  &png_palette, pncolors);
        if (png_status != PNG_INFO_PLTE || png_palette == NULL) {
            sixel_helper_set_additional_message(
                "PLTE chunk not found");
            status = SIXEL_PNG_ERROR;
            goto cleanup;
        }
#  if HAVE_DEBUG
        fprintf(stderr, "palette colors: %d\n", *pncolors);
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        if (ppalette == NULL || *pncolors > reqcolors) {
#  if HAVE_DEBUG
            fprintf(stderr, "detected more colors than required(>%d).\n",
                    reqcolors);
            fprintf(stderr, "expand to RGB format...\n");
#  endif
            png_set_background(png_ptr, &background,
                               PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
            png_set_palette_to_rgb(png_ptr);
            png_set_strip_alpha(png_ptr);
            *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        } else {
            switch (bitdepth) {
            case 1:
                *ppalette = (unsigned char *)
                    sixel_allocator_malloc(allocator,
                                           (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors,
                             png_palette,
                             &background,
                             transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL1;
                break;
            case 2:
                *ppalette = (unsigned char *)
                    sixel_allocator_malloc(allocator,
                                           (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors,
                             png_palette,
                             &background,
                             transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL2;
                break;
            case 4:
                *ppalette = (unsigned char *)
                    sixel_allocator_malloc(allocator,
                                           (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors,
                             png_palette,
                             &background,
                             transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL4;
                break;
            case 8:
                *ppalette = (unsigned char *)
                    sixel_allocator_malloc(allocator,
                                           (size_t)*pncolors * 3);
                if (*ppalette == NULL) {
                    sixel_helper_set_additional_message(
                        "load_png: sixel_allocator_malloc() failed.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                read_palette(png_ptr, info_ptr, *ppalette,
                             *pncolors,
                             png_palette,
                             &background,
                             transparent);
                *pixelformat = SIXEL_PIXELFORMAT_PAL8;
                break;
            default:
                png_set_background(png_ptr, &background,
                                   PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                png_set_palette_to_rgb(png_ptr);
                *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                break;
            }
        }
        break;
    case PNG_COLOR_TYPE_GRAY:
#  if HAVE_DEBUG
        fprintf(stderr, "grayscale PNG(PNG_COLOR_TYPE_GRAY)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        if (1 << bitdepth > reqcolors) {
#  if HAVE_DEBUG
            fprintf(stderr, "detected more colors than required(>%d).\n",
                    reqcolors);
            fprintf(stderr, "expand into RGB format...\n");
#  endif
            png_set_background(png_ptr, &background,
                               PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
            png_set_gray_to_rgb(png_ptr);
            *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        } else {
            switch (bitdepth) {
            case 1:
            case 2:
            case 4:
                if (ppalette) {
#  if HAVE_DECL_PNG_SET_EXPAND_GRAY_1_2_4_TO_8
#   if HAVE_DEBUG
                    fprintf(stderr, "expand %u bpp to 8bpp format...\n",
                            (unsigned int)bitdepth);
#   endif
                    png_set_expand_gray_1_2_4_to_8(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
#  elif HAVE_DECL_PNG_SET_GRAY_1_2_4_TO_8
#   if HAVE_DEBUG
                    fprintf(stderr, "expand %u bpp to 8bpp format...\n",
                            (unsigned int)bitdepth);
#   endif
                    png_set_gray_1_2_4_to_8(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
#  else
#   if HAVE_DEBUG
                    fprintf(stderr, "expand into RGB format...\n");
#   endif
                    png_set_background(png_ptr, &background,
                                       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
#  endif
                } else {
                    png_set_background(png_ptr, &background,
                                       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                }
                break;
            case 8:
                if (ppalette) {
                    *pixelformat = SIXEL_PIXELFORMAT_G8;
                } else {
#  if HAVE_DEBUG
                    fprintf(stderr, "expand into RGB format...\n");
#  endif
                    png_set_background(png_ptr, &background,
                                       PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                    png_set_gray_to_rgb(png_ptr);
                    *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                }
                break;
            default:
#  if HAVE_DEBUG
                fprintf(stderr, "expand into RGB format...\n");
#  endif
                png_set_background(png_ptr, &background,
                                   PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
                png_set_gray_to_rgb(png_ptr);
                *pixelformat = SIXEL_PIXELFORMAT_RGB888;
                break;
            }
        }
        break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
#  if HAVE_DEBUG
        fprintf(stderr, "grayscale-alpha PNG(PNG_COLOR_TYPE_GRAY_ALPHA)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "expand to RGB format...\n");
#  endif
        png_set_background(png_ptr, &background,
                           PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
        png_set_gray_to_rgb(png_ptr);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
#  if HAVE_DEBUG
        fprintf(stderr, "RGBA PNG(PNG_COLOR_TYPE_RGB_ALPHA)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
        fprintf(stderr, "expand to RGB format...\n");
#  endif
        png_set_background(png_ptr, &background,
                           PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    case PNG_COLOR_TYPE_RGB:
#  if HAVE_DEBUG
        fprintf(stderr, "RGB PNG(PNG_COLOR_TYPE_RGB)\n");
        fprintf(stderr, "bitdepth: %u\n", (unsigned int)bitdepth);
#  endif
        png_set_background(png_ptr, &background,
                           PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
        *pixelformat = SIXEL_PIXELFORMAT_RGB888;
        break;
    default:
        /* unknown format */
        goto cleanup;
    }
    if (promote_to_float32 && *pixelformat == SIXEL_PIXELFORMAT_RGB888) {
        png_read_update_info(png_ptr, info_ptr);
        read_bitdepth = png_get_bit_depth(png_ptr, info_ptr);
        read_channels = png_get_channels(png_ptr, info_ptr);
        rowbytes = png_get_rowbytes(png_ptr, info_ptr);

        if (read_bitdepth != 16u || read_channels != 3u) {
            sixel_helper_set_additional_message(
                "load_png: unsupported 16-bit PNG channel layout.");
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }
        if ((size_t)*psy > 0u && (size_t)rowbytes > SIZE_MAX / (size_t)*psy) {
            status = SIXEL_BAD_INTEGER_OVERFLOW;
            goto cleanup;
        }
        raw16_size = (size_t)rowbytes * (size_t)*psy;
        raw16_pixels = (unsigned char *)sixel_allocator_malloc(allocator,
                                                               raw16_size);
        if (raw16_pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }

        rows = (unsigned char **)sixel_allocator_malloc(
            allocator,
            (size_t)*psy * sizeof(unsigned char *));
        if (rows == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        for (i = 0; i < *psy; ++i) {
            rows[i] = raw16_pixels + (size_t)i * (size_t)rowbytes;
        }

        png_read_image(png_ptr, rows);

        status = png_convert_rgb16_rows_to_rgbfloat32(result,
                                                      raw16_pixels,
                                                      rowbytes,
                                                      *psx,
                                                      *psy,
                                                      allocator);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        *pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    } else {
        depth = sixel_helper_compute_depth(*pixelformat);
        *result = (unsigned char *)
            sixel_allocator_malloc(allocator,
                                   (size_t)(*psx * *psy * depth));
        if (*result == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        rows = (unsigned char **)sixel_allocator_malloc(
            allocator,
            (size_t)*psy * sizeof(unsigned char *));
        if (rows == NULL) {
            sixel_helper_set_additional_message(
                "load_png: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        switch (*pixelformat) {
        case SIXEL_PIXELFORMAT_PAL1:
        case SIXEL_PIXELFORMAT_PAL2:
        case SIXEL_PIXELFORMAT_PAL4:
            for (i = 0; i < *psy; ++i) {
                rows[i] = *result + (depth * *psx * (int)bitdepth + 7) / 8 * i;
            }
            break;
        default:
            for (i = 0; i < *psy; ++i) {
                rows[i] = *result + depth * *psx * i;
            }
            break;
        }

        png_read_image(png_ptr, rows);
    }

#if HAVE_LCMS2
    if (enable_cms && has_embedded_icc && has_srgb_chunk_raw && has_chrm_chunk_raw) {
        /* Priority 1: iCCP+sRGB+cHRM coexistence => no conversion. */
    } else if (enable_cms && has_embedded_icc) {
        /* Priority 2: iCCP only. */
        if (*pixelformat == SIXEL_PIXELFORMAT_PAL8 &&
            ppalette != NULL &&
            *ppalette != NULL &&
            pncolors != NULL &&
            *pncolors > 0) {
            sixel_cms_profile_t * embedded_profile;

            embedded_profile = sixel_cms_open_profile_from_mem(icc_profile, icc_profile_length);
            if (embedded_profile != NULL) {
                if (png_convert_profile_to_srgb(*ppalette,
                                                *pncolors,
                                                1,
                                                SIXEL_PIXELFORMAT_RGB888,
                                                embedded_profile)) {
                    cms_converted = 1;
                }
                sixel_cms_close_profile(embedded_profile);
            }
        } else {
            if (png_convert_embedded_icc_to_srgb(*result,
                                                 *psx,
                                                 *psy,
                                                 *pixelformat,
                                                 icc_profile,
                                                 icc_profile_length)) {
                cms_converted = 1;
            }
        }
    } else if (enable_cms && has_srgb_chunk_raw) {
        /* Priority 3: sRGB present => no conversion. */
    } else if (enable_cms &&
               has_gama_chunk_raw &&
               png_build_rgb_profile_from_chunks(png_ptr,
                                                 info_ptr,
                                                 &chunk_profile)) {
        /* Priority 4: gAMA(+/-cHRM). */
        if (*pixelformat == SIXEL_PIXELFORMAT_PAL8 &&
            ppalette != NULL &&
            *ppalette != NULL &&
            pncolors != NULL &&
            *pncolors > 0) {
            if (png_convert_profile_to_srgb(*ppalette,
                                            *pncolors,
                                            1,
                                            SIXEL_PIXELFORMAT_RGB888,
                                            chunk_profile)) {
                cms_converted = 1;
            }
        } else {
            if (png_convert_profile_to_srgb(*result,
                                            *psx,
                                            *psy,
                                            *pixelformat,
                                            chunk_profile)) {
                cms_converted = 1;
            }
        }
        sixel_cms_close_profile(chunk_profile);
    }
#endif
    if (cms_applied != NULL) {
        *cms_applied = cms_converted;
    }

    status = SIXEL_OK;

cleanup:
    if (png_ptr != NULL) {
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
    }

    if (rows != NULL) {
        sixel_allocator_free(allocator, rows);
    }
    if (raw16_pixels != NULL) {
        sixel_allocator_free(allocator, raw16_pixels);
    }

    return status;
}

typedef struct sixel_apng_frame_control {
    png_uint_32 width;
    png_uint_32 height;
    png_uint_32 x_offset;
    png_uint_32 y_offset;
    unsigned int delay_cs;
    unsigned int dispose_op;
    unsigned int blend_op;
} sixel_apng_frame_control_t;

typedef struct sixel_apng_canvas {
    unsigned char *pixels;
    unsigned char *backup;
    int width;
    int height;
} sixel_apng_canvas_t;

typedef struct sixel_apng_state {
    unsigned char const *ihdr;
    size_t ihdr_size;
    unsigned char *shared_chunks;
    size_t shared_chunks_size;
    size_t shared_chunks_capacity;
    unsigned char *chunk_base;
    size_t chunk_size;
    size_t chunk_capacity;
    png_uint_32 expected_sequence;
} sixel_apng_state_t;

static png_uint_32
read_be32(unsigned char const *p)
{
    png_uint_32 value;

    value = ((png_uint_32)p[0] << 24)
          | ((png_uint_32)p[1] << 16)
          | ((png_uint_32)p[2] << 8)
          |  (png_uint_32)p[3];
    return value;
}

static void
write_be32(unsigned char *p, png_uint_32 value)
{
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static png_uint_32
crc32_update(unsigned char const *data, size_t length, png_uint_32 seed)
{
    png_uint_32 crc;
    size_t i;
    int bit;

    crc = ~seed;
    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ 0xedb88320U;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

static SIXELSTATUS
libpng_parse_animation_start_frame_no(int *start_frame_no)
{
    SIXELSTATUS status;
    char const *env_value;
    char *endptr;
    long parsed;

    status = SIXEL_OK;
    env_value = NULL;
    endptr = NULL;
    parsed = 0;

    *start_frame_no = INT_MIN;
    env_value = sixel_compat_getenv("SIXEL_LOADER_ANIMATION_START_FRAME_NO");
    if (env_value == NULL || env_value[0] == '\0') {
        goto end;
    }

    parsed = strtol(env_value, &endptr, 10);
    if (endptr == env_value || *endptr != '\0') {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (parsed < (long)INT_MIN || parsed > (long)INT_MAX) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is out of range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *start_frame_no = (int)parsed;

end:
    return status;
}

static SIXELSTATUS
libpng_resolve_animation_start_frame_no(int start_frame_no,
                                        int frame_count,
                                        int *resolved)
{
    SIXELSTATUS status;
    int index;

    status = SIXEL_OK;
    index = 0;

    if (frame_count <= 0) {
        sixel_helper_set_additional_message(
            "Animation frame count must be positive.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (start_frame_no >= 0) {
        index = start_frame_no;
    } else {
        index = frame_count + start_frame_no;
    }

    if (index < 0 || index >= frame_count) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is outside"
            " the animation frame range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *resolved = index;

end:
    return status;
}

static int
ensure_shared_capacity(
    sixel_apng_state_t       *state,
    size_t                    append_size,
    sixel_allocator_t        *allocator)
{
    unsigned char *next;
    size_t needed;
    size_t next_capacity;

    if (append_size > SIZE_MAX - state->shared_chunks_size) {
        return 0;
    }
    needed = state->shared_chunks_size + append_size;
    if (needed <= state->shared_chunks_capacity) {
        return 1;
    }

    next_capacity = state->shared_chunks_capacity;
    if (next_capacity == 0) {
        next_capacity = 1024;
    }
    while (next_capacity < needed) {
        if (next_capacity > SIZE_MAX / 2) {
            return 0;
        }
        next_capacity *= 2;
    }
    next = (unsigned char *)sixel_allocator_malloc(allocator, next_capacity);
    if (next == NULL) {
        return 0;
    }
    if (state->shared_chunks_size > 0 && state->shared_chunks != NULL) {
        memcpy(next, state->shared_chunks, state->shared_chunks_size);
    }
    sixel_allocator_free(allocator, state->shared_chunks);
    state->shared_chunks = next;
    state->shared_chunks_capacity = next_capacity;
    return 1;
}

static int
append_shared_chunk(
    sixel_apng_state_t       *state,
    unsigned char const      *chunk,
    size_t                    chunk_size,
    sixel_allocator_t        *allocator)
{
    if (!ensure_shared_capacity(state, chunk_size, allocator)) {
        return 0;
    }
    memcpy(state->shared_chunks + state->shared_chunks_size,
           chunk,
           chunk_size);
    state->shared_chunks_size += chunk_size;
    return 1;
}

static int
ensure_chunk_capacity(
    sixel_apng_state_t       *state,
    size_t                    append_size,
    sixel_allocator_t        *allocator)
{
    unsigned char *next;
    size_t needed;
    size_t next_capacity;

    if (append_size > SIZE_MAX - state->chunk_size) {
        return 0;
    }
    needed = state->chunk_size + append_size;
    if (needed <= state->chunk_capacity) {
        return 1;
    }

    next_capacity = state->chunk_capacity;
    if (next_capacity == 0) {
        next_capacity = 4096;
    }
    while (next_capacity < needed) {
        if (next_capacity > SIZE_MAX / 2) {
            return 0;
        }
        next_capacity *= 2;
    }

    next = (unsigned char *)sixel_allocator_malloc(allocator, next_capacity);
    if (next == NULL) {
        return 0;
    }
    if (state->chunk_size > 0 && state->chunk_base != NULL) {
        memcpy(next, state->chunk_base, state->chunk_size);
    }
    sixel_allocator_free(allocator, (void *)state->chunk_base);
    state->chunk_base = next;
    state->chunk_capacity = next_capacity;
    return 1;
}

static int
append_chunk(
    sixel_apng_state_t       *state,
    char const               *type,
    unsigned char const      *data,
    png_uint_32               length,
    sixel_allocator_t        *allocator)
{
    unsigned char *dst;
    png_uint_32 crc;
    size_t chunk_bytes;

    chunk_bytes = (size_t)length + 12;
    if (!ensure_chunk_capacity(state, chunk_bytes, allocator)) {
        return 0;
    }

    dst = (unsigned char *)state->chunk_base + state->chunk_size;
    write_be32(dst, length);
    memcpy(dst + 4, type, 4);
    if (length > 0 && data != NULL) {
        memcpy(dst + 8, data, length);
    }

    crc = crc32_update((unsigned char const *)(dst + 4), 4, 0);
    if (length > 0 && data != NULL) {
        crc = crc32_update(data, length, crc);
    }
    write_be32(dst + 8 + length, (png_uint_32)crc);
    state->chunk_size += chunk_bytes;
    return 1;
}

static int
parse_fctl(
    unsigned char const         *data,
    png_uint_32                  length,
    png_uint_32                 *sequence_no,
    sixel_apng_frame_control_t  *control)
{
    png_uint_32 delay_num;
    png_uint_32 delay_den;
    png_uint_32 raw_delay_den;

    if (length != 26 || sequence_no == NULL || control == NULL) {
        sixel_helper_set_additional_message(
            "APNG parse error: invalid fcTL chunk length");
        return 0;
    }

    *sequence_no = read_be32(data + 0);
    control->width = read_be32(data + 4);
    control->height = read_be32(data + 8);
    control->x_offset = read_be32(data + 12);
    control->y_offset = read_be32(data + 16);
    delay_num = (png_uint_32)(((unsigned int)data[20] << 8) | data[21]);
    delay_den = (png_uint_32)(((unsigned int)data[22] << 8) | data[23]);
    raw_delay_den = delay_den;
    control->dispose_op = data[24];
    control->blend_op = data[25];

    if (control->dispose_op > 2 || control->blend_op > 1) {
        sixel_helper_set_additional_message(
            "APNG parse error: invalid fcTL dispose/blend value");
        return 0;
    }

    if (delay_den == 0) {
        apng_decode_trace_message(
            "fcTL seq=%u delay_den=0 detected, fallback=100 delay_num=%u",
            (unsigned int)*sequence_no,
            (unsigned int)delay_num);
        delay_den = 100;
    }
    /*
     * sixel_frame_set_delay() expects centiseconds like the GIF loader.
     * APNG stores delay as delay_num / delay_den seconds.
     */
    control->delay_cs = (unsigned int)((delay_num * 100U) / delay_den);
    if (control->delay_cs == 0 && delay_num > 0) {
        control->delay_cs = 1;
    }

    apng_decode_trace_message(
        "fcTL seq=%u rect=%ux%u+%u+%u delay_num=%u delay_den=%u "
        "delay_cs=%u dispose=%u blend=%u",
        (unsigned int)*sequence_no,
        (unsigned int)control->width,
        (unsigned int)control->height,
        (unsigned int)control->x_offset,
        (unsigned int)control->y_offset,
        (unsigned int)delay_num,
        (unsigned int)raw_delay_den,
        control->delay_cs,
        (unsigned int)control->dispose_op,
        (unsigned int)control->blend_op);

    return 1;
}

static SIXELSTATUS
decode_png_rgba(
    unsigned char      /* out */ **result,
    int                /* out */ *psx,
    int                /* out */ *psy,
    unsigned char      /* in */  *buffer,
    size_t             /* in */  size,
    sixel_allocator_t  /* in */  *allocator)
{
    SIXELSTATUS status;
    sixel_chunk_t read_chunk;
    png_structp png_ptr;
    png_infop info_ptr;
    png_uint_32 width;
    png_uint_32 height;
    png_uint_32 rowbytes;
    png_byte color_type;
    png_byte bitdepth;
    unsigned char **rows;
    int i;

    status = SIXEL_FALSE;
    png_ptr = NULL;
    info_ptr = NULL;
    rows = NULL;
    *result = NULL;
    *psx = 0;
    *psy = 0;

    png_ptr = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, NULL, &png_error_callback, NULL);
    if (!png_ptr) {
        status = SIXEL_PNG_ERROR;
        goto end;
    }

#if HAVE_SETJMP
    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
        status = SIXEL_PNG_ERROR;
        goto end;
    }
#endif

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        status = SIXEL_PNG_ERROR;
        goto end;
    }

    read_chunk.buffer = buffer;
    read_chunk.size = size;
    png_set_read_fn(png_ptr, (png_voidp)&read_chunk, read_png);
    png_read_info(png_ptr, info_ptr);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    if (width > INT_MAX || height > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto end;
    }

    color_type = png_get_color_type(png_ptr, info_ptr);
    bitdepth = png_get_bit_depth(png_ptr, info_ptr);
    if (bitdepth == 16) {
        png_set_strip_16(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bitdepth < 8) {
#if HAVE_DECL_PNG_SET_EXPAND_GRAY_1_2_4_TO_8
        png_set_expand_gray_1_2_4_to_8(png_ptr);
#elif HAVE_DECL_PNG_SET_GRAY_1_2_4_TO_8
        png_set_gray_1_2_4_to_8(png_ptr);
#endif
    }
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }
    if ((color_type & PNG_COLOR_MASK_ALPHA) == 0 &&
        !png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
    }

    png_read_update_info(png_ptr, info_ptr);
    rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    if (rowbytes != width * 4) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *result = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)height * (size_t)rowbytes);
    if (*result == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    rows = (unsigned char **)sixel_allocator_malloc(
        allocator,
        (size_t)height * sizeof(unsigned char *));
    if (rows == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    for (i = 0; i < (int)height; ++i) {
        rows[i] = *result + (size_t)i * rowbytes;
    }
    png_read_image(png_ptr, rows);

    *psx = (int)width;
    *psy = (int)height;
    status = SIXEL_OK;

end:
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, *result);
        *result = NULL;
    }
    sixel_allocator_free(allocator, rows);
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
    return status;
}

static void
apng_clear_rect(
    sixel_apng_canvas_t const      *canvas,
    sixel_apng_frame_control_t     *control)
{
    int x;
    int y;
    int px;
    int py;
    unsigned char *dst;

    for (y = 0; y < (int)control->height; ++y) {
        py = (int)control->y_offset + y;
        if (py < 0 || py >= canvas->height) {
            continue;
        }
        for (x = 0; x < (int)control->width; ++x) {
            px = (int)control->x_offset + x;
            if (px < 0 || px >= canvas->width) {
                continue;
            }
            dst = canvas->pixels + ((py * canvas->width + px) * 4);
            dst[0] = 0;
            dst[1] = 0;
            dst[2] = 0;
            dst[3] = 0;
        }
    }
}

static void
apng_blend_rect(
    sixel_apng_canvas_t const      *canvas,
    sixel_apng_frame_control_t     *control,
    unsigned char const            *src)
{
    int x;
    int y;
    int px;
    int py;
    int idx;
    unsigned int sa;
    unsigned int da;
    unsigned int oa;
    unsigned char const *sp;
    unsigned char *dp;

    for (y = 0; y < (int)control->height; ++y) {
        py = (int)control->y_offset + y;
        if (py < 0 || py >= canvas->height) {
            continue;
        }
        for (x = 0; x < (int)control->width; ++x) {
            px = (int)control->x_offset + x;
            if (px < 0 || px >= canvas->width) {
                continue;
            }
            idx = y * (int)control->width + x;
            sp = src + idx * 4;
            dp = canvas->pixels + ((py * canvas->width + px) * 4);

            if (control->blend_op == 0) {
                dp[0] = sp[0];
                dp[1] = sp[1];
                dp[2] = sp[2];
                dp[3] = sp[3];
                continue;
            }

            sa = sp[3];
            da = dp[3];
            oa = sa + ((da * (255 - sa)) / 255);
            if (oa == 0) {
                dp[0] = 0;
                dp[1] = 0;
                dp[2] = 0;
                dp[3] = 0;
                continue;
            }
            dp[0] = (unsigned char)((sp[0] * sa + dp[0] * da * (255 - sa) / 255)
                                    / oa);
            dp[1] = (unsigned char)((sp[1] * sa + dp[1] * da * (255 - sa) / 255)
                                    / oa);
            dp[2] = (unsigned char)((sp[2] * sa + dp[2] * da * (255 - sa) / 255)
                                    / oa);
            dp[3] = (unsigned char)oa;
        }
    }
}

static SIXELSTATUS
emit_apng_frame(
    sixel_apng_state_t const      *state,
    sixel_apng_frame_control_t    *control,
    int                            frame_no,
    int                            loop_no,
    int                            multiframe,
    int                            emit_callback,
    unsigned char                 *bgcolor,
    int                            reqcolors,
    int                            fuse_palette,
    sixel_apng_canvas_t           *canvas,
    sixel_load_image_function      fn_load,
    void                          *callback_context,
    sixel_allocator_t             *allocator)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    int width;
    int height;
    size_t png_size;
    unsigned char *png_data;
    unsigned char *subframe;
    unsigned char *emitted;
    size_t canvas_bytes;
    unsigned char ihdr_copy[13];

    frame = NULL;
    width = 0;
    height = 0;
    png_data = NULL;
    subframe = NULL;
    emitted = NULL;
    (void)reqcolors;
    (void)fuse_palette;

    if (state->ihdr == NULL || state->ihdr_size != 13) {
        return SIXEL_BAD_INPUT;
    }
    if (state->chunk_size > SIZE_MAX - 8 - 25) {
        return SIXEL_BAD_ALLOCATION;
    }

    png_size = 8 + 25 + state->shared_chunks_size + state->chunk_size + 12;
    png_data = (unsigned char *)sixel_allocator_malloc(allocator, png_size);
    if (png_data == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memcpy(png_data, "\x89PNG\r\n\x1a\n", 8);
    memcpy(ihdr_copy, state->ihdr, sizeof(ihdr_copy));
    write_be32(ihdr_copy + 0, control->width);
    write_be32(ihdr_copy + 4, control->height);

    memcpy(png_data + 8, "\x00\x00\x00\x0dIHDR", 8);
    memcpy(png_data + 16, ihdr_copy, sizeof(ihdr_copy));
    write_be32(png_data + 29,
               crc32_update((unsigned char const *)(png_data + 12), 17, 0));

    if (state->shared_chunks_size > 0) {
        memcpy(png_data + 33,
               state->shared_chunks,
               state->shared_chunks_size);
    }
    memcpy(png_data + 33 + state->shared_chunks_size,
           state->chunk_base,
           state->chunk_size);
    memcpy(png_data + 33 + state->shared_chunks_size + state->chunk_size,
           "\x00\x00\x00\x00"
           "IEND"
           "\xae\x42\x60\x82",
           12);

    status = decode_png_rgba(&subframe,
                             &width,
                             &height,
                             png_data,
                             png_size,
                             allocator);

    if (width != (int)control->width || height != (int)control->height) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    canvas_bytes = (size_t)canvas->width * (size_t)canvas->height * 4;
    if (control->dispose_op == 2) {
        memcpy(canvas->backup, canvas->pixels, canvas_bytes);
    }
    apng_blend_rect(canvas, control, subframe);

    if (!emit_callback) {
        status = SIXEL_OK;
        goto dispose;
    }

    emitted = (unsigned char *)sixel_allocator_malloc(allocator, canvas_bytes);
    if (emitted == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(emitted, canvas->pixels, canvas_bytes);

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    frame->width = canvas->width;
    frame->height = canvas->height;
    frame->palette = NULL;
    frame->ncolors = 0;
    frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    frame->transparent = (-1);
    sixel_frame_set_delay(frame, (int)control->delay_cs);
    sixel_frame_set_frame_no(frame, frame_no);
    sixel_frame_set_loop_count(frame, loop_no);
    sixel_frame_set_multiframe(frame, multiframe);
    sixel_frame_set_pixels(frame, emitted);
    emitted = NULL;

    status = sixel_frame_strip_alpha(frame, bgcolor);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = fn_load(frame, callback_context);

dispose:

    if (control->dispose_op == 1) {
        apng_clear_rect(canvas, control);
    } else if (control->dispose_op == 2) {
        memcpy(canvas->pixels, canvas->backup, canvas_bytes);
    }

end:
    sixel_allocator_free(allocator, png_data);
    sixel_allocator_free(allocator, subframe);
    sixel_allocator_free(allocator, emitted);
    sixel_frame_unref(frame);

    return status;
}

static SIXELSTATUS
load_apng_frames(
    sixel_chunk_t const       *pchunk,
    int                        fstatic,
    int                        fuse_palette,
    int                        reqcolors,
    unsigned char             *bgcolor,
    int                        loop_control,
    int                        start_frame_no,
    sixel_load_image_function  fn_load,
    void                      *context)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    SIXELSTATUS status;
    sixel_apng_state_t state;
    sixel_apng_frame_control_t control;
    unsigned char const *p;
    size_t remain;
    int seen_actl;
    int has_frame;
    int frame_no;
    int source_frame_no;
    int frames_in_loop;
    int num_frames;
    int num_plays;
    int loop_no;
    int stop_loop;
    int saw_animation;
    int seen_fctl;
    int seen_idat;
    int emit_callback;
    sixel_apng_canvas_t canvas;
    size_t canvas_bytes;
    png_uint_32 sequence_no;
    png_uint_32 fd_sequence;

    status = SIXEL_FALSE;
    memset(&state, 0, sizeof(state));
    memset(&control, 0, sizeof(control));
    p = NULL;
    remain = 0;
    seen_actl = 0;
    has_frame = 0;
    frame_no = 0;
    source_frame_no = 0;
    frames_in_loop = 0;
    num_frames = 0;
    num_plays = 0;
    loop_no = 0;
    stop_loop = 0;
    saw_animation = 0;
    seen_fctl = 0;
    seen_idat = 0;
    emit_callback = 1;
    memset(&canvas, 0, sizeof(canvas));
    canvas_bytes = 0;
    sequence_no = 0;
    fd_sequence = 0;

    /*
     * APNG parsing starts after the PNG signature. Guard against short
     * buffers so size_t subtraction cannot underflow.
     */
    if (pchunk == NULL || pchunk->buffer == NULL ||
        pchunk->size < sizeof(png_signature)) {
        status = SIXEL_FALSE;
        goto end;
    }
    if (memcmp(pchunk->buffer, png_signature, sizeof(png_signature)) != 0) {
        status = SIXEL_FALSE;
        goto end;
    }

    apng_decode_trace_message(
        "load_apng_frames: input_size=%lu static=%d loop_control=%d "
        "start_frame_no=%d",
        (unsigned long)pchunk->size,
        fstatic,
        loop_control,
        start_frame_no);

    for (;;) {
        if (sixel_loader_callback_is_canceled(context)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }

        memset(&state, 0, sizeof(state));
        memset(&control, 0, sizeof(control));
        p = pchunk->buffer + 8;
        remain = pchunk->size - 8;
        seen_actl = 0;
        has_frame = 0;
        frames_in_loop = 0;
        seen_fctl = 0;
        seen_idat = 0;

        if (loop_no > 0 && canvas_bytes > 0) {
            memset(canvas.pixels, 0, canvas_bytes);
            memset(canvas.backup, 0, canvas_bytes);
        }

    while (remain >= 12) {
        if (sixel_loader_callback_is_canceled(context)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }

        png_uint_32 length;

        length = read_be32(p);
        if ((size_t)length > remain - 12) {
            sixel_helper_set_additional_message(
                "APNG parse error: chunk length exceeds input size");
            status = SIXEL_BAD_INPUT;
            goto end;
        }

        apng_decode_trace_message(
            "chunk loop=%d remain=%lu type=%.4s length=%lu expected_seq=%lu",
            loop_no,
            (unsigned long)remain,
            (char const *)(p + 4),
            (unsigned long)length,
            (unsigned long)state.expected_sequence);

        if (memcmp(p + 4, "IHDR", 4) == 0) {
            if (length != 13) {
                sixel_helper_set_additional_message(
                    "APNG parse error: invalid IHDR chunk length");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            state.ihdr = p + 8;
            state.ihdr_size = length;
            if (canvas_bytes == 0) {
                canvas.width = (int)read_be32(p + 8);
                canvas.height = (int)read_be32(p + 12);
            }
            if (canvas.width <= 0 || canvas.height <= 0) {
                sixel_helper_set_additional_message(
                    "APNG parse error: invalid canvas size");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            if (canvas_bytes == 0) {
                canvas_bytes = (size_t)canvas.width * (size_t)canvas.height * 4;
                canvas.pixels = (unsigned char *)sixel_allocator_malloc(
                    pchunk->allocator,
                    canvas_bytes);
                canvas.backup = (unsigned char *)sixel_allocator_malloc(
                    pchunk->allocator,
                    canvas_bytes);
                if (canvas.pixels == NULL || canvas.backup == NULL) {
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
                memset(canvas.pixels, 0, canvas_bytes);
                memset(canvas.backup, 0, canvas_bytes);
            }
        } else if (memcmp(p + 4, "acTL", 4) == 0) {
            if (length != 8) {
                sixel_helper_set_additional_message(
                    "APNG parse error: invalid acTL chunk length");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            seen_actl = 1;
            saw_animation = 1;
            num_frames = (int)read_be32(p + 8);
            num_plays = (int)read_be32(p + 12);
            apng_decode_trace_message(
                "acTL parsed: num_frames=%d num_plays=%d loop_no=%d",
                num_frames,
                num_plays,
                loop_no);
            state.expected_sequence = 0;
            if (num_frames <= 0) {
                sixel_helper_set_additional_message(
                    "APNG parse error: acTL num_frames must be > 0");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            if (loop_no == 0 && start_frame_no != INT_MIN) {
                status = libpng_resolve_animation_start_frame_no(
                    start_frame_no,
                    num_frames,
                    &start_frame_no);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            }
        } else if (memcmp(p + 4, "fcTL", 4) == 0 && seen_actl) {
            if (has_frame && state.chunk_size > 0) {
                emit_callback = 1;
                if (loop_no == 0 && start_frame_no != INT_MIN &&
                    frames_in_loop < start_frame_no) {
                    emit_callback = 0;
                }
                status = emit_apng_frame(&state,
                                         &control,
                                         frame_no,
                                         loop_no,
                                         (!fstatic && num_frames > 1),
                                         emit_callback,
                                         bgcolor,
                                         reqcolors,
                                         fuse_palette,
                                         &canvas,
                                         fn_load,
                                         context,
                                         pchunk->allocator);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }

                if (sixel_loader_callback_is_canceled(context)) {
                    status = SIXEL_INTERRUPTED;
                    goto end;
                }

                ++frame_no;
                ++frames_in_loop;
                if (fstatic && emit_callback) {
                    status = SIXEL_OK;
                    goto end;
                }
                state.chunk_size = 0;
            }
            if (!parse_fctl(p + 8, length, &sequence_no, &control)) {
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            if (sequence_no != state.expected_sequence) {
                sixel_helper_set_additional_message(
                    "APNG parse error: fcTL sequence number mismatch");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            ++state.expected_sequence;
            apng_decode_trace_message(
                "fcTL accepted: seq=%lu next_expected=%lu seen_idat=%d",
                (unsigned long)sequence_no,
                (unsigned long)state.expected_sequence,
                seen_idat);
            if (control.width == 0 || control.height == 0 ||
                control.x_offset > (png_uint_32)canvas.width ||
                control.y_offset > (png_uint_32)canvas.height ||
                control.width > (png_uint_32)canvas.width - control.x_offset ||
                control.height >
                (png_uint_32)canvas.height - control.y_offset) {
                sixel_helper_set_additional_message(
                    "APNG parse error: fcTL rectangle is outside canvas");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            seen_fctl = 1;
            has_frame = 1;
        } else if (memcmp(p + 4, "fdAT", 4) == 0 && seen_actl) {
            if (!has_frame || !seen_fctl || length < 4) {
                sixel_helper_set_additional_message(
                    "APNG parse error: fdAT encountered before valid fcTL");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            fd_sequence = read_be32(p + 8);
            if (fd_sequence != state.expected_sequence) {
                sixel_helper_set_additional_message(
                    "APNG parse error: fdAT sequence number mismatch");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            ++state.expected_sequence;
            if (!append_chunk(&state,
                              "IDAT",
                              p + 12,
                              length - 4,
                              pchunk->allocator)) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        } else if (memcmp(p + 4, "IDAT", 4) == 0) {
            if (seen_actl && !has_frame && (seen_fctl || seen_idat)) {
                sixel_helper_set_additional_message(
                    "APNG parse error: unexpected IDAT ordering");
                status = SIXEL_BAD_INPUT;
                goto end;
            }
            if (!append_chunk(&state,
                              "IDAT",
                              p + 8,
                              length,
                              pchunk->allocator)) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            if (seen_actl && !seen_fctl && !seen_idat) {
                control.width = (png_uint_32)canvas.width;
                control.height = (png_uint_32)canvas.height;
                control.x_offset = 0;
                control.y_offset = 0;
                control.delay_cs = 0;
                control.dispose_op = 0;
                control.blend_op = 0;
            }
            seen_idat = 1;
            if (seen_actl != 0) {
                apng_decode_trace_message(
                    "IDAT accepted: has_frame=%d seen_fctl=%d seen_idat=%d",
                    has_frame,
                    seen_fctl,
                    seen_idat);
            }
            has_frame = 1;
        } else if (memcmp(p + 4, "IEND", 4) == 0) {
            break;
        } else if (memcmp(p + 4, "acTL", 4) != 0 &&
                   memcmp(p + 4, "fcTL", 4) != 0 &&
                   memcmp(p + 4, "fdAT", 4) != 0 &&
                   memcmp(p + 4, "IHDR", 4) != 0 &&
                   memcmp(p + 4, "IEND", 4) != 0 &&
                   state.chunk_size == 0) {
            if (!append_shared_chunk(&state,
                                     p,
                                     (size_t)length + 12,
                                     pchunk->allocator)) {
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }

        p += (size_t)length + 12;
        remain -= (size_t)length + 12;
    }

    if (!seen_actl || !has_frame) {
        status = SIXEL_FALSE;
        goto end;
    }

    if (state.chunk_size > 0) {
        emit_callback = 1;
        if (loop_no == 0 && start_frame_no != INT_MIN &&
            frames_in_loop < start_frame_no) {
            emit_callback = 0;
        }
        if (loop_no == 0 && start_frame_no != INT_MIN) {
            /*
             * frame_no is used by the encoder/tty path to select DECSC
             * for the first emitted frame and DECRC for subsequent frames.
             * Keep frame_no aligned to emitted order when the first loop
             * skips leading source frames.
             */
            frame_no = source_frame_no - start_frame_no;
        } else {
            frame_no = source_frame_no;
        }
        status = emit_apng_frame(&state,
                                 &control,
                                 frame_no,
                                 loop_no,
                                 (!fstatic && num_frames > 1),
                                 emit_callback,
                                 bgcolor,
                                 reqcolors,
                                 fuse_palette,
                                 &canvas,
                                 fn_load,
                                 context,
                                 pchunk->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        if (sixel_loader_callback_is_canceled(context)) {
            status = SIXEL_INTERRUPTED;
            goto end;
        }

        ++source_frame_no;
        ++frames_in_loop;
        if (fstatic && emit_callback) {
            status = SIXEL_OK;
            goto end;
        }
    }

    if (frames_in_loop == 0) {
        sixel_helper_set_additional_message(
            "APNG parse error: no decodable frame in animation");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (num_frames > 0 && frames_in_loop != num_frames) {
        sixel_helper_set_additional_message(
            "APNG parse error: decoded frame count mismatch");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    ++loop_no;

    if (loop_control == SIXEL_LOOP_DISABLE || frames_in_loop == 1) {
        stop_loop = 1;
    } else if (loop_control == SIXEL_LOOP_AUTO) {
        if (num_plays > 0 && loop_no >= num_plays) {
            stop_loop = 1;
        }
    }

    sixel_allocator_free(pchunk->allocator, state.shared_chunks);
    sixel_allocator_free(pchunk->allocator, (void *)state.chunk_base);
    state.shared_chunks = NULL;
    state.chunk_base = NULL;

    if (stop_loop) {
        apng_decode_trace_message(
            "load_apng_frames: stop loop_no=%d frames_in_loop=%d "
            "num_plays=%d",
            loop_no,
            frames_in_loop,
            num_plays);
        status = SIXEL_OK;
        goto end;
    }
    }

end:
    apng_decode_trace_message(
        "load_apng_frames: status=%d frame_no=%d source_frame_no=%d "
        "loop_no=%d saw_animation=%d",
        status,
        frame_no,
        source_frame_no,
        loop_no,
        saw_animation);
    sixel_allocator_free(pchunk->allocator, canvas.pixels);
    sixel_allocator_free(pchunk->allocator, canvas.backup);
    sixel_allocator_free(pchunk->allocator, state.shared_chunks);
    sixel_allocator_free(pchunk->allocator, (void *)state.chunk_base);
    if (!saw_animation && status == SIXEL_FALSE) {
        return SIXEL_FALSE;
    }
    return status;
}
#ifdef HAVE_DIAGNOSTIC_CLOBBERED
# pragma GCC diagnostic pop
#endif

/*
 * Dedicated libpng loader for precise PNG decoding.
 *
 *    +-----------+     +------------------+     +--------------------+
 *    | PNG chunk | --> | libpng decode    | --> | sixel frame emit   |
 *    +-----------+     +------------------+     +--------------------+
 */
static SIXELSTATUS
load_with_libpng(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    int                       /* in */     start_frame_no_set,
    int                       /* in */     start_frame_no_override,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char *pixels;
    int start_frame_no;
    int enable_cms;
    int cms_applied;
    int cms_target_pixelformat;

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;
    start_frame_no = INT_MIN;
    enable_cms = 1;
    cms_applied = 0;
    cms_target_pixelformat = SIXEL_PIXELFORMAT_RGB888;

    (void)fstatic;
    (void)loop_control;

    if (start_frame_no_set) {
        start_frame_no = start_frame_no_override;
    } else {
        status = libpng_parse_animation_start_frame_no(&start_frame_no);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    enable_cms = loader_libpng_get_enable_cms();
    status = load_apng_frames(pchunk,
                              fstatic,
                              fuse_palette,
                              reqcolors,
                              bgcolor,
                              loop_control,
                              start_frame_no,
                              fn_load,
                              context);
    /*
     * Only fall back to single-frame PNG decoding when APNG chunks are
     * absent. If APNG parsing started and returned an error, preserve the
     * error instead of masking it with the non-APNG fallback path.
     */
    if (status != SIXEL_FALSE) {
        goto end;
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = load_png(&pixels,
                      pchunk->buffer,
                      pchunk->size,
                      &frame->width,
                      &frame->height,
                      &frame->palette,
                      &frame->ncolors,
                      fuse_palette ? reqcolors : 0,
                      &frame->pixelformat,
                      bgcolor,
                      &frame->transparent,
                      &cms_applied,
                      enable_cms,
                      pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_frame_set_pixels(frame, pixels);
    if (cms_applied
            && ((frame->pixelformat & SIXEL_FORMATTYPE_PALETTE) == 0)) {
        cms_target_pixelformat = loader_cms_target_pixelformat();
        status = sixel_frame_set_pixelformat(frame, cms_target_pixelformat);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    status = sixel_frame_strip_alpha(frame, bgcolor);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = fn_load(frame, context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);

    return status;
}


static void
sixel_loader_libpng_ref(sixel_loader_component_t *component)
{
    sixel_loader_libpng_component_t *self;

    self = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_libpng_component_t *)component;
    ++self->ref;
}

static void
sixel_loader_libpng_unref(sixel_loader_component_t *component)
{
    sixel_loader_libpng_component_t *self;
    sixel_allocator_t *allocator;

    self = NULL;
    allocator = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_libpng_component_t *)component;
    if (self->ref == 0u) {
        return;
    }

    --self->ref;
    if (self->ref > 0u) {
        return;
    }

    allocator = self->allocator;
    sixel_allocator_free(allocator, self);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
sixel_loader_libpng_setopt(sixel_loader_component_t *component,
                           int option,
                           void const *value)
{
    sixel_loader_libpng_component_t *self;
    int const *flag;
    unsigned char const *color;

    self = NULL;
    flag = NULL;
    color = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libpng_component_t *)component;
    switch (option) {
    case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
        flag = (int const *)value;
        self->fstatic = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_USE_PALETTE:
        flag = (int const *)value;
        self->fuse_palette = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        flag = (int const *)value;
        if (flag != NULL) {
            self->reqcolors = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_BGCOLOR:
        if (value == NULL) {
            self->has_bgcolor = 0;
            return SIXEL_OK;
        }
        color = (unsigned char const *)value;
        self->bgcolor[0] = color[0];
        self->bgcolor[1] = color[1];
        self->bgcolor[2] = color[2];
        self->has_bgcolor = 1;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_LOOP_CONTROL:
        flag = (int const *)value;
        if (flag != NULL) {
            self->loop_control = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_START_FRAME_NO:
        if (value == NULL) {
            self->has_start_frame_no = 0;
            self->start_frame_no = INT_MIN;
            return SIXEL_OK;
        }
        flag = (int const *)value;
        self->start_frame_no = *flag;
        self->has_start_frame_no = 1;
        return SIXEL_OK;
    default:
        return SIXEL_OK;
    }
}

static SIXELSTATUS
sixel_loader_libpng_load(sixel_loader_component_t *component,
                         sixel_chunk_t const *chunk,
                         sixel_load_image_function fn_load,
                         void *context)
{
    sixel_loader_libpng_component_t *self;
    unsigned char *bgcolor;

    self = NULL;
    bgcolor = NULL;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libpng_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    return load_with_libpng(chunk,
                            self->fstatic,
                            self->fuse_palette,
                            self->reqcolors,
                            bgcolor,
                            self->loop_control,
                            self->has_start_frame_no,
                            self->start_frame_no,
                            fn_load,
                            context);
}

static char const *
sixel_loader_libpng_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "libpng";
}

static sixel_loader_component_vtbl_t const g_sixel_loader_libpng_vtbl = {
    sixel_loader_libpng_ref,
    sixel_loader_libpng_unref,
    sixel_loader_libpng_setopt,
    sixel_loader_libpng_load,
    sixel_loader_libpng_name
};

SIXELSTATUS
sixel_loader_libpng_new(sixel_allocator_t *allocator,
                        sixel_loader_component_t **ppcomponent)
{
    sixel_loader_libpng_component_t *self;

    self = NULL;
    if (allocator == NULL || ppcomponent == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppcomponent = NULL;
    self = (sixel_loader_libpng_component_t *)
        sixel_allocator_malloc(allocator, sizeof(*self));
    if (self == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memset(self, 0, sizeof(*self));
    self->base.vtbl = &g_sixel_loader_libpng_vtbl;
    self->allocator = allocator;
    self->ref = 1u;
    self->reqcolors = 256;
    self->start_frame_no = INT_MIN;
    sixel_allocator_ref(allocator);
    *ppcomponent = &self->base;
    return SIXEL_OK;
}

int
loader_can_try_libpng(sixel_chunk_t const *chunk)
{
    if (chunk == NULL) {
        return 0;
    }

    return chunk_is_png(chunk);
}

#else  /* !HAVE_LIBPNG */

/*
 * Provide a dummy symbol so that pedantic compilers do not flag the unit as
 * empty when libpng support is disabled at configure time.
 */
enum { sixel_loader_libpng_placeholder = 0 };

#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_LIBPNG_PLACEHOLDER_UNUSED __attribute__((unused))
#else
# define SIXEL_LIBPNG_PLACEHOLDER_UNUSED
#endif

static void
sixel_loader_libpng_placeholder_function(void)
    SIXEL_LIBPNG_PLACEHOLDER_UNUSED;

static void
sixel_loader_libpng_placeholder_function(void)
{
    /*
     * Tie the placeholder enum to a symbol so MSVC does not warn about an
     * empty translation unit when libpng is disabled.
     */
    (void)sixel_loader_libpng_placeholder;
}

#undef SIXEL_LIBPNG_PLACEHOLDER_UNUSED

#endif  /* HAVE_LIBPNG */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
