/*
 * SPDX-License-Identifier: MIT
 */

#include "cms.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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

static int
sixel_cms_intent_from_name(char const *name, size_t length, int *intent)
{
    if (name == NULL || intent == NULL || length == 0u) {
        return 0;
    }
    if (length == 10u && memcmp(name, "perceptual", 10u) == 0) {
        *intent = INTENT_PERCEPTUAL;
        return 1;
    }
    if (length == 21u &&
        memcmp(name, "relative_colorimetric", 21u) == 0) {
        *intent = INTENT_RELATIVE_COLORIMETRIC;
        return 1;
    }
    if (length == 10u && memcmp(name, "saturation", 10u) == 0) {
        *intent = INTENT_SATURATION;
        return 1;
    }
    if (length == 21u &&
        memcmp(name, "absolute_colorimetric", 21u) == 0) {
        *intent = INTENT_ABSOLUTE_COLORIMETRIC;
        return 1;
    }
    return 0;
}

static int
sixel_cms_intent_contains(int const *intents, size_t count, int intent)
{
    size_t i;

    for (i = 0u; i < count; ++i) {
        if (intents[i] == intent) {
            return 1;
        }
    }
    return 0;
}

static size_t
sixel_cms_build_intent_order(int intents[4])
{
    static int const defaults[4] = {
        INTENT_PERCEPTUAL,
        INTENT_RELATIVE_COLORIMETRIC,
        INTENT_SATURATION,
        INTENT_ABSOLUTE_COLORIMETRIC
    };
    char const *env;
    char const *p;
    char const *end;
    int custom[4];
    size_t custom_count;
    size_t i;
    int exclusive;

    env = getenv("SIXEL_LOADER_CMS_RENDERING_INTENT");
    if (env == NULL || *env == '\0') {
        env = getenv("SIXEL_CMS_RENDERING_INTENT");
    }
    if (env == NULL || *env == '\0') {
        memcpy(intents, defaults, sizeof(defaults));
        return 4u;
    }

    p = env;
    end = env + strlen(env);
    while (p < end && isspace((unsigned char)*p)) {
        ++p;
    }
    while (end > p && isspace((unsigned char)end[-1])) {
        --end;
    }
    if (p == end) {
        memcpy(intents, defaults, sizeof(defaults));
        return 4u;
    }

    exclusive = 0;
    if (end > p && end[-1] == '!') {
        exclusive = 1;
        --end;
        while (end > p && isspace((unsigned char)end[-1])) {
            --end;
        }
        if (p == end) {
            memcpy(intents, defaults, sizeof(defaults));
            return 4u;
        }
    }

    custom_count = 0u;
    while (p < end) {
        char const *token_start;
        char const *token_end;
        char const *comma;
        int intent;

        while (p < end && isspace((unsigned char)*p)) {
            ++p;
        }
        token_start = p;
        comma = memchr(p, ',', (size_t)(end - p));
        if (comma == NULL) {
            token_end = end;
            p = end;
        } else {
            token_end = comma;
            p = comma + 1;
        }
        while (token_end > token_start &&
               isspace((unsigned char)token_end[-1])) {
            --token_end;
        }
        if (token_end == token_start) {
            memcpy(intents, defaults, sizeof(defaults));
            return 4u;
        }
        if (!sixel_cms_intent_from_name(token_start,
                                        (size_t)(token_end - token_start),
                                        &intent)) {
            memcpy(intents, defaults, sizeof(defaults));
            return 4u;
        }
        if (!sixel_cms_intent_contains(custom, custom_count, intent)) {
            if (custom_count >= 4u) {
                memcpy(intents, defaults, sizeof(defaults));
                return 4u;
            }
            custom[custom_count++] = intent;
        }
    }

    if (custom_count == 0u) {
        memcpy(intents, defaults, sizeof(defaults));
        return 4u;
    }

    if (exclusive) {
        memcpy(intents, custom, custom_count * sizeof(custom[0]));
        return custom_count;
    }

    memcpy(intents, custom, custom_count * sizeof(custom[0]));
    for (i = 0u; i < 4u; ++i) {
        if (!sixel_cms_intent_contains(intents, custom_count, defaults[i])) {
            intents[custom_count++] = defaults[i];
        }
    }

    return custom_count;
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
    int intents[4];
    size_t intent_count;
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

    intent_count = sixel_cms_build_intent_order(intents);
    transform->handle = NULL;
    for (i = 0u; i < intent_count; ++i) {
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
