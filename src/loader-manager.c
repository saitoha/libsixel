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

#include "compat_stub.h"
#include "loader-common.h"
#include "loader-order-schema.h"
#include "options.h"
#include "sixel_atomic.h"

#include <ctype.h>
#include <limits.h>
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
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

static void
loader_manager_unref_singleton_ref(sixel_atomic_u32_t *ref)
{
    unsigned int previous;

    previous = 0u;
    if (ref == NULL) {
        return;
    }

    /*
     * Manager is a singleton coordinator. Saturate at zero so accidental
     * extra unref() calls never wrap the reference counter.
     */
    previous = sixel_atomic_fetch_sub_u32(ref, 1u);
    if (previous == 0u) {
        (void)sixel_atomic_fetch_add_u32(ref, 1u);
    }
}


#if HAVE_WIC
static int
loader_manager_parse_positive_int(char const *text,
                                  size_t length,
                                  int *value_out)
{
    size_t index;
    int value;
    unsigned char digit;

    index = 0u;
    value = 0;
    digit = 0u;
    if (text == NULL || value_out == NULL || length == 0u) {
        return 0;
    }

    for (index = 0u; index < length; ++index) {
        digit = (unsigned char)text[index];
        if (digit < (unsigned char)'0' || digit > (unsigned char)'9') {
            return 0;
        }
        if (value > (INT_MAX - 9) / 10) {
            return 0;
        }
        value = value * 10 + (digit - (unsigned char)'0');
    }

    if (value <= 0) {
        return 0;
    }

    *value_out = value;
    return 1;
}
#endif

static int
loader_manager_parse_bool_flag(char const *text,
                               size_t length,
                               int *value_out)
{
    if (text == NULL || value_out == NULL || length != 1u) {
        return 0;
    }
    if (text[0] == '0') {
        *value_out = 0;
        return 1;
    }
    if (text[0] == '1') {
        *value_out = 1;
        return 1;
    }
    return 0;
}

#if HAVE_WIC
static int
loader_manager_read_env_positive_int(char const *name,
                                     int fallback_value)
{
    char const *env_value;
    char *endptr;
    long parsed;

    env_value = NULL;
    endptr = NULL;
    parsed = 0;
    if (name == NULL) {
        return fallback_value;
    }

    env_value = sixel_compat_getenv(name);
    if (env_value == NULL || env_value[0] == '\0') {
        return fallback_value;
    }

    errno = 0;
    parsed = strtol(env_value, &endptr, 10);
    if (errno != 0 || endptr == env_value || endptr == NULL ||
        endptr[0] != '\0' || parsed <= 0) {
        return fallback_value;
    }
    if (parsed > (long)INT_MAX) {
        parsed = (long)INT_MAX;
    }

    return (int)parsed;
}

static int
loader_manager_read_wic_ico_minsize_from_env(int fallback_value)
{
    char const *primary_name;
    char const *legacy_name;
    char const *primary_value;

    primary_name = "SIXEL_LOADER_WIC_ICO_MINSIZE";
    legacy_name = "SIXEL_LODER_WIC_ICO_MINSIZE";
    primary_value = sixel_compat_getenv(primary_name);

    if (primary_value != NULL && primary_value[0] != '\0') {
        return loader_manager_read_env_positive_int(primary_name,
                                                    fallback_value);
    }

    return loader_manager_read_env_positive_int(legacy_name, fallback_value);
}
#endif

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

SIXELSTATUS
loader_manager_parse_loader_order(
    char const *order,
    sixel_option_argument_list_resolution_t *resolution)
{
    char diagnostic[128];

    diagnostic[0] = '\0';
    if (resolution == NULL) {
        sixel_helper_set_additional_message(
            "loader_manager_parse_loader_order: resolution is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    return sixel_loader_order_parse_and_validate(
        order,
        resolution,
        diagnostic,
        sizeof(diagnostic));
}

void
loader_manager_init_loader_suboptions(
    sixel_loader_suboptions_t *suboptions)
{
    if (suboptions == NULL) {
        return;
    }

    suboptions->libpng_enable_cms = 1;
    suboptions->builtin_enable_cms = 1;
#if HAVE_WIC
    suboptions->wic_ico_minsize = loader_manager_read_wic_ico_minsize_from_env(
        0);
#else
    suboptions->wic_ico_minsize = 0;
#endif
}

void
loader_manager_resolve_loader_suboptions(
    sixel_option_argument_list_resolution_t const *resolution,
    sixel_loader_suboptions_t *suboptions)
{
    size_t item_index;
    size_t assignment_index;
    sixel_option_argument_resolution_t const *item;
    char const *key_name;
    char const *value_text;
    size_t value_length;
    int parsed_value;

    item_index = 0u;
    assignment_index = 0u;
    item = NULL;
    key_name = NULL;
    value_text = NULL;
    value_length = 0u;
    parsed_value = 0;

    if (suboptions == NULL) {
        return;
    }

    loader_manager_init_loader_suboptions(suboptions);

    if (resolution == NULL) {
        return;
    }

    while (item_index < resolution->item_count) {
        item = &resolution->items[item_index].resolution;
        if (item->base_def == NULL) {
            ++item_index;
            continue;
        }
        assignment_index = 0u;
        while (assignment_index < item->assignment_count) {
            key_name = item->assignments[assignment_index].resolved_key_name;
            value_text = item->assignments[assignment_index].resolved_value_text;
            value_length = 0u;
            if (value_text != NULL) {
                value_length = strlen(value_text);
            }
            if (key_name == NULL || value_text == NULL) {
                ++assignment_index;
                continue;
            }
#if HAVE_WIC
            if (strcmp(item->base_def->name, "wic") == 0 &&
                strcmp(key_name, "ico_minsize") == 0 &&
                loader_manager_parse_positive_int(value_text,
                                                  value_length,
                                                  &parsed_value)) {
                suboptions->wic_ico_minsize = parsed_value;
                ++assignment_index;
                continue;
            }
#endif
            if (strcmp(item->base_def->name, "libpng") == 0 &&
                strcmp(key_name, "cms") == 0 &&
                loader_manager_parse_bool_flag(value_text,
                                               value_length,
                                               &parsed_value)) {
                suboptions->libpng_enable_cms = parsed_value;
            } else if (strcmp(item->base_def->name, "builtin") == 0 &&
                       strcmp(key_name, "enable_cms") == 0 &&
                       loader_manager_parse_bool_flag(value_text,
                                                      value_length,
                                                      &parsed_value)) {
                suboptions->builtin_enable_cms = parsed_value;
            }
            ++assignment_index;
        }
        ++item_index;
    }
}

size_t
loader_manager_build_plan_from_resolution(
    sixel_option_argument_list_resolution_t const *resolution,
    sixel_loader_entry_t const *entries,
    size_t entry_count,
    sixel_loader_entry_t const **plan,
    size_t plan_capacity)
{
    size_t plan_length;
    size_t index;
    sixel_loader_entry_t const *entry;
    size_t limit;
    int allow_fallback;
    sixel_option_argument_resolution_t const *item;

    plan_length = 0u;
    index = 0u;
    entry = NULL;
    limit = plan_capacity;
    allow_fallback = 1;
    item = NULL;

    if (resolution != NULL) {
        allow_fallback = !resolution->has_trailing_bang;
    }

    if (plan != NULL && plan_capacity > 0u && resolution != NULL) {
        for (index = 0u; index < resolution->item_count; ++index) {
            item = &resolution->items[index].resolution;
            if (item->base_def == NULL || item->base_def->name == NULL) {
                continue;
            }
            entry = loader_manager_lookup_token(item->base_def->name,
                                                strlen(item->base_def->name),
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

    if (allow_fallback && plan != NULL && limit > 0u) {
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
    if (manager == NULL) {
        return;
    }

    loader_manager_unref_singleton_ref(&manager->ref);
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

    /*
     * Chain assembly keeps a strict separation of concerns.
     *
     * 1) plan[] already defines candidate order.
     * 2) factory decides whether a candidate can run for this chunk.
     * 3) factory materializes a backend component.
     * 4) chain owns component references in execution order.
     */
    for (index = 0u; index < plan_length; ++index) {
        if (plan[index] == NULL) {
            continue;
        }
        if (!loader_factory_entry_matches_chunk(manager->factory,
                                                plan[index],
                                                chunk)) {
            continue;
        }

        status = loader_factory_create_component(manager->factory,
                                                 plan[index]->name,
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
