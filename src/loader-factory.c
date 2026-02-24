/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#if HAVE_STRING_H
# include <string.h>
#endif

#include "loader-factory.h"

#include "loader-component-legacy.h"
#include "sixel_atomic.h"

struct sixel_loader_factory {
    sixel_atomic_u32_t ref;
    sixel_loader_registry_t *registry;
};

static struct sixel_loader_factory g_loader_factory_singleton = {
    0u,
    NULL
};


static sixel_loader_entry_t const *
loader_factory_find_entry_by_name(sixel_loader_factory_t const *factory,
                                  char const *name)
{
    sixel_loader_entry_t const *entries;
    size_t entry_count;
    size_t index;

    entries = NULL;
    entry_count = 0u;
    index = 0u;
    if (factory == NULL || factory->registry == NULL || name == NULL) {
        return NULL;
    }

    entry_count = loader_registry_get_entries_from(factory->registry, &entries);
    for (index = 0u; index < entry_count; ++index) {
        if (entries[index].name != NULL && strcmp(entries[index].name, name) == 0) {
            return &entries[index];
        }
    }

    return NULL;
}

static int
loader_factory_magic_matches_chunk(sixel_loader_entry_t const *entry,
                                   sixel_chunk_t const *chunk)
{
    size_t index;
    char const *magic;
    size_t magic_length;

    index = 0u;
    magic = NULL;
    magic_length = 0u;
    if (entry == NULL) {
        return 0;
    }

    if (chunk == NULL || entry->magic_signature_count == 0u ||
        entry->magic_signatures == NULL) {
        return 1;
    }

    for (index = 0u; index < entry->magic_signature_count; ++index) {
        magic = entry->magic_signatures[index];
        if (magic == NULL) {
            continue;
        }
        magic_length = strlen(magic);
        if (magic_length == 0u || chunk->size < magic_length) {
            continue;
        }
        if (memcmp(chunk->buffer, magic, magic_length) == 0) {
            return 1;
        }
    }

    return 0;
}

static void
loader_factory_unref_singleton_ref(sixel_atomic_u32_t *ref)
{
    unsigned int previous;

    previous = 0u;
    if (ref == NULL) {
        return;
    }

    /*
     * Singleton loader objects are process-lifetime in the current design.
     * Treat unref() as a borrow release and saturate at zero to avoid
     * underflow when callers over-release references.
     */
    previous = sixel_atomic_fetch_sub_u32(ref, 1u);
    if (previous == 0u) {
        (void)sixel_atomic_fetch_add_u32(ref, 1u);
    }
}

SIXELSTATUS
loader_factory_get_default(sixel_loader_factory_t **ppfactory)
{
    SIXELSTATUS status;

    status = SIXEL_OK;

    if (ppfactory == NULL) {
        sixel_helper_set_additional_message(
            "loader_factory_get_default: ppfactory is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    /*
     * Lazily bind the singleton factory to the singleton registry.
     *
     * The registry object itself is process-lifetime in the current
     * architecture, so the factory only acquires one shared reference and
     * never releases it during normal operation.
     */
    if (g_loader_factory_singleton.registry == NULL) {
        status = loader_registry_get_default(
            &g_loader_factory_singleton.registry);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    loader_factory_ref(&g_loader_factory_singleton);
    *ppfactory = &g_loader_factory_singleton;

end:
    return status;
}

void
loader_factory_ref(sixel_loader_factory_t *factory)
{
    if (factory == NULL) {
        return;
    }

    (void)sixel_atomic_fetch_add_u32(&factory->ref, 1u);
}

void
loader_factory_unref(sixel_loader_factory_t *factory)
{
    if (factory == NULL) {
        return;
    }

    loader_factory_unref_singleton_ref(&factory->ref);
}

size_t
loader_factory_get_entries(sixel_loader_factory_t const *factory,
                           sixel_loader_entry_t const **entries)
{
    if (factory == NULL) {
        return 0u;
    }

    return loader_registry_get_entries_from(factory->registry, entries);
}

int
loader_factory_entry_available(sixel_loader_factory_t const *factory,
                               char const *name)
{
    if (factory == NULL) {
        return 0;
    }

    return loader_registry_entry_available_from(factory->registry, name);
}

int
loader_factory_entry_matches_chunk(
    sixel_loader_factory_t const *factory,
    sixel_loader_entry_t const *entry,
    sixel_chunk_t const *chunk)
{
    if (factory == NULL || factory->registry == NULL || entry == NULL) {
        return 0;
    }

    return loader_factory_magic_matches_chunk(entry, chunk);
}

SIXELSTATUS
loader_factory_create_component(sixel_loader_factory_t const *factory,
                                char const *name,
                                sixel_allocator_t *allocator,
                                sixel_loader_component_t **ppcomponent)
{
    sixel_loader_entry_t const *entry;

    entry = NULL;
    if (ppcomponent != NULL) {
        *ppcomponent = NULL;
    }

    if (factory == NULL || name == NULL || ppcomponent == NULL) {
        sixel_helper_set_additional_message(
            "loader_factory_create_component: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (factory->registry == NULL) {
        sixel_helper_set_additional_message(
            "loader_factory_create_component: factory is not initialized.");
        return SIXEL_BAD_ARGUMENT;
    }

    entry = loader_factory_find_entry_by_name(factory, name);
    if (entry == NULL) {
        sixel_helper_set_additional_message(
            "loader_factory_create_component: loader is not registered.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (entry->new_component != NULL) {
        return entry->new_component(allocator, ppcomponent);
    }

    *ppcomponent = sixel_loader_component_legacy_new(entry, allocator);
    if (*ppcomponent == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

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
