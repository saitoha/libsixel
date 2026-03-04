/*
 * SPDX-License-Identifier: MIT
 */

#include "cms.h"

#include <stdlib.h>

#if HAVE_LCMS2
# include <lcms2.h>

struct sixel_cms_profile {
    cmsHPROFILE handle;
};

struct sixel_cms_transform {
    cmsHTRANSFORM handle;
};

static cmsUInt32Number
sixel_cms_map_format(sixel_cms_pixel_format_t format)
{
    switch (format) {
    case SIXEL_CMS_PIXELFORMAT_GRAY_8:
        return TYPE_GRAY_8;
    case SIXEL_CMS_PIXELFORMAT_RGBA_8:
        return TYPE_RGBA_8;
    case SIXEL_CMS_PIXELFORMAT_RGB_8:
    default:
        return TYPE_RGB_8;
    }
}

sixel_cms_profile_t *
sixel_cms_open_profile_from_mem(void const *data, size_t length)
{
    sixel_cms_profile_t *profile;

    if (data == NULL || length == 0u) {
        return NULL;
    }
    profile = (sixel_cms_profile_t *)malloc(sizeof(*profile));
    if (profile == NULL) {
        return NULL;
    }
    profile->handle = cmsOpenProfileFromMem(data, (cmsUInt32Number)length);
    if (profile->handle == NULL) {
        free(profile);
        return NULL;
    }
    return profile;
}

sixel_cms_profile_t *
sixel_cms_create_srgb_profile(void)
{
    sixel_cms_profile_t *profile;

    profile = (sixel_cms_profile_t *)malloc(sizeof(*profile));
    if (profile == NULL) {
        return NULL;
    }
    profile->handle = cmsCreate_sRGBProfile();
    if (profile->handle == NULL) {
        free(profile);
        return NULL;
    }
    return profile;
}

sixel_cms_profile_t *
sixel_cms_create_rgb_profile_from_gamma_chrm(double file_gamma,
                                             double white_x,
                                             double white_y,
                                             double red_x,
                                             double red_y,
                                             double green_x,
                                             double green_y,
                                             double blue_x,
                                             double blue_y)
{
    sixel_cms_profile_t *profile;
    cmsToneCurve *curve;
    cmsToneCurve *curves[3];
    cmsCIExyY white_point;
    cmsCIExyYTRIPLE primaries;

    if (file_gamma <= 0.0) {
        return NULL;
    }

    white_point.x = white_x;
    white_point.y = white_y;
    white_point.Y = 1.0;
    primaries.Red.x = red_x;
    primaries.Red.y = red_y;
    primaries.Red.Y = 1.0;
    primaries.Green.x = green_x;
    primaries.Green.y = green_y;
    primaries.Green.Y = 1.0;
    primaries.Blue.x = blue_x;
    primaries.Blue.y = blue_y;
    primaries.Blue.Y = 1.0;

    curve = cmsBuildGamma(NULL, 1.0 / file_gamma);
    if (curve == NULL) {
        return NULL;
    }
    curves[0] = curve;
    curves[1] = curve;
    curves[2] = curve;

    profile = (sixel_cms_profile_t *)malloc(sizeof(*profile));
    if (profile == NULL) {
        cmsFreeToneCurve(curve);
        return NULL;
    }
    profile->handle = cmsCreateRGBProfile(&white_point, &primaries, curves);
    cmsFreeToneCurve(curve);
    if (profile->handle == NULL) {
        free(profile);
        return NULL;
    }

    return profile;
}

sixel_cms_color_space_t
sixel_cms_get_color_space(sixel_cms_profile_t const *profile)
{
    cmsColorSpaceSignature signature;

    if (profile == NULL || profile->handle == NULL) {
        return SIXEL_CMS_COLORSPACE_UNKNOWN;
    }
    signature = cmsGetColorSpace(profile->handle);
    if (signature == cmsSigGrayData) {
        return SIXEL_CMS_COLORSPACE_GRAY;
    }
    if (signature == cmsSigRgbData) {
        return SIXEL_CMS_COLORSPACE_RGB;
    }
    return SIXEL_CMS_COLORSPACE_UNKNOWN;
}

void
sixel_cms_close_profile(sixel_cms_profile_t *profile)
{
    if (profile == NULL) {
        return;
    }
    if (profile->handle != NULL) {
        cmsCloseProfile(profile->handle);
    }
    free(profile);
}

sixel_cms_transform_t *
sixel_cms_create_transform(sixel_cms_profile_t const *src_profile,
                           sixel_cms_pixel_format_t src_format,
                           sixel_cms_profile_t const *dst_profile,
                           sixel_cms_pixel_format_t dst_format,
                           int flags)
{
    sixel_cms_transform_t *transform;
    cmsUInt32Number cflags;
    int const intents[] = {
        INTENT_PERCEPTUAL,
        INTENT_RELATIVE_COLORIMETRIC,
        INTENT_SATURATION,
        INTENT_ABSOLUTE_COLORIMETRIC
    };
    size_t i;

    if (src_profile == NULL || dst_profile == NULL ||
        src_profile->handle == NULL || dst_profile->handle == NULL) {
        return NULL;
    }

    transform = (sixel_cms_transform_t *)malloc(sizeof(*transform));
    if (transform == NULL) {
        return NULL;
    }

    cflags = 0;
    if ((flags & SIXEL_CMS_TRANSFORM_COPY_ALPHA) != 0) {
        cflags |= cmsFLAGS_COPY_ALPHA;
    }

    transform->handle = NULL;
    for (i = 0u; i < sizeof(intents) / sizeof(intents[0]); ++i) {
        transform->handle = cmsCreateTransform(src_profile->handle,
                                               sixel_cms_map_format(src_format),
                                               dst_profile->handle,
                                               sixel_cms_map_format(dst_format),
                                               intents[i],
                                               cflags);
        if (transform->handle != NULL) {
            break;
        }
    }
    if (transform->handle == NULL) {
        free(transform);
        return NULL;
    }

    return transform;
}

int
sixel_cms_do_transform(sixel_cms_transform_t const *transform,
                       void const *src,
                       void *dst,
                       size_t pixel_count)
{
    if (transform == NULL || transform->handle == NULL ||
        src == NULL || dst == NULL || pixel_count == 0u) {
        return 0;
    }

    cmsDoTransform(transform->handle,
                   src,
                   dst,
                   (cmsUInt32Number)pixel_count);
    return 1;
}

void
sixel_cms_delete_transform(sixel_cms_transform_t *transform)
{
    if (transform == NULL) {
        return;
    }
    if (transform->handle != NULL) {
        cmsDeleteTransform(transform->handle);
    }
    free(transform);
}

#else

struct sixel_cms_profile { int unused; };
struct sixel_cms_transform { int unused; };

sixel_cms_profile_t *
sixel_cms_open_profile_from_mem(void const *data, size_t length)
{
    (void)data;
    (void)length;
    return NULL;
}

sixel_cms_profile_t *
sixel_cms_create_srgb_profile(void)
{
    return NULL;
}

sixel_cms_profile_t *
sixel_cms_create_rgb_profile_from_gamma_chrm(double file_gamma,
                                             double white_x,
                                             double white_y,
                                             double red_x,
                                             double red_y,
                                             double green_x,
                                             double green_y,
                                             double blue_x,
                                             double blue_y)
{
    (void)file_gamma;
    (void)white_x;
    (void)white_y;
    (void)red_x;
    (void)red_y;
    (void)green_x;
    (void)green_y;
    (void)blue_x;
    (void)blue_y;
    return NULL;
}

sixel_cms_color_space_t
sixel_cms_get_color_space(sixel_cms_profile_t const *profile)
{
    (void)profile;
    return SIXEL_CMS_COLORSPACE_UNKNOWN;
}

void
sixel_cms_close_profile(sixel_cms_profile_t *profile)
{
    (void)profile;
}

sixel_cms_transform_t *
sixel_cms_create_transform(sixel_cms_profile_t const *src_profile,
                           sixel_cms_pixel_format_t src_format,
                           sixel_cms_profile_t const *dst_profile,
                           sixel_cms_pixel_format_t dst_format,
                           int flags)
{
    (void)src_profile;
    (void)src_format;
    (void)dst_profile;
    (void)dst_format;
    (void)flags;
    return NULL;
}

int
sixel_cms_do_transform(sixel_cms_transform_t const *transform,
                       void const *src,
                       void *dst,
                       size_t pixel_count)
{
    (void)transform;
    (void)src;
    (void)dst;
    (void)pixel_count;
    return 0;
}

void
sixel_cms_delete_transform(sixel_cms_transform_t *transform)
{
    (void)transform;
}

#endif
