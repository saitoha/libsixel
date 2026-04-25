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
#include "classid.h"
#include "classid-dither.h"

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
 * - create() resolves class ids through gperf tables for lookup/dither.
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
    unsigned int class_name_len;
    struct sixel_lookup_policy_classid_entry const *lookup_entry;
    struct sixel_dither_policy_classid_entry const *dither_entry;

    (void)factory;

    status = SIXEL_FALSE;
    class_name_len = 0;
    lookup_entry = NULL;
    dither_entry = NULL;
    if (object != NULL) {
        *object = NULL;
    }

    if (class_name == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (strncmp(class_name, "lookup/", 7) == 0) {
        class_name_len = (unsigned int)strlen(class_name);
        lookup_entry = sixel_lookup_policy_classid_lookup(
            class_name,
            class_name_len);
        if (lookup_entry == NULL) {
            sixel_helper_set_additional_message(
                "sixel_factory_create: unknown lookup class.");
            return SIXEL_BAD_ARGUMENT;
        }
        status = lookup_entry->create(
            (sixel_lookup_policy_interface_t **)object);
        return status;
    }

    if (strncmp(class_name, "dither/", 7) == 0) {
        class_name_len = (unsigned int)strlen(class_name);
        dither_entry = sixel_dither_policy_classid_lookup(
            class_name,
            class_name_len);
        if (dither_entry == NULL) {
            sixel_helper_set_additional_message(
                "sixel_factory_create: unknown dither class.");
            return SIXEL_BAD_ARGUMENT;
        }
        status = dither_entry->create(
            (sixel_dither_policy_interface_t **)object);
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
    (*factory)->vtbl->ref(*factory);
    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
