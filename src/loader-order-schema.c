/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include "loader-order-schema.h"

#include <limits.h>
#if HAVE_STRING_H
# include <string.h>
#endif

enum {
    SIXEL_LOADER_SCHEMA_CHOICE_LIBPNG = 0,
    SIXEL_LOADER_SCHEMA_CHOICE_LIBJPEG,
    SIXEL_LOADER_SCHEMA_CHOICE_LIBWEBP,
    SIXEL_LOADER_SCHEMA_CHOICE_LIBTIFF,
    SIXEL_LOADER_SCHEMA_CHOICE_LIBRSVG,
    SIXEL_LOADER_SCHEMA_CHOICE_BUILTIN,
    SIXEL_LOADER_SCHEMA_CHOICE_WIC,
    SIXEL_LOADER_SCHEMA_CHOICE_COREGRAPHICS,
    SIXEL_LOADER_SCHEMA_CHOICE_GDK_PIXBUF2,
    SIXEL_LOADER_SCHEMA_CHOICE_GD,
    SIXEL_LOADER_SCHEMA_CHOICE_QUICKLOOK,
    SIXEL_LOADER_SCHEMA_CHOICE_GNOME_THUMBNAILER
};

static sixel_suboption_choice_t const g_suboption_choices_loader_enable_cms[] = {
    { "0", 0 },
    { "1", 1 }
};

static sixel_suboption_key_t const g_subkeys_loader_enable_cms[] = {
    {
        "enable_cms",
        "e",
        NULL,
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_suboption_choices_loader_enable_cms,
        sizeof(g_suboption_choices_loader_enable_cms)
            / sizeof(g_suboption_choices_loader_enable_cms[0])
    }
};

#if HAVE_WIC
/*
 * The parser accepts free-form text for ico_minsize; semantic validation
 * happens in sixel_loader_order_validate_resolution().
 */
static sixel_suboption_key_t const g_subkeys_loader_wic[] = {
    {
        "ico_minsize",
        NULL,
        "SIXEL_LODER_WIC_ICO_MINSIZE",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    }
};
#endif

static sixel_option_value_schema_t const g_schema_loader_values[] = {
#if HAVE_LIBPNG
    {
        "libpng",
        SIXEL_LOADER_SCHEMA_CHOICE_LIBPNG,
        g_subkeys_loader_enable_cms,
        sizeof(g_subkeys_loader_enable_cms)
            / sizeof(g_subkeys_loader_enable_cms[0])
    },
#endif
#if HAVE_JPEG
    { "libjpeg", SIXEL_LOADER_SCHEMA_CHOICE_LIBJPEG, NULL, 0u },
#endif
#if HAVE_WEBP
    { "libwebp", SIXEL_LOADER_SCHEMA_CHOICE_LIBWEBP, NULL, 0u },
#endif
#if HAVE_LIBTIFF
    { "libtiff", SIXEL_LOADER_SCHEMA_CHOICE_LIBTIFF, NULL, 0u },
#endif
#if HAVE_LIBRSVG
    { "librsvg", SIXEL_LOADER_SCHEMA_CHOICE_LIBRSVG, NULL, 0u },
#endif
    {
        "builtin",
        SIXEL_LOADER_SCHEMA_CHOICE_BUILTIN,
        g_subkeys_loader_enable_cms,
        sizeof(g_subkeys_loader_enable_cms)
            / sizeof(g_subkeys_loader_enable_cms[0])
    },
#if HAVE_WIC
    {
        "wic",
        SIXEL_LOADER_SCHEMA_CHOICE_WIC,
        g_subkeys_loader_wic,
        sizeof(g_subkeys_loader_wic) / sizeof(g_subkeys_loader_wic[0])
    },
#endif
#if HAVE_COREGRAPHICS
    { "coregraphics", SIXEL_LOADER_SCHEMA_CHOICE_COREGRAPHICS, NULL, 0u },
#endif
#ifdef HAVE_GDK_PIXBUF2
    { "gdk-pixbuf2", SIXEL_LOADER_SCHEMA_CHOICE_GDK_PIXBUF2, NULL, 0u },
#endif
#if HAVE_GD
    { "gd", SIXEL_LOADER_SCHEMA_CHOICE_GD, NULL, 0u },
#endif
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
    { "quicklook", SIXEL_LOADER_SCHEMA_CHOICE_QUICKLOOK, NULL, 0u },
#endif
#if HAVE_FREEDESKTOP_THUMBNAILING
    {
        "gnome-thumbnailer",
        SIXEL_LOADER_SCHEMA_CHOICE_GNOME_THUMBNAILER,
        NULL,
        0u
    },
#endif
};

static sixel_option_argument_schema_t const g_schema_loaders = {
    SIXEL_OPTFLAG_LOADERS,
    "--loaders",
    g_schema_loader_values,
    sizeof(g_schema_loader_values) / sizeof(g_schema_loader_values[0])
};

sixel_option_argument_schema_t const *
sixel_loader_order_schema_get(void)
{
    return &g_schema_loaders;
}

static int
sixel_loader_order_parse_positive_int(char const *text,
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

SIXELSTATUS
sixel_loader_order_validate_resolution(
    sixel_option_argument_list_resolution_t const *resolution)
{
    size_t item_index;
    size_t assignment_index;
    sixel_option_argument_resolution_t const *item;
    sixel_suboption_assignment_t const *assignment;
    char const *base_name;
    int parsed_value;

    item_index = 0u;
    assignment_index = 0u;
    item = NULL;
    assignment = NULL;
    base_name = NULL;
    parsed_value = 0;

    if (resolution == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    while (item_index < resolution->item_count) {
        item = &resolution->items[item_index].resolution;
        if (item->base_def == NULL || item->base_def->name == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
        base_name = item->base_def->name;

        if (strcmp(base_name, "wic") != 0 &&
            strcmp(base_name, "libpng") != 0 &&
            strcmp(base_name, "builtin") != 0 &&
            item->assignment_count > 0u) {
            sixel_helper_set_additional_message(
                "specified loader does not support suboptions.");
            return SIXEL_BAD_ARGUMENT;
        }

        assignment_index = 0u;
        while (assignment_index < item->assignment_count) {
            assignment = item->assignments + assignment_index;
            if (assignment->resolved_key_name == NULL ||
                assignment->resolved_value_text == NULL) {
                return SIXEL_BAD_ARGUMENT;
            }
            if (strcmp(base_name, "wic") == 0 &&
                strcmp(assignment->resolved_key_name, "ico_minsize") == 0) {
                if (!sixel_loader_order_parse_positive_int(
                        assignment->resolved_value_text,
                        strlen(assignment->resolved_value_text),
                        &parsed_value)) {
                    sixel_helper_set_additional_message(
                        "invalid wic suboption; expected :ico_minsize=SIZE.");
                    return SIXEL_BAD_ARGUMENT;
                }
            }
            ++assignment_index;
        }
        ++item_index;
    }

    return SIXEL_OK;
}

SIXELSTATUS
sixel_loader_order_parse_and_validate(
    char const *argument,
    sixel_option_argument_list_resolution_t *resolution,
    char *diagnostic,
    size_t diagnostic_size)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (resolution == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_option_parse_argument_list_with_suboptions(
        argument,
        sixel_loader_order_schema_get(),
        resolution,
        diagnostic,
        diagnostic_size);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_loader_order_validate_resolution(resolution);
    if (SIXEL_FAILED(status)) {
        sixel_option_free_argument_list_resolution(resolution);
        return status;
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
