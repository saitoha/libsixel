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

#ifndef LIBSIXEL_LOADER_MANAGER_H
#define LIBSIXEL_LOADER_MANAGER_H

#include <6cells.h>

#include "chunk.h"
#include "timeline-logger.h"
#include "loader.h"
#include "options.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sixel_loader_entry {
    char const *name;
    char const *classid;
    int default_enabled;
} sixel_loader_entry_t;

/* @classid loader/manager */
SIXEL_INTERNAL_API SIXELSTATUS
sixel_loader_manager_new(sixel_allocator_t *allocator,
                         void **manager);

SIXEL_INTERNAL_API void
sixel_loader_manager_set_prefer_float32(sixel_loader_manager_t *manager,
                                        int prefer_float32);

SIXELSTATUS
loader_manager_parse_loader_order(
    char const *order,
    sixel_option_argument_list_resolution_t *resolution);

void
loader_manager_init_loader_suboptions(
    sixel_loader_suboptions_t *suboptions);

void
loader_manager_resolve_loader_suboptions(
    sixel_option_argument_list_resolution_t const *resolution,
    sixel_loader_suboptions_t *suboptions);

size_t
loader_manager_build_plan_from_resolution(
    sixel_option_argument_list_resolution_t const *resolution,
    sixel_loader_entry_t const *entries,
    size_t entry_count,
    sixel_loader_entry_t const **plan,
    size_t plan_capacity);

SIXEL_INTERNAL_API size_t
loader_manager_get_entries(sixel_loader_entry_t const **entries);

SIXEL_INTERNAL_API int
loader_manager_entry_available(char const *name);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_LOADER_MANAGER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
