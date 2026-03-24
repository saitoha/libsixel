/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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
 * libtiff-backed loader helpers. The implementation stays close to other
 * loader backends so the rest of libsixel continues to operate on decoded
 * RGBA buffers and consistent metadata.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_LIBTIFF

#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include <tiffio.h>

#include <sixel.h>

#include "allocator.h"
#include "cms.h"
#include "chunk.h"
#include "frame.h"
#include "icc-apply.h"
#include "icc-convert.h"
#include "icc-parse.h"
#include "loader-common.h"
#include "loader-libtiff.h"
#include "logger.h"

typedef struct sixel_loader_libtiff_component {
    sixel_loader_component_t base;
    sixel_allocator_t *allocator;
    unsigned int ref;
    int enable_cms;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_control;
    int has_bgcolor;
    unsigned char bgcolor[3];
    int has_start_frame_no;
    int start_frame_no;
} sixel_loader_libtiff_component_t;

typedef struct tiff_memory_chunk {
    unsigned char const *buffer;
    toff_t size;
    toff_t offset;
} tiff_memory_chunk_t;

#if HAVE_LCMS2
/*
 * Convert decoded RGBA pixels from an embedded TIFF ICC profile to sRGB.
 *
 * The alpha channel is preserved while only color channels are transformed.
 * Invalid or unsupported profiles are ignored to preserve compatibility.
 */
static void
tiff_convert_embedded_icc_to_srgb(unsigned char *pixels,
                                  int width,
                                  int height,
                                  void const *profile,
                                  uint32_t profile_length,
                                  uint16_t photometric)
{
    sixel_cms_profile_t * src_profile;
    sixel_cms_profile_t * dst_profile;
    sixel_cms_transform_t * transform;
    sixel_cms_color_space_t src_colorspace;
    size_t pixel_count;
    unsigned char *gray_in;
    unsigned char *rgb_out;
    size_t i;
    int treat_as_gray;
    int treat_as_rgb;

    src_profile = NULL;
    dst_profile = NULL;
    transform = NULL;
    src_colorspace = SIXEL_CMS_COLORSPACE_UNKNOWN;
    pixel_count = 0;
    gray_in = NULL;
    rgb_out = NULL;
    i = 0u;
    treat_as_gray = 0;
    treat_as_rgb = 0;

    if (pixels == NULL || width <= 0 || height <= 0 ||
        profile == NULL || profile_length == 0u) {
        return;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_length);
    if (src_profile == NULL) {
        return;
    }
    src_colorspace = sixel_cms_get_color_space(src_profile);
    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        goto cleanup;
    }

    pixel_count = (size_t)width * (size_t)height;
    switch (photometric) {
    case PHOTOMETRIC_MINISWHITE:
    case PHOTOMETRIC_MINISBLACK:
        treat_as_gray = 1;
        break;
    case PHOTOMETRIC_RGB:
    case PHOTOMETRIC_PALETTE:
    case PHOTOMETRIC_YCBCR:
        treat_as_rgb = 1;
        break;
    default:
        break;
    }

    if (treat_as_gray) {
        if (src_colorspace != SIXEL_CMS_COLORSPACE_GRAY) {
            goto cleanup;
        }
        gray_in = (unsigned char *)malloc(pixel_count);
        rgb_out = (unsigned char *)malloc(pixel_count * 3u);
        if (gray_in == NULL || rgb_out == NULL) {
            goto cleanup;
        }
        for (i = 0u; i < pixel_count; ++i) {
            gray_in[i] = pixels[i * 4u];
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
        for (i = 0u; i < pixel_count; ++i) {
            pixels[i * 4u + 0u] = rgb_out[i * 3u + 0u];
            pixels[i * 4u + 1u] = rgb_out[i * 3u + 1u];
            pixels[i * 4u + 2u] = rgb_out[i * 3u + 2u];
        }
    } else if (treat_as_rgb || photometric == (uint16_t)0xffffu) {
        if (src_colorspace != SIXEL_CMS_COLORSPACE_RGB) {
            goto cleanup;
        }
        transform = sixel_cms_create_transform(src_profile,
                                       SIXEL_CMS_PIXELFORMAT_RGBA_8,
                                       dst_profile,
                                       SIXEL_CMS_PIXELFORMAT_RGBA_8,
                                       SIXEL_CMS_TRANSFORM_COPY_ALPHA);
        if (transform == NULL) {
            goto cleanup;
        }
        sixel_cms_do_transform(transform, pixels, pixels, pixel_count);
    }

cleanup:
    if (rgb_out != NULL) {
        free(rgb_out);
    }
    if (gray_in != NULL) {
        free(gray_in);
    }
    if (transform != NULL) {
        sixel_cms_delete_transform(transform);
    }
    if (dst_profile != NULL) {
        sixel_cms_close_profile(dst_profile);
    }
    if (src_profile != NULL) {
        sixel_cms_close_profile(src_profile);
    }
}
#endif

#if !HAVE_LCMS2
/*
 * no-lcms fallback for RGBA8888 TIFF decode path.
 *
 * sixel_icc_convert_to_srgb_with_pixelformat() does not accept RGBA8888
 * directly, so this helper converts the RGB channels via a temporary RGB888
 * buffer and then writes the transformed RGB values back while preserving
 * the decoded alpha channel.
 */
static void
tiff_convert_embedded_icc_to_srgb_nolcms(unsigned char *pixels,
                                         int width,
                                         int height,
                                         void const *profile,
                                         uint32_t profile_length,
                                         uint16_t photometric)
{
    unsigned char *rgb_pixels;
    size_t pixel_count;
    size_t rgb_bytes;
    size_t i;
    size_t src_offset;
    size_t dst_offset;
    int converted;

    rgb_pixels = NULL;
    pixel_count = 0u;
    rgb_bytes = 0u;
    i = 0u;
    src_offset = 0u;
    dst_offset = 0u;
    converted = 0;

    if (pixels == NULL || width <= 0 || height <= 0 ||
        profile == NULL || profile_length == 0u) {
        return;
    }

    switch (photometric) {
    case PHOTOMETRIC_MINISWHITE:
    case PHOTOMETRIC_MINISBLACK:
    case PHOTOMETRIC_RGB:
    case PHOTOMETRIC_PALETTE:
    case PHOTOMETRIC_YCBCR:
    case (uint16_t)0xffffu:
        break;
    default:
        return;
    }

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / 3u) {
        return;
    }
    rgb_bytes = pixel_count * 3u;

    rgb_pixels = (unsigned char *)malloc(rgb_bytes);
    if (rgb_pixels == NULL) {
        return;
    }

    for (i = 0u; i < pixel_count; ++i) {
        src_offset = i * 4u;
        dst_offset = i * 3u;
        rgb_pixels[dst_offset + 0u] = pixels[src_offset + 0u];
        rgb_pixels[dst_offset + 1u] = pixels[src_offset + 1u];
        rgb_pixels[dst_offset + 2u] = pixels[src_offset + 2u];
    }

    converted = sixel_icc_convert_to_srgb_with_pixelformat(
        rgb_pixels,
        width,
        height,
        SIXEL_PIXELFORMAT_RGB888,
        (unsigned char const *)profile,
        (size_t)profile_length);
    if (converted) {
        for (i = 0u; i < pixel_count; ++i) {
            src_offset = i * 3u;
            dst_offset = i * 4u;
            pixels[dst_offset + 0u] = rgb_pixels[src_offset + 0u];
            pixels[dst_offset + 1u] = rgb_pixels[src_offset + 1u];
            pixels[dst_offset + 2u] = rgb_pixels[src_offset + 2u];
        }
    }

    free(rgb_pixels);
}
#endif

static double
tiff_clamp_unit(double value)
{
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

static int
tiff_orientation_is_supported(uint16_t orientation)
{
    switch (orientation) {
    case ORIENTATION_TOPLEFT:
    case ORIENTATION_TOPRIGHT:
    case ORIENTATION_BOTRIGHT:
    case ORIENTATION_BOTLEFT:
        return 1;
    default:
        return 0;
    }
}

static void
tiff_map_scanline_coordinates(uint16_t orientation,
                              uint32_t width,
                              uint32_t height,
                              uint32_t src_x,
                              uint32_t src_y,
                              uint32_t *dst_x,
                              uint32_t *dst_y)
{
    uint32_t mapped_x;
    uint32_t mapped_y;

    mapped_x = src_x;
    mapped_y = src_y;
    switch (orientation) {
    case ORIENTATION_TOPRIGHT:
        mapped_x = width - 1u - src_x;
        break;
    case ORIENTATION_BOTRIGHT:
        mapped_x = width - 1u - src_x;
        mapped_y = height - 1u - src_y;
        break;
    case ORIENTATION_BOTLEFT:
        mapped_y = height - 1u - src_y;
        break;
    case ORIENTATION_TOPLEFT:
    default:
        break;
    }

    if (dst_x != NULL) {
        *dst_x = mapped_x;
    }
    if (dst_y != NULL) {
        *dst_y = mapped_y;
    }
}

static int
tiff_convert_embedded_icc_to_srgb_float32(float *pixels,
                                          int width,
                                          int height,
                                          void const *profile,
                                          uint32_t profile_length)
{
    int converted;
    int parsed;
    size_t pixel_count;
    sixel_icc_profile_t parsed_profile;

    converted = 0;
    parsed = 0;
    pixel_count = 0u;
    memset(&parsed_profile, 0, sizeof(parsed_profile));
    if (pixels == NULL || width <= 0 || height <= 0 ||
        profile == NULL || profile_length == 0u) {
        return 0;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return 0;
    }
    pixel_count = (size_t)width * (size_t)height;

    converted = sixel_icc_convert_to_srgb_with_pixelformat(
        (unsigned char *)pixels,
        width,
        height,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        (unsigned char const *)profile,
        (size_t)profile_length);
    if (!converted &&
        sixel_icc_parse_profile(profile, (size_t)profile_length, &parsed_profile)) {
        parsed = 1;
        if (parsed_profile.kind == SIXEL_ICC_PROFILE_KIND_RGB
            || parsed_profile.kind == SIXEL_ICC_PROFILE_KIND_GRAY) {
            converted = sixel_icc_apply_rgb_float32(pixels,
                                                    pixel_count,
                                                    &parsed_profile);
        }
    }

    if (parsed) {
        sixel_icc_profile_destroy(&parsed_profile);
    }
    return converted;
}

static SIXELSTATUS
tiff_convert_rgbf32_gamma_to_linear(float *pixels,
                                    size_t pixel_count)
{
    size_t float_bytes;

    float_bytes = 0u;
    if (pixels == NULL || pixel_count == 0u) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    float_bytes = pixel_count * 3u * sizeof(float);

    return sixel_helper_convert_colorspace((unsigned char *)pixels,
                                           float_bytes,
                                           SIXEL_PIXELFORMAT_RGBFLOAT32,
                                           SIXEL_COLORSPACE_GAMMA,
                                           SIXEL_COLORSPACE_LINEAR);
}

static tsize_t
tiff_memory_read(thandle_t handle, tdata_t data, tsize_t length)
{
    tiff_memory_chunk_t *chunk;
    tsize_t to_copy;
    toff_t available;

    chunk = (tiff_memory_chunk_t *)handle;
    if (chunk->offset >= chunk->size) {
        return 0;
    }

    available = chunk->size - chunk->offset;
    to_copy = length;
    if ((toff_t)to_copy > available) {
        to_copy = (tsize_t)available;
    }

    if (to_copy > 0) {
        memcpy(data, chunk->buffer + chunk->offset, to_copy);
        chunk->offset += (toff_t)to_copy;
    }

    return to_copy;
}

static tsize_t
tiff_memory_write(thandle_t handle, tdata_t data, tsize_t length)
{
    (void)handle;
    (void)data;
    (void)length;

    return 0;
}

static toff_t
tiff_memory_seek(thandle_t handle, toff_t offset, int whence)
{
    tiff_memory_chunk_t *chunk;
    toff_t new_offset;

    chunk = (tiff_memory_chunk_t *)handle;
    switch (whence) {
    case SEEK_SET:
        new_offset = offset;
        break;
    case SEEK_CUR:
        if (offset > chunk->size - chunk->offset) {
            new_offset = chunk->size;
        } else {
            new_offset = chunk->offset + offset;
        }
        break;
    case SEEK_END:
        if (offset > chunk->size) {
            new_offset = 0;
        } else {
            new_offset = chunk->size - offset;
        }
        break;
    default:
        return (toff_t)-1;
    }

    if (new_offset > chunk->size) {
        new_offset = chunk->size;
    }

    chunk->offset = new_offset;
    return chunk->offset;
}

static int
tiff_memory_close(thandle_t handle)
{
    (void)handle;
    return 0;
}

static toff_t
tiff_memory_size(thandle_t handle)
{
    tiff_memory_chunk_t *chunk;

    chunk = (tiff_memory_chunk_t *)handle;
    return chunk->size;
}

static int
tiff_memory_map(thandle_t handle, tdata_t *data, toff_t *size)
{
    (void)handle;
    (void)data;
    (void)size;

    return 0;
}

static void
tiff_memory_unmap(thandle_t handle, tdata_t data, toff_t size)
{
    (void)handle;
    (void)data;
    (void)size;
}

/*
 * Decode selected high-precision TIFF layouts directly into RGB float32.
 *
 * Supported source layouts:
 *   - RGB/Gray integer (16-bit, contiguous, no alpha)
 *   - RGB/Gray float32 (SampleFormat=IEEEFP, contiguous, no alpha)
 *
 * The function returns SIXEL_FALSE for unsupported combinations so callers can
 * safely fall back to TIFFReadRGBAImageOriented().
 */
static SIXELSTATUS
tiff_try_load_high_precision(unsigned char      /* out */ **result,
                             TIFF               /* in */  *tif,
                             uint32_t           /* in */  width,
                             uint32_t           /* in */  height,
                             int                /* out */ *ppixelformat,
                             void const         /* in */  *icc_profile,
                             uint32_t           /* in */  icc_profile_length,
                             uint16_t           /* in */  photometric,
                             sixel_allocator_t  /* in */  *allocator)
{
    SIXELSTATUS status;
    tsize_t scanline_size;
    unsigned char *scanline;
    float *float_pixels;
    uint16_t bits_per_sample;
    uint16_t samples_per_pixel;
    uint16_t sample_format;
    uint16_t planar_config;
    uint16_t orientation;
    uint16_t extrasamples;
    uint16_t *extrasample_info;
    size_t pixel_count;
    size_t float_bytes;
    size_t row_sample_count;
    size_t required_scanline_bytes;
    uint32_t y;
    uint32_t x;
    uint32_t dst_x;
    uint32_t dst_y;
    size_t src_base;
    size_t dst_base;
    unsigned char const *row_u8;
    uint16_t const *row_u16;
    float const *row_f32;
    double unit;
    double rgb_unit[3];
    int cms_converted;
    enum {
        TIFF_DECODE_UNSUPPORTED = 0,
        TIFF_DECODE_RGB_UINT,
        TIFF_DECODE_GRAY_UINT,
        TIFF_DECODE_RGB_FLOAT32,
        TIFF_DECODE_GRAY_FLOAT32
    } decode_mode;

    status = SIXEL_FALSE;
    scanline_size = 0;
    scanline = NULL;
    float_pixels = NULL;
    bits_per_sample = 1u;
    samples_per_pixel = 1u;
    sample_format = SAMPLEFORMAT_UINT;
    planar_config = PLANARCONFIG_CONTIG;
    orientation = ORIENTATION_TOPLEFT;
    extrasamples = 0u;
    extrasample_info = NULL;
    pixel_count = 0u;
    float_bytes = 0u;
    row_sample_count = 0u;
    required_scanline_bytes = 0u;
    y = 0u;
    x = 0u;
    dst_x = 0u;
    dst_y = 0u;
    src_base = 0u;
    dst_base = 0u;
    row_u8 = NULL;
    row_u16 = NULL;
    row_f32 = NULL;
    unit = 0.0;
    rgb_unit[0] = 0.0;
    rgb_unit[1] = 0.0;
    rgb_unit[2] = 0.0;
    cms_converted = 0;
    decode_mode = TIFF_DECODE_UNSUPPORTED;

    if (result == NULL || ppixelformat == NULL ||
        tif == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (width == 0u || height == 0u) {
        return SIXEL_FALSE;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    float_bytes = pixel_count * 3u * sizeof(float);

    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sample_format);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planar_config);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &orientation);

    if (sample_format == SAMPLEFORMAT_VOID) {
        sample_format = SAMPLEFORMAT_UINT;
    }
    if (bits_per_sample <= 8u &&
        sample_format != SAMPLEFORMAT_IEEEFP) {
        return SIXEL_FALSE;
    }
    if (planar_config != PLANARCONFIG_CONTIG ||
        TIFFIsTiled(tif) ||
        !tiff_orientation_is_supported(orientation)) {
        return SIXEL_FALSE;
    }

    if (TIFFGetFieldDefaulted(tif,
                              TIFFTAG_EXTRASAMPLES,
                              &extrasamples,
                              &extrasample_info) == 1 &&
        extrasamples > 0u) {
        return SIXEL_FALSE;
    }

    switch (photometric) {
    case PHOTOMETRIC_RGB:
        if (samples_per_pixel > 3u) {
            return SIXEL_FALSE;
        }
        if (sample_format == SAMPLEFORMAT_IEEEFP && bits_per_sample == 32u &&
            samples_per_pixel >= 3u) {
            decode_mode = TIFF_DECODE_RGB_FLOAT32;
        } else if (sample_format == SAMPLEFORMAT_UINT &&
                   bits_per_sample == 16u &&
                   samples_per_pixel >= 3u) {
            decode_mode = TIFF_DECODE_RGB_UINT;
        }
        break;
    case PHOTOMETRIC_MINISWHITE:
    case PHOTOMETRIC_MINISBLACK:
        if (samples_per_pixel > 1u) {
            return SIXEL_FALSE;
        }
        if (sample_format == SAMPLEFORMAT_IEEEFP && bits_per_sample == 32u) {
            decode_mode = TIFF_DECODE_GRAY_FLOAT32;
        } else if (sample_format == SAMPLEFORMAT_UINT &&
                   bits_per_sample == 16u) {
            decode_mode = TIFF_DECODE_GRAY_UINT;
        }
        break;
    default:
        break;
    }
    if (decode_mode == TIFF_DECODE_UNSUPPORTED) {
        return SIXEL_FALSE;
    }

    if ((size_t)width > SIZE_MAX / (size_t)samples_per_pixel) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    row_sample_count = (size_t)width * (size_t)samples_per_pixel;
    switch (decode_mode) {
    case TIFF_DECODE_RGB_FLOAT32:
    case TIFF_DECODE_GRAY_FLOAT32:
        if (row_sample_count > SIZE_MAX / sizeof(float)) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        required_scanline_bytes = row_sample_count * sizeof(float);
        break;
    case TIFF_DECODE_RGB_UINT:
    case TIFF_DECODE_GRAY_UINT:
        if (bits_per_sample == 16u) {
            if (row_sample_count > SIZE_MAX / sizeof(uint16_t)) {
                return SIXEL_BAD_INTEGER_OVERFLOW;
            }
            required_scanline_bytes = row_sample_count * sizeof(uint16_t);
        } else {
            required_scanline_bytes = row_sample_count;
        }
        break;
    default:
        return SIXEL_FALSE;
    }

    scanline_size = TIFFScanlineSize(tif);
    if (scanline_size <= 0 ||
        required_scanline_bytes > (size_t)scanline_size) {
        return SIXEL_FALSE;
    }

    scanline = (unsigned char *)sixel_allocator_malloc(allocator,
                                                       (size_t)scanline_size);
    if (scanline == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    float_pixels = (float *)sixel_allocator_malloc(allocator, float_bytes);
    if (float_pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    for (y = 0u; y < height; ++y) {
        if (TIFFReadScanline(tif, scanline, y, 0) < 0) {
            sixel_helper_set_additional_message(
                "load_tiff: TIFFReadScanline() failed.");
            status = SIXEL_TIFF_ERROR;
            goto cleanup;
        }
        row_u8 = (unsigned char const *)scanline;
        row_u16 = (uint16_t const *)scanline;
        row_f32 = (float const *)scanline;

        for (x = 0u; x < width; ++x) {
            src_base = (size_t)x * (size_t)samples_per_pixel;
            tiff_map_scanline_coordinates(orientation,
                                          width,
                                          height,
                                          x,
                                          y,
                                          &dst_x,
                                          &dst_y);
            dst_base = ((size_t)dst_y * (size_t)width + (size_t)dst_x) * 3u;

            switch (decode_mode) {
            case TIFF_DECODE_RGB_UINT:
                if (bits_per_sample == 16u) {
                    rgb_unit[0] = (double)row_u16[src_base + 0u] / 65535.0;
                    rgb_unit[1] = (double)row_u16[src_base + 1u] / 65535.0;
                    rgb_unit[2] = (double)row_u16[src_base + 2u] / 65535.0;
                } else {
                    rgb_unit[0] = (double)row_u8[src_base + 0u] / 255.0;
                    rgb_unit[1] = (double)row_u8[src_base + 1u] / 255.0;
                    rgb_unit[2] = (double)row_u8[src_base + 2u] / 255.0;
                }
                break;
            case TIFF_DECODE_GRAY_UINT:
                if (bits_per_sample == 16u) {
                    unit = (double)row_u16[src_base] / 65535.0;
                } else {
                    unit = (double)row_u8[src_base] / 255.0;
                }
                if (photometric == PHOTOMETRIC_MINISWHITE) {
                    unit = 1.0 - unit;
                }
                rgb_unit[0] = unit;
                rgb_unit[1] = unit;
                rgb_unit[2] = unit;
                break;
            case TIFF_DECODE_RGB_FLOAT32:
                rgb_unit[0] = (double)row_f32[src_base + 0u];
                rgb_unit[1] = (double)row_f32[src_base + 1u];
                rgb_unit[2] = (double)row_f32[src_base + 2u];
                break;
            case TIFF_DECODE_GRAY_FLOAT32:
                unit = (double)row_f32[src_base];
                if (photometric == PHOTOMETRIC_MINISWHITE) {
                    unit = 1.0 - unit;
                }
                rgb_unit[0] = unit;
                rgb_unit[1] = unit;
                rgb_unit[2] = unit;
                break;
            default:
                status = SIXEL_FALSE;
                goto cleanup;
            }

            float_pixels[dst_base + 0u] = (float)tiff_clamp_unit(rgb_unit[0]);
            float_pixels[dst_base + 1u] = (float)tiff_clamp_unit(rgb_unit[1]);
            float_pixels[dst_base + 2u] = (float)tiff_clamp_unit(rgb_unit[2]);
        }
    }

    if (icc_profile != NULL && icc_profile_length > 0u &&
        photometric != PHOTOMETRIC_CIELAB) {
        cms_converted = tiff_convert_embedded_icc_to_srgb_float32(
            float_pixels,
            (int)width,
            (int)height,
            icc_profile,
            icc_profile_length);
    }

    if (cms_converted) {
        status = tiff_convert_rgbf32_gamma_to_linear(float_pixels, pixel_count);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }

    *result = (unsigned char *)float_pixels;
    *ppixelformat = cms_converted
                  ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                  : SIXEL_PIXELFORMAT_RGBFLOAT32;
    float_pixels = NULL;
    status = SIXEL_OK;

cleanup:
    if (scanline != NULL) {
        sixel_allocator_free(allocator, scanline);
    }
    if (status != SIXEL_OK && float_pixels != NULL) {
        sixel_allocator_free(allocator, float_pixels);
    }

    return status;
}

/*
 * Decode a TIFF stream into an RGBA buffer.
 *
 * The memory-backed TIFF client uses the following flow:
 *
 *    +-----------+     +-----------------+     +--------------------+
 *    | TIFF data | --> | libtiff decode | --> | RGBA pixel buffer  |
 *    +-----------+     +-----------------+     +--------------------+
 */
static SIXELSTATUS
load_tiff(unsigned char      /* out */ **result,
          unsigned char      /* in */  *buffer,
          size_t             /* in */  size,
          int                /* out */ *pwidth,
          int                /* out */ *pheight,
          int                /* out */ *ppixelformat,
          sixel_allocator_t  /* in */  *allocator,
          int                /* in */  enable_cms)
{
    SIXELSTATUS status;
    TIFF *tif;
    tiff_memory_chunk_t chunk;
    uint32_t width;
    uint32_t height;
    uint32_t *raster;
    size_t pixel_count;
    size_t pixel_bytes;
    size_t index;
    unsigned char *pixels;
    uint32_t pixel;
    size_t offset;
    SIXELSTATUS high_precision_status;
    uint32_t icc_profile_length;
    void *icc_profile;
    uint16_t photometric;

    status = SIXEL_TIFF_ERROR;
    tif = NULL;
    raster = NULL;
    pixels = NULL;
    width = 0;
    height = 0;
    pixel_count = 0;
    pixel_bytes = 0;
    high_precision_status = SIXEL_FALSE;
    icc_profile_length = 0u;
    icc_profile = NULL;
    photometric = (uint16_t)0xffffu;

    chunk.buffer = buffer;
    chunk.size = (toff_t)size;
    chunk.offset = 0;

    tif = TIFFClientOpen("sixel-memory",
                         "r",
                         (thandle_t)&chunk,
                         tiff_memory_read,
                         tiff_memory_write,
                         tiff_memory_seek,
                         tiff_memory_close,
                         tiff_memory_size,
                         tiff_memory_map,
                         tiff_memory_unmap);
    if (tif == NULL) {
        sixel_helper_set_additional_message(
            "load_tiff: TIFFClientOpen() failed.");
        goto cleanup;
    }

    if (!TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width) ||
        !TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height)) {
        sixel_helper_set_additional_message(
            "load_tiff: missing image dimensions.");
        goto cleanup;
    }

    if (width == 0 || height == 0) {
        sixel_helper_set_additional_message(
            "load_tiff: invalid image dimensions.");
        goto cleanup;
    }

    if (width > INT_MAX || height > INT_MAX) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    pixel_count = (size_t)width * (size_t)height;

    (void)TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    if (photometric == PHOTOMETRIC_CIELAB) {
        float cie_ref_white[2];

        /* Align Lab decode baseline with CoreGraphics/ImageIO behavior. */
        cie_ref_white[0] = 0.34567f;
        cie_ref_white[1] = 0.35850f;
        (void)TIFFSetField(tif, TIFFTAG_WHITEPOINT, cie_ref_white);
    }
    if (enable_cms) {
        (void)TIFFGetField(tif,
                           TIFFTAG_ICCPROFILE,
                           &icc_profile_length,
                           &icc_profile);
    }

    high_precision_status = tiff_try_load_high_precision(&pixels,
                                                         tif,
                                                         width,
                                                         height,
                                                         ppixelformat,
                                                         enable_cms
                                                            ? icc_profile
                                                            : NULL,
                                                         enable_cms
                                                            ? icc_profile_length
                                                            : 0u,
                                                         photometric,
                                                         allocator);
    if (high_precision_status == SIXEL_OK) {
        *result = pixels;
        *pwidth = (int)width;
        *pheight = (int)height;
        status = SIXEL_OK;
        goto cleanup;
    }
    if (high_precision_status != SIXEL_FALSE) {
        status = high_precision_status;
        goto cleanup;
    }

    if (pixel_count > SIZE_MAX / sizeof(uint32_t)) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }

    raster = (uint32_t *)sixel_allocator_malloc(
        allocator,
        pixel_count * sizeof(uint32_t));
    if (raster == NULL) {
        sixel_helper_set_additional_message(
            "load_tiff: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    if (!TIFFReadRGBAImageOriented(
            tif, width, height, raster, ORIENTATION_TOPLEFT, 0)) {
        sixel_helper_set_additional_message(
            "load_tiff: TIFFReadRGBAImageOriented() failed.");
        goto cleanup;
    }

    if (pixel_count > SIZE_MAX / 4) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    pixel_bytes = pixel_count * 4;

    pixels = (unsigned char *)sixel_allocator_malloc(allocator, pixel_bytes);
    if (pixels == NULL) {
        sixel_helper_set_additional_message(
            "load_tiff: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    for (index = 0; index < pixel_count; ++index) {
        pixel = raster[index];
        offset = index * 4;
        pixels[offset + 0] = TIFFGetR(pixel);
        pixels[offset + 1] = TIFFGetG(pixel);
        pixels[offset + 2] = TIFFGetB(pixel);
        pixels[offset + 3] = TIFFGetA(pixel);
    }

#if HAVE_LCMS2
    if (enable_cms && icc_profile != NULL && icc_profile_length > 0u) {
        tiff_convert_embedded_icc_to_srgb(pixels,
                                          (int)width,
                                          (int)height,
                                          icc_profile,
                                          icc_profile_length,
                                          photometric);
    }
#else
    if (enable_cms && icc_profile != NULL && icc_profile_length > 0u) {
        tiff_convert_embedded_icc_to_srgb_nolcms(pixels,
                                                 (int)width,
                                                 (int)height,
                                                 icc_profile,
                                                 icc_profile_length,
                                                 photometric);
    }
#endif

    *result = pixels;
    *pwidth = (int)width;
    *pheight = (int)height;
    *ppixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    status = SIXEL_OK;

cleanup:
    if (raster != NULL) {
        sixel_allocator_free(allocator, raster);
    }
    if (status != SIXEL_OK && pixels != NULL) {
        sixel_allocator_free(allocator, pixels);
        pixels = NULL;
        *result = NULL;
    }
    if (tif != NULL) {
        TIFFClose(tif);
    }

    return status;
}

/*
 * Dedicated libtiff loader wiring for the common loader callbacks.
 */
static SIXELSTATUS
load_with_libtiff(
    sixel_chunk_t const       /* in */     *pchunk,
    int                       /* in */     enable_cms,
    int                       /* in */     fstatic,
    int                       /* in */     fuse_palette,
    int                       /* in */     reqcolors,
    unsigned char             /* in */     *bgcolor,
    int                       /* in */     loop_control,
    int                       /* in */     start_frame_no_set,
    int                       /* in */     start_frame_no,
    sixel_load_image_function /* in */     fn_load,
    void                      /* in/out */ *context)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char *pixels;
    int pixelformat;

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;
    pixelformat = SIXEL_PIXELFORMAT_RGB888;

    (void)fstatic;
    (void)fuse_palette;
    (void)reqcolors;
    (void)loop_control;
    (void)start_frame_no_set;
    (void)start_frame_no;

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = load_tiff(&pixels,
                       pchunk->buffer,
                       pchunk->size,
                       &frame->width,
                       &frame->height,
                       &pixelformat,
                       pchunk->allocator,
                       enable_cms);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_set_pixelformat(frame, pixelformat);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        sixel_frame_set_pixels_float32(frame, (float *)pixels);
    } else {
        sixel_frame_set_pixels(frame, pixels);
    }

    if (!SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        status = sixel_frame_strip_alpha(frame, bgcolor);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
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
sixel_loader_libtiff_ref(sixel_loader_component_t *component)
{
    sixel_loader_libtiff_component_t *self;

    self = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_libtiff_component_t *)component;
    ++self->ref;
}

static void
sixel_loader_libtiff_unref(sixel_loader_component_t *component)
{
    sixel_loader_libtiff_component_t *self;
    sixel_allocator_t *allocator;

    self = NULL;
    allocator = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_libtiff_component_t *)component;
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
sixel_loader_libtiff_setopt(sixel_loader_component_t *component,
                            int option,
                            void const *value)
{
    sixel_loader_libtiff_component_t *self;
    int const *flag;
    unsigned char const *color;

    self = NULL;
    flag = NULL;
    color = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libtiff_component_t *)component;
    switch (option) {
    case SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS:
        flag = (int const *)value;
        self->enable_cms = (flag != NULL && *flag != 0) ? 1 : 0;
        return SIXEL_OK;
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
sixel_loader_libtiff_load(sixel_loader_component_t *component,
                          sixel_chunk_t const *chunk,
                          sixel_load_image_function fn_load,
                          void *context)
{
    sixel_loader_libtiff_component_t *self;
    unsigned char *bgcolor;

    self = NULL;
    bgcolor = NULL;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libtiff_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    return load_with_libtiff(chunk,
                             self->enable_cms,
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
sixel_loader_libtiff_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "libtiff";
}

static sixel_loader_component_vtbl_t const g_sixel_loader_libtiff_vtbl = {
    sixel_loader_libtiff_ref,
    sixel_loader_libtiff_unref,
    sixel_loader_libtiff_setopt,
    sixel_loader_libtiff_load,
    sixel_loader_libtiff_name
};

SIXELSTATUS
sixel_loader_libtiff_new(sixel_allocator_t *allocator,
                         sixel_loader_component_t **ppcomponent)
{
    sixel_loader_libtiff_component_t *self;

    self = NULL;
    if (allocator == NULL || ppcomponent == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppcomponent = NULL;
    self = (sixel_loader_libtiff_component_t *)
        sixel_allocator_malloc(allocator, sizeof(*self));
    if (self == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memset(self, 0, sizeof(*self));
    self->base.vtbl = &g_sixel_loader_libtiff_vtbl;
    self->allocator = allocator;
    self->ref = 1u;
    self->enable_cms = 0;
    self->reqcolors = 256;
    self->start_frame_no = INT_MIN;
    sixel_allocator_ref(allocator);
    *ppcomponent = &self->base;
    return SIXEL_OK;
}

int
loader_can_try_libtiff(sixel_chunk_t const *chunk)
{
    if (chunk == NULL) {
        return 0;
    }

    return chunk_is_tiff(chunk);
}

#else  /* !HAVE_LIBTIFF */

/*
 * Provide a dummy symbol so that pedantic compilers do not flag the unit as
 * empty when libtiff support is disabled at configure time.
 */
enum { sixel_loader_libtiff_placeholder = 0 };

#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_LIBTIFF_PLACEHOLDER_UNUSED __attribute__((unused))
#else
# define SIXEL_LIBTIFF_PLACEHOLDER_UNUSED
#endif

static void
sixel_loader_libtiff_placeholder_function(void)
    SIXEL_LIBTIFF_PLACEHOLDER_UNUSED;

static void
sixel_loader_libtiff_placeholder_function(void)
{
    /*
     * Tie the placeholder enum to a symbol so MSVC does not warn about an
     * empty translation unit when libtiff is disabled.
     */
    (void)sixel_loader_libtiff_placeholder;
}

#undef SIXEL_LIBTIFF_PLACEHOLDER_UNUSED

#endif  /* HAVE_LIBTIFF */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
