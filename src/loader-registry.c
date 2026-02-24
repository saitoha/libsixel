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
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Loader registry table extracted from loader.c so backend ordering and
 * availability checks live in one place.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_STRING_H
# include <string.h>
#endif

#include "sixel_atomic.h"

#include "loader-builtin.h"
#include "loader-coregraphics.h"
#include "loader-gd.h"
#include "loader-gdk-pixbuf2.h"
#include "loader-gnome-thumbnailer.h"
#include "loader-libjpeg.h"
#include "loader-libpng.h"
#include "loader-libtiff.h"
#include "loader-librsvg.h"
#include "loader-libwebp.h"
#include "loader-quicklook.h"
#include "loader-registry.h"
#include "loader-wic.h"


#if HAVE_LIBPNG
static char const * const g_magic_png[] = { "\x89PNG" };
#endif
#if HAVE_JPEG
static char const * const g_magic_jpeg[] = { "\xff\xd8" };
#endif
#if HAVE_WEBP
static char const * const g_magic_webp[] = { "RIFF" };
#endif
#if HAVE_LIBTIFF
static char const * const g_magic_tiff[] = { "II", "MM" };
#endif
#if HAVE_LIBRSVG
static char const * const g_magic_svg[] = { "<svg", "<?xml" };
#endif
#if HAVE_GD
static char const * const g_magic_gif[] = { "GIF8" };
#endif

static sixel_loader_entry_t const sixel_loader_entries[] = {
    /*
     * Fast loaders take precedence so probing prefers native decoders.
     *
     * 1. libpng   2. libjpeg   3. libwebp   4. libtiff   5. builtin
     * 6+. remaining loaders
     */
#if HAVE_LIBPNG
    {
        "libpng",
        NULL,
        loader_can_try_libpng,
        sixel_loader_libpng_new,
        g_magic_png,
        sizeof(g_magic_png) / sizeof(g_magic_png[0]),
        1
    },
#endif
#if HAVE_JPEG
    {
        "libjpeg",
        NULL,
        loader_can_try_libjpeg,
        sixel_loader_libjpeg_new,
        g_magic_jpeg,
        sizeof(g_magic_jpeg) / sizeof(g_magic_jpeg[0]),
        1
    },
#endif
#if HAVE_WEBP
    {
        "libwebp",
        NULL,
        loader_can_try_libwebp,
        sixel_loader_libwebp_new,
        g_magic_webp,
        sizeof(g_magic_webp) / sizeof(g_magic_webp[0]),
        1
    },
#endif
#if HAVE_LIBTIFF
    {
        "libtiff",
        NULL,
        loader_can_try_libtiff,
        sixel_loader_libtiff_new,
        g_magic_tiff,
        sizeof(g_magic_tiff) / sizeof(g_magic_tiff[0]),
        1
    },
#endif
#if HAVE_LIBRSVG
    {
        "librsvg",
        load_with_librsvg,
        loader_can_try_librsvg,
        NULL,
        g_magic_svg,
        sizeof(g_magic_svg) / sizeof(g_magic_svg[0]),
        1
    },
#endif
    { "builtin", NULL, NULL, sixel_loader_builtin_new, NULL, 0u, 1 },
#if HAVE_WIC
    { "wic", load_with_wic, loader_can_try_wic, NULL, NULL, 0u, 1 },
#endif
#if HAVE_COREGRAPHICS
    { "coregraphics", load_with_coregraphics, NULL, NULL, NULL, 0u, 1 },
#endif
#ifdef HAVE_GDK_PIXBUF2
    { "gdk-pixbuf2", load_with_gdkpixbuf, NULL, NULL, NULL, 0u, 1 },
#endif
#if HAVE_GD
    {
        "gd",
        NULL,
        NULL,
        sixel_loader_gd_new,
        g_magic_gif,
        sizeof(g_magic_gif) / sizeof(g_magic_gif[0]),
        1
    },
#endif
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
    {
        "quicklook",
        load_with_quicklook,
        loader_quicklook_can_decode_chunk,
        NULL,
        NULL,
        0u,
        1
    },
#endif
#if HAVE_FREEDESKTOP_THUMBNAILING
    { "gnome-thumbnailer", load_with_gnome_thumbnailer, NULL, NULL, NULL, 0u, 0 },
#endif
};

struct sixel_loader_registry {
    sixel_atomic_u32_t ref;
    sixel_loader_entry_t const *entries;
    size_t entry_count;
};

static struct sixel_loader_registry g_loader_registry_singleton = {
    0u,
    sixel_loader_entries,
    sizeof(sixel_loader_entries) / sizeof(sixel_loader_entries[0])
};

static void
loader_registry_unref_singleton_ref(sixel_atomic_u32_t *ref)
{
    unsigned int previous;

    previous = 0u;
    if (ref == NULL) {
        return;
    }

    /*
     * Registry table has process lifetime. Refcount tracks borrows only,
     * therefore unref() saturates at zero to avoid counter underflow.
     */
    previous = sixel_atomic_fetch_sub_u32(ref, 1u);
    if (previous == 0u) {
        (void)sixel_atomic_fetch_add_u32(ref, 1u);
    }
}

SIXELSTATUS
loader_registry_get_default(sixel_loader_registry_t **ppregistry)
{
    if (ppregistry == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    loader_registry_ref(&g_loader_registry_singleton);
    *ppregistry = &g_loader_registry_singleton;

    return SIXEL_OK;
}

void
loader_registry_ref(sixel_loader_registry_t *registry)
{
    if (registry == NULL) {
        return;
    }

    (void)sixel_atomic_fetch_add_u32(&registry->ref, 1u);
}

void
loader_registry_unref(sixel_loader_registry_t *registry)
{
    if (registry == NULL) {
        return;
    }

    loader_registry_unref_singleton_ref(&registry->ref);
}

size_t
loader_registry_get_entries_from(
    sixel_loader_registry_t const *registry,
    sixel_loader_entry_t const **entries)
{
    if (entries != NULL) {
        *entries = NULL;
    }

    if (registry == NULL) {
        return 0u;
    }

    if (entries != NULL) {
        *entries = registry->entries;
    }

    return registry->entry_count;
}

int
loader_registry_entry_available_from(
    sixel_loader_registry_t const *registry,
    char const *name)
{
    size_t index;

    index = 0u;
    if (registry == NULL || name == NULL) {
        return 0;
    }

    for (index = 0; index < registry->entry_count; ++index) {
        if (registry->entries[index].name != NULL &&
                strcmp(registry->entries[index].name, name) == 0) {
            return 1;
        }
    }

    return 0;
}

size_t
loader_registry_get_entries(sixel_loader_entry_t const **entries)
{
    return loader_registry_get_entries_from(&g_loader_registry_singleton,
                                            entries);
}

int
loader_registry_entry_available(char const *name)
{
    return loader_registry_entry_available_from(&g_loader_registry_singleton,
                                                name);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
