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

#include "lookup-policy-mono-lightbg.h"
#include "sixel_atomic.h"

/*
 * IDL (internal contract)
 *
 * class LookupMonoLightBg : ILookupPolicy {
 *   ref();
 *   unref();
 *   prepare(request);
 *   map_pixel(pixel);
 * }
 */

typedef struct sixel_lookup_policy_mono_lightbg_object {
    sixel_lookup_policy_interface_t base;
    sixel_atomic_u32_t ref;
    int depth;
    int reqcolor;
} sixel_lookup_policy_mono_lightbg_object_t;

static sixel_lookup_policy_mono_lightbg_object_t *
sixel_lookup_policy_mono_lightbg_from_base(
    sixel_lookup_policy_interface_t *policy)
{
    return (sixel_lookup_policy_mono_lightbg_object_t *)(void *)policy;
}

static sixel_lookup_policy_mono_lightbg_object_t const *
sixel_lookup_policy_mono_lightbg_from_base_const(
    sixel_lookup_policy_interface_t const *policy)
{
    return (sixel_lookup_policy_mono_lightbg_object_t const *)(void const *)
        policy;
}

static void
sixel_lookup_policy_mono_lightbg_ref(sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_mono_lightbg_object_t *object;

    object = NULL;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_mono_lightbg_from_base(policy);
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1U);
}

static void
sixel_lookup_policy_mono_lightbg_unref(
    sixel_lookup_policy_interface_t *policy)
{
    sixel_lookup_policy_mono_lightbg_object_t *object;
    unsigned int previous;

    object = NULL;
    previous = 0U;
    if (policy == NULL) {
        return;
    }

    object = sixel_lookup_policy_mono_lightbg_from_base(policy);
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1U);
    if (previous == 1U) {
        free(object);
    }
}

static SIXELSTATUS
sixel_lookup_policy_mono_lightbg_prepare(
    sixel_lookup_policy_interface_t *policy,
    sixel_lookup_policy_prepare_request_t const *request)
{
    sixel_lookup_policy_mono_lightbg_object_t *object;

    object = NULL;
    if (policy == NULL || request == NULL || request->palette == NULL
            || request->depth <= 0 || request->reqcolor <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = sixel_lookup_policy_mono_lightbg_from_base(policy);
    object->depth = request->depth;
    object->reqcolor = request->reqcolor;

    return SIXEL_OK;
}

static int
sixel_lookup_policy_mono_lightbg_map_pixel(
    sixel_lookup_policy_interface_t const *policy,
    unsigned char const *pixel)
{
    sixel_lookup_policy_mono_lightbg_object_t const *object;
    int n;
    int distant;

    object = NULL;
    n = 0;
    distant = 0;
    if (policy == NULL || pixel == NULL) {
        return 0;
    }

    object = sixel_lookup_policy_mono_lightbg_from_base_const(policy);
    if (object->depth <= 0 || object->reqcolor <= 0) {
        return 0;
    }

    for (n = 0; n < object->depth; ++n) {
        distant += pixel[n];
    }

    return distant < 128 * object->reqcolor ? 1 : 0;
}

static sixel_lookup_policy_vtbl_t const
g_sixel_lookup_policy_mono_lightbg_vtbl = {
    sixel_lookup_policy_mono_lightbg_ref,
    sixel_lookup_policy_mono_lightbg_unref,
    sixel_lookup_policy_mono_lightbg_prepare,
    sixel_lookup_policy_mono_lightbg_map_pixel,
};

#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_lookup_policy_create_mono_lightbg(
    sixel_lookup_policy_interface_t **policy)
{
    sixel_lookup_policy_mono_lightbg_object_t *object;

    object = NULL;
    if (policy != NULL) {
        *policy = NULL;
    }

    if (policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    object = (sixel_lookup_policy_mono_lightbg_object_t *)
        malloc(sizeof(*object));
    if (object == NULL) {
        sixel_helper_set_additional_message(
            "sixel_lookup_policy_create_mono_lightbg: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(object, 0, sizeof(*object));
    object->base.vtbl = &g_sixel_lookup_policy_mono_lightbg_vtbl;
    object->ref = 1U;

    *policy = &object->base;
    return SIXEL_OK;
}
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic pop
#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
