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

#include "loader-manager.h"

#include "allocator.h"
#include "cms.h"
#include "compat_stub.h"
#include "factory.h"
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
    sixel_loader_manager_t base;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_loader_component_interface_t **chain;
    size_t chain_count;
    size_t chain_capacity;
    int skip_predicate_gate;
    sixel_timeline_logger_t *timeline_logger;
    int *timeline_job_seq;
    sixel_loader_t *timeline_loader;
};

static sixel_loader_entry_t const g_sixel_loader_entries[] = {
#if HAVE_LIBPNG
    { "libpng", "loader/libpng", 1 },
#endif
#if HAVE_JPEG
    { "libjpeg", "loader/libjpeg", 1 },
#endif
#if HAVE_WEBP
    { "libwebp", "loader/libwebp", 1 },
#endif
#if HAVE_LIBTIFF
    { "libtiff", "loader/libtiff", 1 },
#endif
#if HAVE_LIBRSVG
    { "librsvg", "loader/librsvg", 1 },
#endif
    { "builtin", "loader/builtin", 1 },
#if HAVE_WIC
    { "wic", "loader/wic", 1 },
#endif
#if HAVE_COREGRAPHICS
    { "coregraphics", "loader/coregraphics", 1 },
#endif
#ifdef HAVE_GDK_PIXBUF2
    { "gdk-pixbuf2", "loader/gdk-pixbuf2", 1 },
#endif
#if HAVE_GD
    { "gd", "loader/gd", 1 },
#endif
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
    { "quicklook", "loader/quicklook", 1 },
#endif
#if HAVE_FREEDESKTOP_THUMBNAILING
    { "gnome-thumbnailer", "loader/gnome-thumbnailer", 0 },
#endif
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
loader_manager_parse_cms_engine(char const *text,
                                size_t length,
                                int *value_out)
{
    sixel_cms_engine_t engine;

    if (text == NULL || value_out == NULL || length == 0u) {
        return 0;
    }
    if (strlen(text) != length) {
        return 0;
    }
    if (!sixel_cms_engine_from_string(text, &engine)) {
        return 0;
    }

    *value_out = (int)engine;
    return 1;
}

static int
loader_manager_read_env_cms_engine(char const *name,
                                   int fallback_value)
{
    char const *env_value;
    int parsed_value;

    env_value = NULL;
    parsed_value = fallback_value;
    if (name == NULL) {
        return fallback_value;
    }

    env_value = sixel_compat_getenv(name);
    if (env_value == NULL || env_value[0] == '\0') {
        return fallback_value;
    }

    if (!loader_manager_parse_cms_engine(env_value,
                                         strlen(env_value),
                                         &parsed_value)) {
        return fallback_value;
    }

    return parsed_value;
}

static int
loader_manager_span_equals_nocase(char const *text,
                                  size_t length,
                                  char const *token)
{
    size_t index;
    unsigned char left;
    unsigned char right;

    index = 0u;
    left = 0u;
    right = 0u;
    if (text == NULL || token == NULL) {
        return 0;
    }

    while (index < length && token[index] != '\0') {
        left = (unsigned char)text[index];
        right = (unsigned char)token[index];
        if (tolower(left) != tolower(right)) {
            return 0;
        }
        ++index;
    }
    if (index != length || token[index] != '\0') {
        return 0;
    }

    return 1;
}

static int
loader_manager_parse_orientation(char const *text,
                                 size_t length,
                                 int *value_out,
                                 int allow_numeric)
{
    if (text == NULL || value_out == NULL || length == 0u) {
        return 0;
    }
    if (loader_manager_span_equals_nocase(text, length, "on")) {
        *value_out = 1;
        return 1;
    }
    if (loader_manager_span_equals_nocase(text, length, "off")) {
        *value_out = 0;
        return 1;
    }
    if (allow_numeric &&
        loader_manager_span_equals_nocase(text, length, "1")) {
        *value_out = 1;
        return 1;
    }
    if (allow_numeric &&
        loader_manager_span_equals_nocase(text, length, "0")) {
        *value_out = 0;
        return 1;
    }

    return 0;
}

static int
loader_manager_parse_builtin_bmp_info40_mode(char const *text,
                                             size_t length,
                                             int *value_out,
                                             int allow_numeric)
{
    if (text == NULL || value_out == NULL || length == 0u) {
        return 0;
    }
    if (loader_manager_span_equals_nocase(text, length, "auto")) {
        *value_out = SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_AUTO;
        return 1;
    }
    if (loader_manager_span_equals_nocase(text, length, "windows")) {
        *value_out = SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_WINDOWS;
        return 1;
    }
    if (loader_manager_span_equals_nocase(text, length, "os2")) {
        *value_out = SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_OS2;
        return 1;
    }
    if (allow_numeric &&
        loader_manager_span_equals_nocase(text, length, "0")) {
        *value_out = SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_AUTO;
        return 1;
    }
    if (allow_numeric &&
        loader_manager_span_equals_nocase(text, length, "1")) {
        *value_out = SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_WINDOWS;
        return 1;
    }
    if (allow_numeric &&
        loader_manager_span_equals_nocase(text, length, "2")) {
        *value_out = SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_OS2;
        return 1;
    }

    return 0;
}

static int
loader_manager_read_env_orientation(char const *name, int fallback_value)
{
    char const *env_value;
    int parsed_value;

    env_value = NULL;
    parsed_value = fallback_value;
    if (name == NULL) {
        return fallback_value;
    }

    env_value = sixel_compat_getenv(name);
    if (env_value == NULL || env_value[0] == '\0') {
        return fallback_value;
    }
    if (!loader_manager_parse_orientation(env_value,
                                          strlen(env_value),
                                          &parsed_value,
                                          1)) {
        return fallback_value;
    }

    return parsed_value;
}

static int
loader_manager_read_env_builtin_bmp_info40_mode(char const *name,
                                                int fallback_value)
{
    char const *env_value;
    int parsed_value;

    env_value = NULL;
    parsed_value = fallback_value;
    if (name == NULL) {
        return fallback_value;
    }

    env_value = sixel_compat_getenv(name);
    if (env_value == NULL || env_value[0] == '\0') {
        return fallback_value;
    }
    if (!loader_manager_parse_builtin_bmp_info40_mode(env_value,
                                                       strlen(env_value),
                                                       &parsed_value,
                                                       1)) {
        return fallback_value;
    }

    return parsed_value;
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
    int default_cms_engine;
    int default_orientation;

    if (suboptions == NULL) {
        return;
    }

    default_cms_engine = loader_manager_read_env_cms_engine(
        "SIXEL_LOADER_CMS_ENGINE",
        SIXEL_CMS_ENGINE_NONE);
    default_orientation = loader_manager_read_env_orientation(
        "SIXEL_LOADER_ORIENTATION",
        1);

    suboptions->libjpeg_enable_cms = 0;
    suboptions->libjpeg_cms_engine = loader_manager_read_env_cms_engine(
        "SIXEL_LOADER_LIBJPEG_CMS_ENGINE",
        default_cms_engine);
    suboptions->libjpeg_enable_orientation =
        loader_manager_read_env_orientation(
            "SIXEL_LOADER_LIBJPEG_ORIENTATION",
            default_orientation);
    suboptions->libpng_enable_cms = 0;
    suboptions->libpng_cms_engine = loader_manager_read_env_cms_engine(
        "SIXEL_LOADER_LIBPNG_CMS_ENGINE",
        default_cms_engine);
    suboptions->libpng_enable_orientation =
        loader_manager_read_env_orientation(
            "SIXEL_LOADER_LIBPNG_ORIENTATION",
            default_orientation);
    suboptions->libwebp_enable_cms = 0;
    suboptions->libwebp_cms_engine = loader_manager_read_env_cms_engine(
        "SIXEL_LOADER_LIBWEBP_CMS_ENGINE",
        default_cms_engine);
    suboptions->libwebp_enable_orientation =
        loader_manager_read_env_orientation(
            "SIXEL_LOADER_LIBWEBP_ORIENTATION",
            default_orientation);
    suboptions->coregraphics_enable_orientation =
        loader_manager_read_env_orientation(
            "SIXEL_LOADER_COREGRAPHICS_ORIENTATION",
            default_orientation);
    suboptions->libtiff_enable_cms = 0;
    suboptions->libtiff_cms_engine = loader_manager_read_env_cms_engine(
        "SIXEL_LOADER_LIBTIFF_CMS_ENGINE",
        default_cms_engine);
    suboptions->builtin_enable_cms = 0;
    suboptions->builtin_cms_engine = loader_manager_read_env_cms_engine(
        "SIXEL_LOADER_BUILTIN_CMS_ENGINE",
        default_cms_engine);
    suboptions->builtin_enable_orientation =
        loader_manager_read_env_orientation(
            "SIXEL_LOADER_BUILTIN_ORIENTATION",
            default_orientation);
    suboptions->builtin_bmp_info40_mode =
        loader_manager_read_env_builtin_bmp_info40_mode(
            "SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE",
            SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE_AUTO);
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
            value_text = item->assignments[assignment_index]
                .resolved_value_text;
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
                       strcmp(key_name, "cms_engine") == 0 &&
                       loader_manager_parse_cms_engine(value_text,
                                                       value_length,
                                                       &parsed_value)) {
                suboptions->libpng_cms_engine = parsed_value;
            } else if (strcmp(item->base_def->name, "libpng") == 0 &&
                       strcmp(key_name, "orientation") == 0 &&
                       loader_manager_parse_orientation(value_text,
                                                        value_length,
                                                        &parsed_value,
                                                        0)) {
                suboptions->libpng_enable_orientation = parsed_value;
            } else if (strcmp(item->base_def->name, "libjpeg") == 0 &&
                       strcmp(key_name, "cms_engine") == 0 &&
                       loader_manager_parse_cms_engine(value_text,
                                                       value_length,
                                                       &parsed_value)) {
                suboptions->libjpeg_cms_engine = parsed_value;
            } else if (strcmp(item->base_def->name, "libjpeg") == 0 &&
                       strcmp(key_name, "orientation") == 0 &&
                       loader_manager_parse_orientation(value_text,
                                                        value_length,
                                                        &parsed_value,
                                                        0)) {
                suboptions->libjpeg_enable_orientation = parsed_value;
            } else if (strcmp(item->base_def->name, "libwebp") == 0 &&
                       strcmp(key_name, "cms_engine") == 0 &&
                       loader_manager_parse_cms_engine(value_text,
                                                       value_length,
                                                       &parsed_value)) {
                suboptions->libwebp_cms_engine = parsed_value;
            } else if (strcmp(item->base_def->name, "libwebp") == 0 &&
                       strcmp(key_name, "orientation") == 0 &&
                       loader_manager_parse_orientation(value_text,
                                                        value_length,
                                                        &parsed_value,
                                                        0)) {
                suboptions->libwebp_enable_orientation = parsed_value;
            } else if (strcmp(item->base_def->name, "coregraphics") == 0 &&
                       strcmp(key_name, "orientation") == 0 &&
                       loader_manager_parse_orientation(value_text,
                                                        value_length,
                                                        &parsed_value,
                                                        0)) {
                suboptions->coregraphics_enable_orientation = parsed_value;
            } else if (strcmp(item->base_def->name, "libtiff") == 0 &&
                       strcmp(key_name, "cms_engine") == 0 &&
                       loader_manager_parse_cms_engine(value_text,
                                                       value_length,
                                                       &parsed_value)) {
                suboptions->libtiff_cms_engine = parsed_value;
            } else if (strcmp(item->base_def->name, "builtin") == 0 &&
                       strcmp(key_name, "cms_engine") == 0 &&
                       loader_manager_parse_cms_engine(value_text,
                                                       value_length,
                                                       &parsed_value)) {
                suboptions->builtin_cms_engine = parsed_value;
            } else if (strcmp(item->base_def->name, "builtin") == 0 &&
                       strcmp(key_name, "bmp_info40_mode") == 0 &&
                       loader_manager_parse_builtin_bmp_info40_mode(
                           value_text,
                           value_length,
                           &parsed_value,
                           0)) {
                suboptions->builtin_bmp_info40_mode = parsed_value;
            } else if (strcmp(item->base_def->name, "builtin") == 0 &&
                       strcmp(key_name, "orientation") == 0 &&
                       loader_manager_parse_orientation(value_text,
                                                        value_length,
                                                        &parsed_value,
                                                        0)) {
                suboptions->builtin_enable_orientation = parsed_value;
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

static size_t
loader_manager_find_entry_index(char const *name)
{
    size_t entry_count;
    size_t index;

    entry_count = sizeof(g_sixel_loader_entries)
        / sizeof(g_sixel_loader_entries[0]);
    index = 0u;
    if (name == NULL) {
        return entry_count;
    }
    for (index = 0u; index < entry_count; ++index) {
        if (g_sixel_loader_entries[index].name != NULL &&
            strcmp(g_sixel_loader_entries[index].name, name) == 0) {
            return index;
        }
    }

    return entry_count;
}

size_t
loader_manager_get_entries(sixel_loader_entry_t const **entries)
{
    if (entries != NULL) {
        *entries = g_sixel_loader_entries;
    }
    return sizeof(g_sixel_loader_entries) / sizeof(g_sixel_loader_entries[0]);
}

int
loader_manager_entry_available(char const *name)
{
    size_t index;
    size_t entry_count;

    index = 0u;
    entry_count = loader_manager_get_entries(NULL);
    index = loader_manager_find_entry_index(name);
    return index < entry_count ? 1 : 0;
}

static void
sixel_loader_manager_clear_chain(sixel_loader_manager_t *manager)
{
    struct sixel_loader_manager *object;
    size_t index;

    object = NULL;
    index = 0u;
    if (manager == NULL) {
        return;
    }
    object = (struct sixel_loader_manager *)manager;
    for (index = 0u; index < object->chain_count; ++index) {
        sixel_loader_component_unref(object->chain[index]);
    }
    if (object->chain != NULL) {
        sixel_allocator_free(object->allocator, object->chain);
    }
    object->chain = NULL;
    object->chain_count = 0u;
    object->chain_capacity = 0u;
}

static SIXELSTATUS
sixel_loader_manager_append_chain(sixel_loader_manager_t *manager,
                                  sixel_loader_component_interface_t *loader)
{
    struct sixel_loader_manager *object;
    sixel_loader_component_interface_t **new_chain;
    size_t new_capacity;
    size_t index;

    object = NULL;
    new_chain = NULL;
    new_capacity = 0u;
    index = 0u;
    if (manager == NULL || loader == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    object = (struct sixel_loader_manager *)manager;
    if (object->chain_count == object->chain_capacity) {
        new_capacity = object->chain_capacity == 0u
            ? 8u : object->chain_capacity * 2u;
        new_chain = (sixel_loader_component_interface_t **)
            sixel_allocator_malloc(
                object->allocator,
                new_capacity * sizeof(*new_chain));
        if (new_chain == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        for (index = 0u; index < object->chain_count; ++index) {
            new_chain[index] = object->chain[index];
        }
        if (object->chain != NULL) {
            sixel_allocator_free(object->allocator, object->chain);
        }
        object->chain = new_chain;
        object->chain_capacity = new_capacity;
    }
    object->chain[object->chain_count] = loader;
    ++object->chain_count;
    sixel_loader_component_ref(loader);
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_loader_manager_apply_component_options(
    sixel_loader_component_interface_t *component,
    sixel_loader_manager_build_request_t const *request)
{
    typedef struct loader_component_option_entry {
        int option;
        char const *name;
    } loader_component_option_entry_t;

    loader_component_option_entry_t const options[] = {
        { SIXEL_LOADER_OPTION_REQUIRE_STATIC, "require-static" },
        { SIXEL_LOADER_OPTION_USE_PALETTE, "use-palette" },
        { SIXEL_LOADER_OPTION_REQCOLORS, "reqcolors" },
        { SIXEL_LOADER_OPTION_BGCOLOR, "bgcolor" },
        { SIXEL_LOADER_OPTION_LOOP_CONTROL, "loop-control" },
        { SIXEL_LOADER_OPTION_START_FRAME_NO, "start-frame-no" }
    };
    void const *value;
    char message[128];
    size_t index;
    SIXELSTATUS status;
    int suboption_value;
    char const *component_name;

    value = NULL;
    message[0] = '\0';
    index = 0u;
    status = SIXEL_FALSE;
    suboption_value = 0;
    component_name = NULL;
    if (component == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    component_name = sixel_loader_component_get_name(component);

    for (index = 0u; index < sizeof(options) / sizeof(options[0]); ++index) {
        switch (options[index].option) {
        case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
            value = &request->require_static;
            break;
        case SIXEL_LOADER_OPTION_USE_PALETTE:
            value = &request->use_palette;
            break;
        case SIXEL_LOADER_OPTION_REQCOLORS:
            value = &request->reqcolors;
            break;
        case SIXEL_LOADER_OPTION_BGCOLOR:
            value = request->has_bgcolor ? request->bgcolor : NULL;
            break;
        case SIXEL_LOADER_OPTION_LOOP_CONTROL:
            value = &request->loop_control;
            break;
        case SIXEL_LOADER_OPTION_START_FRAME_NO:
            value = request->has_start_frame_no
                ? &request->start_frame_no : NULL;
            break;
        default:
            value = NULL;
            break;
        }

        status = sixel_loader_component_setopt(component,
                                               options[index].option,
                                               value);
        if (SIXEL_FAILED(status)) {
            (void)sixel_compat_snprintf(message,
                                        sizeof(message),
                                        "sixel_loader_manager_build_chain: "
                                        "failed to apply loader option "
                                        "'%s'.",
                                        options[index].name);
            sixel_helper_set_additional_message(message);
            return status;
        }
    }

    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_BGCOLOR_SOURCE,
        &request->bgcolor_source);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'bgcolor-source'.");
        return status;
    }

    if (request->suboptions == NULL) {
        return SIXEL_OK;
    }

#if HAVE_WIC
    suboption_value = request->suboptions->wic_ico_minsize;
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_WIC_ICO_MINSIZE,
        &suboption_value);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'wic-ico-minsize'.");
        return status;
    }
#endif

    suboption_value = request->suboptions->libpng_enable_cms;
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBPNG_ENABLE_CMS,
        &suboption_value);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'libpng-enable-cms'.");
        return status;
    }

    suboption_value = request->suboptions->libjpeg_enable_cms;
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBJPEG_ENABLE_CMS,
        &suboption_value);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'libjpeg-enable-cms'.");
        return status;
    }

    suboption_value = request->suboptions->libwebp_enable_cms;
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBWEBP_ENABLE_CMS,
        &suboption_value);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'libwebp-enable-cms'.");
        return status;
    }

    suboption_value = request->suboptions->libtiff_enable_cms;
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBTIFF_ENABLE_CMS,
        &suboption_value);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'libtiff-enable-cms'.");
        return status;
    }

    suboption_value = request->suboptions->builtin_enable_cms;
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_BUILTIN_ENABLE_CMS,
        &suboption_value);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'builtin-enable-cms'.");
        return status;
    }

    suboption_value = request->suboptions->builtin_bmp_info40_mode;
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_BUILTIN_BMP_INFO40_MODE,
        &suboption_value);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'builtin-bmp-info40-mode'.");
        return status;
    }

    suboption_value = request->suboptions->builtin_enable_orientation;
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_BUILTIN_ENABLE_ORIENTATION,
        &suboption_value);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'builtin-enable-orientation'.");
        return status;
    }

    suboption_value = request->suboptions->libjpeg_enable_orientation;
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBJPEG_ENABLE_ORIENTATION,
        &suboption_value);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'libjpeg-enable-orientation'.");
        return status;
    }

    suboption_value = request->suboptions->libpng_enable_orientation;
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBPNG_ENABLE_ORIENTATION,
        &suboption_value);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'libpng-enable-orientation'.");
        return status;
    }

    suboption_value = request->suboptions->libwebp_enable_orientation;
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBWEBP_ENABLE_ORIENTATION,
        &suboption_value);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'libwebp-enable-orientation'.");
        return status;
    }

    suboption_value = request->suboptions->coregraphics_enable_orientation;
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_COREGRAPHICS_ENABLE_ORIENTATION,
        &suboption_value);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'coregraphics-enable-orientation'.");
        return status;
    }

    if (component_name != NULL && strcmp(component_name, "libpng") == 0) {
        suboption_value = request->suboptions->libpng_cms_engine;
    } else if (component_name != NULL &&
               strcmp(component_name, "libjpeg") == 0) {
        suboption_value = request->suboptions->libjpeg_cms_engine;
    } else if (component_name != NULL &&
               strcmp(component_name, "libwebp") == 0) {
        suboption_value = request->suboptions->libwebp_cms_engine;
    } else if (component_name != NULL &&
               strcmp(component_name, "libtiff") == 0) {
        suboption_value = request->suboptions->libtiff_cms_engine;
    } else if (component_name != NULL &&
               (strcmp(component_name, "builtin") == 0 ||
                strcmp(component_name, "gnome-thumbnailer") == 0)) {
        suboption_value = request->suboptions->builtin_cms_engine;
    } else {
        suboption_value = 0;
    }
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE,
        &suboption_value);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: failed to apply loader option "
            "'cms-engine'.");
        return status;
    }

    return SIXEL_OK;
}

static void
sixel_loader_manager_format_worker_name(char *worker,
                                        size_t worker_size,
                                        char const *name)
{
    char const *backend_name;

    backend_name = NULL;
    if (worker == NULL || worker_size == 0u) {
        return;
    }

    backend_name = name != NULL && name[0] != '\0' ? name : "unknown";
    (void)sixel_compat_snprintf(worker,
                                worker_size,
                                "loader/%s",
                                backend_name);
}

static int
sixel_loader_manager_status_allows_fallback(SIXELSTATUS status)
{
    switch (status) {
    case SIXEL_FALSE:
    case SIXEL_BAD_INPUT:
    case SIXEL_JPEG_ERROR:
    case SIXEL_PNG_ERROR:
    case SIXEL_WEBP_ERROR:
    case SIXEL_TIFF_ERROR:
    case SIXEL_GDK_ERROR:
    case SIXEL_GD_ERROR:
    case SIXEL_STBI_ERROR:
    case SIXEL_STBIW_ERROR:
    case SIXEL_COM_ERROR:
    case SIXEL_WIC_ERROR:
        return 1;
    default:
        break;
    }

    return 0;
}

static void
sixel_loader_manager_ref_impl(sixel_loader_manager_t *manager)
{
    struct sixel_loader_manager *object;

    object = NULL;
    if (manager == NULL) {
        return;
    }
    object = (struct sixel_loader_manager *)manager;
    (void)sixel_atomic_fetch_add_u32(&object->ref, 1u);
}

static void
sixel_loader_manager_unref_impl(sixel_loader_manager_t *manager)
{
    struct sixel_loader_manager *object;
    unsigned int previous;

    object = NULL;
    previous = 0u;
    if (manager == NULL) {
        return;
    }
    object = (struct sixel_loader_manager *)manager;
    previous = sixel_atomic_fetch_sub_u32(&object->ref, 1u);
    if (previous != 1u) {
        if (previous == 0u) {
            (void)sixel_atomic_fetch_add_u32(&object->ref, 1u);
        }
        return;
    }
    sixel_loader_manager_clear_chain(manager);
    if (object->factory != NULL && object->factory->vtbl != NULL &&
        object->factory->vtbl->unref != NULL) {
        object->factory->vtbl->unref(object->factory);
    }
    sixel_allocator_unref(object->allocator);
    sixel_allocator_free(object->allocator, object);
}

static SIXELSTATUS
sixel_loader_manager_build_chain_impl(
    sixel_loader_manager_t *manager,
    sixel_loader_manager_build_request_t const *request)
{
    struct sixel_loader_manager *object;
    sixel_loader_entry_t const *entries;
    sixel_loader_entry_t const **plan;
    sixel_loader_entry_t const *entry;
    sixel_loader_component_interface_t *loader;
    SIXELSTATUS status;
    size_t entry_count;
    size_t plan_length;
    size_t index;

    object = NULL;
    entries = NULL;
    plan = NULL;
    entry = NULL;
    loader = NULL;
    status = SIXEL_FALSE;
    entry_count = 0u;
    plan_length = 0u;
    index = 0u;
    if (manager == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    object = (struct sixel_loader_manager *)manager;
    object->skip_predicate_gate = 0;
    object->timeline_logger = request != NULL
        ? request->timeline_logger : NULL;
    object->timeline_job_seq = request != NULL
        ? request->timeline_job_seq : NULL;
    object->timeline_loader = request != NULL
        ? request->timeline_loader : NULL;

    entry_count = loader_manager_get_entries(&entries);
    if (entry_count == 0u || entries == NULL) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: no loader entry.");
        return SIXEL_LOGIC_ERROR;
    }

    plan = (sixel_loader_entry_t const **)sixel_allocator_malloc(
        object->allocator,
        entry_count * sizeof(*plan));
    if (plan == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    plan_length = loader_manager_build_plan_from_resolution(
        request != NULL ? request->resolution : NULL,
        entries,
        entry_count,
        plan,
        entry_count);
    if (plan_length == 0u) {
        sixel_allocator_free(object->allocator, plan);
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: no selectable loader.");
        return SIXEL_BAD_ARGUMENT;
    }

    sixel_loader_manager_clear_chain(manager);
    for (index = 0u; index < plan_length; ++index) {
        entry = plan[index];
        if (entry == NULL || entry->classid == NULL) {
            continue;
        }
        status = object->factory->vtbl->create(object->factory,
                                               entry->classid,
                                               object->allocator,
                                               (void **)&loader);
        if (SIXEL_FAILED(status)) {
            sixel_loader_manager_clear_chain(manager);
            sixel_allocator_free(object->allocator, plan);
            return status;
        }
        if (request != NULL) {
            status = sixel_loader_manager_apply_component_options(loader,
                                                                  request);
            if (SIXEL_FAILED(status)) {
                sixel_loader_component_unref(loader);
                loader = NULL;
                sixel_loader_manager_clear_chain(manager);
                sixel_allocator_free(object->allocator, plan);
                return status;
            }
        }
        status = sixel_loader_manager_append_chain(manager, loader);
        sixel_loader_component_unref(loader);
        loader = NULL;
        if (SIXEL_FAILED(status)) {
            sixel_loader_manager_clear_chain(manager);
            sixel_allocator_free(object->allocator, plan);
            return status;
        }
    }
    sixel_allocator_free(object->allocator, plan);
    if (object->chain_count == 0u) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_build_chain: empty chain.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (request != NULL && request->skip_predicate_gate != 0) {
        object->skip_predicate_gate = 1;
    }
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_loader_manager_load_impl(
    sixel_loader_manager_t *manager,
    sixel_chunk_t const *chunk,
    sixel_loader_component_interface_t **selected_loader,
    sixel_load_image_function fn_load,
    void *load_context)
{
    struct sixel_loader_manager *object;
    SIXELSTATUS status;
    sixel_loader_component_interface_t *loader;
    char const *name;
    char worker[96];
    size_t index;
    int enforce_predicate;

    object = NULL;
    status = SIXEL_FALSE;
    loader = NULL;
    name = NULL;
    worker[0] = '\0';
    index = 0u;
    enforce_predicate = 0;
    if (selected_loader != NULL) {
        *selected_loader = NULL;
    }
    if (manager == NULL || chunk == NULL || fn_load == NULL) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_load: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }
    object = (struct sixel_loader_manager *)manager;
    if (object->chain == NULL || object->chain_count == 0u) {
        sixel_helper_set_additional_message(
            "sixel_loader_manager_load: chain is empty.");
        return SIXEL_BAD_ARGUMENT;
    }
    enforce_predicate = object->skip_predicate_gate ? 0 : 1;
    for (index = 0u; index < object->chain_count; ++index) {
        loader = object->chain[index];
        if (loader == NULL) {
            continue;
        }
        if (enforce_predicate &&
            !sixel_loader_component_predicate(loader, chunk)) {
            continue;
        }
        name = sixel_loader_component_get_name(loader);
        sixel_loader_manager_format_worker_name(worker,
                                                sizeof(worker),
                                                name);
        loader_trace_try(name);
        sixel_loader_timeline_candidate_select_start(
            object->timeline_loader,
            worker);
        loader_timeline_scope_begin(object->timeline_logger,
                                    worker,
                                    object->timeline_job_seq);
        status = sixel_loader_component_load(loader,
                                             chunk,
                                             fn_load,
                                             load_context);
        loader_timeline_scope_end();
        sixel_loader_timeline_candidate_select_finish(
            object->timeline_loader,
            status);
        loader_trace_result(name, status);
        if (status == SIXEL_OK) {
            if (selected_loader != NULL) {
                *selected_loader = loader;
            }
            return SIXEL_OK;
        }
        if (SIXEL_SUCCEEDED(status) ||
                !sixel_loader_manager_status_allows_fallback(status)) {
            return status;
        }
        /*
         * A failed decode rejects only the current candidate.  The next
         * backend may still be able to parse the same byte stream, especially
         * when a forced order probes an optional external decoder first.
         */
    }

    return status;
}

static sixel_loader_manager_vtbl_t const g_sixel_loader_manager_vtbl = {
    sixel_loader_manager_ref_impl,
    sixel_loader_manager_unref_impl,
    sixel_loader_manager_build_chain_impl,
    sixel_loader_manager_load_impl
};

SIXELSTATUS
sixel_loader_manager_new(sixel_allocator_t *allocator,
                         void **manager)
{
    struct sixel_loader_manager *object;
    SIXELSTATUS status;

    object = NULL;
    status = SIXEL_FALSE;
    if (allocator == NULL || manager == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *manager = NULL;

    object = (struct sixel_loader_manager *)sixel_allocator_malloc(
        allocator,
        sizeof(*object));
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    object->base.vtbl = &g_sixel_loader_manager_vtbl;
    object->ref = 1u;
    object->allocator = allocator;
    object->factory = NULL;
    object->chain = NULL;
    object->chain_count = 0u;
    object->chain_capacity = 0u;
    object->skip_predicate_gate = 0;
    object->timeline_logger = NULL;
    object->timeline_job_seq = NULL;
    object->timeline_loader = NULL;
    sixel_allocator_ref(allocator);

    status = sixel_factory_get_default((void **)&object->factory);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_unref(object->allocator);
        sixel_allocator_free(allocator, object);
        return status;
    }

    *manager = &object->base;
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
