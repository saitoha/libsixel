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


static char const g_lookup_policy_name_none_8bit[] = "lookup/none.8bit";
static char const g_lookup_policy_name_none_float32[] = "lookup/none.float32";
static char const g_lookup_policy_name_mono_darkbg_8bit[] =
    "lookup/mono-darkbg.8bit";
static char const g_lookup_policy_name_mono_darkbg_float32[] =
    "lookup/mono-darkbg.float32";
static char const g_lookup_policy_name_mono_lightbg_8bit[] =
    "lookup/mono-lightbg.8bit";
static char const g_lookup_policy_name_mono_lightbg_float32[] =
    "lookup/mono-lightbg.float32";
static char const g_lookup_policy_name_certlut_8bit[] = "lookup/certlut.8bit";
static char const g_lookup_policy_name_certlut_float32[] =
    "lookup/certlut.float32";
static char const g_lookup_policy_name_5bit_8bit[] = "lookup/5bit.8bit";
static char const g_lookup_policy_name_5bit_float32[] = "lookup/5bit.float32";
static char const g_lookup_policy_name_6bit_8bit[] = "lookup/6bit.8bit";
static char const g_lookup_policy_name_6bit_float32[] = "lookup/6bit.float32";
static char const g_lookup_policy_name_eytzinger_8bit[] =
    "lookup/eytzinger.8bit";
static char const g_lookup_policy_name_eytzinger_float32[] =
    "lookup/eytzinger.float32";
static char const g_lookup_policy_name_fhedt_8bit[] = "lookup/fhedt.8bit";
static char const g_lookup_policy_name_fhedt_float32[] =
    "lookup/fhedt.float32";
static char const g_lookup_policy_name_vptree_8bit[] = "lookup/vptree.8bit";
static char const g_lookup_policy_name_vptree_float32[] =
    "lookup/vptree.float32";
static char const g_lookup_policy_name_rbc_8bit[] = "lookup/rbc.8bit";
static char const g_lookup_policy_name_rbc_float32[] = "lookup/rbc.float32";
static char const g_lookup_policy_name_mahalanobis_8bit[] =
    "lookup/mahalanobis.8bit";
static char const g_lookup_policy_name_mahalanobis_float32[] =
    "lookup/mahalanobis.float32";

static int
sixel_lookup_policy_select_prefers_float32(
    sixel_lookup_policy_select_request_t const *request)
{
    if (request == NULL) {
        return 0;
    }

    return SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat);
}

static char const *
sixel_lookup_policy_name_from_lut_policy(int lut_policy,
                                         int prefer_float32)
{
    switch (lut_policy) {
    case SIXEL_LUT_POLICY_CERTLUT:
        if (prefer_float32 != 0) {
            return g_lookup_policy_name_certlut_float32;
        }
        return g_lookup_policy_name_certlut_8bit;
    case SIXEL_LUT_POLICY_5BIT:
        if (prefer_float32 != 0) {
            return g_lookup_policy_name_5bit_float32;
        }
        return g_lookup_policy_name_5bit_8bit;
    case SIXEL_LUT_POLICY_6BIT:
        if (prefer_float32 != 0) {
            return g_lookup_policy_name_6bit_float32;
        }
        return g_lookup_policy_name_6bit_8bit;
    case SIXEL_LUT_POLICY_EYTZINGER:
        if (prefer_float32 != 0) {
            return g_lookup_policy_name_eytzinger_float32;
        }
        return g_lookup_policy_name_eytzinger_8bit;
    case SIXEL_LUT_POLICY_FHEDT:
        if (prefer_float32 != 0) {
            return g_lookup_policy_name_fhedt_float32;
        }
        return g_lookup_policy_name_fhedt_8bit;
    case SIXEL_LUT_POLICY_VPTREE:
        if (prefer_float32 != 0) {
            return g_lookup_policy_name_vptree_float32;
        }
        return g_lookup_policy_name_vptree_8bit;
    case SIXEL_LUT_POLICY_RBC:
        if (prefer_float32 != 0) {
            return g_lookup_policy_name_rbc_float32;
        }
        return g_lookup_policy_name_rbc_8bit;
    case SIXEL_LUT_POLICY_MAHALANOBIS:
        if (prefer_float32 != 0) {
            return g_lookup_policy_name_mahalanobis_float32;
        }
        return g_lookup_policy_name_mahalanobis_8bit;
    default:
        if (prefer_float32 != 0) {
            return g_lookup_policy_name_6bit_float32;
        }
        return g_lookup_policy_name_6bit_8bit;
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
    int prefer_float32;

    sum1 = 0;
    sum2 = 0;
    normalized_lut_policy = SIXEL_LUT_POLICY_6BIT;
    prefer_float32 = sixel_lookup_policy_select_prefers_float32(request);

    if (request == NULL) {
        return g_lookup_policy_name_none_8bit;
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
            if (prefer_float32 != 0) {
                return g_lookup_policy_name_mono_darkbg_float32;
            }
            return g_lookup_policy_name_mono_darkbg_8bit;
        }
        if (sum1 == 255 * 3 && sum2 == 0) {
            if (prefer_float32 != 0) {
                return g_lookup_policy_name_mono_lightbg_float32;
            }
            return g_lookup_policy_name_mono_lightbg_8bit;
        }
    }

    if (request->optimize_lookup == 0
            || request->depth != 3
            || request->lut_policy == SIXEL_LUT_POLICY_NONE) {
        if (prefer_float32 != 0) {
            return g_lookup_policy_name_none_float32;
        }
        return g_lookup_policy_name_none_8bit;
    }

    normalized_lut_policy = sixel_lookup_policy_normalize_fast_lut_policy(
        request->lut_policy);
    return sixel_lookup_policy_name_from_lut_policy(normalized_lut_policy,
                                                    prefer_float32);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
