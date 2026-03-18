/*
 * SPDX-License-Identifier: MIT
 *
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
#include "cms.h"
#include "compat_stub.h"
#include "frompng.h"
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

#if HAVE_LCMS2
static int
sixel_frompng_convert_profile_to_srgb(unsigned char *pixels,
                                      int width,
                                      int height,
                                      int pixelformat,
                                      sixel_cms_profile_t *src_profile)
{
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;
    sixel_cms_color_space_t src_colorspace;
    sixel_cms_pixel_format_t src_type;
    sixel_cms_pixel_format_t dst_type;
    size_t pixel_count;
    unsigned char *gray_in;
    unsigned char *rgb_out;
    size_t i;
    int converted;

    dst_profile = NULL;
    transform = NULL;
    src_colorspace = SIXEL_CMS_COLORSPACE_RGB;
    src_type = SIXEL_CMS_PIXELFORMAT_RGB_8;
    dst_type = SIXEL_CMS_PIXELFORMAT_RGB_8;
    pixel_count = 0u;
    gray_in = NULL;
    rgb_out = NULL;
    i = 0u;
    converted = 0;

    if (pixels == NULL || width <= 0 || height <= 0 || src_profile == NULL) {
        return 0;
    }
    if (pixelformat != SIXEL_PIXELFORMAT_RGB888
            && pixelformat != SIXEL_PIXELFORMAT_G8
            && pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32) {
        return 0;
    }

    src_colorspace = sixel_cms_get_color_space(src_profile);
    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        return 0;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32) {
        src_type = SIXEL_CMS_PIXELFORMAT_RGB_F32;
        dst_type = SIXEL_CMS_PIXELFORMAT_RGB_F32;
    }
    if (src_colorspace == SIXEL_CMS_COLORSPACE_GRAY) {
        if (pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32) {
            goto cleanup;
        }
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
    } else {
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
    }

cleanup:
    free(rgb_out);
    free(gray_in);
    if (transform != NULL) {
        sixel_cms_delete_transform(transform);
    }
    if (dst_profile != NULL) {
        sixel_cms_close_profile(dst_profile);
    }
    return converted;
}

static int
sixel_frompng_convert_icc_to_srgb_internal(unsigned char *pixels,
                                           int width,
                                           int height,
                                           int pixelformat,
                                           unsigned char const *profile,
                                           size_t profile_length)
{
    sixel_cms_profile_t *src_profile;
    int converted;

    src_profile = NULL;
    converted = 0;
    if (pixels == NULL || width <= 0 || height <= 0
            || profile == NULL || profile_length == 0u) {
        return 0;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_length);
    if (src_profile == NULL) {
        return 0;
    }
    converted = sixel_frompng_convert_profile_to_srgb(pixels,
                                                      width,
                                                      height,
                                                      pixelformat,
                                                      src_profile);
    sixel_cms_close_profile(src_profile);
    return converted;
}

void
sixel_frompng_convert_icc_to_srgb(unsigned char *pixels,
                                  int width,
                                  int height,
                                  unsigned char const *profile,
                                  size_t profile_length)
{
    (void)sixel_frompng_convert_icc_to_srgb_internal(pixels,
                                                     width,
                                                     height,
                                                     SIXEL_PIXELFORMAT_RGB888,
                                                     profile,
                                                     profile_length);
}

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
            value16 = ((unsigned int)sample * 65535u + max_value / 2u) / max_value;
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
        sixel_frompng_expand_sample_to_bg(sixel_frompng_read_be16(bkgd_data + 0u),
                                          bitdepth,
                                          &red8,
                                          &red16);
        sixel_frompng_expand_sample_to_bg(sixel_frompng_read_be16(bkgd_data + 2u),
                                          bitdepth,
                                          &green8,
                                          &green16);
        sixel_frompng_expand_sample_to_bg(sixel_frompng_read_be16(bkgd_data + 4u),
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
                                 unsigned char bg8[3],
                                 uint16_t bg16[3],
                                 int *background_from_file)
{
    if (background_from_file != NULL) {
        *background_from_file = 0;
    }
    if (bgcolor != NULL) {
        bg8[0] = bgcolor[0];
        bg8[1] = bgcolor[1];
        bg8[2] = bgcolor[2];
        bg16[0] = (uint16_t)((unsigned int)bgcolor[0] * 257u);
        bg16[1] = (uint16_t)((unsigned int)bgcolor[1] * 257u);
        bg16[2] = (uint16_t)((unsigned int)bgcolor[2] * 257u);
        return;
    }
    if (sixel_frompng_parse_bkgd(buffer, size, bg8, bg16)) {
        if (background_from_file != NULL) {
            *background_from_file = 1;
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

static SIXELSTATUS
sixel_frompng_convert_rgba16_to_rgbfloat32(unsigned char **result,
                                            uint16_t const *src16,
                                            int width,
                                            int height,
                                            uint16_t const bg16[3],
                                            sixel_allocator_t *allocator)
{
    float *dst;
    size_t pixel_count;
    size_t total_bytes;
    size_t index;
    size_t src_offset;
    size_t dst_offset;
    uint16_t alpha16;
    uint64_t blended;
    uint16_t out16;

    dst = NULL;
    pixel_count = 0u;
    total_bytes = 0u;
    index = 0u;
    src_offset = 0u;
    dst_offset = 0u;
    alpha16 = 0u;
    blended = 0u;
    out16 = 0u;

    if (result == NULL || src16 == NULL || bg16 == NULL ||
        allocator == NULL || width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
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
            "load_with_builtin: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (index = 0u; index < pixel_count; ++index) {
        src_offset = index * 4u;
        dst_offset = index * 3u;
        alpha16 = src16[src_offset + 3u];

        blended = (uint64_t)(65535u - (unsigned int)alpha16) * (uint64_t)bg16[0u]
            + (uint64_t)alpha16 * (uint64_t)src16[src_offset + 0u];
        out16 = (uint16_t)((blended + 32767u) / 65535u);
        dst[dst_offset + 0u] = (float)out16 / 65535.0f;

        blended = (uint64_t)(65535u - (unsigned int)alpha16) * (uint64_t)bg16[1u]
            + (uint64_t)alpha16 * (uint64_t)src16[src_offset + 1u];
        out16 = (uint16_t)((blended + 32767u) / 65535u);
        dst[dst_offset + 1u] = (float)out16 / 65535.0f;

        blended = (uint64_t)(65535u - (unsigned int)alpha16) * (uint64_t)bg16[2u]
            + (uint64_t)alpha16 * (uint64_t)src16[src_offset + 2u];
        out16 = (uint16_t)((blended + 32767u) / 65535u);
        dst[dst_offset + 2u] = (float)out16 / 65535.0f;
    }

    *result = (unsigned char *)dst;

    return SIXEL_OK;
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
        } else if (memcmp(chunk_type, "cHRM", 4u) == 0) {
            *has_chrm = 1;
        } else if (memcmp(chunk_type, "gAMA", 4u) == 0) {
            *has_gama = 1;
        }
        offset += chunk_total;
    }

    return 1;
}

static int
sixel_frompng_build_profile_from_chunks(unsigned char const *buffer,
                                        size_t size,
                                        sixel_cms_profile_t **profile)
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

    if (profile == NULL) {
        return 0;
    }
    *profile = NULL;
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
            has_chrm = 1;
        } else if (memcmp(chunk_type, "gAMA", 4u) == 0
                   && chunk_length >= 4u) {
            file_gamma = (double)sixel_frompng_read_be32(chunk_data)
                / 100000.0;
            has_gama = 1;
        }
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
    size_t icc_profile_length;
    sixel_cms_profile_t *chunk_profile;
    int has_iccp;
    int has_srgb;
    int has_chrm;
    int has_gama;
    int converted;

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
    if (has_iccp) {
        if (sixel_frompng_extract_icc(buffer,
                                      size,
                                      &icc_profile,
                                      &icc_profile_length)) {
            converted = sixel_frompng_convert_icc_to_srgb_internal(
                pixels,
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
                                                          &chunk_profile)) {
        converted = sixel_frompng_convert_profile_to_srgb(pixels,
                                                          width,
                                                          height,
                                                          pixelformat,
                                                          chunk_profile);
        sixel_cms_close_profile(chunk_profile);
    }
    if (icc_profile != NULL) {
        sixel_allocator_free(allocator, icc_profile);
    }
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
    (void)sixel_frompng_apply_colorspace_fallback_internal(pixels,
                                                           width,
                                                           height,
                                                           SIXEL_PIXELFORMAT_RGB888,
                                                           buffer,
                                                           size,
                                                           allocator);
}
#else
void
sixel_frompng_convert_icc_to_srgb(unsigned char *pixels,
                                  int width,
                                  int height,
                                  unsigned char const *profile,
                                  size_t profile_length)
{
    (void)pixels;
    (void)width;
    (void)height;
    (void)profile;
    (void)profile_length;
}

void
sixel_frompng_apply_colorspace_fallback(unsigned char *pixels,
                                        int width,
                                        int height,
                                        unsigned char const *buffer,
                                        size_t size,
                                        sixel_allocator_t *allocator)
{
    (void)pixels;
    (void)width;
    (void)height;
    (void)buffer;
    (void)size;
    (void)allocator;
}
#endif  /* HAVE_LCMS2 */

#if !HAVE_LCMS2
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
            value16 = ((unsigned int)sample * 65535u + max_value / 2u) / max_value;
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
        sixel_frompng_expand_sample_to_bg(sixel_frompng_read_be16(bkgd_data + 0u),
                                          bitdepth,
                                          &red8,
                                          &red16);
        sixel_frompng_expand_sample_to_bg(sixel_frompng_read_be16(bkgd_data + 2u),
                                          bitdepth,
                                          &green8,
                                          &green16);
        sixel_frompng_expand_sample_to_bg(sixel_frompng_read_be16(bkgd_data + 4u),
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
                                 unsigned char bg8[3],
                                 uint16_t bg16[3],
                                 int *background_from_file)
{
    if (background_from_file != NULL) {
        *background_from_file = 0;
    }
    if (bgcolor != NULL) {
        bg8[0] = bgcolor[0];
        bg8[1] = bgcolor[1];
        bg8[2] = bgcolor[2];
        bg16[0] = (uint16_t)((unsigned int)bgcolor[0] * 257u);
        bg16[1] = (uint16_t)((unsigned int)bgcolor[1] * 257u);
        bg16[2] = (uint16_t)((unsigned int)bgcolor[2] * 257u);
        return;
    }
    if (sixel_frompng_parse_bkgd(buffer, size, bg8, bg16)) {
        if (background_from_file != NULL) {
            *background_from_file = 1;
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

static SIXELSTATUS
sixel_frompng_convert_rgba16_to_rgbfloat32(unsigned char **result,
                                            uint16_t const *src16,
                                            int width,
                                            int height,
                                            uint16_t const bg16[3],
                                            sixel_allocator_t *allocator)
{
    float *dst;
    size_t pixel_count;
    size_t total_bytes;
    size_t index;
    size_t src_offset;
    size_t dst_offset;
    uint16_t alpha16;
    uint64_t blended;
    uint16_t out16;

    dst = NULL;
    pixel_count = 0u;
    total_bytes = 0u;
    index = 0u;
    src_offset = 0u;
    dst_offset = 0u;
    alpha16 = 0u;
    blended = 0u;
    out16 = 0u;

    if (result == NULL || src16 == NULL || bg16 == NULL ||
        allocator == NULL || width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
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
            "load_with_builtin: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (index = 0u; index < pixel_count; ++index) {
        src_offset = index * 4u;
        dst_offset = index * 3u;
        alpha16 = src16[src_offset + 3u];

        blended = (uint64_t)(65535u - (unsigned int)alpha16) * (uint64_t)bg16[0u]
            + (uint64_t)alpha16 * (uint64_t)src16[src_offset + 0u];
        out16 = (uint16_t)((blended + 32767u) / 65535u);
        dst[dst_offset + 0u] = (float)out16 / 65535.0f;

        blended = (uint64_t)(65535u - (unsigned int)alpha16) * (uint64_t)bg16[1u]
            + (uint64_t)alpha16 * (uint64_t)src16[src_offset + 1u];
        out16 = (uint16_t)((blended + 32767u) / 65535u);
        dst[dst_offset + 1u] = (float)out16 / 65535.0f;

        blended = (uint64_t)(65535u - (unsigned int)alpha16) * (uint64_t)bg16[2u]
            + (uint64_t)alpha16 * (uint64_t)src16[src_offset + 2u];
        out16 = (uint16_t)((blended + 32767u) / 65535u);
        dst[dst_offset + 2u] = (float)out16 / 65535.0f;
    }

    *result = (unsigned char *)dst;

    return SIXEL_OK;
}
#endif  /* !HAVE_LCMS2 */

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
sixel_frompng_decode_source_unit(double value, int transfer_mode, double file_gamma)
{
    value = sixel_frompng_clamp_unit(value);
    if (transfer_mode == SIXEL_FROMPNG_TRANSFER_GAMA && file_gamma > 0.0) {
        return pow(value, 1.0 / file_gamma);
    }
    return sixel_frompng_decode_srgb_unit(value);
}

static SIXELSTATUS
sixel_frompng_roundtrip_target_to_linear(float *pixels,
                                         size_t pixel_count,
                                         int enable_cms)
{
    SIXELSTATUS status;
    size_t size_bytes;
    int target_colorspace;

    if (pixels == NULL || pixel_count == 0u || !enable_cms) {
        return SIXEL_OK;
    }

    target_colorspace = loader_cms_target_colorspace();
    if (target_colorspace == SIXEL_COLORSPACE_LINEAR) {
        return SIXEL_OK;
    }

    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    size_bytes = pixel_count * 3u * sizeof(float);

    status = sixel_helper_convert_colorspace((unsigned char *)pixels,
                                             size_bytes,
                                             SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                                             SIXEL_COLORSPACE_LINEAR,
                                             target_colorspace);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    return sixel_helper_convert_colorspace((unsigned char *)pixels,
                                           size_bytes,
                                           SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                                           target_colorspace,
                                           SIXEL_COLORSPACE_LINEAR);
}

static SIXELSTATUS
sixel_frompng_roundtrip_background_to_linear(double bg_linear[3],
                                             int enable_cms)
{
    SIXELSTATUS status;
    float bg_pixel[3];
    int channel;

    if (bg_linear == NULL || !enable_cms) {
        return SIXEL_OK;
    }

    for (channel = 0; channel < 3; ++channel) {
        bg_pixel[channel] = (float)sixel_frompng_clamp_unit(bg_linear[channel]);
    }

    status = sixel_frompng_roundtrip_target_to_linear(bg_pixel, 1u, enable_cms);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    for (channel = 0; channel < 3; ++channel) {
        bg_linear[channel] = sixel_frompng_clamp_unit((double)bg_pixel[channel]);
    }

    return SIXEL_OK;
}

static int
sixel_frompng_parse_transfer_chunks(unsigned char const *buffer,
                                    size_t size,
                                    int *has_srgb,
                                    int *has_gama,
                                    double *file_gamma)
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
        if (memcmp(chunk_type, "sRGB", 4u) == 0) {
            *has_srgb = 1;
        } else if (memcmp(chunk_type, "gAMA", 4u) == 0 && chunk_length >= 4u) {
            *has_gama = 1;
            *file_gamma = (double)sixel_frompng_read_be32(chunk_data) / 100000.0;
        } else if (memcmp(chunk_type, "IEND", 4u) == 0) {
            break;
        }

        offset += chunk_total;
    }

    return 1;
}

static SIXELSTATUS
sixel_frompng_convert_rgba8_to_linearrgbfloat32(
    unsigned char **result,
    unsigned char const *src,
    int width,
    int height,
    unsigned char const bg8[3],
    int background_from_file,
    int enable_cms,
    unsigned char const *buffer,
    size_t size,
    sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    size_t pixel_count;
    float *dst;
    size_t index;
    size_t src_offset;
    size_t dst_offset;
    int has_srgb;
    int has_gama;
    int transfer_mode;
    int background_colorspace;
    double file_gamma;
    double bg_linear[3];
    double alpha;
    double src_encoded;
    double src_linear;
    double out_linear;
    int channel;

    if (result == NULL || src == NULL || bg8 == NULL ||
        allocator == NULL || width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    has_srgb = 0;
    has_gama = 0;
    file_gamma = 0.0;
    (void)sixel_frompng_parse_transfer_chunks(buffer,
                                              size,
                                              &has_srgb,
                                              &has_gama,
                                              &file_gamma);

    transfer_mode = SIXEL_FROMPNG_TRANSFER_SRGB;
    if (!has_srgb && has_gama && file_gamma > 0.0) {
        transfer_mode = SIXEL_FROMPNG_TRANSFER_GAMA;
    }

    background_colorspace = loader_background_colorspace();
    for (channel = 0; channel < 3; ++channel) {
        double bg_unit;

        bg_unit = (double)bg8[channel] / 255.0;
        if (background_colorspace == SIXEL_COLORSPACE_LINEAR) {
            bg_linear[channel] = sixel_frompng_clamp_unit(bg_unit);
        } else if (background_from_file) {
            bg_linear[channel] = sixel_frompng_decode_source_unit(bg_unit,
                                                                  transfer_mode,
                                                                  file_gamma);
        } else {
            bg_linear[channel] = sixel_frompng_decode_srgb_unit(bg_unit);
        }
    }

    dst = (float *)sixel_allocator_malloc(allocator, pixel_count * 3u * sizeof(float));
    if (dst == NULL) {
        sixel_helper_set_additional_message(
            "load_with_builtin: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    for (index = 0u; index < pixel_count; ++index) {
        src_offset = index * 4u;
        dst_offset = index * 3u;

        src_encoded = (double)src[src_offset + 0u] / 255.0;
        dst[dst_offset + 0u] = (float)sixel_frompng_decode_source_unit(
            src_encoded,
            transfer_mode,
            file_gamma);
        src_encoded = (double)src[src_offset + 1u] / 255.0;
        dst[dst_offset + 1u] = (float)sixel_frompng_decode_source_unit(
            src_encoded,
            transfer_mode,
            file_gamma);
        src_encoded = (double)src[src_offset + 2u] / 255.0;
        dst[dst_offset + 2u] = (float)sixel_frompng_decode_source_unit(
            src_encoded,
            transfer_mode,
            file_gamma);
    }

    status = sixel_frompng_roundtrip_target_to_linear(dst, pixel_count, enable_cms);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, dst);
        return status;
    }

    status = sixel_frompng_roundtrip_background_to_linear(bg_linear, enable_cms);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, dst);
        return status;
    }

    for (index = 0u; index < pixel_count; ++index) {
        src_offset = index * 4u;
        dst_offset = index * 3u;
        alpha = (double)src[src_offset + 3u] / 255.0;

        src_linear = (double)dst[dst_offset + 0u];
        out_linear = src_linear * alpha + bg_linear[0] * (1.0 - alpha);
        dst[dst_offset + 0u] = (float)out_linear;

        src_linear = (double)dst[dst_offset + 1u];
        out_linear = src_linear * alpha + bg_linear[1] * (1.0 - alpha);
        dst[dst_offset + 1u] = (float)out_linear;

        src_linear = (double)dst[dst_offset + 2u];
        out_linear = src_linear * alpha + bg_linear[2] * (1.0 - alpha);
        dst[dst_offset + 2u] = (float)out_linear;
    }

    *result = (unsigned char *)dst;
    return SIXEL_OK;
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
                              sixel_frame_t *frame,
                              int enable_cms,
                              unsigned char const *bgcolor)
{
    SIXELSTATUS status;
    int png_is_16bit;
    int depth;
    uint16_t *pixels16;
    unsigned char *pixels8;
    unsigned char *pixels_float32;
    int cms_applied;
    int target_pixelformat;
    int background_from_file;
    int has_transparency;
    unsigned char background8[3];
    uint16_t background16[3];

    status = SIXEL_FALSE;
    png_is_16bit = 0;
    depth = 0;
    pixels16 = NULL;
    pixels8 = NULL;
    pixels_float32 = NULL;
    cms_applied = 0;
    target_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    background_from_file = 0;
    has_transparency = 0;
    background8[0] = 0u;
    background8[1] = 0u;
    background8[2] = 0u;
    background16[0] = 0u;
    background16[1] = 0u;
    background16[2] = 0u;

    if (pchunk == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    sixel_frompng_resolve_background(pchunk->buffer,
                                     pchunk->size,
                                     bgcolor,
                                     background8,
                                     background16,
                                     &background_from_file);

    png_is_16bit = stbi_is_16_bit_from_memory(pchunk->buffer, (int)pchunk->size);
    if (png_is_16bit != 0) {
        pixels16 = stbi_load_16_from_memory(pchunk->buffer,
                                            (int)pchunk->size,
                                            &frame->width,
                                            &frame->height,
                                            &depth,
                                            4);
        if (pixels16 == NULL) {
            sixel_helper_set_additional_message(stbi_failure_reason());
            return SIXEL_STBI_ERROR;
        }
        status = sixel_frompng_convert_rgba16_to_rgbfloat32(&pixels_float32,
                                                             pixels16,
                                                             frame->width,
                                                             frame->height,
                                                             background16,
                                                             pchunk->allocator);
        stbi_image_free(pixels16);
        pixels16 = NULL;
        if (SIXEL_FAILED(status)) {
            return status;
        }
        sixel_frame_set_pixels(frame, pixels_float32);
        frame->loop_count = 1;
        frame->pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        if (enable_cms) {
#if HAVE_LCMS2
            cms_applied = sixel_frompng_apply_colorspace_fallback_internal(
                pixels_float32,
                frame->width,
                frame->height,
                SIXEL_PIXELFORMAT_RGBFLOAT32,
                pchunk->buffer,
                pchunk->size,
                pchunk->allocator);
#endif
            if (cms_applied) {
                target_pixelformat = loader_cms_target_pixelformat();
                status = sixel_frame_set_pixelformat(frame, target_pixelformat);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
            }
        }
        return SIXEL_OK;
    }

    has_transparency = sixel_frompng_has_transparency(pchunk->buffer,
                                                      pchunk->size);
    pixels8 = stbi_load_from_memory(pchunk->buffer,
                                    (int)pchunk->size,
                                    &frame->width,
                                    &frame->height,
                                    &depth,
                                    has_transparency ? 4 : 3);
    if (pixels8 == NULL) {
        sixel_helper_set_additional_message(stbi_failure_reason());
        return SIXEL_STBI_ERROR;
    }

    if (!has_transparency) {
        sixel_frame_set_pixels(frame, pixels8);
        frame->loop_count = 1;
        frame->pixelformat = SIXEL_PIXELFORMAT_RGB888;
        frame->colorspace = SIXEL_COLORSPACE_GAMMA;

        cms_applied = 0;
        if (enable_cms) {
#if HAVE_LCMS2
            cms_applied = sixel_frompng_apply_colorspace_fallback_internal(
                pixels8,
                frame->width,
                frame->height,
                SIXEL_PIXELFORMAT_RGB888,
                pchunk->buffer,
                pchunk->size,
                pchunk->allocator);
#endif
            if (cms_applied) {
                target_pixelformat = loader_cms_target_pixelformat();
                status = sixel_frame_set_pixelformat(frame, target_pixelformat);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
            }
        }
        return SIXEL_OK;
    }

    status = sixel_frompng_convert_rgba8_to_linearrgbfloat32(
        &pixels_float32,
        pixels8,
        frame->width,
        frame->height,
        background8,
        background_from_file,
        enable_cms,
        pchunk->buffer,
        pchunk->size,
        pchunk->allocator);
    stbi_image_free(pixels8);
    pixels8 = NULL;
    if (SIXEL_FAILED(status)) {
        return status;
    }
    sixel_frame_set_pixels(frame, pixels_float32);
    frame->loop_count = 1;
    frame->pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    frame->colorspace = SIXEL_COLORSPACE_LINEAR;

    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
