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
 *
 * Loader factory object that resolves registry entries and instantiates
 * component objects.
 */

#ifndef LIBSIXEL_LOADER_FACTORY_H
#define LIBSIXEL_LOADER_FACTORY_H

#include <sixel.h>

#include "allocator.h"
#include "chunk.h"
#include "loader-component.h"
#include "loader-registry.h"

typedef struct sixel_loader_factory sixel_loader_factory_t;

/*
 * Factory API boundary
 *
 * - get_entries()/entry_available(): expose registry-backed entry metadata.
 * - entry_matches_chunk(): decide runtime eligibility for an input chunk.
 * - create_component(): materialize a component for eligible entries.
 */

SIXELSTATUS
loader_factory_get_default(sixel_loader_factory_t **ppfactory);

void
loader_factory_ref(sixel_loader_factory_t *factory);

void
loader_factory_unref(sixel_loader_factory_t *factory);

size_t
loader_factory_get_entries(
    sixel_loader_factory_t const *factory,
    sixel_loader_entry_t const **entries);

int
loader_factory_entry_available(
    sixel_loader_factory_t const *factory,
    char const *name);

int
loader_factory_entry_matches_chunk(
    sixel_loader_factory_t const *factory,
    sixel_loader_entry_t const *entry,
    sixel_chunk_t const *chunk);

SIXELSTATUS
loader_factory_create_component(
    sixel_loader_factory_t const *factory,
    char const *name,
    sixel_allocator_t *allocator,
    sixel_loader_component_t **ppcomponent);

#endif /* LIBSIXEL_LOADER_FACTORY_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
