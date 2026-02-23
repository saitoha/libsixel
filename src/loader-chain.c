/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
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
