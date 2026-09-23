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
 * loader backends so the rest of libsixel receives decoded byte or float
 * buffers with consistent metadata.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_LIBTIFF

#include <stdio.h>
#include <stdlib.h>
#include <float.h>

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
#if HAVE_MATH_H
# include <math.h>
#endif

#include <tiffio.h>

#include <sixel.h>

#include "allocator.h"
#include "cms.h"
#include "colorspace.h"
#include "chunk-view.h"
#include "frame-private.h"
#include "frame-factory.h"
#include "loader-common.h"
#include "loader-libtiff.h"
#include "timeline-logger.h"

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

static int
loader_can_try_libtiff(sixel_chunk_t const *chunk);

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
 * Optional CMS failures retain the complete decoded source, including alpha.
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
    unsigned char *rgba_out;
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
    rgba_out = NULL;
    i = 0u;
    treat_as_gray = 0;
    treat_as_rgb = 0;

    if (pixels == NULL || width <= 0 || height <= 0 ||
        profile == NULL || profile_length == 0u) {
        return;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / 4u) {
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
        /* A failed apply may have written only a prefix of this scratch. */
        if (!sixel_cms_do_transform(transform, gray_in, rgb_out,
                                    pixel_count)) {
            goto cleanup;
        }
        for (i = 0u; i < pixel_count; ++i) {
            pixels[i * 4u + 0u] = rgb_out[i * 3u + 0u];
            pixels[i * 4u + 1u] = rgb_out[i * 3u + 1u];
            pixels[i * 4u + 2u] = rgb_out[i * 3u + 2u];
        }
    } else if (treat_as_rgb || photometric == (uint16_t)0xffffu) {
        if (src_colorspace != SIXEL_CMS_COLORSPACE_RGB) {
            goto cleanup;
        }
        /* Keep the existing in-place CMS call inside private storage so a
         * partial failure cannot mix source and transformed caller pixels. */
        rgba_out = (unsigned char *)malloc(pixel_count * 4u);
        if (rgba_out == NULL) {
            goto cleanup;
        }
        memcpy(rgba_out, pixels, pixel_count * 4u);
        transform = sixel_cms_create_transform(src_profile,
                                       SIXEL_CMS_PIXELFORMAT_RGBA_8,
                                       dst_profile,
                                       SIXEL_CMS_PIXELFORMAT_RGBA_8,
                                       SIXEL_CMS_TRANSFORM_COPY_ALPHA);
        if (transform == NULL) {
            goto cleanup;
        }
        if (!sixel_cms_do_transform(transform, rgba_out, rgba_out,
                                    pixel_count)) {
            goto cleanup;
        }
        /* Only color results are committed. Preserve original alpha even
         * if a successful backend disregards COPY_ALPHA in its scratch. */
        for (i = 0u; i < pixel_count; ++i) {
            pixels[i * 4u + 0u] = rgba_out[i * 4u + 0u];
            pixels[i * 4u + 1u] = rgba_out[i * 4u + 1u];
            pixels[i * 4u + 2u] = rgba_out[i * 4u + 2u];
        }
    }

cleanup:
    if (rgba_out != NULL) {
        free(rgba_out);
    }
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

static int
tiff_photometric_allows_embedded_icc(uint16_t photometric,
                                     int high_precision_path)
{
    if (photometric == PHOTOMETRIC_LOGLUV) {
        /* Decoded native XYZ is neither device RGB nor ICC D50 PCS. */
        return 0;
    }
    if (photometric == PHOTOMETRIC_CIELAB) {
        return high_precision_path ? 1 : 0;
    }
    if (high_precision_path) {
        return 1;
    }

    switch (photometric) {
    case PHOTOMETRIC_MINISWHITE:
    case PHOTOMETRIC_MINISBLACK:
    case PHOTOMETRIC_RGB:
    case PHOTOMETRIC_PALETTE:
    case PHOTOMETRIC_YCBCR:
    case (uint16_t)0xffffu:
        return 1;
    default:
        break;
    }
    return 0;
}

#if !HAVE_LCMS2
/*
 * no-lcms fallback for RGBA8888 TIFF decode path.
 *
 * sixel_cms_convert_to_srgb_with_profile_bytes() does not accept RGBA8888
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
    sixel_cms_profile_t *source;
    sixel_cms_color_space_t source_space;
    sixel_cms_color_space_t expected;
    unsigned char *rgb_pixels;
    size_t pixel_count;
    size_t rgb_bytes;
    size_t i;
    size_t src_offset;
    size_t dst_offset;
    int converted;

    source = NULL;
    source_space = SIXEL_CMS_COLORSPACE_UNKNOWN;
    expected = SIXEL_CMS_COLORSPACE_RGB;
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

    if (!tiff_photometric_allows_embedded_icc(photometric, 0)) {
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

    /* Expanded Gray bytes still describe a Gray source. Match the native
     * helper's source/profile guard before using the generic RGB buffer. */
    if (photometric == PHOTOMETRIC_MINISWHITE ||
        photometric == PHOTOMETRIC_MINISBLACK) {
        expected = SIXEL_CMS_COLORSPACE_GRAY;
    }
    source = sixel_cms_open_profile_from_mem(profile, profile_length);
    if (source == NULL) {
        return;
    }
    source_space = sixel_cms_get_color_space(source);
    sixel_cms_close_profile(source);
    if (source_space != expected) {
        return;
    }

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

    converted = sixel_cms_convert_to_srgb_with_profile_bytes(
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

static double
tiff_clamp_lab_ab(double value)
{
    if (value < -1.5) {
        return -1.5;
    }
    if (value > 1.5) {
        return 1.5;
    }
    return value;
}

static double
tiff_normalize_lab_l_sample(double sample)
{
#if HAVE_MATH_H
    if (!isfinite(sample)) {
        return 0.0;
    }
#endif
    if (sample > 1.0) {
        sample /= 100.0;
    }
    return tiff_clamp_unit(sample);
}

static double
tiff_normalize_lab_ab_sample(double sample)
{
#if HAVE_MATH_H
    if (!isfinite(sample)) {
        return 0.0;
    }
#endif
    if (sample < -1.5 || sample > 1.5) {
        sample /= 128.0;
    }
    return tiff_clamp_lab_ab(sample);
}

static double
tiff_decode_lab_u16_l(uint16_t value)
{
    return tiff_clamp_unit((double)value / 65535.0);
}

static double
tiff_decode_lab_u16_ab(uint16_t value)
{
    int32_t signed_value;

    /* TIFF CIELAB16 stores physical a/b times 256 as signed words.
     * Widen before subtracting to avoid implementation-defined narrowing.
     * The shared Lab facade represents physical a/b divided by 128. */
    signed_value = (int32_t)value;
    if (value >= 32768u) {
        signed_value -= 65536;
    }
    return (double)signed_value / 32768.0;
}

/* ICC Lab8 uses offset binary chroma. ITU-T T.42 uses different ranges
 * and offsets; RFC 3949 section 6.2.3 permits a six-value Decode override.
 * Keep physical Lab here, then normalize once for the common CMS facade. */
static int
tiff_lab8_ranges(TIFF *tif, uint16_t photometric, double ranges[6])
{
    float *values;
    uint16_t count;
    size_t i;

    ranges[0] = 0.0;
    ranges[1] = 100.0;
    ranges[2] = -128.0;
    ranges[3] = 127.0;
    ranges[4] = -128.0;
    ranges[5] = 127.0;
    if (photometric != PHOTOMETRIC_ITULAB) {
        return 1;
    }
    ranges[2] = -21760.0 / 255.0;
    ranges[3] = 21590.0 / 255.0;
    ranges[4] = -19200.0 / 255.0;
    ranges[5] = 31800.0 / 255.0;
    values = NULL;
    count = 0u;
    if (TIFFGetField(tif, TIFFTAG_DECODE, &count, &values)) {
        if (count != 6u || values == NULL) {
            return 0;
        }
        for (i = 0u; i < 6u; ++i) {
            if (!isfinite(values[i])) {
                return 0;
            }
            ranges[i] = values[i];
        }
        for (i = 0u; i < 6u; i += 2u) {
            if (ranges[i] >= ranges[i + 1u]) {
                return 0;
            }
        }
        return 2;
    }
    return 1;
}

/* Without an applicable profile, ICC/ITU Lab still requires a D50 color
 * interpretation. Express normalized Lab in the common D50 PCS, then use
 * the analytic linear-sRGB definition (Bradford adaptation included).
 * This is format decoding, independent of the selected ICC engine; it
 * never opens a profile or silently substitutes a different CMS backend.
 * The older CIELAB16/float fallback retains its established D65 contract. */
static SIXELSTATUS
tiff_lab8_to_linear(float *pixels, size_t count)
{
    static double const xy[8] = {
        0.3127, 0.3290, 0.64, 0.33, 0.30, 0.60, 0.15, 0.06
    };
    static double const gamma[3] = {1.0, 1.0, 1.0};
    static double const white[3] = {0.9642, 1.0, 0.8249};
    sixel_color_def_t *linear;
    double f[3];
    double xyz[3];
    double rgb[3];
    size_t i;
    size_t c;
    SIXELSTATUS status;

    linear = sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_POWER, gamma);
    if (linear == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    status = SIXEL_OK;
    for (i = 0u; i < count; ++i) {
        f[1] = (100.0 * pixels[i * 3u] + 16.0) / 116.0;
        f[0] = f[1] + 128.0 * pixels[i * 3u + 1u] / 500.0;
        f[2] = f[1] - 128.0 * pixels[i * 3u + 2u] / 200.0;
        for (c = 0u; c < 3u; ++c) {
            xyz[c] = white[c] * (f[c] > 6.0 / 29.0
                ? f[c] * f[c] * f[c]
                : (f[c] - 4.0 / 29.0) * 108.0 / 841.0);
        }
        if (!sixel_cdef_from_xyz(linear, xyz, rgb, 0) ||
            fabs(rgb[0]) > FLT_MAX || fabs(rgb[1]) > FLT_MAX ||
            fabs(rgb[2]) > FLT_MAX) {
            status = SIXEL_TIFF_ERROR;
            break;
        }
        for (c = 0u; c < 3u; ++c) {
            pixels[i * 3u + c] = (float)rgb[c];
        }
    }
    sixel_cdef_free(linear);
    return status;
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

static tsize_t
tiff_memory_read(thandle_t handle, tdata_t data, tsize_t length)
{
    tiff_memory_chunk_t *memory;
    tsize_t to_copy;
    toff_t available;

    memory = (tiff_memory_chunk_t *)handle;
    if (memory->offset >= memory->size) {
        return 0;
    }

    available = memory->size - memory->offset;
    to_copy = length;
    if ((toff_t)to_copy > available) {
        to_copy = (tsize_t)available;
    }

    if (to_copy > 0) {
        memcpy(data, memory->buffer + memory->offset, to_copy);
        memory->offset += (toff_t)to_copy;
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
    tiff_memory_chunk_t *memory;
    toff_t new_offset;

    memory = (tiff_memory_chunk_t *)handle;
    switch (whence) {
    case SEEK_SET:
        new_offset = offset;
        break;
    case SEEK_CUR:
        if (offset > memory->size - memory->offset) {
            new_offset = memory->size;
        } else {
            new_offset = memory->offset + offset;
        }
        break;
    case SEEK_END:
        if (offset > memory->size) {
            new_offset = 0;
        } else {
            new_offset = memory->size - offset;
        }
        break;
    default:
        return (toff_t)-1;
    }

    if (new_offset > memory->size) {
        new_offset = memory->size;
    }

    memory->offset = new_offset;
    return memory->offset;
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
    tiff_memory_chunk_t *memory;

    memory = (tiff_memory_chunk_t *)handle;
    return memory->size;
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

/* Express stored XYZ in the canonical sRGB destination basis. D65 belongs
 * to that basis, not to an inferred source white. No CAT, exposure scaling,
 * profile apply or unit clipping belongs at this native decode boundary. */
static int
tiff_logluv_xyz_to_linear(float const xyz[3], double rgb[3])
{
    double x;
    double y;
    double z;

    x = xyz[0];
    y = xyz[1];
    z = xyz[2];
    if (!isfinite(x) || !isfinite(y) || !isfinite(z)) {
        return 0;
    }
    rgb[0] = 3.240969941904521 * x - 1.537383177570093 * y
           - 0.498610760293003 * z;
    rgb[1] = -0.969243636280880 * x + 1.875967501507721 * y
           + 0.041555057407176 * z;
    rgb[2] = 0.055630079696993 * x - 0.203976958888977 * y
           + 1.056971514242878 * z;
    /* Reject an unrepresentable result before narrowing to float. */
    return isfinite(rgb[0]) && fabs(rgb[0]) <= FLT_MAX &&
           isfinite(rgb[1]) && fabs(rgb[1]) <= FLT_MAX &&
           isfinite(rgb[2]) && fabs(rgb[2]) <= FLT_MAX;
}

/* Keep the integer-path photometric/profile check before the shared float
 * boundary. Expanded Gray samples are RGB-shaped storage, not an RGB source
 * definition. An inapplicable profile leaves all normalized source values
 * untouched, identically in builds with and without Little CMS. */
static int
tiff_convert_samples_to_linear(float *pixels, int width, int height,
                               void const *profile, uint32_t length,
                               uint16_t photometric)
{
    sixel_cms_profile_t *source;
    sixel_cms_color_space_t expected;
    int converted;

    source = NULL;
    converted = 0;
    expected = photometric == PHOTOMETRIC_RGB
             ? SIXEL_CMS_COLORSPACE_RGB : SIXEL_CMS_COLORSPACE_GRAY;
    source = sixel_cms_open_profile_from_mem(profile, length);
    if (source == NULL) {
        return 0;
    }
    if (sixel_cms_get_color_space(source) == expected) {
        converted = sixel_cms_convert_profile_to_linearrgb(
            (unsigned char *)pixels, width, height,
            SIXEL_PIXELFORMAT_RGBFLOAT32, source);
    }
    sixel_cms_close_profile(source);
    return converted;
}

/* Four TIFF ink coordinates are not RGB, nor JPEG's complemented Adobe
 * samples. Keep them intact while a private RGB destination is populated.
 * A failed or nonfinite apply must never become the unmanaged fallback. */
static int
tiff_convert_cmyk_to_linear(float **pixels, size_t count,
                             void const *profile, uint32_t length,
                             sixel_allocator_t *allocator)
{
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *destination;
    sixel_cms_transform_t *transform;
    float *rgb;
    size_t i;
    int converted;

    source = sixel_cms_open_profile_from_mem(profile, length);
    destination = NULL;
    transform = NULL;
    rgb = NULL;
    converted = 0;
    if (source == NULL ||
        sixel_cms_get_color_space(source) != SIXEL_CMS_COLORSPACE_CMYK) {
        goto cleanup;
    }
    rgb = (float *)sixel_allocator_malloc(allocator,
                                           count * 3u * sizeof(float));
    if (rgb == NULL) { goto cleanup; }
    destination = sixel_cms_create_linear_srgb_profile();
    transform = sixel_cms_create_transform(source,
        SIXEL_CMS_PIXELFORMAT_CMYK_F32, destination,
        SIXEL_CMS_PIXELFORMAT_RGB_F32, SIXEL_CMS_TRANSFORM_DEFAULT);
    converted = sixel_cms_do_transform(transform, *pixels, rgb, count);
    for (i = 0u; converted && i < count * 3u; ++i) {
        converted = isfinite(rgb[i]);
    }
    if (converted) {
        sixel_allocator_free(allocator, *pixels);
        *pixels = rgb;
        rgb = NULL;
    }
cleanup:
    sixel_allocator_free(allocator, rgb);
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(destination);
    sixel_cms_close_profile(source);
    return converted;
}

/* Finish normalized integer alpha using the same transaction, background
 * and mask policy for contiguous, separate and tiled source storage. The
 * caller retains both allocations on failure; success transfers color and
 * transfers alpha only when it becomes a public binary mask. */
static SIXELSTATUS
tiff_publish_integer_alpha(unsigned char **result, float **ppixels,
                            unsigned char **palpha, uint32_t width,
                            uint32_t height, uint16_t bits,
                            uint16_t photometric, int associated,
                            int all_opaque, void const *profile,
                            uint32_t profile_length, int *ppixelformat,
                            int *pnegotiate_target, unsigned char **pmask,
                            unsigned char const *bgcolor,
                            int source_linear, int byte_eligible)
{
    SIXELSTATUS status;
    float *pixels;
    unsigned char *alpha;
    size_t count;
    size_t i;
    size_t channel;
    unsigned int a;
    unsigned int maximum;
    double background[3];
    double value;
    double coverage;
    int managed;
    int preserve_zero;
    int has_mask;
    int composed;

    pixels = *ppixels;
    alpha = *palpha;
    count = (size_t)width * height;
    maximum = bits == 8u ? 255u : 65535u;
    managed = source_linear;
    has_mask = composed = 0;
    if (!managed && profile != NULL && profile_length > 0u) {
        managed = tiff_convert_samples_to_linear(
            pixels, (int)width, (int)height, profile, profile_length,
            photometric);
    }

    preserve_zero = bgcolor == NULL ||
        loader_transparent_policy() != SIXEL_ALPHA_POLICY_COMPOSITE;
    for (channel = 0u; channel < 3u; ++channel) {
        background[channel] = bgcolor == NULL ? 0.0
            : (double)bgcolor[channel] / 255.0;
        if (loader_background_colorspace() != SIXEL_COLORSPACE_LINEAR) {
            background[channel] =
                sixel_color_decode_srgb(background[channel]);
        }
    }
    for (i = 0u; i < count; ++i) {
        a = bits == 8u ? alpha[i] : ((uint16_t const *)alpha)[i];
        /* Retain original alpha through CMS. After consuming element i,
         * byte i can hold its mask without overwriting an unread UINT16.
         * Only exact source zero is masked, never a rounded byte zero. */
        if (bgcolor != NULL && a < maximum && (a != 0u || !preserve_zero)) {
            coverage = (double)a / maximum;
            for (channel = 0u; channel < 3u; ++channel) {
                value = pixels[i * 3u + channel];
                if (!managed) {
                    value = sixel_color_decode_srgb(value);
                }
                value = coverage * value +
                    (1.0 - coverage) * background[channel];
                if (!managed) {
                    value = sixel_color_encode_srgb(value);
                }
                if (!isfinite(value) || fabs(value) > FLT_MAX) {
                    status = SIXEL_TIFF_ERROR;
                    return status;
                }
                pixels[i * 3u + channel] = (float)value;
            }
            composed = 1;
        }
        alpha[i] = a == 0u && preserve_zero ? 1u : 0u;
        has_mask |= alpha[i] != 0u;
    }

    *ppixelformat = managed ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                           : SIXEL_PIXELFORMAT_RGBFLOAT32;
    if (byte_eligible && bits == 8u && !managed &&
        (all_opaque || (!associated && !composed))) {
        /* Only original byte sources can compact without precision
         * loss, including all255 associated alpha. UINT16 stays float even
         * when fully opaque, matching the existing opaque16 source path. */
        for (i = 0u; i < count * 3u; ++i) {
            ((unsigned char *)pixels)[i] = (unsigned char)(
                (double)pixels[i] * 255.0 + 0.5);
        }
        *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    }
    if (has_mask) {
        *pmask = alpha;
        *palpha = NULL;
    }
    *pnegotiate_target = managed;
    *result = (unsigned char *)pixels;
    *ppixels = NULL;
    status = SIXEL_OK;

    return status;
}

/* Read declared integer alpha before libtiff's RGBA convenience conversion.
 * RGB and Gray convenience readers do not share association behavior. Keep
 * original alpha separate until source interpretation and composition end. */
static SIXELSTATUS
tiff_try_load_integer_alpha(unsigned char **result, TIFF *tif,
                            uint32_t width, uint32_t height, int *ppixelformat,
                            void const *profile, uint32_t profile_length,
                            uint16_t photometric, sixel_allocator_t *allocator,
                            int *pnegotiate_target, unsigned char **pmask,
                            unsigned char const *bgcolor)
{
    SIXELSTATUS status;
    unsigned char *row;
    unsigned char *alpha;
    float *pixels;
    uint16_t const *row_u16;
    uint16_t bits;
    uint16_t samples;
    uint16_t format;
    uint16_t planar;
    uint16_t orientation;
    uint16_t extras;
    uint16_t *extra_info;
    tsize_t row_size;
    size_t count;
    size_t sample_bytes;
    size_t source;
    size_t destination;
    size_t channel;
    uint32_t x;
    uint32_t y;
    uint32_t dx;
    uint32_t dy;
    unsigned int a;
    unsigned int maximum;
    unsigned int numerator;
    unsigned int denominator;
    int rgb;
    int associated;
    int all_opaque;

    status = SIXEL_FALSE;
    row = NULL;
    alpha = NULL;
    pixels = NULL;
    bits = 1u;
    samples = 1u;
    format = SAMPLEFORMAT_UINT;
    planar = PLANARCONFIG_CONTIG;
    orientation = ORIENTATION_TOPLEFT;
    extras = 0u;
    extra_info = NULL;
    all_opaque = 1;

    rgb = photometric == PHOTOMETRIC_RGB;
    if (!rgb && photometric != PHOTOMETRIC_MINISBLACK &&
        photometric != PHOTOMETRIC_MINISWHITE) {
        return SIXEL_FALSE;
    }
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bits);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samples);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &format);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planar);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &orientation);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_EXTRASAMPLES,
                               &extras, &extra_info);
    if ((bits != 8u && bits != 16u) ||
        (format != SAMPLEFORMAT_UINT && format != SAMPLEFORMAT_VOID) ||
        samples != (rgb ? 4u : 2u) || extras != 1u || extra_info == NULL ||
        (extra_info[0] != EXTRASAMPLE_ASSOCALPHA &&
         extra_info[0] != EXTRASAMPLE_UNASSALPHA) ||
        planar != PLANARCONFIG_CONTIG || TIFFIsTiled(tif) ||
        !tiff_orientation_is_supported(orientation)) {
        return SIXEL_FALSE;
    }
    associated = extra_info[0] == EXTRASAMPLE_ASSOCALPHA;
    sample_bytes = bits / 8u;
    maximum = bits == 8u ? 255u : 65535u;
    if (width == 0u || height == 0u ||
        (size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    count = (size_t)width * height;
    if (count > SIZE_MAX / (3u * sizeof(float)) ||
        (size_t)width > SIZE_MAX / samples / sample_bytes) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    row_size = TIFFScanlineSize(tif);
    if (row_size <= 0 ||
        (size_t)row_size < (size_t)width * samples * sample_bytes) {
        return SIXEL_TIFF_ERROR;
    }

    /* After accepting the source layout, every read/allocation failure is
     * terminal. Retrying RGBA could silently replace the source contract. */
    row = (unsigned char *)sixel_allocator_malloc(allocator,
                                                 (size_t)row_size);
    pixels = (float *)sixel_allocator_malloc(allocator,
                                             count * 3u * sizeof(float));
    alpha = (unsigned char *)sixel_allocator_malloc(
        allocator, count * sample_bytes);
    if (row == NULL || pixels == NULL || alpha == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    /* TIFFReadScanline returns native-endian UINT16 samples. */
    row_u16 = (uint16_t const *)row;
    for (y = 0u; y < height; ++y) {
        if (TIFFReadScanline(tif, row, y, 0) < 0) {
            sixel_helper_set_additional_message(
                "load_tiff: alpha TIFFReadScanline() failed.");
            status = SIXEL_TIFF_ERROR;
            goto cleanup;
        }
        for (x = 0u; x < width; ++x) {
            source = (size_t)x * samples;
            tiff_map_scanline_coordinates(orientation, width, height,
                                          x, y, &dx, &dy);
            destination = (size_t)dy * width + dx;
            a = bits == 8u ? row[source + samples - 1u]
                           : row_u16[source + samples - 1u];
            if (bits == 8u) {
                alpha[destination] = (unsigned char)a;
            } else {
                ((uint16_t *)alpha)[destination] = (uint16_t)a;
            }
            all_opaque &= a == maximum;
            denominator = associated ? a : maximum;
            for (channel = 0u; channel < 3u; ++channel) {
                numerator = bits == 8u
                    ? row[source + (rgb ? channel : 0u)]
                    : row_u16[source + (rgb ? channel : 0u)];
                if (photometric == PHOTOMETRIC_MINISWHITE) {
                    /* Use the reviewed libvips-compatible Gray order:
                     * undo storage polarity before undoing association. */
                    numerator = maximum - numerator;
                }
                /* One rounding from the original integer ratio. Values
                 * above one are valid associated-source coordinates. */
                pixels[destination * 3u + channel] = denominator == 0u
                    ? 0.0f : (float)((double)numerator / denominator);
            }
        }
    }
    status = tiff_publish_integer_alpha(result, &pixels, &alpha,
        width, height, bits, photometric, associated, all_opaque,
        profile, profile_length, ppixelformat, pnegotiate_target,
        pmask, bgcolor, 0, 1);

cleanup:
    sixel_allocator_free(allocator, row);
    sixel_allocator_free(allocator, pixels);
    sixel_allocator_free(allocator, alpha);
    return status;
}


/* Read accepted opaque separate UINT16 strips in codec-sequential order.
 * The caller validates layout, dimensions, row size and owned buffers first.
 * A plane must finish before another starts: alternating planes per row can
 * require unsupported random access inside a compressed multi-row strip. */
static SIXELSTATUS
tiff_read_separate_uint16(float *pixels, TIFF *tif, unsigned char *scanline,
                          uint32_t width, uint32_t height,
                          uint16_t samples, uint16_t photometric,
                          uint16_t orientation)
{
    uint16_t const *words;
    uint16_t plane;
    uint32_t x;
    uint32_t y;
    uint32_t dst_x;
    uint32_t dst_y;
    size_t destination;
    double unit;

    words = (uint16_t const *)scanline;
    for (plane = 0u; plane < samples; ++plane) {
        for (y = 0u; y < height; ++y) {
            if (TIFFReadScanline(tif, scanline, y, plane) != 1) {
                sixel_helper_set_additional_message(
                    "load_tiff: TIFFReadScanline() failed.");
                return SIXEL_TIFF_ERROR;
            }
            for (x = 0u; x < width; ++x) {
                tiff_map_scanline_coordinates(orientation, width, height,
                                              x, y, &dst_x, &dst_y);
                destination = ((size_t)dst_y * width + dst_x) * 3u;
                /* Native-endian original words reach float storage once.
                 * Gray polarity precedes CMS; equal RGB-shaped components
                 * still describe one Gray source coordinate. */
                unit = photometric == PHOTOMETRIC_MINISWHITE
                    ? (double)(65535u - words[x]) / 65535.0
                    : (double)words[x] / 65535.0;
                if (photometric == PHOTOMETRIC_CIELAB) {
                    pixels[destination + plane] = (float)(plane == 0u
                        ? tiff_decode_lab_u16_l(words[x])
                        : tiff_decode_lab_u16_ab(words[x]));
                } else if (photometric == PHOTOMETRIC_RGB) {
                    pixels[destination + plane] = (float)unit;
                } else {
                    pixels[destination] = (float)unit;
                    pixels[destination + 1u] = (float)unit;
                    pixels[destination + 2u] = (float)unit;
                }
            }
        }
    }
    return SIXEL_OK;
}


/* Separate integer storage addressing from source interpretation. Unmanaged
 * byte sources write bytes directly. When float work is required, integer
 * numerators fit exactly in F32, so separate alpha can arrive in a later
 * plane without rounding the final color/alpha ratio twice or allocating
 * another full-image integer buffer. */
typedef struct {
    uint32_t width, height, output_width, output_height;
    uint16_t bits, samples, planar, orientation, photometric;
    int has_alpha, associated, channels;
    uint16_t *map[3];
    float *pixels;
    unsigned char *bytes;
    unsigned char *alpha;
} tiff_integer_layout_t;

static void
tiff_scatter_integer_row(tiff_integer_layout_t *layout,
                         unsigned char const *row, uint32_t left,
                         uint32_t y, uint32_t columns, uint16_t plane)
{
    uint32_t x;
    uint32_t dx;
    uint32_t dy;
    uint16_t first;
    uint16_t last;
    uint16_t channel;
    uint16_t word;
    unsigned int value;
    unsigned int maximum;
    size_t sample;
    size_t bit;
    size_t destination;
    size_t c;

    maximum = (1u << layout->bits) - 1u;
    first = layout->planar == PLANARCONFIG_SEPARATE ? plane : 0u;
    last = layout->planar == PLANARCONFIG_SEPARATE
         ? (uint16_t)(plane + 1u) : layout->samples;
    for (x = 0u; x < columns; ++x) {
        /* Orient physical source coordinates once. The output stride is
         * shared by color, alpha, CMS dimensions and public frame metadata. */
        tiff_map_scanline_coordinates(layout->orientation,
            layout->width, layout->height, left + x, y, &dx, &dy);
        switch (layout->orientation) {
        case ORIENTATION_LEFTTOP:
            dx = y;
            dy = left + x;
            break;
        case ORIENTATION_RIGHTTOP:
            dx = layout->height - 1u - y;
            dy = left + x;
            break;
        case ORIENTATION_RIGHTBOT:
            dx = layout->height - 1u - y;
            dy = layout->width - 1u - left - x;
            break;
        case ORIENTATION_LEFTBOT:
            dx = y;
            dy = layout->width - 1u - left - x;
            break;
        default:
            break;
        }
        destination = (size_t)dy * layout->output_width + dx;
        for (channel = first; channel < last; ++channel) {
            sample = layout->planar == PLANARCONFIG_SEPARATE ? x
                   : (size_t)x * layout->samples + channel;
            if (layout->bits == 16u) {
                /* Decoded words are native-endian. memcpy also handles
                 * any row alignment without an unaligned typed access. */
                memcpy(&word, row + sample * sizeof(word), sizeof(word));
                value = word;
            } else if (layout->bits == 8u) {
                value = row[sample];
            } else {
                /* TIFFClientOpen("r") uses the default B decoded order:
                 * MSB first, independent of host byte order and the file's
                 * FillOrder. Libtiff has already reversed file bits. */
                bit = sample * layout->bits;
                value = (row[bit / 8u] >>
                         (8u - layout->bits - bit % 8u)) & maximum;
            }
            if (layout->has_alpha && channel + 1u == layout->samples) {
                if (layout->bits == 8u) {
                    layout->alpha[destination] = (unsigned char)value;
                } else {
                    ((uint16_t *)layout->alpha)[destination] =
                        (uint16_t)value;
                }
            } else if (layout->bytes != NULL) {
                /* Unmanaged byte samples do not need a normalized float
                 * image. All 1/2/4-bit gray levels expand exactly to bytes. */
                if (layout->photometric == PHOTOMETRIC_RGB ||
                    layout->photometric == PHOTOMETRIC_SEPARATED) {
                    layout->bytes[destination * layout->channels + channel] =
                        (unsigned char)value;
                } else {
                    if (layout->photometric == PHOTOMETRIC_MINISWHITE) {
                        value = maximum - value;
                    }
                    for (c = 0u; c < 3u; ++c) {
                        layout->bytes[destination * 3u + c] =
                            (unsigned char)(value * (255u / maximum));
                    }
                }
            } else if (layout->photometric == PHOTOMETRIC_RGB ||
                       layout->photometric == PHOTOMETRIC_SEPARATED ||
                       layout->photometric == PHOTOMETRIC_CIELAB ||
                       layout->photometric == PHOTOMETRIC_ICCLAB ||
                       layout->photometric == PHOTOMETRIC_ITULAB) {
                layout->pixels[destination * layout->channels + channel] =
                    (float)value;
            } else {
                for (c = 0u; c < 3u; ++c) {
                    layout->pixels[destination * 3u + c] =
                        layout->photometric == PHOTOMETRIC_PALETTE
                        ? (float)layout->map[c][value] : (float)value;
                }
            }
        }
    }
}

static SIXELSTATUS
tiff_try_load_integer_layouts(unsigned char **result, TIFF *tif,
                              uint32_t width, uint32_t height,
                              int *ppixelformat, void const *profile,
                              uint32_t profile_length, uint16_t photometric,
                              sixel_allocator_t *allocator,
                              int *pnegotiate_target, unsigned char **pmask,
                              unsigned char const *bgcolor,
                              int *pwidth, int *pheight)
{
    tiff_integer_layout_t layout;
    SIXELSTATUS status;
    unsigned char *buffer;
    uint16_t format;
    uint16_t inkset;
    uint16_t inks;
    uint16_t extras;
    uint16_t *extra_info;
    uint16_t plane;
    uint16_t planes;
    uint32_t block_width;
    uint32_t block_height;
    uint32_t depth;
    uint32_t tile_depth;
    uint32_t left;
    uint32_t top;
    uint32_t y;
    uint32_t columns;
    tmsize_t row_size;
    tmsize_t buffer_size;
    tmsize_t decoded;
    size_t count;
    size_t row_samples;
    size_t minimum_row;
    size_t i;
    size_t c;
    unsigned int maximum;
    unsigned int numerator;
    unsigned int denominator;
    unsigned int a;
    int rgb;
    int palette;
    int tiled;
    int managed;
    int all_opaque;
    int byte_storage;
    int has_mask;
    int lab;
    int cmyk;
    int lab_decode;
    int linear;
    double lab_ranges[6];
    double unit;
    double black;

    memset(&layout, 0, sizeof(layout));
    layout.width = width;
    layout.height = height;
    layout.bits = layout.samples = 1u;
    layout.planar = PLANARCONFIG_CONTIG;
    layout.orientation = ORIENTATION_TOPLEFT;
    layout.photometric = photometric;
    format = SAMPLEFORMAT_UINT;
    extras = 0u;
    extra_info = NULL;
    buffer = NULL;
    status = SIXEL_FALSE;
    managed = 0;
    all_opaque = 1;
    depth = tile_depth = 1u;
    rgb = photometric == PHOTOMETRIC_RGB;
    palette = photometric == PHOTOMETRIC_PALETTE;
    cmyk = photometric == PHOTOMETRIC_SEPARATED;
    lab = photometric == PHOTOMETRIC_CIELAB ||
          photometric == PHOTOMETRIC_ICCLAB ||
          photometric == PHOTOMETRIC_ITULAB;
    layout.channels = cmyk ? 4 : 3;
    lab_decode = 0;
    linear = 0;
    if (!rgb && !palette && !lab && !cmyk &&
        photometric != PHOTOMETRIC_MINISBLACK &&
        photometric != PHOTOMETRIC_MINISWHITE) {
        return SIXEL_FALSE;
    }
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &layout.bits);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &layout.samples);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &format);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &layout.planar);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &layout.orientation);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_EXTRASAMPLES,
                               &extras, &extra_info);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_IMAGEDEPTH, &depth);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_TILEDEPTH, &tile_depth);
    tiled = TIFFIsTiled(tif);
    if ((format != SAMPLEFORMAT_UINT && format != SAMPLEFORMAT_VOID) ||
        (layout.bits != 1u && layout.bits != 2u && layout.bits != 4u &&
         layout.bits != 8u && layout.bits != 16u) ||
        ((rgb || lab || cmyk) && layout.bits < 8u) ||
        ((photometric == PHOTOMETRIC_ICCLAB ||
          photometric == PHOTOMETRIC_ITULAB) && layout.bits != 8u) ||
        (layout.planar != PLANARCONFIG_CONTIG &&
         layout.planar != PLANARCONFIG_SEPARATE) ||
        layout.orientation < 1u || layout.orientation > 8u ||
        depth != 1u || (tiled && tile_depth != 1u)) {
        return SIXEL_FALSE;
    }
    layout.has_alpha = extras == 1u && extra_info != NULL &&
        (extra_info[0] == EXTRASAMPLE_ASSOCALPHA ||
         extra_info[0] == EXTRASAMPLE_UNASSALPHA);
    if ((extras != 0u && !layout.has_alpha) ||
        (layout.has_alpha && (palette || layout.bits < 8u)) ||
        layout.samples != (cmyk ? 4u : (rgb || lab) ? 3u : 1u) +
                          layout.has_alpha) {
        return SIXEL_FALSE;
    }
    if (cmyk) {
        /* SEPARATED is not necessarily CMYK. Do not interpret arbitrary
         * N-ink data, even when a mismatching ICC happens to say CMYK. */
        inkset = INKSET_CMYK;
        inks = 4u;
        (void)TIFFGetFieldDefaulted(tif, TIFFTAG_INKSET, &inkset);
        (void)TIFFGetField(tif, TIFFTAG_NUMBEROFINKS, &inks);
        if (inkset != INKSET_CMYK || inks != 4u) {
            return SIXEL_FALSE;
        }
    }
    layout.associated = layout.has_alpha &&
        extra_info[0] == EXTRASAMPLE_ASSOCALPHA;
    byte_storage = layout.bits <= 8u && !palette && !lab &&
        (profile == NULL || profile_length == 0u) &&
        (!layout.has_alpha || (!layout.associated && bgcolor == NULL));
    /* Established contiguous and separate opaque16 readers retain their
     * gates, fault behavior and target policy. Existing opaque Lab scanline
     * routes likewise retain their established fallback. New integer Lab
     * and CMYK layouts retain source coordinates before any RGB rendering. */
    if (!byte_storage && !tiled && layout.orientation <= 4u &&
        !palette && !lab && !cmyk &&
        layout.bits >= 8u &&
        (layout.planar == PLANARCONFIG_CONTIG ||
         (layout.bits == 16u && !layout.has_alpha))) {
        return SIXEL_FALSE;
    }
    if (lab && !tiled && layout.orientation <= 4u && !layout.has_alpha &&
        ((photometric == PHOTOMETRIC_CIELAB && layout.bits == 16u) ||
         (photometric != PHOTOMETRIC_CIELAB &&
          layout.planar == PLANARCONFIG_CONTIG))) {
        return SIXEL_FALSE;
    }
    if (lab && photometric != PHOTOMETRIC_CIELAB) {
        lab_decode = tiff_lab8_ranges(tif, photometric, lab_ranges);
        if (!lab_decode) { return SIXEL_TIFF_ERROR; }
    }
    if (palette && !TIFFGetField(tif, TIFFTAG_COLORMAP,
                                &layout.map[0], &layout.map[1],
                                &layout.map[2])) {
        return SIXEL_TIFF_ERROR;
    }
    layout.output_width = layout.orientation >= 5u ? height : width;
    layout.output_height = layout.orientation >= 5u ? width : height;
    if (width == 0u || height == 0u ||
        (size_t)width > SIZE_MAX / height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    count = (size_t)width * height;
    if (count > SIZE_MAX / ((size_t)layout.channels * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    block_width = width;
    block_height = 1u;
    if (tiled &&
        (!TIFFGetField(tif, TIFFTAG_TILEWIDTH, &block_width) ||
         !TIFFGetField(tif, TIFFTAG_TILELENGTH, &block_height))) {
        return SIXEL_TIFF_ERROR;
    }
    if (block_width == 0u || block_height == 0u ||
        block_width > INT_MAX || block_height > INT_MAX) {
        return SIXEL_TIFF_ERROR;
    }
    planes = layout.planar == PLANARCONFIG_SEPARATE ? layout.samples : 1u;
    row_samples = layout.planar == PLANARCONFIG_SEPARATE
                ? 1u : layout.samples;
    if ((size_t)block_width > (SIZE_MAX - 7u) /
        row_samples / layout.bits) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    minimum_row = ((size_t)block_width * row_samples * layout.bits + 7u) / 8u;
    row_size = tiled ? TIFFTileRowSize(tif) : TIFFScanlineSize(tif);
    buffer_size = tiled ? TIFFTileSize(tif) : row_size;
    if (row_size <= 0 || buffer_size <= 0 ||
        (size_t)row_size < minimum_row ||
        (size_t)row_size > SIZE_MAX / block_height ||
        (size_t)buffer_size != (size_t)row_size * block_height) {
        return SIXEL_TIFF_ERROR;
    }

    /* Selection is complete. Every subsequent source failure is terminal,
     * including a short decoded tile; no convenience-RGBA retry is safe. */
    buffer = (unsigned char *)sixel_allocator_malloc(allocator,
                                                     (size_t)buffer_size);
    if (byte_storage) {
        layout.bytes = (unsigned char *)sixel_allocator_malloc(allocator,
                                            count * layout.channels);
    } else {
        layout.pixels = (float *)sixel_allocator_malloc(allocator,
                                count * layout.channels * sizeof(float));
    }
    if (layout.has_alpha) {
        layout.alpha = (unsigned char *)sixel_allocator_malloc(allocator,
                                                count * (layout.bits / 8u));
    }
    if (buffer == NULL ||
        (byte_storage ? layout.bytes == NULL : layout.pixels == NULL) ||
        (layout.has_alpha && layout.alpha == NULL)) {
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    for (plane = 0u; plane < planes; ++plane) {
        for (top = 0u; top < height; top += block_height) {
            for (left = 0u; left < width; left += block_width) {
                decoded = tiled
                    ? TIFFReadTile(tif, buffer, left, top, 0u, plane)
                    : TIFFReadScanline(tif, buffer, top, plane);
                if (decoded != (tiled ? buffer_size : 1)) {
                    status = SIXEL_TIFF_ERROR;
                    goto cleanup;
                }
                columns = width - left < block_width
                        ? width - left : block_width;
                for (y = 0u; y < block_height && y < height - top; ++y) {
                    tiff_scatter_integer_row(&layout,
                        buffer + (size_t)y * (size_t)row_size,
                        left, top + y, columns, plane);
                }
            }
        }
    }
    if (byte_storage) {
        if (cmyk) {
            /* Preserve the established uncalibrated libtiff byte fallback:
             * floor((255-C/M/Y)*(255-K)/255), with no float image. Consume
             * K before contracting four source bytes into three RGB bytes. */
            for (i = 0u; i < count; ++i) {
                a = 255u - layout.bytes[i * 4u + 3u];
                for (c = 0u; c < 3u; ++c) {
                    layout.bytes[i * 3u + c] = (unsigned char)(
                        a * (255u - layout.bytes[i * 4u + c]) / 255u);
                }
            }
        }
        has_mask = 0;
        if (layout.has_alpha) {
            for (i = 0u; i < count; ++i) {
                layout.alpha[i] = layout.alpha[i] == 0u;
                has_mask |= layout.alpha[i];
            }
        }
        if (has_mask) {
            *pmask = layout.alpha;
            layout.alpha = NULL;
        }
        *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
        *pnegotiate_target = 0;
        *result = layout.bytes;
        layout.bytes = NULL;
        *pwidth = (int)layout.output_width;
        *pheight = (int)layout.output_height;
        status = SIXEL_OK;
        goto cleanup;
    }
    maximum = palette ? 65535u : (1u << layout.bits) - 1u;
    for (i = 0u; i < count; ++i) {
        a = !layout.has_alpha ? maximum : layout.bits == 8u
          ? layout.alpha[i] : ((uint16_t const *)layout.alpha)[i];
        all_opaque &= a == maximum;
        denominator = layout.associated ? a : maximum;
        for (c = 0u; c < (size_t)layout.channels; ++c) {
            numerator = (unsigned int)layout.pixels[i * layout.channels + c];
            if (photometric == PHOTOMETRIC_MINISWHITE) {
                numerator = maximum - numerator;
            }
            unit = (double)numerator / maximum;
            if (lab && c != 0u) {
                if (photometric == PHOTOMETRIC_CIELAB) {
                    /* CIELAB8 chroma is signed, unlike offset ICCLAB8.
                     * CIELAB16 chroma is signed physical a/b times 256. */
                    unit = layout.bits == 16u
                        ? tiff_decode_lab_u16_ab((uint16_t)numerator)
                        : ((double)numerator -
                           (numerator >= 128u ? 256.0 : 0.0)) / 128.0;
                } else if (lab_decode != 2) {
                    unit = photometric == PHOTOMETRIC_ICCLAB
                        ? ((double)numerator - 128.0) / 128.0
                        : ((double)numerator - (c == 1u ? 128.0 : 96.0)) *
                          (c == 1u ? 170.0 : 200.0) / (255.0 * 128.0);
                }
            }
            if (lab_decode == 2) {
                unit = (lab_ranges[c * 2u] + (double)numerator / 255.0 *
                    (lab_ranges[c * 2u + 1u] - lab_ranges[c * 2u])) /
                    (c == 0u ? 100.0 : 128.0);
            }
            if (layout.associated) {
                unit = denominator == 0u ? 0.0
                    : unit * maximum / denominator;
            }
            /* Retain the exact original integer-ratio rounding on RGB/Gray.
             * Lab association is removed after signed/offset decoding. */
            if (!lab) {
                unit = denominator == 0u ? 0.0
                     : (double)numerator / denominator;
            }
            if (!isfinite(unit) || fabs(unit) > FLT_MAX) {
                status = SIXEL_TIFF_ERROR;
                goto cleanup;
            }
            layout.pixels[i * layout.channels + c] = (float)unit;
        }
    }
    if (lab || cmyk) {
        if (profile != NULL && profile_length > 0u) {
            managed = cmyk ? tiff_convert_cmyk_to_linear(&layout.pixels,
                count, profile, profile_length, allocator)
                : sixel_cms_convert_to_linearrgb_with_profile_bytes(
                    (unsigned char *)layout.pixels,
                    (int)layout.output_width, (int)layout.output_height,
                    SIXEL_PIXELFORMAT_CIELABFLOAT32, profile, profile_length);
        }
        linear = managed;
        if (!managed && lab) {
            if (layout.bits == 8u) {
                status = tiff_lab8_to_linear(layout.pixels, count);
                linear = 1;
            } else {
                status = sixel_helper_convert_colorspace(
                    (unsigned char *)layout.pixels,
                    count * 3u * sizeof(float),
                    SIXEL_PIXELFORMAT_CIELABFLOAT32,
                    SIXEL_COLORSPACE_CIELAB, SIXEL_COLORSPACE_GAMMA);
            }
            if (SIXEL_FAILED(status)) { goto cleanup; }
        } else if (!managed && cmyk) {
            for (i = 0u; i < count; ++i) {
                black = layout.pixels[i * 4u + 3u];
                for (c = 0u; c < 3u; ++c) {
                    unit = layout.pixels[i * 4u + c];
                    if (layout.bits == 8u &&
                        (!layout.associated || all_opaque)) {
                        /* Recover byte numerators before integer fallback;
                         * F32 normalization must not shift a floor by one. */
                        numerator = 255u - (unsigned int)(unit * 255.0 + 0.5);
                        a = 255u - (unsigned int)(black * 255.0 + 0.5);
                        unit = (double)(numerator * a / 255u) / 255.0;
                    } else {
                        unit = (1.0 - unit) * (1.0 - black);
                    }
                    layout.pixels[i * 3u + c] = (float)unit;
                }
            }
        }
        /* Original Lab/ink profiles have been consumed (or declined).
         * Never feed the resulting RGB fallback back into those profiles. */
        profile = NULL;
        profile_length = 0u;
    }
    if (layout.has_alpha) {
        status = tiff_publish_integer_alpha(result, &layout.pixels,
            &layout.alpha, layout.output_width, layout.output_height,
            layout.bits, photometric, layout.associated, all_opaque,
            profile, profile_length, ppixelformat, pnegotiate_target,
            pmask, bgcolor, linear, !lab);
        if (SIXEL_FAILED(status)) { goto cleanup; }
    } else {
        if (profile != NULL && profile_length > 0u) {
            managed = tiff_convert_samples_to_linear(layout.pixels,
                (int)layout.output_width, (int)layout.output_height,
                profile, profile_length,
                palette ? PHOTOMETRIC_RGB : photometric);
        }
        *ppixelformat = managed || linear
                               ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                               : SIXEL_PIXELFORMAT_RGBFLOAT32;
        if (!managed && !palette && !lab && layout.bits <= 8u) {
            /* All legal 1/2/4-bit Gray levels also have an exact byte
             * representation. Only these unchanged source levels compact. */
            for (i = 0u; i < count * 3u; ++i) {
                ((unsigned char *)layout.pixels)[i] = (unsigned char)(
                    (double)layout.pixels[i] * 255.0 + 0.5);
            }
            *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
        }
        *pnegotiate_target = linear ||
            (managed && (layout.bits <= 8u || palette));
        *result = (unsigned char *)layout.pixels;
        layout.pixels = NULL;
    }
    *pwidth = (int)layout.output_width;
    *pheight = (int)layout.output_height;
    status = SIXEL_OK;

cleanup:
    sixel_allocator_free(allocator, buffer);
    sixel_allocator_free(allocator, layout.pixels);
    sixel_allocator_free(allocator, layout.bytes);
    sixel_allocator_free(allocator, layout.alpha);
    return status;
}

/*
 * Decode selected high-precision TIFF layouts directly into float32 buffers.
 *
 * Supported source layouts:
 *   - RGB/Gray integer (8/16-bit contiguous or 16-bit separate strips,
 *     no alpha, orientation 1-4)
 *   - RGB/Gray float32 (SampleFormat=IEEEFP, contiguous, no alpha)
 *   - CIELAB16 contiguous/separate strips; CIELAB float32 contiguous
 *   - ICCLAB8/ITULAB8 contiguous strips, with original Lab normalization
 *     (all Lab modes: no alpha, orientation 1-4)
 *   - LogLuv SGILOG/SGILOG24 via decoded XYZ float32, contiguous, no alpha
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
                             sixel_allocator_t  /* in */  *allocator,
                             int                /* out */ *pnegotiate_target)
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
    uint16_t compression;
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
    size_t component;
    unsigned char const *row_u8;
    uint16_t const *row_u16;
    float const *row_f32;
    double unit;
    double rgb_unit[3];
    double lab_unit[3];
    double lab_ranges[6];
    int cms_converted;
    int separate_uint16;
    int lab_decode;
    int native_float;
    int float_colorspace;
    int float_pixelformat;
    enum {
        TIFF_DECODE_UNSUPPORTED = 0,
        TIFF_DECODE_RGB_UINT,
        TIFF_DECODE_GRAY_UINT,
        TIFF_DECODE_RGB_FLOAT32,
        TIFF_DECODE_GRAY_FLOAT32,
        TIFF_DECODE_LAB_UINT,
        TIFF_DECODE_LAB_BYTE,
        TIFF_DECODE_LAB_FLOAT32,
        TIFF_DECODE_LOGLUV_FLOAT32
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
    compression = COMPRESSION_NONE;
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
    lab_unit[0] = 0.0;
    lab_unit[1] = 0.0;
    lab_unit[2] = 0.0;
    cms_converted = 0;
    separate_uint16 = 0;
    native_float = 0;
    float_colorspace = SIXEL_COLORSPACE_GAMMA;
    float_pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    decode_mode = TIFF_DECODE_UNSUPPORTED;

    if (result == NULL || ppixelformat == NULL || pnegotiate_target == NULL ||
        tif == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *pnegotiate_target = 0;
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
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL,
                                &samples_per_pixel);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sample_format);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planar_config);
    (void)TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &orientation);

    if (sample_format == SAMPLEFORMAT_VOID) {
        sample_format = SAMPLEFORMAT_UINT;
    }
    if (photometric != PHOTOMETRIC_LOGLUV && bits_per_sample <= 8u &&
        sample_format != SAMPLEFORMAT_IEEEFP &&
        !(bits_per_sample == 8u && sample_format == SAMPLEFORMAT_UINT &&
          (photometric == PHOTOMETRIC_RGB ||
           photometric == PHOTOMETRIC_MINISBLACK ||
           photometric == PHOTOMETRIC_MINISWHITE ||
           photometric == PHOTOMETRIC_ICCLAB ||
           photometric == PHOTOMETRIC_ITULAB))) {
        return SIXEL_FALSE;
    }
    /* Do not widen the existing float, alpha or tiled source routes.
     * Extra-sample rejection below also precedes every read or allocation. */
    separate_uint16 = planar_config == PLANARCONFIG_SEPARATE &&
        bits_per_sample == 16u && sample_format == SAMPLEFORMAT_UINT &&
        (((photometric == PHOTOMETRIC_RGB ||
           photometric == PHOTOMETRIC_CIELAB) && samples_per_pixel == 3u) ||
         ((photometric == PHOTOMETRIC_MINISBLACK ||
           photometric == PHOTOMETRIC_MINISWHITE) && samples_per_pixel == 1u));
    if ((planar_config != PLANARCONFIG_CONTIG && !separate_uint16) ||
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
    case PHOTOMETRIC_LOGLUV:
        (void)TIFFGetFieldDefaulted(tif, TIFFTAG_COMPRESSION, &compression);
        if (samples_per_pixel != 3u ||
            (compression != COMPRESSION_SGILOG &&
             compression != COMPRESSION_SGILOG24)) {
            return SIXEL_FALSE;
        }
        /* All unsupported-layout exits precede this state mutation. From
         * here, failure is terminal: an RGBA retry could consume a different
         * interface or a partially read codec stream. */
        if (!TIFFSetField(tif, TIFFTAG_SGILOGDATAFMT, SGILOGDATAFMT_FLOAT) ||
            !TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE,
                                    &bits_per_sample) ||
            !TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT,
                                    &sample_format) ||
            !TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL,
                                    &samples_per_pixel) ||
            bits_per_sample != 32u || sample_format != SAMPLEFORMAT_IEEEFP ||
            samples_per_pixel != 3u) {
            return SIXEL_TIFF_ERROR;
        }
        decode_mode = TIFF_DECODE_LOGLUV_FLOAT32;
        break;
    case PHOTOMETRIC_RGB:
        if (samples_per_pixel > 3u) {
            return SIXEL_FALSE;
        }
        if (sample_format == SAMPLEFORMAT_IEEEFP && bits_per_sample == 32u &&
            samples_per_pixel >= 3u) {
            decode_mode = TIFF_DECODE_RGB_FLOAT32;
        } else if (sample_format == SAMPLEFORMAT_UINT &&
                   (bits_per_sample == 8u || bits_per_sample == 16u) &&
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
                   (bits_per_sample == 8u || bits_per_sample == 16u)) {
            decode_mode = TIFF_DECODE_GRAY_UINT;
        }
        break;
    case PHOTOMETRIC_ICCLAB:
    case PHOTOMETRIC_ITULAB:
        if (samples_per_pixel != 3u || bits_per_sample != 8u ||
            sample_format != SAMPLEFORMAT_UINT) {
            return SIXEL_FALSE;
        }
        lab_decode = tiff_lab8_ranges(tif, photometric, lab_ranges);
        if (!lab_decode) {
            sixel_helper_set_additional_message(
                "load_tiff: invalid ITULAB Decode ranges.");
            return SIXEL_TIFF_ERROR;
        }
        decode_mode = TIFF_DECODE_LAB_BYTE;
        break;
    case PHOTOMETRIC_CIELAB:
        if (samples_per_pixel < 3u || samples_per_pixel > 3u) {
            return SIXEL_FALSE;
        }
        if (sample_format == SAMPLEFORMAT_IEEEFP && bits_per_sample == 32u) {
            decode_mode = TIFF_DECODE_LAB_FLOAT32;
        } else if (sample_format == SAMPLEFORMAT_UINT &&
                   bits_per_sample == 16u) {
            decode_mode = TIFF_DECODE_LAB_UINT;
        }
        break;
    default:
        break;
    }
    if (decode_mode == TIFF_DECODE_UNSUPPORTED) {
        return SIXEL_FALSE;
    }
    /* Native float samples must reach the CMS without a unit clamp. */
    native_float =
        decode_mode == TIFF_DECODE_RGB_FLOAT32 ||
        decode_mode == TIFF_DECODE_GRAY_FLOAT32;
    if (decode_mode == TIFF_DECODE_LAB_BYTE ||
        decode_mode == TIFF_DECODE_LAB_UINT ||
        decode_mode == TIFF_DECODE_LAB_FLOAT32) {
        float_colorspace = SIXEL_COLORSPACE_CIELAB;
        float_pixelformat = SIXEL_PIXELFORMAT_CIELABFLOAT32;
    }

    if ((size_t)width > SIZE_MAX / (size_t)samples_per_pixel) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    row_sample_count = (size_t)width *
        (separate_uint16 ? 1u : (size_t)samples_per_pixel);
    switch (decode_mode) {
    case TIFF_DECODE_RGB_FLOAT32:
    case TIFF_DECODE_GRAY_FLOAT32:
    case TIFF_DECODE_LAB_FLOAT32:
    case TIFF_DECODE_LOGLUV_FLOAT32:
        if (row_sample_count > SIZE_MAX / sizeof(float)) {
            return SIXEL_BAD_INTEGER_OVERFLOW;
        }
        required_scanline_bytes = row_sample_count * sizeof(float);
        break;
    case TIFF_DECODE_RGB_UINT:
    case TIFF_DECODE_GRAY_UINT:
    case TIFF_DECODE_LAB_UINT:
    case TIFF_DECODE_LAB_BYTE:
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
        /* An eligible separate source cannot retry via byte RGBA. */
        return separate_uint16 || decode_mode == TIFF_DECODE_LAB_BYTE ||
            decode_mode == TIFF_DECODE_LOGLUV_FLOAT32
             ? SIXEL_TIFF_ERROR : SIXEL_FALSE;
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

    if (separate_uint16) {
        status = tiff_read_separate_uint16(float_pixels, tif, scanline,
            width, height, samples_per_pixel, photometric, orientation);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }

    /* Contiguous modes retain their existing sample interpretation. */
    for (y = 0u; !separate_uint16 && y < height; ++y) {
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
            case TIFF_DECODE_LOGLUV_FLOAT32:
                if (!tiff_logluv_xyz_to_linear(row_f32 + src_base,
                                               rgb_unit)) {
                    status = SIXEL_TIFF_ERROR;
                    goto cleanup;
                }
                break;
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
            case TIFF_DECODE_LAB_BYTE:
                for (component = 0u; component < 3u; ++component) {
                    unit = row_u8[src_base + component];
                    if (lab_decode == 2) {
                        unit = lab_ranges[2u * component] + unit / 255.0 *
                            (lab_ranges[2u * component + 1u] -
                             lab_ranges[2u * component]);
                    } else if (component == 0u) {
                        unit *= 100.0 / 255.0;
                    } else if (photometric == PHOTOMETRIC_ICCLAB) {
                        unit -= 128.0;
                    } else if (component == 1u) {
                        unit = (unit - 128.0) * (170.0 / 255.0);
                    } else {
                        unit = (unit - 96.0) * (200.0 / 255.0);
                    }
                    unit /= component == 0u ? 100.0 : 128.0;
                    if (!isfinite(unit) || fabs(unit) > FLT_MAX) {
                        status = SIXEL_TIFF_ERROR;
                        goto cleanup;
                    }
                    lab_unit[component] = unit;
                }
                break;
            case TIFF_DECODE_LAB_UINT:
                lab_unit[0] = tiff_decode_lab_u16_l(row_u16[src_base + 0u]);
                lab_unit[1] = tiff_decode_lab_u16_ab(row_u16[src_base + 1u]);
                lab_unit[2] = tiff_decode_lab_u16_ab(row_u16[src_base + 2u]);
                break;
            case TIFF_DECODE_LAB_FLOAT32:
                lab_unit[0] = tiff_normalize_lab_l_sample(
                    (double)row_f32[src_base + 0u]);
                lab_unit[1] = tiff_normalize_lab_ab_sample(
                    (double)row_f32[src_base + 1u]);
                lab_unit[2] = tiff_normalize_lab_ab_sample(
                    (double)row_f32[src_base + 2u]);
                break;
            default:
                status = SIXEL_FALSE;
                goto cleanup;
            }

            if (float_colorspace == SIXEL_COLORSPACE_CIELAB) {
                float_pixels[dst_base + 0u] = (float)lab_unit[0];
                float_pixels[dst_base + 1u] = (float)lab_unit[1];
                float_pixels[dst_base + 2u] = (float)lab_unit[2];
            } else if (native_float) {
                /* Preserve finite native words; no gamut or byte boundary. */
                if (!isfinite(rgb_unit[0]) || !isfinite(rgb_unit[1]) ||
                    !isfinite(rgb_unit[2]) ||
                    fabs(rgb_unit[0]) > FLT_MAX ||
                    fabs(rgb_unit[1]) > FLT_MAX ||
                    fabs(rgb_unit[2]) > FLT_MAX) {
                    status = SIXEL_TIFF_ERROR;
                    goto cleanup;
                }
                if (decode_mode == TIFF_DECODE_RGB_FLOAT32) {
                    memcpy(float_pixels + dst_base, row_f32 + src_base,
                           3u * sizeof(float));
                } else if (photometric == PHOTOMETRIC_MINISBLACK) {
                    memcpy(float_pixels + dst_base, row_f32 + src_base,
                           sizeof(float));
                    memcpy(float_pixels + dst_base + 1u, row_f32 + src_base,
                           sizeof(float));
                    memcpy(float_pixels + dst_base + 2u, row_f32 + src_base,
                           sizeof(float));
                } else {
                    /* Extend libsixel's existing unit-relative polarity. */
                    float_pixels[dst_base] = (float)rgb_unit[0];
                    float_pixels[dst_base + 1u] = (float)rgb_unit[1];
                    float_pixels[dst_base + 2u] = (float)rgb_unit[2];
                }
            } else if (decode_mode == TIFF_DECODE_LOGLUV_FLOAT32) {
                /* Native XYZ conversion is already finite and linear. */
                float_pixels[dst_base + 0u] = (float)rgb_unit[0];
                float_pixels[dst_base + 1u] = (float)rgb_unit[1];
                float_pixels[dst_base + 2u] = (float)rgb_unit[2];
            } else {
                float_pixels[dst_base + 0u] =
                    (float)tiff_clamp_unit(rgb_unit[0]);
                float_pixels[dst_base + 1u] =
                    (float)tiff_clamp_unit(rgb_unit[1]);
                float_pixels[dst_base + 2u] =
                    (float)tiff_clamp_unit(rgb_unit[2]);
            }
        }
    }

    if (icc_profile != NULL && icc_profile_length > 0u &&
        tiff_photometric_allows_embedded_icc(photometric, 1)) {
        if (decode_mode == TIFF_DECODE_RGB_UINT ||
            decode_mode == TIFF_DECODE_GRAY_UINT) {
            cms_converted = tiff_convert_samples_to_linear(
                float_pixels, (int)width, (int)height, icc_profile,
                icc_profile_length, photometric);
        } else if (decode_mode == TIFF_DECODE_RGB_FLOAT32 ||
            decode_mode == TIFF_DECODE_GRAY_FLOAT32 ||
            decode_mode == TIFF_DECODE_LAB_BYTE ||
            decode_mode == TIFF_DECODE_LAB_UINT ||
            decode_mode == TIFF_DECODE_LAB_FLOAT32) {
            /* Retain device RGB/Gray or Lab through a direct linear apply.
             * Success publishes finite extended RGB. Failure preserves the
             * source samples for the existing format-specific fallback. */
            cms_converted = sixel_cms_convert_to_linearrgb_with_profile_bytes(
                (unsigned char *)float_pixels,
                (int)width,
                (int)height,
                float_pixelformat,
                (unsigned char const *)icc_profile,
                (size_t)icc_profile_length);
        }
    }

    /* RGB, Gray and Lab already contain signed linear CMS output.
     * Only failed Lab conversion should enter the original Lab fallback. */
    if (!cms_converted && decode_mode == TIFF_DECODE_LAB_BYTE) {
        status = tiff_lab8_to_linear(float_pixels, pixel_count);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    } else if (!cms_converted && float_colorspace == SIXEL_COLORSPACE_CIELAB) {
        status = sixel_helper_convert_colorspace((unsigned char *)float_pixels,
                                                 float_bytes,
                                                 float_pixelformat,
                                                 SIXEL_COLORSPACE_CIELAB,
                                                 SIXEL_COLORSPACE_GAMMA);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }

    *result = (unsigned char *)float_pixels;
    *pnegotiate_target = decode_mode == TIFF_DECODE_LOGLUV_FLOAT32 ||
        decode_mode == TIFF_DECODE_LAB_BYTE ||
        (cms_converted && bits_per_sample == 8u &&
         (decode_mode == TIFF_DECODE_RGB_UINT ||
          decode_mode == TIFF_DECODE_GRAY_UINT));
    *ppixelformat = cms_converted || *pnegotiate_target
                  ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                  : SIXEL_PIXELFORMAT_RGBFLOAT32;
    if (!cms_converted && bits_per_sample == 8u &&
        (decode_mode == TIFF_DECODE_RGB_UINT ||
         decode_mode == TIFF_DECODE_GRAY_UINT)) {
        /* These are unchanged original byte samples, not CMS output.
         * Normalization roundoff is far below a half-byte; reconstructing
         * them exactly preserves the unmanaged byte-pipeline behavior.
         * Forward compaction never overwrites an unread float sample. */
        for (component = 0u; component < pixel_count * 3u; ++component) {
            ((unsigned char *)float_pixels)[component] = (unsigned char)(
                (double)float_pixels[component] * 255.0 + 0.5);
        }
        *ppixelformat = SIXEL_PIXELFORMAT_RGB888;
    }
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
 * Decode a TIFF stream into owned byte or float color pixels.
 *
 * The memory-backed TIFF client uses the following flow:
 *
 *    +-----------+     +-----------------+     +--------------------+
 *    | TIFF data | --> | libtiff decode  | --> | byte/float + mask  |
 *    +-----------+     +-----------------+     +--------------------+
 */
static SIXELSTATUS
load_tiff(unsigned char      /* out */ **result,
          unsigned char const /* in */ *buffer,
          size_t             /* in */  size,
          int                /* out */ *pwidth,
          int                /* out */ *pheight,
          int                /* out */ *ppixelformat,
          sixel_allocator_t  /* in */  *allocator,
          int                /* in */  enable_cms,
          int                /* out */ *pnegotiate_target,
          unsigned char      /* out */ **pmask,
          unsigned char const /* in */ *bgcolor)
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
    float *decoded_rgb;
    uint32_t pixel;
    size_t offset;
    SIXELSTATUS high_precision_status;
    uint32_t icc_profile_length;
    void *icc_profile;
    uint16_t photometric;
    uint16_t bits_per_sample;
    uint16_t samples_per_pixel;
    uint16_t sample_format;
    uint16_t extra_samples;
    uint16_t *extra_types;
    int opaque_ycbcr;

    status = SIXEL_TIFF_ERROR;
    tif = NULL;
    raster = NULL;
    pixels = NULL;
    decoded_rgb = NULL;
    width = 0;
    height = 0;
    pixel_count = 0;
    pixel_bytes = 0;
    high_precision_status = SIXEL_FALSE;
    icc_profile_length = 0u;
    icc_profile = NULL;
    photometric = (uint16_t)0xffffu;
    bits_per_sample = samples_per_pixel = extra_samples = 0u;
    sample_format = SAMPLEFORMAT_UINT;
    extra_types = NULL;
    opaque_ycbcr = 0;

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

    high_precision_status = tiff_try_load_integer_layouts(
        &pixels, tif, width, height, ppixelformat,
        enable_cms ? icc_profile : NULL,
        enable_cms ? icc_profile_length : 0u,
        photometric, allocator, pnegotiate_target, pmask, bgcolor,
        pwidth, pheight);
    if (high_precision_status == SIXEL_OK) {
        *result = pixels;
        status = SIXEL_OK;
        goto cleanup;
    }
    if (high_precision_status != SIXEL_FALSE) {
        status = high_precision_status;
        goto cleanup;
    }

    high_precision_status = tiff_try_load_integer_alpha(
        &pixels, tif, width, height, ppixelformat,
        enable_cms ? icc_profile : NULL,
        enable_cms ? icc_profile_length : 0u,
        photometric, allocator, pnegotiate_target, pmask, bgcolor);
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
                                                         allocator,
                                                         pnegotiate_target);
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

    /* Keep the established decoded-RGB profile domain for opaque YCbCr8.
     * libtiff still owns component conversion and its first byte rounding;
     * promoting that raster cannot recover original YCbCr precision. Only
     * the following CMS stage gains an extended linear publication. */
    if (photometric == PHOTOMETRIC_YCBCR) {
        (void)TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE,
                                    &bits_per_sample);
        (void)TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL,
                                    &samples_per_pixel);
        (void)TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT,
                                    &sample_format);
        (void)TIFFGetField(tif, TIFFTAG_EXTRASAMPLES,
                           &extra_samples, &extra_types);
        opaque_ycbcr = bits_per_sample == 8u && samples_per_pixel == 3u &&
            extra_samples == 0u && (sample_format == SAMPLEFORMAT_UINT ||
                                   sample_format == SAMPLEFORMAT_VOID);
        for (index = 0u; opaque_ycbcr && index < pixel_count; ++index) {
            opaque_ycbcr = pixels[index * 4u + 3u] == 255u;
        }
    }
    if (opaque_ycbcr) {
        /* The RGBA source remains intact through optional CMS work. A
         * failed scratch allocation/profile/apply keeps every source byte
         * and must not retry the old bounded byte transform. */
        if (enable_cms && icc_profile != NULL && icc_profile_length > 0u &&
            pixel_count <= SIZE_MAX / (3u * sizeof(float))) {
            decoded_rgb = (float *)sixel_allocator_malloc(
                allocator, pixel_count * 3u * sizeof(float));
            if (decoded_rgb != NULL) {
                for (index = 0u; index < pixel_count * 3u; ++index) {
                    decoded_rgb[index] = (float)((double)pixels[
                        index / 3u * 4u + index % 3u] / 255.0);
                }
                /* The original photometric is YCbCr, but the actual CMS
                 * input is now decoded RGB; require an RGB ICC profile. */
                if (tiff_convert_samples_to_linear(decoded_rgb,
                    (int)width, (int)height, icc_profile,
                    icc_profile_length, PHOTOMETRIC_RGB)) {
                    sixel_allocator_free(allocator, pixels);
                    pixels = (unsigned char *)decoded_rgb;
                    decoded_rgb = NULL;
                    *result = pixels;
                    *pwidth = (int)width;
                    *pheight = (int)height;
                    *ppixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
                    *pnegotiate_target = 1;
                    status = SIXEL_OK;
                    goto cleanup;
                }
                sixel_allocator_free(allocator, decoded_rgb);
                decoded_rgb = NULL;
            }
        }
    } else {
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
    }

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
    sixel_allocator_t         /* in */     *allocator,
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
    unsigned char *mask;
    int pixelformat;
    int negotiate_target;

    status = SIXEL_FALSE;
    frame = NULL;
    pixels = NULL;
    mask = NULL;
    pixelformat = SIXEL_PIXELFORMAT_RGB888;
    negotiate_target = 0;

    (void)fstatic;
    (void)fuse_palette;
    (void)reqcolors;
    (void)loop_control;
    (void)start_frame_no_set;
    (void)start_frame_no;

    if (pchunk == NULL || allocator == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_frame_create_from_factory(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = load_tiff(&pixels,
                       sixel_chunk_get_buffer(pchunk),
                       sixel_chunk_get_size(pchunk),
                       &frame->width,
                       &frame->height,
                       &pixelformat,
                       allocator,
                       enable_cms,
                       &negotiate_target,
                       &mask,
                       bgcolor);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_as_interface(frame)->vtbl->init_pixels(
        sixel_frame_as_interface(frame),
        &(sixel_frame_pixels_request_t){
            pixels,
            NULL,
            frame->width,
            frame->height,
            pixelformat,
            -1,
            -1,
            SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)
            ? SIXEL_FRAME_PIXELS_FLOAT32
            : SIXEL_FRAME_PIXELS_U8
        });
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, pixels);
        goto end;
    }
    pixels = NULL;
    if (mask != NULL) {
        frame->transparent_mask = mask;
        frame->transparent_mask_size =
            (size_t)frame->width * (size_t)frame->height;
        frame->alpha_zero_is_transparent = 1;
        mask = NULL;
    }

    if (!SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        status = sixel_frame_strip_alpha(frame, bgcolor);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (negotiate_target) {
        /* Native XYZ, D50 Lab8 and managed RGB/Gray establish canonical
         * linear samples before target conversion. Alpha-bearing input also
         * resolves background composition before this final boundary.
         * Failed byte ICC input keeps its original straight-source values
         * in gamma byte or float storage, depending on source fractions. */
        status = sixel_frame_set_pixelformat(
            frame, loader_cms_target_pixelformat());
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
    sixel_allocator_free(allocator, mask);
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
    case SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE:
        flag = (int const *)value;
        if (flag != NULL && *flag >= 0) {
            self->enable_cms = (*flag == SIXEL_CMS_ENGINE_NONE) ? 0 : 1;
        }
        sixel_helper_set_loader_cms_engine(flag != NULL ? *flag : -1);
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
    SIXELSTATUS status;
    int header_job_id;
    int decode_job_id;
    sixel_loader_timeline_callback_state_t timeline_state;

    self = NULL;
    bgcolor = NULL;
    status = SIXEL_FALSE;
    header_job_id = -1;
    decode_job_id = -1;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_libtiff_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    header_job_id = loader_timeline_phase_start("header/read");
    decode_job_id = loader_timeline_phase_start("decode/pixels");
    loader_timeline_callback_state_init(&timeline_state,
                                        fn_load,
                                        context,
                                        header_job_id,
                                        decode_job_id);

    status = load_with_libtiff(chunk,
                               self->allocator,
                               self->enable_cms,
                               self->fstatic,
                               self->fuse_palette,
                               self->reqcolors,
                               bgcolor,
                               self->loop_control,
                               self->has_start_frame_no,
                               self->start_frame_no,
                               loader_timeline_emit_frame_callback,
                               &timeline_state);

    loader_timeline_callback_close_header(&timeline_state, status);
    loader_timeline_callback_close_decode(&timeline_state, status);
    loader_timeline_optional_skip_if_unmarked("post/colorspace");
    loader_timeline_optional_skip_if_unmarked("post/background");
    loader_timeline_optional_skip_if_unmarked("post/icc");

    return status;
}

static char const *
sixel_loader_libtiff_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "libtiff";
}

static int
sixel_loader_libtiff_predicate(sixel_loader_component_t *component,
                               sixel_chunk_t const *chunk)
{
    (void)component;
    return loader_can_try_libtiff(chunk);
}

static sixel_loader_component_vtbl_t const g_sixel_loader_libtiff_vtbl = {
    sixel_loader_libtiff_ref,
    sixel_loader_libtiff_unref,
    sixel_loader_libtiff_setopt,
    sixel_loader_libtiff_load,
    sixel_loader_libtiff_name,
    sixel_loader_libtiff_predicate
};

SIXELSTATUS
sixel_loader_libtiff_new(sixel_allocator_t *allocator,
                         void **ppcomponent)
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

static int
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
