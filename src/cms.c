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

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include "cms.h"
#include "colorspace.h"

#include <ctype.h>
#include <float.h>
#include <stdio.h>
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
#include "loader-common.h"
#include "options.h"
#include "pthread-once.h"
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
    int is_cielab_d50_profile;
    int builtin_profile_valid;
    sixel_icc_profile_t builtin_profile;
    /* Analytic RGB endpoints retain extended values in float transforms. */
    sixel_color_def_t *rgb_definition;
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

#if defined(__PCC__) || defined(__TINYC__)
# define SIXEL_CMS_NO_TLS_COMPILER 1
#else
# define SIXEL_CMS_NO_TLS_COMPILER 0
#endif

#if defined(_MSC_VER)
# if defined(_MT)
#  define SIXEL_CMS_TLS __declspec(thread)
#  define SIXEL_CMS_TLS_AVAILABLE 1
# else
#  define SIXEL_CMS_TLS
#  define SIXEL_CMS_TLS_AVAILABLE 0
# endif
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) \
    && !SIXEL_CMS_NO_TLS_COMPILER
# define SIXEL_CMS_TLS _Thread_local
# define SIXEL_CMS_TLS_AVAILABLE 1
#elif (defined(__GNUC__) || defined(__clang__)) \
    && !SIXEL_CMS_NO_TLS_COMPILER
# define SIXEL_CMS_TLS __thread
# define SIXEL_CMS_TLS_AVAILABLE 1
#else
# define SIXEL_CMS_TLS
# define SIXEL_CMS_TLS_AVAILABLE 0
#endif

static SIXEL_CMS_TLS sixel_cms_engine_t g_sixel_cms_engine
    = SIXEL_CMS_ENGINE_AUTO;

#if SIXEL_ENABLE_THREADS && !SIXEL_CMS_TLS_AVAILABLE
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
#  if !defined(UNICODE)
#   define UNICODE
#  endif
#  if !defined(_UNICODE)
#   define _UNICODE
#  endif
#  if !defined(WIN32_LEAN_AND_MEAN)
#   define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
static CRITICAL_SECTION g_sixel_cms_engine_mutex;
static INIT_ONCE g_sixel_cms_engine_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
sixel_cms_engine_lock_init_once(PINIT_ONCE once,
                                PVOID parameter,
                                PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;

    InitializeCriticalSection(&g_sixel_cms_engine_mutex);
    return TRUE;
}
# else
#  include <pthread.h>
/*
 * Use runtime initialization so Cosmopolitan/pcc builds avoid warnings from
 * partial pthread struct initializers.
 */
static pthread_mutex_t g_sixel_cms_engine_mutex;
SIXEL_PTHREAD_ONCE_DECLARE(g_sixel_cms_engine_mutex_once);
static int g_sixel_cms_engine_mutex_ready = 0;

static void
sixel_cms_engine_lock_init_once(void)
{
    int rc;

    rc = pthread_mutex_init(&g_sixel_cms_engine_mutex, NULL);
    if (rc == 0) {
        g_sixel_cms_engine_mutex_ready = 1;
    }
}
# endif

/*
 * pcc builds without language-level TLS fall back to a process-global engine
 * flag. Guard reads and writes so concurrent loader threads do not race.
 */
static void
sixel_cms_engine_lock(void)
{
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    BOOL initialized;

    initialized = InitOnceExecuteOnce(&g_sixel_cms_engine_once,
                                      sixel_cms_engine_lock_init_once,
                                      NULL,
                                      NULL);
    if (!initialized) {
        abort();
    }
    EnterCriticalSection(&g_sixel_cms_engine_mutex);
# else
    int rc;
    int once_status;

    once_status = pthread_once(&g_sixel_cms_engine_mutex_once,
                               sixel_cms_engine_lock_init_once);
    if (once_status != 0 || !g_sixel_cms_engine_mutex_ready) {
        abort();
    }

    rc = pthread_mutex_lock(&g_sixel_cms_engine_mutex);
    if (rc != 0) {
        abort();
    }
# endif
}

static void
sixel_cms_engine_unlock(void)
{
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
    && !defined(WITH_WINPTHREAD)
    LeaveCriticalSection(&g_sixel_cms_engine_mutex);
# else
    int rc;

    if (!g_sixel_cms_engine_mutex_ready) {
        abort();
    }
    rc = pthread_mutex_unlock(&g_sixel_cms_engine_mutex);
    if (rc != 0) {
        abort();
    }
# endif
}
#else
static void
sixel_cms_engine_lock(void)
{
}

static void
sixel_cms_engine_unlock(void)
{
}
#endif

#undef SIXEL_CMS_TLS
#undef SIXEL_CMS_TLS_AVAILABLE
#undef SIXEL_CMS_NO_TLS_COMPILER

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
    sixel_cms_engine_t selected;

    selected = SIXEL_CMS_ENGINE_AUTO;
    switch (engine) {
    case SIXEL_CMS_ENGINE_NONE:
    case SIXEL_CMS_ENGINE_AUTO:
    case SIXEL_CMS_ENGINE_BUILTIN:
    case SIXEL_CMS_ENGINE_LCMS2:
    case SIXEL_CMS_ENGINE_COLORSYNC:
        selected = engine;
        break;
    default:
        selected = SIXEL_CMS_ENGINE_AUTO;
        break;
    }

    sixel_cms_engine_lock();
    g_sixel_cms_engine = selected;
    sixel_cms_engine_unlock();
}

sixel_cms_engine_t
sixel_cms_get_engine(void)
{
    sixel_cms_engine_t selected;

    sixel_cms_engine_lock();
    selected = g_sixel_cms_engine;
    sixel_cms_engine_unlock();

    return sixel_cms_resolve_engine(selected);
}

#if HAVE_LCMS2
static cmsUInt32Number
sixel_cms_map_format_lcms(sixel_cms_pixel_format_t format)
{
    switch (format) {
    case SIXEL_CMS_PIXELFORMAT_GRAY_8:
        return TYPE_GRAY_8;
    case SIXEL_CMS_PIXELFORMAT_GRAY_F32:
        return TYPE_GRAY_FLT;
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

static char const *
sixel_cms_intent_name(int intent)
{
    switch (intent) {
    case SIXEL_CMS_INTENT_PERCEPTUAL:
        return "perceptual";
    case SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC:
        return "relative";
    case SIXEL_CMS_INTENT_SATURATION:
        return "saturation";
    case SIXEL_CMS_INTENT_ABSOLUTE_COLORIMETRIC:
        return "absolute";
    default:
        return "unknown";
    }
}

/* Trace the policy after registry decoding, at the actual CMS consumer. */
static void
sixel_cms_trace_intent_order(int const *intents,
                             size_t intent_count,
                             int exclusive)
{
    size_t index;

    index = 0u;
    if (!sixel_trace_topic_is_enabled("loader_contract")) {
        return;
    }
    fprintf(stderr, "LSXCMS1|intent_order=");
    while (index < intent_count) {
        if (index > 0u) {
            fputc('+', stderr);
        }
        fputs(sixel_cms_intent_name(intents[index]), stderr);
        ++index;
    }
    fprintf(stderr, "|exclusive=%d\n", exclusive != 0);
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
    unsigned int encoded;
    int custom[4];
    size_t custom_count;
    size_t i;
    int exclusive;

    encoded = loader_cms_rendering_intent_order();
    custom_count = sixel_option_choice_list_count(encoded);
    exclusive = sixel_option_choice_list_is_exclusive(encoded);
    if (custom_count == 0u || custom_count > 4u) {
        memcpy(intents, defaults, sizeof(defaults));
        sixel_cms_trace_intent_order(intents, 4u, 0);
        return 4u;
    }
    i = 0u;
    while (i < custom_count) {
        if (!sixel_option_choice_list_value_at(encoded, i, custom + i)) {
            memcpy(intents, defaults, sizeof(defaults));
            sixel_cms_trace_intent_order(intents, 4u, 0);
            return 4u;
        }
        ++i;
    }

    if (exclusive) {
        memcpy(intents, custom, custom_count * sizeof(custom[0]));
        sixel_cms_trace_intent_order(intents, custom_count, 1);
        return custom_count;
    }

    memcpy(intents, custom, custom_count * sizeof(custom[0]));
    for (i = 0u; i < 4u; ++i) {
        if (!sixel_cms_intent_contains(intents, custom_count, defaults[i])) {
            intents[custom_count++] = defaults[i];
        }
    }

    sixel_cms_trace_intent_order(intents, custom_count, 0);
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
    profile->is_cielab_d50_profile = 0;
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
        profile->builtin_profile.d2b_lut[i].kind = SIXEL_ICC_LUT_INVALID;
        profile->builtin_profile.d2b_mab[i].type = SIXEL_ICC_MAB_TYPE_INVALID;
        profile->builtin_profile.b2d_lut[i].kind = SIXEL_ICC_LUT_INVALID;
        profile->builtin_profile.b2d_mab[i].type = SIXEL_ICC_MAB_TYPE_INVALID;
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
    double gamma_r,
    double gamma_g,
    double gamma_b,
    double white_x,
    double white_y,
    double red_x,
    double red_y,
    double green_x,
    double green_y,
    double blue_x,
    double blue_y)
{
    cmsToneCurve *curves[3];
    cmsCIExyY white_point;
    cmsCIExyYTRIPLE primaries;
    int index;

    curves[0] = NULL;
    curves[1] = NULL;
    curves[2] = NULL;
    index = 0;
    if (profile == NULL ||
        gamma_r <= 0.0 ||
        gamma_g <= 0.0 ||
        gamma_b <= 0.0) {
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

    curves[0] = cmsBuildGamma(NULL, 1.0 / gamma_r);
    curves[1] = cmsBuildGamma(NULL, 1.0 / gamma_g);
    curves[2] = cmsBuildGamma(NULL, 1.0 / gamma_b);
    if (curves[0] == NULL ||
        curves[1] == NULL ||
        curves[2] == NULL) {
        for (index = 0; index < 3; ++index) {
            if (curves[index] != NULL) {
                cmsFreeToneCurve(curves[index]);
                curves[index] = NULL;
            }
        }
        return 0;
    }

    profile->lcms_handle = cmsCreateRGBProfile(&white_point, &primaries, curves);
    for (index = 0; index < 3; ++index) {
        cmsFreeToneCurve(curves[index]);
        curves[index] = NULL;
    }
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
    profile->is_cielab_d50_profile = 1;
    return 1;
}
#endif

static int
sixel_cms_matrix3_invert_internal(double const m[9], double inv_out[9])
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
sixel_cms_build_rgb_to_xyz_matrix_internal(double white_x,
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

    if (!sixel_cms_matrix3_invert_internal(primaries, inv_primaries)) {
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
    double gamma_r,
    double gamma_g,
    double gamma_b,
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

    if (profile == NULL ||
        gamma_r <= 0.0 ||
        gamma_g <= 0.0 ||
        gamma_b <= 0.0) {
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
    gamma[0] = (CGFloat)(1.0 / gamma_r);
    gamma[1] = (CGFloat)(1.0 / gamma_g);
    gamma[2] = (CGFloat)(1.0 / gamma_b);
    /* Calibrated RGB stores each primary's XYZ column consecutively;
     * the internal matrix uses rows. Passing rows changes the primaries. */
    for (i = 0u; i < 9u; ++i) {
        cg_matrix[i] = (CGFloat)matrix[(i % 3u) * 3u + i / 3u];
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
    profile->is_cielab_d50_profile = 1;
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
    case SIXEL_CMS_PIXELFORMAT_GRAY_F32:
    case SIXEL_CMS_PIXELFORMAT_RGB_F32:
        *depth = kColorSync32BitFloat;
#if WORDS_BIGENDIAN
        value_layout |= kColorSyncByteOrder32Big;
#else
        value_layout |= kColorSyncByteOrder32Little;
#endif
        *bytes_per_pixel = (format == SIXEL_CMS_PIXELFORMAT_GRAY_F32
                            ? 1u : 3u) * sizeof(float);
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
                != SIXEL_ICC_LUT_INVALID ||
            profile->builtin_profile.d2b_mab[slot].type
                != SIXEL_ICC_MAB_TYPE_INVALID ||
            profile->builtin_profile.d2b_lut[slot].kind
                != SIXEL_ICC_LUT_INVALID ||
            profile->builtin_profile.b2d_mab[slot].type
                != SIXEL_ICC_MAB_TYPE_INVALID ||
            profile->builtin_profile.b2d_lut[slot].kind
                != SIXEL_ICC_LUT_INVALID) {
            has_dynamic_lut_path = 1;
        }
        if (slot > 0u &&
            (profile->builtin_profile.a2b_mab[slot].type
                 != SIXEL_ICC_MAB_TYPE_INVALID ||
             profile->builtin_profile.a2b_lut[slot].kind
                 != SIXEL_ICC_LUT_INVALID ||
             profile->builtin_profile.d2b_mab[slot].type
                 != SIXEL_ICC_MAB_TYPE_INVALID ||
             profile->builtin_profile.d2b_lut[slot].kind
                 != SIXEL_ICC_LUT_INVALID)) {
            has_secondary_a2b_slot = 1;
        }
        if (profile->builtin_profile.b2a_mab[slot].type
                != SIXEL_ICC_MAB_TYPE_INVALID ||
            profile->builtin_profile.b2a_lut[slot].kind
                != SIXEL_ICC_LUT_INVALID ||
            profile->builtin_profile.b2d_mab[slot].type
                != SIXEL_ICC_MAB_TYPE_INVALID ||
            profile->builtin_profile.b2d_lut[slot].kind
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
    case SIXEL_ICC_PROFILE_KIND_LAB:
        profile->colorspace = SIXEL_CMS_COLORSPACE_LAB;
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

    profile->rgb_definition = sixel_cdef_name("srgb");
    if (profile->rgb_definition == NULL) {
        return 0;
    }
    profile->backend = SIXEL_CMS_PROFILE_BACKEND_BUILTIN;
    profile->colorspace = SIXEL_CMS_COLORSPACE_RGB;
    profile->is_srgb_profile = 1;
    profile->is_cielab_d50_profile = 0;
    return 1;
}

static int
sixel_cms_profile_create_cielab_d50_builtin(sixel_cms_profile_t *profile)
{
    if (profile == NULL) {
        return 0;
    }

    sixel_icc_profile_destroy(&profile->builtin_profile);
    profile->backend = SIXEL_CMS_PROFILE_BACKEND_BUILTIN;
    profile->colorspace = SIXEL_CMS_COLORSPACE_LAB;
    profile->is_srgb_profile = 0;
    profile->is_cielab_d50_profile = 1;
    profile->builtin_profile_valid = 1;
    profile->builtin_profile.kind = SIXEL_ICC_PROFILE_KIND_LAB;
    profile->builtin_profile.pcs = SIXEL_ICC_PROFILE_PCS_LAB;
    return 1;
}

static int
sixel_cms_profile_create_rgb_gamma_chrm_builtin(
    sixel_cms_profile_t *profile,
    double gamma_r,
    double gamma_g,
    double gamma_b,
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
    double gamma_values[3];
    double xy[8];
    size_t i;

    i = 0u;
    memset(matrix, 0, sizeof(matrix));
    memset(gamma_values, 0, sizeof(gamma_values));
    if (profile == NULL ||
        gamma_r <= 0.0 ||
        gamma_g <= 0.0 ||
        gamma_b <= 0.0) {
        return 0;
    }
    if (!sixel_cms_build_rgb_to_xyz_matrix_internal(white_x,
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

    gamma_values[0] = 1.0 / gamma_r;
    gamma_values[1] = 1.0 / gamma_g;
    gamma_values[2] = 1.0 / gamma_b;
    xy[0] = white_x;
    xy[1] = white_y;
    xy[2] = red_x;
    xy[3] = red_y;
    xy[4] = green_x;
    xy[5] = green_y;
    xy[6] = blue_x;
    xy[7] = blue_y;
    profile->rgb_definition = sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_POWER,
                                             gamma_values);
    if (profile->rgb_definition == NULL) {
        return 0;
    }
    sixel_icc_profile_destroy(&profile->builtin_profile);
    profile->backend = SIXEL_CMS_PROFILE_BACKEND_BUILTIN;
    profile->colorspace = SIXEL_CMS_COLORSPACE_RGB;
    profile->is_srgb_profile = 0;
    profile->is_cielab_d50_profile = 0;
    profile->builtin_profile_valid = 1;
    profile->builtin_profile.kind = SIXEL_ICC_PROFILE_KIND_RGB;
    profile->builtin_profile.pcs = SIXEL_ICC_PROFILE_PCS_XYZ;

    for (i = 0u; i < 3u; ++i) {
        profile->builtin_profile.curves[i].kind = SIXEL_ICC_CURVE_GAMMA;
        profile->builtin_profile.curves[i].gamma = gamma_values[i];
    }

    profile->builtin_profile.matrix_to_xyz_d50[0][0] = matrix[0];
    profile->builtin_profile.matrix_to_xyz_d50[0][1] = matrix[1];
    profile->builtin_profile.matrix_to_xyz_d50[0][2] = matrix[2];
    profile->builtin_profile.matrix_to_xyz_d50[1][0] = matrix[3];
    profile->builtin_profile.matrix_to_xyz_d50[1][1] = matrix[4];
    profile->builtin_profile.matrix_to_xyz_d50[1][2] = matrix[5];
    profile->builtin_profile.matrix_to_xyz_d50[2][0] = matrix[6];
    profile->builtin_profile.matrix_to_xyz_d50[2][1] = matrix[7];
    profile->builtin_profile.matrix_to_xyz_d50[2][2] = matrix[8];
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
    if (sixel_cms_profile_create_cielab_d50_builtin(profile)) {
        return profile;
    }
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
    return sixel_cms_create_rgb_profile_from_gammas_chrm(
        file_gamma,
        file_gamma,
        file_gamma,
        white_x,
        white_y,
        red_x,
        red_y,
        green_x,
        green_y,
        blue_x,
        blue_y);
}

SIXELAPI sixel_cms_profile_t *
sixel_cms_create_rgb_profile_from_gammas_chrm(double gamma_r,
                                              double gamma_g,
                                              double gamma_b,
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
                                                      gamma_r,
                                                      gamma_g,
                                                      gamma_b,
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
                                                          gamma_r,
                                                          gamma_g,
                                                          gamma_b,
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
    if (sixel_cms_profile_create_rgb_gamma_chrm_builtin(profile,
                                                        gamma_r,
                                                        gamma_g,
                                                        gamma_b,
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
    sixel_cdef_free(profile->rgb_definition);

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

static double const sixel_cms_xyz_d50_to_d65[3][3] = {
    { 0.955576615033105, -0.023039344716079, 0.063163632249801 },
    { -0.028289544243554, 1.009941617371114, 0.021007654996191 },
    { 0.012298165717208, -0.020483025232449, 1.329909826449758 }
};

static double const sixel_cms_xyz_to_srgb_d65[3][3] = {
    { 3.240969941904521, -1.537383177570093, -0.498610760293003 },
    { -0.969243636280880, 1.875967501507721, 0.041555057407176 },
    { 0.055630079696993, -0.203976958888977, 1.056971514242878 }
};

static double
sixel_cms_clamp_unit(double value)
{
    /* Normalize NaN before comparisons so it cannot poison matrix math. */
    if (value != value) {
        return 0.0;
    }
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

static double
sixel_cms_encode_srgb_unit(double linear_value)
{
    linear_value = sixel_cms_clamp_unit(linear_value);
    if (linear_value <= 0.0031308) {
        return linear_value * 12.92;
    }
    return 1.055 * pow(linear_value, 1.0 / 2.4) - 0.055;
}

static double
sixel_cms_clamp_lab_ab(double value)
{
    /* Lab float input uses the same deterministic NaN policy as RGB. */
    if (value != value) {
        return 0.0;
    }
    if (value < -1.5) {
        return -1.5;
    }
    if (value > 1.5) {
        return 1.5;
    }
    return value;
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

static void
sixel_cms_xyz_d50_to_srgb_unit(double rgb_unit[3], double const xyz_d50[3])
{
    double xyz_d65[3];
    double rgb_linear[3];

    sixel_cms_apply_matrix3(sixel_cms_xyz_d50_to_d65, xyz_d50, xyz_d65);
    sixel_cms_apply_matrix3(sixel_cms_xyz_to_srgb_d65, xyz_d65, rgb_linear);
    rgb_unit[0] = sixel_cms_encode_srgb_unit(rgb_linear[0]);
    rgb_unit[1] = sixel_cms_encode_srgb_unit(rgb_linear[1]);
    rgb_unit[2] = sixel_cms_encode_srgb_unit(rgb_linear[2]);
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
        kind != SIXEL_ICC_PROFILE_KIND_GRAY &&
        kind != SIXEL_ICC_PROFILE_KIND_LAB) {
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
sixel_cms_builtin_kind_channel_count(sixel_icc_profile_kind_t kind,
                                     size_t *out_count)
{
    if (out_count == NULL) {
        return 0;
    }

    if (kind == SIXEL_ICC_PROFILE_KIND_RGB ||
        kind == SIXEL_ICC_PROFILE_KIND_LAB) {
        *out_count = 3u;
        return 1;
    }
    if (kind == SIXEL_ICC_PROFILE_KIND_GRAY) {
        *out_count = 1u;
        return 1;
    }
    if (kind == SIXEL_ICC_PROFILE_KIND_CMYK) {
        *out_count = 4u;
        return 1;
    }

    return 0;
}

static int
sixel_cms_builtin_load_src_unit(double src_unit[4],
                                size_t *input_channel_count,
                                unsigned char *alpha,
                                sixel_cms_pixel_format_t src_format,
                                void const *src,
                                size_t pixel_index)
{
    unsigned char const *src_u8;
    uint16_t const *src_u16;
    float const *src_f32;
    size_t base;

    src_u8 = NULL;
    src_u16 = NULL;
    src_f32 = NULL;
    base = 0u;
    if (src_unit == NULL || input_channel_count == NULL || src == NULL) {
        return 0;
    }

    src_u8 = (unsigned char const *)src;
    src_u16 = (uint16_t const *)src;
    src_f32 = (float const *)src;
    switch (src_format) {
    case SIXEL_CMS_PIXELFORMAT_RGB_8:
        base = pixel_index * 3u;
        src_unit[0] = (double)src_u8[base + 0u] / 255.0;
        src_unit[1] = (double)src_u8[base + 1u] / 255.0;
        src_unit[2] = (double)src_u8[base + 2u] / 255.0;
        *input_channel_count = 3u;
        return 1;
    case SIXEL_CMS_PIXELFORMAT_RGB_F32:
        base = pixel_index * 3u;
        src_unit[0] = sixel_cms_clamp_unit((double)src_f32[base + 0u]);
        src_unit[1] = sixel_cms_clamp_unit((double)src_f32[base + 1u]);
        src_unit[2] = sixel_cms_clamp_unit((double)src_f32[base + 2u]);
        *input_channel_count = 3u;
        return 1;
    case SIXEL_CMS_PIXELFORMAT_RGBA_8:
        base = pixel_index * 4u;
        src_unit[0] = (double)src_u8[base + 0u] / 255.0;
        src_unit[1] = (double)src_u8[base + 1u] / 255.0;
        src_unit[2] = (double)src_u8[base + 2u] / 255.0;
        if (alpha != NULL) {
            *alpha = src_u8[base + 3u];
        }
        *input_channel_count = 3u;
        return 1;
    case SIXEL_CMS_PIXELFORMAT_GRAY_F32:
        src_unit[0] = sixel_cms_clamp_unit(src_f32[pixel_index]);
        *input_channel_count = 1u;
        return 1;
    case SIXEL_CMS_PIXELFORMAT_GRAY_8:
        src_unit[0] = (double)src_u8[pixel_index] / 255.0;
        *input_channel_count = 1u;
        return 1;
    case SIXEL_CMS_PIXELFORMAT_CMYK_8:
        base = pixel_index * 4u;
        src_unit[0] = (double)src_u8[base + 0u] / 255.0;
        src_unit[1] = (double)src_u8[base + 1u] / 255.0;
        src_unit[2] = (double)src_u8[base + 2u] / 255.0;
        src_unit[3] = (double)src_u8[base + 3u] / 255.0;
        *input_channel_count = 4u;
        return 1;
    case SIXEL_CMS_PIXELFORMAT_CMYK_16:
        base = pixel_index * 4u;
        src_unit[0] = (double)src_u16[base + 0u] / 65535.0;
        src_unit[1] = (double)src_u16[base + 1u] / 65535.0;
        src_unit[2] = (double)src_u16[base + 2u] / 65535.0;
        src_unit[3] = (double)src_u16[base + 3u] / 65535.0;
        *input_channel_count = 4u;
        return 1;
    case SIXEL_CMS_PIXELFORMAT_CMYK_F32:
        base = pixel_index * 4u;
        src_unit[0] = sixel_cms_clamp_unit((double)src_f32[base + 0u]);
        src_unit[1] = sixel_cms_clamp_unit((double)src_f32[base + 1u]);
        src_unit[2] = sixel_cms_clamp_unit((double)src_f32[base + 2u]);
        src_unit[3] = sixel_cms_clamp_unit((double)src_f32[base + 3u]);
        *input_channel_count = 4u;
        return 1;
    case SIXEL_CMS_PIXELFORMAT_LAB_F32:
        base = pixel_index * 3u;
        src_unit[0] = sixel_cms_clamp_unit((double)src_f32[base + 0u]);
        src_unit[1] = sixel_cms_clamp_lab_ab((double)src_f32[base + 1u]);
        src_unit[2] = sixel_cms_clamp_lab_ab((double)src_f32[base + 2u]);
        *input_channel_count = 3u;
        return 1;
    default:
        break;
    }

    return 0;
}

static int
sixel_cms_builtin_store_dst_unit(void *dst,
                                 sixel_cms_pixel_format_t dst_format,
                                 size_t pixel_index,
                                 double const dst_unit[4],
                                 unsigned char alpha)
{
    unsigned char *dst_u8;
    uint16_t *dst_u16;
    float *dst_f32;
    size_t base;

    dst_u8 = NULL;
    dst_u16 = NULL;
    dst_f32 = NULL;
    base = 0u;
    if (dst == NULL || dst_unit == NULL) {
        return 0;
    }

    dst_u8 = (unsigned char *)dst;
    dst_u16 = (uint16_t *)dst;
    dst_f32 = (float *)dst;
    switch (dst_format) {
    case SIXEL_CMS_PIXELFORMAT_RGB_8:
        base = pixel_index * 3u;
        dst_u8[base + 0u] =
            (unsigned char)(sixel_cms_clamp_unit(dst_unit[0]) * 255.0 + 0.5);
        dst_u8[base + 1u] =
            (unsigned char)(sixel_cms_clamp_unit(dst_unit[1]) * 255.0 + 0.5);
        dst_u8[base + 2u] =
            (unsigned char)(sixel_cms_clamp_unit(dst_unit[2]) * 255.0 + 0.5);
        return 1;
    case SIXEL_CMS_PIXELFORMAT_RGB_F32:
        base = pixel_index * 3u;
        dst_f32[base + 0u] = (float)sixel_cms_clamp_unit(dst_unit[0]);
        dst_f32[base + 1u] = (float)sixel_cms_clamp_unit(dst_unit[1]);
        dst_f32[base + 2u] = (float)sixel_cms_clamp_unit(dst_unit[2]);
        return 1;
    case SIXEL_CMS_PIXELFORMAT_RGBA_8:
        base = pixel_index * 4u;
        dst_u8[base + 0u] =
            (unsigned char)(sixel_cms_clamp_unit(dst_unit[0]) * 255.0 + 0.5);
        dst_u8[base + 1u] =
            (unsigned char)(sixel_cms_clamp_unit(dst_unit[1]) * 255.0 + 0.5);
        dst_u8[base + 2u] =
            (unsigned char)(sixel_cms_clamp_unit(dst_unit[2]) * 255.0 + 0.5);
        dst_u8[base + 3u] = alpha;
        return 1;
    case SIXEL_CMS_PIXELFORMAT_GRAY_F32:
        dst_f32[pixel_index] = (float)sixel_cms_clamp_unit(dst_unit[0]);
        return 1;
    case SIXEL_CMS_PIXELFORMAT_GRAY_8:
        dst_u8[pixel_index] =
            (unsigned char)(sixel_cms_clamp_unit(dst_unit[0]) * 255.0 + 0.5);
        return 1;
    case SIXEL_CMS_PIXELFORMAT_CMYK_8:
        base = pixel_index * 4u;
        dst_u8[base + 0u] =
            (unsigned char)(sixel_cms_clamp_unit(dst_unit[0]) * 255.0 + 0.5);
        dst_u8[base + 1u] =
            (unsigned char)(sixel_cms_clamp_unit(dst_unit[1]) * 255.0 + 0.5);
        dst_u8[base + 2u] =
            (unsigned char)(sixel_cms_clamp_unit(dst_unit[2]) * 255.0 + 0.5);
        dst_u8[base + 3u] =
            (unsigned char)(sixel_cms_clamp_unit(dst_unit[3]) * 255.0 + 0.5);
        return 1;
    case SIXEL_CMS_PIXELFORMAT_CMYK_16:
        base = pixel_index * 4u;
        dst_u16[base + 0u] = (uint16_t)(
            sixel_cms_clamp_unit(dst_unit[0]) * 65535.0 + 0.5);
        dst_u16[base + 1u] = (uint16_t)(
            sixel_cms_clamp_unit(dst_unit[1]) * 65535.0 + 0.5);
        dst_u16[base + 2u] = (uint16_t)(
            sixel_cms_clamp_unit(dst_unit[2]) * 65535.0 + 0.5);
        dst_u16[base + 3u] = (uint16_t)(
            sixel_cms_clamp_unit(dst_unit[3]) * 65535.0 + 0.5);
        return 1;
    case SIXEL_CMS_PIXELFORMAT_CMYK_F32:
        base = pixel_index * 4u;
        dst_f32[base + 0u] = (float)sixel_cms_clamp_unit(dst_unit[0]);
        dst_f32[base + 1u] = (float)sixel_cms_clamp_unit(dst_unit[1]);
        dst_f32[base + 2u] = (float)sixel_cms_clamp_unit(dst_unit[2]);
        dst_f32[base + 3u] = (float)sixel_cms_clamp_unit(dst_unit[3]);
        return 1;
    case SIXEL_CMS_PIXELFORMAT_LAB_F32:
        base = pixel_index * 3u;
        dst_f32[base + 0u] = (float)sixel_cms_clamp_unit(dst_unit[0]);
        dst_f32[base + 1u] = (float)sixel_cms_clamp_lab_ab(
            (sixel_cms_clamp_unit(dst_unit[1]) * 255.0 - 128.0) / 128.0);
        dst_f32[base + 2u] = (float)sixel_cms_clamp_lab_ab(
            (sixel_cms_clamp_unit(dst_unit[2]) * 255.0 - 128.0) / 128.0);
        return 1;
    default:
        break;
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
    size_t input_channel_count;
    size_t output_channel_count;
    size_t expected_channel_count;
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
    input_channel_count = 0u;
    output_channel_count = 0u;
    expected_channel_count = 0u;
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
    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_LAB_F32 &&
        src_profile->builtin_profile.kind != SIXEL_ICC_PROFILE_KIND_LAB) {
        return 0;
    }
    if (transform->dst_format == SIXEL_CMS_PIXELFORMAT_LAB_F32 &&
        dst_profile->builtin_profile.kind != SIXEL_ICC_PROFILE_KIND_LAB) {
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

    if (!sixel_cms_builtin_kind_channel_count(dst_profile->builtin_profile.kind,
                                              &expected_channel_count)) {
        return 0;
    }

    if (transform->dst_format == SIXEL_CMS_PIXELFORMAT_GRAY_8 ||
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_GRAY_F32) {
        output_channel_count = 1u;
    } else if (transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_8 ||
               transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32 ||
               transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGBA_8 ||
               transform->dst_format == SIXEL_CMS_PIXELFORMAT_LAB_F32) {
        output_channel_count = 3u;
    } else if (transform->dst_format == SIXEL_CMS_PIXELFORMAT_CMYK_8 ||
               transform->dst_format == SIXEL_CMS_PIXELFORMAT_CMYK_16 ||
               transform->dst_format == SIXEL_CMS_PIXELFORMAT_CMYK_F32) {
        output_channel_count = 4u;
    } else {
        return 0;
    }
    if (output_channel_count != expected_channel_count) {
        return 0;
    }

    for (i = 0u; i < pixel_count; ++i) {
        alpha = 255u;
        if (!sixel_cms_builtin_load_src_unit(src_unit,
                                             &input_channel_count,
                                             &alpha,
                                             transform->src_format,
                                             src,
                                             i)) {
            return 0;
        }
        if (transform->src_format == SIXEL_CMS_PIXELFORMAT_LAB_F32 &&
            src_profile->builtin_profile.kind == SIXEL_ICC_PROFILE_KIND_LAB) {
            /*
             * SIXEL Lab float buffers store a/b in [-1, 1].  ICC apply path
             * expects Lab device/PCS units where a/b live in [0, 1].
             */
            src_unit[1] = sixel_cms_clamp_unit(
                (src_unit[1] * 128.0 + 128.0) / 255.0);
            src_unit[2] = sixel_cms_clamp_unit(
                (src_unit[2] * 128.0 + 128.0) / 255.0);
        }
        if (!sixel_cms_builtin_apply_src_to_xyz_intent(xyz_d50,
                                                       src_unit,
                                                       input_channel_count,
                                                       src_profile,
                                                       transform) ||
            !sixel_cms_builtin_apply_xyz_to_dst_intent(dst_unit,
                                                       output_channel_count,
                                                       xyz_d50,
                                                       dst_profile,
                                                       transform) ||
            !sixel_cms_builtin_store_dst_unit(dst,
                                              transform->dst_format,
                                              i,
                                              dst_unit,
                                              alpha)) {
            return 0;
        }
    }

    return 1;
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
    float const *src_lab_f32;
    size_t i;
    unsigned char *tmp_rgb;
    unsigned char *tmp_gray;
    double src_lab[3];
    double src_device_unit[4];
    double xyz_d50[3];
    double dst_unit[3];
    size_t lab_base;

    src_profile = NULL;
    dst_profile = NULL;
    dst_u8 = NULL;
    src_u8 = NULL;
    dst_f32 = NULL;
    src_lab_f32 = NULL;
    i = 0u;
    tmp_rgb = NULL;
    tmp_gray = NULL;
    memset(src_lab, 0, sizeof(src_lab));
    memset(src_device_unit, 0, sizeof(src_device_unit));
    memset(xyz_d50, 0, sizeof(xyz_d50));
    memset(dst_unit, 0, sizeof(dst_unit));
    lab_base = 0u;

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
    src_lab_f32 = (float const *)src;
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
        dst_f32 = (float *)dst;
        for (i = 0u; i < pixel_count * 3u; ++i) {
            dst_f32[i] = (float)sixel_cms_clamp_unit(
                (double)dst_f32[i]);
        }
        if (!sixel_cms_builtin_profile_is_srgb(src_profile)) {
            return sixel_cms_builtin_apply_rgb_f32_intent(
                dst_f32,
                pixel_count,
                &src_profile->builtin_profile,
                transform);
        }
        return 1;
    }

    if (transform->src_format == SIXEL_CMS_PIXELFORMAT_GRAY_F32 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32) {
        dst_f32 = (float *)dst;
        for (i = 0u; i < pixel_count; ++i) {
            src_device_unit[0] = sixel_cms_clamp_unit(src_lab_f32[i]);
            if (!sixel_cms_builtin_apply_src_to_xyz_intent(
                    xyz_d50, src_device_unit, 1u, src_profile, transform)) {
                return 0;
            }
            sixel_cms_xyz_d50_to_srgb_unit(dst_unit, xyz_d50);
            dst_f32[i * 3u] = (float)dst_unit[0];
            dst_f32[i * 3u + 1u] = (float)dst_unit[1];
            dst_f32[i * 3u + 2u] = (float)dst_unit[2];
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

    if (!sixel_cms_builtin_profile_is_srgb(src_profile) &&
        src_profile->builtin_profile_valid &&
        transform->src_format == SIXEL_CMS_PIXELFORMAT_LAB_F32 &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32) {
        if (sixel_cms_get_color_space(src_profile)
                != SIXEL_CMS_COLORSPACE_LAB) {
            return 0;
        }
        dst_f32 = (float *)dst;
        for (i = 0u; i < pixel_count; ++i) {
            lab_base = i * 3u;
            src_lab[0] = sixel_cms_clamp_unit(
                (double)src_lab_f32[lab_base + 0u]);
            src_lab[1] = sixel_cms_clamp_lab_ab(
                (double)src_lab_f32[lab_base + 1u]);
            src_lab[2] = sixel_cms_clamp_lab_ab(
                (double)src_lab_f32[lab_base + 2u]);

            src_device_unit[0] = src_lab[0];
            src_device_unit[1] = sixel_cms_clamp_unit(
                (src_lab[1] * 128.0 + 128.0) / 255.0);
            src_device_unit[2] = sixel_cms_clamp_unit(
                (src_lab[2] * 128.0 + 128.0) / 255.0);
            if (!sixel_cms_builtin_apply_src_to_xyz_intent(
                    xyz_d50,
                    src_device_unit,
                    3u,
                    src_profile,
                    transform)) {
                return 0;
            }

            sixel_cms_xyz_d50_to_srgb_unit(dst_unit, xyz_d50);
            dst_f32[lab_base + 0u] = (float)dst_unit[0];
            dst_f32[lab_base + 1u] = (float)dst_unit[1];
            dst_f32[lab_base + 2u] = (float)dst_unit[2];
        }
        return 1;
    }

    return 0;
}

static double
sixel_cdef_pcs_scale(sixel_icc_lut_t const *lut);

/*
 * Float working RGB is an analytic endpoint, not an ICC device or a byte
 * export. Connect through physical XYZ D50 and preserve extended RGB. Keep
 * ICC device/LUT-domain validation inside the profile evaluator. Legacy
 * byte transforms retain their existing bounded conversion contract.
 */
static int
sixel_cms_float_source_xyz(double xyz[3], double const source[4],
                            size_t channels,
                            sixel_cms_transform_t const *transform)
{
    sixel_cms_profile_t const *profile;
    sixel_icc_profile_t const *icc;
    sixel_icc_lut_t const *lut;
    unsigned int slot;
    size_t i;
    size_t channel;
    double scale;
    double linear[3];

    profile = transform->src_profile;
    if (profile->rgb_definition != NULL) {
        return channels == 3u && sixel_cdef_to_xyz(profile->rgb_definition,
                                                    source, xyz, 1);
    }
    if (!profile->builtin_profile_valid) {
        return 0;
    }
    icc = &profile->builtin_profile;
    for (i = 0u; i < transform->builtin_a2b_slot_count; ++i) {
        slot = transform->builtin_a2b_slots[i];
        if (sixel_icc_apply_device_to_xyz_d50_with_a2b_slot(xyz, source,
                channels, icc, slot, 0)) {
            if (icc->pcs == SIXEL_ICC_PROFILE_PCS_XYZ) {
                /* LUT PCS uses u1.7/u1.15; matrix tags already contain
                 * physical XYZ. A selected LUT never uses matrix fallback. */
                lut = icc->a2b_lut[slot].kind != SIXEL_ICC_LUT_INVALID
                    ? &icc->a2b_lut[slot] : &icc->d2b_lut[slot];
                scale = sixel_cdef_pcs_scale(lut);
                for (channel = 0u; channel < 3u; ++channel) {
                    xyz[channel] *= scale;
                }
            }
            return 1;
        }
    }
    if (icc->kind == SIXEL_ICC_PROFILE_KIND_RGB && channels == 3u) {
        /* Evaluate retained parametric TRCs directly. The legacy helper's
         * sampled 16-bit table discards precision before float storage. */
        for (channel = 0u; channel < 3u; ++channel) {
            if (!sixel_icc_curve_device(&icc->curves[channel],
                                           source[channel], 0,
                                           &linear[channel])) {
                return 0;
            }
        }
        for (channel = 0u; channel < 3u; ++channel) {
            xyz[channel] = icc->matrix_to_xyz_d50[channel][0] * linear[0] +
                icc->matrix_to_xyz_d50[channel][1] * linear[1] +
                icc->matrix_to_xyz_d50[channel][2] * linear[2];
        }
        return 1;
    }
    return sixel_icc_apply_device_to_xyz_d50_with_a2b_slot(xyz, source,
        channels, icc, SIXEL_ICC_A2B_SLOT_COUNT, 1);
}

static int
sixel_cms_float_rgb(sixel_cms_transform_t const *transform,
                      void const *src, float *dst, size_t count)
{
    double source[4];
    double xyz[3];
    double rgb[3];
    float const *input;
    size_t channels;
    size_t pixel;
    size_t step;
    size_t channel;
    size_t base;

    input = src;
    if (count > SIZE_MAX / (4u * sizeof(float))) {
        return 0;
    }
    for (step = 0u; step < count; ++step) {
        /* Expanding an aliased byte/gray buffer must read its tail first.
         * CMYK float shrinks and therefore keeps forward traversal. */
        pixel = src == (void const *)dst &&
            transform->src_format != SIXEL_CMS_PIXELFORMAT_CMYK_F32
            ? count - step - 1u : step;
        base = pixel * 3u;
        if (transform->src_format == SIXEL_CMS_PIXELFORMAT_RGB_F32) {
            channels = 3u;
            for (channel = 0u; channel < 3u; ++channel) {
                source[channel] = input[base + channel];
                /* Preserve the facade's NaN/Inf normalization separately
                 * from finite RGB, whose extended range is intentional. */
                if (!isfinite(source[channel])) {
                    source[channel] = sixel_cms_clamp_unit(source[channel]);
                }
            }
        } else if (!sixel_cms_builtin_load_src_unit(source, &channels, NULL,
                       transform->src_format, src, pixel)) {
            return 0;
        }
        if (!sixel_cms_float_source_xyz(xyz, source, channels, transform) ||
            !sixel_cdef_from_xyz(transform->dst_profile->rgb_definition,
                                   xyz, rgb, 1)) {
            return 0;
        }
        for (channel = 0u; channel < 3u; ++channel) {
            if (!isfinite(rgb[channel]) || fabs(rgb[channel]) > FLT_MAX) {
                return 0;
            }
        }
        for (channel = 0u; channel < 3u; ++channel) {
            dst[base + channel] = (float)rgb[channel];
        }
    }
    return 1;
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
    if (dst_profile->rgb_definition != NULL &&
        transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32) {
        return sixel_cms_float_rgb(transform, src, dst, pixel_count);
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

static int
sixel_cms_apply_backend(sixel_cms_transform_t const *transform,
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

sixel_cms_engine_t
sixel_cms_transform_engine(sixel_cms_transform_t const *transform)
{
    if (transform == NULL) {
        return SIXEL_CMS_ENGINE_NONE;
    }
    switch (transform->backend) {
    case SIXEL_CMS_PROFILE_BACKEND_BUILTIN:
        return SIXEL_CMS_ENGINE_BUILTIN;
    case SIXEL_CMS_PROFILE_BACKEND_LCMS2:
        return SIXEL_CMS_ENGINE_LCMS2;
    case SIXEL_CMS_PROFILE_BACKEND_COLORSYNC:
        return SIXEL_CMS_ENGINE_COLORSYNC;
    default:
        return SIXEL_CMS_ENGINE_NONE;
    }
}

int
sixel_cms_do_transform(sixel_cms_transform_t const *transform,
                       void const *src, void *dst, size_t pixel_count)
{
    int result;
    char const *path;

    result = sixel_cms_apply_backend(transform, src, dst, pixel_count);
    if (transform != NULL) {
        path = "external";
        if (transform->backend == SIXEL_CMS_PROFILE_BACKEND_BUILTIN) {
            path = transform->dst_profile->rgb_definition != NULL &&
                transform->dst_format == SIXEL_CMS_PIXELFORMAT_RGB_F32
                ? "extended-rgb" : "bounded-device";
        }
        sixel_trace_topic_message("loader",
            "LSXCMS2|event=apply|engine=%s|path=%s|src=%d|dst=%d|ok=%d",
            sixel_cms_engine_to_string(sixel_cms_transform_engine(transform)),
            path, (int)transform->src_format, (int)transform->dst_format,
            result);
    }
    return result;
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
    size_t sample_bytes;
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
    sample_bytes = 1u;
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
        normalized_pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32 &&
        normalized_pixelformat != SIXEL_PIXELFORMAT_CIELABFLOAT32) {
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
    if (normalized_pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32 &&
        src_colorspace != SIXEL_CMS_COLORSPACE_LAB) {
        return 0;
    }

    dst_profile = sixel_cms_create_srgb_profile();
    if (dst_profile == NULL) {
        return 0;
    }

    if (normalized_pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32) {
        sample_bytes = sizeof(float);
        src_type = SIXEL_CMS_PIXELFORMAT_RGB_F32;
        dst_type = SIXEL_CMS_PIXELFORMAT_RGB_F32;
    } else if (normalized_pixelformat == SIXEL_PIXELFORMAT_CIELABFLOAT32) {
        src_type = SIXEL_CMS_PIXELFORMAT_LAB_F32;
        dst_type = SIXEL_CMS_PIXELFORMAT_RGB_F32;
    }

    if (src_colorspace == SIXEL_CMS_COLORSPACE_GRAY) {
        /* Pack one gray sample without reducing sixteen-bit source detail. */
        if (pixel_count > SIZE_MAX / (3u * sample_bytes)) {
            goto cleanup;
        }
        gray_in = (unsigned char *)malloc(pixel_count * sample_bytes);
        rgb_out = (unsigned char *)malloc(pixel_count * 3u * sample_bytes);
        if (gray_in == NULL || rgb_out == NULL) {
            goto cleanup;
        }
        if (sample_bytes == sizeof(float)) {
            for (i = 0u; i < pixel_count; ++i) {
                ((float *)gray_in)[i] = ((float *)pixels)[i * 3u];
            }
        } else if (normalized_pixelformat == SIXEL_PIXELFORMAT_G8) {
            memcpy(gray_in, pixels, pixel_count);
        } else {
            for (i = 0u; i < pixel_count; ++i) {
                gray_in[i] = pixels[i * 3u];
            }
        }
        transform = sixel_cms_create_transform(src_profile,
                                               sample_bytes == sizeof(float)
                                               ? SIXEL_CMS_PIXELFORMAT_GRAY_F32
                                               : SIXEL_CMS_PIXELFORMAT_GRAY_8,
                                               dst_profile,
                                               dst_type,
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
            memcpy(pixels, rgb_out, pixel_count * 3u * sample_bytes);
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
    size_t i;
    double value;

    if (pixels == NULL || pixel_count == 0u) {
        return 0;
    }
    if (pixel_count > SIZE_MAX / (3u * sizeof(float))) {
        return 0;
    }
    /* Validate the entire in-place operation before changing any samples.
     * Transfer decoding is not a gamut or byte-storage boundary. */
    for (i = 0u; i < pixel_count * 3u; ++i) {
        value = sixel_color_decode_srgb(pixels[i]);
        if (!isfinite(value) || fabs(value) > FLT_MAX) {
            return 0;
        }
    }
    for (i = 0u; i < pixel_count * 3u; ++i) {
        pixels[i] = (float)sixel_color_decode_srgb(pixels[i]);
    }
    return 1;
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
    /* A failed apply may already have changed part of the input. Retrying
     * through a bounded sRGB fallback can both reinterpret and clip it. */
    return converted;
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
    sixel_cms_color_space_t src_colorspace;
    size_t pixel_count;
    int converted;

    dst_profile = NULL;
    transform = NULL;
    src_colorspace = SIXEL_CMS_COLORSPACE_UNKNOWN;
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
    src_colorspace = sixel_cms_get_color_space(src_profile);
    if (src_colorspace != SIXEL_CMS_COLORSPACE_LAB) {
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


/*
 * Color definitions connect RGB endpoints through XYZ D50, without the
 * legacy CMS convenience route through bounded sRGB. Definitions contain
 * either an analytic matrix/TRC or an owned, parsed ICC profile. The job
 * transform owns independent endpoint copies, so queue/clone lifetimes
 * never depend on the loader's temporary metadata buffers.
 */
struct sixel_color_def {
    double to_xyz[9];
    double from_xyz[9];
    double gamma[3];
    sixel_color_trc_t trc;
    unsigned char *bytes;
    size_t length;
    sixel_icc_profile_t icc;
    int matrix_valid;
};

struct sixel_color_xform {
    sixel_color_def_t *source;
    sixel_color_def_t *destination;
    int intent;
};

static int
sixel_cdef_finite(double const values[3])
{
    return values != NULL && isfinite(values[0]) &&
        isfinite(values[1]) && isfinite(values[2]);
}

static void
sixel_cdef_matrix(double const matrix[9], double const in[3], double out[3])
{
    int row;

    for (row = 0; row < 3; ++row) {
        out[row] = matrix[row * 3] * in[0] +
            matrix[row * 3 + 1] * in[1] + matrix[row * 3 + 2] * in[2];
    }
}

/*
 * Legacy ICC LUT evaluators return normalized encoded PCS samples. XYZ
 * LUT8 is u1.7 and LUT16/mAB is u1.15, whereas matrix tags are s15.16
 * tristimulus values. Decode this boundary only on the new descriptor path;
 * changing the legacy global PCS helper would also affect unrelated users.
 */
static double
sixel_cdef_pcs_scale(sixel_icc_lut_t const *lut)
{
    if (lut->kind == SIXEL_ICC_LUT_MFT1 ||
        lut->kind == SIXEL_ICC_LUT_MFT1_RGB_GRAY_A2B0) {
        return 255.0 / 128.0;
    }
    return 65535.0 / 32768.0;
}

sixel_color_def_t *
sixel_cdef_rgb(double const xy[8], sixel_color_trc_t trc,
                double const gamma[3])
{
    static double const bradford[9] = {
        0.8951, 0.2664, -0.1614,
        -0.7502, 1.7135, 0.0367,
        0.0389, -0.0685, 1.0296
    };
    static double const d50[3] = {0.9642, 1.0, 0.8249};
    sixel_color_def_t *definition;
    double matrix[9];
    double inverse[9];
    double white[3];
    double source_cone[3];
    double target_cone[3];
    double column[3];
    double cone[3];
    int i;
    int j;

    if (xy == NULL || gamma == NULL ||
        (trc != SIXEL_COLOR_TRC_SRGB && trc != SIXEL_COLOR_TRC_POWER)) {
        return NULL;
    }
    for (i = 0; i < 4; ++i) {
        if (!isfinite(xy[2 * i]) || !isfinite(xy[2 * i + 1]) ||
            xy[2 * i] < 0.0 || xy[2 * i + 1] <= 0.0 ||
            xy[2 * i] + xy[2 * i + 1] > 1.0 + 1e-12) {
            return NULL;
        }
    }
    for (i = 0; i < 3; ++i) {
        if (!isfinite(gamma[i]) || gamma[i] <= 0.0 || gamma[i] > 10.0) {
            return NULL;
        }
    }
    if (!sixel_cms_build_rgb_to_xyz_matrix_internal(
            xy[0], xy[1], xy[2], xy[3], xy[4], xy[5], xy[6], xy[7],
            matrix) || !sixel_cms_matrix3_invert_internal(bradford, inverse)) {
        return NULL;
    }
    white[0] = xy[0] / xy[1];
    white[1] = 1.0;
    white[2] = (1.0 - xy[0] - xy[1]) / xy[1];
    sixel_cdef_matrix(bradford, white, source_cone);
    sixel_cdef_matrix(bradford, d50, target_cone);
    for (i = 0; i < 3; ++i) {
        if (!isfinite(source_cone[i]) || source_cone[i] <= 1e-12) {
            return NULL;
        }
    }
    definition = calloc(1, sizeof(*definition));
    if (definition == NULL) {
        return NULL;
    }
    for (i = 0; i < 3; ++i) {
        if (matrix[3 + i] <= 0.0) {
            sixel_cdef_free(definition);
            return NULL;
        }
    }
    definition->trc = trc;
    memcpy(definition->gamma, gamma, sizeof(definition->gamma));
    /* Adapt the native white and every primary exactly once to the PCS. */
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            column[j] = matrix[j * 3 + i];
        }
        sixel_cdef_matrix(bradford, column, cone);
        for (j = 0; j < 3; ++j) {
            cone[j] *= target_cone[j] / source_cone[j];
        }
        sixel_cdef_matrix(inverse, cone, column);
        for (j = 0; j < 3; ++j) {
            definition->to_xyz[j * 3 + i] = column[j];
        }
    }
    if (!sixel_cms_matrix3_invert_internal(definition->to_xyz,
                                           definition->from_xyz)) {
        sixel_cdef_free(definition);
        return NULL;
    }
    for (i = 0; i < 9; ++i) {
        if (!isfinite(definition->to_xyz[i]) ||
            !isfinite(definition->from_xyz[i])) {
            sixel_cdef_free(definition);
            return NULL;
        }
    }
    return definition;
}

sixel_color_def_t *
sixel_cdef_name(char const *name)
{
    static double const srgb[8] = {
        0.3127, 0.3290, 0.64, 0.33, 0.30, 0.60, 0.15, 0.06
    };
    static double const p3[8] = {
        0.3127, 0.3290, 0.68, 0.32, 0.265, 0.69, 0.15, 0.06
    };
    static double const gamma[3] = {1.0, 1.0, 1.0};

    if (name == NULL) {
        return NULL;
    }
    if (strcmp(name, "srgb") == 0) {
        return sixel_cdef_rgb(srgb, SIXEL_COLOR_TRC_SRGB, gamma);
    }
    if (strcmp(name, "display-p3") == 0) {
        return sixel_cdef_rgb(p3, SIXEL_COLOR_TRC_SRGB, gamma);
    }
    return NULL;
}

sixel_color_def_t *
sixel_cdef_icc(void const *bytes, size_t length)
{
    sixel_color_def_t *definition;
    unsigned char const *header;
    size_t declared;
    int i;

    if (bytes == NULL || length < 128u || length > 16u * 1024u * 1024u) {
        return NULL;
    }
    header = bytes;
    declared = (size_t)header[0] * 16777216u + (size_t)header[1] * 65536u
        + (size_t)header[2] * 256u + (size_t)header[3];
    /* This owned endpoint requires an exact ICC envelope, unlike the
     * legacy parser's more permissive embedded-profile compatibility path.
     * This is not a complete class-specific ICC conformance check. */
    if (declared != length || memcmp(header + 36, "acsp", 4) != 0) {
        return NULL;
    }
    definition = calloc(1, sizeof(*definition));
    if (definition == NULL) {
        return NULL;
    }
    definition->bytes = malloc(length);
    if (definition->bytes == NULL) {
        sixel_cdef_free(definition);
        return NULL;
    }
    memcpy(definition->bytes, bytes, length);
    definition->length = length;
    if (!sixel_icc_parse_profile(definition->bytes, length, &definition->icc)
        || definition->icc.kind != SIXEL_ICC_PROFILE_KIND_RGB ||
        (definition->icc.pcs == SIXEL_ICC_PROFILE_PCS_LAB && header[8] < 4)) {
        /* Legacy v2 Lab LUT scaling needs a separate validated adapter. */
        sixel_cdef_free(definition);
        return NULL;
    }
    memcpy(definition->to_xyz, definition->icc.matrix_to_xyz_d50,
           sizeof(definition->to_xyz));
    definition->matrix_valid = sixel_cms_matrix3_invert_internal(
        definition->to_xyz, definition->from_xyz);
    for (i = 0; i < 3; ++i) {
        if (definition->icc.curves[i].kind == SIXEL_ICC_CURVE_INVALID) {
            definition->matrix_valid = 0;
        }
    }
    return definition;
}

sixel_color_def_t *
sixel_cdef_clone(sixel_color_def_t const *definition)
{
    sixel_color_def_t *clone;

    if (definition == NULL) {
        return NULL;
    }
    if (definition->bytes != NULL) {
        return sixel_cdef_icc(definition->bytes, definition->length);
    }
    clone = malloc(sizeof(*clone));
    if (clone != NULL) {
        memcpy(clone, definition, sizeof(*clone));
    }
    return clone;
}

void
sixel_cdef_free(sixel_color_def_t *definition)
{
    if (definition != NULL) {
        sixel_icc_profile_destroy(&definition->icc);
        free(definition->bytes);
        free(definition);
    }
}

int
sixel_cdef_to_xyz(sixel_color_def_t const *definition,
                   double const rgb[3], double xyz[3], int intent)
{
    double linear[3];
    double result[3];
    int i;

    /* Absolute intent requires media-white handling, which is not enabled. */
    if (definition == NULL || xyz == NULL || !sixel_cdef_finite(rgb) ||
        intent < 0 || intent > 2) {
        return 0;
    }
    if (definition->bytes != NULL) {
        if (definition->icc.a2b_lut[intent].kind != SIXEL_ICC_LUT_INVALID ||
            definition->icc.a2b_mab[intent].type !=
                SIXEL_ICC_MAB_TYPE_INVALID ||
            definition->icc.d2b_lut[intent].kind != SIXEL_ICC_LUT_INVALID ||
            definition->icc.d2b_mab[intent].type !=
                SIXEL_ICC_MAB_TYPE_INVALID) {
            if (!sixel_icc_apply_device_to_xyz_d50_with_a2b_slot(
                    result, rgb, 3u, &definition->icc,
                    (unsigned int)intent, 0)) {
                return 0;
            }
            if (definition->icc.pcs == SIXEL_ICC_PROFILE_PCS_XYZ) {
                for (i = 0; i < 3; ++i) {
                    result[i] *= sixel_cdef_pcs_scale(
                        definition->icc.a2b_lut[intent].kind !=
                        SIXEL_ICC_LUT_INVALID
                            ? &definition->icc.a2b_lut[intent]
                            : &definition->icc.d2b_lut[intent]);
                }
            }
        } else {
            if (!definition->matrix_valid) {
                return 0;
            }
            for (i = 0; i < 3; ++i) {
                if (!sixel_icc_curve_device(&definition->icc.curves[i],
                                               rgb[i], 0, &linear[i])) {
                    return 0;
                }
            }
            sixel_cdef_matrix(definition->to_xyz, linear, result);
        }
    } else {
        for (i = 0; i < 3; ++i) {
            linear[i] = definition->trc == SIXEL_COLOR_TRC_SRGB
                ? sixel_color_decode_srgb(rgb[i])
                : copysign(pow(fabs(rgb[i]), definition->gamma[i]), rgb[i]);
        }
        sixel_cdef_matrix(definition->to_xyz, linear, result);
    }
    if (!sixel_cdef_finite(result)) {
        return 0;
    }
    memcpy(xyz, result, sizeof(result));
    return 1;
}

int
sixel_cdef_from_xyz(sixel_color_def_t const *definition,
                     double const xyz[3], double rgb[3], int intent)
{
    double linear[3];
    double result[3];
    int i;

    if (definition == NULL || rgb == NULL || !sixel_cdef_finite(xyz) ||
        intent < 0 || intent > 2) {
        return 0;
    }
    if (definition->bytes != NULL) {
        if (definition->icc.b2a_lut[intent].kind != SIXEL_ICC_LUT_INVALID ||
            definition->icc.b2a_mab[intent].type !=
                SIXEL_ICC_MAB_TYPE_INVALID ||
            definition->icc.b2d_lut[intent].kind != SIXEL_ICC_LUT_INVALID ||
            definition->icc.b2d_mab[intent].type !=
                SIXEL_ICC_MAB_TYPE_INVALID) {
            memcpy(linear, xyz, sizeof(linear));
            if (definition->icc.pcs == SIXEL_ICC_PROFILE_PCS_XYZ) {
                for (i = 0; i < 3; ++i) {
                    linear[i] /= sixel_cdef_pcs_scale(
                        definition->icc.b2a_lut[intent].kind !=
                        SIXEL_ICC_LUT_INVALID
                            ? &definition->icc.b2a_lut[intent]
                            : &definition->icc.b2d_lut[intent]);
                }
            }
            if (!sixel_icc_apply_xyz_d50_to_device_with_b2a_slot(
                    result, 3u, linear, &definition->icc,
                    (unsigned int)intent)) {
                return 0;
            }
        } else {
            if (!definition->matrix_valid) {
                return 0;
            }
            sixel_cdef_matrix(definition->from_xyz, xyz, linear);
            for (i = 0; i < 3; ++i) {
                if (!sixel_icc_curve_device(&definition->icc.curves[i],
                                               linear[i], 1, &result[i])) {
                    return 0;
                }
            }
        }
    } else {
        sixel_cdef_matrix(definition->from_xyz, xyz, linear);
        for (i = 0; i < 3; ++i) {
            result[i] = definition->trc == SIXEL_COLOR_TRC_SRGB
                ? sixel_color_encode_srgb(linear[i])
                : copysign(pow(fabs(linear[i]), 1.0 / definition->gamma[i]),
                           linear[i]);
        }
    }
    if (!sixel_cdef_finite(result)) {
        return 0;
    }
    memcpy(rgb, result, sizeof(result));
    return 1;
}

int
sixel_cdef_output_ready(sixel_color_def_t const *definition,
                         int encoding_intent, int evaluation_intent)
{
    static double const probes[][3] = {
        {0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {0.5, 0.5, 0.5},
        {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}
    };
    double xyz[3];
    double encoded[3];
    double evaluated[3];
    size_t i;

    for (i = 0; i < sizeof(probes) / sizeof(probes[0]); ++i) {
        if (!sixel_cdef_to_xyz(definition, probes[i], xyz,
                                evaluation_intent) ||
            !sixel_cdef_from_xyz(definition, xyz, encoded,
                                  encoding_intent) ||
            !sixel_cdef_to_xyz(definition, encoded, evaluated,
                                evaluation_intent)) {
            return 0;
        }
    }
    /* M6 must additionally evaluate every finalized, quantized wire entry. */
    return 1;
}

sixel_color_xform_t *
sixel_cxform_new(sixel_color_def_t const *source,
                  sixel_color_def_t const *destination, int intent)
{
    sixel_color_xform_t *transform;

    if (source == NULL || destination == NULL || intent < 0 || intent > 2) {
        return NULL;
    }
    transform = calloc(1, sizeof(*transform));
    if (transform == NULL) {
        return NULL;
    }
    transform->source = sixel_cdef_clone(source);
    transform->destination = sixel_cdef_clone(destination);
    transform->intent = intent;
    if (transform->source == NULL || transform->destination == NULL) {
        sixel_cxform_free(transform);
        return NULL;
    }
    return transform;
}

int
sixel_cxform_apply(sixel_color_xform_t const *transform,
                    double const source[3], double destination[3])
{
    double xyz[3];

    return transform != NULL &&
        sixel_cdef_to_xyz(transform->source, source, xyz, transform->intent)
        && sixel_cdef_from_xyz(transform->destination, xyz, destination,
                                transform->intent);
}

void
sixel_cxform_free(sixel_color_xform_t *transform)
{
    if (transform != NULL) {
        sixel_cdef_free(transform->destination);
        sixel_cdef_free(transform->source);
        free(transform);
    }
}

/* Parse exact tokens and consume every numeric field, including the end.
 * Whitespace and nonfinite numbers are rejected rather than normalized. */
int
sixel_cdef_parse(char const *text, sixel_color_def_t **definition,
                  char const **icc_path, int *intent)
{
    sixel_color_def_t *result;
    char const *cursor;
    char const *path;
    char *end;
    double xy[8];
    double gamma[3];
    int slot;
    int i;

    if (text == NULL || definition == NULL || icc_path == NULL
            || intent == NULL) {
        return 0;
    }
    result = NULL;
    path = NULL;
    slot = 1;
    if (strcmp(text, "auto") == 0) {
        /* The loader resolves metadata; this is not an sRGB profile tag. */
    } else if (strncmp(text, "icc:", 4) == 0) {
        path = text + 4;
        if (strncmp(path, "perceptual:", 11) == 0) {
            slot = 0;
            path += 11;
        } else if (strncmp(path, "relative:", 9) == 0) {
            path += 9;
        } else if (strncmp(path, "saturation:", 11) == 0) {
            slot = 2;
            path += 11;
        }
        if (*path == '\0' || strcmp(path, "-") == 0) {
            return 0;
        }
    } else if (strncmp(text, "rgb:", 4) == 0) {
        for (cursor = text; *cursor != '\0'; ++cursor) {
            if (isspace((unsigned char)*cursor)) {
                return 0;
            }
        }
        cursor = text + 4;
        for (i = 0; i < 8; ++i) {
            xy[i] = strtod(cursor, &end);
            if (cursor == end || !isfinite(xy[i])
                    || *end != (i == 7 ? ':' : ',')) {
                return 0;
            }
            cursor = end + 1;
        }
        if (strcmp(cursor, "srgb") == 0) {
            gamma[0] = gamma[1] = gamma[2] = 1.0;
            result = sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_SRGB, gamma);
        } else if (strncmp(cursor, "gamma=", 6) == 0) {
            cursor += 6;
            for (i = 0; i < 3; ++i) {
                gamma[i] = strtod(cursor, &end);
                if (cursor == end || !isfinite(gamma[i])
                        || *end != (i == 2 ? '\0' : ',')) {
                    return 0;
                }
                cursor = end + (i != 2);
            }
            result = sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_POWER, gamma);
        }
        if (result == NULL) {
            return 0;
        }
    } else {
        result = sixel_cdef_name(text);
        if (result == NULL) {
            return 0;
        }
    }
    *definition = result;
    *icc_path = path;
    *intent = slot;
    return 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
