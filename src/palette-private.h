/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_PALETTE_PRIVATE_H
#define LIBSIXEL_PALETTE_PRIVATE_H

#include "palette.h"
#include "sixel_atomic.h"

/*
 * Palette storage stays private to the palette implementation family.  The
 * public component pointer is the vtbl-only sixel_palette_t interface; files
 * outside src/palette*.c must use the generated view and metadata methods.
 */
typedef struct sixel_palette_build_context {
    unsigned int requested_colors;
    int method_for_largest;
    int method_for_rep;
    int quality_mode;
    int force_palette;
    int use_reversible;
    int quantize_model;
    int final_merge_mode;
    int lut_policy;
} sixel_palette_build_context_t;

typedef struct sixel_palette_storage {
    sixel_palette_t palette_interface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    unsigned char *entries;
    size_t entries_size;
    float *entries_float32;
    size_t entries_float32_size;
    unsigned int entry_count;
    unsigned int requested_colors;
    unsigned int original_colors;
    int depth;
    int float_depth;
    sixel_palette_build_context_t *build_context;
} sixel_palette_storage_t;

static inline sixel_palette_storage_t *
sixel_palette_storage_from_interface(sixel_palette_t *palette)
{
    return (sixel_palette_storage_t *)palette;
}

static inline sixel_palette_storage_t const *
sixel_palette_storage_from_interface_const(sixel_palette_t const *palette)
{
    return (sixel_palette_storage_t const *)palette;
}

static inline sixel_palette_build_context_t *
sixel_palette_context_from_interface(sixel_palette_t *palette)
{
    sixel_palette_storage_t *storage;

    storage = sixel_palette_storage_from_interface(palette);
    if (storage == NULL) {
        return NULL;
    }
    return storage->build_context;
}

#define SIXEL_PALETTE_STORAGE(palette) \
    sixel_palette_storage_from_interface(palette)
#define SIXEL_PALETTE_STORAGE_CONST(palette) \
    sixel_palette_storage_from_interface_const(palette)
#define SIXEL_PALETTE_CONTEXT(palette) \
    sixel_palette_context_from_interface(palette)

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_resize(sixel_palette_t *palette,
                     unsigned int colors,
                     int depth,
                     sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_set_entries(sixel_palette_t *palette,
                          unsigned char const *entries,
                          unsigned int colors,
                          int depth,
                          sixel_allocator_t *allocator);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_palette_set_entries_float32(sixel_palette_t *palette,
                                  float const *entries,
                                  unsigned int colors,
                                  int depth,
                                  sixel_allocator_t *allocator);

SIXEL_INTERNAL_API int
sixel_palette_timeline_next_job_id(void);

#endif /* LIBSIXEL_PALETTE_PRIVATE_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
