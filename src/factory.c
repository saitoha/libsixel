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

#if HAVE_STRING_H
#include <string.h>
#endif

#include "factory.h"
#include "lookup-policy.h"

/*
 * IDL (internal contract)
 *
 * interface IFactory {
 *   ref();
 *   unref();
 *   create(class_name, out object);
 * }
 *
 * Ownership/lifetime:
 * - This implementation is a process-lifetime singleton.
 * - ref/unref stay no-op to preserve a uniform COM-like service surface.
 */

static void
sixel_factory_ref_noop(sixel_factory_t *factory)
{
    (void)factory;
}

static void
sixel_factory_unref_noop(sixel_factory_t *factory)
{
    (void)factory;
}

static SIXELSTATUS
sixel_factory_create_default(sixel_factory_t *factory,
                             char const *class_name,
                             void **object)
{
    SIXELSTATUS status;

    (void)factory;

    status = SIXEL_FALSE;
    if (object != NULL) {
        *object = NULL;
    }

    if (class_name == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (strncmp(class_name, "lookup/", 7) == 0) {
        status = sixel_lookup_policy_create_by_name(
            class_name,
            (sixel_lookup_policy_interface_t **)object);
        return status;
    }

    sixel_helper_set_additional_message(
        "sixel_factory_create: unknown class name.");
    return SIXEL_BAD_ARGUMENT;
}

static sixel_factory_vtbl_t const g_sixel_factory_vtbl = {
    sixel_factory_ref_noop,
    sixel_factory_unref_noop,
    sixel_factory_create_default
};

static sixel_factory_t g_sixel_factory_singleton = {
    &g_sixel_factory_vtbl
};

SIXELSTATUS
sixel_factory_get_default(sixel_factory_t **factory)
{
    if (factory == NULL) {
        sixel_helper_set_additional_message(
            "sixel_factory_get_default: factory is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    *factory = &g_sixel_factory_singleton;
    sixel_factory_ref(*factory);
    return SIXEL_OK;
}

void
sixel_factory_ref(sixel_factory_t *factory)
{
    if (factory == NULL || factory->vtbl == NULL
            || factory->vtbl->ref == NULL) {
        return;
    }

    factory->vtbl->ref(factory);
}

void
sixel_factory_unref(sixel_factory_t *factory)
{
    if (factory == NULL || factory->vtbl == NULL
            || factory->vtbl->unref == NULL) {
        return;
    }

    factory->vtbl->unref(factory);
}

SIXELSTATUS
sixel_factory_create(sixel_factory_t *factory,
                     char const *class_name,
                     void **object)
{
    if (factory == NULL || factory->vtbl == NULL
            || factory->vtbl->create == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return factory->vtbl->create(factory, class_name, object);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
