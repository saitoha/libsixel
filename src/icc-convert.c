/*
 * SPDX-License-Identifier: MIT
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "icc-convert.h"

#include <stdlib.h>
#include <string.h>

#if HAVE_LIMITS_H
# include <limits.h>
#endif

#include "icc-apply.h"
#include "icc-parse.h"

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#if HAVE_LCMS2
int
sixel_icc_convert_profile_to_srgb(unsigned char *pixels,
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
    int normalized_pixelformat;

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
    normalized_pixelformat = pixelformat;
    if (normalized_pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        normalized_pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    }

    if (pixels == NULL || width <= 0 || height <= 0 || src_profile == NULL) {
        return 0;
    }
    if (normalized_pixelformat != SIXEL_PIXELFORMAT_RGB888
            && normalized_pixelformat != SIXEL_PIXELFORMAT_G8
            && normalized_pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32) {
        return 0;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return 0;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / 3u) {
        return 0;
    }

    src_colorspace = sixel_cms_get_color_space(src_profile);
    if (normalized_pixelformat == SIXEL_PIXELFORMAT_G8
            && src_colorspace != SIXEL_CMS_COLORSPACE_GRAY) {
        return 0;
    }
    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        return 0;
    }

    if (normalized_pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32) {
        src_type = SIXEL_CMS_PIXELFORMAT_RGB_F32;
        dst_type = SIXEL_CMS_PIXELFORMAT_RGB_F32;
    }

    if (src_colorspace == SIXEL_CMS_COLORSPACE_GRAY) {
        if (normalized_pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32) {
            goto cleanup;
        }
        gray_in = (unsigned char *)malloc(pixel_count);
        rgb_out = (unsigned char *)malloc(pixel_count * 3u);
        if (gray_in == NULL || rgb_out == NULL) {
            goto cleanup;
        }
        if (normalized_pixelformat == SIXEL_PIXELFORMAT_G8) {
            memcpy(gray_in, pixels, pixel_count);
        } else {
            for (i = 0u; i < pixel_count; ++i) {
                gray_in[i] = pixels[i * 3u];
            }
        }
        transform = sixel_cms_create_transform(src_profile,
                                               SIXEL_CMS_PIXELFORMAT_GRAY_8,
                                               dst_profile,
                                               SIXEL_CMS_PIXELFORMAT_RGB_8,
                                               SIXEL_CMS_TRANSFORM_DEFAULT);
        if (transform == NULL) {
            goto cleanup;
        }
        if (!sixel_cms_do_transform(transform, gray_in, rgb_out, pixel_count)) {
            goto cleanup;
        }
        if (normalized_pixelformat == SIXEL_PIXELFORMAT_G8) {
            for (i = 0u; i < pixel_count; ++i) {
                pixels[i] = rgb_out[i * 3u];
            }
        } else {
            memcpy(pixels, rgb_out, pixel_count * 3u);
        }
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
#else
int
sixel_icc_convert_profile_to_srgb(unsigned char *pixels,
                                  int width,
                                  int height,
                                  int pixelformat,
                                  sixel_cms_profile_t *src_profile)
{
    (void)pixels;
    (void)width;
    (void)height;
    (void)pixelformat;
    (void)src_profile;
    return 0;
}
#endif  /* HAVE_LCMS2 */

int
sixel_icc_convert_to_srgb(unsigned char *pixels,
                          int width,
                          int height,
                          unsigned char const *profile,
                          size_t profile_length)
{
    return sixel_icc_convert_to_srgb_with_pixelformat(pixels,
                                                      width,
                                                      height,
                                                      SIXEL_PIXELFORMAT_RGB888,
                                                      profile,
                                                      profile_length);
}

int
sixel_icc_convert_to_srgb_with_pixelformat(
    unsigned char *pixels,
    int width,
    int height,
    int pixelformat,
    unsigned char const *profile,
    size_t profile_length)
{
#if HAVE_LCMS2
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
    converted = sixel_icc_convert_profile_to_srgb(pixels,
                                                  width,
                                                  height,
                                                  pixelformat,
                                                  src_profile);
    sixel_cms_close_profile(src_profile);
    return converted;
#else
    size_t pixel_count;
    sixel_icc_profile_t icc_profile;
    int converted;
    int normalized_pixelformat;

    pixel_count = 0u;
    converted = 0;
    normalized_pixelformat = pixelformat;
    if (normalized_pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        normalized_pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    }
    memset(&icc_profile, 0, sizeof(icc_profile));
    if (pixels == NULL || width <= 0 || height <= 0
            || profile == NULL || profile_length == 0u) {
        return 0;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return 0;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (!sixel_icc_parse_profile(profile, profile_length, &icc_profile)) {
        return 0;
    }

    switch (normalized_pixelformat) {
    case SIXEL_PIXELFORMAT_RGB888:
        converted = sixel_icc_apply_rgb_u8(pixels, pixel_count, &icc_profile);
        break;
    case SIXEL_PIXELFORMAT_G8:
        converted = sixel_icc_apply_gray_u8(pixels, pixel_count, &icc_profile);
        break;
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
        converted = sixel_icc_apply_rgb_float32((float *)pixels,
                                                pixel_count,
                                                &icc_profile);
        break;
    default:
        break;
    }

    sixel_icc_profile_destroy(&icc_profile);
    return converted;
#endif  /* HAVE_LCMS2 */
}
