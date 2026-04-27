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

#include "dither-policy.h"


static char const g_dither_policy_name_none_8bit[] = "dither/none.8bit";
static char const g_dither_policy_name_none_float32[] = "dither/none.float32";
static char const g_dither_policy_name_fs_8bit[] = "dither/fs.8bit";
static char const g_dither_policy_name_fs_float32[] = "dither/fs.float32";
static char const g_dither_policy_name_atkinson_8bit[] = "dither/atkinson.8bit";
static char const g_dither_policy_name_atkinson_float32[] =
    "dither/atkinson.float32";
static char const g_dither_policy_name_jajuni_8bit[] = "dither/jajuni.8bit";
static char const g_dither_policy_name_jajuni_float32[] =
    "dither/jajuni.float32";
static char const g_dither_policy_name_stucki_8bit[] = "dither/stucki.8bit";
static char const g_dither_policy_name_stucki_float32[] =
    "dither/stucki.float32";
static char const g_dither_policy_name_burkes_8bit[] = "dither/burkes.8bit";
static char const g_dither_policy_name_burkes_float32[] =
    "dither/burkes.float32";
static char const g_dither_policy_name_sierra1_8bit[] = "dither/sierra1.8bit";
static char const g_dither_policy_name_sierra1_float32[] =
    "dither/sierra1.float32";
static char const g_dither_policy_name_sierra2_8bit[] = "dither/sierra2.8bit";
static char const g_dither_policy_name_sierra2_float32[] =
    "dither/sierra2.float32";
static char const g_dither_policy_name_sierra3_8bit[] = "dither/sierra3.8bit";
static char const g_dither_policy_name_sierra3_float32[] =
    "dither/sierra3.float32";
static char const g_dither_policy_name_lso2_8bit[] = "dither/lso2.8bit";
static char const g_dither_policy_name_lso2_float32[] = "dither/lso2.float32";
static char const g_dither_policy_name_a_dither_8bit[] =
    "dither/a_dither.8bit";
static char const g_dither_policy_name_a_dither_float32[] =
    "dither/a_dither.float32";
static char const g_dither_policy_name_x_dither_8bit[] =
    "dither/x_dither.8bit";
static char const g_dither_policy_name_x_dither_float32[] =
    "dither/x_dither.float32";
static char const g_dither_policy_name_bluenoise_8bit[] =
    "dither/bluenoise.8bit";
static char const g_dither_policy_name_bluenoise_float32[] =
    "dither/bluenoise.float32";
static char const g_dither_policy_name_interframe_8bit[] =
    "dither/interframe.8bit";
static char const g_dither_policy_name_interframe_float32[] =
    "dither/interframe.float32";

static int
sixel_dither_policy_select_prefers_float32(
    sixel_dither_policy_select_request_t const *request)
{
    if (request == NULL) {
        return 0;
    }

    return SIXEL_PIXELFORMAT_IS_FLOAT32(request->pixelformat);
}

char const *
sixel_dither_policy_select_name(
    sixel_dither_policy_select_request_t const *request)
{
    int method_for_diffuse;
    int ncolors;

    int prefer_float32;

    method_for_diffuse = SIXEL_DIFFUSE_FS;
    ncolors = 256;
    prefer_float32 = sixel_dither_policy_select_prefers_float32(request);

    if (request != NULL) {
        method_for_diffuse = request->method_for_diffuse;
        ncolors = request->ncolors;
    }

    if (method_for_diffuse == SIXEL_DIFFUSE_AUTO) {
        if (ncolors > 16) {
            method_for_diffuse = SIXEL_DIFFUSE_FS;
        } else {
            method_for_diffuse = SIXEL_DIFFUSE_ATKINSON;
        }
    }

    switch (method_for_diffuse) {
    case SIXEL_DIFFUSE_NONE:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_none_float32;
        }
        return g_dither_policy_name_none_8bit;
    case SIXEL_DIFFUSE_FS:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_fs_float32;
        }
        return g_dither_policy_name_fs_8bit;
    case SIXEL_DIFFUSE_ATKINSON:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_atkinson_float32;
        }
        return g_dither_policy_name_atkinson_8bit;
    case SIXEL_DIFFUSE_JAJUNI:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_jajuni_float32;
        }
        return g_dither_policy_name_jajuni_8bit;
    case SIXEL_DIFFUSE_STUCKI:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_stucki_float32;
        }
        return g_dither_policy_name_stucki_8bit;
    case SIXEL_DIFFUSE_BURKES:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_burkes_float32;
        }
        return g_dither_policy_name_burkes_8bit;
    case SIXEL_DIFFUSE_SIERRA1:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_sierra1_float32;
        }
        return g_dither_policy_name_sierra1_8bit;
    case SIXEL_DIFFUSE_SIERRA2:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_sierra2_float32;
        }
        return g_dither_policy_name_sierra2_8bit;
    case SIXEL_DIFFUSE_SIERRA3:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_sierra3_float32;
        }
        return g_dither_policy_name_sierra3_8bit;
    case SIXEL_DIFFUSE_LSO2:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_lso2_float32;
        }
        return g_dither_policy_name_lso2_8bit;
    case SIXEL_DIFFUSE_A_DITHER:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_a_dither_float32;
        }
        return g_dither_policy_name_a_dither_8bit;
    case SIXEL_DIFFUSE_X_DITHER:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_x_dither_float32;
        }
        return g_dither_policy_name_x_dither_8bit;
    case SIXEL_DIFFUSE_BLUENOISE_DITHER:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_bluenoise_float32;
        }
        return g_dither_policy_name_bluenoise_8bit;
    case SIXEL_DIFFUSE_INTERFRAME:
        if (prefer_float32 != 0) {
            return g_dither_policy_name_interframe_float32;
        }
        return g_dither_policy_name_interframe_8bit;
    default:
        break;
    }

    if (prefer_float32 != 0) {
        return g_dither_policy_name_fs_float32;
    }
    return g_dither_policy_name_fs_8bit;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
