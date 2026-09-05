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
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if defined(SIXEL_ENABLE_THREADS) && SIXEL_ENABLE_THREADS
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
     && !defined(WITH_WINPTHREAD)
#  if !defined(WIN32_LEAN_AND_MEAN)
#   define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
# else
#  include <pthread.h>
# endif
#endif

#include "cms.h"
#include "compat_stub.h"
#include "cpu.h"
#include "dither-interframe-method.h"
#include "decoder.h"
#include "encoder.h"
#include "fromhdr.h"
#include "loader-common.h"
#include "lookup-policy.h"
#include "options-registry.h"
#include "palette-common-cover.h"
#include "palette-common-snap.h"
#include "palette-heckbert.h"
#include "palette-kcenter.h"
#include "palette-kmeans.h"
#include "palette-kmedoids.h"
#include "threading.h"

#define SIXEL_REGISTRY_ARRAY_LENGTH(array_) \
    (sizeof(array_) / sizeof((array_)[0]))
#define SIXEL_REGISTRY_BASE_BIT(index_) (1ULL << (unsigned int)(index_))

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#define SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE \
    SIXEL_OPTION_SCOPE_ENCODER
#define SIXEL_REGISTRY_DECODER_CONSUMER_SCOPE \
    SIXEL_OPTION_SCOPE_DECODER
#define SIXEL_REGISTRY_CONSUMER_SCOPE_FOR_TARGET(target_) \
    (((target_) == SIXEL_SUBOPTION_TARGET_DECODER || \
      (target_) == SIXEL_SUBOPTION_TARGET_DEQUANTIZE) \
         ? SIXEL_REGISTRY_DECODER_CONSUMER_SCOPE \
         : SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE)

/*
 * Make an incompatible binding a compile-time error where the compiler can
 * compare C types.  The portable fallback still rejects width mismatches;
 * runtime registry validation additionally checks storage and range rules.
 */
#if defined(__GNUC__) || defined(__clang__)
# define SIXEL_REGISTRY_MEMBER_TYPE_MATCHES(type_, field_, value_type_) \
    __builtin_types_compatible_p( \
        __typeof__(((type_ *)0)->field_), value_type_)
#else
# define SIXEL_REGISTRY_MEMBER_TYPE_MATCHES(type_, field_, value_type_) \
    (sizeof(((type_ *)0)->field_) == sizeof(value_type_))
#endif

#define SIXEL_REGISTRY_CHECKED_OFFSET(type_, field_, value_type_) \
    (offsetof(type_, field_) + \
     0u * sizeof(char[SIXEL_REGISTRY_MEMBER_TYPE_MATCHES( \
                         type_, field_, value_type_) ? 1 : -1]))

#define SIXEL_REGISTRY_NO_BINDING \
    { \
        SIXEL_SUBOPTION_TARGET_NONE, SIXEL_SUBOPTION_STORAGE_INT, \
        SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE, \
        SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE, NULL \
    }

#define SIXEL_REGISTRY_CHOICE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_) \
    { \
        (optflag_), (base_), SIXEL_OPTION_SCOPE_ALL, (name_), (short_), \
        (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, 0.0, 0.0, 0, 0, \
        0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, NULL, NULL, \
        SIXEL_REGISTRY_NO_BINDING, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_TYPED_CHOICE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    target_, type_, field_) \
    { \
        (optflag_), (base_), \
        SIXEL_REGISTRY_CONSUMER_SCOPE_FOR_TARGET(target_), \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, 0.0, 0.0, 0, 0, \
        0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, NULL, NULL, \
        { \
            (target_), SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(type_, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_1(field_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_TYPED_CHOICE_ENV( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    environment_choices_, target_, type_, field_) \
    { \
        (optflag_), (base_), \
        SIXEL_REGISTRY_CONSUMER_SCOPE_FOR_TARGET(target_), \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), (environment_choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(environment_choices_), 0.0, 0.0, 0, \
        0, 0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, NULL, NULL, \
        { \
            (target_), SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(type_, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_1(field_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_TYPED_CHOICE_LIST( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    target_, type_, field_) \
    { \
        (optflag_), (base_), \
        SIXEL_REGISTRY_CONSUMER_SCOPE_FOR_TARGET(target_), \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE_LIST, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, 0.0, 0.0, 0, 0, \
        0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, \
        "ordered choice list contains an invalid value.", NULL, \
        { \
            (target_), SIXEL_SUBOPTION_STORAGE_UINT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(type_, field_, unsigned int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_1(field_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_TYPED_BOOLEAN( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, target_, type_, \
    field_) \
    { \
        (optflag_), (base_), \
        SIXEL_REGISTRY_CONSUMER_SCOPE_FOR_TARGET(target_), \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_BOOLEAN, NULL, 0u, NULL, 0u, 0.0, \
        1.0, 1, 1, 1, SIXEL_SUBOPTION_ENV_RANGE_REJECT, \
        "boolean suboption must be 0 or 1.", NULL, \
        { \
            (target_), SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(type_, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_1(field_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_NUMBER( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, kind_, \
    minimum_, maximum_, has_minimum_, has_maximum_, allow_zero_, message_, \
    suffix_) \
    { \
        (optflag_), (base_), SIXEL_OPTION_SCOPE_ALL, (name_), (short_), \
        (env_), (fallback_), \
        (legacy_), (kind_), NULL, 0u, NULL, 0u, (minimum_), (maximum_), \
        (has_minimum_), (has_maximum_), (allow_zero_), \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, (message_), (suffix_), \
        SIXEL_REGISTRY_NO_BINDING, NULL, 0ULL \
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

#define SIXEL_REGISTRY_TYPED_UINT_VALUE_MESSAGE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, environment_range_, message_, suffix_, target_, \
    type_, field_) \
    { \
        (optflag_), (base_), \
        SIXEL_REGISTRY_CONSUMER_SCOPE_FOR_TARGET(target_), \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_UINT, NULL, 0u, NULL, 0u, \
        (minimum_), \
        (maximum_), 1, 1, (allow_zero_), (environment_range_), (message_), \
        (suffix_), \
        { \
            (target_), SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(type_, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_1(field_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_TYPED_UINT( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, target_, type_, field_) \
    SIXEL_REGISTRY_TYPED_UINT_VALUE_MESSAGE( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
        maximum_, allow_zero_, SIXEL_SUBOPTION_ENV_RANGE_REJECT, message_, \
        NULL, target_, type_, field_)

#define SIXEL_REGISTRY_TYPED_DOUBLE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, message_, target_, type_, field_) \
    { \
        (optflag_), (base_), \
        SIXEL_REGISTRY_CONSUMER_SCOPE_FOR_TARGET(target_), \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_DOUBLE, NULL, 0u, NULL, 0u, \
        (minimum_), (maximum_), 1, 1, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, (message_), NULL, \
        { \
            (target_), SIXEL_SUBOPTION_STORAGE_DOUBLE, \
            SIXEL_REGISTRY_CHECKED_OFFSET(type_, field_, double), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_1(field_) \
        }, NULL, 0ULL \
    }

/*
 * Fix each non-encoder binding to its owning target and structure.  Registry
 * rows therefore cannot pair a valid field offset with the wrong target
 * class, even when both structures happen to use members of the same width.
 */
#define SIXEL_REGISTRY_DEQUANTIZE_CHOICE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    field_) \
    SIXEL_REGISTRY_TYPED_CHOICE( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
        SIXEL_SUBOPTION_TARGET_DEQUANTIZE, sixel_dequantize_options_t, field_)

#define SIXEL_REGISTRY_DEQUANTIZE_UINT( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, field_) \
    SIXEL_REGISTRY_TYPED_UINT( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
        maximum_, allow_zero_, message_, SIXEL_SUBOPTION_TARGET_DEQUANTIZE, \
        sixel_dequantize_options_t, field_)

#define SIXEL_REGISTRY_LOADER_CHOICE_ENV( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    environment_choices_, field_) \
    SIXEL_REGISTRY_TYPED_CHOICE_ENV( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
        environment_choices_, SIXEL_SUBOPTION_TARGET_LOADER, \
        sixel_loader_suboptions_t, field_)

#define SIXEL_REGISTRY_LOADER_CHOICE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    field_) \
    SIXEL_REGISTRY_TYPED_CHOICE( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
        SIXEL_SUBOPTION_TARGET_LOADER, sixel_loader_suboptions_t, field_)

#define SIXEL_REGISTRY_LOADER_CHOICE_LIST( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    field_) \
    SIXEL_REGISTRY_TYPED_CHOICE_LIST( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
        SIXEL_SUBOPTION_TARGET_LOADER, sixel_loader_suboptions_t, field_)

#define SIXEL_REGISTRY_LOADER_BOOLEAN( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, field_) \
    SIXEL_REGISTRY_TYPED_BOOLEAN( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_TARGET_LOADER, sixel_loader_suboptions_t, field_)

#define SIXEL_REGISTRY_LOADER_UINT( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, suffix_, field_) \
    SIXEL_REGISTRY_TYPED_UINT_VALUE_MESSAGE( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
        maximum_, allow_zero_, SIXEL_SUBOPTION_ENV_RANGE_REJECT, \
        message_, suffix_, \
        SIXEL_SUBOPTION_TARGET_LOADER, sixel_loader_suboptions_t, field_)

/* Preserve loader environments that historically parsed through strtol(). */
#define SIXEL_REGISTRY_LOADER_UINT_ENV_REJECT_SIGNED( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, suffix_, field_) \
    SIXEL_REGISTRY_TYPED_UINT_VALUE_MESSAGE( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
        maximum_, allow_zero_, \
        SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG, message_, suffix_, \
        SIXEL_SUBOPTION_TARGET_LOADER, sixel_loader_suboptions_t, field_)

#define SIXEL_REGISTRY_LOADER_UINT_ENV_CLAMP_MAXIMUM( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, suffix_, field_) \
    SIXEL_REGISTRY_TYPED_UINT_VALUE_MESSAGE( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
        maximum_, allow_zero_, SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM, \
        message_, suffix_, \
        SIXEL_SUBOPTION_TARGET_LOADER, sixel_loader_suboptions_t, field_)

#define SIXEL_REGISTRY_LOADER_DOUBLE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, message_, field_) \
    SIXEL_REGISTRY_TYPED_DOUBLE( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
        maximum_, message_, SIXEL_SUBOPTION_TARGET_LOADER, \
        sixel_loader_suboptions_t, field_)

#define SIXEL_REGISTRY_LOADER_UINT_ENV_CLAMP_MAXIMUM_DIGITS( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, suffix_, trace_topic_, field_) \
    { \
        (optflag_), (base_), SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE, \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_UINT, NULL, 0u, NULL, 0u, \
        (minimum_), (maximum_), 1, 1, (allow_zero_), \
        SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM | \
            SIXEL_SUBOPTION_ENV_RANGE_PARSE_DIGITS_ONLY, \
        (message_), (suffix_), \
        { \
            SIXEL_SUBOPTION_TARGET_LOADER, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_loader_suboptions_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_1(field_) \
        }, \
        (trace_topic_), 0ULL \
    }

#define SIXEL_REGISTRY_LOADER_SIZE_ENV_ERROR( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, message_, \
    suffix_, field_, override_) \
    { \
        (optflag_), (base_), SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE, \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_SIZE, NULL, 0u, NULL, 0u, \
        0.0, 0.0, 0, 0, 1, SIXEL_SUBOPTION_ENV_RANGE_REJECT, \
        (message_), (suffix_), \
        { \
            SIXEL_SUBOPTION_TARGET_LOADER, SIXEL_SUBOPTION_STORAGE_SIZE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_loader_suboptions_t, field_, size_t), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_loader_suboptions_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

/* Encoder-owned size controls are copied into per-frame dither state. */
#define SIXEL_REGISTRY_ENCODER_SIZE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, \
    environment_range_, message_, field_, override_) \
    { \
        (optflag_), (base_), SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE, \
        (name_), (short_), (env_), (fallback_), (legacy_), \
        SIXEL_SUBOPTION_VALUE_SIZE, NULL, 0u, NULL, 0u, 0.0, 0.0, 0, 0, 1, \
        (environment_range_), (message_), NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_SIZE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_encoder_t, field_, size_t), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_encoder_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_ENCODER_POSITIVE_SIZE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, message_, \
    field_, override_) \
    { \
        (optflag_), (base_), SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE, \
        (name_), (short_), (env_), (fallback_), (legacy_), \
        SIXEL_SUBOPTION_VALUE_SIZE, NULL, 0u, NULL, 0u, 1.0, 0.0, 1, 0, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, (message_), NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_SIZE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_encoder_t, field_, size_t), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_encoder_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

/* Decoder-owned suboptions bind directly to request-local decoder state. */
#define SIXEL_REGISTRY_DECODER_SIZE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, \
    environment_range_, message_, field_, override_) \
    { \
        (optflag_), (base_), SIXEL_REGISTRY_DECODER_CONSUMER_SCOPE, \
        (name_), (short_), (env_), (fallback_), (legacy_), \
        SIXEL_SUBOPTION_VALUE_SIZE, NULL, 0u, NULL, 0u, 0.0, 0.0, 0, 0, 1, \
        (environment_range_), (message_), NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_DECODER, SIXEL_SUBOPTION_STORAGE_SIZE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_decoder_t, field_, size_t), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_decoder_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

/* Runtime policy is process-wide, while each row keeps consumer visibility. */
#define SIXEL_REGISTRY_RUNTIME_NUMBER( \
    optflag_, base_, name_, short_, env_, kind_, minimum_, maximum_, \
    has_minimum_, has_maximum_, range_, message_, scope_, storage_, \
    value_type_, field_, override_) \
    { \
        (optflag_), (base_), (scope_), (name_), (short_), (env_), NULL, \
        NULL, \
        (kind_), NULL, 0u, NULL, 0u, (minimum_), (maximum_), \
        (has_minimum_), (has_maximum_), 0, (range_), (message_), NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_RUNTIME, (storage_), \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_runtime_policy_options_t, field_, value_type_), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_runtime_policy_options_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_RUNTIME_SIZE( \
    optflag_, base_, name_, short_, env_, range_, message_, scope_, field_, \
    override_) \
    SIXEL_REGISTRY_RUNTIME_NUMBER( \
        optflag_, base_, name_, short_, env_, \
        SIXEL_SUBOPTION_VALUE_SIZE, 0.0, 0.0, 0, 0, range_, message_, \
        scope_, SIXEL_SUBOPTION_STORAGE_SIZE, size_t, field_, override_)

#define SIXEL_REGISTRY_RUNTIME_UINT( \
    optflag_, base_, name_, short_, env_, minimum_, maximum_, range_, \
    message_, scope_, field_, override_) \
    SIXEL_REGISTRY_RUNTIME_NUMBER( \
        optflag_, base_, name_, short_, env_, \
        SIXEL_SUBOPTION_VALUE_UINT, minimum_, maximum_, 1, 1, range_, \
        message_, scope_, SIXEL_SUBOPTION_STORAGE_UINT, unsigned int, \
        field_, override_)

#define SIXEL_REGISTRY_RUNTIME_INT( \
    optflag_, base_, name_, short_, env_, minimum_, maximum_, range_, \
    message_, scope_, field_, override_) \
    SIXEL_REGISTRY_RUNTIME_NUMBER( \
        optflag_, base_, name_, short_, env_, \
        SIXEL_SUBOPTION_VALUE_INT, minimum_, maximum_, 1, 1, range_, \
        message_, scope_, SIXEL_SUBOPTION_STORAGE_INT, int, field_, \
        override_)

#define SIXEL_REGISTRY_RUNTIME_CHOICE( \
    optflag_, base_, name_, short_, env_, choices_, range_, scope_, field_, \
    override_) \
    { \
        (optflag_), (base_), (scope_), (name_), \
        (short_), (env_), NULL, NULL, SIXEL_SUBOPTION_VALUE_CHOICE, \
        (choices_), SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, \
        0.0, 0.0, 0, 0, 0, (range_), NULL, NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_RUNTIME, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_runtime_policy_options_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_runtime_policy_options_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_DIAGNOSTICS_BOOLEAN( \
    optflag_, base_, name_, short_, env_, scope_, field_, override_) \
    { \
        (optflag_), (base_), (scope_), (name_), (short_), (env_), NULL, \
        NULL, SIXEL_SUBOPTION_VALUE_BOOLEAN, NULL, 0u, NULL, 0u, \
        0.0, 1.0, 1, 1, 0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, NULL, NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_DIAGNOSTICS, \
            SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_diagnostics_policy_options_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_diagnostics_policy_options_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_DIAGNOSTICS_STRING( \
    optflag_, base_, name_, short_, env_, scope_, field_, override_) \
    { \
        (optflag_), (base_), (scope_), (name_), (short_), (env_), NULL, \
        NULL, SIXEL_SUBOPTION_VALUE_STRING, NULL, 0u, NULL, 0u, \
        0.0, 0.0, 0, 0, 0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, \
        "string suboption must not be empty.", NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_DIAGNOSTICS, \
            SIXEL_SUBOPTION_STORAGE_STRING, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_diagnostics_policy_options_t, field_, char const *), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_diagnostics_policy_options_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_DIAGNOSTICS_INT( \
    optflag_, base_, name_, short_, env_, scope_, minimum_, maximum_, \
    range_, message_, field_, override_) \
    { \
        (optflag_), (base_), (scope_), (name_), (short_), (env_), NULL, \
        NULL, SIXEL_SUBOPTION_VALUE_INT, NULL, 0u, NULL, 0u, \
        (minimum_), (maximum_), 1, 1, 0, (range_), (message_), NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_DIAGNOSTICS, \
            SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_diagnostics_policy_options_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_diagnostics_policy_options_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_CLIPBOARD_STRING( \
    optflag_, base_, name_, short_, env_, field_, override_) \
    { \
        (optflag_), (base_), SIXEL_OPTION_SCOPE_ALL, (name_), (short_), \
        (env_), NULL, NULL, SIXEL_SUBOPTION_VALUE_STRING, NULL, 0u, NULL, \
        0u, 0.0, 0.0, 0, 0, 0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, \
        "string suboption must not be empty.", NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_CLIPBOARD, \
            SIXEL_SUBOPTION_STORAGE_STRING, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_clipboard_policy_options_t, field_, char const *), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_clipboard_policy_options_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

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
        (optflag_), (base_), SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE, \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, 0.0, 0.0, 0, 0, \
        0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, NULL, NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

/* Preserve legacy strtol() syntax while restricting the numeric choices. */
#define SIXEL_REGISTRY_ENCODER_CHOICE_ENV_PARSE_SIGNED_LONG( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    field_, override_) \
    { \
        (optflag_), (base_), SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE, \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, 0.0, 0.0, 0, 0, \
        0, SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG, NULL, NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

/* One semantic row may belong to an explicit set of structured bases. */
#define SIXEL_REGISTRY_ENCODER_MULTI_CHOICE( \
    optflag_, base_mask_, name_, short_, env_, fallback_, legacy_, choices_, \
    field_, override_) \
    { \
        (optflag_), NULL, SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE, \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, 0.0, 0.0, 0, 0, \
        0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, NULL, NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, (base_mask_) \
    }

#define SIXEL_REGISTRY_ENCODER_CHOICE_ENV( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    environment_choices_, field_, override_) \
    { \
        (optflag_), (base_), SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE, \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), (environment_choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(environment_choices_), 0.0, 0.0, 0, \
        0, 0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, NULL, NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_ENCODER_BOOLEAN( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, field_, \
    override_) \
    { \
        (optflag_), (base_), SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE, \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_BOOLEAN, NULL, 0u, NULL, 0u, 0.0, \
        1.0, 1, 1, 1, SIXEL_SUBOPTION_ENV_RANGE_REJECT, \
        "boolean suboption must be 0 or 1.", NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_2(field_, override_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_ENCODER_DIRECT_CHOICE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    field_) \
    { \
        (optflag_), (base_), SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE, \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, 0.0, 0.0, 0, 0, \
        0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, NULL, NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_OFFSET_NONE, SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_SUBOPTION_BINDING_ID_1(field_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_ENCODER_MIRROR_CHOICE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, choices_, \
    field_, override_, mirror_) \
    { \
        (optflag_), (base_), SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE, \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), SIXEL_SUBOPTION_VALUE_CHOICE, (choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(choices_), NULL, 0u, 0.0, 0.0, 0, 0, \
        0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, NULL, NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, SIXEL_SUBOPTION_STORAGE_INT, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, field_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, override_, int), \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, mirror_, int), \
            SIXEL_SUBOPTION_BINDING_ID_3(field_, override_, mirror_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_ENCODER_NUMBER( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, kind_, \
    minimum_, maximum_, has_minimum_, has_maximum_, allow_zero_, \
    environment_range_, message_, storage_, value_type_, field_, second_, \
    override_, binding_id_) \
    { \
        (optflag_), (base_), SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE, \
        (name_), (short_), (env_), (fallback_), \
        (legacy_), (kind_), NULL, 0u, NULL, 0u, (minimum_), (maximum_), \
        (has_minimum_), (has_maximum_), (allow_zero_), \
        (environment_range_), (message_), NULL, \
        { \
            SIXEL_SUBOPTION_TARGET_ENCODER, (storage_), \
            SIXEL_REGISTRY_CHECKED_OFFSET( \
                sixel_encoder_t, field_, value_type_), \
            (second_), \
            SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, override_, int), \
            SIXEL_SUBOPTION_OFFSET_NONE, (binding_id_) \
        }, NULL, 0ULL \
    }

#define SIXEL_REGISTRY_ENCODER_UINT( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_UINT, minimum_, maximum_, 1, 1, allow_zero_, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, message_, \
        SIXEL_SUBOPTION_STORAGE_UINT, unsigned int, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_, \
        SIXEL_SUBOPTION_BINDING_ID_2(field_, override_))

#define SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_SIGNED( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_UINT, minimum_, maximum_, 1, 1, allow_zero_, \
        SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM | \
            SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM | \
            SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG, \
        message_, SIXEL_SUBOPTION_STORAGE_UINT, unsigned int, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_, \
        SIXEL_SUBOPTION_BINDING_ID_2(field_, override_))

#define SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_UNSIGNED( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_UINT, minimum_, maximum_, 1, 1, allow_zero_, \
        SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM | \
            SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM | \
            SIXEL_SUBOPTION_ENV_RANGE_CLAMP_UINT_WIDTH, \
        message_, SIXEL_SUBOPTION_STORAGE_UINT, unsigned int, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_, \
        SIXEL_SUBOPTION_BINDING_ID_2(field_, override_))

#define SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_UINT, minimum_, maximum_, 1, 1, allow_zero_, \
        SIXEL_SUBOPTION_ENV_RANGE_CLAMP_POSITIVE_MINIMUM | \
            SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM | \
            SIXEL_SUBOPTION_ENV_RANGE_REJECT_UINT_WIDTH | \
            SIXEL_SUBOPTION_ENV_RANGE_PARSE_UNSIGNED_LONG, \
        message_, SIXEL_SUBOPTION_STORAGE_UINT, unsigned int, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_, \
        SIXEL_SUBOPTION_BINDING_ID_2(field_, override_))

#define SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_MAXIMUM_SIGNED( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_UINT, minimum_, maximum_, 1, 1, allow_zero_, \
        SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM | \
            SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG, \
        message_, SIXEL_SUBOPTION_STORAGE_UINT, unsigned int, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_, \
        SIXEL_SUBOPTION_BINDING_ID_2(field_, override_))

#define SIXEL_REGISTRY_ENCODER_UINT_ENV_REJECT_UNSIGNED_LONG( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, allow_zero_, message_, field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_UINT, minimum_, maximum_, 1, 1, allow_zero_, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT_UINT_WIDTH | \
            SIXEL_SUBOPTION_ENV_RANGE_PARSE_UNSIGNED_LONG, \
        message_, SIXEL_SUBOPTION_STORAGE_UINT, unsigned int, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_, \
        SIXEL_SUBOPTION_BINDING_ID_2(field_, override_))

#define SIXEL_REGISTRY_ENCODER_INT( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, message_, \
    field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_INT, 0.0, 0.0, 0, 0, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, message_, \
        SIXEL_SUBOPTION_STORAGE_INT, int, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_, \
        SIXEL_SUBOPTION_BINDING_ID_2(field_, override_))

#define SIXEL_REGISTRY_ENCODER_FLOAT( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, message_, \
    field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_FLOAT, 0.0, 0.0, 0, 0, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, message_, \
        SIXEL_SUBOPTION_STORAGE_FLOAT, float, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_, \
        SIXEL_SUBOPTION_BINDING_ID_2(field_, override_))

#define SIXEL_REGISTRY_ENCODER_DOUBLE( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, message_, field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_DOUBLE, minimum_, maximum_, 1, 1, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, message_, \
        SIXEL_SUBOPTION_STORAGE_DOUBLE, double, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_, \
        SIXEL_SUBOPTION_BINDING_ID_2(field_, override_))

#define SIXEL_REGISTRY_ENCODER_DOUBLE_ENV_CLAMP( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, message_, field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_DOUBLE, minimum_, maximum_, 1, 1, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM | \
            SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM, \
        message_, SIXEL_SUBOPTION_STORAGE_DOUBLE, double, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_, \
        SIXEL_SUBOPTION_BINDING_ID_2(field_, override_))

#define SIXEL_REGISTRY_ENCODER_SCALED_U8( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, message_, field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_SCALED_U8, minimum_, maximum_, 1, 1, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, message_, \
        SIXEL_SUBOPTION_STORAGE_INT, int, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_, \
        SIXEL_SUBOPTION_BINDING_ID_2(field_, override_))

#define SIXEL_REGISTRY_ENCODER_SCALED_U8_ENV_CLAMP( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, minimum_, \
    maximum_, message_, field_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_SCALED_U8, minimum_, maximum_, 1, 1, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM | \
            SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM, \
        message_, SIXEL_SUBOPTION_STORAGE_INT, int, field_, \
        SIXEL_SUBOPTION_OFFSET_NONE, override_, \
        SIXEL_SUBOPTION_BINDING_ID_2(field_, override_))

#define SIXEL_REGISTRY_ENCODER_INT_PAIR( \
    optflag_, base_, name_, short_, env_, fallback_, legacy_, message_, \
    field_, second_, override_) \
    SIXEL_REGISTRY_ENCODER_NUMBER( \
        optflag_, base_, name_, short_, env_, fallback_, legacy_, \
        SIXEL_SUBOPTION_VALUE_INT_PAIR, 0.0, 0.0, 0, 0, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, message_, \
        SIXEL_SUBOPTION_STORAGE_INT_PAIR, int, field_, \
        SIXEL_REGISTRY_CHECKED_OFFSET(sixel_encoder_t, second_, int), \
        override_, SIXEL_SUBOPTION_BINDING_ID_3(field_, second_, override_))

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

static sixel_option_value_schema_t const g_palette_sampling_values[] = {
    {
        "auto", SIXEL_PALETTE_SAMPLING_AUTO, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "full-frame", SIXEL_PALETTE_SAMPLING_FULL_FRAME, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "adaptive-grid", SIXEL_PALETTE_SAMPLING_ADAPTIVE_GRID, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    }
};

static sixel_option_value_schema_t const g_palette_binning_values[] = {
    {
        "auto", SIXEL_PALETTE_BINNING_AUTO, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "none", SIXEL_PALETTE_BINNING_NONE, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "exact", SIXEL_PALETTE_BINNING_EXACT, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "hard", SIXEL_PALETTE_BINNING_HARD, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "soft", SIXEL_PALETTE_BINNING_SOFT, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    }
};

static sixel_option_value_schema_t const g_merge_policy_values[] = {
    { "auto", SIXEL_FINAL_MERGE_AUTO, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "none", SIXEL_FINAL_MERGE_NONE, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "ward", SIXEL_FINAL_MERGE_WARD, 0u, SIXEL_OPTION_BASE_POLICY_NONE }
};

static sixel_option_value_schema_t const g_cover_policy_values[] = {
    { "auto", SIXEL_PALETTE_COVER_AUTO, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "off", SIXEL_PALETTE_COVER_OFF, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    {
        "corners", SIXEL_PALETTE_COVER_CORNERS, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "faces", SIXEL_PALETTE_COVER_FACES, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "edges", SIXEL_PALETTE_COVER_EDGES, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "all", SIXEL_PALETTE_COVER_EDGES, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    { "0", SIXEL_PALETTE_COVER_OFF, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "1", SIXEL_PALETTE_COVER_AUTO, 0u, SIXEL_OPTION_BASE_POLICY_NONE }
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

#define SIXEL_LOOKUP_BASE_SET_DENSE \
    (SIXEL_REGISTRY_BASE_BIT(SIXEL_LOOKUP_BASE_5BIT) | \
     SIXEL_REGISTRY_BASE_BIT(SIXEL_LOOKUP_BASE_6BIT))

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

static sixel_option_value_schema_t const g_precision_values[] = {
    {
        "auto", SIXEL_OPTION_PRECISION_AUTO, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "8bit", SIXEL_OPTION_PRECISION_8BIT, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "float32", SIXEL_OPTION_PRECISION_FLOAT32, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    }
};

static sixel_suboption_choice_t const g_precision_environment_choices[] = {
    { "0", SIXEL_OPTION_PRECISION_8BIT },
    { "1", SIXEL_OPTION_PRECISION_FLOAT32 }
};

static sixel_option_value_schema_t const g_threads_values[] = {
    { "auto", 0, 0u, SIXEL_OPTION_BASE_POLICY_NONE }
};

static sixel_option_value_schema_t const g_gpu_policy_values[] = {
    { "off", SIXEL_GPU_POLICY_OFF, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "auto", SIXEL_GPU_POLICY_AUTO, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "force", SIXEL_GPU_POLICY_FORCE, 0u, SIXEL_OPTION_BASE_POLICY_NONE }
};

static sixel_option_value_schema_t const g_runtime_policy_values[] = {
    { "auto", SIXEL_SIMD_LEVEL_NEON, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    {
        "scalar", SIXEL_SIMD_LEVEL_SCALAR, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    { "none", SIXEL_SIMD_LEVEL_SCALAR, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "sse2", SIXEL_SIMD_LEVEL_SSE2, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "avx", SIXEL_SIMD_LEVEL_AVX, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "neon", SIXEL_SIMD_LEVEL_NEON, 0u, SIXEL_OPTION_BASE_POLICY_NONE }
};

static sixel_option_value_schema_t const g_diagnostics_values[] = {
    {
        "human", SIXEL_DIAGNOSTICS_MODE_HUMAN, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "code", SIXEL_DIAGNOSTICS_MODE_CODE, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    }
};

enum {
    SIXEL_CLIPBOARD_BASE_SYSTEM = 0,
    SIXEL_CLIPBOARD_BASE_FILE
};

static sixel_option_value_schema_t const g_clipboard_policy_values[] = {
    {
        "system", SIXEL_CLIPBOARD_BACKEND_SYSTEM, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "file", SIXEL_CLIPBOARD_BACKEND_FILE, 1u,
        SIXEL_OPTION_BASE_POLICY_NONE
    }
};

static sixel_suboption_choice_t const
g_clipboard_policy_environment_choices[] = {
    { "system", SIXEL_CLIPBOARD_BACKEND_SYSTEM },
    { "file", SIXEL_CLIPBOARD_BACKEND_FILE },
    { "fake", SIXEL_CLIPBOARD_BACKEND_FILE }
};

static sixel_option_value_schema_t const g_terminal_policy_values[] = {
    { "0", 0, 0u, SIXEL_OPTION_BASE_POLICY_NONE },
    { "1", 1, 0u, SIXEL_OPTION_BASE_POLICY_NONE }
};

static sixel_suboption_choice_t const g_runtime_resize_choices[] = {
    { "preserve", SIXEL_RUNTIME_RESIZE_PRECISION_PRESERVE },
    { "linear", SIXEL_RUNTIME_RESIZE_PRECISION_LINEAR32 },
    { "float", SIXEL_RUNTIME_RESIZE_PRECISION_FLOAT_WORK }
};

static sixel_option_value_schema_t const g_transparent_policy_values[] = {
    {
        "composite", SIXEL_TRANSPARENT_POLICY_COMPOSITE, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "transparent", SIXEL_TRANSPARENT_POLICY_BACKGROUND, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "background", SIXEL_TRANSPARENT_POLICY_BACKGROUND, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "clear", SIXEL_TRANSPARENT_POLICY_BACKGROUND, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "p2-0", SIXEL_TRANSPARENT_POLICY_BACKGROUND, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "p20", SIXEL_TRANSPARENT_POLICY_BACKGROUND, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "keep", SIXEL_TRANSPARENT_POLICY_KEEP, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "keep-destination", SIXEL_TRANSPARENT_POLICY_KEEP, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "previous", SIXEL_TRANSPARENT_POLICY_KEEP, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "p2-1", SIXEL_TRANSPARENT_POLICY_KEEP, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "p21", SIXEL_TRANSPARENT_POLICY_KEEP, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    }
};

static sixel_option_value_schema_t const g_6delta_error_values[] = {
    {
        "diffuse", SIXEL_6DELTA_ERROR_DIFFUSE, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    },
    {
        "skip", SIXEL_6DELTA_ERROR_SKIP, 0u,
        SIXEL_OPTION_BASE_POLICY_NONE
    }
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

static sixel_suboption_choice_t const g_lookup_packing_choices[] = {
    { "linear", SIXEL_LOOKUP_PACK_LINEAR },
    { "morton", SIXEL_LOOKUP_PACK_MORTON },
    { "hilbert", SIXEL_LOOKUP_PACK_HILBERT }
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

static sixel_suboption_choice_t const g_palette_cover_mode_choices[] = {
    { "hard", SIXEL_PALETTE_COVER_MODE_HARD },
    { "soft", SIXEL_PALETTE_COVER_MODE_SOFT }
};

static sixel_suboption_choice_t const g_palette_snap_target_choices[] = {
    { "auto", SIXEL_PALETTE_SNAP_POLICY_NEAREST },
    { "nearest", SIXEL_PALETTE_SNAP_POLICY_NEAREST },
    { "reversible", SIXEL_PALETTE_SNAP_POLICY_REVERSIBLE }
};

static sixel_suboption_choice_t const g_palette_snap_timing_choices[] = {
    { "once", SIXEL_PALETTE_SNAP_TIMING_ONCE },
    { "polish", SIXEL_PALETTE_SNAP_TIMING_POLISH },
    { "merge", SIXEL_PALETTE_SNAP_TIMING_MERGE },
    { "resolve", SIXEL_PALETTE_SNAP_TIMING_RESOLVE },
    { "all", SIXEL_PALETTE_SNAP_TIMING_ALL }
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

static sixel_suboption_choice_t const g_fhedt_resolution_choices[] = {
    { "64", 64 },
    { "128", 128 },
    { "256", 256 }
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

static sixel_suboption_choice_t const g_loader_background_policy_choices[] = {
    { "file_first", SIXEL_LOADER_BACKGROUND_POLICY_FILE_FIRST },
    { "explicit_first", SIXEL_LOADER_BACKGROUND_POLICY_EXPLICIT_FIRST }
};

static sixel_suboption_choice_t const
g_loader_background_colorspace_choices[] = {
    { "gamma", SIXEL_COLORSPACE_GAMMA },
    { "linear", SIXEL_COLORSPACE_LINEAR }
};

static sixel_suboption_choice_t const
g_loader_cms_target_colorspace_choices[] = {
    { "gamma", SIXEL_COLORSPACE_GAMMA },
    { "linear", SIXEL_COLORSPACE_LINEAR },
    { "cielab", SIXEL_COLORSPACE_CIELAB },
    { "oklab", SIXEL_COLORSPACE_OKLAB },
    { "din99d", SIXEL_COLORSPACE_DIN99D }
};

static sixel_suboption_choice_t const g_loader_cms_intent_choices[] = {
    { "perceptual", SIXEL_CMS_INTENT_PERCEPTUAL },
    { "relative", SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC },
    { "relative_colorimetric", SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC },
    { "saturation", SIXEL_CMS_INTENT_SATURATION },
    { "absolute", SIXEL_CMS_INTENT_ABSOLUTE_COLORIMETRIC },
    { "absolute_colorimetric", SIXEL_CMS_INTENT_ABSOLUTE_COLORIMETRIC }
};

static sixel_suboption_choice_t const g_loader_hdr_fallback_choices[] = {
    { "linear-srgb", SIXEL_BUILTIN_HDR_FALLBACK_LINEAR_SRGB },
    { "srgb", SIXEL_BUILTIN_HDR_FALLBACK_SRGB }
};

static sixel_suboption_choice_t const g_loader_hdr_fallback_env_choices[] = {
    { "linear-srgb", SIXEL_BUILTIN_HDR_FALLBACK_LINEAR_SRGB },
    { "linear_srgb", SIXEL_BUILTIN_HDR_FALLBACK_LINEAR_SRGB },
    { "linearsrgb", SIXEL_BUILTIN_HDR_FALLBACK_LINEAR_SRGB },
    { "linear", SIXEL_BUILTIN_HDR_FALLBACK_LINEAR_SRGB },
    { "srgb", SIXEL_BUILTIN_HDR_FALLBACK_SRGB },
    { "gamma-srgb", SIXEL_BUILTIN_HDR_FALLBACK_SRGB },
    { "gamma_srgb", SIXEL_BUILTIN_HDR_FALLBACK_SRGB },
    { "gammasrgb", SIXEL_BUILTIN_HDR_FALLBACK_SRGB },
    { "gamma", SIXEL_BUILTIN_HDR_FALLBACK_SRGB }
};

static sixel_suboption_choice_t const g_loader_hdr_tonemap_choices[] = {
    { "none", SIXEL_BUILTIN_HDR_TONEMAP_NONE },
    { "reinhard", SIXEL_BUILTIN_HDR_TONEMAP_REINHARD }
};

/*
 * This is the sole authoritative suboption registry.  A NULL base pointer
 * means that the row is shared by every base value of the owning option.
 * Common rows keep orthogonal controls identical without copying definitions
 * into every quantizer or diffusion method.
 */
static sixel_suboption_key_t const g_suboptions[] = {
    SIXEL_REGISTRY_DIAGNOSTICS_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIAGNOSTICS, NULL,
        "quiet", 'Q', "SIXEL_DIAG_MODE_QUIET",
        SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE,
        quiet, quiet_override),
    SIXEL_REGISTRY_DIAGNOSTICS_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIAGNOSTICS, NULL,
        "prefix_suggestions", 'P', "SIXEL_OPTION_PREFIX_SUGGESTIONS",
        SIXEL_OPTION_SCOPE_ALL,
        prefix_suggestions, prefix_suggestions_override),
    SIXEL_REGISTRY_DIAGNOSTICS_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIAGNOSTICS, NULL,
        "fuzzy_suggestions", 'F', "SIXEL_OPTION_FUZZY_SUGGESTIONS",
        SIXEL_OPTION_SCOPE_ALL,
        fuzzy_suggestions, fuzzy_suggestions_override),
    SIXEL_REGISTRY_DIAGNOSTICS_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIAGNOSTICS, NULL,
        "path_suggestions", 'S', "SIXEL_OPTION_PATH_SUGGESTIONS",
        SIXEL_OPTION_SCOPE_ALL,
        path_suggestions, path_suggestions_override),
    SIXEL_REGISTRY_DIAGNOSTICS_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIAGNOSTICS, NULL,
        "force_colors", 'C', "SIXEL_STATUS_FORCE_COLORS",
        SIXEL_OPTION_SCOPE_ALL,
        force_colors, force_colors_override),
    SIXEL_REGISTRY_DIAGNOSTICS_STRING(
        SIXEL_OPTION_SCHEMA_DIAGNOSTICS, NULL,
        "trace_topic", 'T', "SIXEL_TRACE_TOPIC",
        SIXEL_OPTION_SCOPE_ALL,
        trace_topic, trace_topic_override),
    SIXEL_REGISTRY_DIAGNOSTICS_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIAGNOSTICS, NULL,
        "handoff_trace", 'H', "SIXEL_ENCODE_HANDOFF_TRACE_MINIMAL",
        SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE,
        handoff_trace, handoff_trace_override),
    SIXEL_REGISTRY_DIAGNOSTICS_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIAGNOSTICS, NULL,
        "psd_trace", 'D', "SIXEL_PSD_TRACE_ONLY",
        SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE,
        psd_trace, psd_trace_override),
    SIXEL_REGISTRY_DIAGNOSTICS_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIAGNOSTICS, NULL,
        "psd_header_only", 'E', "SIXEL_PSD_TRACE_HEADER_ONLY",
        SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE,
        psd_header_only, psd_header_only_override),
    SIXEL_REGISTRY_DIAGNOSTICS_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIAGNOSTICS, NULL,
        "abort_trace", 'A', "SIXEL_ABORT_TRACE",
        SIXEL_OPTION_SCOPE_ALL,
        abort_trace, abort_trace_override),
    SIXEL_REGISTRY_CLIPBOARD_STRING(
        SIXEL_OPTION_SCHEMA_CLIPBOARD_POLICY,
        g_clipboard_policy_values + SIXEL_CLIPBOARD_BASE_FILE,
        "directory", 'D', "SIXEL_CLIPBOARD_FILE_DIR",
        directory, directory_override),
    SIXEL_REGISTRY_DIAGNOSTICS_INT(
        SIXEL_OPTION_SCHEMA_DIAGNOSTICS, NULL,
        "log_lines", 'N', "SIXEL_LOG_LINES",
        SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE,
        1.0, (double)INT_MAX,
        SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM |
            SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM |
            SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG_PREFIX,
        "diagnostics log_lines must be in range 1..2147483647.",
        log_lines, log_lines_override),

    SIXEL_REGISTRY_RUNTIME_SIZE(
        SIXEL_OPTION_SCHEMA_RUNTIME_POLICY, NULL,
        "colorspace_min", 'C',
        "SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS",
        SIXEL_SUBOPTION_ENV_RANGE_CLAMP_SIZE_WIDTH,
        "runtime colorspace_min must be an unsigned integer.",
        SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE,
        colorspace_parallel_min_pixels,
        colorspace_parallel_min_pixels_override),
    SIXEL_REGISTRY_RUNTIME_UINT(
        SIXEL_OPTION_SCHEMA_RUNTIME_POLICY, NULL,
        "parallel_factor", 'F', "SIXEL_PARALLEL_FACTOR",
        1.0, (double)INT_MAX,
        SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG,
        "runtime parallel_factor must be a positive integer.",
        SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE,
        parallel_factor, parallel_factor_override),
    SIXEL_REGISTRY_RUNTIME_INT(
        SIXEL_OPTION_SCHEMA_RUNTIME_POLICY, NULL,
        "parallel_skew", 'K', "SIXEL_PARALLEL_SKEW",
        -20.0, 20.0,
        SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM |
            SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM,
        "runtime parallel_skew must be in range -20..20.",
        SIXEL_REGISTRY_DECODER_CONSUMER_SCOPE,
        parallel_skew, parallel_skew_override),
    SIXEL_REGISTRY_RUNTIME_CHOICE(
        SIXEL_OPTION_SCHEMA_RUNTIME_POLICY, NULL,
        "resize_precision", 'R',
        "SIXEL_PLANNER_RESIZE_PRECISION_MODE",
        g_runtime_resize_choices,
        SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG_PREFIX,
        SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE,
        resize_precision, resize_precision_override),
    SIXEL_REGISTRY_RUNTIME_SIZE(
        SIXEL_OPTION_SCHEMA_RUNTIME_POLICY, NULL,
        "scale_min_bytes", 'B', "SIXEL_SCALE_PARALLEL_MIN_BYTES",
        SIXEL_SUBOPTION_ENV_RANGE_CLAMP_SIZE_WIDTH,
        "runtime scale_min_bytes must be an unsigned integer.",
        SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE,
        scale_parallel_min_bytes,
        scale_parallel_min_bytes_override),

    SIXEL_REGISTRY_ENCODER_SIZE(
        SIXEL_OPTION_SCHEMA_GPU_POLICY, NULL,
        "palette_threshold", 'P', "SIXEL_GPU_PALETTE_THRESHOLD",
        NULL, NULL,
        SIXEL_SUBOPTION_ENV_RANGE_SATURATE_UNSIGNED_LONG,
        "GPU palette threshold must be an unsigned integer.",
        gpu_palette_threshold,
        gpu_palette_threshold_override),
    SIXEL_REGISTRY_DECODER_SIZE(
        SIXEL_OPTION_SCHEMA_GPU_POLICY, NULL,
        "dequant_threshold", 'D', "SIXEL_GPU_DEQUANT_THRESHOLD",
        NULL, NULL,
        SIXEL_SUBOPTION_ENV_RANGE_SATURATE_UNSIGNED_LONG,
        "GPU dequant threshold must be an unsigned integer.",
        gpu_dequant_threshold,
        gpu_dequant_threshold_override),

    SIXEL_REGISTRY_DEQUANTIZE_CHOICE(
        SIXEL_OPTION_SCHEMA_DEQUANTIZE,
        g_dequantize_values + SIXEL_DEQUANTIZE_BASE_LSO_UNDITHER,
        "variant", 'V', "SIXEL_DEQUANTIZE_LSO_VARIANT", NULL, NULL,
        g_dequantize_lso_variant_choices,
        method),
    SIXEL_REGISTRY_DEQUANTIZE_UINT(
        SIXEL_OPTION_SCHEMA_DEQUANTIZE,
        g_dequantize_values + SIXEL_DEQUANTIZE_BASE_SELECTIVE_BLUR,
        "threshold", 'T',
        "SIXEL_DEQUANTIZE_SELECTIVE_BLUR_THRESHOLD", NULL, NULL,
        0.0, 441.0, 0,
        "selective_blur threshold must be an integer in range 0..441.",
        selective_blur_threshold),

    SIXEL_REGISTRY_ENCODER_DIRECT_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION, NULL,
        "scan", 'N', "SIXEL_DITHER_SCAN", NULL, NULL,
        g_diffusion_scan_choices, method_for_scan),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_MAXIMUM_SIGNED(
        SIXEL_OPTION_SCHEMA_DIFFUSION, NULL,
        "band_overwrap", 'O', "SIXEL_DITHER_PARALLEL_BAND_OVERWRAP",
        NULL, NULL, 0.0, (double)INT_MAX, 1,
        "-d band_overwrap must be in range 0-2147483647.",
        dither_parallel_band_overwrap,
        dither_parallel_band_overwrap_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_MAXIMUM_SIGNED(
        SIXEL_OPTION_SCHEMA_DIFFUSION, NULL,
        "band_width", 'B', "SIXEL_DITHER_PARALLEL_BAND_WIDTH",
        NULL, NULL, 1.0, (double)INT_MAX, 0,
        "-d band_width must be in range 1-2147483647.",
        dither_parallel_band_width,
        dither_parallel_band_width_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_MAXIMUM_SIGNED(
        SIXEL_OPTION_SCHEMA_DIFFUSION, NULL,
        "threads_max", 'J', "SIXEL_DITHER_PARALLEL_THREADS_MAX",
        NULL, NULL, 1.0, (double)INT_MAX, 0,
        "-d threads_max must be in range 1-2147483647.",
        dither_parallel_threads_max,
        dither_parallel_threads_max_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_DIFFUSION, NULL,
        "pin_threads", 'I', "SIXEL_DITHER_PIN_THREADS", NULL, NULL,
        dither_pin_threads, dither_pin_threads_override),
    SIXEL_REGISTRY_ENCODER_DIRECT_CHOICE(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_SIERRA,
        "variant", 'V', "SIXEL_DITHER_SIERRA_VARIANT", NULL, NULL,
        g_sierra_variant_choices, method_for_diffuse),
    SIXEL_REGISTRY_ENCODER_FLOAT(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_A_DITHER,
        "strength", 'T', "SIXEL_DITHER_A_DITHER_STRENGTH", NULL, NULL,
        "-d a_dither:strength must be a floating point value.",
        a_dither_strength, a_dither_strength_override),
    SIXEL_REGISTRY_ENCODER_FLOAT(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        g_diffusion_values + SIXEL_DIFFUSION_BASE_X_DITHER,
        "strength", 'T', "SIXEL_DITHER_X_DITHER_STRENGTH", NULL, NULL,
        "-d x_dither:strength must be a floating point value.",
        x_dither_strength, x_dither_strength_override),
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
    SIXEL_REGISTRY_ENCODER_SCALED_U8_ENV_CLAMP(
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

    SIXEL_REGISTRY_ENCODER_DOUBLE_ENV_CLAMP(
        SIXEL_OPTION_SCHEMA_MERGE_POLICY, NULL,
        "merge_oversplit", 'O', "SIXEL_PALETTE_OVERSPLIT_FACTOR",
        NULL, NULL, 1.0, 3.0,
        "-F merge_oversplit must be in range 1.0-3.0.",
        merge_policy_oversplit,
        merge_policy_oversplit_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_SIGNED(
        SIXEL_OPTION_SCHEMA_MERGE_POLICY, NULL,
        "merge_lloyd", 'L',
        "SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT",
        NULL, NULL, 0.0, 30.0, 0,
        "-F merge_lloyd must be 0 or in range 1-30.",
        merge_policy_lloyd,
        merge_policy_lloyd_override),
    SIXEL_REGISTRY_ENCODER_DOUBLE_ENV_CLAMP(
        SIXEL_OPTION_SCHEMA_MERGE_POLICY, NULL,
        "channel_l", 'C', "SIXEL_PALETTE_MERGE_CHANNEL_FACTOR_L",
        NULL, "SIXEL_PALETTE_CHANNEL_FACTOR_L", 0.0, 1.0,
        "-F channel_l must be in range 0.0-1.0.",
        merge_policy_channel_factor_l,
        merge_policy_channel_factor_l_override),

    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_COVER_POLICY, NULL,
        "cover_grow", 'V', "SIXEL_PALETTE_COVER_GROW", NULL, NULL,
        cover_policy_grow, cover_policy_grow_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_COVER_POLICY, NULL,
        "cover_mode", 'W', "SIXEL_PALETTE_COVER_MODE", NULL, NULL,
        g_palette_cover_mode_choices,
        cover_policy_mode, cover_policy_mode_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_COVER_POLICY, NULL,
        "snap_target", 'T', "SIXEL_PALETTE_SNAP_TARGET_POLICY", NULL, NULL,
        g_palette_snap_target_choices,
        cover_policy_snap_target, cover_policy_snap_target_override),
    SIXEL_REGISTRY_ENCODER_CHOICE(
        SIXEL_OPTION_SCHEMA_COVER_POLICY, NULL,
        "snap_timing", 'I', "SIXEL_PALETTE_SNAP_TIMING_POLICY", NULL, NULL,
        g_palette_snap_timing_choices,
        cover_policy_snap_timing, cover_policy_snap_timing_override),
    SIXEL_REGISTRY_ENCODER_DOUBLE_ENV_CLAMP(
        SIXEL_OPTION_SCHEMA_COVER_POLICY, NULL,
        "snap_rate", 'A', "SIXEL_PALETTE_SNAP_APPROACH_RATE", NULL, NULL,
        0.0, 1.0, "-a snap_rate must be in range 0.0-1.0.",
        cover_policy_snap_approach_rate,
        cover_policy_snap_approach_rate_override),
    SIXEL_REGISTRY_ENCODER_DOUBLE_ENV_CLAMP(
        SIXEL_OPTION_SCHEMA_COVER_POLICY, NULL,
        "snap_channel_l", 'L', "SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L",
        NULL, NULL, 0.0, 1.0,
        "-a snap_channel_l must be in range 0.0-1.0.",
        cover_policy_snap_channel_factor_l,
        cover_policy_snap_channel_factor_l_override),

    SIXEL_REGISTRY_ENCODER_POSITIVE_SIZE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL, NULL,
        "sample_target", 'C', "SIXEL_PALETTE_SAMPLE_TARGET", NULL, NULL,
        "-Q sample_target must be a positive integer.",
        palette_sample_target, palette_sample_override),
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
    SIXEL_REGISTRY_ENCODER_DOUBLE_ENV_CLAMP(
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
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_SIGNED(
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
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_SIGNED(
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
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_UNSIGNED(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "seed", 'S', "SIXEL_PALETTE_KMEANS_SEED", NULL, NULL,
        0.0, 4294967295.0, 0,
        "-Q seed must be in range 0-4294967295.",
        quantize_model_kmeans_seed,
        quantize_model_kmeans_seed_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_UNSIGNED(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "restarts", 'E', "SIXEL_PALETTE_KMEANS_RESTARTS", NULL, NULL,
        1.0, 32.0, 0, "-Q restarts must be in range 1-32.",
        quantize_model_kmeans_restarts,
        quantize_model_kmeans_restarts_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_UNSIGNED(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "iter", 'A', "SIXEL_PALETTE_KMEANS_ITER", NULL, NULL,
        1.0, 100.0, 0, "-Q iter must be in range 1-100.",
        quantize_model_kmeans_iter,
        quantize_model_kmeans_iter_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_SIGNED(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "iter_max", 'X', "SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX",
        NULL, NULL, 1.0, 100.0, 0,
        "-Q iter_max must be in range 1-100.",
        quantize_model_kmeans_iter_max,
        quantize_model_kmeans_iter_max_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_UNSIGNED(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "miniter", 'U', "SIXEL_PALETTE_KMEANS_MINITER", NULL, NULL,
        1.0, 100.0, 1, "-Q miniter must be 0 or in range 1-100.",
        quantize_model_kmeans_miniter,
        quantize_model_kmeans_miniter_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_UNSIGNED(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "polish_iter", 'H', "SIXEL_PALETTE_KMEANS_POLISH_ITER",
        NULL, NULL, 1.0, 16.0, 1,
        "-Q polish_iter must be 0 or in range 1-16.",
        quantize_model_kmeans_polish_iter,
        quantize_model_kmeans_polish_iter_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_UNSIGNED(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_KMEANS,
        "feedback_slots", 'K', "SIXEL_PALETTE_KMEANS_FEEDBACK_SLOTS",
        NULL, NULL, 1.0, 16.0, 0,
        "-Q feedback_slots must be in range 1-16.",
        quantize_model_kmeans_feedback_slots,
        quantize_model_kmeans_feedback_slots_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_UNSIGNED(
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
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "iter", 'I', "SIXEL_PALETTE_KMEDOIDS_ITER", NULL, NULL,
        1.0, 64.0, 0, "-Q iter must be in range 1-64.",
        quantize_model_kmedoids_iter,
        quantize_model_kmedoids_iter_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "sample", 'M', "SIXEL_PALETTE_KMEDOIDS_SAMPLE", NULL, NULL,
        64.0, 1048576.0, 1,
        "-Q sample must be 0 or in range 64-1048576.",
        quantize_model_kmedoids_sample,
        quantize_model_kmedoids_sample_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "clara_trials", 'T', "SIXEL_PALETTE_KMEDOIDS_CLARA_TRIALS",
        NULL, NULL, 1.0, 32.0, 0,
        "-Q clara_trials must be in range 1-32.",
        quantize_model_kmedoids_clara_trials,
        quantize_model_kmedoids_clara_trials_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "clara_sample", 'K', "SIXEL_PALETTE_KMEDOIDS_CLARA_SAMPLE",
        NULL, NULL, 64.0, 1048576.0, 1,
        "-Q clara_sample must be 0 or in range 64-1048576.",
        quantize_model_kmedoids_clara_sample,
        quantize_model_kmedoids_clara_sample_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "clarans_local", 'J', "SIXEL_PALETTE_KMEDOIDS_CLARANS_LOCAL",
        NULL, NULL, 1.0, 32.0, 0,
        "-Q clarans_local must be in range 1-32.",
        quantize_model_kmedoids_clarans_local,
        quantize_model_kmedoids_clarans_local_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "clarans_neighbors", 'N',
        "SIXEL_PALETTE_KMEDOIDS_CLARANS_NEIGHBORS", NULL, NULL,
        1.0, 5000000.0, 1,
        "-Q clarans_neighbors must be 0 or in range 1-5000000.",
        quantize_model_kmedoids_clarans_neighbors,
        quantize_model_kmedoids_clarans_neighbors_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "bandit_iter", 'D', "SIXEL_PALETTE_KMEDOIDS_BANDIT_ITER",
        NULL, NULL, 1.0, 64.0, 0,
        "-Q bandit_iter must be in range 1-64.",
        quantize_model_kmedoids_bandit_iter,
        quantize_model_kmedoids_bandit_iter_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "bandit_candidates", 'E',
        "SIXEL_PALETTE_KMEDOIDS_BANDIT_CANDIDATES", NULL, NULL,
        8.0, 4096.0, 0,
        "-Q bandit_candidates must be in range 8-4096.",
        quantize_model_kmedoids_bandit_candidates,
        quantize_model_kmedoids_bandit_candidates_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "bandit_batch", 'X', "SIXEL_PALETTE_KMEDOIDS_BANDIT_BATCH",
        NULL, NULL, 8.0, 4096.0, 0,
        "-Q bandit_batch must be in range 8-4096.",
        quantize_model_kmedoids_bandit_batch,
        quantize_model_kmedoids_bandit_batch_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "histbits", 'H', "SIXEL_PALETTE_KMEDOIDS_HISTBITS", NULL, NULL,
        3.0, 6.0, 0, "-Q histbits must be in range 3-6.",
        quantize_model_kmedoids_histbits,
        quantize_model_kmedoids_histbits_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_MEDOIDS,
        "point_budget", 'B', "SIXEL_PALETTE_KMEDOIDS_POINT_BUDGET",
        NULL, NULL, 64.0, 16384.0, 0,
        "-Q point_budget must be in range 64-16384.",
        quantize_model_kmedoids_point_budget,
        quantize_model_kmedoids_point_budget_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE(
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
    SIXEL_REGISTRY_ENCODER_UINT_ENV_CLAMP_POSITIVE(
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
    SIXEL_REGISTRY_ENCODER_UINT_ENV_REJECT_UNSIGNED_LONG(
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
    SIXEL_REGISTRY_ENCODER_UINT_ENV_REJECT_UNSIGNED_LONG(
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
    SIXEL_REGISTRY_ENCODER_UINT_ENV_REJECT_UNSIGNED_LONG(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "restarts", 'X', "SIXEL_PALETTE_KCENTER_RESTARTS", NULL, NULL,
        1.0, 32.0, 0, "-Q restarts must be in range 1-32.",
        quantize_model_kcenter_restarts,
        quantize_model_kcenter_restarts_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_REJECT_UNSIGNED_LONG(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "init_seeds", 'N', "SIXEL_PALETTE_KCENTER_INIT_SEEDS", NULL, NULL,
        1.0, 8.0, 0, "-Q init_seeds must be in range 1-8.",
        quantize_model_kcenter_init_seeds,
        quantize_model_kcenter_init_seeds_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_REJECT_UNSIGNED_LONG(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "iter", 'I', "SIXEL_PALETTE_KCENTER_ITER", NULL, NULL,
        1.0, 64.0, 0, "-Q iter must be in range 1-64.",
        quantize_model_kcenter_iter,
        quantize_model_kcenter_iter_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_REJECT_UNSIGNED_LONG(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "histbits", 'H', "SIXEL_PALETTE_KCENTER_HISTBITS", NULL, NULL,
        3.0, 6.0, 0, "-Q histbits must be in range 3-6.",
        quantize_model_kcenter_histbits,
        quantize_model_kcenter_histbits_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_REJECT_UNSIGNED_LONG(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        g_quantize_values + SIXEL_QUANTIZE_BASE_CENTER,
        "point_budget", 'B', "SIXEL_PALETTE_KCENTER_POINT_BUDGET",
        NULL, NULL, 64.0, 16384.0, 1,
        "-Q point_budget must be 0 or in range 64-16384.",
        quantize_model_kcenter_point_budget,
        quantize_model_kcenter_point_budget_override),
    SIXEL_REGISTRY_ENCODER_UINT_ENV_REJECT_UNSIGNED_LONG(
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
    SIXEL_REGISTRY_ENCODER_UINT_ENV_REJECT_UNSIGNED_LONG(
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
    SIXEL_REGISTRY_ENCODER_UINT_ENV_REJECT_UNSIGNED_LONG(
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

    SIXEL_REGISTRY_ENCODER_MULTI_CHOICE(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        SIXEL_LOOKUP_BASE_SET_DENSE,
        "packing", 'P', "SIXEL_LOOKUP_PACKING", NULL, NULL,
        g_lookup_packing_choices,
        lut_policy_packing,
        lut_policy_packing_override),
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
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT,
        "first_touch", 'O', "SIXEL_LOOKUP_FHEDT_FIRST_TOUCH",
        NULL, "SIXEL_FHEDT_FIRST_TOUCH",
        lut_policy_fhedt_first_touch,
        lut_policy_fhedt_first_touch_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT,
        "pin_threads", 'P', "SIXEL_LOOKUP_FHEDT_PIN_THREADS",
        NULL, "SIXEL_FHEDT_PIN_THREADS",
        lut_policy_fhedt_pin_threads,
        lut_policy_fhedt_pin_threads_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT,
        "refine", 'F', "SIXEL_LOOKUP_FHEDT_REFINE", NULL, NULL,
        lut_policy_fhedt_refine,
        lut_policy_fhedt_refine_override),
    SIXEL_REGISTRY_ENCODER_CHOICE_ENV_PARSE_SIGNED_LONG(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT,
        "resolution", 'R', "SIXEL_LOOKUP_FHEDT_RESOLUTION", NULL, NULL,
        g_fhedt_resolution_choices,
        lut_policy_fhedt_resolution,
        lut_policy_fhedt_resolution_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT,
        "shared", 'S', "SIXEL_LOOKUP_FHEDT_SHARED", NULL, NULL,
        lut_policy_fhedt_shared,
        lut_policy_fhedt_shared_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT,
        "tile_depth", 'T', "SIXEL_LOOKUP_FHEDT_TILE_DEPTH",
        NULL, "SIXEL_FHEDT_TILE_DEPTH", 1.0, 1024.0, 0,
        "FHEDT tile_depth must be an integer in range 1-1024.",
        lut_policy_fhedt_tile_depth,
        lut_policy_fhedt_tile_depth_override),
    SIXEL_REGISTRY_ENCODER_UINT(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT,
        "tile_xy", 'X', "SIXEL_LOOKUP_FHEDT_TILE_XY",
        NULL, "SIXEL_FHEDT_TILE_XY", 1.0, 1024.0, 0,
        "FHEDT tile_xy must be an integer in range 1-1024.",
        lut_policy_fhedt_tile_xy,
        lut_policy_fhedt_tile_xy_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT,
        "cache", 'C', "SIXEL_LOOKUP_FHEDT_USE_CACHE", NULL, NULL,
        lut_policy_fhedt_use_cache,
        lut_policy_fhedt_use_cache_override),
    SIXEL_REGISTRY_ENCODER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        g_lookup_values + SIXEL_LOOKUP_BASE_FHEDT,
        "dist2", 'D', "SIXEL_LOOKUP_FHEDT_USE_DIST2", NULL, NULL,
        lut_policy_fhedt_use_dist2,
        lut_policy_fhedt_use_dist2_override),

    SIXEL_REGISTRY_LOADER_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS, NULL,
        "background_policy", 'P', "SIXEL_BACKGROUND_POLICY", NULL, NULL,
        g_loader_background_policy_choices,
        background_policy),
    SIXEL_REGISTRY_LOADER_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS, NULL,
        "background_colorspace", 'C',
        "SIXEL_LOADER_BACKGROUND_COLORSPACE", NULL, NULL,
        g_loader_background_colorspace_choices,
        background_colorspace),
    SIXEL_REGISTRY_LOADER_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS, NULL,
        "cms_target", 'T', "SIXEL_LOADER_CMS_TARGET_COLORSPACE",
        NULL, NULL, g_loader_cms_target_colorspace_choices,
        cms_target_colorspace),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS, NULL,
        "prefer_8bit", 'V', "SIXEL_LOADER_PREFER_8BIT", NULL, NULL,
        cms_prefer_8bit),
    SIXEL_REGISTRY_LOADER_CHOICE_LIST(
        SIXEL_OPTION_SCHEMA_LOADERS, NULL,
        "cms_intent", 'R', "SIXEL_LOADER_CMS_RENDERING_INTENT", NULL,
        "SIXEL_CMS_RENDERING_INTENT", g_loader_cms_intent_choices,
        cms_rendering_intent_order),
    SIXEL_REGISTRY_LOADER_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_LOADERS, NULL,
        "hdr_fallback_profile", 'F',
        "SIXEL_LOADER_HDR_FALLBACK_PROFILE", NULL, NULL,
        g_loader_hdr_fallback_choices, g_loader_hdr_fallback_env_choices,
        hdr_fallback_profile),
    SIXEL_REGISTRY_LOADER_DOUBLE(
        SIXEL_OPTION_SCHEMA_LOADERS, NULL,
        "hdr_exposure", 'X', "SIXEL_LOADER_HDR_EXPOSURE_EV", NULL, NULL,
        -DBL_MAX, DBL_MAX,
        "HDR exposure must be a finite number.",
        hdr_exposure_ev),
    SIXEL_REGISTRY_LOADER_CHOICE(
        SIXEL_OPTION_SCHEMA_LOADERS, NULL,
        "hdr_tonemap", 'H', "SIXEL_LOADER_HDR_TONEMAP", NULL, NULL,
        g_loader_hdr_tonemap_choices,
        hdr_tonemap_mode),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS, NULL,
        "hdr_header_exposure", 'U',
        "SIXEL_LOADER_HDR_USE_HEADER_EXPOSURE", NULL, NULL,
        hdr_use_header_exposure),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS, NULL,
        "osc11_query", 'Q', "SIXEL_LOADER_OSC11_BG_QUERY", NULL, NULL,
        osc11_bg_query),
    SIXEL_REGISTRY_LOADER_UINT_ENV_REJECT_SIGNED(
        SIXEL_OPTION_SCHEMA_LOADERS, NULL,
        "osc11_timeout", 'W', "SIXEL_LOADER_OSC11_BG_QUERY_TIMEOUT_MS",
        NULL, NULL, 0.0, (double)INT_MAX, 1,
        "invalid loader suboption value \"",
        "\" for key \"osc11_timeout\"; expected milliseconds from 0 to "
        "2147483647.",
        osc11_bg_query_timeout_ms),
    SIXEL_REGISTRY_LOADER_UINT_ENV_CLAMP_MAXIMUM(
        SIXEL_OPTION_SCHEMA_LOADERS, NULL,
        "thumbnail_size", 'Z', "SIXEL_THUMBNAILER_HINT_SIZE", NULL, NULL,
        1.0, (double)INT_MAX, 0,
        "invalid loader suboption value \"",
        "\" for key \"thumbnail_size\"; expected a positive integer.",
        thumbnail_size_hint),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS, NULL,
        "trns_keycolor", 'K', "SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR",
        NULL, NULL,
        png_trns_keycolor),

#if HAVE_LIBPNG
    SIXEL_REGISTRY_LOADER_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBPNG,
        "cms_engine", 'E', "SIXEL_LOADER_LIBPNG_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices,
        g_loader_cms_environment_choices,
        libpng_cms_engine),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBPNG,
        "orientation", 'O', "SIXEL_LOADER_LIBPNG_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL,
        libpng_enable_orientation),
#endif
#if HAVE_JPEG
    SIXEL_REGISTRY_LOADER_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBJPEG,
        "cms_engine", 'E', "SIXEL_LOADER_LIBJPEG_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices,
        g_loader_cms_environment_choices,
        libjpeg_cms_engine),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBJPEG,
        "orientation", 'O', "SIXEL_LOADER_LIBJPEG_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL,
        libjpeg_enable_orientation),
#endif
#if HAVE_WEBP
    SIXEL_REGISTRY_LOADER_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBWEBP,
        "cms_engine", 'E', "SIXEL_LOADER_LIBWEBP_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices,
        g_loader_cms_environment_choices,
        libwebp_cms_engine),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBWEBP,
        "orientation", 'O', "SIXEL_LOADER_LIBWEBP_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL,
        libwebp_enable_orientation),
    SIXEL_REGISTRY_LOADER_UINT_ENV_CLAMP_MAXIMUM_DIGITS(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBWEBP,
        "max_output_frames", 'M',
        "SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES", NULL, NULL,
        1.0, (double)SIXEL_LOADER_LIBWEBP_MAX_OUTPUT_FRAMES_DEFAULT, 0,
        "invalid libwebp suboption value \"",
        "\" for key \"max_output_frames\"; expected a positive integer.",
        "webp_decode",
        libwebp_max_output_frames),
#endif
#if HAVE_COREGRAPHICS
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_COREGRAPHICS,
        "orientation", 'O', "SIXEL_LOADER_COREGRAPHICS_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL,
        coregraphics_enable_orientation),
    SIXEL_REGISTRY_LOADER_SIZE_ENV_ERROR(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_COREGRAPHICS,
        "cache_max_bytes", 'M',
        "SIXEL_LOADER_COREGRAPHICS_CACHE_MAX_BYTES", NULL, NULL,
        "invalid coregraphics suboption value \"",
        "\" for key \"cache_max_bytes\"; expected a byte count.",
        coregraphics_cache_max_bytes,
        coregraphics_cache_max_bytes_override),
#endif
#if HAVE_LIBTIFF
    SIXEL_REGISTRY_LOADER_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBTIFF,
        "cms_engine", 'E', "SIXEL_LOADER_LIBTIFF_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices,
        g_loader_cms_environment_choices,
        libtiff_cms_engine),
#endif
#if HAVE_LIBRSVG
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBRSVG,
        "relative_resources", 'A',
        "SIXEL_LOADER_LIBRSVG_ALLOW_RELATIVE_RESOURCES", NULL, NULL,
        librsvg_allow_relative_resources),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_LIBRSVG,
        "stdin_svgz", 'S', "SIXEL_LOADER_LIBRSVG_ALLOW_STDIN_SVGZ",
        NULL, NULL,
        librsvg_allow_stdin_svgz),
#endif
    SIXEL_REGISTRY_LOADER_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "cms_engine", 'E', "SIXEL_LOADER_BUILTIN_CMS_ENGINE",
        "SIXEL_LOADER_CMS_ENGINE", NULL, g_loader_cms_engine_choices,
        g_loader_cms_environment_choices,
        builtin_cms_engine),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "orientation", 'O', "SIXEL_LOADER_BUILTIN_ORIENTATION",
        "SIXEL_LOADER_ORIENTATION", NULL,
        builtin_enable_orientation),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "pam_duplicate_keys", 'D',
        "SIXEL_LOADER_PAM_ALLOW_DUPLICATE_REQUIRED_KEYS", NULL, NULL,
        builtin_pam_duplicate_keys),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "pam_endhdr_tokens", 'G',
        "SIXEL_LOADER_PAM_ALLOW_ENDHDR_TRAILING_TOKENS", NULL, NULL,
        builtin_pam_endhdr_tokens),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "pam_large_header", 'J', "SIXEL_LOADER_PAM_ALLOW_LARGE_HEADER",
        NULL, NULL,
        builtin_pam_large_header),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "pnm_trailing_data", 'N', "SIXEL_LOADER_PNM_ALLOW_TRAILING_DATA",
        NULL, NULL,
        builtin_pnm_trailing_data),
    SIXEL_REGISTRY_LOADER_BOOLEAN(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "pnm_truncated_ascii", 'Y',
        "SIXEL_LOADER_PNM_ALLOW_TRUNCATED_ASCII", NULL, NULL,
        builtin_pnm_truncated_ascii),
    SIXEL_REGISTRY_LOADER_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_BUILTIN,
        "bmp_info40_mode", 'B', "SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE",
        NULL, NULL, g_loader_bmp_info40_mode_choices,
        g_loader_bmp_environment_choices,
        builtin_bmp_info40_mode),
#if HAVE_WIC
    SIXEL_REGISTRY_LOADER_UINT(
        SIXEL_OPTION_SCHEMA_LOADERS,
        g_loader_values + SIXEL_LOADER_INDEX_WIC,
        "ico_minsize", 'I', "SIXEL_LOADER_WIC_ICO_MINSIZE", NULL,
        "SIXEL_LODER_WIC_ICO_MINSIZE", 1.0, 2147483647.0, 0,
        "invalid wic suboption value \"",
        "\" for key \"ico_minsize\"; expected a positive integer.",
        wic_ico_minsize),
#endif
};

#define SIXEL_REGISTRY_OPTION_SCHEMA( \
    option_id_, scope_, optflag_, name_, form_, default_policy_, \
    default_value_, values_, env_) \
    { \
        (option_id_), (scope_), (optflag_), (name_), (form_), \
        SIXEL_SUBOPTION_VALUE_STRUCTURED, SIXEL_OPTION_MATCH_PREFIX, \
        SIXEL_OPTION_MATCH_PREFIX, (env_), NULL, NULL, NULL, 0u, \
        0.0, 0.0, 0, 0, 0.0, 0.0, 0, 0, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, NULL, NULL, NULL, NULL, NULL, \
        NULL, NULL, \
        (default_policy_), { (default_value_) }, (values_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(values_) \
    }

#define SIXEL_REGISTRY_RUNTIME_OPTION_SCHEMA( \
    option_id_, scope_, optflag_, name_, default_value_, values_, env_) \
    { \
        (option_id_), (scope_), (optflag_), (name_), \
        SIXEL_OPTION_ARGUMENT_SINGLE, SIXEL_SUBOPTION_VALUE_STRUCTURED, \
        SIXEL_OPTION_MATCH_PREFIX, \
        SIXEL_OPTION_MATCH_PREFIX | SIXEL_OPTION_MATCH_CASE_INSENSITIVE, \
        (env_), NULL, NULL, NULL, 0u, 0.0, 0.0, 0, 0, 0.0, 0.0, 0, 0, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, NULL, NULL, NULL, NULL, NULL, \
        NULL, NULL, SIXEL_OPTION_DEFAULT_FIXED, { (default_value_) }, \
        (values_), SIXEL_REGISTRY_ARRAY_LENGTH(values_) \
    }

#define SIXEL_REGISTRY_DIAGNOSTICS_OPTION_SCHEMA( \
    option_id_, scope_, optflag_, name_, default_value_, values_, env_) \
    { \
        (option_id_), (scope_), (optflag_), (name_), \
        SIXEL_OPTION_ARGUMENT_SINGLE, SIXEL_SUBOPTION_VALUE_STRUCTURED, \
        SIXEL_OPTION_MATCH_PREFIX, SIXEL_OPTION_MATCH_EXACT, \
        (env_), NULL, NULL, NULL, 0u, 0.0, 0.0, 0, 0, 0.0, 0.0, 0, 0, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, NULL, NULL, NULL, NULL, NULL, \
        NULL, NULL, SIXEL_OPTION_DEFAULT_FIXED, { (default_value_) }, \
        (values_), SIXEL_REGISTRY_ARRAY_LENGTH(values_) \
    }

#define SIXEL_REGISTRY_CLIPBOARD_OPTION_SCHEMA( \
    option_id_, scope_, optflag_, name_, default_value_, values_, env_, \
    env_choices_) \
    { \
        (option_id_), (scope_), (optflag_), (name_), \
        SIXEL_OPTION_ARGUMENT_SINGLE, SIXEL_SUBOPTION_VALUE_STRUCTURED, \
        SIXEL_OPTION_MATCH_PREFIX, \
        SIXEL_OPTION_MATCH_EXACT | SIXEL_OPTION_MATCH_CASE_INSENSITIVE, \
        (env_), NULL, NULL, (env_choices_), \
        SIXEL_REGISTRY_ARRAY_LENGTH(env_choices_), \
        0.0, 0.0, 0, 0, 0.0, 0.0, 0, 0, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, NULL, NULL, NULL, NULL, NULL, \
        NULL, NULL, SIXEL_OPTION_DEFAULT_FIXED, { (default_value_) }, \
        (values_), SIXEL_REGISTRY_ARRAY_LENGTH(values_) \
    }

#define SIXEL_REGISTRY_SCALAR_SCHEMA( \
    option_id_, scope_, optflag_, name_, kind_, argument_flags_, \
    environment_flags_, env_, environment_choices_, \
    environment_choice_count_, minimum_, maximum_, has_minimum_, \
    has_maximum_, environment_minimum_, environment_maximum_, \
    environment_has_minimum_, environment_has_maximum_, allow_zero_, \
    environment_range_, message_, decoder_message_, minimum_message_, \
    maximum_message_, environment_message_, range_message_, \
    default_policy_, default_value_, values_, value_count_) \
    { \
        (option_id_), (scope_), (optflag_), (name_), \
        SIXEL_OPTION_ARGUMENT_SINGLE, (kind_), (argument_flags_), \
        (environment_flags_), (env_), NULL, NULL, \
        (environment_choices_), (environment_choice_count_), \
        (minimum_), (maximum_), (has_minimum_), (has_maximum_), \
        (environment_minimum_), (environment_maximum_), \
        (environment_has_minimum_), (environment_has_maximum_), \
        (allow_zero_), (environment_range_), (message_), \
        (decoder_message_), NULL, \
        (minimum_message_), (maximum_message_), \
        (environment_message_), (range_message_), \
        (default_policy_), { (default_value_) }, (values_), (value_count_) \
    }

#define SIXEL_REGISTRY_SCALAR_CHOICE( \
    option_id_, scope_, optflag_, name_, argument_flags_, \
    environment_flags_, env_, message_, default_value_, values_) \
    SIXEL_REGISTRY_SCALAR_SCHEMA( \
        option_id_, scope_, optflag_, name_, SIXEL_SUBOPTION_VALUE_CHOICE, \
        argument_flags_, environment_flags_, env_, NULL, 0u, 0.0, 0.0, \
        0, 0, 0.0, 0.0, 0, 0, 0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, \
        message_, NULL, NULL, NULL, NULL, NULL, \
        SIXEL_OPTION_DEFAULT_FIXED, \
        default_value_, values_, SIXEL_REGISTRY_ARRAY_LENGTH(values_))

#define SIXEL_REGISTRY_SCALAR_CHOICE_ENV( \
    option_id_, scope_, optflag_, name_, argument_flags_, \
    environment_flags_, env_, message_, default_value_, values_, \
    env_choices_) \
    SIXEL_REGISTRY_SCALAR_SCHEMA( \
        option_id_, scope_, optflag_, name_, SIXEL_SUBOPTION_VALUE_CHOICE, \
        argument_flags_, environment_flags_, env_, env_choices_, \
        SIXEL_REGISTRY_ARRAY_LENGTH(env_choices_), 0.0, 0.0, 0, 0, \
        0.0, 0.0, 0, 0, 0, SIXEL_SUBOPTION_ENV_RANGE_REJECT, message_, \
        NULL, NULL, NULL, NULL, NULL, \
        SIXEL_OPTION_DEFAULT_FIXED, default_value_, values_, \
        SIXEL_REGISTRY_ARRAY_LENGTH(values_))

#define SIXEL_REGISTRY_SCALAR_INT( \
    option_id_, scope_, optflag_, name_, argument_flags_, \
    environment_flags_, env_, minimum_, maximum_, has_minimum_, \
    has_maximum_, environment_range_, message_, decoder_message_, \
    minimum_message_, maximum_message_, environment_message_, \
    range_message_, \
    default_policy_, default_value_, values_, value_count_) \
    SIXEL_REGISTRY_SCALAR_SCHEMA( \
        option_id_, scope_, optflag_, name_, SIXEL_SUBOPTION_VALUE_INT, \
        argument_flags_, environment_flags_, env_, NULL, 0u, minimum_, \
        maximum_, has_minimum_, has_maximum_, minimum_, maximum_, \
        has_minimum_, has_maximum_, 0, environment_range_, message_, \
        decoder_message_, minimum_message_, maximum_message_, \
        environment_message_, range_message_, \
        default_policy_, default_value_, values_, value_count_)

#define SIXEL_REGISTRY_SCALAR_UINT( \
    option_id_, scope_, optflag_, name_, env_, minimum_, maximum_, \
    environment_minimum_, environment_maximum_, environment_range_, \
    message_, minimum_message_, maximum_message_, default_policy_, \
    default_value_) \
    SIXEL_REGISTRY_SCALAR_SCHEMA( \
        option_id_, scope_, optflag_, name_, SIXEL_SUBOPTION_VALUE_UINT, \
        SIXEL_OPTION_MATCH_EXACT, SIXEL_OPTION_MATCH_EXACT, env_, NULL, 0u, \
        minimum_, maximum_, 1, 1, environment_minimum_, \
        environment_maximum_, 1, 1, 0, environment_range_, message_, NULL, \
        minimum_message_, maximum_message_, NULL, NULL, \
        default_policy_, default_value_, NULL, 0u)

#define SIXEL_REGISTRY_SCALAR_STRING( \
    option_id_, scope_, optflag_, name_, env_) \
    SIXEL_REGISTRY_SCALAR_SCHEMA( \
        option_id_, scope_, optflag_, name_, SIXEL_SUBOPTION_VALUE_STRING, \
        SIXEL_OPTION_MATCH_EXACT, SIXEL_OPTION_MATCH_EXACT, env_, NULL, 0u, \
        0.0, 0.0, 0, 0, 0.0, 0.0, 0, 0, 0, \
        SIXEL_SUBOPTION_ENV_RANGE_REJECT, \
        "option value must not be empty.", NULL, NULL, NULL, NULL, NULL, \
        SIXEL_OPTION_DEFAULT_OWNER, \
        0, NULL, 0u)

static sixel_option_argument_schema_t const g_options[] = {
    SIXEL_REGISTRY_OPTION_SCHEMA(
        SIXEL_OPTION_SCHEMA_DEQUANTIZE,
        SIXEL_OPTION_SCOPE_DECODER,
        SIXEL_OPTFLAG_DEQUANTIZE,
        "dequantize",
        SIXEL_OPTION_ARGUMENT_SINGLE,
        SIXEL_OPTION_DEFAULT_FIXED,
        SIXEL_DEQUANTIZE_NONE,
        g_dequantize_values,
        NULL),
    SIXEL_REGISTRY_OPTION_SCHEMA(
        SIXEL_OPTION_SCHEMA_DIFFUSION,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_DIFFUSION,
        "diffusion",
        SIXEL_OPTION_ARGUMENT_SINGLE,
        SIXEL_OPTION_DEFAULT_FIXED,
        SIXEL_DIFFUSE_AUTO,
        g_diffusion_values,
        NULL),
    SIXEL_REGISTRY_OPTION_SCHEMA(
        SIXEL_OPTION_SCHEMA_QUANTIZE_MODEL,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_QUANTIZE_MODEL,
        "quantize-model",
        SIXEL_OPTION_ARGUMENT_LIST,
        SIXEL_OPTION_DEFAULT_FIXED,
        SIXEL_QUANTIZE_MODEL_AUTO,
        g_quantize_values,
        NULL),
    SIXEL_REGISTRY_OPTION_SCHEMA(
        SIXEL_OPTION_SCHEMA_PALETTE_SAMPLING,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_PALETTE_SAMPLING,
        "palette-sampling",
        SIXEL_OPTION_ARGUMENT_SINGLE,
        SIXEL_OPTION_DEFAULT_FIXED,
        SIXEL_PALETTE_SAMPLING_AUTO,
        g_palette_sampling_values,
        "SIXEL_PALETTE_SAMPLING"),
    SIXEL_REGISTRY_OPTION_SCHEMA(
        SIXEL_OPTION_SCHEMA_PALETTE_BINNING,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_PALETTE_BINNING,
        "palette-binning",
        SIXEL_OPTION_ARGUMENT_SINGLE,
        SIXEL_OPTION_DEFAULT_FIXED,
        SIXEL_PALETTE_BINNING_AUTO,
        g_palette_binning_values,
        "SIXEL_PALETTE_BINNING"),
    SIXEL_REGISTRY_OPTION_SCHEMA(
        SIXEL_OPTION_SCHEMA_MERGE_POLICY,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_MERGE_POLICY,
        "merge-policy",
        SIXEL_OPTION_ARGUMENT_SINGLE,
        SIXEL_OPTION_DEFAULT_FIXED,
        SIXEL_FINAL_MERGE_AUTO,
        g_merge_policy_values,
        "SIXEL_PALETTE_FINAL_MERGE"),
    SIXEL_REGISTRY_OPTION_SCHEMA(
        SIXEL_OPTION_SCHEMA_COVER_POLICY,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_COVER_POLICY,
        "cover-policy",
        SIXEL_OPTION_ARGUMENT_SINGLE,
        SIXEL_OPTION_DEFAULT_FIXED,
        SIXEL_PALETTE_COVER_AUTO,
        g_cover_policy_values,
        "SIXEL_PALETTE_COVER"),
    SIXEL_REGISTRY_OPTION_SCHEMA(
        SIXEL_OPTION_SCHEMA_LUT_POLICY,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_LUT_POLICY,
        "lookup-policy",
        SIXEL_OPTION_ARGUMENT_SINGLE,
        SIXEL_OPTION_DEFAULT_FIXED,
        SIXEL_LUT_POLICY_AUTO,
        g_lookup_values,
        "SIXEL_DITHER_LOOKUP_POLICY"),
    SIXEL_REGISTRY_OPTION_SCHEMA(
        SIXEL_OPTION_SCHEMA_LOADERS,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_LOADERS,
        "loaders",
        SIXEL_OPTION_ARGUMENT_LIST,
        SIXEL_OPTION_DEFAULT_OWNER,
        0,
        g_loader_values,
        "SIXEL_LOADER_PRIORITY_LIST"),
    SIXEL_REGISTRY_SCALAR_CHOICE_ENV(
        SIXEL_OPTION_SCHEMA_PRECISION,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_PRECISION,
        "precision",
        SIXEL_OPTION_MATCH_PREFIX,
        SIXEL_OPTION_MATCH_EXACT,
        "SIXEL_FLOAT32_DITHER",
        "precision accepts auto, 8bit, or float32.",
        SIXEL_OPTION_PRECISION_AUTO,
        g_precision_values,
        g_precision_environment_choices),
    SIXEL_REGISTRY_SCALAR_INT(
        SIXEL_OPTION_SCHEMA_THREADS,
        SIXEL_OPTION_SCOPE_ALL,
        SIXEL_OPTFLAG_THREADS,
        "threads",
        SIXEL_OPTION_MATCH_CASE_INSENSITIVE,
        SIXEL_OPTION_MATCH_CASE_INSENSITIVE,
        "SIXEL_THREADS",
        1.0,
        (double)INT_MAX,
        1,
        1,
        SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM |
            SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM |
            SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG,
        "threads accepts positive integers or 'auto'.",
        "threads must be a positive integer or 'auto'.",
        NULL,
        NULL,
        NULL,
        NULL,
        SIXEL_OPTION_DEFAULT_OWNER,
        0,
        g_threads_values,
        SIXEL_REGISTRY_ARRAY_LENGTH(g_threads_values)),
    SIXEL_REGISTRY_SCALAR_UINT(
        SIXEL_OPTION_SCHEMA_COLORS,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_COLORS,
        "colors",
        "SIXEL_COLORS",
        1.0,
        (double)SIXEL_PALETTE_MAX,
        2.0,
        (double)SIXEL_PALETTE_MAX,
        SIXEL_SUBOPTION_ENV_RANGE_REJECT,
        "cannot parse -p/--colors option.",
        "-p/--colors parameter must be 1 or more.",
        "-p/--colors parameter must be less then or equal to 256.",
        SIXEL_OPTION_DEFAULT_FIXED,
        SIXEL_PALETTE_MAX),
    SIXEL_REGISTRY_SCALAR_INT(
        SIXEL_OPTION_SCHEMA_START_FRAME,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_START_FRAME,
        "start-frame",
        SIXEL_OPTION_MATCH_EXACT,
        SIXEL_OPTION_MATCH_EXACT,
        "SIXEL_LOADER_ANIMATION_START_FRAME_NO",
        0.0,
        0.0,
        0,
        0,
        SIXEL_SUBOPTION_ENV_RANGE_REJECT,
        "cannot parse start_frame option.",
        NULL,
        NULL,
        NULL,
        "SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer.",
        "SIXEL_LOADER_ANIMATION_START_FRAME_NO is out of range.",
        SIXEL_OPTION_DEFAULT_OWNER,
        0,
        NULL,
        0u),
    SIXEL_REGISTRY_OPTION_SCHEMA(
        SIXEL_OPTION_SCHEMA_GPU_POLICY,
        SIXEL_OPTION_SCOPE_ALL,
        SIXEL_OPTFLAG_GPU_POLICY,
        "gpu-policy",
        SIXEL_OPTION_ARGUMENT_SINGLE,
        SIXEL_OPTION_DEFAULT_FIXED,
        SIXEL_GPU_POLICY_OFF,
        g_gpu_policy_values,
        "SIXEL_GPU_POLICY"),
    SIXEL_REGISTRY_SCALAR_CHOICE(
        SIXEL_OPTION_SCHEMA_TRANSPARENT_POLICY,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_TRANSPARENT_POLICY,
        "transparent-policy",
        SIXEL_OPTION_MATCH_PREFIX,
        SIXEL_OPTION_MATCH_EXACT,
        "SIXEL_TRANSPARENT_POLICY",
        "cannot parse transparent policy option.",
        SIXEL_TRANSPARENT_POLICY_BACKGROUND,
        g_transparent_policy_values),
    SIXEL_REGISTRY_SCALAR_UINT(
        SIXEL_OPTION_SCHEMA_6DELTA_THRESHOLD,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_6DELTA_THRESHOLD,
        "6delta-threshold",
        "SIXEL_6DELTA_THRESHOLD",
        0.0,
        255.0,
        0.0,
        255.0,
        SIXEL_SUBOPTION_ENV_RANGE_REJECT,
        "6delta threshold must be an integer in range 0..255.",
        NULL,
        NULL,
        SIXEL_OPTION_DEFAULT_OWNER,
        0),
    SIXEL_REGISTRY_SCALAR_CHOICE(
        SIXEL_OPTION_SCHEMA_6DELTA_ERROR,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_6DELTA_ERROR,
        "6delta-error",
        SIXEL_OPTION_MATCH_PREFIX,
        SIXEL_OPTION_MATCH_PREFIX,
        "SIXEL_6DELTA_ERROR",
        "cannot parse 6delta error option.",
        SIXEL_6DELTA_ERROR_DIFFUSE,
        g_6delta_error_values),
    SIXEL_REGISTRY_SCALAR_STRING(
        SIXEL_OPTION_SCHEMA_BGCOLOR,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_BGCOLOR,
        "bgcolor",
        "SIXEL_BGCOLOR"),
    SIXEL_REGISTRY_RUNTIME_OPTION_SCHEMA(
        SIXEL_OPTION_SCHEMA_RUNTIME_POLICY,
        SIXEL_OPTION_SCOPE_ALL,
        SIXEL_OPTFLAG_RUNTIME_POLICY,
        "runtime-policy",
        SIXEL_SIMD_LEVEL_NEON,
        g_runtime_policy_values,
        "SIXEL_SIMD_LEVEL"),
    SIXEL_REGISTRY_DIAGNOSTICS_OPTION_SCHEMA(
        SIXEL_OPTION_SCHEMA_DIAGNOSTICS,
        SIXEL_OPTION_SCOPE_ALL,
        SIXEL_OPTFLAG_DIAGNOSTICS,
        "diagnostics",
        SIXEL_DIAGNOSTICS_MODE_HUMAN,
        g_diagnostics_values,
        "SIXEL_DIAG_MODE"),
    SIXEL_REGISTRY_SCALAR_STRING(
        SIXEL_OPTION_SCHEMA_LOG_PATH,
        SIXEL_OPTION_SCOPE_ALL,
        SIXEL_OPTFLAG_LOG_PATH,
        "log-path",
        "SIXEL_LOG_PATH"),
    SIXEL_REGISTRY_CLIPBOARD_OPTION_SCHEMA(
        SIXEL_OPTION_SCHEMA_CLIPBOARD_POLICY,
        SIXEL_OPTION_SCOPE_ALL,
        SIXEL_OPTFLAG_CLIPBOARD_POLICY,
        "clipboard-policy",
        SIXEL_CLIPBOARD_BACKEND_SYSTEM,
        g_clipboard_policy_values,
        "SIXEL_CLIPBOARD_BACKEND",
        g_clipboard_policy_environment_choices),
    SIXEL_REGISTRY_SCALAR_CHOICE(
        SIXEL_OPTION_SCHEMA_TERMINAL_POLICY,
        SIXEL_OPTION_SCOPE_ENCODER,
        SIXEL_OPTFLAG_TERMINAL_POLICY,
        "terminal-policy",
        SIXEL_OPTION_MATCH_EXACT,
        SIXEL_OPTION_MATCH_EXACT,
        "SIXEL_ANIMATION_HIDE_CURSOR",
        "terminal policy must be 0 or 1.",
        0,
        g_terminal_policy_values),
};

static int
sixel_option_registry_base_index(
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    size_t *base_index_out)
{
    size_t base_index;

    base_index = 0u;
    if (schema == NULL || base_def == NULL || base_index_out == NULL) {
        return 0;
    }
    while (base_index < schema->value_count) {
        if (schema->values + base_index == base_def) {
            *base_index_out = base_index;
            return 1;
        }
        ++base_index;
    }

    return 0;
}

static int
sixel_option_registry_key_is_common(sixel_suboption_key_t const *key)
{
    return key != NULL && key->base_def == NULL && key->base_mask == 0ULL;
}

static int
sixel_option_registry_key_applies_to_base(
    sixel_suboption_key_t const *key,
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def)
{
    size_t base_index;

    base_index = 0u;
    if (key == NULL || schema == NULL) {
        return 0;
    }
    if (base_def == NULL || sixel_option_registry_key_is_common(key)) {
        return 1;
    }
    if (key->base_def != NULL) {
        return key->base_def == base_def;
    }
    if (!sixel_option_registry_base_index(schema,
                                          base_def,
                                          &base_index) ||
        base_index >= sizeof(key->base_mask) * CHAR_BIT) {
        return 0;
    }

    return (key->base_mask & (1ULL << (unsigned int)base_index)) != 0ULL;
}

static int
sixel_option_registry_key_applies(
    sixel_suboption_key_t const *key,
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    unsigned int consumer_scope)
{
    if (key == NULL || schema == NULL || consumer_scope == 0u) {
        return 0;
    }
    if (key->option_id != schema->option_id ||
        (key->consumer_scope & consumer_scope) == 0u) {
        return 0;
    }
    return sixel_option_registry_key_applies_to_base(key,
                                                     schema,
                                                     base_def);
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
    return sixel_option_registry_suboption_count_for_scope(
        schema,
        base_def,
        SIXEL_OPTION_SCOPE_ALL);
}

size_t
sixel_option_registry_suboption_count_for_scope(
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    unsigned int consumer_scope)
{
    size_t index;
    size_t count;

    index = 0u;
    count = 0u;
    while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
        if (sixel_option_registry_key_applies(g_suboptions + index,
                                              schema,
                                              base_def,
                                              consumer_scope)) {
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
    return sixel_option_registry_suboption_at_for_scope(
        schema,
        base_def,
        SIXEL_OPTION_SCOPE_ALL,
        requested_index);
}

sixel_suboption_key_t const *
sixel_option_registry_suboption_at_for_scope(
    sixel_option_argument_schema_t const *schema,
    sixel_option_value_schema_t const *base_def,
    unsigned int consumer_scope,
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

    if (schema == NULL || consumer_scope == 0u) {
        return NULL;
    }

    /* A NULL base enumerates every row once for environment initialization. */
    if (base_def == NULL) {
        while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
            if (g_suboptions[index].option_id == schema->option_id &&
                (g_suboptions[index].consumer_scope & consumer_scope) !=
                    0u) {
                if (matched_index == requested_index) {
                    return g_suboptions + index;
                }
                ++matched_index;
            }
            ++index;
        }
        return NULL;
    }

    while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
        if (g_suboptions[index].option_id == schema->option_id &&
            (g_suboptions[index].consumer_scope & consumer_scope) != 0u) {
            if (sixel_option_registry_key_is_common(
                    g_suboptions + index)) {
                ++common_count;
            } else if (sixel_option_registry_key_applies_to_base(
                           g_suboptions + index,
                           schema,
                           base_def)) {
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
            (g_suboptions[index].consumer_scope & consumer_scope) != 0u &&
            ((request_common && sixel_option_registry_key_is_common(
                                    g_suboptions + index)) ||
             (!request_common &&
              !sixel_option_registry_key_is_common(g_suboptions + index) &&
              sixel_option_registry_key_applies_to_base(
                  g_suboptions + index,
                  schema,
                  base_def)))) {
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

sixel_suboption_key_t const *
sixel_option_registry_suboption_by_binding(
    sixel_option_schema_id_t option_id,
    char const *base_name,
    char const *binding_identifier)
{
    sixel_option_argument_schema_t const *schema;
    sixel_option_value_schema_t const *base_def;
    sixel_suboption_key_t const *key;
    size_t base_index;
    size_t index;

    schema = NULL;
    base_def = NULL;
    key = NULL;
    base_index = 0u;
    index = 0u;
    if (binding_identifier == NULL || binding_identifier[0] == '\0') {
        return NULL;
    }
    schema = sixel_option_registry_get(option_id);
    if (schema == NULL) {
        return NULL;
    }
    if (base_name != NULL) {
        while (base_index < schema->value_count) {
            if (strcmp(schema->values[base_index].name, base_name) == 0) {
                base_def = schema->values + base_index;
                break;
            }
            ++base_index;
        }
        if (base_def == NULL) {
            return NULL;
        }
    }
    while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
        key = g_suboptions + index;
        if (key->option_id == option_id &&
            key->binding.identifier != NULL &&
            strcmp(key->binding.identifier, binding_identifier) == 0 &&
            ((base_name == NULL &&
              sixel_option_registry_key_is_common(key)) ||
             (base_name != NULL &&
              !sixel_option_registry_key_is_common(key) &&
              sixel_option_registry_key_applies_to_base(key,
                                                        schema,
                                                        base_def)))) {
            return key;
        }
        ++index;
    }

    return NULL;
}

/*
 * Process-wide runtime consumers are also linked into the Quick Look core,
 * which deliberately excludes the CLI parser.  Resolve their registered
 * environment values here so environment spelling and validation never leak
 * back into individual consumers.
 */
static sixel_option_environment_result_t
sixel_option_registry_resolve_process_binding(
    sixel_option_schema_id_t option_id,
    char const *binding_identifier,
    sixel_suboption_value_kind_t expected_kind,
    sixel_suboption_value_t *value)
{
    sixel_suboption_key_t const *key;
    char const *text;
    char *endptr;
    unsigned long long parsed_uint;
    long parsed_int;
    size_t index;

    key = NULL;
    text = NULL;
    endptr = NULL;
    parsed_uint = 0ULL;
    parsed_int = 0L;
    index = 0u;
    if (binding_identifier == NULL || value == NULL ||
        !sixel_option_registry_validate()) {
        return SIXEL_OPTION_ENVIRONMENT_INVALID;
    }
    key = sixel_option_registry_suboption_by_binding(
        option_id,
        NULL,
        binding_identifier);
    if (key == NULL || key->value_kind != expected_kind ||
        key->env_name == NULL) {
        return SIXEL_OPTION_ENVIRONMENT_INVALID;
    }
    text = sixel_compat_getenv(key->env_name);
    if (text == NULL || text[0] == '\0') {
        return SIXEL_OPTION_ENVIRONMENT_UNSET;
    }

    if (expected_kind == SIXEL_SUBOPTION_VALUE_BOOLEAN) {
        if (text[0] == '0' && text[1] == '\0') {
            value->int_value = 0;
            return SIXEL_OPTION_ENVIRONMENT_MATCH;
        }
        if (text[0] == '1' && text[1] == '\0') {
            value->int_value = 1;
            return SIXEL_OPTION_ENVIRONMENT_MATCH;
        }
        return SIXEL_OPTION_ENVIRONMENT_INVALID;
    }

    if (expected_kind == SIXEL_SUBOPTION_VALUE_STRING) {
        value->string_value = text;
        return SIXEL_OPTION_ENVIRONMENT_MATCH;
    }

    if (expected_kind == SIXEL_SUBOPTION_VALUE_SIZE) {
        errno = 0;
        parsed_uint = strtoull(text, &endptr, 10);
        if (endptr == text || endptr == NULL || endptr[0] != '\0' ||
            errno == ERANGE) {
            return SIXEL_OPTION_ENVIRONMENT_INVALID;
        }
        if (parsed_uint > (unsigned long long)SIZE_MAX) {
            if ((key->environment_range_policy &
                 SIXEL_SUBOPTION_ENV_RANGE_CLAMP_SIZE_WIDTH) == 0) {
                return SIXEL_OPTION_ENVIRONMENT_RANGE;
            }
            parsed_uint = (unsigned long long)SIZE_MAX;
        }
        value->size_value = (size_t)parsed_uint;
        return SIXEL_OPTION_ENVIRONMENT_MATCH;
    }

    errno = 0;
    parsed_int = strtol(text, &endptr, 10);
    if (endptr == text || endptr == NULL) {
        return SIXEL_OPTION_ENVIRONMENT_INVALID;
    }
    if (expected_kind == SIXEL_SUBOPTION_VALUE_INT) {
        if (endptr[0] != '\0' &&
            (key->environment_range_policy &
             SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG_PREFIX) == 0) {
            return SIXEL_OPTION_ENVIRONMENT_INVALID;
        }
        if (errno == ERANGE) {
            if (parsed_int == LONG_MIN && key->has_minimum &&
                (key->environment_range_policy &
                 SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM) != 0) {
                parsed_int = (long)key->minimum;
            } else if (parsed_int == LONG_MAX && key->has_maximum &&
                       (key->environment_range_policy &
                        SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM) != 0) {
                parsed_int = (long)key->maximum;
            } else {
                return SIXEL_OPTION_ENVIRONMENT_INVALID;
            }
        }
        if (parsed_int < (long)INT_MIN) {
            if (!key->has_minimum ||
                (key->environment_range_policy &
                 SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM) == 0) {
                return SIXEL_OPTION_ENVIRONMENT_INVALID;
            }
            parsed_int = (long)key->minimum;
        }
        if (parsed_int > (long)INT_MAX) {
            if (!key->has_maximum ||
                (key->environment_range_policy &
                 SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM) == 0) {
                return SIXEL_OPTION_ENVIRONMENT_INVALID;
            }
            parsed_int = (long)key->maximum;
        }
    } else if (errno == ERANGE || parsed_int < (long)INT_MIN ||
               parsed_int > (long)INT_MAX ||
               (expected_kind != SIXEL_SUBOPTION_VALUE_CHOICE &&
                endptr[0] != '\0')) {
        return SIXEL_OPTION_ENVIRONMENT_INVALID;
    }

    if (expected_kind == SIXEL_SUBOPTION_VALUE_UINT) {
        if (parsed_int < 0L ||
            (key->has_minimum && (double)parsed_int < key->minimum) ||
            (key->has_maximum && (double)parsed_int > key->maximum)) {
            return SIXEL_OPTION_ENVIRONMENT_RANGE;
        }
        value->uint_value = (unsigned int)parsed_int;
        return SIXEL_OPTION_ENVIRONMENT_MATCH;
    }
    if (expected_kind == SIXEL_SUBOPTION_VALUE_INT) {
        if (key->has_minimum && (double)parsed_int < key->minimum) {
            if ((key->environment_range_policy &
                 SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM) == 0) {
                return SIXEL_OPTION_ENVIRONMENT_RANGE;
            }
            parsed_int = (long)key->minimum;
        }
        if (key->has_maximum && (double)parsed_int > key->maximum) {
            if ((key->environment_range_policy &
                 SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM) == 0) {
                return SIXEL_OPTION_ENVIRONMENT_RANGE;
            }
            parsed_int = (long)key->maximum;
        }
        value->int_value = (int)parsed_int;
        return SIXEL_OPTION_ENVIRONMENT_MATCH;
    }
    if (expected_kind == SIXEL_SUBOPTION_VALUE_CHOICE &&
        (key->environment_range_policy &
         SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG_PREFIX) != 0) {
        while (index < key->choice_count) {
            if (key->choices[index].value == (int)parsed_int) {
                value->int_value = (int)parsed_int;
                return SIXEL_OPTION_ENVIRONMENT_MATCH;
            }
            ++index;
        }
        return SIXEL_OPTION_ENVIRONMENT_INVALID;
    }

    return SIXEL_OPTION_ENVIRONMENT_INVALID;
}

SIXEL_INTERNAL_API sixel_option_environment_result_t
sixel_option_registry_resolve_runtime_binding(
    char const *binding_identifier,
    sixel_suboption_value_kind_t expected_kind,
    sixel_suboption_value_t *value)
{
    return sixel_option_registry_resolve_process_binding(
        SIXEL_OPTION_SCHEMA_RUNTIME_POLICY,
        binding_identifier,
        expected_kind,
        value);
}

static int
sixel_diagnostics_boolean_value(
    char const *binding_identifier,
    int configured_value,
    int override_active,
    int fallback)
{
    sixel_suboption_value_t value;

    memset(&value, 0, sizeof(value));
    if (override_active) {
        return configured_value;
    }
    if (sixel_option_registry_resolve_process_binding(
            SIXEL_OPTION_SCHEMA_DIAGNOSTICS,
            binding_identifier,
            SIXEL_SUBOPTION_VALUE_BOOLEAN,
            &value) == SIXEL_OPTION_ENVIRONMENT_MATCH) {
        return value.int_value;
    }

    return fallback;
}

SIXEL_INTERNAL_API int
sixel_diagnostics_mode_is_code(void)
{
    sixel_suboption_value_t value;
    sixel_diagnostics_policy_options_t options;

    memset(&value, 0, sizeof(value));
    memset(&options, 0, sizeof(options));
    sixel_diagnostics_policy_load(&options);
    if (options.mode_override) {
        return options.mode == SIXEL_DIAGNOSTICS_MODE_CODE;
    }
    if (sixel_option_resolve_scalar_environment(
            SIXEL_OPTION_SCHEMA_DIAGNOSTICS,
            &value,
            NULL,
            0u) == SIXEL_OPTION_ENVIRONMENT_MATCH) {
        return value.int_value == SIXEL_DIAGNOSTICS_MODE_CODE;
    }

    return 0;
}

SIXEL_INTERNAL_API int
sixel_diagnostics_quiet_is_enabled(void)
{
    sixel_diagnostics_policy_options_t options;

    memset(&options, 0, sizeof(options));
    sixel_diagnostics_policy_load(&options);
    return sixel_diagnostics_boolean_value(
        SIXEL_SUBOPTION_BINDING_ID_2(quiet, quiet_override),
        options.quiet,
        options.quiet_override,
        0);
}

SIXEL_INTERNAL_API int
sixel_diagnostics_prefix_suggestions_are_enabled(void)
{
    sixel_diagnostics_policy_options_t options;

    memset(&options, 0, sizeof(options));
    sixel_diagnostics_policy_load(&options);
    return sixel_diagnostics_boolean_value(
        SIXEL_SUBOPTION_BINDING_ID_2(
            prefix_suggestions,
            prefix_suggestions_override),
        options.prefix_suggestions,
        options.prefix_suggestions_override,
        options.cli_suggestion_defaults);
}

SIXEL_INTERNAL_API int
sixel_diagnostics_fuzzy_suggestions_are_enabled(void)
{
    sixel_diagnostics_policy_options_t options;

    memset(&options, 0, sizeof(options));
    sixel_diagnostics_policy_load(&options);
    return sixel_diagnostics_boolean_value(
        SIXEL_SUBOPTION_BINDING_ID_2(
            fuzzy_suggestions,
            fuzzy_suggestions_override),
        options.fuzzy_suggestions,
        options.fuzzy_suggestions_override,
        options.cli_suggestion_defaults);
}

SIXEL_INTERNAL_API int
sixel_diagnostics_path_suggestions_are_enabled(void)
{
    sixel_diagnostics_policy_options_t options;

    memset(&options, 0, sizeof(options));
    sixel_diagnostics_policy_load(&options);
    return sixel_diagnostics_boolean_value(
        SIXEL_SUBOPTION_BINDING_ID_2(
            path_suggestions,
            path_suggestions_override),
        options.path_suggestions,
        options.path_suggestions_override,
        0);
}

SIXEL_INTERNAL_API int
sixel_diagnostics_force_colors_is_enabled(void)
{
    sixel_diagnostics_policy_options_t options;

    memset(&options, 0, sizeof(options));
    sixel_diagnostics_policy_load(&options);
    return sixel_diagnostics_boolean_value(
        SIXEL_SUBOPTION_BINDING_ID_2(force_colors, force_colors_override),
        options.force_colors,
        options.force_colors_override,
        0);
}

SIXEL_INTERNAL_API int
sixel_diagnostics_handoff_trace_is_enabled(void)
{
    sixel_diagnostics_policy_options_t options;

    memset(&options, 0, sizeof(options));
    sixel_diagnostics_policy_load(&options);
    return sixel_diagnostics_boolean_value(
        SIXEL_SUBOPTION_BINDING_ID_2(
            handoff_trace,
            handoff_trace_override),
        options.handoff_trace,
        options.handoff_trace_override,
        0);
}

SIXEL_INTERNAL_API int
sixel_diagnostics_psd_trace_is_enabled(void)
{
    sixel_diagnostics_policy_options_t options;

    memset(&options, 0, sizeof(options));
    sixel_diagnostics_policy_load(&options);
    return sixel_diagnostics_boolean_value(
        SIXEL_SUBOPTION_BINDING_ID_2(psd_trace, psd_trace_override),
        options.psd_trace,
        options.psd_trace_override,
        0);
}

SIXEL_INTERNAL_API int
sixel_diagnostics_psd_header_only_is_enabled(void)
{
    sixel_diagnostics_policy_options_t options;

    memset(&options, 0, sizeof(options));
    sixel_diagnostics_policy_load(&options);
    return sixel_diagnostics_boolean_value(
        SIXEL_SUBOPTION_BINDING_ID_2(
            psd_header_only,
            psd_header_only_override),
        options.psd_header_only,
        options.psd_header_only_override,
        0);
}

SIXEL_INTERNAL_API int
sixel_diagnostics_abort_trace_is_enabled(void)
{
    sixel_diagnostics_policy_options_t options;

    memset(&options, 0, sizeof(options));
    sixel_diagnostics_policy_load(&options);
    return sixel_diagnostics_boolean_value(
        SIXEL_SUBOPTION_BINDING_ID_2(abort_trace, abort_trace_override),
        options.abort_trace,
        options.abort_trace_override,
        1);
}

SIXEL_INTERNAL_API int
sixel_diagnostics_trace_topic_is_enabled(char const *topic)
{
    sixel_suboption_value_t value;
    int configured;
    int enabled;

    memset(&value, 0, sizeof(value));
    configured = 0;
    enabled = sixel_diagnostics_policy_trace_topic_is_enabled(
        topic,
        &configured);
    if (configured) {
        return enabled;
    }
    if (sixel_option_registry_resolve_process_binding(
            SIXEL_OPTION_SCHEMA_DIAGNOSTICS,
            SIXEL_SUBOPTION_BINDING_ID_2(
                trace_topic,
                trace_topic_override),
            SIXEL_SUBOPTION_VALUE_STRING,
            &value) != SIXEL_OPTION_ENVIRONMENT_MATCH) {
        return 0;
    }
    return sixel_diagnostics_topic_list_contains(value.string_value, topic);
}

SIXEL_INTERNAL_API void
sixel_diagnostics_timeline_line_policy(int *enabled, int *stride)
{
    sixel_diagnostics_policy_options_t options;
    sixel_suboption_value_t value;
    sixel_option_environment_result_t result;

    memset(&options, 0, sizeof(options));
    memset(&value, 0, sizeof(value));
    result = SIXEL_OPTION_ENVIRONMENT_INVALID;
    if (enabled == NULL || stride == NULL) {
        return;
    }
    *enabled = 0;
    *stride = 1;
    sixel_diagnostics_policy_load(&options);
    if (options.log_lines_override) {
        *enabled = 1;
        *stride = options.log_lines;
        return;
    }
    result = sixel_option_registry_resolve_process_binding(
        SIXEL_OPTION_SCHEMA_DIAGNOSTICS,
        SIXEL_SUBOPTION_BINDING_ID_2(log_lines, log_lines_override),
        SIXEL_SUBOPTION_VALUE_INT,
        &value);
    if (result == SIXEL_OPTION_ENVIRONMENT_UNSET) {
        return;
    }
    *enabled = 1;
    if (result == SIXEL_OPTION_ENVIRONMENT_MATCH) {
        *stride = value.int_value;
    }
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

/* Long option names are stored canonically without leading dashes. */
static int
sixel_option_registry_option_name_is_valid(char const *name)
{
    size_t index;

    index = 0u;
    if (name == NULL || name[0] == '\0' || name[0] == '-') {
        return 0;
    }
    while (name[index] != '\0') {
        if ((name[index] < 'a' || name[index] > 'z') &&
            (name[index] < '0' || name[index] > '9') &&
            name[index] != '-') {
            return 0;
        }
        ++index;
    }
    return name[index - 1u] != '-';
}

/* Base values additionally preserve established underscore spellings. */
static int
sixel_option_registry_value_name_is_valid(char const *name)
{
    size_t index;

    index = 0u;
    if (name == NULL || name[0] == '\0' || name[0] == '-' ||
        name[0] == '_') {
        return 0;
    }
    while (name[index] != '\0') {
        if ((name[index] < 'a' || name[index] > 'z') &&
            (name[index] < '0' || name[index] > '9') &&
            name[index] != '-' && name[index] != '_') {
            return 0;
        }
        ++index;
    }
    return name[index - 1u] != '-' && name[index - 1u] != '_';
}

static int
sixel_option_registry_binding_kind_is_valid(
    sixel_suboption_key_t const *key)
{
    if (key == NULL) {
        return 0;
    }

    switch (key->value_kind) {
    case SIXEL_SUBOPTION_VALUE_CHOICE:
    case SIXEL_SUBOPTION_VALUE_BOOLEAN:
    case SIXEL_SUBOPTION_VALUE_INT:
    case SIXEL_SUBOPTION_VALUE_SCALED_U8:
        return key->binding.storage_kind == SIXEL_SUBOPTION_STORAGE_INT;
    case SIXEL_SUBOPTION_VALUE_CHOICE_LIST:
        return key->binding.storage_kind == SIXEL_SUBOPTION_STORAGE_UINT;
    case SIXEL_SUBOPTION_VALUE_UINT:
        return key->binding.storage_kind == SIXEL_SUBOPTION_STORAGE_UINT ||
            key->binding.storage_kind == SIXEL_SUBOPTION_STORAGE_INT;
    case SIXEL_SUBOPTION_VALUE_SIZE:
        return key->binding.storage_kind == SIXEL_SUBOPTION_STORAGE_SIZE;
    case SIXEL_SUBOPTION_VALUE_FLOAT:
        return key->binding.storage_kind == SIXEL_SUBOPTION_STORAGE_FLOAT;
    case SIXEL_SUBOPTION_VALUE_DOUBLE:
        return key->binding.storage_kind == SIXEL_SUBOPTION_STORAGE_DOUBLE;
    case SIXEL_SUBOPTION_VALUE_INT_PAIR:
        return key->binding.storage_kind ==
            SIXEL_SUBOPTION_STORAGE_INT_PAIR;
    case SIXEL_SUBOPTION_VALUE_STRING:
        return key->binding.storage_kind == SIXEL_SUBOPTION_STORAGE_STRING;
    default:
        break;
    }

    return 0;
}

/*
 * Consumer visibility and storage ownership are separate axes.  A row may be
 * attached to a schema shared by both frontends, but its binding must stay in
 * the same encoder or decoder family as the consumers allowed to see it.
 */
static int
sixel_option_registry_binding_scope_is_valid(
    sixel_suboption_key_t const *key,
    sixel_option_argument_schema_t const *schema)
{
    unsigned int allowed_scope;

    allowed_scope = 0u;
    if (key == NULL || schema == NULL || key->consumer_scope == 0u ||
        (key->consumer_scope & ~SIXEL_OPTION_SCOPE_ALL) != 0u ||
        (key->consumer_scope & ~schema->scope) != 0u) {
        return 0;
    }

    switch (key->binding.target_class) {
    case SIXEL_SUBOPTION_TARGET_ENCODER:
    case SIXEL_SUBOPTION_TARGET_LOADER:
        allowed_scope = SIXEL_REGISTRY_ENCODER_CONSUMER_SCOPE;
        break;
    case SIXEL_SUBOPTION_TARGET_DECODER:
    case SIXEL_SUBOPTION_TARGET_DEQUANTIZE:
        allowed_scope = SIXEL_REGISTRY_DECODER_CONSUMER_SCOPE;
        break;
    case SIXEL_SUBOPTION_TARGET_RUNTIME:
    case SIXEL_SUBOPTION_TARGET_DIAGNOSTICS:
    case SIXEL_SUBOPTION_TARGET_CLIPBOARD:
        allowed_scope = SIXEL_OPTION_SCOPE_ALL;
        break;
    default:
        return 0;
    }

    return (key->consumer_scope & ~allowed_scope) == 0u;
}

/* Every row must be reachable through one declared schema and base. */
static int
sixel_option_registry_owner_is_valid(sixel_suboption_key_t const *key)
{
    sixel_option_argument_schema_t const *schema;
    unsigned long long valid_base_mask;
    size_t base_index;

    schema = NULL;
    valid_base_mask = 0ULL;
    base_index = 0u;
    if (key == NULL) {
        return 0;
    }
    schema = sixel_option_registry_get(key->option_id);
    if (schema == NULL) {
        return 0;
    }
    if (key->base_def == NULL && key->base_mask == 0ULL) {
        return schema->value_count > 0u;
    }
    if (key->base_def == NULL) {
        if (schema->value_count == 0u ||
            schema->value_count > sizeof(key->base_mask) * CHAR_BIT) {
            return 0;
        }
        valid_base_mask = schema->value_count ==
                sizeof(key->base_mask) * CHAR_BIT
            ? ULLONG_MAX
            : (1ULL << (unsigned int)schema->value_count) - 1ULL;
        return (key->base_mask & ~valid_base_mask) == 0ULL;
    }
    if (key->base_mask != 0ULL) {
        return 0;
    }
    while (base_index < schema->value_count) {
        if (key->base_def == schema->values + base_index) {
            return 1;
        }
        ++base_index;
    }

    return 0;
}

/* Value fields must never overlap control fields in any binding. */
static int
sixel_option_registry_binding_roles_conflict(
    sixel_suboption_binding_t const *left,
    sixel_suboption_binding_t const *right)
{
    size_t left_values[2];
    size_t left_controls[2];
    size_t right_values[2];
    size_t right_controls[2];
    size_t value_index;
    size_t control_index;

    left_values[0] = SIXEL_SUBOPTION_OFFSET_NONE;
    left_values[1] = SIXEL_SUBOPTION_OFFSET_NONE;
    left_controls[0] = SIXEL_SUBOPTION_OFFSET_NONE;
    left_controls[1] = SIXEL_SUBOPTION_OFFSET_NONE;
    right_values[0] = SIXEL_SUBOPTION_OFFSET_NONE;
    right_values[1] = SIXEL_SUBOPTION_OFFSET_NONE;
    right_controls[0] = SIXEL_SUBOPTION_OFFSET_NONE;
    right_controls[1] = SIXEL_SUBOPTION_OFFSET_NONE;
    value_index = 0u;
    control_index = 0u;
    if (left == NULL || right == NULL ||
        left->target_class != right->target_class) {
        return 0;
    }

    left_values[0] = left->value_offset;
    left_values[1] = left->second_value_offset;
    left_controls[0] = left->override_offset;
    left_controls[1] = left->mirror_offset;
    right_values[0] = right->value_offset;
    right_values[1] = right->second_value_offset;
    right_controls[0] = right->override_offset;
    right_controls[1] = right->mirror_offset;

    while (value_index < 2u) {
        control_index = 0u;
        while (control_index < 2u) {
            if (left_values[value_index] != SIXEL_SUBOPTION_OFFSET_NONE &&
                left_values[value_index] == right_controls[control_index]) {
                return 1;
            }
            if (right_values[value_index] != SIXEL_SUBOPTION_OFFSET_NONE &&
                right_values[value_index] == left_controls[control_index]) {
                return 1;
            }
            ++control_index;
        }
        ++value_index;
    }

    return 0;
}

/* Distinct semantic rows may not claim the same value field. */
static int
sixel_option_registry_binding_values_conflict(
    sixel_suboption_key_t const *left_key,
    sixel_suboption_key_t const *right_key)
{
    sixel_suboption_binding_t const *left;
    sixel_suboption_binding_t const *right;
    size_t left_values[2];
    size_t right_values[2];
    size_t left_index;
    size_t right_index;

    left = NULL;
    right = NULL;
    left_values[0] = SIXEL_SUBOPTION_OFFSET_NONE;
    left_values[1] = SIXEL_SUBOPTION_OFFSET_NONE;
    right_values[0] = SIXEL_SUBOPTION_OFFSET_NONE;
    right_values[1] = SIXEL_SUBOPTION_OFFSET_NONE;
    left_index = 0u;
    right_index = 0u;
    if (left_key == NULL || right_key == NULL) {
        return 0;
    }
    left = &left_key->binding;
    right = &right_key->binding;
    if (left->target_class != right->target_class) {
        return 0;
    }

    /* Alternative bases may expose exactly the same semantic binding. */
    if (left_key->option_id == right_key->option_id &&
        left_key->name != NULL && right_key->name != NULL &&
        strcmp(left_key->name, right_key->name) == 0 &&
        left->identifier != NULL && right->identifier != NULL &&
        strcmp(left->identifier, right->identifier) == 0 &&
        left->storage_kind == right->storage_kind &&
        left->value_offset == right->value_offset &&
        left->second_value_offset == right->second_value_offset &&
        left->override_offset == right->override_offset &&
        left->mirror_offset == right->mirror_offset) {
        return 0;
    }

    left_values[0] = left->value_offset;
    left_values[1] = left->second_value_offset;
    right_values[0] = right->value_offset;
    right_values[1] = right->second_value_offset;
    while (left_index < 2u) {
        right_index = 0u;
        while (right_index < 2u) {
            if (left_values[left_index] != SIXEL_SUBOPTION_OFFSET_NONE &&
                left_values[left_index] == right_values[right_index]) {
                return 1;
            }
            ++right_index;
        }
        ++left_index;
    }

    return 0;
}

static int
sixel_option_registry_validate_uncached(void)
{
    size_t option_index;
    size_t previous_option_index;
    size_t base_index;
    size_t previous_base_index;
    size_t key_index;
    size_t previous_index;
    size_t key_count;
    size_t suboption_index;
    size_t previous_suboption_index;
    size_t choice_index;
    size_t previous_choice_index;
    size_t unique_choice_count;
    sixel_option_argument_schema_t const *schema;
    sixel_option_argument_schema_t const *previous_schema;
    sixel_option_value_schema_t const *base_def;
    sixel_option_value_schema_t const *previous_base_def;
    sixel_suboption_key_t const *key;
    sixel_suboption_key_t const *previous;
    int default_found;
    int environment_choice_value_found;

    option_index = 0u;
    previous_option_index = 0u;
    base_index = 0u;
    previous_base_index = 0u;
    key_index = 0u;
    previous_index = 0u;
    key_count = 0u;
    suboption_index = 0u;
    previous_suboption_index = 0u;
    choice_index = 0u;
    previous_choice_index = 0u;
    unique_choice_count = 0u;
    schema = NULL;
    previous_schema = NULL;
    base_def = NULL;
    previous_base_def = NULL;
    key = NULL;
    previous = NULL;
    default_found = 0;
    environment_choice_value_found = 0;

    if (SIXEL_REGISTRY_ARRAY_LENGTH(g_options) !=
        (size_t)SIXEL_OPTION_SCHEMA_COUNT) {
        return 0;
    }

    while (suboption_index <
           SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
        key = g_suboptions + suboption_index;
        if (!sixel_option_registry_owner_is_valid(key) ||
            !sixel_option_registry_environment_name_is_valid(
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
        if (sixel_option_registry_binding_roles_conflict(&key->binding,
                                                         &key->binding)) {
            return 0;
        }
        previous_suboption_index = 0u;
        while (previous_suboption_index < suboption_index) {
            previous = g_suboptions + previous_suboption_index;
            if (sixel_option_registry_binding_roles_conflict(
                    &key->binding,
                    &previous->binding) ||
                sixel_option_registry_binding_values_conflict(
                    key,
                    previous)) {
                return 0;
            }
            ++previous_suboption_index;
        }
        ++suboption_index;
    }

    while (option_index < SIXEL_REGISTRY_ARRAY_LENGTH(g_options)) {
        schema = g_options + option_index;
        if ((unsigned int)schema->option_id >=
                (unsigned int)SIXEL_OPTION_SCHEMA_COUNT ||
            (size_t)schema->option_id != option_index ||
            schema->scope == 0u ||
            (schema->scope & ~SIXEL_OPTION_SCOPE_ALL) != 0u ||
            schema->optflag <= 0 ||
            !sixel_option_registry_option_name_is_valid(
                schema->option_name) ||
            (schema->argument_form != SIXEL_OPTION_ARGUMENT_SINGLE &&
             schema->argument_form != SIXEL_OPTION_ARGUMENT_LIST) ||
            (schema->argument_match_flags &
             ~(SIXEL_OPTION_MATCH_PREFIX |
               SIXEL_OPTION_MATCH_CASE_INSENSITIVE)) != 0u ||
            (schema->environment_match_flags &
             ~(SIXEL_OPTION_MATCH_PREFIX |
               SIXEL_OPTION_MATCH_CASE_INSENSITIVE)) != 0u ||
            !sixel_option_registry_environment_name_is_valid(
                schema->env_name,
                0) ||
            !sixel_option_registry_environment_name_is_valid(
                schema->env_fallback_name,
                0) ||
            !sixel_option_registry_environment_name_is_valid(
                schema->env_legacy_name,
                0) ||
            (schema->default_policy != SIXEL_OPTION_DEFAULT_FIXED &&
             schema->default_policy != SIXEL_OPTION_DEFAULT_OWNER) ||
            (schema->values == NULL) != (schema->value_count == 0u) ||
            (schema->default_policy == SIXEL_OPTION_DEFAULT_OWNER &&
             schema->default_value.int_value != 0)) {
            return 0;
        }
        if (schema->value_kind == SIXEL_SUBOPTION_VALUE_STRUCTURED) {
            if (schema->values == NULL || schema->value_count == 0u ||
                schema->has_minimum || schema->has_maximum ||
                schema->environment_has_minimum ||
                schema->environment_has_maximum ||
                schema->allow_zero ||
                schema->environment_range_policy !=
                    SIXEL_SUBOPTION_ENV_RANGE_REJECT ||
                schema->invalid_value_message != NULL ||
                schema->decoder_invalid_value_message != NULL ||
                schema->invalid_value_suffix != NULL ||
                schema->minimum_error_message != NULL ||
                schema->maximum_error_message != NULL ||
                schema->environment_invalid_value_message != NULL ||
                schema->range_error_message != NULL) {
                return 0;
            }
        } else {
            if (schema->argument_form != SIXEL_OPTION_ARGUMENT_SINGLE ||
                (schema->value_kind != SIXEL_SUBOPTION_VALUE_CHOICE &&
                 schema->value_kind != SIXEL_SUBOPTION_VALUE_INT &&
                 schema->value_kind != SIXEL_SUBOPTION_VALUE_UINT &&
                 schema->value_kind != SIXEL_SUBOPTION_VALUE_STRING)) {
                return 0;
            }
            if (schema->value_kind == SIXEL_SUBOPTION_VALUE_CHOICE &&
                (schema->values == NULL || schema->value_count == 0u)) {
                return 0;
            }
            if (schema->value_kind != SIXEL_SUBOPTION_VALUE_INT &&
                schema->value_kind != SIXEL_SUBOPTION_VALUE_UINT &&
                (schema->range_error_message != NULL ||
                 schema->minimum_error_message != NULL ||
                 schema->maximum_error_message != NULL)) {
                return 0;
            }
            if ((schema->decoder_invalid_value_message != NULL &&
                 (schema->scope & SIXEL_OPTION_SCOPE_DECODER) == 0u) ||
                (schema->minimum_error_message != NULL &&
                 !schema->has_minimum) ||
                (schema->maximum_error_message != NULL &&
                 !schema->has_maximum) ||
                (schema->has_minimum && schema->has_maximum &&
                 schema->minimum > schema->maximum) ||
                (schema->environment_has_minimum &&
                 schema->environment_has_maximum &&
                 schema->environment_minimum >
                    schema->environment_maximum)) {
                return 0;
            }
            if (schema->value_kind == SIXEL_SUBOPTION_VALUE_STRING &&
                (schema->values != NULL || schema->has_minimum ||
                 schema->has_maximum ||
                 schema->environment_has_minimum ||
                 schema->environment_has_maximum || schema->allow_zero ||
                 schema->environment_range_policy !=
                    SIXEL_SUBOPTION_ENV_RANGE_REJECT)) {
                return 0;
            }
        }
        if ((schema->environment_choices == NULL) !=
                (schema->environment_choice_count == 0u) ||
            (schema->environment_choice_count > 0u &&
             schema->value_kind != SIXEL_SUBOPTION_VALUE_CHOICE &&
             schema->value_kind != SIXEL_SUBOPTION_VALUE_STRUCTURED)) {
            return 0;
        }
        choice_index = 0u;
        while (choice_index < schema->environment_choice_count) {
            if (!sixel_option_registry_value_name_is_valid(
                    schema->environment_choices[choice_index].name)) {
                return 0;
            }
            environment_choice_value_found = 0;
            base_index = 0u;
            while (base_index < schema->value_count) {
                if (schema->environment_choices[choice_index].value ==
                    schema->values[base_index].value) {
                    environment_choice_value_found = 1;
                    break;
                }
                ++base_index;
            }
            if (!environment_choice_value_found) {
                return 0;
            }
            previous_choice_index = 0u;
            while (previous_choice_index < choice_index) {
                if (strcmp(
                        schema->environment_choices[
                            previous_choice_index].name,
                        schema->environment_choices[choice_index].name) ==
                    0) {
                    return 0;
                }
                ++previous_choice_index;
            }
            ++choice_index;
        }
        if ((schema->environment_range_policy &
             ~(SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM |
               SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM |
               SIXEL_SUBOPTION_ENV_RANGE_CLAMP_POSITIVE_MINIMUM |
               SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG |
               SIXEL_SUBOPTION_ENV_RANGE_CLAMP_UINT_WIDTH |
               SIXEL_SUBOPTION_ENV_RANGE_REJECT_UINT_WIDTH |
               SIXEL_SUBOPTION_ENV_RANGE_PARSE_UNSIGNED_LONG |
               SIXEL_SUBOPTION_ENV_RANGE_CLAMP_SIZE_WIDTH |
               SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG_PREFIX)) != 0 ||
            ((schema->environment_range_policy &
              SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM) != 0 &&
             !schema->environment_has_minimum) ||
            ((schema->environment_range_policy &
              SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM) != 0 &&
             !schema->environment_has_maximum)) {
            return 0;
        }
        suboption_index = 0u;
        while (suboption_index <
               SIXEL_REGISTRY_ARRAY_LENGTH(g_suboptions)) {
            key = g_suboptions + suboption_index;
            if ((schema->env_name != NULL &&
                 strcmp(schema->env_name, key->env_name) == 0) ||
                (schema->env_fallback_name != NULL &&
                 strcmp(schema->env_fallback_name, key->env_name) == 0) ||
                (schema->env_legacy_name != NULL &&
                 strcmp(schema->env_legacy_name, key->env_name) == 0)) {
                return 0;
            }
            ++suboption_index;
        }
        previous_option_index = 0u;
        while (previous_option_index < option_index) {
            previous_schema = g_options + previous_option_index;
            if (previous_schema->option_id == schema->option_id ||
                ((previous_schema->scope & schema->scope) != 0u &&
                 (previous_schema->optflag == schema->optflag ||
                  strcmp(previous_schema->option_name,
                         schema->option_name) == 0))) {
                return 0;
            }
            if (schema->env_name != NULL &&
                previous_schema->env_name != NULL &&
                strcmp(previous_schema->env_name,
                       schema->env_name) == 0) {
                return 0;
            }
            ++previous_option_index;
        }
        default_found = 0;
        base_index = 0u;
        while (base_index < schema->value_count) {
            base_def = schema->values + base_index;
            if (!sixel_option_registry_value_name_is_valid(
                    base_def->name) ||
                (schema->value_kind != SIXEL_SUBOPTION_VALUE_STRUCTURED &&
                 (base_def->common_suboption_offset != 0u ||
                  base_def->base_policy != SIXEL_OPTION_BASE_POLICY_NONE))) {
                return 0;
            }
            previous_base_index = 0u;
            while (previous_base_index < base_index) {
                previous_base_def =
                    schema->values + previous_base_index;
                if (strcmp(previous_base_def->name,
                           base_def->name) == 0) {
                    return 0;
                }
                ++previous_base_index;
            }
            if (schema->default_policy == SIXEL_OPTION_DEFAULT_FIXED &&
                base_def->value == schema->default_value.int_value) {
                default_found = 1;
            }
            key_count = sixel_option_registry_suboption_count(
                schema,
                base_def);
            if (schema->value_kind != SIXEL_SUBOPTION_VALUE_STRUCTURED &&
                key_count != 0u) {
                return 0;
            }
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
                    !sixel_option_registry_binding_scope_is_valid(
                        key,
                        schema) ||
                    key->binding.value_offset ==
                        SIXEL_SUBOPTION_OFFSET_NONE ||
                    key->binding.identifier == NULL ||
                    key->binding.identifier[0] == '\0' ||
                    !sixel_option_registry_binding_kind_is_valid(key)) {
                    return 0;
                }
                if (key->value_kind == SIXEL_SUBOPTION_VALUE_UINT &&
                    key->binding.storage_kind ==
                        SIXEL_SUBOPTION_STORAGE_INT &&
                    (!key->has_maximum ||
                     key->maximum > (double)INT_MAX)) {
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
                if (key->value_kind ==
                        SIXEL_SUBOPTION_VALUE_CHOICE_LIST) {
                    if (key->choices == NULL || key->choice_count == 0u ||
                        key->environment_choices != NULL ||
                        key->environment_choice_count != 0u ||
                        key->has_minimum || key->has_maximum ||
                        key->allow_zero ||
                        key->environment_range_policy !=
                            SIXEL_SUBOPTION_ENV_RANGE_REJECT) {
                        return 0;
                    }
                    unique_choice_count = 0u;
                    choice_index = 0u;
                    while (choice_index < key->choice_count) {
                        if (!sixel_option_registry_value_name_is_valid(
                                key->choices[choice_index].name) ||
                            key->choices[choice_index].value < 0 ||
                            key->choices[choice_index].value >
                                (int)
                                SIXEL_SUBOPTION_CHOICE_LIST_VALUE_MASK) {
                            return 0;
                        }
                        previous_choice_index = 0u;
                        while (previous_choice_index < choice_index) {
                            if (strcmp(
                                    key->choices[previous_choice_index].name,
                                    key->choices[choice_index].name) == 0) {
                                return 0;
                            }
                            if (key->choices[previous_choice_index].value ==
                                key->choices[choice_index].value) {
                                break;
                            }
                            ++previous_choice_index;
                        }
                        if (previous_choice_index == choice_index) {
                            ++unique_choice_count;
                        }
                        ++choice_index;
                    }
                    if (unique_choice_count >
                        SIXEL_SUBOPTION_CHOICE_LIST_MAX) {
                        return 0;
                    }
                }
                if (key->value_kind == SIXEL_SUBOPTION_VALUE_BOOLEAN &&
                    (key->choices != NULL || key->choice_count != 0u ||
                     key->environment_choices != NULL ||
                     key->environment_choice_count != 0u ||
                     key->binding.storage_kind !=
                        SIXEL_SUBOPTION_STORAGE_INT)) {
                    return 0;
                }
                if (key->value_kind == SIXEL_SUBOPTION_VALUE_STRING &&
                    (key->choices != NULL || key->choice_count != 0u ||
                     key->environment_choices != NULL ||
                     key->environment_choice_count != 0u ||
                     key->has_minimum || key->has_maximum ||
                     key->allow_zero ||
                     key->environment_range_policy !=
                        SIXEL_SUBOPTION_ENV_RANGE_REJECT ||
                     key->binding.storage_kind !=
                        SIXEL_SUBOPTION_STORAGE_STRING)) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     ~(SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM |
                       SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM |
                       SIXEL_SUBOPTION_ENV_RANGE_CLAMP_POSITIVE_MINIMUM |
                       SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG |
                       SIXEL_SUBOPTION_ENV_RANGE_CLAMP_UINT_WIDTH |
                       SIXEL_SUBOPTION_ENV_RANGE_REJECT_UINT_WIDTH |
                       SIXEL_SUBOPTION_ENV_RANGE_PARSE_UNSIGNED_LONG |
                       SIXEL_SUBOPTION_ENV_RANGE_PARSE_DIGITS_ONLY |
                       SIXEL_SUBOPTION_ENV_RANGE_SATURATE_UNSIGNED_LONG |
                       SIXEL_SUBOPTION_ENV_RANGE_CLAMP_SIZE_WIDTH |
                       SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG_PREFIX)) !=
                    0) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM) != 0 &&
                    (key->environment_range_policy &
                     SIXEL_SUBOPTION_ENV_RANGE_CLAMP_POSITIVE_MINIMUM) != 0) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     (SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM |
                      SIXEL_SUBOPTION_ENV_RANGE_CLAMP_POSITIVE_MINIMUM)) !=
                        0 &&
                    !key->has_minimum) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     SIXEL_SUBOPTION_ENV_RANGE_CLAMP_POSITIVE_MINIMUM) != 0 &&
                    key->value_kind != SIXEL_SUBOPTION_VALUE_UINT) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     (SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG |
                      SIXEL_SUBOPTION_ENV_RANGE_CLAMP_UINT_WIDTH |
                      SIXEL_SUBOPTION_ENV_RANGE_REJECT_UINT_WIDTH |
                      SIXEL_SUBOPTION_ENV_RANGE_PARSE_UNSIGNED_LONG |
                      SIXEL_SUBOPTION_ENV_RANGE_PARSE_DIGITS_ONLY |
                      SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG_PREFIX)) !=
                        0 &&
                    key->value_kind != SIXEL_SUBOPTION_VALUE_UINT &&
                    !((key->environment_range_policy &
                       (SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG |
                        SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG_PREFIX)) !=
                          0 &&
                      (key->value_kind == SIXEL_SUBOPTION_VALUE_INT ||
                       key->value_kind == SIXEL_SUBOPTION_VALUE_CHOICE))) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     (SIXEL_SUBOPTION_ENV_RANGE_SATURATE_UNSIGNED_LONG |
                      SIXEL_SUBOPTION_ENV_RANGE_CLAMP_SIZE_WIDTH)) != 0 &&
                    key->value_kind != SIXEL_SUBOPTION_VALUE_SIZE) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     SIXEL_SUBOPTION_ENV_RANGE_SATURATE_UNSIGNED_LONG) != 0 &&
                    (key->environment_range_policy &
                     (SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG |
                      SIXEL_SUBOPTION_ENV_RANGE_CLAMP_UINT_WIDTH |
                      SIXEL_SUBOPTION_ENV_RANGE_REJECT_UINT_WIDTH |
                      SIXEL_SUBOPTION_ENV_RANGE_PARSE_UNSIGNED_LONG |
                      SIXEL_SUBOPTION_ENV_RANGE_PARSE_DIGITS_ONLY)) != 0) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     SIXEL_SUBOPTION_ENV_RANGE_CLAMP_SIZE_WIDTH) != 0 &&
                    (key->environment_range_policy &
                     ~SIXEL_SUBOPTION_ENV_RANGE_CLAMP_SIZE_WIDTH) != 0) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG_PREFIX) !=
                        0 &&
                    (key->environment_range_policy &
                     ~(SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG_PREFIX |
                       SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM |
                       SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM)) != 0) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG) != 0 &&
                    (key->environment_range_policy &
                     (SIXEL_SUBOPTION_ENV_RANGE_CLAMP_UINT_WIDTH |
                      SIXEL_SUBOPTION_ENV_RANGE_REJECT_UINT_WIDTH |
                      SIXEL_SUBOPTION_ENV_RANGE_PARSE_UNSIGNED_LONG)) != 0) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     SIXEL_SUBOPTION_ENV_RANGE_PARSE_UNSIGNED_LONG) != 0 &&
                    (key->environment_range_policy &
                     (SIXEL_SUBOPTION_ENV_RANGE_CLAMP_UINT_WIDTH |
                      SIXEL_SUBOPTION_ENV_RANGE_REJECT_UINT_WIDTH)) !=
                        SIXEL_SUBOPTION_ENV_RANGE_REJECT_UINT_WIDTH) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     SIXEL_SUBOPTION_ENV_RANGE_PARSE_DIGITS_ONLY) != 0 &&
                    (key->environment_range_policy &
                     (SIXEL_SUBOPTION_ENV_RANGE_PARSE_SIGNED_LONG |
                      SIXEL_SUBOPTION_ENV_RANGE_PARSE_UNSIGNED_LONG)) != 0) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     SIXEL_SUBOPTION_ENV_RANGE_CLAMP_UINT_WIDTH) != 0 &&
                    (key->environment_range_policy &
                     SIXEL_SUBOPTION_ENV_RANGE_REJECT_UINT_WIDTH) != 0) {
                    return 0;
                }
                if ((key->environment_range_policy &
                     SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM) != 0 &&
                    !key->has_maximum) {
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
                        ((previous->consumer_scope &
                          key->consumer_scope) != 0u &&
                         (strcmp(previous->name, key->name) == 0 ||
                          previous->short_name == key->short_name))) {
                        return 0;
                    }
                    ++previous_index;
                }
                ++key_index;
            }
            ++base_index;
        }
        if (schema->default_policy == SIXEL_OPTION_DEFAULT_FIXED &&
            schema->value_count > 0u && !default_found) {
            return 0;
        }
        ++option_index;
    }

    return 1;
}

/*
 * The registry is immutable, so validate its O(n squared) cross-row
 * invariants exactly once.  Every consumer can then keep the runtime guard
 * without multiplying initialization cost or introducing a data race.
 */
static int sixel_option_registry_validation_result = 0;

#if defined(SIXEL_ENABLE_THREADS) && SIXEL_ENABLE_THREADS
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
     && !defined(WITH_WINPTHREAD)
static INIT_ONCE sixel_option_registry_validation_once =
    INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK
sixel_option_registry_validate_once_cb(PINIT_ONCE once,
                                       PVOID parameter,
                                       PVOID *context)
{
    (void)once;
    (void)parameter;
    (void)context;

    sixel_option_registry_validation_result =
        sixel_option_registry_validate_uncached();
    return TRUE;
}
# else
static pthread_once_t sixel_option_registry_validation_once =
    PTHREAD_ONCE_INIT;

static void
sixel_option_registry_validate_once_cb(void)
{
    sixel_option_registry_validation_result =
        sixel_option_registry_validate_uncached();
}
# endif
#else
static int sixel_option_registry_validation_initialized = 0;
#endif

int
sixel_option_registry_validate(void)
{
#if defined(SIXEL_ENABLE_THREADS) && SIXEL_ENABLE_THREADS
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__) \
     && !defined(WITH_WINPTHREAD)
    BOOL initialized;

    initialized = InitOnceExecuteOnce(
        &sixel_option_registry_validation_once,
        sixel_option_registry_validate_once_cb,
        NULL,
        NULL);
    return initialized && sixel_option_registry_validation_result;
# else
    int status;

    status = pthread_once(&sixel_option_registry_validation_once,
                          sixel_option_registry_validate_once_cb);
    return status == 0 && sixel_option_registry_validation_result;
# endif
#else
    if (!sixel_option_registry_validation_initialized) {
        sixel_option_registry_validation_result =
            sixel_option_registry_validate_uncached();
        sixel_option_registry_validation_initialized = 1;
    }
    return sixel_option_registry_validation_result;
#endif
}

/*
 * Scalar option values use the same registry metadata in every consumer.
 * Keeping this parser with the immutable table also lets small embedders use
 * scalar options without linking the CLI-oriented options.c object.
 */
static int
sixel_option_registry_ascii_equal(char left, char right, int ignore_case)
{
    unsigned char left_byte;
    unsigned char right_byte;

    left_byte = (unsigned char)left;
    right_byte = (unsigned char)right;
    if (ignore_case != 0) {
        left_byte = (unsigned char)tolower(left_byte);
        right_byte = (unsigned char)tolower(right_byte);
    }
    return left_byte == right_byte;
}

static int
sixel_option_registry_name_matches(char const *name,
                                   char const *text,
                                   unsigned int flags)
{
    size_t index;
    int ignore_case;

    index = 0u;
    ignore_case =
        (flags & SIXEL_OPTION_MATCH_CASE_INSENSITIVE) != 0u;
    if (name == NULL || text == NULL || text[0] == '\0') {
        return 0;
    }
    while (text[index] != '\0') {
        if (name[index] == '\0' ||
            !sixel_option_registry_ascii_equal(name[index],
                                               text[index],
                                               ignore_case)) {
            return 0;
        }
        ++index;
    }
    return name[index] == '\0' ||
        (flags & SIXEL_OPTION_MATCH_PREFIX) != 0u;
}

static int
sixel_option_registry_match_choice(
    sixel_option_argument_schema_t const *schema,
    char const *text,
    int environment_value,
    int *matched_value)
{
    sixel_suboption_choice_t const *environment_choices;
    size_t environment_choice_count;
    size_t index;
    int candidate;
    int candidate_set;
    int ambiguous;
    unsigned int flags;

    environment_choices = NULL;
    environment_choice_count = 0u;
    index = 0u;
    candidate = 0;
    candidate_set = 0;
    ambiguous = 0;
    if (schema == NULL || text == NULL || matched_value == NULL) {
        return 0;
    }
    flags = environment_value != 0
        ? schema->environment_match_flags
        : schema->argument_match_flags;
    environment_choices = schema->environment_choices;
    environment_choice_count = schema->environment_choice_count;
    if (environment_value != 0 && environment_choice_count > 0u) {
        while (index < environment_choice_count) {
            if (sixel_option_registry_name_matches(
                    environment_choices[index].name,
                    text,
                    flags)) {
                if (!candidate_set) {
                    candidate = environment_choices[index].value;
                    candidate_set = 1;
                } else if (candidate != environment_choices[index].value) {
                    ambiguous = 1;
                }
            }
            ++index;
        }
    } else {
        while (index < schema->value_count) {
            if (sixel_option_registry_name_matches(
                    schema->values[index].name,
                    text,
                    flags)) {
                if (!candidate_set) {
                    candidate = schema->values[index].value;
                    candidate_set = 1;
                } else if (candidate != schema->values[index].value) {
                    ambiguous = 1;
                }
            }
            ++index;
        }
    }
    if (!candidate_set || ambiguous) {
        return 0;
    }
    *matched_value = candidate;
    return 1;
}

static void
sixel_option_registry_set_diagnostic(char *diagnostic,
                                     size_t diagnostic_size,
                                     char const *message)
{
    if (diagnostic == NULL || diagnostic_size == 0u || message == NULL) {
        return;
    }
    (void)sixel_compat_snprintf(diagnostic,
                                diagnostic_size,
                                "%s",
                                message);
}

static void
sixel_option_registry_report_scalar_error(
    int environment_value,
    char *diagnostic,
    size_t diagnostic_size,
    char const *message)
{
    sixel_option_registry_set_diagnostic(diagnostic,
                                         diagnostic_size,
                                         message);
    if (environment_value == 0 && message != NULL) {
        sixel_helper_set_additional_message(message);
    }
}

static SIXELSTATUS
sixel_option_registry_parse_scalar(
    sixel_option_argument_schema_t const *schema,
    unsigned int consumer_scope,
    char const *text,
    int environment_value,
    sixel_suboption_value_t *value,
    char *diagnostic,
    size_t diagnostic_size)
{
    char *endptr;
    long parsed_int;
    unsigned long long parsed_uint;
    unsigned int flags;
    double minimum;
    double maximum;
    char const *invalid_message;
    int has_minimum;
    int has_maximum;
    int range_error;
    int minimum_error;
    int maximum_error;

    endptr = NULL;
    parsed_int = 0L;
    parsed_uint = 0ULL;
    flags = SIXEL_SUBOPTION_ENV_RANGE_REJECT;
    minimum = 0.0;
    maximum = 0.0;
    invalid_message = NULL;
    has_minimum = 0;
    has_maximum = 0;
    range_error = 0;
    minimum_error = 0;
    maximum_error = 0;
    if (diagnostic != NULL && diagnostic_size > 0u) {
        diagnostic[0] = '\0';
    }
    if (schema == NULL || text == NULL || text[0] == '\0' ||
        value == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    minimum = schema->minimum;
    maximum = schema->maximum;
    has_minimum = schema->has_minimum;
    has_maximum = schema->has_maximum;
    invalid_message = schema->invalid_value_message;
    if (environment_value != 0) {
        minimum = schema->environment_minimum;
        maximum = schema->environment_maximum;
        has_minimum = schema->environment_has_minimum;
        has_maximum = schema->environment_has_maximum;
    } else if ((consumer_scope & SIXEL_OPTION_SCOPE_DECODER) != 0u &&
               schema->decoder_invalid_value_message != NULL) {
        invalid_message = schema->decoder_invalid_value_message;
    }
    if (schema->value_kind == SIXEL_SUBOPTION_VALUE_STRUCTURED ||
        schema->value_kind == SIXEL_SUBOPTION_VALUE_CHOICE ||
        schema->value_count > 0u) {
        if (sixel_option_registry_match_choice(schema,
                                               text,
                                               environment_value,
                                               &value->int_value)) {
            return SIXEL_OK;
        }
        if (schema->value_kind == SIXEL_SUBOPTION_VALUE_STRUCTURED ||
            schema->value_kind == SIXEL_SUBOPTION_VALUE_CHOICE) {
            sixel_option_registry_report_scalar_error(
                environment_value,
                diagnostic,
                diagnostic_size,
                invalid_message);
            return SIXEL_BAD_ARGUMENT;
        }
    }
    if (environment_value != 0) {
        flags = (unsigned int)schema->environment_range_policy;
    }
    if (schema->value_kind == SIXEL_SUBOPTION_VALUE_INT) {
        errno = 0;
        parsed_int = strtol(text, &endptr, 10);
        if (endptr == text || endptr == NULL || endptr[0] != '\0') {
            goto invalid;
        }
        if (errno == ERANGE || parsed_int < (long)INT_MIN ||
            parsed_int > (long)INT_MAX) {
            range_error = 1;
            goto invalid;
        }
        if (has_minimum &&
            (double)parsed_int < minimum &&
            (flags & SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM) != 0u) {
            parsed_int = (long)minimum;
        }
        if (has_maximum &&
            (double)parsed_int > maximum &&
            (flags & SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM) != 0u) {
            parsed_int = (long)maximum;
        }
        if (has_minimum && (double)parsed_int < minimum) {
            minimum_error = 1;
            goto invalid;
        }
        if (has_maximum && (double)parsed_int > maximum) {
            maximum_error = 1;
            goto invalid;
        }
        value->int_value = (int)parsed_int;
        return SIXEL_OK;
    }
    if (schema->value_kind == SIXEL_SUBOPTION_VALUE_UINT) {
        if (text[0] == '-') {
            minimum_error = 1;
            goto invalid;
        }
        errno = 0;
        parsed_uint = strtoull(text, &endptr, 10);
        if (endptr == text || endptr == NULL || endptr[0] != '\0') {
            goto invalid;
        }
        if (errno == ERANGE || parsed_uint > (unsigned long long)UINT_MAX) {
            range_error = 1;
            goto invalid;
        }
        if (has_minimum &&
            (double)parsed_uint < minimum &&
            (flags & SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MINIMUM) != 0u) {
            parsed_uint = (unsigned long long)minimum;
        }
        if (has_maximum &&
            (double)parsed_uint > maximum &&
            (flags & SIXEL_SUBOPTION_ENV_RANGE_CLAMP_MAXIMUM) != 0u) {
            parsed_uint = (unsigned long long)maximum;
        }
        if (has_minimum && (double)parsed_uint < minimum) {
            minimum_error = 1;
            goto invalid;
        }
        if (has_maximum && (double)parsed_uint > maximum) {
            maximum_error = 1;
            goto invalid;
        }
        value->uint_value = (unsigned int)parsed_uint;
        return SIXEL_OK;
    }
    if (schema->value_kind == SIXEL_SUBOPTION_VALUE_STRING) {
        value->string_value = text;
        return SIXEL_OK;
    }

invalid:
    sixel_option_registry_report_scalar_error(
        environment_value,
        diagnostic,
        diagnostic_size,
        environment_value == 0 && minimum_error &&
            schema->minimum_error_message != NULL
            ? schema->minimum_error_message
            : environment_value == 0 && maximum_error &&
                schema->maximum_error_message != NULL
                ? schema->maximum_error_message
                : range_error && schema->range_error_message != NULL
            ? schema->range_error_message
            : environment_value != 0 &&
                schema->environment_invalid_value_message != NULL
                ? schema->environment_invalid_value_message
                : invalid_message);
    return SIXEL_BAD_ARGUMENT;
}

char const *
sixel_option_resolve_argument_environment(
    sixel_option_schema_id_t option_id)
{
    sixel_option_argument_schema_t const *schema;
    char const *text;

    schema = sixel_option_registry_get(option_id);
    text = NULL;
    if (schema == NULL || !sixel_option_registry_validate()) {
        return NULL;
    }
    if (schema->env_name != NULL) {
        text = sixel_compat_getenv(schema->env_name);
        if (text != NULL && text[0] != '\0') {
            return text;
        }
    }
    if (schema->env_fallback_name != NULL) {
        text = sixel_compat_getenv(schema->env_fallback_name);
        if (text != NULL && text[0] != '\0') {
            return text;
        }
    }
    if (schema->env_legacy_name != NULL) {
        text = sixel_compat_getenv(schema->env_legacy_name);
        if (text != NULL && text[0] != '\0') {
            return text;
        }
    }
    return NULL;
}

int
sixel_option_argument_environment_is_present(
    sixel_option_schema_id_t option_id)
{
    sixel_option_argument_schema_t const *schema;

    schema = sixel_option_registry_get(option_id);
    if (schema == NULL || !sixel_option_registry_validate()) {
        return 0;
    }
    if (schema->env_name != NULL &&
        sixel_compat_getenv(schema->env_name) != NULL) {
        return 1;
    }
    if (schema->env_fallback_name != NULL &&
        sixel_compat_getenv(schema->env_fallback_name) != NULL) {
        return 1;
    }
    if (schema->env_legacy_name != NULL &&
        sixel_compat_getenv(schema->env_legacy_name) != NULL) {
        return 1;
    }
    return 0;
}

int
sixel_option_encoder_environment_is_present(int optflag)
{
    size_t index;

    index = 0u;
    while (index < SIXEL_REGISTRY_ARRAY_LENGTH(g_options)) {
        if (g_options[index].optflag == optflag &&
            (g_options[index].scope &
             SIXEL_OPTION_SCOPE_ENCODER) != 0u) {
            return sixel_option_argument_environment_is_present(
                g_options[index].option_id);
        }
        ++index;
    }
    return 0;
}

int
sixel_option_resolve_scalar_environment(
    sixel_option_schema_id_t option_id,
    sixel_suboption_value_t *value,
    char *diagnostic,
    size_t diagnostic_size)
{
    sixel_option_argument_schema_t const *schema;
    char const *text;

    schema = sixel_option_registry_get(option_id);
    text = NULL;
    if (schema == NULL || value == NULL ||
        !sixel_option_registry_validate()) {
        return SIXEL_OPTION_ENVIRONMENT_INVALID;
    }
    text = sixel_option_resolve_argument_environment(option_id);
    if (text == NULL) {
        return SIXEL_OPTION_ENVIRONMENT_UNSET;
    }
    if (SIXEL_FAILED(sixel_option_registry_parse_scalar(schema,
                                                        0u,
                                                        text,
                                                        1,
                                                        value,
                                                        diagnostic,
                                                        diagnostic_size))) {
        return SIXEL_OPTION_ENVIRONMENT_INVALID;
    }
    return SIXEL_OPTION_ENVIRONMENT_MATCH;
}

SIXELSTATUS
sixel_option_parse_scalar_argument(
    sixel_option_schema_id_t option_id,
    unsigned int consumer_scope,
    char const *argument,
    sixel_suboption_value_t *value,
    char *diagnostic,
    size_t diagnostic_size)
{
    sixel_option_argument_schema_t const *schema;

    schema = sixel_option_registry_get(option_id);
    if (schema == NULL || !sixel_option_registry_validate()) {
        return SIXEL_BAD_ARGUMENT;
    }
    return sixel_option_registry_parse_scalar(schema,
                                              consumer_scope,
                                              argument,
                                              0,
                                              value,
                                              diagnostic,
                                              diagnostic_size);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
