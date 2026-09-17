/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * PNG helpers shared by builtin loader path.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#if HAVE_STDINT_H
#include <stdint.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#if HAVE_MATH_H
#include <math.h>
#endif

#include "allocator.h"
#include "chunk-view.h"
#include "cms.h"
#include "compat_stub.h"
#include "frompng.h"
#include "frame-private.h"
#include "loader-common.h"

char const *stbi_failure_reason(void);
void stbi_image_free(void *retval_from_stbi_load);
int stbi_is_16_bit_from_memory(unsigned char const *buffer, int len);
unsigned short *stbi_load_16_from_memory(unsigned char const *buffer,
                                         int len,
                                         int *x,
                                         int *y,
                                         int *channels_in_file,
                                         int desired_channels);
unsigned char *stbi_load_from_memory(unsigned char const *buffer,
                                     int len,
                                     int *x,
                                     int *y,
                                     int *channels_in_file,
                                     int desired_channels);
char *stbi_zlib_decode_malloc_guesssize_headerflag(char const *buffer,
                                                   int len,
                                                   int initial_size,
                                                   int *outlen,
                                                   int parse_header);

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

static int
sixel_frompng_extract_icc(unsigned char const *buffer, size_t size,
                           unsigned char **profile, size_t *profile_length);

static int
sixel_frompng_build_chrm_to_srgb_matrix(uint32_t const chrm_xy[8],
                                        double source_to_srgb[3][3]);

static uint32_t
sixel_frompng_read_be32(unsigned char const *p)
{
    return ((uint32_t)p[0] << 24)
        | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8)
        | (uint32_t)p[3];
}

static uint16_t
sixel_frompng_read_be16(unsigned char const *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static int
sixel_frompng_parse_ihdr(unsigned char const *buffer,
                         size_t size,
                         unsigned char *bitdepth,
                         unsigned char *color_type)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    if (buffer == NULL || bitdepth == NULL || color_type == NULL) {
        return 0;
    }
    if (size < 8u + 12u + 13u) {
        return 0;
    }
    if (memcmp(buffer, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }
    if (sixel_frompng_read_be32(buffer + 8u) != 13u) {
        return 0;
    }
    if (memcmp(buffer + 12u, "IHDR", 4u) != 0) {
        return 0;
    }

    *bitdepth = buffer[24u];
    *color_type = buffer[25u];
    return 1;
}

static void
sixel_frompng_expand_sample_to_bg(uint16_t sample,
                                  unsigned char bitdepth,
                                  unsigned char *out8,
                                  uint16_t *out16)
{
    unsigned int max_value;
    unsigned int value8;
    unsigned int value16;

    value8 = 0u;
    value16 = 0u;
    if (bitdepth == 16u) {
        value16 = (unsigned int)sample;
        value8 = value16 >> 8;
    } else {
        max_value = (1u << bitdepth) - 1u;
        if (max_value == 0u) {
            value8 = 0u;
            value16 = 0u;
        } else {
            value8 = ((unsigned int)sample * 255u + max_value / 2u) / max_value;
            value16 = ((unsigned int)sample * 65535u + max_value / 2u)
                / max_value;
        }
    }
    *out8 = (unsigned char)value8;
    *out16 = (uint16_t)value16;
}

static int
sixel_frompng_parse_bkgd(unsigned char const *buffer,
                         size_t size,
                         unsigned char bg8[3],
                         uint16_t bg16[3])
{
    unsigned char bitdepth;
    unsigned char color_type;
    size_t offset;
    uint32_t chunk_length;
    size_t chunk_total;
    unsigned char const *chunk_type;
    unsigned char const *chunk_data;
    unsigned char const *plte_data;
    size_t plte_entries;
    unsigned char const *bkgd_data;
    uint32_t bkgd_length;
    unsigned int index;
    uint16_t gray16;
    uint16_t red16;
    uint16_t green16;
    uint16_t blue16;
    unsigned char gray8;
    unsigned char red8;
    unsigned char green8;
    unsigned char blue8;

    bitdepth = 0u;
    color_type = 0u;
    offset = 0u;
    chunk_length = 0u;
    chunk_total = 0u;
    chunk_type = NULL;
    chunk_data = NULL;
    plte_data = NULL;
    plte_entries = 0u;
    bkgd_data = NULL;
    bkgd_length = 0u;
    index = 0u;
    gray16 = 0u;
    red16 = 0u;
    green16 = 0u;
    blue16 = 0u;
    gray8 = 0u;
    red8 = 0u;
    green8 = 0u;
    blue8 = 0u;

    if (bg8 == NULL || bg16 == NULL) {
        return 0;
    }
    if (!sixel_frompng_parse_ihdr(buffer, size, &bitdepth, &color_type)) {
        return 0;
    }
    if (bitdepth != 1u &&
        bitdepth != 2u &&
        bitdepth != 4u &&
        bitdepth != 8u &&
        bitdepth != 16u) {
        return 0;
    }

    offset = 8u;
    while (offset + 12u <= size) {
        chunk_length = sixel_frompng_read_be32(buffer + offset);
        chunk_total = (size_t)chunk_length + 12u;
        if (chunk_total > size - offset) {
            return 0;
        }
        chunk_type = buffer + offset + 4u;
        chunk_data = buffer + offset + 8u;

        if (memcmp(chunk_type, "PLTE", 4u) == 0) {
            plte_data = chunk_data;
            plte_entries = (size_t)chunk_length / 3u;
        } else if (memcmp(chunk_type, "bKGD", 4u) == 0) {
            bkgd_data = chunk_data;
            bkgd_length = chunk_length;
        } else if (memcmp(chunk_type, "IEND", 4u) == 0) {
            break;
        }
        offset += chunk_total;
    }

    if (bkgd_data == NULL) {
        return 0;
    }

    switch (color_type) {
    case 0u: /* grayscale */
    case 4u: /* grayscale + alpha */
        if (bkgd_length < 2u) {
            return 0;
        }
        sixel_frompng_expand_sample_to_bg(sixel_frompng_read_be16(bkgd_data),
                                          bitdepth,
                                          &gray8,
                                          &gray16);
        bg8[0] = gray8;
        bg8[1] = gray8;
        bg8[2] = gray8;
        bg16[0] = gray16;
        bg16[1] = gray16;
        bg16[2] = gray16;
        return 1;
    case 2u: /* rgb */
    case 6u: /* rgba */
        if (bkgd_length < 6u) {
            return 0;
        }
        sixel_frompng_expand_sample_to_bg(
            sixel_frompng_read_be16(bkgd_data + 0u),
                                          bitdepth,
                                          &red8,
                                          &red16);
        sixel_frompng_expand_sample_to_bg(
            sixel_frompng_read_be16(bkgd_data + 2u),
                                          bitdepth,
                                          &green8,
                                          &green16);
        sixel_frompng_expand_sample_to_bg(
            sixel_frompng_read_be16(bkgd_data + 4u),
                                          bitdepth,
                                          &blue8,
                                          &blue16);
        bg8[0] = red8;
        bg8[1] = green8;
        bg8[2] = blue8;
        bg16[0] = red16;
        bg16[1] = green16;
        bg16[2] = blue16;
        return 1;
    case 3u: /* indexed */
        if (bkgd_length < 1u || plte_data == NULL) {
            return 0;
        }
        index = (unsigned int)bkgd_data[0];
        if ((size_t)index >= plte_entries) {
            return 0;
        }
        red8 = plte_data[(size_t)index * 3u + 0u];
        green8 = plte_data[(size_t)index * 3u + 1u];
        blue8 = plte_data[(size_t)index * 3u + 2u];
        bg8[0] = red8;
        bg8[1] = green8;
        bg8[2] = blue8;
        bg16[0] = (uint16_t)((unsigned int)red8 * 257u);
        bg16[1] = (uint16_t)((unsigned int)green8 * 257u);
        bg16[2] = (uint16_t)((unsigned int)blue8 * 257u);
        return 1;
    default:
        return 0;
    }
}

static void
sixel_frompng_resolve_background(unsigned char const *buffer,
                                 size_t size,
                                 unsigned char const *bgcolor,
                                 int bgcolor_source,
                                 unsigned char bg8[3],
                                 uint16_t bg16[3],
                                 int *background_from_file,
                                 int *has_background)
{
    unsigned char file_bg8[3];
    uint16_t file_bg16[3];
    int has_file_background;
    int has_explicit_background;
    int policy;
    int use_file_background;
    int use_explicit_background;

    file_bg8[0] = 0u;
    file_bg8[1] = 0u;
    file_bg8[2] = 0u;
    file_bg16[0] = 0u;
    file_bg16[1] = 0u;
    file_bg16[2] = 0u;
    has_file_background = 0;
    has_explicit_background = 0;
    policy = SIXEL_LOADER_BACKGROUND_POLICY_FILE_FIRST;
    use_file_background = 0;
    use_explicit_background = 0;
    if (background_from_file != NULL) {
        *background_from_file = 0;
    }
    if (has_background != NULL) {
        *has_background = 0;
    }
    if (bgcolor != NULL) {
        has_explicit_background = 1;
    }
    has_file_background = sixel_frompng_parse_bkgd(buffer,
                                                   size,
                                                   file_bg8,
                                                   file_bg16);
    policy = loader_background_policy();
    if (policy == SIXEL_LOADER_BACKGROUND_POLICY_EXPLICIT_FIRST) {
        if (has_explicit_background != 0) {
            use_explicit_background = 1;
        } else if (has_file_background != 0) {
            use_file_background = 1;
        }
    } else {
        if (has_file_background != 0) {
            use_file_background = 1;
        } else if (has_explicit_background != 0) {
            use_explicit_background = 1;
        }
    }

    if (use_explicit_background != 0) {
        (void)bgcolor_source;
        bg8[0] = bgcolor[0];
        bg8[1] = bgcolor[1];
        bg8[2] = bgcolor[2];
        bg16[0] = (uint16_t)((unsigned int)bgcolor[0] * 257u);
        bg16[1] = (uint16_t)((unsigned int)bgcolor[1] * 257u);
        bg16[2] = (uint16_t)((unsigned int)bgcolor[2] * 257u);
        if (has_background != NULL) {
            *has_background = 1;
        }
        return;
    }
    if (use_file_background != 0) {
        bg8[0] = file_bg8[0];
        bg8[1] = file_bg8[1];
        bg8[2] = file_bg8[2];
        bg16[0] = file_bg16[0];
        bg16[1] = file_bg16[1];
        bg16[2] = file_bg16[2];
        if (background_from_file != NULL) {
            *background_from_file = 1;
        }
        if (has_background != NULL) {
            *has_background = 1;
        }
        return;
    }

    bg8[0] = 0u;
    bg8[1] = 0u;
    bg8[2] = 0u;
    bg16[0] = 0u;
    bg16[1] = 0u;
    bg16[2] = 0u;
}

static int
sixel_frompng_detect_chunk_flags(unsigned char const *buffer,
                                 size_t size,
                                 int *has_iccp,
                                 int *has_srgb,
                                 int *has_chrm,
                                 int *has_gama)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;
    uint32_t chunk_length;
    size_t chunk_total;
    unsigned char const *chunk_type;

    if (has_iccp == NULL || has_srgb == NULL || has_chrm == NULL
            || has_gama == NULL) {
        return 0;
    }

    *has_iccp = 0;
    *has_srgb = 0;
    *has_chrm = 0;
    *has_gama = 0;

    if (buffer == NULL || size < sizeof(png_signature) + 12u) {
        return 0;
    }
    if (memcmp(buffer, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }

    offset = sizeof(png_signature);
    while (offset + 12u <= size) {
        chunk_length = sixel_frompng_read_be32(buffer + offset);
        chunk_total = (size_t)chunk_length + 12u;
        if (chunk_total > size - offset) {
            return 0;
        }
        chunk_type = buffer + offset + 4u;

        if (memcmp(chunk_type, "iCCP", 4u) == 0) {
            *has_iccp = 1;
        } else if (memcmp(chunk_type, "sRGB", 4u) == 0) {
            *has_srgb = 1;
        } else if (memcmp(chunk_type, "cHRM", 4u) == 0
#if !HAVE_LCMS2
                   && chunk_length >= 32u
#endif
                   ) {
            *has_chrm = 1;
        } else if (memcmp(chunk_type, "gAMA", 4u) == 0
#if !HAVE_LCMS2
                   && chunk_length >= 4u
#endif
                   ) {
            *has_gama = 1;
        }
#if !HAVE_LCMS2
        /* Preserve the original bounded metadata policy in this build. */
        if (memcmp(chunk_type, "IEND", 4u) == 0) {
            break;
        }
#endif
        offset += chunk_total;
    }

    return 1;
}

static int
sixel_frompng_build_profile_from_chunks(unsigned char const *buffer,
                                        size_t size,
                                        sixel_cms_profile_t **profile,
                                        int *invalid_chrm)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    sixel_cms_profile_t *built_profile;
    size_t offset;
    uint32_t chunk_length;
    size_t chunk_total;
    unsigned char const *chunk_type;
    unsigned char const *chunk_data;
    int has_chrm;
    int has_gama;
    double white_x;
    double white_y;
    double red_x;
    double red_y;
    double green_x;
    double green_y;
    double blue_x;
    double blue_y;
    double file_gamma;
    double validation_matrix[3][3];
    uint32_t chrm_grid[8];
    int i;

    if (profile == NULL) {
        return 0;
    }
    *profile = NULL;
    if (invalid_chrm != NULL) {
        *invalid_chrm = 0;
    }
    if (buffer == NULL || size < sizeof(png_signature) + 12u) {
        return 0;
    }
    if (memcmp(buffer, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }

    built_profile = NULL;
    has_chrm = 0;
    has_gama = 0;
    white_x = 0.0;
    white_y = 0.0;
    red_x = 0.0;
    red_y = 0.0;
    green_x = 0.0;
    green_y = 0.0;
    blue_x = 0.0;
    blue_y = 0.0;
    file_gamma = 0.0;

    offset = sizeof(png_signature);
    while (offset + 12u <= size) {
        chunk_length = sixel_frompng_read_be32(buffer + offset);
        chunk_total = (size_t)chunk_length + 12u;
        if (chunk_total > size - offset) {
            return 0;
        }
        chunk_type = buffer + offset + 4u;
        chunk_data = buffer + offset + 8u;

        if (memcmp(chunk_type, "cHRM", 4u) == 0 && chunk_length >= 32u) {
            white_x = (double)sixel_frompng_read_be32(chunk_data + 0u)
                / 100000.0;
            white_y = (double)sixel_frompng_read_be32(chunk_data + 4u)
                / 100000.0;
            red_x = (double)sixel_frompng_read_be32(chunk_data + 8u)
                / 100000.0;
            red_y = (double)sixel_frompng_read_be32(chunk_data + 12u)
                / 100000.0;
            green_x = (double)sixel_frompng_read_be32(chunk_data + 16u)
                / 100000.0;
            green_y = (double)sixel_frompng_read_be32(chunk_data + 20u)
                / 100000.0;
            blue_x = (double)sixel_frompng_read_be32(chunk_data + 24u)
                / 100000.0;
            blue_y = (double)sixel_frompng_read_be32(chunk_data + 28u)
                / 100000.0;
            for (i = 0; i < 8; ++i) {
                chrm_grid[i] = sixel_frompng_read_be32(chunk_data + 4 * i);
            }
            has_chrm = 1;
        } else if (memcmp(chunk_type, "gAMA", 4u) == 0
                   && chunk_length >= 4u) {
            file_gamma = (double)sixel_frompng_read_be32(chunk_data)
                / 100000.0;
            has_gama = 1;
        }
#if !HAVE_LCMS2
        if (memcmp(chunk_type, "IEND", 4u) == 0) {
            break;
        }
#endif
        offset += chunk_total;
    }

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
    if (file_gamma <= 0.0) {
        return 0;
    }

    /* Metadata rejection is independent of a later allocation or CMM
     * failure. Only this result authorizes the opaque gAMA fallback. */
    if (has_chrm &&
        !sixel_frompng_build_chrm_to_srgb_matrix(chrm_grid,
                                                  validation_matrix)) {
        if (invalid_chrm != NULL) {
            *invalid_chrm = 1;
            return 0;
        }
#if !HAVE_LCMS2
        /* The byte helper historically ignores invalid primaries but still
         * honors gAMA. Build that interpretation through the same facade;
         * this is metadata rejection, never a retry after a CMM failure. */
        white_x = 0.3127;
        white_y = 0.3290;
        red_x = 0.6400;
        red_y = 0.3300;
        green_x = 0.3000;
        green_y = 0.6000;
        blue_x = 0.1500;
        blue_y = 0.0600;
#endif
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
sixel_frompng_apply_colorspace_fallback_internal(unsigned char *pixels,
                                                 int width,
                                                 int height,
                                                 int pixelformat,
                                                 unsigned char const *buffer,
                                                 size_t size,
                                                 sixel_allocator_t *allocator)
{
    unsigned char *icc_profile;
    unsigned char *staged;
    size_t pixel_count;
    size_t pixel_bytes;
    size_t sample_bytes;
    size_t icc_profile_length;
    sixel_cms_profile_t *chunk_profile;
    int has_iccp;
    int has_srgb;
    int has_chrm;
    int has_gama;
    int converted;

    staged = NULL;
    icc_profile = NULL;
    icc_profile_length = 0u;
    chunk_profile = NULL;
    has_iccp = 0;
    has_srgb = 0;
    has_chrm = 0;
    has_gama = 0;
    converted = 0;

    if (pixels == NULL || width <= 0 || height <= 0
            || buffer == NULL || allocator == NULL) {
        return 0;
    }
    if (pixelformat != SIXEL_PIXELFORMAT_RGB888 &&
        pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32) {
        return 0;
    }
    sample_bytes = pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32
        ? sizeof(float) : 1u;
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return 0;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / (3u * sample_bytes)) {
        return 0;
    }
    pixel_bytes = pixel_count * 3u * sample_bytes;
    if (!sixel_frompng_detect_chunk_flags(buffer,
                                          size,
                                          &has_iccp,
                                          &has_srgb,
                                          &has_chrm,
                                          &has_gama)) {
        return 0;
    }
    if (has_iccp && has_srgb && has_chrm) {
        return 0;
    }
    if (!has_iccp && (has_srgb || !has_gama)) {
        return 0;
    }
    /* CMM failure can leave a modified prefix. Keep all source samples owned
     * by the caller intact until the complete transform has succeeded. */
    staged = sixel_allocator_malloc(allocator, pixel_bytes);
    if (staged == NULL) {
        return 0;
    }
    memcpy(staged, pixels, pixel_bytes);
    if (has_iccp) {
        if (sixel_frompng_extract_icc(buffer,
                                      size,
                                      &icc_profile,
                                      &icc_profile_length)) {
            converted = sixel_cms_convert_to_srgb_with_profile_bytes(
                staged,
                width,
                height,
                pixelformat,
                icc_profile,
                icc_profile_length);
        }
    } else if (has_srgb) {
        /* no-op */
    } else if (has_gama
               && sixel_frompng_build_profile_from_chunks(buffer,
                                                          size,
                &chunk_profile, NULL)) {
        converted = sixel_cms_convert_profile_to_srgb(
            staged,
            width,
            height,
            pixelformat,
            chunk_profile);
        sixel_cms_close_profile(chunk_profile);
    }
    if (converted) {
        memcpy(pixels, staged, pixel_bytes);
    }
    stbi_image_free(icc_profile);
    sixel_allocator_free(allocator, staged);
    return converted;
}

void
sixel_frompng_apply_colorspace_fallback(unsigned char *pixels,
                                        int width,
                                        int height,
                                        unsigned char const *buffer,
                                        size_t size,
                                        sixel_allocator_t *allocator)
{
    (void)sixel_frompng_apply_colorspace_fallback_internal(
        pixels,
        width,
        height,
        SIXEL_PIXELFORMAT_RGB888,
        buffer,
        size,
        allocator);
}

int
sixel_frompng_apply_colorspace_fallback_pixelformat(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    unsigned char const *buffer,
    size_t size,
    sixel_allocator_t *allocator)
{
    return sixel_frompng_apply_colorspace_fallback_internal(pixels,
                                                            width,
                                                            height,
                                                            pixelformat,
                                                            buffer,
                                                            size,
                                                            allocator);
}

static int
sixel_frompng_extract_icc(unsigned char const *buffer,
                          size_t size,
                          unsigned char **profile,
                          size_t *profile_length)
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;
    uint32_t chunk_length;
    unsigned char const *chunk_data;
    size_t chunk_total;
    size_t name_index;
    size_t compressed_offset;
    unsigned char compression_method;
    unsigned char *decoded;
    int decoded_length;

    if (profile == NULL || profile_length == NULL) {
        return 0;
    }
    *profile = NULL;
    *profile_length = 0u;

    if (buffer == NULL || size < sizeof(png_signature) + 12u) {
        return 0;
    }
    if (memcmp(buffer, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }

    offset = sizeof(png_signature);
    while (offset + 12u <= size) {
        chunk_length = sixel_frompng_read_be32(buffer + offset);
        chunk_total = (size_t)chunk_length + 12u;
        if (chunk_total > size - offset) {
            return 0;
        }
        if (memcmp(buffer + offset + 4u, "iCCP", 4u) != 0) {
#if !HAVE_LCMS2
            /* Match the private parser's former end-of-container boundary. */
            if (memcmp(buffer + offset + 4u, "IEND", 4u) == 0) {
                break;
            }
#endif
            offset += chunk_total;
            continue;
        }

        chunk_data = buffer + offset + 8u;
        if (chunk_length < 3u) {
            return 0;
        }
        name_index = 0u;
        while (name_index < (size_t)chunk_length
                && chunk_data[name_index] != 0u) {
            ++name_index;
        }
        if (name_index == (size_t)chunk_length) {
            return 0;
        }
        compressed_offset = name_index + 1u;
        if (compressed_offset >= (size_t)chunk_length) {
            return 0;
        }
        compression_method = chunk_data[compressed_offset];
        if (compression_method != 0u) {
            return 0;
        }
        ++compressed_offset;
        if (compressed_offset >= (size_t)chunk_length) {
            return 0;
        }

        decoded = (unsigned char *)stbi_zlib_decode_malloc_guesssize_headerflag(
            (char const *)(chunk_data + compressed_offset),
            (int)((size_t)chunk_length - compressed_offset),
            16384,
            &decoded_length,
            1);
        if (decoded == NULL || decoded_length <= 0) {
            stbi_image_free(decoded);
            return 0;
        }
        *profile = decoded;
        *profile_length = (size_t)decoded_length;
        return 1;
    }

    return 0;
}

enum {
    SIXEL_FROMPNG_TRANSFER_SRGB = 0,
    SIXEL_FROMPNG_TRANSFER_GAMA = 1
};

static double
sixel_frompng_clamp_unit(double value)
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
sixel_frompng_decode_srgb_unit(double value)
{
    value = sixel_frompng_clamp_unit(value);
    if (value <= 0.04045) {
        return value / 12.92;
    }
    return pow((value + 0.055) / 1.055, 2.4);
}

static double
sixel_frompng_decode_source_unit(double value, int transfer_mode,
                                  double file_gamma)
{
    value = sixel_frompng_clamp_unit(value);
    if (transfer_mode == SIXEL_FROMPNG_TRANSFER_GAMA && file_gamma > 0.0) {
        return pow(value, 1.0 / file_gamma);
    }
    return sixel_frompng_decode_srgb_unit(value);
}

static int
sixel_frompng_parse_transfer_chunks(unsigned char const *buffer,
                                    size_t size,
                                    int *has_srgb,
                                    int *has_gama,
                                    double *file_gamma,
                                    int *has_iccp,
                                    int *has_chrm,
                                    uint32_t chrm_xy[8])
{
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;
    uint32_t chunk_length;
    size_t chunk_total;
    unsigned char const *chunk_type;
    unsigned char const *chunk_data;

    if (has_srgb == NULL || has_gama == NULL || file_gamma == NULL) {
        return 0;
    }
    *has_srgb = 0;
    *has_gama = 0;
    *file_gamma = 0.0;
    if (has_iccp != NULL) {
        *has_iccp = 0;
    }
    if (has_chrm != NULL) {
        *has_chrm = 0;
    }
    if (chrm_xy != NULL) {
        memset(chrm_xy, 0, sizeof(uint32_t) * 8u);
    }

    if (buffer == NULL || size < sizeof(png_signature) + 12u) {
        return 0;
    }
    if (memcmp(buffer, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }

    offset = sizeof(png_signature);
    while (offset + 12u <= size) {
        chunk_length = sixel_frompng_read_be32(buffer + offset);
        chunk_total = (size_t)chunk_length + 12u;
        if (chunk_total > size - offset) {
            return 0;
        }

        chunk_type = buffer + offset + 4u;
        chunk_data = buffer + offset + 8u;
        if (memcmp(chunk_type, "iCCP", 4u) == 0) {
            if (has_iccp != NULL) {
                *has_iccp = 1;
            }
        } else if (memcmp(chunk_type, "sRGB", 4u) == 0) {
            *has_srgb = 1;
        } else if (memcmp(chunk_type, "cHRM", 4u) == 0 && chunk_length >= 32u) {
            if (has_chrm != NULL) {
                *has_chrm = 1;
            }
            if (chrm_xy != NULL) {
                chrm_xy[0] = sixel_frompng_read_be32(chunk_data + 0u);
                chrm_xy[1] = sixel_frompng_read_be32(chunk_data + 4u);
                chrm_xy[2] = sixel_frompng_read_be32(chunk_data + 8u);
                chrm_xy[3] = sixel_frompng_read_be32(chunk_data + 12u);
                chrm_xy[4] = sixel_frompng_read_be32(chunk_data + 16u);
                chrm_xy[5] = sixel_frompng_read_be32(chunk_data + 20u);
                chrm_xy[6] = sixel_frompng_read_be32(chunk_data + 24u);
                chrm_xy[7] = sixel_frompng_read_be32(chunk_data + 28u);
            }
        } else if (memcmp(chunk_type, "gAMA", 4u) == 0 && chunk_length >= 4u) {
            *has_gama = 1;
            *file_gamma = (double)sixel_frompng_read_be32(chunk_data)
                / 100000.0;
        } else if (memcmp(chunk_type, "IEND", 4u) == 0) {
            break;
        }

        offset += chunk_total;
    }

    return 1;
}

static int
sixel_frompng_invert_3x3(double in[3][3], double out[3][3])
{
    double det;
    double inv_det;

    det = in[0][0] * (in[1][1] * in[2][2] - in[1][2] * in[2][1])
        - in[0][1] * (in[1][0] * in[2][2] - in[1][2] * in[2][0])
        + in[0][2] * (in[1][0] * in[2][1] - in[1][1] * in[2][0]);
    if (!isfinite(det) || fabs(det) < 1.0e-12) {
        return 0;
    }
    inv_det = 1.0 / det;

    out[0][0] =  (in[1][1] * in[2][2] - in[1][2] * in[2][1]) * inv_det;
    out[0][1] = -(in[0][1] * in[2][2] - in[0][2] * in[2][1]) * inv_det;
    out[0][2] =  (in[0][1] * in[1][2] - in[0][2] * in[1][1]) * inv_det;
    out[1][0] = -(in[1][0] * in[2][2] - in[1][2] * in[2][0]) * inv_det;
    out[1][1] =  (in[0][0] * in[2][2] - in[0][2] * in[2][0]) * inv_det;
    out[1][2] = -(in[0][0] * in[1][2] - in[0][2] * in[1][0]) * inv_det;
    out[2][0] =  (in[1][0] * in[2][1] - in[1][1] * in[2][0]) * inv_det;
    out[2][1] = -(in[0][0] * in[2][1] - in[0][1] * in[2][0]) * inv_det;
    out[2][2] =  (in[0][0] * in[1][1] - in[0][1] * in[1][0]) * inv_det;
    return 1;
}

static int
sixel_frompng_build_chrm_to_srgb_matrix(uint32_t const chrm_xy[8],
                                        double source_to_srgb[3][3])
{
    static double const xyz_to_srgb[3][3] = {
        { 3.240969941904521, -1.537383177570093, -0.498610760293003 },
        { -0.969243636280880, 1.875967501507721, 0.041555057407176 },
        { 0.055630079696993, -0.203976958888977, 1.056971514242878 }
    };
    double bradford[3][3] = {
        { 0.8951, 0.2664, -0.1614 },
        { -0.7502, 1.7135, 0.0367 },
        { 0.0389, -0.0685, 1.0296 }
    };
    double xyz[4][3];
    double primaries[3][3];
    double inverse[3][3];
    double bradford_inverse[3][3];
    double adapted[3][3];
    double result[3][3];
    double target_white[3];
    double scale[3];
    double ratio[3];
    double cone[3];
    double source_cone;
    double target_cone;
    uint32_t x;
    uint32_t y;
    uint32_t z;
    int row;
    int col;
    int i;

    if (chrm_xy == NULL || source_to_srgb == NULL) {
        return 0;
    }
    for (i = 0; i < 4; ++i) {
        x = chrm_xy[2 * i];
        y = chrm_xy[2 * i + 1];
        if (x > 100000u || y == 0u || y > 100000u - x) {
            return 0;
        }
        /* cHRM uses an integer grid. Subtract there before division: a
         * boundary primary has exact Z=0, not a floating-point residual. */
        z = 100000u - x - y;
        xyz[i][0] = (double)x / y;
        xyz[i][1] = 1.0;
        xyz[i][2] = (double)z / y;
    }
    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 3; ++col) {
            primaries[row][col] = xyz[col + 1][row];
        }
    }
    if (!sixel_frompng_invert_3x3(primaries, inverse) ||
        !sixel_frompng_invert_3x3(bradford, bradford_inverse)) {
        return 0;
    }
    for (row = 0; row < 3; ++row) {
        scale[row] = inverse[row][0] * xyz[0][0] +
            inverse[row][1] + inverse[row][2] * xyz[0][2];
        if (!isfinite(scale[row]) || scale[row] <= 0.0) {
            return 0;
        }
    }
    target_white[0] = 31270.0 / 32900.0;
    target_white[1] = 1.0;
    target_white[2] = 35830.0 / 32900.0;
    for (row = 0; row < 3; ++row) {
        source_cone = 0.0;
        target_cone = 0.0;
        for (i = 0; i < 3; ++i) {
            source_cone += bradford[row][i] * xyz[0][i];
            target_cone += bradford[row][i] * target_white[i];
        }
        if (!isfinite(source_cone) || source_cone <= 0.0 ||
            !isfinite(target_cone) || target_cone <= 0.0) {
            return 0;
        }
        ratio[row] = target_cone / source_cone;
    }
    /* Adapt each source primary to D65 before the sRGB matrix. The source
     * white and primary columns must undergo the same Bradford transform. */
    for (col = 0; col < 3; ++col) {
        for (row = 0; row < 3; ++row) {
            cone[row] = 0.0;
            for (i = 0; i < 3; ++i) {
                cone[row] += bradford[row][i] *
                    primaries[i][col] * scale[col];
            }
            cone[row] *= ratio[row];
        }
        for (row = 0; row < 3; ++row) {
            adapted[row][col] = bradford_inverse[row][0] * cone[0] +
                bradford_inverse[row][1] * cone[1] +
                bradford_inverse[row][2] * cone[2];
        }
    }
    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 3; ++col) {
            result[row][col] = xyz_to_srgb[row][0] * adapted[0][col] +
                xyz_to_srgb[row][1] * adapted[1][col] +
                xyz_to_srgb[row][2] * adapted[2][col];
            if (!isfinite(result[row][col])) {
                return 0;
            }
        }
    }
    memcpy(source_to_srgb, result, sizeof(result));
    return 1;
}

/* Container fallback decoding is independent of CMS. A failed transform
 * may still need the source transfer for alpha composition, but never a
 * second profile evaluator or a second source-to-destination matrix. */
typedef struct {
    sixel_cms_profile_t *profile;
    int transfer_mode;
    double file_gamma;
} sixel_frompng_source_t;

static void
sixel_frompng_select_source(unsigned char const *buffer, size_t size,
                             int enable_cms, int has_alpha,
                             sixel_frompng_source_t *source)
{
    unsigned char *icc;
    size_t icc_size;
    int has_srgb;
    int has_gama;
    int has_iccp;
    int has_chrm;
    int flags_valid;
    int flag_iccp;
    int flag_srgb;
    int flag_chrm;
    int flag_gama;
    int invalid_chrm;
    uint32_t chrm_xy[8];

    memset(source, 0, sizeof(*source));
    if (!enable_cms) {
        return;
    }
    icc = NULL;
    icc_size = 0u;
    (void)sixel_frompng_parse_transfer_chunks(buffer, size, &has_srgb,
        &has_gama, &source->file_gamma, &has_iccp, &has_chrm, chrm_xy);
    /* Preserve each build's metadata coexistence and end-of-container
     * policy while routing the selected interpretation to the same CMS. */
    invalid_chrm = 0;
    flags_valid = sixel_frompng_detect_chunk_flags(buffer, size,
        &flag_iccp, &flag_srgb, &flag_chrm, &flag_gama);
    if (flags_valid && !(flag_iccp && flag_srgb && flag_chrm)) {
        if (flag_iccp && sixel_frompng_extract_icc(buffer, size,
                                                    &icc, &icc_size)) {
            source->profile = sixel_cms_open_profile_from_mem(icc, icc_size);
        } else if (!flag_iccp && !flag_srgb && flag_gama) {
            (void)sixel_frompng_build_profile_from_chunks(buffer, size,
                                                          &source->profile,
                                                          &invalid_chrm);
        }
    }
    /* Preserve the alpha failure fallback. An opaque image gains gAMA
     * only after independently rejecting unusable cHRM metadata above. */
    if ((has_alpha || invalid_chrm) && !has_srgb && has_gama &&
        source->file_gamma > 0.0) {
        source->transfer_mode = SIXEL_FROMPNG_TRANSFER_GAMA;
    }
    stbi_image_free(icc);
}

static void
sixel_frompng_normalize_samples(float *pixels,
                                 unsigned char const *samples8,
                                 uint16_t const *samples16,
                                 size_t count, int depth)
{
    size_t i;
    int c;

    for (i = 0u; i < count; ++i) {
        for (c = 0; c < 3; ++c) {
            pixels[i * 3u + c] = samples16 != NULL
                ? (float)((double)samples16[i * 4u + c] / 65535.0)
                : (float)((double)samples8[i * depth + c] / 255.0);
        }
    }
}

static void
sixel_frompng_decode_samples(float *pixels, size_t count,
                              sixel_frompng_source_t const *source)
{
    size_t i;

    for (i = 0u; i < count * 3u; ++i) {
        pixels[i] = (float)sixel_frompng_decode_source_unit(pixels[i],
            source->transfer_mode, source->file_gamma);
    }
}

static int
sixel_frompng_has_transparency(unsigned char const *buffer, size_t size)
{
    unsigned char bitdepth;
    unsigned char color_type;
    static unsigned char const png_signature[8] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au
    };
    size_t offset;
    uint32_t chunk_length;
    size_t chunk_total;
    unsigned char const *chunk_type;

    bitdepth = 0u;
    color_type = 0u;
    offset = 0u;
    chunk_length = 0u;
    chunk_total = 0u;
    chunk_type = NULL;

    if (!sixel_frompng_parse_ihdr(buffer, size, &bitdepth, &color_type)) {
        return 0;
    }
    (void)bitdepth;
    if (color_type == 4u || color_type == 6u) {
        return 1;
    }
    if (buffer == NULL || size < sizeof(png_signature) + 12u) {
        return 0;
    }
    if (memcmp(buffer, png_signature, sizeof(png_signature)) != 0) {
        return 0;
    }

    offset = sizeof(png_signature);
    while (offset + 12u <= size) {
        chunk_length = sixel_frompng_read_be32(buffer + offset);
        chunk_total = (size_t)chunk_length + 12u;
        if (chunk_total > size - offset) {
            return 0;
        }
        chunk_type = buffer + offset + 4u;
        if (memcmp(chunk_type, "tRNS", 4u) == 0) {
            return 1;
        }
        if (memcmp(chunk_type, "IDAT", 4u) == 0 ||
            memcmp(chunk_type, "IEND", 4u) == 0) {
            break;
        }
        offset += chunk_total;
    }

    return 0;
}

SIXELSTATUS
sixel_frompng_load_nonindexed(sixel_chunk_t const *pchunk,
                              sixel_allocator_t *allocator,
                              sixel_frame_t *frame,
                              int enable_cms,
                              unsigned char const *bgcolor,
                              int bgcolor_source)
{
    SIXELSTATUS status;
    sixel_frompng_source_t source;
    unsigned char const *buffer;
    size_t size;
    int depth;
    int has_alpha;
    int is_16bit;
    int from_file;
    int has_background;
    int policy;
    int preserve_zero;
    int has_zero;
    int converted;
    int background_converted;
    int linear;
    int format;
    int target;
    unsigned char background8[3];
    uint16_t background16[3];
    float background[3];
    uint16_t *samples16;
    unsigned char *samples8;
    unsigned char *mask;
    unsigned char *output;
    float *pixels;
    size_t count;
    size_t i;
    int c;
    double alpha;

    if (pchunk == NULL || allocator == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    buffer = sixel_chunk_get_buffer(pchunk);
    size = sixel_chunk_get_size(pchunk);
    if (size > INT_MAX) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    status = SIXEL_OK;
    samples16 = NULL;
    samples8 = NULL;
    pixels = NULL;
    mask = NULL;
    has_zero = 0;
    converted = 0;
    background_converted = 1;
    from_file = 0;
    has_background = 0;
    memset(&source, 0, sizeof(source));
    sixel_frompng_resolve_background(buffer, size, bgcolor, bgcolor_source,
        background8, background16, &from_file, &has_background);
    has_alpha = sixel_frompng_has_transparency(buffer, size);
    policy = loader_transparent_policy();
    /* A zero-alpha mask can be removed only after actual composition. */
    preserve_zero = !has_background ||
        SIXEL_LOADER_ALPHA_POLICY_PRESERVES_ZERO(policy);
    is_16bit = stbi_is_16_bit_from_memory(buffer, (int)size);
    if (is_16bit) {
        samples16 = stbi_load_16_from_memory(buffer, (int)size,
            &frame->width, &frame->height, &depth, 4);
    } else {
        samples8 = stbi_load_from_memory(buffer, (int)size,
            &frame->width, &frame->height, &depth, has_alpha ? 4 : 3);
    }
    if (samples16 == NULL && samples8 == NULL) {
        sixel_helper_set_additional_message(stbi_failure_reason());
        return SIXEL_STBI_ERROR;
    }
    depth = has_alpha || is_16bit ? 4 : 3;
    if (frame->width <= 0 || frame->height <= 0 ||
        (size_t)frame->width > SIZE_MAX / (size_t)frame->height) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    count = (size_t)frame->width * (size_t)frame->height;
    if (count > SIZE_MAX / (3u * sizeof(float))) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    sixel_frompng_select_source(buffer, size, enable_cms, has_alpha, &source);
    linear = has_alpha || source.transfer_mode == SIXEL_FROMPNG_TRANSFER_GAMA;
    if (linear || is_16bit || source.profile != NULL) {
        pixels = (float *)sixel_allocator_malloc(allocator,
                                                  count * 3u * sizeof(float));
        if (pixels == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        sixel_frompng_normalize_samples(pixels, samples8, samples16,
                                          count, depth);
        for (c = 0; c < 3; ++c) {
            background[c] = is_16bit
                ? (float)((double)background16[c] / 65535.0)
                : (float)((double)background8[c] / 255.0);
        }
        if (source.profile != NULL) {
            converted = sixel_cms_convert_profile_to_linearrgb(
                (unsigned char *)pixels, frame->width, frame->height,
                SIXEL_PIXELFORMAT_RGBFLOAT32, source.profile);
            if (from_file && has_alpha && has_background) {
                /* Both applications use the same selected source profile.
                 * Neither result is published until both have succeeded. */
                background_converted = sixel_cms_convert_profile_to_linearrgb(
                    (unsigned char *)background, 1, 1,
                    SIXEL_PIXELFORMAT_RGBFLOAT32, source.profile);
            }
            converted = converted && background_converted;
            if (!converted) {
                /* The image may have succeeded before the background failed,
                 * or vice versa. Restore both from the original integers;
                 * fallback must never decode a partially managed image. */
                sixel_frompng_normalize_samples(pixels, samples8, samples16,
                                                  count, depth);
                for (c = 0; c < 3; ++c) {
                    background[c] = is_16bit
                        ? (float)((double)background16[c] / 65535.0)
                        : (float)((double)background8[c] / 255.0);
                }
            }
        }
        linear = linear || converted;
        if (linear && !converted) {
            sixel_frompng_decode_samples(pixels, count, &source);
        }
        if (has_alpha && has_background) {
            if (from_file) {
                if (!converted) {
                    sixel_frompng_decode_samples(background, 1u, &source);
                }
            } else if (loader_background_colorspace() !=
                       SIXEL_COLORSPACE_LINEAR) {
                /* An explicit background has its own declared transfer;
                 * it never inherits the file's ICC profile or gAMA. */
                for (c = 0; c < 3; ++c) {
                    background[c] = (float)sixel_frompng_decode_srgb_unit(
                        background[c]);
                }
            }
        }
    }
    if (has_alpha && preserve_zero) {
        mask = (unsigned char *)sixel_allocator_malloc(allocator, count);
        if (mask == NULL) {
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
    }
    if (has_alpha) {
        for (i = 0u; i < count; ++i) {
            /* Alpha remains a normalized coverage sample. Color transfer and
             * profile evaluation above process RGB channels only. */
            alpha = is_16bit ? (double)samples16[i * 4u + 3u] / 65535.0
                            : (double)samples8[i * 4u + 3u] / 255.0;
            if (mask != NULL) {
                mask[i] = alpha == 0.0;
                has_zero |= mask[i];
            }
            if (!has_background || (preserve_zero && alpha == 0.0)) {
                continue;
            }
            for (c = 0; c < 3; ++c) {
                pixels[i * 3u + c] = (float)(pixels[i * 3u + c] * alpha +
                    background[c] * (1.0 - alpha));
            }
        }
    }
    /* Preserve the ordinary untagged eight-bit path. A failed profile does
     * not turn unchanged device bytes into a falsely managed float frame. */
    if (!linear && !is_16bit) {
        sixel_allocator_free(allocator, pixels);
        pixels = NULL;
        output = samples8;
        format = SIXEL_PIXELFORMAT_RGB888;
    } else {
        output = (unsigned char *)pixels;
        format = linear ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
                        : SIXEL_PIXELFORMAT_RGBFLOAT32;
    }
    status = sixel_frame_as_interface(frame)->vtbl->init_pixels(
        sixel_frame_as_interface(frame),
        &(sixel_frame_pixels_request_t){
            output, NULL, frame->width, frame->height, format,
            linear ? SIXEL_COLORSPACE_LINEAR : SIXEL_COLORSPACE_GAMMA,
            -1, pixels != NULL ? SIXEL_FRAME_PIXELS_FLOAT32
                               : SIXEL_FRAME_PIXELS_U8
        });
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (pixels != NULL) {
        pixels = NULL;
    } else {
        samples8 = NULL;
    }
    frame->loop_count = 1;
    frame->transparent = -1;
    frame->alpha_zero_is_transparent = has_zero;
    if (has_zero) {
        frame->transparent_mask = mask;
        frame->transparent_mask_size = count;
        mask = NULL;
    }
    if (converted || source.transfer_mode == SIXEL_FROMPNG_TRANSFER_GAMA) {
        target = loader_cms_target_pixelformat();
        /* The setter keeps gamma in double until the final byte rounding.
         * An intermediate gamma float store could cross a half-byte tie. */
        status = sixel_frame_set_pixelformat(frame, target);
    }

cleanup:
    sixel_cms_close_profile(source.profile);
    stbi_image_free(samples16);
    stbi_image_free(samples8);
    sixel_allocator_free(allocator, pixels);
    sixel_allocator_free(allocator, mask);
    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
