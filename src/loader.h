/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025-2026 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
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

#ifndef LIBSIXEL_LOADER_H
#define LIBSIXEL_LOADER_H

#include <6cells.h>

#include "chunk.h"

#ifdef __cplusplus
extern "C" {
#endif

SIXEL_INTERNAL_API void
sixel_loader_component_ref(sixel_loader_component_t *component);

SIXEL_INTERNAL_API void
sixel_loader_component_unref(sixel_loader_component_t *component);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_loader_component_setopt(sixel_loader_component_t *component,
                              int option,
                              void const *value);

SIXEL_INTERNAL_API int
sixel_loader_component_predicate(sixel_loader_component_t *component,
                                 sixel_chunk_t const *chunk);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_loader_component_load(sixel_loader_component_t *component,
                            sixel_chunk_t const *chunk,
                            sixel_load_image_function fn_load,
                            void *context);

SIXEL_INTERNAL_API char const *
sixel_loader_component_get_name(sixel_loader_component_t const *component);

SIXEL_INTERNAL_API void
sixel_loader_timeline_candidate_select_start(sixel_loader_t *loader,
                                             char const *worker);

SIXEL_INTERNAL_API void
sixel_loader_timeline_candidate_select_finish(sixel_loader_t *loader,
                                              SIXELSTATUS status);

SIXEL_INTERNAL_API void
sixel_helper_set_thumbnail_size_hint(int size);

SIXEL_INTERNAL_API int
sixel_loader_callback_is_canceled(void *data);

SIXEL_INTERNAL_API int
sixel_loader_get_start_frame_no(sixel_loader_t const *loader,
                                int *start_frame_no);

SIXEL_INTERNAL_API SIXELSTATUS
sixel_loader_set_cancel_callback(
    sixel_loader_t *loader,
    sixel_cancel_function cancel_function,
    void *cancel_context);

typedef int (*sixel_loader_wait_predicate_t)(void *context);

SIXEL_INTERNAL_API int
sixel_loader_is_osc11_bg_query_enabled(char const *value);

SIXEL_INTERNAL_API int
sixel_loader_parse_osc11_bg_query_timeout_ms(char const *value);

SIXEL_INTERNAL_API int
sixel_loader_wait_for_condition(sixel_loader_wait_predicate_t predicate,
                                void *context,
                                int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_LOADER_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
