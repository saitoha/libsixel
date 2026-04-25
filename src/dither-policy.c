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


static char const g_dither_policy_name_none[] = "dither/none";
static char const g_dither_policy_name_fs[] = "dither/fs";
static char const g_dither_policy_name_atkinson[] = "dither/atkinson";
static char const g_dither_policy_name_jajuni[] = "dither/jajuni";
static char const g_dither_policy_name_stucki[] = "dither/stucki";
static char const g_dither_policy_name_burkes[] = "dither/burkes";
static char const g_dither_policy_name_sierra1[] = "dither/sierra1";
static char const g_dither_policy_name_sierra2[] = "dither/sierra2";
static char const g_dither_policy_name_sierra3[] = "dither/sierra3";
static char const g_dither_policy_name_lso2[] = "dither/lso2";
static char const g_dither_policy_name_a_dither[] = "dither/a_dither";
static char const g_dither_policy_name_x_dither[] = "dither/x_dither";
static char const g_dither_policy_name_bluenoise[] = "dither/bluenoise";
static char const g_dither_policy_name_interframe[] = "dither/interframe";

char const *
sixel_dither_policy_select_name(
    sixel_dither_policy_select_request_t const *request)
{
    int method_for_diffuse;
    int ncolors;

    method_for_diffuse = SIXEL_DIFFUSE_FS;
    ncolors = 256;

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
        return g_dither_policy_name_none;
    case SIXEL_DIFFUSE_FS:
        return g_dither_policy_name_fs;
    case SIXEL_DIFFUSE_ATKINSON:
        return g_dither_policy_name_atkinson;
    case SIXEL_DIFFUSE_JAJUNI:
        return g_dither_policy_name_jajuni;
    case SIXEL_DIFFUSE_STUCKI:
        return g_dither_policy_name_stucki;
    case SIXEL_DIFFUSE_BURKES:
        return g_dither_policy_name_burkes;
    case SIXEL_DIFFUSE_SIERRA1:
        return g_dither_policy_name_sierra1;
    case SIXEL_DIFFUSE_SIERRA2:
        return g_dither_policy_name_sierra2;
    case SIXEL_DIFFUSE_SIERRA3:
        return g_dither_policy_name_sierra3;
    case SIXEL_DIFFUSE_LSO2:
        return g_dither_policy_name_lso2;
    case SIXEL_DIFFUSE_A_DITHER:
        return g_dither_policy_name_a_dither;
    case SIXEL_DIFFUSE_X_DITHER:
        return g_dither_policy_name_x_dither;
    case SIXEL_DIFFUSE_BLUENOISE_DITHER:
        return g_dither_policy_name_bluenoise;
    case SIXEL_DIFFUSE_INTERFRAME:
        return g_dither_policy_name_interframe;
    default:
        break;
    }

    return g_dither_policy_name_fs;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
