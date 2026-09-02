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

#include <sixel.h>
#include <stddef.h>
#include <string.h>

#include "cms.h"
#include "dither-interframe-method.h"
#include "encoder.h"
#include "loader-common.h"
#include "options-registry.h"
#include "palette-common-cover.h"
#include "palette-heckbert.h"
#include "palette-kcenter.h"
#include "palette-kmeans.h"
#include "palette-kmedoids.h"

#define SIXEL_REGISTRY_ARRAY_LENGTH(array_) \
    (sizeof(array_) / sizeof((array_)[0]))

/*
 * Make a width mismatch in a binding a compile-time error.  C99 cannot
 * compare signedness portably, but checking the actual member width prevents
 * a registry typo from overwriting an adjacent structure member.
 */
#define SIXEL_REGISTRY_CHECKED_OFFSET(type_, field_, value_type_) \
    (offsetof(type_, field_) + \
     0u * sizeof(char[sizeof(((type_ *)0)->field_) == \
                     sizeof(value_type_) ? 1 : -1]))

#define SIXEL_REGISTRY_NO_BINDING \
    { \
        SIXEL_SUBOPTION_TARGET_NONE, SIXEL_SUBOPTION_STORAGE_INT, \
        SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE, \
        SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE \
    }

#define SIXEL_REGISTRY_CHOICE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, 0.0, 0.0, 0, 0, \
        0, 0, NULL, NULL, SIXEL_REGISTRY_NO_BINDING \
    }

#define SIXEL_REGISTRY_BOUND_CHOICE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    target_, type_, field_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, 0.0, 0.0, 0, 0, \
        0, 0, NULL, NULL, \
        { \
            (target_), SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(type_, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE \
        } \
    }

#define SIXEL_REGISTRY_BOUND_CHOICE_ENV( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    environment_choices_, target_, type_, field_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), (environment_choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(environment_choices_), 0.0, 0.0, 0, \
        0, 0, 0, NULL, NULL, \
        { \
            (target_), SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(type_, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE \
        } \
    }

#define SIXEL_REGISTRY_BOUND_BOOLEAN( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, target_, type_, \
    field_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_BOOLEAN, NULL, 0u, NULL, 0u, 0.0, \
        1.0, 1, 1, 1, 0, "boolean suboption must be 0 or 1.", NULL, \
        { \
            (target_), SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(type_, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE \
        } \
    }

#define SIXEL_REGISTRY_NUMBER( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, kind_, \
    minimum_, maximum_, has_minimum_, has_maximum_, allow_zero_, message_, \
    suffix_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), (kind_), NULL, 0u, NULL, 0u, (minimum_), (maximum_), \
        (has_minimum_), (has_maximum_), (allow_zero_), 0, (message_), \
        (suffix_), \
        SIXEL_REGISTRY_NO_BINDING \
    }

#define SIXEL_REGISTRY_INT( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, message_) \
    SIXEL_REGISTRY_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_INT, 0.0, 0.0, 0, 0, 0, message_, NULL)

#define SIXEL_REGISTRY_UINT( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_) \
    SIXEL_REGISTRY_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_UINT, minimum_, maximum_, 1, 1, allow_zero_, \
        message_, NULL)

#define SIXEL_REGISTRY_UINT_VALUE_MESSAGE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, suffix_) \
    SIXEL_REGISTRY_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_UINT, minimum_, maximum_, 1, 1, allow_zero_, \
        message_, suffix_)

#define SIXEL_REGISTRY_BOUND_UINT_VALUE_MESSAGE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, environment_clamp_, message_, suffix_, target_, \
    type_, field_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_UINT, NULL, 0u, NULL, 0u, \
        (minimum_), \
        (maximum_), 1, 1, (allow_zero_), (environment_clamp_), (message_), \
        (suffix_), \
        { \
            (target_), SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(type_, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE \
        } \
    }

#define SIXEL_REGISTRY_BOUND_UINT( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, target_, type_, field_) \
    SIXEL_REGISTRY_BOUND_UINT_VALUE_MESSAGE( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
        maximum_, allow_zero_, 0, message_, NULL, target_, type_, field_)

#define SIXEL_REGISTRY_FLOAT( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, message_) \
    SIXEL_REGISTRY_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_FLOAT, 0.0, 0.0, 0, 0, 0, message_, NULL)

#define SIXEL_REGISTRY_DOUBLE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, message_) \
    SIXEL_REGISTRY_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_DOUBLE, minimum_, maximum_, 1, 1, 0, message_, \
        NULL)

#define SIXEL_REGISTRY_INT_PAIR( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, message_) \
    SIXEL_REGISTRY_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_INT_PAIR, 0.0, 0.0, 0, 0, 0, message_, NULL)

#define SIXEL_REGISTRY_SCALED_U8( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, message_) \
    SIXEL_REGISTRY_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_SCALED_U8, minimum_, maximum_, 1, 1, 0, \
        message_, NULL)

#define SIXEL_REGISTRY_ENCODER_CHOICE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    field_, override_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, 0.0, 0.0, 0, 0, \
        0, 0, NULL, NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE \
        } \
    }

#define SIXEL_REGISTRY_ENCODER_CHOICE_ENV( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    environment_choices_, field_, override_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), (environment_choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(environment_choices_), 0.0, 0.0, 0, \
        0, 0, 0, NULL, NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE \
        } \
    }

#define SIXEL_REGISTRY_ENCODER_BOOLEAN( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, field_, \
    override_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_BOOLEAN, NULL, 0u, NULL, 0u, 0.0, \
        1.0, 1, 1, 1, 0, "boolean suboption must be 0 or 1.", NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE \
        } \
    }

#define SIXEL_REGISTRY_ENCODER_DIRECT_CHOICE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    field_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, 0.0, 0.0, 0, 0, \
        0, 0, NULL, NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE \
        } \
    }

#define SIXEL_REGISTRY_ENCODER_MIRROR_CHOICE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    field_, override_, mirror_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, 0.0, 0.0, 0, 0, \
        0, 0, NULL, NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, override_, int), \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, mirror_, int) \
        } \
    }

#define SIXEL_REGISTRY_ENCODER_NUMBER( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, kind_, \
    minimum_, maximum_, has_minimum_, has_maximum_, allow_zero_, message_, \
    storage_, value_type_, field_, second_, override_) \
    { \
        (optflag_), (base_), (name_), (short_), (env_), (fallback_), \
        (legacy_), (kind_), NULL, 0u, NULL, 0u, (minimum_), (maximum_), \
        (has_minimum_), (has_maximum_), (allow_zero_), 0, (message_), NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, (storage_), \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_encoder_t, field_, value_type_), \
            (second_), \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE \
        } \
    }

#define SIXEL_REGISTRY_ENCODER_UINT( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_UINT, minimum_, maximum_, 1, 1, allow_zero_, \
        message_, SIXEL_SUBOPTION_STORAGE_UINT, unsigned int, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_)

#define SIXEL_REGISTRY_ENCODER_INT( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, message_, \
    field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_INT, 0.0, 0.0, 0, 0, 0, message_, \
        SIXEL_SUBOPTION_STORAGE_INT, int, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_)

#define SIXEL_REGISTRY_ENCODER_FLOAT( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, message_, \
    field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_FLOAT, 0.0, 0.0, 0, 0, 0, message_, \
        SIXEL_SUBOPTION_STORAGE_FLOAT, float, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_)

#define SIXEL_REGISTRY_ENCODER_DOUBLE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, message_, field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_DOUBLE, minimum_, maximum_, 1, 1, 0, message_, \
        SIXEL_SUBOPTION_STORAGE_DOUBLE, double, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_)

#define SIXEL_REGISTRY_ENCODER_SCALED_U8( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, message_, field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_SCALED_U8, minimum_, maximum_, 1, 1, 0, \
        message_, SIXEL_SUBOPTION_STORAGE_INT, int, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_)

#define SIXEL_REGISTRY_ENCODER_INT_PAIR( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, message_, \
    field_, second_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_INT_PAIR, 0.0, 0.0, 0, 0, 0, message_, \
        SIXEL_SUBOPTION_STORAGE_INT_PAIR, int, field_, \
        SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, second_, int), \
        override_)

enum {
    SIXEL_DEQUANTIZE_BASE_NONE = 0,
    SIXEL_DEQUANTIZE_BASE_K_UNDITHER,
    SIXEL_DEQUANTIZE_BASE_LSO_UNDITHER,
    SIXEL_DEQUANTIZE_BASE_SELECTIVE_BLUR
};

static sixel_option_value_schema_t const g_dequantize_values[] = {
    { "none", SIXEL_DEQUANTIZE_NONE, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    {
        "k_undither", SIXEL_DEQUANTIZE_K_UNDITHER, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "lso_undither", SIXEL_OPTION_DEQUANTIZE_LSO_BASE, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "selective_blur", SIXEL_DEQUANTIZE_SELECTIVE_BLUR, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    }
};

enum {
    SIXEL_DIFFUSION_BASE_AUTO = 0,
    SIXEL_DIFFUSION_BASE_NONE,
    SIXEL_DIFFUSION_BASE_FS,
    SIXEL_DIFFUSION_BASE_ATKINSON,
    SIXEL_DIFFUSION_BASE_JAJUNI,
    SIXEL_DIFFUSION_BASE_STUCKI,
    SIXEL_DIFFUSION_BASE_BURKES,
    SIXEL_DIFFUSION_BASE_SIERRA,
    SIXEL_DIFFUSION_BASE_A_DITHER,
    SIXEL_DIFFUSION_BASE_X_DITHER,
    SIXEL_DIFFUSION_BASE_BLUENOISE,
    SIXEL_DIFFUSION_BASE_LSO2,
    SIXEL_DIFFUSION_BASE_INTERFRAME,
    SIXEL_DIFFUSION_BASE_STBN
};

static sixel_option_value_schema_t const g_diffusion_values[] = {
    { "auto", SIXEL_DIFFUSE_AUTO, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "none", SIXEL_DIFFUSE_NONE, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "fs", SIXEL_DIFFUSE_FS, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "atkinson", SIXEL_DIFFUSE_ATKINSON, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "jajuni", SIXEL_DIFFUSE_JAJUNI, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "stucki", SIXEL_DIFFUSE_STUCKI, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "burkes", SIXEL_DIFFUSE_BURKES, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "sierra", SIXEL_DIFFUSE_SIERRA1, 1u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "a_dither", SIXEL_DIFFUSE_A_DITHER, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "x_dither", SIXEL_DIFFUSE_X_DITHER, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    {
        "bluenoise", SIXEL_DIFFUSE_BLUENOISE_DITHER, 6u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    { "lso2", SIXEL_DIFFUSE_LSO2, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    {
        "interframe",
        SIXEL_DIFFUSE_INTERFRAME,
        1u,
        SIXEL_OPTION_BASE_POLICY_DIFFUSION_INTERFRAME
    },
    {
        "stbn",
        SIXEL_DIFFUSE_INTERFRAME,
        3u,
        SIXEL_OPTION_BASE_POLICY_DIFFUSION_STBN
    }
};

enum {
    SIXEL_QUANTIZE_BASE_AUTO = 0,
    SIXEL_QUANTIZE_BASE_HECKBERT,
    SIXEL_QUANTIZE_BASE_KMEANS,
    SIXEL_QUANTIZE_BASE_MEDOIDS,
    SIXEL_QUANTIZE_BASE_CENTER
};

static sixel_option_value_schema_t const g_quantize_values[] = {
    { "auto", SIXEL_QUANTIZE_MODEL_AUTO, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    {
        "heckbert", SIXEL_QUANTIZE_MODEL_MEDIANCUT, 1u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "kmeans", SIXEL_QUANTIZE_MODEL_KMEANS, 17u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "medoids", SIXEL_QUANTIZE_MODEL_KMEDOIDS, 17u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "center", SIXEL_QUANTIZE_MODEL_KCENTER, 20u,
        SIXEL_OPTION_BASE_POLICY_NONE
    }
};

enum {
    SIXEL_LOOKUP_BASE_AUTO = 0,
    SIXEL_LOOKUP_BASE_5BIT,
    SIXEL_LOOKUP_BASE_6BIT,
    SIXEL_LOOKUP_BASE_NONE,
    SIXEL_LOOKUP_BASE_CERTLUT,
    SIXEL_LOOKUP_BASE_EYTZINGER,
    SIXEL_LOOKUP_BASE_FHEDT,
    SIXEL_LOOKUP_BASE_VPTREE,
    SIXEL_LOOKUP_BASE_RBC,
    SIXEL_LOOKUP_BASE_MAHALANOBIS
};

static sixel_option_value_schema_t const g_lookup_values[] = {
    { "auto", SIXEL_LUT_POLICY_AUTO, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "5bit", SIXEL_LUT_POLICY_5BIT, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "6bit", SIXEL_LUT_POLICY_6BIT, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "none", SIXEL_LUT_POLICY_NONE, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "certlut", SIXEL_LUT_POLICY_CERTLUT, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    {
        "eytzinger", SIXEL_LUT_POLICY_EYTZINGER, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    { "fhedt", SIXEL_LUT_POLICY_FHEDT, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "vptree", SIXEL_LUT_POLICY_VPTREE, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "rbc", SIXEL_LUT_POLICY_RBC, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    {
        "mahalanobis", SIXEL_LUT_POLICY_MAHALANOBIS, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    }
};

enum {
    SIXEL_LOADER_VALUE_LIBPNG = 0,
    SIXEL_LOADER_VALUE_LIBJPEG,
    SIXEL_LOADER_VALUE_LIBWEBP,
    SIXEL_LOADER_VALUE_LIBTIFF,
    SIXEL_LOADER_VALUE_LIBRSVG,
    SIXEL_LOADER_VALUE_BUILTIN,
    SIXEL_LOADER_VALUE_WIC,
    SIXEL_LOADER_VALUE_COREGRAPHICS,
    SIXEL_LOADER_VALUE_GDK_PIXBUF2,
    SIXEL_LOADER_VALUE_GD,
    SIXEL_LOADER_VALUE_QUICKLOOK,
    SIXEL_LOADER_VALUE_GNOME_THUMBNAILER
};

/*
 * Feature-disabled loaders are omitted from the base table.  Keep a second
 * enum for physical table indexes so base pointers remain correct without
 * changing the stable resolved values above.
 */
enum {
#if HAVE_LIBPNG
    SIXEL_LOADER_INDEX_LIBPNG,
#endif
#if HAVE_JPEG
    SIXEL_LOADER_INDEX_LIBJPEG,
#endif
#if HAVE_WEBP
    SIXEL_LOADER_INDEX_LIBWEBP,
#endif
#if HAVE_LIBTIFF
    SIXEL_LOADER_INDEX_LIBTIFF,
#endif
#if HAVE_LIBRSVG
    SIXEL_LOADER_INDEX_LIBRSVG,
#endif
    SIXEL_LOADER_INDEX_BUILTIN,
#if HAVE_WIC
    SIXEL_LOADER_INDEX_WIC,
#endif
#if HAVE_COREGRAPHICS
    SIXEL_LOADER_INDEX_COREGRAPHICS,
#endif
#if HAVE_GDK_PIXBUF2
    SIXEL_LOADER_INDEX_GDK_PIXBUF2,
#endif
#if HAVE_GD
    SIXEL_LOADER_INDEX_GD,
#endif
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
    SIXEL_LOADER_INDEX_QUICKLOOK,
#endif
#if HAVE_FREEDESKTOP_THUMBNAILING
    SIXEL_LOADER_INDEX_GNOME_THUMBNAILER,
#endif
    SIXEL_LOADER_INDEX_COUNT
};

static sixel_option_value_schema_t const g_loader_values[] = {
#if HAVE_LIBPNG
    { "libpng", SIXEL_LOADER_VALUE_LIBPNG, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
#endif
#if HAVE_JPEG
    {
        "libjpeg", SIXEL_LOADER_VALUE_LIBJPEG, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
#endif
#if HAVE_WEBP
    {
        "libwebp", SIXEL_LOADER_VALUE_LIBWEBP, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
#endif
#if HAVE_LIBTIFF
    {
        "libtiff", SIXEL_LOADER_VALUE_LIBTIFF, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
#endif
#if HAVE_LIBRSVG
    {
        "librsvg", SIXEL_LOADER_VALUE_LIBRSVG, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
#endif
    {
        "builtin", SIXEL_LOADER_VALUE_BUILTIN, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
#if HAVE_WIC
    { "wic", SIXEL_LOADER_VALUE_WIC, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
#endif
#if HAVE_COREGRAPHICS
    {
        "coregraphics", SIXEL_LOADER_VALUE_COREGRAPHICS, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
#endif
#if HAVE_GDK_PIXBUF2
    {
        "gdk-pixbuf2", SIXEL_LOADER_VALUE_GDK_PIXBUF2, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
#endif
#if HAVE_GD
    { "gd", SIXEL_LOADER_VALUE_GD, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
#endif
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
    {
        "quicklook", SIXEL_LOADER_VALUE_QUICKLOOK, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
#endif
#if HAVE_FREEDESKTOP_THUMBNAILING
    {
        "gnome-thumbnailer",
        SIXEL_LOADER_VALUE_GNOME_THUMBNAILER,
        0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
#endif
};

static sixel_suboption_choice_t const g_dequantize_lso_variant_choices[] = {
    { "fs", SIXEL_DEQUANTIZE_LSO_UNDITHER_VFS },
    { "light", SIXEL_DEQUANTIZE_LSO_UNDITHER_VLIGHT }
};

static sixel_suboption_choice_t const g_stbn_source_choices[] = {
    { "hash", SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_HASH },
    { "mask", SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_MASK },
    { "pmj", SIXEL_INTERFRAME_STRATEGY_TOKEN_PMJ }
};

static sixel_suboption_choice_t const g_stbn_source_environment_choices[] = {
    { "stbn", SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_HASH },
    { "stbn-hash", SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_HASH },
    { "stbn-mask", SIXEL_INTERFRAME_STRATEGY_TOKEN_STBN_MASK }
};

static sixel_suboption_choice_t const g_diffusion_scan_choices[] = {
    { "auto", SIXEL_SCAN_AUTO },
    { "serpentine", SIXEL_SCAN_SERPENTINE },
    { "raster", SIXEL_SCAN_RASTER }
};

static sixel_suboption_choice_t const g_bluenoise_channel_choices[] = {
    { "mono", 0 },
    { "rgb", 1 }
};

static sixel_suboption_choice_t const g_bluenoise_size_choices[] = {
    { "64", 64 }
};

static sixel_suboption_choice_t const g_interframe_diffusion_choices[] = {
    { "auto", SIXEL_DIFFUSE_FS },
    { "none", SIXEL_DIFFUSE_NONE },
    { "fs", SIXEL_DIFFUSE_FS },
    { "atkinson", SIXEL_DIFFUSE_ATKINSON },
    { "jajuni", SIXEL_DIFFUSE_JAJUNI },
    { "stucki", SIXEL_DIFFUSE_STUCKI },
    { "burkes", SIXEL_DIFFUSE_BURKES },
    { "sierra1", SIXEL_DIFFUSE_SIERRA1 },
    { "sierra2", SIXEL_DIFFUSE_SIERRA2 },
    { "sierra3", SIXEL_DIFFUSE_SIERRA3 }
};

static sixel_suboption_choice_t const g_sierra_variant_choices[] = {
    { "1", SIXEL_DIFFUSE_SIERRA1 },
    { "2", SIXEL_DIFFUSE_SIERRA2 },
    { "3", SIXEL_DIFFUSE_SIERRA3 }
};

static sixel_suboption_choice_t const g_kmeans_init_type_choices[] = {
    { "auto", SIXEL_PALETTE_KMEANS_INIT_AUTO },
    { "none", SIXEL_PALETTE_KMEANS_INIT_NONE },
    { "pca", SIXEL_PALETTE_KMEANS_INIT_PCA }
};

static sixel_suboption_choice_t const g_kmeans_binning_choices[] = {
    { "auto", SIXEL_PALETTE_KMEANS_BINNING_AUTO },
    { "none", SIXEL_PALETTE_KMEANS_BINNING_NONE },
    { "hard", SIXEL_PALETTE_KMEANS_BINNING_HARD },
    { "soft", SIXEL_PALETTE_KMEANS_BINNING_SOFT }
};

static sixel_suboption_choice_t const g_kmeans_mapping_choices[] = {
    { "uniform", SIXEL_PALETTE_KMEANS_MAPPING_UNIFORM },
    { "srgb", SIXEL_PALETTE_KMEANS_MAPPING_SRGB }
};

static sixel_suboption_choice_t const g_kmeans_softdist_choices[] = {
    { "trilinear", SIXEL_PALETTE_KMEANS_SOFTDIST_TRILINEAR }
};

static sixel_suboption_choice_t const g_kmeans_prune_choices[] = {
    { "auto", SIXEL_PALETTE_KMEANS_PRUNE_AUTO },
    { "none", SIXEL_PALETTE_KMEANS_PRUNE_NONE },
    { "hamerly", SIXEL_PALETTE_KMEANS_PRUNE_HAMERLY },
    { "elkan", SIXEL_PALETTE_KMEANS_PRUNE_ELKAN },
    { "yinyang", SIXEL_PALETTE_KMEANS_PRUNE_YINYANG }
};

static sixel_suboption_choice_t const g_kmedoids_algo_choices[] = {
    { "auto", SIXEL_PALETTE_KMEDOIDS_ALGO_AUTO },
    { "pam", SIXEL_PALETTE_KMEDOIDS_ALGO_PAM },
    { "sample", SIXEL_PALETTE_KMEDOIDS_ALGO_CLARA },
    { "random", SIXEL_PALETTE_KMEDOIDS_ALGO_CLARANS },
    { "bandit", SIXEL_PALETTE_KMEDOIDS_ALGO_BANDITPAM }
};

static sixel_suboption_choice_t const g_kcenter_algo_choices[] = {
    { "auto", SIXEL_PALETTE_KCENTER_ALGO_AUTO },
    { "fft", SIXEL_PALETTE_KCENTER_ALGO_FFT },
    { "swap", SIXEL_PALETTE_KCENTER_ALGO_SWAP },
    { "hybrid", SIXEL_PALETTE_KCENTER_ALGO_HYBRID }
};

static sixel_suboption_choice_t const g_kcenter_profile_choices[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_PROFILE_LEGACY },
    { "speed", SIXEL_PALETTE_KCENTER_PROFILE_SPEED },
    { "balance", SIXEL_PALETTE_KCENTER_PROFILE_BALANCE },
    { "quality", SIXEL_PALETTE_KCENTER_PROFILE_QUALITY }
};

static sixel_suboption_choice_t const g_kcenter_auto_policy_choices[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_AUTO_POLICY_LEGACY },
    { "adaptive", SIXEL_PALETTE_KCENTER_AUTO_POLICY_ADAPTIVE }
};

static sixel_suboption_choice_t const g_kcenter_space_policy_choices[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_SPACE_POLICY_LEGACY },
    { "perceptual", SIXEL_PALETTE_KCENTER_SPACE_POLICY_PERCEPTUAL }
};

static sixel_suboption_choice_t const g_kcenter_candidate_policy_choices[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_LEGACY },
    { "hybrid", SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY_HYBRID }
};

static sixel_suboption_choice_t const g_kcenter_budget_policy_choices[] = {
    { "legacy", SIXEL_PALETTE_KCENTER_BUDGET_POLICY_LEGACY },
    { "adaptive", SIXEL_PALETTE_KCENTER_BUDGET_POLICY_ADAPTIVE }
};

static sixel_suboption_choice_t const g_kcenter_swap_update_choices[] = {
    { "full", SIXEL_PALETTE_KCENTER_SWAP_UPDATE_FULL },
    { "incremental", SIXEL_PALETTE_KCENTER_SWAP_UPDATE_INCREMENTAL }
};

static sixel_suboption_choice_t const g_palette_cover_choices[] = {
    { "off", SIXEL_PALETTE_COVER_OFF },
    { "corners", SIXEL_PALETTE_COVER_CORNERS },
    { "faces", SIXEL_PALETTE_COVER_FACES },
    { "edges", SIXEL_PALETTE_COVER_EDGES },
    { "all", SIXEL_PALETTE_COVER_EDGES },
    { "auto", SIXEL_PALETTE_COVER_AUTO }
};

static sixel_suboption_choice_t const g_palette_cover_mode_choices[] = {
    { "hard", SIXEL_PALETTE_COVER_MODE_HARD },
    { "soft", SIXEL_PALETTE_COVER_MODE_SOFT }
};

static sixel_suboption_choice_t const g_quantize_merge_choices[] = {
    { "auto", SIXEL_FINAL_MERGE_AUTO },
    { "none", SIXEL_FINAL_MERGE_NONE },
    { "ward", SIXEL_FINAL_MERGE_WARD }
};

static sixel_suboption_choice_t const g_heckbert_profile_choices[] = {
    { "compat", SIXEL_HECKBERT_PROFILE_COMPAT },
    { "speed", SIXEL_HECKBERT_PROFILE_SPEED },
    { "quality", SIXEL_HECKBERT_PROFILE_QUALITY }
};

static sixel_suboption_choice_t const g_loader_cms_engine_choices[] = {
    { "none", SIXEL_CMS_ENGINE_NONE },
    { "auto", SIXEL_CMS_ENGINE_AUTO },
    { "builtin", SIXEL_CMS_ENGINE_BUILTIN },
    { "lcms2", SIXEL_CMS_ENGINE_LCMS2 },
    { "colorsync", SIXEL_CMS_ENGINE_COLORSYNC }
};

static sixel_suboption_choice_t const g_loader_cms_environment_choices[] = {
    { "off", SIXEL_CMS_ENGINE_NONE },
    { "disabled", SIXEL_CMS_ENGINE_NONE },
    { "lcms", SIXEL_CMS_ENGINE_LCMS2 },
    { "color-sync", SIXEL_CMS_ENGINE_COLORSYNC }
};

static sixel_suboption_choice_t const g_loader_bmp_info40_mode_choices[] = {
    { "auto", SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_AUTO },
    { "windows", SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_WINDOWS },
    { "os2", SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_OS2 }
};

static sixel_suboption_choice_t const g_loader_bmp_environment_choices[] = {
    { "0", SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_AUTO },
    { "1", SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_WINDOWS },
    { "2", SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_OS2 }
};

/*
 * This is the sole authoritative suboption registry.  A NULL base pointer
 * means that the row is shared by every base value of the owning option.
 * Common rows keep orthogonal controls identical without copying definitions
 * into every quantizer or diffusion method.
 */
static sixel_suboption_key_t const g_suboptions[] = {
    SIXEL_REGISTRY_BOUND_CHOICE(
        SIXEL_OPTION_SCHEMA_DEQUANTIZE,
        g_dequantize_values + SIXEL_DEQUANTIZE_BASE_LSO_UNDITHER,
        "variant", 'V', "SIXEL_DEQUANTIZE_LSO_VARIANT", NULL, NULL,
        g_dequantize_lso_variant_choices,
        SIXEL_SUBOPTION_TARGET_DEQUANTIZE,
        sixel_dequantize_options_t,
        method),
    SIXEL_REGISTRY_BOUND_UINT(
        SIXEL_OPTION_SCHEMA_DEQUANTIZE,
        g_dequantize_values + SIXEL_DEQUANTIZE_BASE_SELECTIVE_BLUR,
        "threshold", 'T',
        "SIXEL_DEQUANTIZE_SELECTIVE_BLUR_THRESHOLD", NULL, NULL,
        0.0, 441.0, 0,
        "selective_blur threshold must be an integer in range 0..441.",
        SIXEL_SUBOPTION_TARGET_DEQUANTIZE,
        sixel_dequantize_options_t,
        selective_blur_threshold),

    SIXEL_REGISTRY_ENCODER_DIRECT_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION, NULL,
        "scan", 'N', "SIXEL_DITHER_SCAN", NULL, NULL,
        g_diffusion_scan_choices, method_for_scan),
    SIXEL_REGISTRY_ENCODER_DIRECT_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_SIERRA,
        "variant", 'V', "SIXEL_DITHER_SIERRA_VARIANT", NULL, NULL,
        g_sierra_variant_choices, method_for_diffuse),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_INTERFRAME,
        "diffusion", 'D', "SIXEL_DITHER_INTERFRAME_DIFFUSION",
        NULL, NULL, g_interframe_diffusion_choices,
        interframe_spatial_diffuse,
        interframe_spatial_diffuse_override),
    SIXEL_REGISTRY_ENCODER_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "source", 'S', "SIXEL_DITHER_STBN_SOURCE",
        NULL, NULL, g_stbn_source_choices,
        g_stbn_source_environment_choices,
        interframe_strategy_token,
        interframe_strategy_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "diffusion", 'D', "SIXEL_DITHER_STBN_DIFFUSION",
        NULL, NULL, g_interframe_diffusion_choices,
        interframe_spatial_diffuse,
        interframe_spatial_diffuse_override),
    SIXEL_REGISTRY_ENCODER_SCALED_U8(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "strength", 'T', "SIXEL_DITHER_STBN_STRENGTH",
        NULL, NULL, 0.0, 2.0,
        "-d stbn:strength must be in range 0.0-2.0.",
        interframe_noise_strength_u8,
        interframe_noise_strength_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "motion_adapt", 'M', "SIXEL_DITHER_STBN_MOTION_ADAPT",
        NULL, NULL,
        stbn_motion_adapt_enabled, stbn_motion_adapt_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "scene_cut_reset", 'C',
        "SIXEL_DITHER_STBN_SCENE_CUT_RESET",
        NULL, NULL,
        stbn_scene_cut_reset_enabled, stbn_scene_cut_reset_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "scene_detect", 'E', "SIXEL_DITHER_STBN_SCENE_DETECT",
        NULL, NULL,
        stbn_scene_detect_enabled, stbn_scene_detect_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "alpha_guard", 'A', "SIXEL_DITHER_STBN_ALPHA_GUARD",
        NULL, NULL,
        stbn_alpha_guard_enabled, stbn_alpha_guard_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "perceptual_weight", 'P',
        "SIXEL_DITHER_STBN_PERCEPTUAL_WEIGHT",
        NULL, NULL,
        stbn_perceptual_weight_enabled,
        stbn_perceptual_weight_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_STBN,
        "fastpath", 'F', "SIXEL_DITHER_STBN_FASTPATH",
        NULL, NULL,
        stbn_fastpath_enabled, stbn_fastpath_override),
    SIXEL_REGISTRY_ENCODER_FLOAT(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE,
        "strength", 'T', "SIXEL_DITHER_BLUENOISE_STRENGTH",
        NULL, NULL,
        "-d bluenoise:strength must be a floating point value.",
        bluenoise_strength, bluenoise_strength_override),
    SIXEL_REGISTRY_ENCODER_FLOAT(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE,
        "gradient_factor", 'G',
        "SIXEL_DITHER_BLUENOISE_GRADIENT_FACTOR", NULL, NULL,
        "-d bluenoise:gradient_factor must be a floating point value.",
        bluenoise_gradient_factor,
        bluenoise_gradient_factor_override),
    SIXEL_REGISTRY_ENCODER_INT_PAIR(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE,
        "phase", 'P', "SIXEL_DITHER_BLUENOISE_PHASE", NULL, NULL,
        "-d bluenoise:phase must be in form X,Y.",
        bluenoise_phase_x, bluenoise_phase_y, bluenoise_phase_override),
    SIXEL_REGISTRY_ENCODER_INT(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE,
        "seed", 'S', "SIXEL_DITHER_BLUENOISE_SEED", NULL, NULL,
        "-d bluenoise:seed must be a 32-bit signed integer.",
        bluenoise_seed, bluenoise_seed_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE,
        "channel", 'C', "SIXEL_DITHER_BLUENOISE_CHANNEL", NULL, NULL,
        g_bluenoise_channel_choices,
        bluenoise_channel_rgb, bluenoise_channel_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_BLUENOISE,
        "size", 'Z', "SIXEL_DITHER_BLUENOISE_SIZE", NULL, NULL,
        g_bluenoise_size_choices,
        bluenoise_size, bluenoise_size_override),

    SIXEL_REGISTRY_ENCODER_MIRROR_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL,
        "merge", 'G', "SIXEL_PALETTE_FINAL_MERGE", NULL, NULL,
        g_quantize_merge_choices,
        quantize_model_merge_mode, quantize_model_merge_override,
        final_merge_mode),
    SIXEL_REGISTRY_ENCODER_DOUBLE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL,
        "merge_oversplit", 'O', "SIXEL_PALETTE_OVERSPLIT_FACTOR",
        NULL, NULL, 1.0, 3.0,
        "-Q merge_oversplit must be in range 1.0-3.0.",
        quantize_model_merge_oversplit,
        quantize_model_merge_oversplit_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL,
        "merge_lloyd", 'L',
        "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT",
        NULL, NULL, 1.0, 30.0, 1,
        "-Q merge_lloyd must be 0 or in range 1-30.",
        quantize_model_merge_lloyd,
        quantize_model_merge_lloyd_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL,
        "cover", 'C', "SIXEL_PALETTE_COVER", NULL, NULL,
        g_palette_cover_choices,
        quantize_model_cover, quantize_model_cover_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL,
        "cover_grow", 'V', "SIXEL_PALETTE_COVER_GROW", NULL, NULL,
        quantize_model_cover_grow, quantize_model_cover_grow_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL,
        "cover_mode", 'W', "SIXEL_PALETTE_COVER_MODE", NULL, NULL,
        g_palette_cover_mode_choices,
        quantize_model_cover_mode, quantize_model_cover_mode_override),
    SIXEL_REGISTRY_ENCODER_DIRECT_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_HECKBERT,
        "profile", 'P', "SIXEL_PALETTE_HECKBERT_PROFILE", NULL, NULL,
        g_heckbert_profile_choices,
        quantize_model_heckbert_profile),

    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "inittype", 'I', "SIXEL_PALETTE_KMEANS_INITTYPE", NULL, NULL,
        g_kmeans_init_type_choices,
        quantize_model_kmeans_init_type,
        quantize_model_kmeans_init_override),
    SIXEL_REGISTRY_ENCODER_DOUBLE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "threshold", 'T', "SIXEL_PALETTE_KMEANS_THRESHOLD", NULL, NULL,
        0.0, 0.5, "-Q threshold must be in range 0.0-0.5.",
        quantize_model_kmeans_threshold,
        quantize_model_kmeans_threshold_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "binning", 'B', "SIXEL_PALETTE_KMEANS_BINNING", NULL, NULL,
        g_kmeans_binning_choices,
        quantize_model_kmeans_binning_mode,
        quantize_model_kmeans_binning_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "binbits", 'N', "SIXEL_PALETTE_KMEANS_BINBITS", NULL, NULL,
        4.0, 8.0, 0, "-Q binbits must be in range 4-8.",
        quantize_model_kmeans_binbits,
        quantize_model_kmeans_binbits_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "mapping", 'M', "SIXEL_PALETTE_KMEANS_MAPPING", NULL, NULL,
        g_kmeans_mapping_choices,
        quantize_model_kmeans_mapping_mode,
        quantize_model_kmeans_mapping_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "softdist", 'D', "SIXEL_PALETTE_KMEANS_SOFTDIST", NULL, NULL,
        g_kmeans_softdist_choices,
        quantize_model_kmeans_softdist_mode,
        quantize_model_kmeans_softdist_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "autoratio", 'R', "SIXEL_PALETTE_KMEANS_AUTORATIO", NULL, NULL,
        1.0, 1048576.0, 0,
        "-Q autoratio must be in range 1-1048576.",
        quantize_model_kmeans_autoratio,
        quantize_model_kmeans_autoratio_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "feedback", 'F', "SIXEL_PALETTE_KMEANS_FEEDBACK", NULL, NULL,
        quantize_model_kmeans_feedback_mode,
        quantize_model_kmeans_feedback_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "prune", 'P', "SIXEL_PALETTE_KMEANS_PRUNE", NULL, NULL,
        g_kmeans_prune_choices,
        quantize_model_kmeans_prune_policy,
        quantize_model_kmeans_prune_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "seed", 'S', "SIXEL_PALETTE_KMEANS_SEED", NULL, NULL,
        0.0, 4294967295.0, 0,
        "-Q seed must be in range 0-4294967295.",
        quantize_model_kmeans_seed,
        quantize_model_kmeans_seed_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "restarts", 'E', "SIXEL_PALETTE_KMEANS_RESTARTS", NULL, NULL,
        1.0, 32.0, 0, "-Q restarts must be in range 1-32.",
        quantize_model_kmeans_restarts,
        quantize_model_kmeans_restarts_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "iter", 'A', "SIXEL_PALETTE_KMEANS_ITER", NULL, NULL,
        1.0, 100.0, 0, "-Q iter must be in range 1-100.",
        quantize_model_kmeans_iter,
        quantize_model_kmeans_iter_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "iter_max", 'X', "SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX",
        NULL, NULL, 1.0, 100.0, 0,
        "-Q iter_max must be in range 1-100.",
        quantize_model_kmeans_iter_max,
        quantize_model_kmeans_iter_max_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "miniter", 'U', "SIXEL_PALETTE_KMEANS_MINITER", NULL, NULL,
        1.0, 100.0, 1, "-Q miniter must be 0 or in range 1-100.",
        quantize_model_kmeans_miniter,
        quantize_model_kmeans_miniter_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "polish_iter", 'H', "SIXEL_PALETTE_KMEANS_POLISH_ITER",
        NULL, NULL, 1.0, 16.0, 1,
        "-Q polish_iter must be 0 or in range 1-16.",
        quantize_model_kmeans_polish_iter,
        quantize_model_kmeans_polish_iter_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "feedback_slots", 'K', "SIXEL_PALETTE_KMEANS_FEEDBACK_SLOTS",
        NULL, NULL, 1.0, 16.0, 0,
        "-Q feedback_slots must be in range 1-16.",
        quantize_model_kmeans_feedback_slots,
        quantize_model_kmeans_feedback_slots_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "feedback_interval", 'J',
        "SIXEL_PALETTE_KMEANS_FEEDBACK_INTERVAL", NULL, NULL,
        1.0, 64.0, 0,
        "-Q feedback_interval must be in range 1-64.",
        quantize_model_kmeans_feedback_interval,
        quantize_model_kmeans_feedback_interval_override),

    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "algo", 'A', "SIXEL_PALETTE_KMEDOIDS_ALGO", NULL, NULL,
        g_kmedoids_algo_choices,
        quantize_model_kmedoids_algo,
        quantize_model_kmedoids_algo_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "seed", 'S', "SIXEL_PALETTE_KMEDOIDS_SEED", NULL, NULL,
        0.0, 4294967295.0, 0,
        "-Q seed must be in range 0-4294967295.",
        quantize_model_kmedoids_seed,
        quantize_model_kmedoids_seed_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "iter", 'I', "SIXEL_PALETTE_KMEDOIDS_ITER", NULL, NULL,
        1.0, 64.0, 0, "-Q iter must be in range 1-64.",
        quantize_model_kmedoids_iter,
        quantize_model_kmedoids_iter_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "sample", 'M', "SIXEL_PALETTE_KMEDOIDS_SAMPLE", NULL, NULL,
        64.0, 1048576.0, 1,
        "-Q sample must be 0 or in range 64-1048576.",
        quantize_model_kmedoids_sample,
        quantize_model_kmedoids_sample_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "clara_trials", 'T', "SIXEL_PALETTE_KMEDOIDS_CLARA_TRIALS",
        NULL, NULL, 1.0, 32.0, 0,
        "-Q clara_trials must be in range 1-32.",
        quantize_model_kmedoids_clara_trials,
        quantize_model_kmedoids_clara_trials_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "clara_sample", 'K', "SIXEL_PALETTE_KMEDOIDS_CLARA_SAMPLE",
        NULL, NULL, 64.0, 1048576.0, 1,
        "-Q clara_sample must be 0 or in range 64-1048576.",
        quantize_model_kmedoids_clara_sample,
        quantize_model_kmedoids_clara_sample_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "clarans_local", 'J', "SIXEL_PALETTE_KMEDOIDS_CLARANS_LOCAL",
        NULL, NULL, 1.0, 32.0, 0,
        "-Q clarans_local must be in range 1-32.",
        quantize_model_kmedoids_clarans_local,
        quantize_model_kmedoids_clarans_local_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "clarans_neighbors", 'N',
        "SIXEL_PALETTE_KMEDOIDS_CLARANS_NEIGHBORS", NULL, NULL,
        1.0, 5000000.0, 1,
        "-Q clarans_neighbors must be 0 or in range 1-5000000.",
        quantize_model_kmedoids_clarans_neighbors,
        quantize_model_kmedoids_clarans_neighbors_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "bandit_iter", 'D', "SIXEL_PALETTE_KMEDOIDS_BANDIT_ITER",
        NULL, NULL, 1.0, 64.0, 0,
        "-Q bandit_iter must be in range 1-64.",
        quantize_model_kmedoids_bandit_iter,
        quantize_model_kmedoids_bandit_iter_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "bandit_candidates", 'E',
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_CANDIDATES", NULL, NULL,
        8.0, 4096.0, 0,
        "-Q bandit_candidates must be in range 8-4096.",
        quantize_model_kmedoids_bandit_candidates,
        quantize_model_kmedoids_bandit_candidates_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "bandit_batch", 'X', "SIXEL_PALETTE_KMEDOIDS_BANDIT_BATCH",
        NULL, NULL, 8.0, 4096.0, 0,
        "-Q bandit_batch must be in range 8-4096.",
        quantize_model_kmedoids_bandit_batch,
        quantize_model_kmedoids_bandit_batch_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "histbits", 'H', "SIXEL_PALETTE_KMEDOIDS_HISTBITS", NULL, NULL,
        3.0, 6.0, 0, "-Q histbits must be in range 3-6.",
        quantize_model_kmedoids_histbits,
        quantize_model_kmedoids_histbits_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "point_budget", 'B', "SIXEL_PALETTE_KMEDOIDS_POINT_BUDGET",
        NULL, NULL, 64.0, 16384.0, 0,
        "-Q point_budget must be in range 64-16384.",
        quantize_model_kmedoids_point_budget,
        quantize_model_kmedoids_point_budget_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "rare_keep", 'R', "SIXEL_PALETTE_KMEDOIDS_RARE_KEEP", NULL, NULL,
        0.0, 1024.0, 0, "-Q rare_keep must be in range 0-1024.",
        quantize_model_kmedoids_rare_keep,
        quantize_model_kmedoids_rare_keep_override),
    SIXEL_REGISTRY_ENCODER_DOUBLE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "prune_mass", 'U', "SIXEL_PALETTE_KMEDOIDS_PRUNE_MASS",
        NULL, NULL, 0.9, 1.0,
        "-Q prune_mass must be in range 0.900-1.000.",
        quantize_model_kmedoids_prune_mass,
        quantize_model_kmedoids_prune_mass_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "auction", 'Q', "SIXEL_PALETTE_KMEDOIDS_AUCTION", NULL, NULL,
        quantize_model_kmedoids_auction,
        quantize_model_kmedoids_auction_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "auction_shortlist", 'Y',
        "SIXEL_PALETTE_KMEDOIDS_AUCTION_SHORTLIST", NULL, NULL,
        2.0, 8.0, 0, "-Q auction_shortlist must be in range 2-8.",
        quantize_model_kmedoids_auction_shortlist,
        quantize_model_kmedoids_auction_shortlist_override),

    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "algo", 'A', "SIXEL_PALETTE_KCENTER_ALGO", NULL, NULL,
        g_kcenter_algo_choices,
        quantize_model_kcenter_algo,
        quantize_model_kcenter_algo_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "profile", 'P', "SIXEL_PALETTE_KCENTER_PROFILE", NULL, NULL,
        g_kcenter_profile_choices,
        quantize_model_kcenter_profile,
        quantize_model_kcenter_profile_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "seed", 'S', "SIXEL_PALETTE_KCENTER_SEED", NULL, NULL,
        0.0, 4294967295.0, 0,
        "-Q seed must be in range 0-4294967295.",
        quantize_model_kcenter_seed,
        quantize_model_kcenter_seed_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "auto_policy", 'Q', "SIXEL_PALETTE_KCENTER_AUTO_POLICY",
        NULL, NULL, g_kcenter_auto_policy_choices,
        quantize_model_kcenter_auto_policy,
        quantize_model_kcenter_auto_policy_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "auto_fft_threshold", 'F',
        "SIXEL_PALETTE_KCENTER_AUTO_FFT_THRESHOLD", NULL, NULL,
        256.0, 65536.0, 0,
        "-Q auto_fft_threshold must be in range 256-65536.",
        quantize_model_kcenter_auto_fft_threshold,
        quantize_model_kcenter_auto_fft_threshold_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "space_policy", 'E', "SIXEL_PALETTE_KCENTER_SPACE_POLICY",
        NULL, NULL, g_kcenter_space_policy_choices,
        quantize_model_kcenter_space_policy,
        quantize_model_kcenter_space_policy_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "candidate_policy", 'Z',
        "SIXEL_PALETTE_KCENTER_CANDIDATE_POLICY",
        NULL, NULL, g_kcenter_candidate_policy_choices,
        quantize_model_kcenter_candidate_policy,
        quantize_model_kcenter_candidate_policy_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "restarts", 'X', "SIXEL_PALETTE_KCENTER_RESTARTS", NULL, NULL,
        1.0, 32.0, 0, "-Q restarts must be in range 1-32.",
        quantize_model_kcenter_restarts,
        quantize_model_kcenter_restarts_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "init_seeds", 'N', "SIXEL_PALETTE_KCENTER_INIT_SEEDS", NULL, NULL,
        1.0, 8.0, 0, "-Q init_seeds must be in range 1-8.",
        quantize_model_kcenter_init_seeds,
        quantize_model_kcenter_init_seeds_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "iter", 'I', "SIXEL_PALETTE_KCENTER_ITER", NULL, NULL,
        1.0, 64.0, 0, "-Q iter must be in range 1-64.",
        quantize_model_kcenter_iter,
        quantize_model_kcenter_iter_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "histbits", 'H', "SIXEL_PALETTE_KCENTER_HISTBITS", NULL, NULL,
        3.0, 6.0, 0, "-Q histbits must be in range 3-6.",
        quantize_model_kcenter_histbits,
        quantize_model_kcenter_histbits_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "point_budget", 'B', "SIXEL_PALETTE_KCENTER_POINT_BUDGET",
        NULL, NULL, 64.0, 16384.0, 1,
        "-Q point_budget must be 0 or in range 64-16384.",
        quantize_model_kcenter_point_budget,
        quantize_model_kcenter_point_budget_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "rare_keep", 'R', "SIXEL_PALETTE_KCENTER_RARE_KEEP", NULL, NULL,
        1.0, 2048.0, 1,
        "-Q rare_keep must be 0 or in range 1-2048.",
        quantize_model_kcenter_rare_keep,
        quantize_model_kcenter_rare_keep_override),
    SIXEL_REGISTRY_ENCODER_DOUBLE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "prune_mass", 'U', "SIXEL_PALETTE_KCENTER_PRUNE_MASS", NULL, NULL,
        0.9, 1.0, "-Q prune_mass must be in range 0.900-1.000.",
        quantize_model_kcenter_prune_mass,
        quantize_model_kcenter_prune_mass_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "budget_policy", 'D', "SIXEL_PALETTE_KCENTER_BUDGET_POLICY",
        NULL, NULL, g_kcenter_budget_policy_choices,
        quantize_model_kcenter_budget_policy,
        quantize_model_kcenter_budget_policy_override),
    SIXEL_REGISTRY_ENCODER_DOUBLE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "budget_scale", 'Y', "SIXEL_PALETTE_KCENTER_BUDGET_SCALE",
        NULL, NULL, 0.25, 4.0,
        "-Q budget_scale must be in range 0.25-4.00.",
        quantize_model_kcenter_budget_scale,
        quantize_model_kcenter_budget_scale_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "swap_topk", 'K', "SIXEL_PALETTE_KCENTER_SWAP_TOPK", NULL, NULL,
        1.0, 16.0, 0, "-Q swap_topk must be in range 1-16.",
        quantize_model_kcenter_swap_topk,
        quantize_model_kcenter_swap_topk_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "swap_update", 'M', "SIXEL_PALETTE_KCENTER_SWAP_UPDATE",
        NULL, NULL, g_kcenter_swap_update_choices,
        quantize_model_kcenter_swap_update,
        quantize_model_kcenter_swap_update_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "swap_patience", 'T', "SIXEL_PALETTE_KCENTER_SWAP_PATIENCE",
        NULL, NULL, 1.0, 8.0, 1,
        "-Q swap_patience must be 0 or in range 1-8.",
        quantize_model_kcenter_swap_patience,
        quantize_model_kcenter_swap_patience_override),
    SIXEL_REGISTRY_ENCODER_DOUBLE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "swap_min_gain", 'J', "SIXEL_PALETTE_KCENTER_SWAP_MIN_GAIN",
        NULL, NULL, 0.0, 8.0,
        "-Q swap_min_gain must be in range 0.0-8.0.",
        quantize_model_kcenter_swap_min_gain,
        quantize_model_kcenter_swap_min_gain_override),

    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_5BIT,
        "shared_instance", 'S', "SIXEL_LOOKUP_5BIT_SHARED_INSTANCE",
        NULL, NULL,
        lut_policy_shared_instance,
        lut_policy_shared_instance_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_6BIT,
        "shared_instance", 'S', "SIXEL_LOOKUP_6BIT_SHARED_INSTANCE",
        NULL, NULL,
        lut_policy_shared_instance,
        lut_policy_shared_instance_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_CERTLUT,
        "shared_instance", 'S', "SIXEL_LOOKUP_CERTLUT_SHARED_INSTANCE",
        NULL, NULL,
        lut_policy_shared_instance,
        lut_policy_shared_instance_override),

#if HAVE_LIBPNG
    SIXEL_REGISTRY_BOUND_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBPNG,
        "cms_engine", 'E', "SIXEL_LOADER_LIBPNG_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices,
        g_loader_cms_environment_choices,
        SIXEL_SUBOPTION_TARGET_LOADER,
        sixel_loader_suboptions_t,
        libpng_cms_engine),
    SIXEL_REGISTRY_BOUND_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBPNG,
        "orientation", 'O', "SIXEL_LOADER_LIBPNG_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL,
        SIXEL_SUBOPTION_TARGET_LOADER,
        sixel_loader_suboptions_t,
        libpng_enable_orientation),
#endif
#if HAVE_JPEG
    SIXEL_REGISTRY_BOUND_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBJPEG,
        "cms_engine", 'E', "SIXEL_LOADER_LIBJPEG_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices,
        g_loader_cms_environment_choices,
        SIXEL_SUBOPTION_TARGET_LOADER,
        sixel_loader_suboptions_t,
        libjpeg_cms_engine),
    SIXEL_REGISTRY_BOUND_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBJPEG,
        "orientation", 'O', "SIXEL_LOADER_LIBJPEG_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL,
        SIXEL_SUBOPTION_TARGET_LOADER,
        sixel_loader_suboptions_t,
        libjpeg_enable_orientation),
#endif
#if HAVE_WEBP
    SIXEL_REGISTRY_BOUND_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBWEBP,
        "cms_engine", 'E', "SIXEL_LOADER_LIBWEBP_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices,
        g_loader_cms_environment_choices,
        SIXEL_SUBOPTION_TARGET_LOADER,
        sixel_loader_suboptions_t,
        libwebp_cms_engine),
    SIXEL_REGISTRY_BOUND_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBWEBP,
        "orientation", 'O', "SIXEL_LOADER_LIBWEBP_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL,
        SIXEL_SUBOPTION_TARGET_LOADER,
        sixel_loader_suboptions_t,
        libwebp_enable_orientation),
#endif
#if HAVE_COREGRAPHICS
    SIXEL_REGISTRY_BOUND_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_COREGRAPHICS,
        "orientation", 'O', "SIXEL_LOADER_COREGRAPHICS_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL,
        SIXEL_SUBOPTION_TARGET_LOADER,
        sixel_loader_suboptions_t,
        coregraphics_enable_orientation),
#endif
#if HAVE_LIBTIFF
    SIXEL_REGISTRY_BOUND_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBTIFF,
        "cms_engine", 'E', "SIXEL_LOADER_LIBTIFF_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices,
        g_loader_cms_environment_choices,
        SIXEL_SUBOPTION_TARGET_LOADER,
        sixel_loader_suboptions_t,
        libtiff_cms_engine),
#endif
    SIXEL_REGISTRY_BOUND_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "cms_engine", 'E', "SIXEL_LOADER_BUILTIN_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices,
        g_loader_cms_environment_choices,
        SIXEL_SUBOPTION_TARGET_LOADER,
        sixel_loader_suboptions_t,
        builtin_cms_engine),
    SIXEL_REGISTRY_BOUND_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "orientation", 'O', "SIXEL_LOADER_BUILTIN_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL,
        SIXEL_SUBOPTION_TARGET_LOADER,
        sixel_loader_suboptions_t,
        builtin_enable_orientation),
    SIXEL_REGISTRY_BOUND_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "bmp_info40_mode", 'B', "SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE",
        NULL, NULL, g_loader_bmp_info40_mode_choices,
        g_loader_bmp_environment_choices,
        SIXEL_SUBOPTION_TARGET_LOADER,
        sixel_loader_suboptions_t,
        builtin_bmp_info40_mode),
#if HAVE_WIC
    SIXEL_REGISTRY_BOUND_UINT_VALUE_MESSAGE(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_WIC,
        "ico_minsize", 'I', "SIXEL_LOADER_WIC_ICO_MINSIZE", NULL,
        "SIXEL_LODER_WIC_ICO_MINSIZE", 1.0, 2147483647.0, 0, 1,
        "invalid wic suboption value \"",
        "\" for key \"ico_minsize\"; expected a positive integer.",
        SIXEL_SUBOPTION_TARGET_LOADER,
        sixel_loader_suboptions_t,
        wic_ico_minsize),
#endif
};

static sixel_option_argument_schema_t const g_options[] = {
    {
        SIXEL_OPTION_SCHEMA_DEQUANTIZE,
        SIXEL_OPTFLAG_DEQUANTIZE,
        "dequantize",
        g_dequantize_values,
        SIXEL_REGISTRY_ARRAY_LENGTH(g_dequantize_values)
    },
    {
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        SIXEL_OPTFLAG_DIFFUSION,
        "--diffusion",
        g_diffusion_values,
        SIXEL_REGISTRY_ARRAY_LENGTH(g_diffusion_values)
    },
    {
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        SIXEL_OPTFLAG_QUANTIZE_MODEL,
        "--quantize-model",
        g_quantize_values,
        SIXEL_REGISTRY_ARRAY_LENGTH(g_quantize_values)
    },
    {
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        SIXEL_OPTFLAG_LUT_POLICY,
        "--lookup-policy",
        g_lookup_values,
        SIXEL_REGISTRY_ARRAY_LENGTH(g_lookup_values)
    },
    {
        SIXEL_OPTION_SCHEMA_LOADERS,
        SIXEL_OPTFLAG_LOADERS,
        "--loaders",
        g_loader_values,
        SIXEL_REGISTRY_ARRAY_LENGTH(g_loader_values)
    }
};

static int
sixel_option_registry_key_applies(
    sixel_suboption_key_t const *key,
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def)
{
    if (key == NULL || schema == NULL || base_def == NULL) {
        return 0;
    }
    if (key->option_id != schema->option_id) {
        return 0;
    }
    return key->base_def == NULL || key->base_def == base_def;
}

sixel_option_argument_schema_t const *
sixel_option_registry_get(sixel_option_schema_id_t option_id)
{
    size_t index;

    index = 0u;
    while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_options)) {
        if (g_options[index].option_id == option_id) {
            return g_options + index;
        }
        ++index;
    }

    return NULL;
}

size_t
sixel_option_registry_suboption_count(
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def)
{
    size_t index;
    size_t count;

    index = 0u;
    count = 0u;
    while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
        if (sixel_option_registry_key_applies(g_suboptions + index,
                                              schema,
                                              base_def)) {
            ++count;
        }
        ++index;
    }

    return count;
}

sixel_suboption_key_t const *
sixel_option_registry_suboption_at(
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    size_t requested_index)
{
    size_t index;
    size_t matched_index;
    size_t common_count;
    size_t specific_count;
    size_t common_offset;
    int request_common;

    index = 0u;
    matched_index = 0u;
    common_count = 0u;
    specific_count = 0u;
    common_offset = 0u;
    request_common = 0;

    if (schema == NULL || base_def == NULL) {
        return NULL;
    }

    while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
        if (g_suboptions[index].option_id == schema->option_id) {
            if (g_suboptions[index].base_def == NULL) {
                ++common_count;
            } else if (g_suboptions[index].base_def == base_def) {
                ++specific_count;
            }
        }
        ++index;
    }

    common_offset = base_def->common_suboption_offset;
    if (common_offset > specific_count) {
        common_offset = specific_count;
    }
    if (requested_index >= common_offset &&
        requested_index < common_offset + common_count) {
        request_common = 1;
        requested_index -= common_offset;
    } else {
        if (requested_index >= common_offset + common_count) {
            requested_index -= common_count;
        }
    }

    index = 0u;
    while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
        if (g_suboptions[index].option_id == schema->option_id &&
            ((request_common && g_suboptions[index].base_def == NULL) ||
             (!request_common &&
              g_suboptions[index].base_def == base_def))) {
            if (matched_index == requested_index) {
                return g_suboptions + index;
            }
            ++matched_index;
        }
        ++index;
    }

    return NULL;
}

sixel_suboption_key_t const *
sixel_option_registry_suboption_by_environment(char const *name)
{
    size_t index;

    index = 0u;
    if (name == NULL || name[0] == '\0') {
        return NULL;
    }
    while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
        if (strcmp(g_suboptions[index].env_name, name) == 0) {
            return g_suboptions + index;
        }
        ++index;
    }

    return NULL;
}

/*
 * Environment names are part of the registry contract.  Keep their syntax
 * independent of the host shell so a malformed row fails on every platform.
 */
static int
sixel_option_registry_environment_name_is_valid(
    char const *name,
    int required)
{
    size_t index;

    index = 0u;
    if (name == NULL) {
        return required == 0;
    }
    if (name[0] < 'A' || name[0] > 'Z') {
        return 0;
    }
    index = 1u;
    while (name[index] != '\0') {
        if ((name[index] < 'A' || name[index] > 'Z') &&
            (name[index] < '0' || name[index] > '9') &&
            name[index] != '_') {
            return 0;
        }
        ++index;
    }

    return 1;
}

int
sixel_option_registry_validate(void)
{
    size_t option_index;
    size_t base_index;
    size_t key_index;
    size_t previous_index;
    size_t key_count;
    size_t suboption_index;
    size_t previous_suboption_index;
    sixel_option_argument_schema_t const *schema;
    sixel_option_value_schema_t const *base_def;
    sixel_suboption_key_t const *key;
    sixel_suboption_key_t const *previous;

    option_index = 0u;
    base_index = 0u;
    key_index = 0u;
    previous_index = 0u;
    key_count = 0u;
    suboption_index = 0u;
    previous_suboption_index = 0u;
    schema = NULL;
    base_def = NULL;
    key = NULL;
    previous = NULL;

    while (suboption_index <
           SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
        key = g_suboptions + suboption_index;
        if (!sixel_option_registry_environment_name_is_valid(
                key->env_name,
                1) ||
            !sixel_option_registry_environment_name_is_valid(
                key->env_fallback_name,
                0) ||
            !sixel_option_registry_environment_name_is_valid(
                key->env_legacy_name,
                0)) {
            return 0;
        }
        previous_suboption_index = 0u;
        while (previous_suboption_index < suboption_index) {
            previous = g_suboptions + previous_suboption_index;
            if (strcmp(previous->env_name, key->env_name) == 0) {
                return 0;
            }
            ++previous_suboption_index;
        }
        ++suboption_index;
    }

    while (option_index < SIXEL_REGISTRY_ARRAY_LENGTH(g_options)) {
        schema = g_options + option_index;
        base_index = 0u;
        while (base_index < schema->value_count) {
            base_def = schema->values + base_index;
            key_count = sixel_option_registry_suboption_count(schema,
                                                               base_def);
            key_index = 0u;
            while (key_index < key_count) {
                key = sixel_option_registry_suboption_at(schema,
                                                         base_def,
                                                         key_index);
                if (key == NULL || key->name == NULL ||
                    key->name[0] == '\0' || key->env_name == NULL ||
                    key->env_name[0] == '\0' ||
                    key->short_name < 'A' || key->short_name > 'Z' ||
                    key->binding.target_class ==
                        SIXEL_SUBOPTION_TARGET_NONE ||
                    key->binding.value_offset ==
                        SIXEL_SUBOPTION_OFFSET_NONE) {
                    return 0;
                }
                if ((key->environment_choices == NULL) !=
                    (key->environment_choice_count == 0u)) {
                    return 0;
                }
                if (key->environment_choice_count > 0u &&
                    key->value_kind != SIXEL_SUBOPTION_VALUE_CHOICE) {
                    return 0;
                }
                if (key->value_kind == SIXEL_SUBOPTION_VALUE_BOOLEAN &&
                    (key->choices != NULL || key->choice_count != 0u ||
                     key->environment_choices != NULL ||
                     key->environment_choice_count != 0u ||
                     key->binding.storage_kind !=
                         SIXEL_SUBOPTION_STORAGE_INT)) {
                    return 0;
                }
                if (key->environment_clamp_maximum &&
                    (key->value_kind != SIXEL_SUBOPTION_VALUE_UINT ||
                     !key->has_maximum)) {
                    return 0;
                }
                if (key->binding.storage_kind ==
                        SIXEL_SUBOPTION_STORAGE_INT_PAIR &&
                    key->binding.second_value_offset ==
                        SIXEL_SUBOPTION_OFFSET_NONE) {
                    return 0;
                }
                previous_index = 0u;
                while (previous_index < key_index) {
                    previous = sixel_option_registry_suboption_at(
                        schema,
                        base_def,
                        previous_index);
                    if (previous == NULL ||
                        strcmp(previous->name, key->name) == 0 ||
                        previous->short_name == key->short_name) {
                        return 0;
                    }
                    ++previous_index;
                }
                ++key_index;
            }
            ++base_index;
        }
        ++option_index;
    }

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
