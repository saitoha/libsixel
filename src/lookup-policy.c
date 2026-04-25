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
#include "config.h"
#endif

#include <stddef.h>
#include <string.h>

#include "lookup-policy.h"
#include "lookup-policy-normal.h"
#include "lookup-policy-mono-darkbg.h"
#include "lookup-policy-mono-lightbg.h"
#include "lookup-policy-certlut.h"
#include "lookup-policy-5bit.h"
#include "lookup-policy-6bit.h"
#include "lookup-policy-eytzinger.h"
#include "lookup-policy-fhedt.h"
#include "lookup-policy-vptree.h"
#include "lookup-policy-rbc.h"
#include "lookup-policy-mahalanobis.h"

/*
 * IDL (internal contract)
 *
 * interface ILookupPolicyDispatcher {
 *   select_name(select_request);
 *   create_by_name(name, out policy);
 * }
 *
 * Ownership/lifetime:
 * - create_by_name() returns refcount=1 objects from concrete classes.
 * - Callers use policy->vtbl methods directly.
 *
 * Creation path:
 * - name selector resolves "lookup/..." names.
 * - registry resolves names to per-class constructors.
 */

static char const g_lookup_policy_name_normal[] = "lookup/normal";
static char const g_lookup_policy_name_mono_darkbg[] = "lookup/mono-darkbg";
static char const g_lookup_policy_name_mono_lightbg[] = "lookup/mono-lightbg";
static char const g_lookup_policy_name_certlut[] = "lookup/certlut";
static char const g_lookup_policy_name_5bit[] = "lookup/5bit";
static char const g_lookup_policy_name_6bit[] = "lookup/6bit";
static char const g_lookup_policy_name_eytzinger[] = "lookup/eytzinger";
static char const g_lookup_policy_name_fhedt[] = "lookup/fhedt";
static char const g_lookup_policy_name_vptree[] = "lookup/vptree";
static char const g_lookup_policy_name_rbc[] = "lookup/rbc";
static char const g_lookup_policy_name_mahalanobis[] = "lookup/mahalanobis";

typedef SIXELSTATUS (*sixel_lookup_policy_create_fn)(
    sixel_lookup_policy_interface_t **policy);

typedef struct sixel_lookup_policy_registry_entry {
    char const *name;
    sixel_lookup_policy_create_fn create;
} sixel_lookup_policy_registry_entry_t;

static sixel_lookup_policy_registry_entry_t const g_lookup_policy_registry[] = {
    { g_lookup_policy_name_normal, sixel_lookup_policy_create_normal },
    { g_lookup_policy_name_mono_darkbg,
      sixel_lookup_policy_create_mono_darkbg },
    { g_lookup_policy_name_mono_lightbg,
      sixel_lookup_policy_create_mono_lightbg },
    { g_lookup_policy_name_certlut, sixel_lookup_policy_create_certlut },
    { g_lookup_policy_name_5bit, sixel_lookup_policy_create_5bit },
    { g_lookup_policy_name_6bit, sixel_lookup_policy_create_6bit },
    { g_lookup_policy_name_eytzinger,
      sixel_lookup_policy_create_eytzinger },
    { g_lookup_policy_name_fhedt, sixel_lookup_policy_create_fhedt },
    { g_lookup_policy_name_vptree, sixel_lookup_policy_create_vptree },
    { g_lookup_policy_name_rbc, sixel_lookup_policy_create_rbc },
    { g_lookup_policy_name_mahalanobis,
      sixel_lookup_policy_create_mahalanobis }
};

static char const *
sixel_lookup_policy_name_from_lut_policy(int lut_policy)
{
    switch (lut_policy) {
    case SIXEL_LUT_POLICY_CERTLUT:
        return g_lookup_policy_name_certlut;
    case SIXEL_LUT_POLICY_5BIT:
        return g_lookup_policy_name_5bit;
    case SIXEL_LUT_POLICY_6BIT:
        return g_lookup_policy_name_6bit;
    case SIXEL_LUT_POLICY_EYTZINGER:
        return g_lookup_policy_name_eytzinger;
    case SIXEL_LUT_POLICY_FHEDT:
        return g_lookup_policy_name_fhedt;
    case SIXEL_LUT_POLICY_VPTREE:
        return g_lookup_policy_name_vptree;
    case SIXEL_LUT_POLICY_RBC:
        return g_lookup_policy_name_rbc;
    case SIXEL_LUT_POLICY_MAHALANOBIS:
        return g_lookup_policy_name_mahalanobis;
    default:
        return g_lookup_policy_name_6bit;
    }
}

static int
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

static sixel_lookup_policy_registry_entry_t const *
sixel_lookup_policy_find_registry_entry(char const *name)
{
    size_t i;

    if (name == NULL) {
        return NULL;
    }

    for (i = 0; i < sizeof(g_lookup_policy_registry)
            / sizeof(g_lookup_policy_registry[0]); ++i) {
        if (strcmp(g_lookup_policy_registry[i].name, name) == 0) {
            return &g_lookup_policy_registry[i];
        }
    }

    return NULL;
}

char const *
sixel_lookup_policy_select_name(
    sixel_lookup_policy_select_request_t const *request)
{
    int sum1;
    int sum2;
    int n;
    int normalized_lut_policy;

    sum1 = 0;
    sum2 = 0;
    normalized_lut_policy = SIXEL_LUT_POLICY_6BIT;

    if (request == NULL) {
        return g_lookup_policy_name_normal;
    }

    if (request->reqcolor == 2 && request->palette != NULL
            && request->depth > 0) {
        for (n = 0; n < request->depth; ++n) {
            sum1 += request->palette[n];
        }
        for (n = request->depth;
                n < request->depth + request->depth;
                ++n) {
            sum2 += request->palette[n];
        }
        if (sum1 == 0 && sum2 == 255 * 3) {
            return g_lookup_policy_name_mono_darkbg;
        }
        if (sum1 == 255 * 3 && sum2 == 0) {
            return g_lookup_policy_name_mono_lightbg;
        }
    }

    if (request->optimize_lookup == 0
            || request->depth != 3
            || request->lut_policy == SIXEL_LUT_POLICY_NONE) {
        return g_lookup_policy_name_normal;
    }

    normalized_lut_policy = sixel_lookup_policy_normalize_fast_lut_policy(
        request->lut_policy);
    return sixel_lookup_policy_name_from_lut_policy(normalized_lut_policy);
}

SIXELSTATUS
sixel_lookup_policy_create_by_name(
    char const *name,
    sixel_lookup_policy_interface_t **policy)
{
    sixel_lookup_policy_registry_entry_t const *entry;

    entry = NULL;
    if (policy != NULL) {
        *policy = NULL;
    }

    if (name == NULL || policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    entry = sixel_lookup_policy_find_registry_entry(name);
    if (entry == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_create_by_name: unknown lookup class.");
        return SIXEL_BAD_ARGUMENT;
    }

    return entry->create(policy);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
