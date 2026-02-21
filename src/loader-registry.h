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
 * Loader registry accessors and entry definitions.
 */

#ifndef LIBSIXEL_LOADER_REGISTRY_H
#define LIBSIXEL_LOADER_REGISTRY_H

#include <sixel.h>

#include "chunk.h"

typedef int (*sixel_loader_predicate)(sixel_chunk_t const *pchunk);

typedef SIXELSTATUS (*sixel_loader_backend)(
    sixel_chunk_t const       *pchunk,
    int                        fstatic,
    int                        fuse_palette,
    int                        reqcolors,
    unsigned char             *bgcolor,
    int                        loop_control,
    int                        start_frame_no_set,
    int                        start_frame_no,
    sixel_load_image_function  fn_load,
    void                      *context);

typedef struct sixel_loader_entry {
    char const              *name;
    sixel_loader_backend     backend;
    sixel_loader_predicate   predicate;
    int                      default_enabled;
} sixel_loader_entry_t;

size_t loader_registry_get_entries(sixel_loader_entry_t const **entries);
int loader_registry_entry_available(char const *name);

#endif /* LIBSIXEL_LOADER_REGISTRY_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
