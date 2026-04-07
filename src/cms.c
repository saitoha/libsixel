/*
 * SPDX-License-Identifier: MIT
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include "cms.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_MATH_H
# include <math.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif

#include "compat_stub.h"
#include "icc-apply.h"
#include "icc-parse.h"
#include <sixel.h>

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#define SIXEL_SRGB_WHITE_X 0.3127
#define SIXEL_SRGB_WHITE_Y 0.3290
#define SIXEL_SRGB_RED_X   0.6400
#define SIXEL_SRGB_RED_Y   0.3300
#define SIXEL_SRGB_GREEN_X 0.3000
#define SIXEL_SRGB_GREEN_Y 0.6000
#define SIXEL_SRGB_BLUE_X  0.1500
#define SIXEL_SRGB_BLUE_Y  0.0600

#if HAVE_LCMS2
# include <lcms2.h>
#endif

#if defined(__APPLE__) && HAVE_COREGRAPHICS
# define SIXEL_CMS_HAVE_COLORSYNC 1
# include <CoreGraphics/CoreGraphics.h>
# include <ColorSync/ColorSync.h>
#else
# define SIXEL_CMS_HAVE_COLORSYNC 0
#endif

typedef enum sixel_cms_profile_backend {
    SIXEL_CMS_PROFILE_BACKEND_NONE = 0,
    SIXEL_CMS_PROFILE_BACKEND_BUILTIN,
    SIXEL_CMS_PROFILE_BACKEND_LCMS2,
    SIXEL_CMS_PROFILE_BACKEND_COLORSYNC
} sixel_cms_profile_backend_t;

struct sixel_cms_profile {
    sixel_cms_profile_backend_t backend;
    sixel_cms_color_space_t colorspace;
    int is_srgb_profile;
    int builtin_profile_valid;
    sixel_icc_profile_t builtin_profile;
#if HAVE_LCMS2
    cmsHPROFILE lcms_handle;
#endif
#if SIXEL_CMS_HAVE_COLORSYNC
    ColorSyncProfileRef colorsync_handle;
#endif
};

struct sixel_cms_transform {
    sixel_cms_profile_backend_t backend;
    sixel_cms_pixel_format_t src_format;
    sixel_cms_pixel_format_t dst_format;
    int flags;
    sixel_cms_profile_t const *src_profile;
    sixel_cms_profile_t const *dst_profile;
    unsigned int builtin_a2b_slots[SIXEL_ICC_A2B_SLOT_COUNT];
    size_t builtin_a2b_slot_count;
#if HAVE_LCMS2
    cmsHTRANSFORM lcms_handle;
#endif
#if SIXEL_CMS_HAVE_COLORSYNC
    ColorSyncTransformRef colorsync_handle;
    ColorSyncDataDepth src_depth;
    ColorSyncDataDepth dst_depth;
    ColorSyncDataLayout src_layout;
    ColorSyncDataLayout dst_layout;
    size_t src_bytes_per_pixel;
    size_t dst_bytes_per_pixel;
#endif
};

#if defined(_MSC_VER)
# if defined(_MT)
#  define SIXEL_CMS_TLS __declspec(thread)
# else
#  define SIXEL_CMS_TLS
# endif
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
# define SIXEL_CMS_TLS _Thread_local
#elif defined(__GNUC__) || defined(__clang__)
# define SIXEL_CMS_TLS __thread
#else
# define SIXEL_CMS_TLS
#endif

static SIXEL_CMS_TLS sixel_cms_engine_t g_sixel_cms_engine
    = SIXEL_CMS_ENGINE_AUTO;

#undef SIXEL_CMS_TLS

typedef enum sixel_cms_rendering_intent {
    SIXEL_CMS_INTENT_PERCEPTUAL = 0,
    SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC = 1,
    SIXEL_CMS_INTENT_SATURATION = 2,
    SIXEL_CMS_INTENT_ABSOLUTE_COLORIMETRIC = 3
} sixel_cms_rendering_intent_t;

static int
sixel_cms_ascii_case_equal(char const *lhs, char const *rhs)
{
    size_t index;
    unsigned char left;
    unsigned char right;

    if (lhs == NULL || rhs == NULL) {
        return 0;
    }

    index = 0u;
    while (lhs[index] != '\0' && rhs[index] != '\0') {
        left = (unsigned char)lhs[index];
        right = (unsigned char)rhs[index];
        if (tolower(left) != tolower(right)) {
            return 0;
        }
        ++index;
    }

    return lhs[index] == '\0' && rhs[index] == '\0';
}

int
sixel_cms_engine_from_string(char const *text,
                             sixel_cms_engine_t *out_engine)
{
    if (text == NULL || out_engine == NULL) {
        return 0;
    }

    if (sixel_cms_ascii_case_equal(text, "none") ||
        sixel_cms_ascii_case_equal(text, "off") ||
        sixel_cms_ascii_case_equal(text, "disabled")) {
        *out_engine = SIXEL_CMS_ENGINE_NONE;
        return 1;
    }
    if (sixel_cms_ascii_case_equal(text, "auto")) {
        *out_engine = SIXEL_CMS_ENGINE_AUTO;
        return 1;
    }
    if (sixel_cms_ascii_case_equal(text, "builtin")) {
        *out_engine = SIXEL_CMS_ENGINE_BUILTIN;
        return 1;
    }
    if (sixel_cms_ascii_case_equal(text, "lcms") ||
        sixel_cms_ascii_case_equal(text, "lcms2")) {
        *out_engine = SIXEL_CMS_ENGINE_LCMS2;
        return 1;
    }
    if (sixel_cms_ascii_case_equal(text, "colorsync") ||
        sixel_cms_ascii_case_equal(text, "color-sync")) {
        *out_engine = SIXEL_CMS_ENGINE_COLORSYNC;
        return 1;
    }

    return 0;
}

char const *
sixel_cms_engine_to_string(sixel_cms_engine_t engine)
{
    switch (engine) {
    case SIXEL_CMS_ENGINE_NONE:
        return "none";
    case SIXEL_CMS_ENGINE_BUILTIN:
        return "builtin";
    case SIXEL_CMS_ENGINE_LCMS2:
        return "lcms2";
    case SIXEL_CMS_ENGINE_COLORSYNC:
        return "colorsync";
    case SIXEL_CMS_ENGINE_AUTO:
    default:
        return "auto";
    }
}

static sixel_cms_engine_t
sixel_cms_resolve_engine(sixel_cms_engine_t requested)
{
    sixel_cms_engine_t selected;

    selected = requested;

    if (selected == SIXEL_CMS_ENGINE_NONE) {
        return SIXEL_CMS_ENGINE_NONE;
    }

    if (selected == SIXEL_CMS_ENGINE_LCMS2) {
#if HAVE_LCMS2
        return SIXEL_CMS_ENGINE_LCMS2;
#else
        selected = SIXEL_CMS_ENGINE_AUTO;
#endif
    }

    if (selected == SIXEL_CMS_ENGINE_COLORSYNC) {
#if SIXEL_CMS_HAVE_COLORSYNC
        return SIXEL_CMS_ENGINE_COLORSYNC;
#else
        selected = SIXEL_CMS_ENGINE_AUTO;
#endif
    }

    if (selected == SIXEL_CMS_ENGINE_BUILTIN) {
        return SIXEL_CMS_ENGINE_BUILTIN;
    }

#if HAVE_LCMS2
    return SIXEL_CMS_ENGINE_LCMS2;
#elif SIXEL_CMS_HAVE_COLORSYNC
    return SIXEL_CMS_ENGINE_COLORSYNC;
#else
    return SIXEL_CMS_ENGINE_BUILTIN;
#endif
}

void
sixel_cms_set_engine(sixel_cms_engine_t engine)
{
    switch (engine) {
    case SIXEL_CMS_ENGINE_NONE:
    case SIXEL_CMS_ENGINE_AUTO:
    case SIXEL_CMS_ENGINE_BUILTIN:
    case SIXEL_CMS_ENGINE_LCMS2:
    case SIXEL_CMS_ENGINE_COLORSYNC:
        g_sixel_cms_engine = engine;
        break;
    default:
        g_sixel_cms_engine = SIXEL_CMS_ENGINE_AUTO;
        break;
    }
}

sixel_cms_engine_t
sixel_cms_get_engine(void)
{
    return sixel_cms_resolve_engine(g_sixel_cms_engine);
}

#if HAVE_LCMS2
static cmsUInt32Number
sixel_cms_map_format_lcms(sixel_cms_pixel_format_t format)
{
    switch (format) {
    case SIXEL_CMS_PIXELFORMAT_GRAY_8:
        return TYPE_GRAY_8;
    case SIXEL_CMS_PIXELFORMAT_RGBA_8:
        return TYPE_RGBA_8;
    case SIXEL_CMS_PIXELFORMAT_CMYK_8:
        return TYPE_CMYK_8;
    case SIXEL_CMS_PIXELFORMAT_CMYK_16:
        return TYPE_CMYK_16;
    case SIXEL_CMS_PIXELFORMAT_CMYK_F32:
        return TYPE_CMYK_FLT;
    case SIXEL_CMS_PIXELFORMAT_LAB_F32:
        return TYPE_Lab_FLT;
    case SIXEL_CMS_PIXELFORMAT_RGB_F32:
        return TYPE_RGB_FLT;
    case SIXEL_CMS_PIXELFORMAT_RGB_8:
    default:
        return TYPE_RGB_8;
    }
}
#endif

static int
sixel_cms_intent_from_name(char const *name, size_t length, int *intent)
{
    if (name == NULL || intent == NULL || length == 0u) {
        return 0;
    }
    if (length == 10u && memcmp(name, "perceptual", 10u) == 0) {
        *intent = SIXEL_CMS_INTENT_PERCEPTUAL;
        return 1;
    }
    if (length == 8u && memcmp(name, "relative", 8u) == 0) {
        *intent = SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC;
        return 1;
    }
    if (length == 21u &&
        memcmp(name, "relative_colorimetric", 21u) == 0) {
        *intent = SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC;
        return 1;
    }
    if (length == 10u && memcmp(name, "saturation", 10u) == 0) {
        *intent = SIXEL_CMS_INTENT_SATURATION;
        return 1;
    }
    if (length == 8u && memcmp(name, "absolute", 8u) == 0) {
        *intent = SIXEL_CMS_INTENT_ABSOLUTE_COLORIMETRIC;
        return 1;
    }
    if (length == 21u &&
        memcmp(name, "absolute_colorimetric", 21u) == 0) {
        *intent = SIXEL_CMS_INTENT_ABSOLUTE_COLORIMETRIC;
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
        SIXEL_CMS_INTENT_PERCEPTUAL,
        SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC,
        SIXEL_CMS_INTENT_SATURATION,
        SIXEL_CMS_INTENT_ABSOLUTE_COLORIMETRIC
    };
    char const *env;
    char const *p;
    char const *end;
    int custom[4];
    size_t custom_count;
    size_t i;
    int exclusive;

    env = sixel_compat_getenv("SIXEL_LOADER_CMS_RENDERING_INTENT");
    if (env == NULL || *env == '\0') {
        env = sixel_compat_getenv("SIXEL_CMS_RENDERING_INTENT");
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
        comma = strchr(p, ',');
        if (comma == NULL || comma > end) {
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

static unsigned int
sixel_cms_intent_to_a2b_slot(int intent)
{
    switch (intent) {
    case SIXEL_CMS_INTENT_PERCEPTUAL:
        return 0u;
    case SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC:
        return 1u;
    case SIXEL_CMS_INTENT_ABSOLUTE_COLORIMETRIC:
        return 1u;
    case SIXEL_CMS_INTENT_SATURATION:
        return 2u;
    default:
        return 0u;
    }
}

static size_t
sixel_cms_build_builtin_a2b_order(unsigned int slots[SIXEL_ICC_A2B_SLOT_COUNT])
{
    int intents[4];
    size_t intent_count;
    size_t slot_count;
    size_t i;
    unsigned int slot;
    int exists;
    size_t j;

    intent_count = 0u;
    slot_count = 0u;
    i = 0u;
    slot = 0u;
    exists = 0;
    j = 0u;
    if (slots == NULL) {
        return 0u;
    }

    intent_count = sixel_cms_build_intent_order(intents);
    for (i = 0u; i < intent_count; ++i) {
        slot = sixel_cms_intent_to_a2b_slot(intents[i]);
        if (slot >= SIXEL_ICC_A2B_SLOT_COUNT) {
            continue;
        }
        exists = 0;
        for (j = 0u; j < slot_count; ++j) {
            if (slots[j] == slot) {
                exists = 1;
                break;
            }
        }
        if (!exists && slot_count < SIXEL_ICC_A2B_SLOT_COUNT) {
            slots[slot_count++] = slot;
        }
    }

    if (slot_count == 0u) {
        slots[0u] = 0u;
        slots[1u] = 1u;
        slots[2u] = 2u;
        return SIXEL_ICC_A2B_SLOT_COUNT;
    }

    return slot_count;
}

#if SIXEL_CMS_HAVE_COLORSYNC
static sixel_cms_color_space_t
sixel_cms_parse_icc_colorspace_signature(unsigned char const *data,
                                         size_t length)
{
    unsigned char const *signature;

    if (data == NULL || length < 24u) {
        return SIXEL_CMS_COLORSPACE_UNKNOWN;
    }

    signature = data + 16u;
    if (memcmp(signature, "RGB ", 4u) == 0) {
        return SIXEL_CMS_COLORSPACE_RGB;
    }
    if (memcmp(signature, "GRAY", 4u) == 0) {
        return SIXEL_CMS_COLORSPACE_GRAY;
    }
    if (memcmp(signature, "CMYK", 4u) == 0) {
        return SIXEL_CMS_COLORSPACE_CMYK;
    }
    if (memcmp(signature, "Lab ", 4u) == 0) {
        return SIXEL_CMS_COLORSPACE_LAB;
    }

    return SIXEL_CMS_COLORSPACE_UNKNOWN;
}
#endif

static sixel_cms_profile_t *
sixel_cms_allocate_profile(void)
{
    sixel_cms_profile_t *profile;
    size_t i;

    profile = (sixel_cms_profile_t *)malloc(sizeof(*profile));
    if (profile == NULL) {
        return NULL;
    }

    memset(profile, 0, sizeof(*profile));
    profile->backend = SIXEL_CMS_PROFILE_BACKEND_NONE;
    profile->colorspace = SIXEL_CMS_COLORSPACE_UNKNOWN;
    profile->is_srgb_profile = 0;
    profile->builtin_profile_valid = 0;
    profile->builtin_profile.kind = SIXEL_ICC_PROFILE_KIND_INVALID;
    profile->builtin_profile.pcs = SIXEL_ICC_PROFILE_PCS_INVALID;
    for (i = 0u; i < 3u; ++i) {
        profile->builtin_profile.curves[i].kind = SIXEL_ICC_CURVE_INVALID;
        profile->builtin_profile.curves[i].gamma = 1.0;
        profile->builtin_profile.curves[i].table = NULL;
        profile->builtin_profile.curves[i].table_length = 0u;
    }
    for (i = 0u; i < SIXEL_ICC_A2B_SLOT_COUNT; ++i) {
        profile->builtin_profile.a2b_lut[i].kind = SIXEL_ICC_LUT_INVALID;
        profile->builtin_profile.a2b_mab[i].type = SIXEL_ICC_MAB_TYPE_INVALID;
        profile->builtin_profile.b2a_lut[i].kind = SIXEL_ICC_LUT_INVALID;
        profile->builtin_profile.b2a_mab[i].type = SIXEL_ICC_MAB_TYPE_INVALID;
    }

    return profile;
}

#if HAVE_LCMS2
static int
sixel_cms_profile_open_lcms2(sixel_cms_profile_t *profile,
                             void const *data,
                             size_t length)
{
    cmsColorSpaceSignature signature;

    if (profile == NULL || data == NULL || length == 0u) {
        return 0;
    }

    profile->lcms_handle = cmsOpenProfileFromMem(data, (cmsUInt32Number)length);
    if (profile->lcms_handle == NULL) {
        return 0;
    }

    signature = cmsGetColorSpace(profile->lcms_handle);
    if (signature == cmsSigGrayData) {
        profile->colorspace = SIXEL_CMS_COLORSPACE_GRAY;
    } else if (signature == cmsSigRgbData) {
        profile->colorspace = SIXEL_CMS_COLORSPACE_RGB;
    } else if (signature == cmsSigCmykData) {
        profile->colorspace = SIXEL_CMS_COLORSPACE_CMYK;
    } else if (signature == cmsSigLabData) {
        profile->colorspace = SIXEL_CMS_COLORSPACE_LAB;
    } else {
        profile->colorspace = SIXEL_CMS_COLORSPACE_UNKNOWN;
    }

    profile->backend = SIXEL_CMS_PROFILE_BACKEND_LCMS2;
    return 1;
}

static int
sixel_cms_profile_create_srgb_lcms2(sixel_cms_profile_t *profile)
{
    if (profile == NULL) {
        return 0;
    }

    profile->lcms_handle = cmsCreate_sRGBProfile();
    if (profile->lcms_handle == NULL) {
        return 0;
    }

    profile->backend = SIXEL_CMS_PROFILE_BACKEND_LCMS2;
    profile->colorspace = SIXEL_CMS_COLORSPACE_RGB;
    profile->is_srgb_profile = 1;
    return 1;
}

static int
sixel_cms_profile_create_rgb_gamma_chrm_lcms2(
    sixel_cms_profile_t *profile,
    double file_gamma,
    double white_x,
    double white_y,
    double red_x,
    double red_y,
    double green_x,
    double green_y,
    double blue_x,
    double blue_y)
{
    cmsToneCurve *curve;
    cmsToneCurve *curves[3];
    cmsCIExyY white_point;
    cmsCIExyYTRIPLE primaries;

    if (profile == NULL || file_gamma <= 0.0) {
        return 0;
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
        return 0;
    }
    curves[0] = curve;
    curves[1] = curve;
    curves[2] = curve;

    profile->lcms_handle = cmsCreateRGBProfile(&white_point, &primaries, curves);
    cmsFreeToneCurve(curve);
    if (profile->lcms_handle == NULL) {
        return 0;
    }

    profile->backend = SIXEL_CMS_PROFILE_BACKEND_LCMS2;
    profile->colorspace = SIXEL_CMS_COLORSPACE_RGB;
    return 1;
}

static int
sixel_cms_profile_create_cielab_d50_lcms2(sixel_cms_profile_t *profile)
{
    if (profile == NULL) {
        return 0;
    }

    profile->lcms_handle = cmsCreateLab4Profile(NULL);
    if (profile->lcms_handle == NULL) {
        return 0;
    }

    profile->backend = SIXEL_CMS_PROFILE_BACKEND_LCMS2;
    profile->colorspace = SIXEL_CMS_COLORSPACE_LAB;
    return 1;
}
#endif

#if SIXEL_CMS_HAVE_COLORSYNC
static int
sixel_cms_matrix3_invert(double const m[9], double inv_out[9])
{
    double det;

    det = m[0] * (m[4] * m[8] - m[5] * m[7])
        - m[1] * (m[3] * m[8] - m[5] * m[6])
        + m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (fabs(det) < 1.0e-12) {
        return 0;
    }

    inv_out[0] = (m[4] * m[8] - m[5] * m[7]) / det;
    inv_out[1] = (m[2] * m[7] - m[1] * m[8]) / det;
    inv_out[2] = (m[1] * m[5] - m[2] * m[4]) / det;
    inv_out[3] = (m[5] * m[6] - m[3] * m[8]) / det;
    inv_out[4] = (m[0] * m[8] - m[2] * m[6]) / det;
    inv_out[5] = (m[2] * m[3] - m[0] * m[5]) / det;
    inv_out[6] = (m[3] * m[7] - m[4] * m[6]) / det;
    inv_out[7] = (m[1] * m[6] - m[0] * m[7]) / det;
    inv_out[8] = (m[0] * m[4] - m[1] * m[3]) / det;

    return 1;
}

static int
sixel_cms_build_rgb_to_xyz_matrix(double white_x,
                                  double white_y,
                                  double red_x,
                                  double red_y,
                                  double green_x,
                                  double green_y,
                                  double blue_x,
                                  double blue_y,
                                  double out_matrix[9])
{
    double primaries[9];
    double inv_primaries[9];
    double white_xyz[3];
    double scales[3];

    if (white_y <= 0.0 || red_y <= 0.0 || green_y <= 0.0 || blue_y <= 0.0) {
        return 0;
    }

    primaries[0] = red_x / red_y;
    primaries[1] = green_x / green_y;
    primaries[2] = blue_x / blue_y;
    primaries[3] = 1.0;
    primaries[4] = 1.0;
    primaries[5] = 1.0;
    primaries[6] = (1.0 - red_x - red_y) / red_y;
    primaries[7] = (1.0 - green_x - green_y) / green_y;
    primaries[8] = (1.0 - blue_x - blue_y) / blue_y;

    white_xyz[0] = white_x / white_y;
    white_xyz[1] = 1.0;
    white_xyz[2] = (1.0 - white_x - white_y) / white_y;

    if (!sixel_cms_matrix3_invert(primaries, inv_primaries)) {
        return 0;
    }

    scales[0] = inv_primaries[0] * white_xyz[0]
              + inv_primaries[1] * white_xyz[1]
              + inv_primaries[2] * white_xyz[2];
    scales[1] = inv_primaries[3] * white_xyz[0]
              + inv_primaries[4] * white_xyz[1]
              + inv_primaries[5] * white_xyz[2];
    scales[2] = inv_primaries[6] * white_xyz[0]
              + inv_primaries[7] * white_xyz[1]
              + inv_primaries[8] * white_xyz[2];

    out_matrix[0] = primaries[0] * scales[0];
    out_matrix[1] = primaries[1] * scales[1];
    out_matrix[2] = primaries[2] * scales[2];
    out_matrix[3] = primaries[3] * scales[0];
    out_matrix[4] = primaries[4] * scales[1];
    out_matrix[5] = primaries[5] * scales[2];
    out_matrix[6] = primaries[6] * scales[0];
    out_matrix[7] = primaries[7] * scales[1];
    out_matrix[8] = primaries[8] * scales[2];

    return 1;
}

static int
sixel_cms_profile_open_colorsync(sixel_cms_profile_t *profile,
                                 void const *data,
                                 size_t length)
{
    CFDataRef icc_data;
    CFErrorRef error;

    if (profile == NULL || data == NULL || length == 0u) {
        return 0;
    }

    icc_data = CFDataCreate(kCFAllocatorDefault,
                            (UInt8 const *)data,
                            (CFIndex)length);
    if (icc_data == NULL) {
        return 0;
    }

    error = NULL;
    profile->colorsync_handle = ColorSyncProfileCreate(icc_data, &error);
    if (error != NULL) {
        CFRelease(error);
    }
    CFRelease(icc_data);
    if (profile->colorsync_handle == NULL) {
        return 0;
    }

    profile->backend = SIXEL_CMS_PROFILE_BACKEND_COLORSYNC;
    profile->colorspace = sixel_cms_parse_icc_colorspace_signature(
        (unsigned char const *)data,
        length);
    return 1;
}

static int
sixel_cms_profile_create_srgb_colorsync(sixel_cms_profile_t *profile)
{
    if (profile == NULL) {
        return 0;
    }

    profile->colorsync_handle = ColorSyncProfileCreateWithName(
        kColorSyncSRGBProfile);
    if (profile->colorsync_handle == NULL) {
        return 0;
    }

    profile->backend = SIXEL_CMS_PROFILE_BACKEND_COLORSYNC;
    profile->colorspace = SIXEL_CMS_COLORSPACE_RGB;
    profile->is_srgb_profile = 1;
    return 1;
}

static int
sixel_cms_profile_create_rgb_gamma_chrm_colorsync(
    sixel_cms_profile_t *profile,
    double file_gamma,
    double white_x,
    double white_y,
    double red_x,
    double red_y,
    double green_x,
    double green_y,
    double blue_x,
    double blue_y)
{
    double matrix[9];
    CGFloat white_point[3];
    CGFloat gamma[3];
    CGFloat cg_matrix[9];
    CGColorSpaceRef color_space;
    CFDataRef icc_data;
    CFErrorRef error;
    size_t i;

    if (profile == NULL || file_gamma <= 0.0) {
        return 0;
    }

    if (!sixel_cms_build_rgb_to_xyz_matrix(white_x,
                                           white_y,
                                           red_x,
                                           red_y,
                                           green_x,
                                           green_y,
                                           blue_x,
                                           blue_y,
                                           matrix)) {
        return 0;
    }

    white_point[0] = (CGFloat)(white_x / white_y);
    white_point[1] = (CGFloat)1.0;
    white_point[2] = (CGFloat)((1.0 - white_x - white_y) / white_y);
    gamma[0] = (CGFloat)(1.0 / file_gamma);
    gamma[1] = (CGFloat)(1.0 / file_gamma);
    gamma[2] = (CGFloat)(1.0 / file_gamma);
    for (i = 0u; i < 9u; ++i) {
        cg_matrix[i] = (CGFloat)matrix[i];
    }

    color_space = CGColorSpaceCreateCalibratedRGB(white_point,
                                                   NULL,
                                                   gamma,
                                                   cg_matrix);
    if (color_space == NULL) {
        return 0;
    }

    icc_data = CGColorSpaceCopyICCData(color_space);
    CGColorSpaceRelease(color_space);
    if (icc_data == NULL) {
        return 0;
    }

    error = NULL;
    profile->colorsync_handle = ColorSyncProfileCreate(icc_data, &error);
    if (error != NULL) {
        CFRelease(error);
    }
    CFRelease(icc_data);
    if (profile->colorsync_handle == NULL) {
        return 0;
    }

    profile->backend = SIXEL_CMS_PROFILE_BACKEND_COLORSYNC;
    profile->colorspace = SIXEL_CMS_COLORSPACE_RGB;
    return 1;
}

static int
sixel_cms_profile_create_cielab_d50_colorsync(sixel_cms_profile_t *profile)
{
    if (profile == NULL) {
        return 0;
    }

    profile->colorsync_handle = ColorSyncProfileCreateWithName(
        kColorSyncGenericLabProfile);
    if (profile->colorsync_handle == NULL) {
        return 0;
    }

    profile->backend = SIXEL_CMS_PROFILE_BACKEND_COLORSYNC;
    profile->colorspace = SIXEL_CMS_COLORSPACE_LAB;
    return 1;
}

static CFStringRef
sixel_cms_colorsync_intent_to_cfstring(int intent)
{
    switch (intent) {
    case SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC:
        return kColorSyncRenderingIntentRelative;
    case SIXEL_CMS_INTENT_SATURATION:
        return kColorSyncRenderingIntentSaturation;
    case SIXEL_CMS_INTENT_ABSOLUTE_COLORIMETRIC:
        return kColorSyncRenderingIntentAbsolute;
    case SIXEL_CMS_INTENT_PERCEPTUAL:
    default:
        return kColorSyncRenderingIntentPerceptual;
    }
}

static int
sixel_cms_colorsync_map_format(sixel_cms_pixel_format_t format,
                               ColorSyncDataDepth *depth,
                               ColorSyncDataLayout *layout,
                               size_t *bytes_per_pixel)
{
    ColorSyncDataLayout value_layout;

    if (depth == NULL || layout == NULL || bytes_per_pixel == NULL) {
        return 0;
    }

    value_layout = kColorSyncAlphaNone;
    switch (format) {
    case SIXEL_CMS_PIXELFORMAT_GRAY_8:
        *depth = kColorSync8BitInteger;
        *bytes_per_pixel = 1u;
        break;
    case SIXEL_CMS_PIXELFORMAT_RGB_8:
        *depth = kColorSync8BitInteger;
        *bytes_per_pixel = 3u;
        break;
    case SIXEL_CMS_PIXELFORMAT_RGBA_8:
        *depth = kColorSync8BitInteger;
        value_layout = kColorSyncAlphaLast;
        *bytes_per_pixel = 4u;
        break;
    case SIXEL_CMS_PIXELFORMAT_RGB_F32:
        *depth = kColorSync32BitFloat;
#if WORDS_BIGENDIAN
        value_layout |= kColorSyncByteOrder32Big;
#else
        value_layout |= kColorSyncByteOrder32Little;
#endif
        *bytes_per_pixel = 3u * sizeof(float);
        break;
    case SIXEL_CMS_PIXELFORMAT_CMYK_8:
        *depth = kColorSync8BitInteger;
        *bytes_per_pixel = 4u;
        break;
    case SIXEL_CMS_PIXELFORMAT_CMYK_16:
        *depth = kColorSync16BitInteger;
#if WORDS_BIGENDIAN
        value_layout |= kColorSyncByteOrder16Big;
#else
        value_layout |= kColorSyncByteOrder16Little;
#endif
        *bytes_per_pixel = 4u * sizeof(uint16_t);
        break;
    case SIXEL_CMS_PIXELFORMAT_CMYK_F32:
        *depth = kColorSync32BitFloat;
#if WORDS_BIGENDIAN
        value_layout |= kColorSyncByteOrder32Big;
#else
        value_layout |= kColorSyncByteOrder32Little;
#endif
        *bytes_per_pixel = 4u * sizeof(float);
        break;
    case SIXEL_CMS_PIXELFORMAT_LAB_F32:
        *depth = kColorSync32BitFloat;
#if WORDS_BIGENDIAN
        value_layout |= kColorSyncByteOrder32Big;
#else
        value_layout |= kColorSyncByteOrder32Little;
#endif
        *bytes_per_pixel = 3u * sizeof(float);
        break;
    default:
        return 0;
    }

    *layout = value_layout;
    return 1;
}

static ColorSyncTransformRef
sixel_cms_colorsync_create_transform_one(
    ColorSyncProfileRef src_profile,
    ColorSyncProfileRef dst_profile,
    int intent,
    CFStringRef src_tag,
    CFStringRef dst_tag)
{
    CFTypeRef src_keys[3];
    CFTypeRef src_values[3];
    CFTypeRef dst_keys[3];
    CFTypeRef dst_values[3];
    CFDictionaryRef src_dict;
    CFDictionaryRef dst_dict;
    CFTypeRef sequence_values[2];
    CFArrayRef sequence;
    ColorSyncTransformRef transform;

    src_dict = NULL;
    dst_dict = NULL;
    sequence = NULL;
    transform = NULL;

    src_keys[0] = kColorSyncProfile;
    src_values[0] = src_profile;
    src_keys[1] = kColorSyncRenderingIntent;
    src_values[1] = sixel_cms_colorsync_intent_to_cfstring(intent);
    src_keys[2] = kColorSyncTransformTag;
    src_values[2] = src_tag;

    dst_keys[0] = kColorSyncProfile;
    dst_values[0] = dst_profile;
    dst_keys[1] = kColorSyncRenderingIntent;
    dst_values[1] = sixel_cms_colorsync_intent_to_cfstring(intent);
    dst_keys[2] = kColorSyncTransformTag;
    dst_values[2] = dst_tag;

    src_dict = CFDictionaryCreate(kCFAllocatorDefault,
                                  src_keys,
                                  src_values,
                                  3,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks);
    if (src_dict == NULL) {
        return NULL;
    }

    dst_dict = CFDictionaryCreate(kCFAllocatorDefault,
                                  dst_keys,
                                  dst_values,
                                  3,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks);
    if (dst_dict == NULL) {
        CFRelease(src_dict);
        return NULL;
    }

    sequence_values[0] = src_dict;
    sequence_values[1] = dst_dict;
    sequence = CFArrayCreate(kCFAllocatorDefault,
                             sequence_values,
                             2,
                             &kCFTypeArrayCallBacks);
    if (sequence != NULL) {
        transform = ColorSyncTransformCreate(sequence, NULL);
        CFRelease(sequence);
    }

    CFRelease(dst_dict);
    CFRelease(src_dict);
    return transform;
}
#endif

static int
sixel_cms_profile_open_builtin(sixel_cms_profile_t *profile,
                               void const *data,
                               size_t length)
{
    size_t illuminant_offset;
    int has_dynamic_lut_path;
    int has_secondary_a2b_slot;
    int has_any_b2a_slot;
    size_t slot;

    illuminant_offset = 0u;
    has_dynamic_lut_path = 0;
    has_secondary_a2b_slot = 0;
    has_any_b2a_slot = 0;
    slot = 0u;
    if (profile == NULL || data == NULL || length == 0u) {
        return 0;
    }

    if (!sixel_icc_parse_profile(data, length, &profile->builtin_profile)) {
        return 0;
    }

    /*
     * Some RGB/GRAY fixtures ship a single A2B0 LUT with a zero PCS
     * illuminant. ColorSync treats that shape as non-convertible and
     * effectively falls back to no ICC conversion. Builtin mirrors only that
     * narrow case so intent-slot and CMYK LUT coverage remain active.
     */
    illuminant_offset = 68u;
    for (slot = 0u; slot < SIXEL_ICC_A2B_SLOT_COUNT; ++slot) {
        if (profile->builtin_profile.a2b_mab[slot].type
                != SIXEL_ICC_MAB_TYPE_INVALID ||
            profile->builtin_profile.a2b_lut[slot].kind
                != SIXEL_ICC_LUT_INVALID ||
            profile->builtin_profile.b2a_mab[slot].type
                != SIXEL_ICC_MAB_TYPE_INVALID ||
            profile->builtin_profile.b2a_lut[slot].kind
                != SIXEL_ICC_LUT_INVALID) {
            has_dynamic_lut_path = 1;
        }
        if (slot > 0u &&
            (profile->builtin_profile.a2b_mab[slot].type
                 != SIXEL_ICC_MAB_TYPE_INVALID ||
             profile->builtin_profile.a2b_lut[slot].kind
                 != SIXEL_ICC_LUT_INVALID)) {
            has_secondary_a2b_slot = 1;
        }
        if (profile->builtin_profile.b2a_mab[slot].type
                != SIXEL_ICC_MAB_TYPE_INVALID ||
            profile->builtin_profile.b2a_lut[slot].kind
                != SIXEL_ICC_LUT_INVALID) {
            has_any_b2a_slot = 1;
        }
    }
    if (has_dynamic_lut_path &&
        (profile->builtin_profile.kind == SIXEL_ICC_PROFILE_KIND_RGB ||
         profile->builtin_profile.kind == SIXEL_ICC_PROFILE_KIND_GRAY) &&
        !has_secondary_a2b_slot &&
        !has_any_b2a_slot &&
        length >= illuminant_offset + 12u &&
        memcmp((unsigned char const *)data + illuminant_offset,
               "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
               12u) == 0) {
        sixel_icc_profile_destroy(&profile->builtin_profile);
        return 0;
    }

    profile->builtin_profile_valid = 1;
    profile->backend = SIXEL_CMS_PROFILE_BACKEND_BUILTIN;
    switch (profile->builtin_profile.kind) {
    case SIXEL_ICC_PROFILE_KIND_RGB:
        profile->colorspace = SIXEL_CMS_COLORSPACE_RGB;
        break;
    case SIXEL_ICC_PROFILE_KIND_GRAY:
        profile->colorspace = SIXEL_CMS_COLORSPACE_GRAY;
        break;
    case SIXEL_ICC_PROFILE_KIND_CMYK:
        profile->colorspace = SIXEL_CMS_COLORSPACE_CMYK;
        break;
    default:
        profile->colorspace = SIXEL_CMS_COLORSPACE_UNKNOWN;
        break;
    }

    return 1;
}

static int
sixel_cms_profile_create_srgb_builtin(sixel_cms_profile_t *profile)
{
    if (profile == NULL) {
        return 0;
    }

    profile->backend = SIXEL_CMS_PROFILE_BACKEND_BUILTIN;
    profile->colorspace = SIXEL_CMS_COLORSPACE_RGB;
    profile->is_srgb_profile = 1;
    return 1;
}

sixel_cms_profile_t *
sixel_cms_open_profile_from_mem(void const *data, size_t length)
{
    sixel_cms_profile_t *profile;
    sixel_cms_engine_t engine;

    if (data == NULL || length == 0u) {
        return NULL;
    }

    profile = sixel_cms_allocate_profile();
    if (profile == NULL) {
        return NULL;
    }

    engine = sixel_cms_get_engine();
    if (engine == SIXEL_CMS_ENGINE_NONE) {
        free(profile);
        return NULL;
    }
#if HAVE_LCMS2
    if (engine == SIXEL_CMS_ENGINE_LCMS2 &&
        sixel_cms_profile_open_lcms2(profile, data, length)) {
        return profile;
    }
#endif
#if SIXEL_CMS_HAVE_COLORSYNC
    if (engine == SIXEL_CMS_ENGINE_COLORSYNC &&
        sixel_cms_profile_open_colorsync(profile, data, length)) {
        return profile;
    }
#endif
    if (sixel_cms_profile_open_builtin(profile, data, length)) {
        return profile;
    }

    free(profile);
    return NULL;
}

SIXELAPI sixel_cms_profile_t *
sixel_cms_create_srgb_profile(void)
{
    sixel_cms_profile_t *profile;
    sixel_cms_engine_t engine;

    profile = sixel_cms_allocate_profile();
    if (profile == NULL) {
        return NULL;
    }

    engine = sixel_cms_get_engine();
    if (engine == SIXEL_CMS_ENGINE_NONE) {
        free(profile);
        return NULL;
    }
#if HAVE_LCMS2
    if (engine == SIXEL_CMS_ENGINE_LCMS2 &&
        sixel_cms_profile_create_srgb_lcms2(profile)) {
        return profile;
    }
#endif
#if SIXEL_CMS_HAVE_COLORSYNC
    if (engine == SIXEL_CMS_ENGINE_COLORSYNC &&
        sixel_cms_profile_create_srgb_colorsync(profile)) {
        return profile;
    }
#endif
    if (sixel_cms_profile_create_srgb_builtin(profile)) {
        return profile;
    }

    free(profile);
    return NULL;
}

SIXELAPI sixel_cms_profile_t *
sixel_cms_create_linear_srgb_profile(void)
{
    return sixel_cms_create_rgb_profile_from_gamma_chrm(
        1.0,
        SIXEL_SRGB_WHITE_X,
        SIXEL_SRGB_WHITE_Y,
        SIXEL_SRGB_RED_X,
        SIXEL_SRGB_RED_Y,
        SIXEL_SRGB_GREEN_X,
        SIXEL_SRGB_GREEN_Y,
        SIXEL_SRGB_BLUE_X,
        SIXEL_SRGB_BLUE_Y);
}

SIXELAPI sixel_cms_profile_t *
sixel_cms_create_cielab_d50_profile(void)
{
    sixel_cms_profile_t *profile;
    sixel_cms_engine_t engine;

    profile = sixel_cms_allocate_profile();
    if (profile == NULL) {
        return NULL;
    }

    engine = sixel_cms_get_engine();
    if (engine == SIXEL_CMS_ENGINE_NONE) {
        free(profile);
        return NULL;
    }
#if HAVE_LCMS2
    if (engine == SIXEL_CMS_ENGINE_LCMS2 &&
        sixel_cms_profile_create_cielab_d50_lcms2(profile)) {
        return profile;
    }
#endif
#if SIXEL_CMS_HAVE_COLORSYNC
    if (engine == SIXEL_CMS_ENGINE_COLORSYNC &&
        sixel_cms_profile_create_cielab_d50_colorsync(profile)) {
        return profile;
    }
#endif
    free(profile);
    return NULL;
}

SIXELAPI sixel_cms_profile_t *
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
    sixel_cms_engine_t engine;

#if !HAVE_LCMS2 && !SIXEL_CMS_HAVE_COLORSYNC
    (void)file_gamma;
    (void)white_x;
    (void)white_y;
    (void)red_x;
    (void)red_y;
    (void)green_x;
    (void)green_y;
    (void)blue_x;
    (void)blue_y;
#endif

    profile = sixel_cms_allocate_profile();
    if (profile == NULL) {
        return NULL;
    }

    engine = sixel_cms_get_engine();
    if (engine == SIXEL_CMS_ENGINE_NONE) {
        free(profile);
        return NULL;
    }
#if HAVE_LCMS2
    if (engine == SIXEL_CMS_ENGINE_LCMS2 &&
        sixel_cms_profile_create_rgb_gamma_chrm_lcms2(profile,
                                                      file_gamma,
                                                      white_x,
                                                      white_y,
                                                      red_x,
                                                      red_y,
                                                      green_x,
                                                      green_y,
                                                      blue_x,
                                                      blue_y)) {
        return profile;
    }
#endif
#if SIXEL_CMS_HAVE_COLORSYNC
    if (engine == SIXEL_CMS_ENGINE_COLORSYNC &&
        sixel_cms_profile_create_rgb_gamma_chrm_colorsync(profile,
                                                          file_gamma,
                                                          white_x,
                                                          white_y,
                                                          red_x,
                                                          red_y,
                                                          green_x,
                                                          green_y,
                                                          blue_x,
                                                          blue_y)) {
        return profile;
    }
#endif

    free(profile);
    return NULL;
}

sixel_cms_color_space_t
sixel_cms_get_color_space(sixel_cms_profile_t const *profile)
{
    if (profile == NULL) {
        return SIXEL_CMS_COLORSPACE_UNKNOWN;
    }

    return profile->colorspace;
}

SIXELAPI void
sixel_cms_close_profile(sixel_cms_profile_t *profile)
{
    if (profile == NULL) {
        return;
    }

#if HAVE_LCMS2
    if (profile->lcms_handle != NULL) {
        cmsCloseProfile(profile->lcms_handle);
        profile->lcms_handle = NULL;
    }
#endif
#if SIXEL_CMS_HAVE_COLORSYNC
    if (profile->colorsync_handle != NULL) {
        CFRelease(profile->colorsync_handle);
        profile->colorsync_handle = NULL;
    }
#endif
    if (profile->builtin_profile_valid) {
        sixel_icc_profile_destroy(&profile->builtin_profile);
        profile->builtin_profile_valid = 0;
    }

    free(profile);
}

#if HAVE_LCMS2
static sixel_cms_transform_t *
sixel_cms_create_transform_lcms2(sixel_cms_profile_t const *src_profile,
                                 sixel_cms_pixel_format_t src_format,
                                 sixel_cms_profile_t const *dst_profile,
                                 sixel_cms_pixel_format_t dst_format,
                                 int flags)
{
    sixel_cms_transform_t *transform;
    cmsUInt32Number cflags;
    cmsUInt32Number trial_flags[2];
    size_t trial_flag_count;
    size_t tf;
    int intents[4];
    size_t intent_count;
    size_t i;

    if (src_profile == NULL || dst_profile == NULL ||
        src_profile->lcms_handle == NULL || dst_profile->lcms_handle == NULL) {
        return NULL;
    }

    transform = (sixel_cms_transform_t *)malloc(sizeof(*transform));
    if (transform == NULL) {
        return NULL;
    }
    memset(transform, 0, sizeof(*transform));

    transform->backend = SIXEL_CMS_PROFILE_BACKEND_LCMS2;
    transform->src_format = src_format;
    transform->dst_format = dst_format;
    transform->flags = flags;
    transform->src_profile = src_profile;
    transform->dst_profile = dst_profile;

    cflags = 0;
    if ((flags & SIXEL_CMS_TRANSFORM_COPY_ALPHA) != 0) {
        cflags |= cmsFLAGS_COPY_ALPHA;
    }

    trial_flags[0] = cflags;
    trial_flags[1] = cflags | cmsFLAGS_NOOPTIMIZE;
    trial_flag_count = 2u;

    intent_count = sixel_cms_build_intent_order(intents);
    transform->lcms_handle = NULL;
    for (tf = 0u; tf < trial_flag_count && transform->lcms_handle == NULL; ++tf) {
        for (i = 0u; i < intent_count; ++i) {
            transform->lcms_handle = cmsCreateTransform(
                src_profile->lcms_handle,
                sixel_cms_map_format_lcms(src_format),
                dst_profile->lcms_handle,
                sixel_cms_map_format_lcms(dst_format),
                intents[i],
                trial_flags[tf]);
            if (transform->lcms_handle != NULL) {
                break;
            }
        }
    }

    if (transform->lcms_handle == NULL) {
        free(transform);
        return NULL;
    }

    return transform;
}
#endif

#if SIXEL_CMS_HAVE_COLORSYNC
static sixel_cms_transform_t *
sixel_cms_create_transform_colorsync(sixel_cms_profile_t const *src_profile,
                                     sixel_cms_pixel_format_t src_format,
                                     sixel_cms_profile_t const *dst_profile,
                                     sixel_cms_pixel_format_t dst_format,
                                     int flags)
{
    sixel_cms_transform_t *transform;
    int intents[4];
    size_t intent_count;
    size_t i;

    if (src_profile == NULL || dst_profile == NULL ||
        src_profile->colorsync_handle == NULL ||
        dst_profile->colorsync_handle == NULL) {
        return NULL;
    }

    transform = (sixel_cms_transform_t *)malloc(sizeof(*transform));
    if (transform == NULL) {
        return NULL;
    }
    memset(transform, 0, sizeof(*transform));

    transform->backend = SIXEL_CMS_PROFILE_BACKEND_COLORSYNC;
    transform->src_format = src_format;
    transform->dst_format = dst_format;
    transform->flags = flags;
    transform->src_profile = src_profile;
    transform->dst_profile = dst_profile;

    if (!sixel_cms_colorsync_map_format(src_format,
                                        &transform->src_depth,
                                        &transform->src_layout,
                                        &transform->src_bytes_per_pixel) ||
        !sixel_cms_colorsync_map_format(dst_format,
                                        &transform->dst_depth,
                                        &transform->dst_layout,
                                        &transform->dst_bytes_per_pixel)) {
        free(transform);
        return NULL;
    }

    intent_count = sixel_cms_build_intent_order(intents);
    for (i = 0u; i < intent_count; ++i) {
        transform->colorsync_handle = sixel_cms_colorsync_create_transform_one(
            src_profile->colorsync_handle,
            dst_profile->colorsync_handle,
            intents[i],
            kColorSyncTransformDeviceToPCS,
            kColorSyncTransformPCSToDevice);
        if (transform->colorsync_handle != NULL) {
            break;
        }
        transform->colorsync_handle = sixel_cms_colorsync_create_transform_one(
            src_profile->colorsync_handle,
            dst_profile->colorsync_handle,
            intents[i],
            kColorSyncTransformDeviceToDevice,
            kColorSyncTransformDeviceToDevice);
        if (transform->colorsync_handle != NULL) {
            break;
        }
    }

    if (transform->colorsync_handle == NULL) {
        free(transform);
        return NULL;
    }

    return transform;
}
#endif

static sixel_cms_transform_t *
sixel_cms_create_transform_builtin(sixel_cms_profile_t const *src_profile,
                                   sixel_cms_pixel_format_t src_format,
                                   sixel_cms_profile_t const *dst_profile,
                                   sixel_cms_pixel_format_t dst_format,
                                   int flags)
{
    sixel_cms_transform_t *transform;
    size_t i;

    i = 0u;
    if (src_profile == NULL || dst_profile == NULL) {
        return NULL;
    }

    transform = (sixel_cms_transform_t *)malloc(sizeof(*transform));
    if (transform == NULL) {
        return NULL;
    }
    memset(transform, 0, sizeof(*transform));

    transform->backend = SIXEL_CMS_PROFILE_BACKEND_BUILTIN;
    transform->src_format = src_format;
    transform->dst_format = dst_format;
    transform->flags = flags;
    transform->src_profile = src_profile;
    transform->dst_profile = dst_profile;
    transform->builtin_a2b_slot_count
        = sixel_cms_build_builtin_a2b_order(transform->builtin_a2b_slots);
    for (i = transform->builtin_a2b_slot_count;
         i < SIXEL_ICC_A2B_SLOT_COUNT;
         ++i) {
        /* i is bounded by SIXEL_ICC_A2B_SLOT_COUNT (0..2). */
        transform->builtin_a2b_slots[i] = (unsigned int)i;
    }

    return transform;
}

sixel_cms_transform_t *
sixel_cms_create_transform(sixel_cms_profile_t const *src_profile,
                           sixel_cms_pixel_format_t src_format,
                           sixel_cms_profile_t const *dst_profile,
                           sixel_cms_pixel_format_t dst_format,
                           int flags)
{
    if (src_profile == NULL || dst_profile == NULL) {
        return NULL;
    }
    if (src_profile->backend != dst_profile->backend) {
        return NULL;
    }

#if HAVE_LCMS2
    if (src_profile->backend == SIXEL_CMS_PROFILE_BACKEND_LCMS2) {
        return sixel_cms_create_transform_lcms2(src_profile,
                                                src_format,
                                                dst_profile,
                                                dst_format,
                                                flags);
    }
#endif
#if SIXEL_CMS_HAVE_COLORSYNC
    if (src_profile->backend == SIXEL_CMS_PROFILE_BACKEND_COLORSYNC) {
        return sixel_cms_create_transform_colorsync(src_profile,
                                                    src_format,
                                                    dst_profile,
                                                    dst_format,
                                                    flags);
    }
#endif
    if (src_profile->backend == SIXEL_CMS_PROFILE_BACKEND_BUILTIN) {
        return sixel_cms_create_transform_builtin(src_profile,
                                                  src_format,
                                                  dst_profile,
                                                  dst_format,
                                                  flags);
    }

    return NULL;
}

static int
sixel_cms_builtin_profile_is_srgb(sixel_cms_profile_t const *profile)
{
    if (profile == NULL) {
        return 0;
    }
    return profile->is_srgb_profile != 0;
}

static int
sixel_cms_builtin_profile_can_convert(sixel_cms_profile_t const *profile)
{
    if (profile == NULL) {
        return 0;
    }
    if (profile->is_srgb_profile) {
        return 1;
    }
    return profile->builtin_profile_valid != 0;
}

static int
sixel_cms_builtin_convert_rgb8_to_rgbf32(float *dst,
                                         unsigned char const *src,
                                         size_t pixel_count)
{
    size_t i;
    size_t src_offset;
    size_t dst_offset;

    if (dst == NULL || src == NULL) {
        return 0;
    }

    for (i = 0u; i < pixel_count; ++i) {
        src_offset = i * 3u;
        dst_offset = i * 3u;
        dst[dst_offset + 0u] = (float)src[src_offset + 0u] / 255.0f;
        dst[dst_offset + 1u] = (float)src[src_offset + 1u] / 255.0f;
        dst[dst_offset + 2u] = (float)src[src_offset + 2u] / 255.0f;
    }

    return 1;
}

static double const sixel_cms_srgb_to_xyz_d65[3][3] = {
    { 0.412390799265959, 0.357584339383878, 0.180480788401834 },
    { 0.212639005871510, 0.715168678767756, 0.072192315360734 },
    { 0.019330818715592, 0.119194779794626, 0.950532152249661 }
};

static double const sixel_cms_xyz_d65_to_d50[3][3] = {
    { 1.047811243660631, 0.022886602481693, -0.050126975968528 },
    { 0.029542398290574, 0.990484403490440, -0.017049095628961 },
    { -0.009234489723310, 0.015043616793498, 0.752131635474607 }
};

static double
sixel_cms_clamp_unit(double value)
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
sixel_cms_decode_srgb_unit(double value)
{
    value = sixel_cms_clamp_unit(value);
    if (value <= 0.04045) {
        return value / 12.92;
    }
    return pow((value + 0.055) / 1.055, 2.4);
}

static void
sixel_cms_apply_matrix3(double const matrix[3][3],
                        double const in[3],
                        double out[3])
{
    out[0] = matrix[0][0] * in[0] + matrix[0][1] * in[1] + matrix[0][2] * in[2];
    out[1] = matrix[1][0] * in[0] + matrix[1][1] * in[1] + matrix[1][2] * in[2];
    out[2] = matrix[2][0] * in[0] + matrix[2][1] * in[1] + matrix[2][2] * in[2];
}

static void
sixel_cms_srgb_unit_to_xyz_d50(double xyz_d50[3], double const rgb_unit[3])
{
    double rgb_linear[3];
    double xyz_d65[3];

    rgb_linear[0] = sixel_cms_decode_srgb_unit(rgb_unit[0]);
    rgb_linear[1] = sixel_cms_decode_srgb_unit(rgb_unit[1]);
    rgb_linear[2] = sixel_cms_decode_srgb_unit(rgb_unit[2]);

    sixel_cms_apply_matrix3(sixel_cms_srgb_to_xyz_d65, rgb_linear, xyz_d65);
    sixel_cms_apply_matrix3(sixel_cms_xyz_d65_to_d50, xyz_d65, xyz_d50);
}

static int
sixel_cms_builtin_apply_rgb_u8_intent(
    unsigned char *pixels,
    size_t pixel_count,
    sixel_icc_profile_t const *profile,
    sixel_cms_transform_t const *transform)
{
    size_t i;

    i = 0u;
    if (pixels == NULL || profile == NULL || transform == NULL) {
        return 0;
    }

    for (i = 0u; i < transform->builtin_a2b_slot_count; ++i) {
        if (sixel_icc_apply_rgb_u8_with_a2b_slot(
                pixels,
                pixel_count,
                profile,
                transform->builtin_a2b_slots[i],
                0)) {
            return 1;
        }
    }

    return sixel_icc_apply_rgb_u8_with_a2b_slot(
        pixels,
        pixel_count,
        profile,
        SIXEL_ICC_A2B_SLOT_COUNT,
        1);
}

static int
sixel_cms_builtin_apply_rgb_f32_intent(
    float *pixels,
    size_t pixel_count,
    sixel_icc_profile_t const *profile,
    sixel_cms_transform_t const *transform)
{
    size_t i;

    i = 0u;
    if (pixels == NULL || profile == NULL || transform == NULL) {
        return 0;
    }

    for (i = 0u; i < transform->builtin_a2b_slot_count; ++i) {
        if (sixel_icc_apply_rgb_float32_with_a2b_slot(
                pixels,
                pixel_count,
                profile,
                transform->builtin_a2b_slots[i],
                0)) {
            return 1;
        }
    }

    return sixel_icc_apply_rgb_float32_with_a2b_slot(
        pixels,
        pixel_count,
        profile,
        SIXEL_ICC_A2B_SLOT_COUNT,
        1);
}

static int
sixel_cms_builtin_apply_gray_u8_intent(
    unsigned char *pixels,
    size_t pixel_count,
    sixel_icc_profile_t const *profile,
    sixel_cms_transform_t const *transform)
{
    size_t i;

    i = 0u;
    if (pixels == NULL || profile == NULL || transform == NULL) {
        return 0;
    }

    for (i = 0u; i < transform->builtin_a2b_slot_count; ++i) {
        if (sixel_icc_apply_gray_u8_with_a2b_slot(
                pixels,
                pixel_count,
                profile,
                transform->builtin_a2b_slots[i],
                0)) {
            return 1;
        }
    }

    return sixel_icc_apply_gray_u8_with_a2b_slot(
        pixels,
        pixel_count,
        profile,
        SIXEL_ICC_A2B_SLOT_COUNT,
        1);
}

static int
sixel_cms_builtin_apply_cmyk_u8_intent(
    float *dst_pixels,
    unsigned char const *src_pixels,
    size_t pixel_count,
    sixel_icc_profile_t const *profile,
    sixel_cms_transform_t const *transform)
{
    size_t i;

    i = 0u;
    if (dst_pixels == NULL || src_pixels == NULL ||
        profile == NULL || transform == NULL) {
        return 0;
    }

    for (i = 0u; i < transform->builtin_a2b_slot_count; ++i) {
        if (sixel_icc_apply_cmyk_u8_to_rgb_float32_with_a2b_slot(
                dst_pixels,
                src_pixels,
                pixel_count,
                profile,
                transform->builtin_a2b_slots[i])) {
            return 1;
        }
    }

    return 0;
}

static int
sixel_cms_builtin_apply_cmyk_u16_intent(
    float *dst_pixels,
    uint16_t const *src_pixels,
    size_t pixel_count,
    sixel_icc_profile_t const *profile,
    sixel_cms_transform_t const *transform)
{
    size_t i;

    i = 0u;
    if (dst_pixels == NULL || src_pixels == NULL ||
        profile == NULL || transform == NULL) {
        return 0;
    }

    for (i = 0u; i < transform->builtin_a2b_slot_count; ++i) {
        if (sixel_icc_apply_cmyk_u16_to_rgb_float32_with_a2b_slot(
                dst_pixels,
                src_pixels,
                pixel_count,
                profile,
                transform->builtin_a2b_slots[i])) {
            return 1;
        }
    }

    return 0;
}

static int
sixel_cms_builtin_apply_cmyk_f32_intent(
    float *dst_pixels,
    float const *src_pixels,
    size_t pixel_count,
    sixel_icc_profile_t const *profile,
    sixel_cms_transform_t const *transform)
{
    size_t i;

    i = 0u;
    if (dst_pixels == NULL || src_pixels == NULL ||
        profile == NULL || transform == NULL) {
        return 0;
    }

    for (i = 0u; i < transform->builtin_a2b_slot_count; ++i) {
        if (sixel_icc_apply_cmyk_float32_to_rgb_float32_with_a2b_slot(
                dst_pixels,
                src_pixels,
                pixel_count,
                profile,
                transform->builtin_a2b_slots[i])) {
            return 1;
        }
    }

    return 0;
}

static int
sixel_cms_builtin_apply_src_to_xyz_intent(
    double xyz_d50[3],
    double const *src_unit,
    size_t input_channel_count,
    sixel_cms_profile_t const *src_profile,
    sixel_cms_transform_t const *transform)
{
    size_t i;
    sixel_icc_profile_kind_t kind;

    i = 0u;
    kind = SIXEL_ICC_PROFILE_KIND_INVALID;
    if (xyz_d50 == NULL || src_unit == NULL ||
        src_profile == NULL || transform == NULL) {
        return 0;
    }

    if (sixel_cms_builtin_profile_is_srgb(src_profile)) {
        if (input_channel_count != 3u) {
            return 0;
        }
        sixel_cms_srgb_unit_to_xyz_d50(xyz_d50, src_unit);
        return 1;
    }
    if (!src_profile->builtin_profile_valid) {
        return 0;
    }

    for (i = 0u; i < transform->builtin_a2b_slot_count; ++i) {
        if (sixel_icc_apply_device_to_xyz_d50_with_a2b_slot(
                xyz_d50,
                src_unit,
                input_channel_count,
                &src_profile->builtin_profile,
                transform->builtin_a2b_slots[i],
                0)) {
            return 1;
        }
    }

    kind = src_profile->builtin_profile.kind;
    if (kind != SIXEL_ICC_PROFILE_KIND_RGB &&
        kind != SIXEL_ICC_PROFILE_KIND_GRAY) {
        return 0;
    }

    return sixel_icc_apply_device_to_xyz_d50_with_a2b_slot(
        xyz_d50,
        src_unit,
        input_channel_count,
        &src_profile->builtin_profile,
        SIXEL_ICC_A2B_SLOT_COUNT,
        1);
}

static int
sixel_cms_builtin_apply_xyz_to_dst_intent(
    double *dst_unit,
    size_t output_channel_count,
    double const xyz_d50[3],
    sixel_cms_profile_t const *dst_profile,
    sixel_cms_transform_t const *transform)
{
    size_t i;

    i = 0u;
    if (dst_unit == NULL || xyz_d50 == NULL ||
        dst_profile == NULL || transform == NULL) {
        return 0;
    }
    if (!dst_profile->builtin_profile_valid) {
        return 0;
    }

    for (i = 0u; i < transform->builtin_a2b_slot_count; ++i) {
        if (sixel_icc_apply_xyz_d50_to_device_with_b2a_slot(
                dst_unit,
                output_channel_count,
                xyz_d50,
                &dst_profile->builtin_profile,
                transform->builtin_a2b_slots[i])) {
            return 1;
        }
    }

    return 0;
}

static int
sixel_cms_do_transform_builtin_device_to_device(
    sixel_cms_transform_t const *transform,
    void const *src,
    void *dst,
    size_t pixel_count)
{
    sixel_cms_profile_t const *src_profile;
    sixel_cms_profile_t const *dst_profile;
    unsigned char const *src_u8;
    unsigned char *dst_u8;
    float const *src_f32;
    float *dst_f32;
    uint16_t const *src_u16;
    size_t i;
    double src_unit[4];
    double xyz_d50[3];
    double dst_unit[4];
    unsigned char alpha;

    src_profile = NULL;
    dst_profile = NULL;
    src_u8 = NULL;
    dst_u8 = NULL;
    src_f32 = NULL;
    dst_f32 = NULL;
    src_u16 = NULL;
    i = 0u;
    memset(src_unit, 0, sizeof(src_unit));
    memset(xyz_d50, 0, sizeof(xyz_d50));
    memset(dst_unit, 0, sizeof(dst_unit));
    alpha = 255u;
    if (transform == NULL || src == NULL || dst == NULL || pixel_count == 0u) {
        return 0;
    }

    src_profile = transform->src_profile;
    dst_profile = transform->dst_profile;
    if (src_profile == NULL || dst_profile == NULL) {
        return 0;
    }
    if (!sixel_cms_builtin_profile_can_convert(src_profile) ||
        !sixel_cms_builtin_profile_can_convert(dst_profile)) {
        return 0;
    }
    if (sixel_cms_builtin_profile_is_srgb(dst_profile)) {
        return 0;
    }

    src_u8 = (unsigned char const *)src;
    dst_u8 = (unsigned char *)dst;
    src_f32 = (float const *)src;
    dst_f32 = (float *)dst;

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_RGB_8 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_8 &&
        dst_profile->builtin_profile.kind == SIXEL_ICC_PROFILE_KIND_RGB) {
        for (i = 0u; i < pixel_count; ++i) {
            src_unit[0] = (double)src_u8[i * 3u + 0u] / 255.0;
            src_unit[1] = (double)src_u8[i * 3u + 1u] / 255.0;
            src_unit[2] = (double)src_u8[i * 3u + 2u] / 255.0;
            if (!sixel_cms_builtin_apply_src_to_xyz_intent(
                    xyz_d50,
                    src_unit,
                    3u,
                    src_profile,
                    transform) ||
                !sixel_cms_builtin_apply_xyz_to_dst_intent(
                    dst_unit,
                    3u,
                    xyz_d50,
                    dst_profile,
                    transform)) {
                return 0;
            }
            dst_u8[i * 3u + 0u] =
                (unsigned char)(sixel_cms_clamp_unit(dst_unit[0]) * 255.0
                                + 0.5);
            dst_u8[i * 3u + 1u] =
                (unsigned char)(sixel_cms_clamp_unit(dst_unit[1]) * 255.0
                                + 0.5);
            dst_u8[i * 3u + 2u] =
                (unsigned char)(sixel_cms_clamp_unit(dst_unit[2]) * 255.0
                                + 0.5);
        }
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_RGB_8 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32 &&
        dst_profile->builtin_profile.kind == SIXEL_ICC_PROFILE_KIND_RGB) {
        for (i = 0u; i < pixel_count; ++i) {
            src_unit[0] = (double)src_u8[i * 3u + 0u] / 255.0;
            src_unit[1] = (double)src_u8[i * 3u + 1u] / 255.0;
            src_unit[2] = (double)src_u8[i * 3u + 2u] / 255.0;
            if (!sixel_cms_builtin_apply_src_to_xyz_intent(
                    xyz_d50,
                    src_unit,
                    3u,
                    src_profile,
                    transform) ||
                !sixel_cms_builtin_apply_xyz_to_dst_intent(
                    dst_unit,
                    3u,
                    xyz_d50,
                    dst_profile,
                    transform)) {
                return 0;
            }
            dst_f32[i * 3u + 0u] = (float)sixel_cms_clamp_unit(dst_unit[0]);
            dst_f32[i * 3u + 1u] = (float)sixel_cms_clamp_unit(dst_unit[1]);
            dst_f32[i * 3u + 2u] = (float)sixel_cms_clamp_unit(dst_unit[2]);
        }
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_RGB_F32 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32 &&
        dst_profile->builtin_profile.kind == SIXEL_ICC_PROFILE_KIND_RGB) {
        for (i = 0u; i < pixel_count; ++i) {
            src_unit[0] = sixel_cms_clamp_unit((double)src_f32[i * 3u + 0u]);
            src_unit[1] = sixel_cms_clamp_unit((double)src_f32[i * 3u + 1u]);
            src_unit[2] = sixel_cms_clamp_unit((double)src_f32[i * 3u + 2u]);
            if (!sixel_cms_builtin_apply_src_to_xyz_intent(
                    xyz_d50,
                    src_unit,
                    3u,
                    src_profile,
                    transform) ||
                !sixel_cms_builtin_apply_xyz_to_dst_intent(
                    dst_unit,
                    3u,
                    xyz_d50,
                    dst_profile,
                    transform)) {
                return 0;
            }
            dst_f32[i * 3u + 0u] = (float)sixel_cms_clamp_unit(dst_unit[0]);
            dst_f32[i * 3u + 1u] = (float)sixel_cms_clamp_unit(dst_unit[1]);
            dst_f32[i * 3u + 2u] = (float)sixel_cms_clamp_unit(dst_unit[2]);
        }
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_RGBA_8 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGBA_8 &&
        dst_profile->builtin_profile.kind == SIXEL_ICC_PROFILE_KIND_RGB) {
        for (i = 0u; i < pixel_count; ++i) {
            src_unit[0] = (double)src_u8[i * 4u + 0u] / 255.0;
            src_unit[1] = (double)src_u8[i * 4u + 1u] / 255.0;
            src_unit[2] = (double)src_u8[i * 4u + 2u] / 255.0;
            alpha = src_u8[i * 4u + 3u];
            if (!sixel_cms_builtin_apply_src_to_xyz_intent(
                    xyz_d50,
                    src_unit,
                    3u,
                    src_profile,
                    transform) ||
                !sixel_cms_builtin_apply_xyz_to_dst_intent(
                    dst_unit,
                    3u,
                    xyz_d50,
                    dst_profile,
                    transform)) {
                return 0;
            }
            dst_u8[i * 4u + 0u] =
                (unsigned char)(sixel_cms_clamp_unit(dst_unit[0]) * 255.0
                                + 0.5);
            dst_u8[i * 4u + 1u] =
                (unsigned char)(sixel_cms_clamp_unit(dst_unit[1]) * 255.0
                                + 0.5);
            dst_u8[i * 4u + 2u] =
                (unsigned char)(sixel_cms_clamp_unit(dst_unit[2]) * 255.0
                                + 0.5);
            dst_u8[i * 4u + 3u] = alpha;
        }
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_GRAY_8 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_GRAY_8 &&
        dst_profile->builtin_profile.kind == SIXEL_ICC_PROFILE_KIND_GRAY) {
        for (i = 0u; i < pixel_count; ++i) {
            src_unit[0] = (double)src_u8[i] / 255.0;
            if (!sixel_cms_builtin_apply_src_to_xyz_intent(
                    xyz_d50,
                    src_unit,
                    1u,
                    src_profile,
                    transform) ||
                !sixel_cms_builtin_apply_xyz_to_dst_intent(
                    dst_unit,
                    1u,
                    xyz_d50,
                    dst_profile,
                    transform)) {
                return 0;
            }
            dst_u8[i] = (unsigned char)(sixel_cms_clamp_unit(dst_unit[0])
                                        * 255.0 + 0.5);
        }
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_GRAY_8 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_8 &&
        dst_profile->builtin_profile.kind == SIXEL_ICC_PROFILE_KIND_RGB) {
        for (i = 0u; i < pixel_count; ++i) {
            src_unit[0] = (double)src_u8[i] / 255.0;
            if (!sixel_cms_builtin_apply_src_to_xyz_intent(
                    xyz_d50,
                    src_unit,
                    1u,
                    src_profile,
                    transform) ||
                !sixel_cms_builtin_apply_xyz_to_dst_intent(
                    dst_unit,
                    3u,
                    xyz_d50,
                    dst_profile,
                    transform)) {
                return 0;
            }
            dst_u8[i * 3u + 0u] =
                (unsigned char)(sixel_cms_clamp_unit(dst_unit[0]) * 255.0
                                + 0.5);
            dst_u8[i * 3u + 1u] =
                (unsigned char)(sixel_cms_clamp_unit(dst_unit[1]) * 255.0
                                + 0.5);
            dst_u8[i * 3u + 2u] =
                (unsigned char)(sixel_cms_clamp_unit(dst_unit[2]) * 255.0
                                + 0.5);
        }
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_CMYK_8 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32 &&
        dst_profile->builtin_profile.kind == SIXEL_ICC_PROFILE_KIND_RGB) {
        for (i = 0u; i < pixel_count; ++i) {
            src_unit[0] = (double)src_u8[i * 4u + 0u] / 255.0;
            src_unit[1] = (double)src_u8[i * 4u + 1u] / 255.0;
            src_unit[2] = (double)src_u8[i * 4u + 2u] / 255.0;
            src_unit[3] = (double)src_u8[i * 4u + 3u] / 255.0;
            if (!sixel_cms_builtin_apply_src_to_xyz_intent(
                    xyz_d50,
                    src_unit,
                    4u,
                    src_profile,
                    transform) ||
                !sixel_cms_builtin_apply_xyz_to_dst_intent(
                    dst_unit,
                    3u,
                    xyz_d50,
                    dst_profile,
                    transform)) {
                return 0;
            }
            dst_f32[i * 3u + 0u] = (float)sixel_cms_clamp_unit(dst_unit[0]);
            dst_f32[i * 3u + 1u] = (float)sixel_cms_clamp_unit(dst_unit[1]);
            dst_f32[i * 3u + 2u] = (float)sixel_cms_clamp_unit(dst_unit[2]);
        }
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_CMYK_16 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32 &&
        dst_profile->builtin_profile.kind == SIXEL_ICC_PROFILE_KIND_RGB) {
        src_u16 = (uint16_t const *)src;
        for (i = 0u; i < pixel_count; ++i) {
            src_unit[0] = (double)src_u16[i * 4u + 0u] / 65535.0;
            src_unit[1] = (double)src_u16[i * 4u + 1u] / 65535.0;
            src_unit[2] = (double)src_u16[i * 4u + 2u] / 65535.0;
            src_unit[3] = (double)src_u16[i * 4u + 3u] / 65535.0;
            if (!sixel_cms_builtin_apply_src_to_xyz_intent(
                    xyz_d50,
                    src_unit,
                    4u,
                    src_profile,
                    transform) ||
                !sixel_cms_builtin_apply_xyz_to_dst_intent(
                    dst_unit,
                    3u,
                    xyz_d50,
                    dst_profile,
                    transform)) {
                return 0;
            }
            dst_f32[i * 3u + 0u] = (float)sixel_cms_clamp_unit(dst_unit[0]);
            dst_f32[i * 3u + 1u] = (float)sixel_cms_clamp_unit(dst_unit[1]);
            dst_f32[i * 3u + 2u] = (float)sixel_cms_clamp_unit(dst_unit[2]);
        }
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_CMYK_F32 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32 &&
        dst_profile->builtin_profile.kind == SIXEL_ICC_PROFILE_KIND_RGB) {
        for (i = 0u; i < pixel_count; ++i) {
            src_unit[0] = sixel_cms_clamp_unit((double)src_f32[i * 4u + 0u]);
            src_unit[1] = sixel_cms_clamp_unit((double)src_f32[i * 4u + 1u]);
            src_unit[2] = sixel_cms_clamp_unit((double)src_f32[i * 4u + 2u]);
            src_unit[3] = sixel_cms_clamp_unit((double)src_f32[i * 4u + 3u]);
            if (!sixel_cms_builtin_apply_src_to_xyz_intent(
                    xyz_d50,
                    src_unit,
                    4u,
                    src_profile,
                    transform) ||
                !sixel_cms_builtin_apply_xyz_to_dst_intent(
                    dst_unit,
                    3u,
                    xyz_d50,
                    dst_profile,
                    transform)) {
                return 0;
            }
            dst_f32[i * 3u + 0u] = (float)sixel_cms_clamp_unit(dst_unit[0]);
            dst_f32[i * 3u + 1u] = (float)sixel_cms_clamp_unit(dst_unit[1]);
            dst_f32[i * 3u + 2u] = (float)sixel_cms_clamp_unit(dst_unit[2]);
        }
        return 1;
    }

    return 0;
}

static int
sixel_cms_do_transform_builtin_to_srgb(
    sixel_cms_transform_t const *transform,
    void const *src,
    void *dst,
    size_t pixel_count)
{
    sixel_cms_profile_t const *src_profile;
    sixel_cms_profile_t const *dst_profile;
    unsigned char *dst_u8;
    unsigned char const *src_u8;
    float *dst_f32;
    size_t i;
    unsigned char *tmp_rgb;
    unsigned char *tmp_gray;

    src_profile = NULL;
    dst_profile = NULL;
    dst_u8 = NULL;
    src_u8 = NULL;
    dst_f32 = NULL;
    i = 0u;
    tmp_rgb = NULL;
    tmp_gray = NULL;

    if (transform == NULL || src == NULL || dst == NULL || pixel_count == 0u) {
        return 0;
    }

    src_profile = transform->src_profile;
    dst_profile = transform->dst_profile;
    if (src_profile == NULL || dst_profile == NULL) {
        return 0;
    }
    if (!sixel_cms_builtin_profile_can_convert(src_profile) ||
        !sixel_cms_builtin_profile_can_convert(dst_profile)) {
        return 0;
    }
    if (!sixel_cms_builtin_profile_is_srgb(dst_profile)) {
        return 0;
    }

    src_u8 = (unsigned char const *)src;
    dst_u8 = (unsigned char *)dst;

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_RGB_8 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_8) {
        if (src != dst) {
            if (pixel_count > SIZE_MAX / 3u) {
                return 0;
            }
            memcpy(dst, src, pixel_count * 3u);
        }
        if (!sixel_cms_builtin_profile_is_srgb(src_profile)) {
            return sixel_cms_builtin_apply_rgb_u8_intent(
                dst_u8,
                pixel_count,
                &src_profile->builtin_profile,
                transform);
        }
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_RGB_F32 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32) {
        if (src != dst) {
            if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
                return 0;
            }
            memcpy(dst, src, pixel_count * 3u * sizeof(float));
        }
        if (!sixel_cms_builtin_profile_is_srgb(src_profile)) {
            return sixel_cms_builtin_apply_rgb_f32_intent(
                (float *)dst,
                pixel_count,
                &src_profile->builtin_profile,
                transform);
        }
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_GRAY_8 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_GRAY_8) {
        if (src != dst) {
            memcpy(dst, src, pixel_count);
        }
        if (!sixel_cms_builtin_profile_is_srgb(src_profile)) {
            return sixel_cms_builtin_apply_gray_u8_intent(
                (unsigned char *)dst,
                pixel_count,
                &src_profile->builtin_profile,
                transform);
        }
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_GRAY_8 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_8) {
        tmp_gray = (unsigned char *)malloc(pixel_count);
        if (tmp_gray == NULL) {
            return 0;
        }
        memcpy(tmp_gray, src, pixel_count);
        if (!sixel_cms_builtin_profile_is_srgb(src_profile) &&
            !sixel_cms_builtin_apply_gray_u8_intent(
                tmp_gray,
                pixel_count,
                &src_profile->builtin_profile,
                transform)) {
            free(tmp_gray);
            return 0;
        }
        for (i = 0u; i < pixel_count; ++i) {
            dst_u8[i * 3u + 0u] = tmp_gray[i];
            dst_u8[i * 3u + 1u] = tmp_gray[i];
            dst_u8[i * 3u + 2u] = tmp_gray[i];
        }
        free(tmp_gray);
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_RGBA_8 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGBA_8) {
        if (pixel_count > SIZE_MAX / 3u) {
            return 0;
        }
        tmp_rgb = (unsigned char *)malloc(pixel_count * 3u);
        if (tmp_rgb == NULL) {
            return 0;
        }
        for (i = 0u; i < pixel_count; ++i) {
            tmp_rgb[i * 3u + 0u] = src_u8[i * 4u + 0u];
            tmp_rgb[i * 3u + 1u] = src_u8[i * 4u + 1u];
            tmp_rgb[i * 3u + 2u] = src_u8[i * 4u + 2u];
        }
        if (!sixel_cms_builtin_profile_is_srgb(src_profile) &&
            !sixel_cms_builtin_apply_rgb_u8_intent(
                tmp_rgb,
                pixel_count,
                &src_profile->builtin_profile,
                transform)) {
            free(tmp_rgb);
            return 0;
        }
        for (i = 0u; i < pixel_count; ++i) {
            dst_u8[i * 4u + 0u] = tmp_rgb[i * 3u + 0u];
            dst_u8[i * 4u + 1u] = tmp_rgb[i * 3u + 1u];
            dst_u8[i * 4u + 2u] = tmp_rgb[i * 3u + 2u];
            dst_u8[i * 4u + 3u] = src_u8[i * 4u + 3u];
        }
        free(tmp_rgb);
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_RGB_8 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32) {
        if (pixel_count > SIZE_MAX / 3u) {
            return 0;
        }
        tmp_rgb = (unsigned char *)malloc(pixel_count * 3u);
        if (tmp_rgb == NULL) {
            return 0;
        }
        memcpy(tmp_rgb, src, pixel_count * 3u);
        if (!sixel_cms_builtin_profile_is_srgb(src_profile) &&
            !sixel_cms_builtin_apply_rgb_u8_intent(
                tmp_rgb,
                pixel_count,
                &src_profile->builtin_profile,
                transform)) {
            free(tmp_rgb);
            return 0;
        }
        dst_f32 = (float *)dst;
        if (!sixel_cms_builtin_convert_rgb8_to_rgbf32(dst_f32,
                                                      tmp_rgb,
                                                      pixel_count)) {
            free(tmp_rgb);
            return 0;
        }
        free(tmp_rgb);
        return 1;
    }

    if (!sixel_cms_builtin_profile_is_srgb(src_profile) &&
        src_profile->builtin_profile_valid &&
        transform->src_format == SIXEL_CMS_PIXELFORMAT_CMYK_8 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32) {
        return sixel_cms_builtin_apply_cmyk_u8_intent(
            (float *)dst,
            (unsigned char const *)src,
            pixel_count,
            &src_profile->builtin_profile,
            transform);
    }

    if (!sixel_cms_builtin_profile_is_srgb(src_profile) &&
        src_profile->builtin_profile_valid &&
        transform->src_format == SIXEL_CMS_PIXELFORMAT_CMYK_16 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32) {
        return sixel_cms_builtin_apply_cmyk_u16_intent(
            (float *)dst,
            (uint16_t const *)src,
            pixel_count,
            &src_profile->builtin_profile,
            transform);
    }

    if (!sixel_cms_builtin_profile_is_srgb(src_profile) &&
        src_profile->builtin_profile_valid &&
        transform->src_format == SIXEL_CMS_PIXELFORMAT_CMYK_F32 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32) {
        return sixel_cms_builtin_apply_cmyk_f32_intent(
            (float *)dst,
            (float const *)src,
            pixel_count,
            &src_profile->builtin_profile,
            transform);
    }

    return 0;
}

static int
sixel_cms_do_transform_builtin(sixel_cms_transform_t const *transform,
                               void const *src,
                               void *dst,
                               size_t pixel_count)
{
    sixel_cms_profile_t const *dst_profile;

    dst_profile = NULL;
    if (transform == NULL || src == NULL || dst == NULL || pixel_count == 0u) {
        return 0;
    }

    dst_profile = transform->dst_profile;
    if (dst_profile == NULL) {
        return 0;
    }
    if (sixel_cms_builtin_profile_is_srgb(dst_profile)) {
        return sixel_cms_do_transform_builtin_to_srgb(transform,
                                                      src,
                                                      dst,
                                                      pixel_count);
    }
    return sixel_cms_do_transform_builtin_device_to_device(transform,
                                                           src,
                                                           dst,
                                                           pixel_count);
}

int
sixel_cms_do_transform(sixel_cms_transform_t const *transform,
                       void const *src,
                       void *dst,
                       size_t pixel_count)
{
    if (transform == NULL || src == NULL || dst == NULL || pixel_count == 0u) {
        return 0;
    }

#if HAVE_LCMS2
    if (transform->backend == SIXEL_CMS_PROFILE_BACKEND_LCMS2) {
        if (transform->lcms_handle == NULL) {
            return 0;
        }
        cmsDoTransform(transform->lcms_handle,
                       src,
                       dst,
                       (cmsUInt32Number)pixel_count);
        return 1;
    }
#endif

#if SIXEL_CMS_HAVE_COLORSYNC
    if (transform->backend == SIXEL_CMS_PROFILE_BACKEND_COLORSYNC) {
        size_t src_rowbytes;
        size_t dst_rowbytes;
        int copy_alpha;
        unsigned char *alpha_copy;
        unsigned char const *src_u8;
        unsigned char *dst_u8;
        size_t i;
        int converted;

        if (transform->colorsync_handle == NULL ||
            transform->src_bytes_per_pixel == 0u ||
            transform->dst_bytes_per_pixel == 0u) {
            return 0;
        }
        if (pixel_count > SIZE_MAX / transform->src_bytes_per_pixel ||
            pixel_count > SIZE_MAX / transform->dst_bytes_per_pixel) {
            return 0;
        }

        src_rowbytes = pixel_count * transform->src_bytes_per_pixel;
        dst_rowbytes = pixel_count * transform->dst_bytes_per_pixel;

        copy_alpha = ((transform->flags & SIXEL_CMS_TRANSFORM_COPY_ALPHA) != 0 &&
                      transform->src_format == SIXEL_CMS_PIXELFORMAT_RGBA_8 &&
                      transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGBA_8);
        alpha_copy = NULL;
        src_u8 = (unsigned char const *)src;
        dst_u8 = (unsigned char *)dst;
        i = 0u;
        if (copy_alpha && src == dst) {
            alpha_copy = (unsigned char *)malloc(pixel_count);
            if (alpha_copy == NULL) {
                return 0;
            }
            for (i = 0u; i < pixel_count; ++i) {
                alpha_copy[i] = src_u8[i * 4u + 3u];
            }
        }

        converted = ColorSyncTransformConvert(transform->colorsync_handle,
                                              pixel_count,
                                              1u,
                                              dst,
                                              transform->dst_depth,
                                              transform->dst_layout,
                                              dst_rowbytes,
                                              src,
                                              transform->src_depth,
                                              transform->src_layout,
                                              src_rowbytes,
                                              NULL) ? 1 : 0;
        if (!converted) {
            free(alpha_copy);
            return 0;
        }

        if (copy_alpha) {
            if (src == dst) {
                for (i = 0u; i < pixel_count; ++i) {
                    dst_u8[i * 4u + 3u] = alpha_copy[i];
                }
            } else {
                for (i = 0u; i < pixel_count; ++i) {
                    dst_u8[i * 4u + 3u] = src_u8[i * 4u + 3u];
                }
            }
        }
        free(alpha_copy);
        return 1;
    }
#endif

    if (transform->backend == SIXEL_CMS_PROFILE_BACKEND_BUILTIN) {
        return sixel_cms_do_transform_builtin(transform,
                                              src,
                                              dst,
                                              pixel_count);
    }

    return 0;
}

void
sixel_cms_delete_transform(sixel_cms_transform_t *transform)
{
    if (transform == NULL) {
        return;
    }

#if HAVE_LCMS2
    if (transform->lcms_handle != NULL) {
        cmsDeleteTransform(transform->lcms_handle);
        transform->lcms_handle = NULL;
    }
#endif
#if SIXEL_CMS_HAVE_COLORSYNC
    if (transform->colorsync_handle != NULL) {
        CFRelease(transform->colorsync_handle);
        transform->colorsync_handle = NULL;
    }
#endif

    free(transform);
}

SIXELAPI int
sixel_cms_convert_profile_to_srgb(unsigned char *pixels,
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
    if (normalized_pixelformat != SIXEL_PIXELFORMAT_RGB888 &&
        normalized_pixelformat != SIXEL_PIXELFORMAT_G8 &&
        normalized_pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32) {
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
    if (normalized_pixelformat == SIXEL_PIXELFORMAT_G8 &&
        src_colorspace != SIXEL_CMS_COLORSPACE_GRAY) {
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

int
sixel_cms_convert_to_srgb_with_profile_bytes(
    unsigned char *pixels,
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
    if (pixels == NULL || width <= 0 || height <= 0 ||
        profile == NULL || profile_length == 0u) {
        return 0;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_length);
    if (src_profile == NULL) {
        return 0;
    }

    converted = sixel_cms_convert_profile_to_srgb(pixels,
                                                  width,
                                                  height,
                                                  pixelformat,
                                                  src_profile);
    sixel_cms_close_profile(src_profile);

    return converted;
}

int
sixel_cms_convert_rgbf32_gamma_to_linear(float *pixels, size_t pixel_count)
{
    SIXELSTATUS status;
    size_t float_bytes;

    status = SIXEL_FALSE;
    float_bytes = 0u;
    if (pixels == NULL || pixel_count == 0u) {
        return 0;
    }
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return 0;
    }
    float_bytes = pixel_count * 3u * sizeof(float);
    status = sixel_helper_convert_colorspace((unsigned char *)pixels,
                                             float_bytes,
                                             SIXEL_PIXELFORMAT_RGBFLOAT32,
                                             SIXEL_COLORSPACE_GAMMA,
                                             SIXEL_COLORSPACE_LINEAR);
    return SIXEL_SUCCEEDED(status);
}

SIXELAPI int
sixel_cms_convert_profile_to_linearrgb(unsigned char *pixels,
                                       int width,
                                       int height,
                                       int pixelformat,
                                       sixel_cms_profile_t *src_profile)
{
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;
    size_t pixel_count;
    int normalized_pixelformat;
    int converted;

    dst_profile = NULL;
    transform = NULL;
    pixel_count = 0u;
    normalized_pixelformat = pixelformat;
    converted = 0;
    if (normalized_pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        normalized_pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    }
    if (pixels == NULL || width <= 0 || height <= 0 || src_profile == NULL) {
        return 0;
    }
    if (normalized_pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32) {
        return 0;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return 0;
    }
    pixel_count = (size_t)width * (size_t)height;

    dst_profile = sixel_cms_create_linear_srgb_profile();
    if (dst_profile != NULL) {
        transform = sixel_cms_create_transform(src_profile,
                                               SIXEL_CMS_PIXELFORMAT_RGB_F32,
                                               dst_profile,
                                               SIXEL_CMS_PIXELFORMAT_RGB_F32,
                                               SIXEL_CMS_TRANSFORM_DEFAULT);
        if (transform != NULL &&
            sixel_cms_do_transform(transform, pixels, pixels, pixel_count)) {
            converted = 1;
        }
    }

    if (transform != NULL) {
        sixel_cms_delete_transform(transform);
    }
    if (dst_profile != NULL) {
        sixel_cms_close_profile(dst_profile);
    }
    if (converted) {
        return 1;
    }

    if (!sixel_cms_convert_profile_to_srgb((unsigned char *)pixels,
                                           width,
                                           height,
                                           SIXEL_PIXELFORMAT_RGBFLOAT32,
                                           src_profile)) {
        return 0;
    }
    return sixel_cms_convert_rgbf32_gamma_to_linear((float *)pixels,
                                                    pixel_count);
}

int
sixel_cms_convert_to_linearrgb_with_profile_bytes(
    unsigned char *pixels,
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
    if (pixels == NULL || width <= 0 || height <= 0 ||
        profile == NULL || profile_length == 0u) {
        return 0;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_length);
    if (src_profile == NULL) {
        return 0;
    }

    converted = sixel_cms_convert_profile_to_linearrgb(pixels,
                                                       width,
                                                       height,
                                                       pixelformat,
                                                       src_profile);
    sixel_cms_close_profile(src_profile);

    return converted;
}

SIXELAPI int
sixel_cms_convert_profile_to_cielab(unsigned char *pixels,
                                    int width,
                                    int height,
                                    int pixelformat,
                                    sixel_cms_profile_t *src_profile)
{
    sixel_cms_profile_t *dst_profile;
    sixel_cms_transform_t *transform;
    size_t pixel_count;
    int converted;

    dst_profile = NULL;
    transform = NULL;
    pixel_count = 0u;
    converted = 0;
    if (pixels == NULL || width <= 0 || height <= 0 || src_profile == NULL) {
        return 0;
    }
    if (pixelformat != SIXEL_PIXELFORMAT_CIELABFLOAT32) {
        return 0;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return 0;
    }
    pixel_count = (size_t)width * (size_t)height;

    dst_profile = sixel_cms_create_cielab_d50_profile();
    if (dst_profile != NULL) {
        transform = sixel_cms_create_transform(src_profile,
                                               SIXEL_CMS_PIXELFORMAT_LAB_F32,
                                               dst_profile,
                                               SIXEL_CMS_PIXELFORMAT_LAB_F32,
                                               SIXEL_CMS_TRANSFORM_DEFAULT);
        if (transform != NULL &&
            sixel_cms_do_transform(transform, pixels, pixels, pixel_count)) {
            converted = 1;
        }
    }
    if (transform != NULL) {
        sixel_cms_delete_transform(transform);
    }
    if (dst_profile != NULL) {
        sixel_cms_close_profile(dst_profile);
    }

    return converted;
}

int
sixel_cms_convert_to_cielab_with_profile_bytes(
    unsigned char *pixels,
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
    if (pixels == NULL || width <= 0 || height <= 0 ||
        profile == NULL || profile_length == 0u) {
        return 0;
    }

    src_profile = sixel_cms_open_profile_from_mem(profile, profile_length);
    if (src_profile == NULL) {
        return 0;
    }

    converted = sixel_cms_convert_profile_to_cielab(pixels,
                                                    width,
                                                    height,
                                                    pixelformat,
                                                    src_profile);
    sixel_cms_close_profile(src_profile);

    return converted;
}
