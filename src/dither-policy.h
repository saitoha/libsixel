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

#ifndef LIBSIXEL_DITHER_POLICY_H
#define LIBSIXEL_DITHER_POLICY_H

#include <sixel.h>

#include "lookup-policy.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IDL (internal contract)
 *
 * interface IDitherPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   apply(request);
 *   supports_parallel_bands();
 * }
 *
 * Ownership/lifetime:
 * - Factory create returns refcount=1 policy objects.
 * - Callers release with policy->vtbl->unref(policy).
 *
 * Creation path:
 * - sixel_dither_policy_select_name(select_request)
 * - services/factory -> create("dither/...", &policy)
 * - policy->prepare(request)
 */

typedef struct sixel_dither_policy_interface sixel_dither_policy_interface_t;

typedef int (*sixel_dither_lookup_map_fn)(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel);

typedef struct sixel_dither_policy_select_request {
    int method_for_diffuse;
    int ncolors;
    int pixelformat;
} sixel_dither_policy_select_request_t;

typedef struct sixel_dither_policy_prepare_request {
    sixel_dither_t *dither;
    int depth;
    int reqcolor;
    int method_for_scan;
    int pixelformat;
} sixel_dither_policy_prepare_request_t;

typedef struct sixel_dither_policy_apply_request {
    sixel_index_t *result;
    unsigned char *data;
    int width;
    int height;
    int band_origin;
    int output_start;
    int depth;
    unsigned char *palette;
    int method_for_scan;
    sixel_lookup_policy_interface_t *lookup_policy;
    sixel_dither_t *dither;
    int pixelformat;
} sixel_dither_policy_apply_request_t;

typedef int sixel_dither_policy_supports_parallel_result_t;

typedef struct sixel_dither_policy_vtbl {
    void (*ref)(sixel_dither_policy_interface_t *policy);
    void (*unref)(sixel_dither_policy_interface_t *policy);
    SIXELSTATUS (*prepare)(
        sixel_dither_policy_interface_t *policy,
        sixel_dither_policy_prepare_request_t const *request);
    SIXELSTATUS (*apply)(
        sixel_dither_policy_interface_t *policy,
        sixel_dither_policy_apply_request_t const *request);
    sixel_dither_policy_supports_parallel_result_t
    (*supports_parallel_bands)(sixel_dither_policy_interface_t const *policy);
} sixel_dither_policy_vtbl_t;

struct sixel_dither_policy_interface {
    sixel_dither_policy_vtbl_t const *vtbl;
};

SIXEL_INTERNAL_API char const *
sixel_dither_policy_select_name(
    sixel_dither_policy_select_request_t const *request);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_DITHER_POLICY_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
