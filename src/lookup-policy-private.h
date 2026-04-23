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

#ifndef LIBSIXEL_LOOKUP_POLICY_PRIVATE_H
#define LIBSIXEL_LOOKUP_POLICY_PRIVATE_H

#include <limits.h>

#include "lookup-policy.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IDL (internal contract)
 *
 * interface ILookupPolicyFactoryInternals {
 *   create_lookup_normal(out policy);
 *   create_lookup_mono_darkbg(out policy);
 *   create_lookup_mono_lightbg(out policy);
 *   create_lookup_certlut(out policy);
 *   create_lookup_5bit(out policy);
 *   create_lookup_6bit(out policy);
 *   create_lookup_eytzinger(out policy);
 *   create_lookup_fhedt(out policy);
 *   create_lookup_vptree(out policy);
 *   create_lookup_rbc(out policy);
 *   create_lookup_mahalanobis(out policy);
 * }
 *
 * Ownership/lifetime:
 * - Each constructor returns refcount=1 policy objects.
 * - Callers release with policy->vtbl->unref(policy).
 *
 * Creation path:
 * - lookup-policy.c registry resolves class names to constructors.
 */

typedef SIXELSTATUS (*sixel_lookup_policy_create_fn)(
    sixel_lookup_policy_interface_t **policy);

typedef struct sixel_lookup_policy_registry_entry {
    char const *name;
    sixel_lookup_policy_create_fn create;
} sixel_lookup_policy_registry_entry_t;

static inline SIXELSTATUS
sixel_lookup_policy_validate_complexion_limit(int depth, int complexion)
{
    enum { max_channel_diff_sq = 255 * 255 };
    int non_weighted_components;
    long long weighted_budget;
    long long max_complexion;

    if (complexion <= 1) {
        return SIXEL_OK;
    }

    non_weighted_components = (depth > 1) ? (depth - 1) : 0;
    weighted_budget = (long long)INT_MAX
        - (long long)max_channel_diff_sq * (long long)non_weighted_components;
    if (weighted_budget <= 0) {
        max_complexion = 0;
    } else {
        max_complexion = weighted_budget / (long long)max_channel_diff_sq;
    }

    if ((long long)complexion > max_complexion) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_prepare: complexion parameter is too large.");
        return SIXEL_BAD_ARGUMENT;
    }

    return SIXEL_OK;
}

static inline int
sixel_lookup_policy_normalize_fast_lut_policy(int lut_policy)
{
    int normalized;

    normalized = lut_policy;
    if (normalized != SIXEL_LUT_POLICY_CERTLUT
        && normalized != SIXEL_LUT_POLICY_5BIT
        && normalized != SIXEL_LUT_POLICY_6BIT
        && normalized != SIXEL_LUT_POLICY_EYTZINGER
        && normalized != SIXEL_LUT_POLICY_FHEDT
        && normalized != SIXEL_LUT_POLICY_VPTREE
        && normalized != SIXEL_LUT_POLICY_RBC
        && normalized != SIXEL_LUT_POLICY_MAHALANOBIS) {
        normalized = SIXEL_LUT_POLICY_6BIT;
    }

    return normalized;
}

static inline int
sixel_lookup_policy_shared_cache_enabled(int lut_policy)
{
    int shared;

    shared = 1;
    if (lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
        shared = sixel_lookup_env_shared_certlut();
    } else if (lut_policy == SIXEL_LUT_POLICY_5BIT) {
        shared = sixel_lookup_env_shared_5bit();
    } else if (lut_policy == SIXEL_LUT_POLICY_6BIT) {
        shared = sixel_lookup_env_shared_6bit();
    }

    return shared;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_create_normal(sixel_lookup_policy_interface_t **policy);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_create_mono_darkbg(
    sixel_lookup_policy_interface_t **policy);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_create_mono_lightbg(
    sixel_lookup_policy_interface_t **policy);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_create_certlut(sixel_lookup_policy_interface_t **policy);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_create_5bit(sixel_lookup_policy_interface_t **policy);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_create_6bit(sixel_lookup_policy_interface_t **policy);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_create_eytzinger(
    sixel_lookup_policy_interface_t **policy);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_create_fhedt(sixel_lookup_policy_interface_t **policy);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_create_vptree(sixel_lookup_policy_interface_t **policy);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_create_rbc(sixel_lookup_policy_interface_t **policy);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_create_mahalanobis(
    sixel_lookup_policy_interface_t **policy);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_LOOKUP_POLICY_PRIVATE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
