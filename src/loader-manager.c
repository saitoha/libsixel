/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include "loader-manager.h"

#include "sixel_atomic.h"

#include <ctype.h>
#if HAVE_STRING_H
# include <string.h>
#endif

struct sixel_loader_manager {
    sixel_atomic_u32_t ref;
    sixel_loader_factory_t *factory;
};

static struct sixel_loader_manager g_loader_manager_singleton = {
    0u,
    NULL
};

static int
loader_manager_plan_contains(sixel_loader_entry_t const **plan,
                             size_t plan_length,
                             sixel_loader_entry_t const *entry)
{
    size_t index;

    index = 0u;
    for (index = 0u; index < plan_length; ++index) {
        if (plan[index] == entry) {
            return 1;
        }
    }

    return 0;
}

static size_t
loader_manager_token_name_length(char const *token,
                                 size_t token_length)
{
    size_t length;

    length = token_length;
    if (token != NULL && token_length > 0 && token[token_length - 1] == '@') {
        length = token_length - 1u;
    }

    return length;
}

static int
loader_manager_token_matches(char const *token,
                             size_t token_length,
                             char const *name)
{
    size_t index;
    unsigned char left;
    unsigned char right;

    index = 0u;
    left = 0u;
    right = 0u;
    for (index = 0u; index < token_length && name[index] != '\0'; ++index) {
        left = (unsigned char)token[index];
        right = (unsigned char)name[index];
        if (tolower(left) != tolower(right)) {
            return 0;
        }
    }

    if (index != token_length || name[index] != '\0') {
        return 0;
    }

    return 1;
}

static sixel_loader_entry_t const *
loader_manager_lookup_token(char const *token,
                            size_t token_length,
                            sixel_loader_entry_t const *entries,
                            size_t entry_count)
{
    size_t index;

    index = 0u;
    for (index = 0u; index < entry_count; ++index) {
        if (loader_manager_token_matches(token,
                                         token_length,
                                         entries[index].name)) {
            return &entries[index];
        }
    }

    return NULL;
}

size_t
loader_manager_build_plan(
    char const *order,
    sixel_loader_entry_t const *entries,
    size_t entry_count,
    sixel_loader_entry_t const **plan,
    size_t plan_capacity)
{
    size_t plan_length;
    size_t index;
    char const *cursor;
    char const *token_start;
    char const *token_end;
    char const *order_end;
    size_t token_length;
    sixel_loader_entry_t const *entry;
    size_t limit;
    int allow_fallback;

    plan_length = 0u;
    index = 0u;
    cursor = order;
    token_start = order;
    token_end = order;
    order_end = NULL;
    token_length = 0u;
    entry = NULL;
    limit = plan_capacity;
    allow_fallback = 1;

    if (order != NULL) {
        order_end = order + strlen(order);
        while (order_end > order &&
               isspace((unsigned char)order_end[-1])) {
            --order_end;
        }
        if (order_end > order && order_end[-1] == '!') {
            allow_fallback = 0;
            --order_end;
            while (order_end > order &&
                   isspace((unsigned char)order_end[-1])) {
                --order_end;
            }
        }
    }

    if (order != NULL && plan != NULL && plan_capacity > 0u) {
        token_start = order;
        cursor = order;
        while (cursor < order_end) {
            if (*cursor == ',') {
                token_end = cursor;
                while (token_start < token_end &&
                       isspace((unsigned char)*token_start)) {
                    ++token_start;
                }
                while (token_end > token_start &&
                       isspace((unsigned char)token_end[-1])) {
                    --token_end;
                }
                token_length = (size_t)(token_end - token_start);
                if (token_length > 0u) {
                    entry = loader_manager_lookup_token(
                        token_start,
                        loader_manager_token_name_length(token_start,
                                                         token_length),
                        entries,
                        entry_count);
                    if (entry != NULL &&
                        !loader_manager_plan_contains(plan,
                                                     plan_length,
                                                     entry) &&
                        plan_length < limit) {
                        plan[plan_length] = entry;
                        ++plan_length;
                    }
                }
                token_start = cursor + 1;
            }
            ++cursor;
        }

        token_end = order_end;
        while (token_start < token_end &&
               isspace((unsigned char)*token_start)) {
            ++token_start;
        }
        while (token_end > token_start &&
               isspace((unsigned char)token_end[-1])) {
            --token_end;
        }
        token_length = (size_t)(token_end - token_start);
        if (token_length > 0u) {
            entry = loader_manager_lookup_token(
                token_start,
                loader_manager_token_name_length(token_start, token_length),
                entries,
                entry_count);
            if (entry != NULL &&
                !loader_manager_plan_contains(plan, plan_length, entry) &&
                plan_length < limit) {
                plan[plan_length] = entry;
                ++plan_length;
            }
        }
    }

    if (allow_fallback) {
        for (index = 0u; index < entry_count && plan_length < limit; ++index) {
            entry = &entries[index];
            if (!entry->default_enabled) {
                continue;
            }
            if (!loader_manager_plan_contains(plan, plan_length, entry)) {
                plan[plan_length] = entry;
                ++plan_length;
            }
        }
    }

    return plan_length;
}

SIXELSTATUS
loader_manager_get_default(sixel_loader_manager_t **ppmanager)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (ppmanager == NULL) {
        sixel_helper_set_additional_message(
            "loader_manager_get_default: ppmanager is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (g_loader_manager_singleton.factory == NULL) {
        status = loader_factory_get_default(
            &g_loader_manager_singleton.factory);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    loader_manager_ref(&g_loader_manager_singleton);
    *ppmanager = &g_loader_manager_singleton;

    return status;
}

void
loader_manager_ref(sixel_loader_manager_t *manager)
{
    if (manager == NULL) {
        return;
    }

    (void)sixel_atomic_fetch_add_u32(&manager->ref, 1u);
}

void
loader_manager_unref(sixel_loader_manager_t *manager)
{
    unsigned int previous;

    previous = 0u;
    if (manager == NULL) {
        return;
    }

    previous = sixel_atomic_fetch_sub_u32(&manager->ref, 1u);
    if (previous == 0u) {
        (void)sixel_atomic_fetch_add_u32(&manager->ref, 1u);
    }
}

SIXELSTATUS
loader_manager_build_chain_from_plan(
    sixel_loader_manager_t *manager,
    sixel_loader_entry_t const **plan,
    size_t plan_length,
    sixel_chunk_t const *chunk,
    sixel_allocator_t *allocator,
    sixel_loader_chain_t **ppchain)
{
    SIXELSTATUS status;
    sixel_loader_chain_t *chain;
    sixel_loader_component_t *component;
    size_t index;

    status = SIXEL_OK;
    chain = NULL;
    component = NULL;
    index = 0u;
    if (ppchain == NULL || manager == NULL || plan == NULL) {
        sixel_helper_set_additional_message(
            "loader_manager_build_chain_from_plan: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    *ppchain = NULL;
    status = loader_chain_new(&chain, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    for (index = 0u; index < plan_length; ++index) {
        if (plan[index] == NULL) {
            continue;
        }
        if (plan[index]->predicate != NULL &&
            chunk != NULL &&
            plan[index]->predicate(chunk) == 0) {
            continue;
        }

        status = loader_factory_create_component(manager->factory,
                                                 plan[index],
                                                 allocator,
                                                 &component);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        status = loader_chain_append(chain, component);
        sixel_loader_component_unref(component);
        component = NULL;
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    *ppchain = chain;
    chain = NULL;

end:
    if (component != NULL) {
        sixel_loader_component_unref(component);
    }
    loader_chain_unref(chain);
    return status;
}

SIXELSTATUS
loader_manager_execute_chain(
    sixel_loader_manager_t *manager,
    sixel_loader_chain_t const *chain,
    sixel_chunk_t const *chunk,
    sixel_load_image_function fn_load,
    void *load_context,
    sixel_loader_manager_configure_component_fn fn_configure,
    void *configure_context,
    sixel_loader_manager_trace_try_fn fn_try,
    sixel_loader_manager_trace_result_fn fn_result,
    void *trace_context,
    char const **selected_name)
{
    SIXELSTATUS status;
    sixel_loader_chain_node_t const *node;
    char const *name;

    status = SIXEL_FALSE;
    node = NULL;
    name = NULL;
    if (selected_name != NULL) {
        *selected_name = NULL;
    }
    if (manager == NULL || chain == NULL || chunk == NULL) {
        sixel_helper_set_additional_message(
            "loader_manager_execute_chain: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    (void)manager;
    node = loader_chain_head(chain);
    while (node != NULL) {
        name = sixel_loader_component_get_name(node->component);
        if (fn_configure != NULL) {
            status = fn_configure(node->component, configure_context);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }

        if (fn_try != NULL) {
            fn_try(name, trace_context);
        }

        status = sixel_loader_component_load(node->component,
                                             chunk,
                                             fn_load,
                                             load_context);

        if (fn_result != NULL) {
            fn_result(name, status, trace_context);
        }

        if (SIXEL_SUCCEEDED(status)) {
            if (selected_name != NULL) {
                *selected_name = name;
            }
            return status;
        }
        node = node->next;
    }

    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
