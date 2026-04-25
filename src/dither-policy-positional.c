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

#include <stdlib.h>
#include <string.h>

#include "dither-policy-a-dither.h"
#include "dither-policy-x-dither.h"
#include "dither-policy-bluenoise.h"
#include "dither-policy-backend.h"
#include "sixel_atomic.h"

/*
 * IDL (internal contract)
 *
 * class DitherPositional : IDitherPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   apply(request);
 *   supports_parallel_bands();
 * }
 */

typedef struct sixel_dither_policy_positional_object {
    sixel_dither_policy_interface_t base;
    sixel_atomic_u32_t ref;
    int method_for_scan;
    int pixelformat;
} sixel_dither_policy_positional_object_t;

static sixel_dither_policy_positional_object_t *
sixel_dither_policy_positional_from_base(
    sixel_dither_policy_interface_t *policy)
{
    return (sixel_dither_policy_positional_object_t *)(void *)policy;
}

static sixel_dither_policy_positional_object_t const *
sixel_dither_policy_positional_from_base_const(
    sixel_dither_policy_interface_t const *policy)
{
    return (sixel_dither_policy_positional_object_t const *)(void const *)
        policy;
}

static void
sixel_dither_policy_positional_ref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_positional_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_positional_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_dither_policy_positional_unref(sixel_dither_policy_interface_t *policy)
{
    sixel_dither_policy_positional_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;

    if (policy == NULL) {
        return;
    }

    object = sixel_dither_policy_positional_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        free(object);
    }
}

static SIXELSTATUS
sixel_dither_policy_positional_prepare(
    sixel_dither_policy_interface_t *policy,
    sixel_dither_policy_prepare_request_t const *request)
{
    sixel_dither_policy_positional_object_t *object;

    object = NULL;

    if (policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_positional_from_base(policy);
    object->method_for_scan = request->method_for_scan;
    object->pixelformat = request->pixelformat;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_dither_policy_positional_apply_method(
    sixel_dither_policy_interface_t const *policy,
    sixel_dither_policy_apply_request_t const *request,
    int method_for_diffuse)
{
    sixel_dither_policy_positional_object_t const *object;
    sixel_dither_policy_apply_request_t effective;

    object = NULL;
    memset(&effective, 0, sizeof(effective));

    if (policy == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_dither_policy_positional_from_base_const(policy);
    effective = *request;
    effective.method_for_scan = object->method_for_scan;
    effective.pixelformat = object->pixelformat;

    return sixel_dither_policy_apply_positional_backend(&effective,
                                                        method_for_diffuse);
}

#define DEFINE_POSITIONAL_DITHER_POLICY(CLASS, METHOD)                      \
static SIXELSTATUS                                                          \
sixel_dither_policy_##CLASS##_apply(                                        \
    sixel_dither_policy_interface_t *policy,                                \
    sixel_dither_policy_apply_request_t const *request)                     \
{                                                                            \
    return sixel_dither_policy_positional_apply_method(policy,              \
                                                        request,             \
                                                        METHOD);             \
}                                                                            \
                                                                             \
static int                                                                   \
sixel_dither_policy_##CLASS##_supports_parallel_bands(                      \
    sixel_dither_policy_interface_t const *policy)                           \
{                                                                            \
    (void)policy;                                                            \
    return 1;                                                                \
}                                                                            \
                                                                             \
static sixel_dither_policy_vtbl_t const                                      \
g_sixel_dither_policy_##CLASS##_vtbl = {                                    \
    sixel_dither_policy_positional_ref,                                      \
    sixel_dither_policy_positional_unref,                                    \
    sixel_dither_policy_positional_prepare,                                  \
    sixel_dither_policy_##CLASS##_apply,                                     \
    sixel_dither_policy_##CLASS##_supports_parallel_bands                    \
};                                                                           \
                                                                             \
SIXELSTATUS                                                                  \
sixel_dither_policy_create_##CLASS(                                          \
    sixel_dither_policy_interface_t **policy)                                \
{                                                                            \
    sixel_dither_policy_positional_object_t *object;                         \
                                                                             \
    object = NULL;                                                           \
    if (policy == NULL) {                                                    \
        return SIXEL_BAD_ARGUMENT;                                           \
    }                                                                        \
    *policy = NULL;                                                          \
                                                                             \
    object = (sixel_dither_policy_positional_object_t *)                     \
        malloc(sizeof(*object));                                             \
    if (object == NULL) {                                                    \
        return SIXEL_BAD_ALLOCATION;                                         \
    }                                                                        \
                                                                             \
    object->base.vtbl = &g_sixel_dither_policy_##CLASS##_vtbl;              \
    object->ref = 1U;                                                        \
    object->method_for_scan = SIXEL_SCAN_AUTO;                               \
    object->pixelformat = SIXEL_PIXELFORMAT_RGB888;                          \
                                                                             \
    *policy = &object->base;                                                 \
    return SIXEL_OK;                                                         \
}

DEFINE_POSITIONAL_DITHER_POLICY(a_dither, SIXEL_DIFFUSE_A_DITHER)
DEFINE_POSITIONAL_DITHER_POLICY(x_dither, SIXEL_DIFFUSE_X_DITHER)
DEFINE_POSITIONAL_DITHER_POLICY(bluenoise, SIXEL_DIFFUSE_BLUENOISE_DITHER)

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
