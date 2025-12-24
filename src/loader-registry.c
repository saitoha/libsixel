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

#include "loader-builtin.h"
#include "loader-coregraphics.h"
#include "loader-gd.h"
#include "loader-gdk-pixbuf2.h"
#include "loader-gnome-thumbnailer.h"
#include "loader-libjpeg.h"
#include "loader-libpng.h"
#include "loader-quicklook.h"
#include "loader-registry.h"
#include "loader-wic.h"

static sixel_loader_entry_t const sixel_loader_entries[] = {
    /*
     * Fast loaders take precedence so probing prefers native decoders.
     *
     * 1. libpng   2. libjpeg   3. builtin   4+. remaining generic loaders
     */
#if HAVE_LIBPNG
    { "libpng", load_with_libpng, loader_can_try_libpng, 1 },
#endif
#if HAVE_JPEG
    { "libjpeg", load_with_libjpeg, loader_can_try_libjpeg, 1 },
#endif
    { "builtin", load_with_builtin, NULL, 1 },
#if HAVE_WIC
    { "wic", load_with_wic, loader_can_try_wic, 1 },
#endif
#if HAVE_COREGRAPHICS
    { "coregraphics", load_with_coregraphics, NULL, 1 },
#endif
#ifdef HAVE_GDK_PIXBUF2
    { "gdk-pixbuf2", load_with_gdkpixbuf, NULL, 1 },
#endif
#if HAVE_GD
    { "gd", load_with_gd, NULL, 1 },
#endif
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
    { "quicklook", load_with_quicklook, loader_quicklook_can_decode_chunk, 1 },
#endif
#if HAVE_UNISTD_H && HAVE_SYS_WAIT_H && HAVE_FORK
    { "gnome-thumbnailer", load_with_gnome_thumbnailer, NULL, 0 },
#endif
};

size_t
loader_registry_get_entries(sixel_loader_entry_t const **entries)
{
    if (entries != NULL) {
        *entries = sixel_loader_entries;
    }

    return sizeof(sixel_loader_entries) /
           sizeof(sixel_loader_entries[0]);
}

int
loader_registry_entry_available(char const *name)
{
    size_t index;
    size_t entry_count;

    if (name == NULL) {
        return 0;
    }

    entry_count = sizeof(sixel_loader_entries) /
                  sizeof(sixel_loader_entries[0]);

    for (index = 0; index < entry_count; ++index) {
        if (sixel_loader_entries[index].name != NULL &&
                strcmp(sixel_loader_entries[index].name, name) == 0) {
            return 1;
        }
    }

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
