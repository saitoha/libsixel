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

#include "loader-common.h"
#include "options.h"
#include "sixel_atomic.h"

#include <ctype.h>
#include <limits.h>
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


static sixel_suboption_choice_t const
g_suboption_choices_loader_enable_cms_loader[] = {
    { "0", 0 },
    { "1", 1 }
};

static sixel_suboption_key_t const g_subkeys_loader_enable_cms_loader[] = {
    {
        "enable_cms",
        "e",
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_suboption_choices_loader_enable_cms_loader,
        sizeof(g_suboption_choices_loader_enable_cms_loader)
            / sizeof(g_suboption_choices_loader_enable_cms_loader[0])
    }
};

#if HAVE_WIC
/*
 * The loader option parser accepts free-form text for ico_minsize and
 * validates it later via loader_manager_parse_positive_int().
 */
static sixel_suboption_key_t const g_subkeys_loader_wic_loader[] = {
    {
        "ico_minsize",
        NULL,
        NULL,
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    }
};
#endif

static sixel_option_value_schema_t const
g_schema_loader_values_loader[] = {
#if HAVE_LIBPNG
    {
        "libpng",
        0,
        g_subkeys_loader_enable_cms_loader,
        sizeof(g_subkeys_loader_enable_cms_loader)
            / sizeof(g_subkeys_loader_enable_cms_loader[0])
    },
#endif
    {
        "builtin",
        1,
        g_subkeys_loader_enable_cms_loader,
        sizeof(g_subkeys_loader_enable_cms_loader)
            / sizeof(g_subkeys_loader_enable_cms_loader[0])
    },
#if HAVE_WIC
    {
        "wic",
        2,
        g_subkeys_loader_wic_loader,
        sizeof(g_subkeys_loader_wic_loader)
            / sizeof(g_subkeys_loader_wic_loader[0])
    },
#endif
};

static sixel_option_argument_schema_t const g_schema_loaders_loader = {
    SIXEL_OPTFLAG_LOADERS,
    "--loaders",
    g_schema_loader_values_loader,
    sizeof(g_schema_loader_values_loader)
        / sizeof(g_schema_loader_values_loader[0])
};

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
    size_t index;
    size_t length;

    index = 0u;
    length = token_length;

    /*
     * Loader tokens can carry suboptions after ':' (for example
     * "wic:ico_minsize=30"). Build-plan lookup must match only the loader
     * name so that suboptions do not prevent backend resolution.
     */
    while (index < length) {
        if (token[index] == ':') {
            length = index;
            break;
        }
        ++index;
    }

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


void
loader_manager_apply_loader_suboptions(char const *order)
{
    char const *cursor;
    char const *token_start;
    char const *token_end;
    char const *order_end;
    size_t token_length;
    char token_buffer[256];
    char match_detail[128];
    sixel_option_argument_resolution_t resolution;
    size_t assignment_index;
    char const *key_name;
    char const *value_text;
    size_t value_length;
    int parsed_value;

    cursor = order;
    token_start = order;
    token_end = order;
    order_end = NULL;
    token_length = 0u;
    token_buffer[0] = '\0';
    match_detail[0] = '\0';
    resolution.resolved_base_value = 0;
    resolution.base_def = NULL;
    resolution.assignments = NULL;
    resolution.assignment_count = 0u;
    assignment_index = 0u;
    key_name = NULL;
    value_text = NULL;
    value_length = 0u;
    parsed_value = 0;

    /* Reset overrides first so malformed tokens cannot leak previous state. */
#if HAVE_WIC
    sixel_helper_set_wic_ico_minsize(0);
#endif
    sixel_helper_set_libpng_enable_cms(-1);
    sixel_helper_set_builtin_enable_cms(-1);

    if (order == NULL || order[0] == '\0') {
        return;
    }

    order_end = order + strlen(order);
    while (order_end > order && isspace((unsigned char)order_end[-1])) {
        --order_end;
    }
    if (order_end > order && order_end[-1] == '!') {
        --order_end;
    }

    token_start = order;
    cursor = order;
    while (cursor <= order_end) {
        if (cursor == order_end || *cursor == ',') {
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
            if (token_length > 0u && token_length < sizeof(token_buffer)) {
                memcpy(token_buffer, token_start, token_length);
                token_buffer[token_length] = '\0';
                if (SIXEL_SUCCEEDED(
                        sixel_option_parse_argument_with_suboptions(
                            token_buffer,
                            &g_schema_loaders_loader,
                            &resolution,
                            match_detail,
                            sizeof(match_detail))) &&
                    resolution.base_def != NULL) {
                    for (assignment_index = 0u;
                         assignment_index < resolution.assignment_count;
                         ++assignment_index) {
                        key_name = resolution.assignments[assignment_index]
                            .resolved_key_name;
                        value_text = resolution.assignments[assignment_index]
                            .resolved_value_text;
                        value_length = 0u;
                        if (value_text != NULL) {
                            value_length = strlen(value_text);
                        }
                        if (key_name == NULL || value_text == NULL) {
                            continue;
                        }
#if HAVE_WIC
                        if (strcmp(resolution.base_def->name, "wic") == 0 &&
                            strcmp(key_name, "ico_minsize") == 0 &&
                            loader_manager_parse_positive_int(
                                value_text,
                                value_length,
                                &parsed_value)) {
                            sixel_helper_set_wic_ico_minsize(parsed_value);
                            continue;
                        }
#endif
                        if (strcmp(key_name, "enable_cms") == 0 &&
                            loader_manager_parse_bool_flag(
                                value_text,
                                value_length,
                                &parsed_value)) {
                            if (strcmp(resolution.base_def->name, "libpng")
                                == 0) {
                                sixel_helper_set_libpng_enable_cms(parsed_value);
                            } else if (strcmp(resolution.base_def->name,
                                              "builtin") == 0) {
                                sixel_helper_set_builtin_enable_cms(parsed_value);
                            }
                        }
                    }
                }
                sixel_option_free_argument_resolution(&resolution);
            }
            token_start = cursor + 1;
        }
        ++cursor;
    }
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
