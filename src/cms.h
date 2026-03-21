/*
 * SPDX-License-Identifier: MIT
 */

#ifndef LIBSIXEL_CMS_H
#define LIBSIXEL_CMS_H

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sixel_cms_profile sixel_cms_profile_t;
typedef struct sixel_cms_transform sixel_cms_transform_t;

typedef enum sixel_cms_color_space {
    SIXEL_CMS_COLORSPACE_UNKNOWN = 0,
    SIXEL_CMS_COLORSPACE_RGB,
    SIXEL_CMS_COLORSPACE_GRAY,
    SIXEL_CMS_COLORSPACE_CMYK
} sixel_cms_color_space_t;

typedef enum sixel_cms_pixel_format {
    SIXEL_CMS_PIXELFORMAT_GRAY_8 = 1,
    SIXEL_CMS_PIXELFORMAT_RGB_8,
    SIXEL_CMS_PIXELFORMAT_RGBA_8,
    SIXEL_CMS_PIXELFORMAT_RGB_F32,
    SIXEL_CMS_PIXELFORMAT_CMYK_8
} sixel_cms_pixel_format_t;

enum {
    SIXEL_CMS_TRANSFORM_DEFAULT = 0,
    SIXEL_CMS_TRANSFORM_COPY_ALPHA = 1
};

sixel_cms_profile_t *
sixel_cms_open_profile_from_mem(void const *data, size_t length);

sixel_cms_profile_t *
sixel_cms_create_srgb_profile(void);

sixel_cms_profile_t *
sixel_cms_create_rgb_profile_from_gamma_chrm(double file_gamma,
                                             double white_x,
                                             double white_y,
                                             double red_x,
                                             double red_y,
                                             double green_x,
                                             double green_y,
                                             double blue_x,
                                             double blue_y);

sixel_cms_color_space_t
sixel_cms_get_color_space(sixel_cms_profile_t const *profile);

void
sixel_cms_close_profile(sixel_cms_profile_t *profile);

sixel_cms_transform_t *
sixel_cms_create_transform(sixel_cms_profile_t const *src_profile,
                           sixel_cms_pixel_format_t src_format,
                           sixel_cms_profile_t const *dst_profile,
                           sixel_cms_pixel_format_t dst_format,
                           int flags);

int
sixel_cms_do_transform(sixel_cms_transform_t const *transform,
                       void const *src,
                       void *dst,
                       size_t pixel_count);

void
sixel_cms_delete_transform(sixel_cms_transform_t *transform);

#ifdef __cplusplus
}
#endif

#endif
