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
#include "classid-factory.h"
#include "loader-manager.h"

/*
 * IDL (internal contract)
 *
 * interface IFactory {
 *   ref();
 *   unref();
 *   create(class_name, allocator, out object);
 * }
 *
 * Ownership/lifetime:
 * - This implementation is a process-lifetime singleton.
 * - ref/unref stay no-op to preserve a uniform COM-like service surface.
 * - create() resolves lookup/dither class ids, loader manager, and loader
 *   component names.
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
                             sixel_allocator_t *allocator,
                             void **object)
{
    SIXELSTATUS status;
    unsigned int class_name_len;
    struct sixel_factory_classid_entry const *entry;
    size_t loader_name_len;
    char classid[128];

    (void)factory;

    status = SIXEL_FALSE;
    class_name_len = 0;
    entry = NULL;
    loader_name_len = 0u;
    classid[0] = '\0';
    if (object != NULL) {
        *object = NULL;
    }

    if (allocator == NULL || class_name == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (strcmp(class_name, "loader/manager") == 0) {
        status = sixel_loader_manager_new(
            allocator,
            (sixel_loader_manager_t **)object);
        return status;
    }

    class_name_len = (unsigned int)strlen(class_name);
    entry = sixel_factory_classid_lookup(class_name, class_name_len);
    if (entry == NULL && strchr(class_name, '/') == NULL) {
        loader_name_len = strlen(class_name);
        if (loader_name_len + 8u > sizeof(classid)) {
            sixel_helper_set_additional_message(
                "sixel_factory_create: loader class id is too long.");
            return SIXEL_BAD_ARGUMENT;
        }
        memcpy(classid, "loader/", 7u);
        memcpy(classid + 7u, class_name, loader_name_len + 1u);
        class_name_len = (unsigned int)(loader_name_len + 7u);
        entry = sixel_factory_classid_lookup(classid, class_name_len);
    }

    if (entry == NULL) {
        sixel_helper_set_additional_message(
            "sixel_factory_create: unknown class name.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (entry->create == NULL) {
        sixel_helper_set_additional_message(
            "sixel_factory_create: class is not available.");
        return SIXEL_BAD_ARGUMENT;
    }

    status = entry->create(allocator, object);
    return status;
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
