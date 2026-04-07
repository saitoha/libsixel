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
# include "config.h"
#endif

#include "loader-chain.h"

#include "sixel_atomic.h"

struct sixel_loader_chain {
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_loader_chain_node_t *head;
    sixel_loader_chain_node_t *tail;
    size_t length;
};

SIXELSTATUS
loader_chain_new(sixel_loader_chain_t **ppchain,
                 sixel_allocator_t *allocator)
{
    sixel_loader_chain_t *chain;

    chain = NULL;
    if (ppchain == NULL) {
        sixel_helper_set_additional_message(
            "loader_chain_new: ppchain is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    *ppchain = NULL;
    chain = sixel_allocator_malloc(allocator, sizeof(*chain));
    if (chain == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    chain->ref = 1u;
    chain->allocator = allocator;
    chain->head = NULL;
    chain->tail = NULL;
    chain->length = 0u;

    *ppchain = chain;
    return SIXEL_OK;
}

void
loader_chain_ref(sixel_loader_chain_t *chain)
{
    if (chain == NULL) {
        return;
    }
    (void)sixel_atomic_fetch_add_u32(&chain->ref, 1u);
}

void
loader_chain_unref(sixel_loader_chain_t *chain)
{
    unsigned int previous;
    sixel_loader_chain_node_t *node;
    sixel_loader_chain_node_t *next;

    previous = 0u;
    node = NULL;
    next = NULL;
    if (chain == NULL) {
        return;
    }

    previous = sixel_atomic_fetch_sub_u32(&chain->ref, 1u);
    if (previous != 1u) {
        if (previous == 0u) {
            (void)sixel_atomic_fetch_add_u32(&chain->ref, 1u);
        }
        /*
         * Unlike singleton objects, chain instances own heap nodes and are
         * destructed only at the exact refcount transition 1 -> 0.
         */
        return;
    }

    node = chain->head;
    while (node != NULL) {
        next = node->next;
        sixel_loader_component_unref(node->component);
        sixel_allocator_free(chain->allocator, node);
        node = next;
    }

    sixel_allocator_free(chain->allocator, chain);
}

SIXELSTATUS
loader_chain_append(sixel_loader_chain_t *chain,
                    sixel_loader_component_t *component)
{
    sixel_loader_chain_node_t *node;

    node = NULL;
    if (chain == NULL || component == NULL) {
        sixel_helper_set_additional_message(
            "loader_chain_append: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    node = sixel_allocator_malloc(chain->allocator, sizeof(*node));
    if (node == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    node->next = NULL;
    node->component = component;
    sixel_loader_component_ref(component);

    if (chain->tail == NULL) {
        chain->head = node;
        chain->tail = node;
    } else {
        chain->tail->next = node;
        chain->tail = node;
    }

    ++chain->length;
    return SIXEL_OK;
}

sixel_loader_chain_node_t const *
loader_chain_head(sixel_loader_chain_t const *chain)
{
    if (chain == NULL) {
        return NULL;
    }
    return chain->head;
}

size_t
loader_chain_length(sixel_loader_chain_t const *chain)
{
    if (chain == NULL) {
        return 0u;
    }
    return chain->length;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
