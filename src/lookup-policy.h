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

#ifndef LIBSIXEL_LOOKUP_POLICY_H
#define LIBSIXEL_LOOKUP_POLICY_H

#include <sixel.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IDL (internal contract)
 *
 * interface ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 *   lookup_source_is_float();
 *   prefer_palette_float_lookup();
 * }
 *
 * Ownership/lifetime:
 * - Factory create returns refcount=1 policy objects.
 * - Callers release with policy->vtbl->unref(policy).
 *
 * Creation path:
 * - sixel_lookup_policy_select_name(select_request)
 * - services/factory -> create("lookup/...", &policy)
 * - create() resolves names via per-class constructors
 * - policy->prepare(request)
 *
 * Reuse path:
 * - Callers may pass a prepared policy object through
 *   request.reuse_policy/request.reuse_policy_slot.
 * - request.parallel_dither_active carries execution context so policies can
 *   disable non-thread-safe cache paths without relying on global state.
 * - Implementations may choose to ignore reuse hints.
 */

typedef struct sixel_lookup_policy_interface sixel_lookup_policy_interface_t;

typedef struct sixel_lookup_policy_select_request {
    unsigned char const *palette;
    int depth;
    int reqcolor;
    int optimize_lookup;
    int lut_policy;
} sixel_lookup_policy_select_request_t;

typedef struct sixel_lookup_policy_prepare_request {
    unsigned char const *palette;
    float const *palette_float;
    int depth;
    int float_depth;
    int reqcolor;
    int pixelformat;
    int parallel_dither_active;
    sixel_lookup_policy_interface_t *reuse_policy;
    sixel_lookup_policy_interface_t **reuse_policy_slot;
    sixel_allocator_t *allocator;
} sixel_lookup_policy_prepare_request_t;

typedef int sixel_lookup_policy_result_t;

typedef struct sixel_lookup_policy_vtbl {
    void (*ref)(sixel_lookup_policy_interface_t *policy);
    void (*unref)(sixel_lookup_policy_interface_t *policy);
    SIXELSTATUS (*prepare)(sixel_lookup_policy_interface_t *policy,
                           sixel_lookup_policy_prepare_request_t const
                           *request);
    sixel_lookup_policy_result_t
    (*map_pixel)(sixel_lookup_policy_interface_t const *policy,
                 unsigned char const *pixel);
    sixel_lookup_policy_result_t
    (*lookup_source_is_float)(sixel_lookup_policy_interface_t const *policy);
    sixel_lookup_policy_result_t
    (*prefer_palette_float_lookup)(
        sixel_lookup_policy_interface_t const *policy);
} sixel_lookup_policy_vtbl_t;

struct sixel_lookup_policy_interface {
    sixel_lookup_policy_vtbl_t const *vtbl;
};

SIXEL_INTERNAL_API char const *
sixel_lookup_policy_select_name(
    sixel_lookup_policy_select_request_t const *request);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_lookup_policy_create_by_name(
    char const *name,
    sixel_lookup_policy_interface_t **policy);

SIXEL_INTERNAL_API int
sixel_lookup_policy_certlut_shared_instance_enabled(void);

SIXEL_INTERNAL_API int
sixel_lookup_policy_5bit_shared_instance_enabled(void);

SIXEL_INTERNAL_API int
sixel_lookup_policy_6bit_shared_instance_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_LOOKUP_POLICY_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
