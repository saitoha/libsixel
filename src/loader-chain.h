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

#ifndef LIBSIXEL_LOADER_CHAIN_H
#define LIBSIXEL_LOADER_CHAIN_H

#include <sixel.h>

#include "allocator.h"
#include "loader-component.h"

typedef struct sixel_loader_chain_node {
    struct sixel_loader_chain_node *next;
    sixel_loader_component_t *component;
} sixel_loader_chain_node_t;

typedef struct sixel_loader_chain sixel_loader_chain_t;

SIXELSTATUS
loader_chain_new(sixel_loader_chain_t **ppchain,
                 sixel_allocator_t *allocator);

void
loader_chain_ref(sixel_loader_chain_t *chain);

void
loader_chain_unref(sixel_loader_chain_t *chain);

SIXELSTATUS
loader_chain_append(sixel_loader_chain_t *chain,
                    sixel_loader_component_t *component);

sixel_loader_chain_node_t const *
loader_chain_head(sixel_loader_chain_t const *chain);

size_t
loader_chain_length(sixel_loader_chain_t const *chain);

#endif /* LIBSIXEL_LOADER_CHAIN_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
