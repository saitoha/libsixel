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

#include "lookup-policy.h"


static char const g_lookup_policy_name_none[] = "lookup/none";
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
        return g_lookup_policy_name_none;
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
        return g_lookup_policy_name_none;
    }

    normalized_lut_policy = sixel_lookup_policy_normalize_fast_lut_policy(
        request->lut_policy);
    return sixel_lookup_policy_name_from_lut_policy(normalized_lut_policy);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
