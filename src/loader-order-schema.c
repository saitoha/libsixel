/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include "loader-order-schema.h"

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
 * The parser accepts free-form text for ico_minsize and validates it later.
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

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
