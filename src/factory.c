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
#include "classid-lookup-policy.h"
#include "classid-dither-policy.h"
#include "loader-registry.h"

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
 * - create() resolves lookup/dither class ids and loader component names.
 */

static SIXELSTATUS
sixel_factory_create_loader_component(char const *loader_name,
                                      sixel_allocator_t *allocator,
                                      void **object)
{
    SIXELSTATUS status;
    sixel_loader_registry_t *registry;
    sixel_loader_entry_t const *entries;
    sixel_loader_entry_t const *entry;
    size_t entry_count;
    size_t index;

    status = SIXEL_FALSE;
    registry = NULL;
    entries = NULL;
    entry = NULL;
    entry_count = 0u;
    index = 0u;
    if (loader_name == NULL || *loader_name == '\0'
            || allocator == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = loader_registry_get_default(&registry);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    entry_count = loader_registry_get_entries_from(registry, &entries);
    for (index = 0u; index < entry_count; ++index) {
        if (entries[index].name != NULL
                && strcmp(entries[index].name, loader_name) == 0) {
            entry = &entries[index];
            break;
        }
    }

    loader_registry_unref(registry);
    registry = NULL;

    if (entry == NULL) {
        sixel_helper_set_additional_message(
            "sixel_factory_create: unknown loader class.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (entry->new_component == NULL) {
        sixel_helper_set_additional_message(
            "sixel_factory_create: loader does not provide component"
            " constructor.");
        return SIXEL_LOGIC_ERROR;
    }

    status = entry->new_component(allocator,
                                  (sixel_loader_component_t **)object);
    return status;
}

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
    char const *loader_name;
    struct sixel_lookup_policy_classid_entry const *lookup_entry;
    struct sixel_dither_policy_classid_entry const *dither_entry;

    (void)factory;

    status = SIXEL_FALSE;
    class_name_len = 0;
    loader_name = NULL;
    lookup_entry = NULL;
    dither_entry = NULL;
    if (object != NULL) {
        *object = NULL;
    }

    if (allocator == NULL || class_name == NULL || object == NULL) {
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
            allocator,
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
            allocator,
            (sixel_dither_policy_interface_t **)object);
        return status;
    }

    if (strncmp(class_name, "loader/", 7) == 0) {
        loader_name = class_name + 7;
    } else if (strchr(class_name, '/') == NULL) {
        loader_name = class_name;
    }
    if (loader_name != NULL) {
        status = sixel_factory_create_loader_component(loader_name,
                                                       allocator,
                                                       object);
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
