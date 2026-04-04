/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "options.h"
#include "fuzz-loader-builtin-struct-common.h"

static sixel_suboption_choice_t const g_choice_onoff[] = {
    { "on", 1 },
    { "off", 0 }
};

static sixel_suboption_choice_t const g_choice_cms[] = {
    { "none", 0 },
    { "auto", 1 },
    { "builtin", 2 },
    { "lcms2", 3 },
    { "colorsync", 4 }
};

static sixel_suboption_choice_t const g_choice_tonemap[] = {
    { "none", 0 },
    { "reinhard", 1 },
    { "aces", 2 },
    { "hable", 3 }
};

static sixel_suboption_choice_t const g_choice_colorspace[] = {
    { "gamma", 0 },
    { "linear", 1 },
    { "cielab", 2 },
    { "oklab", 3 },
    { "din99d", 4 }
};

static sixel_suboption_key_t const g_subkeys_builtin[] = {
    {
        "cms_engine",
        "e",
        "SIXEL_LOADER_BUILTIN_CMS_ENGINE",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_choice_cms,
        sizeof(g_choice_cms) / sizeof(g_choice_cms[0])
    },
    {
        "target",
        "t",
        "SIXEL_LOADER_CMS_TARGET_COLORSPACE",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_choice_colorspace,
        sizeof(g_choice_colorspace) / sizeof(g_choice_colorspace[0])
    }
};

static sixel_suboption_key_t const g_subkeys_hdr[] = {
    {
        "tonemap",
        "t",
        "SIXEL_LOADER_HDR_TONEMAP",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_choice_tonemap,
        sizeof(g_choice_tonemap) / sizeof(g_choice_tonemap[0])
    },
    {
        "ev",
        "e",
        "SIXEL_LOADER_HDR_EXPOSURE_EV",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    },
    {
        "header",
        "h",
        "SIXEL_LOADER_HDR_USE_HEADER_EXPOSURE",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_choice_onoff,
        sizeof(g_choice_onoff) / sizeof(g_choice_onoff[0])
    }
};

static sixel_suboption_key_t const g_subkeys_wic[] = {
    {
        "ico_minsize",
        "m",
        "SIXEL_LOADER_WIC_ICO_MINSIZE",
        SIXEL_SUBOPTION_VALUE_FREE,
        NULL,
        0u
    }
};

static sixel_suboption_key_t const g_subkeys_generic[] = {
    {
        "orientation",
        "o",
        "SIXEL_LOADER_ORIENTATION",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_choice_onoff,
        sizeof(g_choice_onoff) / sizeof(g_choice_onoff[0])
    },
    {
        "cms_engine",
        "e",
        "SIXEL_LOADER_CMS_ENGINE",
        SIXEL_SUBOPTION_VALUE_CHOICE,
        g_choice_cms,
        sizeof(g_choice_cms) / sizeof(g_choice_cms[0])
    }
};

static sixel_option_value_schema_t const g_values[] = {
    {
        "builtin",
        0,
        g_subkeys_builtin,
        sizeof(g_subkeys_builtin) / sizeof(g_subkeys_builtin[0])
    },
    {
        "hdr",
        1,
        g_subkeys_hdr,
        sizeof(g_subkeys_hdr) / sizeof(g_subkeys_hdr[0])
    },
    {
        "wic",
        2,
        g_subkeys_wic,
        sizeof(g_subkeys_wic) / sizeof(g_subkeys_wic[0])
    },
    {
        "libpng",
        3,
        g_subkeys_generic,
        sizeof(g_subkeys_generic) / sizeof(g_subkeys_generic[0])
    },
    {
        "libjpeg",
        4,
        g_subkeys_generic,
        sizeof(g_subkeys_generic) / sizeof(g_subkeys_generic[0])
    },
    {
        "coregraphics",
        5,
        g_subkeys_generic,
        sizeof(g_subkeys_generic) / sizeof(g_subkeys_generic[0])
    }
};

static sixel_option_argument_schema_t const g_schema = {
    SIXEL_OPTFLAG_LOADERS,
    "loaders",
    g_values,
    sizeof(g_values) / sizeof(g_values[0])
};

static char const *g_free_values[] = {
    "0",
    "1",
    "16",
    "255",
    "256",
    "-1",
    "2147483647",
    "999999999999",
    "0.0",
    "0.5",
    "-2.0",
    "garbage",
    ""
};

static int
fuzz_append_text(char *buffer, size_t buffer_size, size_t *offset, char const *text)
{
    size_t len;

    if (buffer == NULL || offset == NULL || text == NULL) {
        return 0;
    }

    len = strlen(text);
    if (*offset + len + 1u > buffer_size) {
        return 0;
    }

    memcpy(buffer + *offset, text, len);
    *offset += len;
    buffer[*offset] = '\0';
    return 1;
}

static char const *
fuzz_pick_free_value(fuzz_cursor_t *cursor)
{
    size_t index;

    index = (size_t)(fuzz_cursor_take_u8(cursor, 0u)
                     % (sizeof(g_free_values) / sizeof(g_free_values[0])));
    return g_free_values[index];
}

static int
fuzz_build_argument(fuzz_cursor_t *cursor, char *buffer, size_t buffer_size)
{
    size_t item_count;
    size_t item_index;
    size_t offset;

    if (cursor == NULL || buffer == NULL || buffer_size < 8u) {
        return 0;
    }

    buffer[0] = '\0';
    offset = 0u;

    item_count = 1u + (size_t)(fuzz_cursor_take_u8(cursor, 0u) % 4u);
    for (item_index = 0u; item_index < item_count; ++item_index) {
        sixel_option_value_schema_t const *value_def;
        size_t value_index;
        size_t sub_count;
        size_t sub_index;

        value_index = (size_t)(fuzz_cursor_take_u8(cursor, 0u)
                               % (sizeof(g_values) / sizeof(g_values[0])));
        value_def = &g_values[value_index];

        if (item_index > 0u) {
            if (!fuzz_append_text(buffer, buffer_size, &offset, ",")) {
                return 0;
            }
        }
        if (!fuzz_append_text(buffer, buffer_size, &offset, value_def->name)) {
            return 0;
        }

        if (value_def->subkey_count == 0u) {
            continue;
        }

        sub_count = (size_t)(fuzz_cursor_take_u8(cursor, 0u)
                             % (value_def->subkey_count + 1u));
        for (sub_index = 0u; sub_index < sub_count; ++sub_index) {
            sixel_suboption_key_t const *key_def;
            size_t key_index;

            key_index = (size_t)(fuzz_cursor_take_u8(cursor, 0u)
                                 % value_def->subkey_count);
            key_def = &value_def->subkeys[key_index];

            if (!fuzz_append_text(buffer, buffer_size, &offset, ":")) {
                return 0;
            }

            if ((fuzz_cursor_take_u8(cursor, 0u) & 0x01u) != 0u &&
                key_def->short_name != NULL && key_def->short_name[0] != '\0') {
                if (!fuzz_append_text(buffer,
                                      buffer_size,
                                      &offset,
                                      key_def->short_name)) {
                    return 0;
                }
            } else {
                if (!fuzz_append_text(buffer, buffer_size, &offset, key_def->name)) {
                    return 0;
                }
            }

            if (!fuzz_append_text(buffer, buffer_size, &offset, "=")) {
                return 0;
            }

            if (key_def->value_kind == SIXEL_SUBOPTION_VALUE_CHOICE &&
                key_def->choices != NULL &&
                key_def->choice_count > 0u) {
                size_t choice_index;

                choice_index = (size_t)(fuzz_cursor_take_u8(cursor, 0u)
                                        % key_def->choice_count);
                if (!fuzz_append_text(buffer,
                                      buffer_size,
                                      &offset,
                                      key_def->choices[choice_index].name)) {
                    return 0;
                }
            } else {
                if (!fuzz_append_text(buffer,
                                      buffer_size,
                                      &offset,
                                      fuzz_pick_free_value(cursor))) {
                    return 0;
                }
            }
        }
    }

    if ((fuzz_cursor_take_u8(cursor, 0u) & 0x03u) == 0u) {
        if (!fuzz_append_text(buffer, buffer_size, &offset, "!")) {
            return 0;
        }
    }

    return 1;
}

static void
fuzz_verify_roundtrip(char const *argument)
{
    sixel_option_argument_list_resolution_t resolution;
    sixel_option_argument_list_resolution_t reparsed;
    SIXELSTATUS status;
    char diagnostic[512];

    if (argument == NULL) {
        return;
    }

    memset(&resolution, 0, sizeof(resolution));
    memset(&reparsed, 0, sizeof(reparsed));
    memset(diagnostic, 0, sizeof(diagnostic));

    sixel_option_init_argument_list_resolution(&resolution);
    sixel_option_init_argument_list_resolution(&reparsed);

    status = sixel_option_parse_argument_list_with_suboptions(
        argument,
        &g_schema,
        &resolution,
        diagnostic,
        sizeof(diagnostic));
    if (SIXEL_FAILED(status)) {
        sixel_option_free_argument_list_resolution(&resolution);
        sixel_option_free_argument_list_resolution(&reparsed);
        return;
    }

    if (resolution.canonical_argument == NULL) {
        abort();
    }

    status = sixel_option_parse_argument_list_with_suboptions(
        resolution.canonical_argument,
        &g_schema,
        &reparsed,
        diagnostic,
        sizeof(diagnostic));
    if (SIXEL_FAILED(status)) {
        abort();
    }

    if (reparsed.canonical_argument == NULL) {
        abort();
    }

    if (strcmp(resolution.canonical_argument,
               reparsed.canonical_argument) != 0) {
        abort();
    }

    if (resolution.item_count != reparsed.item_count) {
        abort();
    }

    if (resolution.has_trailing_bang != reparsed.has_trailing_bang) {
        abort();
    }

    sixel_option_free_argument_list_resolution(&resolution);
    sixel_option_free_argument_list_resolution(&reparsed);
}

int
LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
    enum { FUZZ_MAX_INPUT_BYTES = 256 * 1024 };

    fuzz_cursor_t cursor;
    char argument[1024];

    if (data == NULL || size > (size_t)FUZZ_MAX_INPUT_BYTES) {
        return 0;
    }

    memset(argument, 0, sizeof(argument));
    fuzz_cursor_init(&cursor, data, size);
    if (!fuzz_build_argument(&cursor, argument, sizeof(argument))) {
        return 0;
    }

    fuzz_verify_roundtrip(argument);
    return 0;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
