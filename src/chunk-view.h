/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2016 Hayaki Saito
 */

#ifndef LIBSIXEL_CHUNK_VIEW_H
#define LIBSIXEL_CHUNK_VIEW_H

#include "chunk.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * These small adapters keep internal call sites readable while still routing
 * all non-construction access through the IDL-generated vtbl contract.
 */
static inline SIXELSTATUS
sixel_chunk_get_bytes(sixel_chunk_t const *chunk,
                      sixel_chunk_bytes_view_t *view)
{
    if (chunk == NULL || chunk->vtbl == NULL ||
        chunk->vtbl->get_bytes == NULL || view == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    return chunk->vtbl->get_bytes(chunk, view);
}

static inline unsigned char const *
sixel_chunk_get_buffer(sixel_chunk_t const *chunk)
{
    sixel_chunk_bytes_view_t view;

    view.bytes = NULL;
    view.size = 0u;
    if (sixel_chunk_get_bytes(chunk, &view) != SIXEL_OK) {
        return NULL;
    }

    return view.bytes;
}

static inline size_t
sixel_chunk_get_size(sixel_chunk_t const *chunk)
{
    sixel_chunk_bytes_view_t view;

    view.bytes = NULL;
    view.size = 0u;
    if (sixel_chunk_get_bytes(chunk, &view) != SIXEL_OK) {
        return 0u;
    }

    return view.size;
}

static inline char const *
sixel_chunk_get_source_path(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->vtbl == NULL ||
        chunk->vtbl->source_path == NULL) {
        return NULL;
    }

    return chunk->vtbl->source_path(chunk);
}

static inline sixel_allocator_t *
sixel_chunk_get_allocator(sixel_chunk_t const *chunk)
{
    if (chunk == NULL || chunk->vtbl == NULL ||
        chunk->vtbl->allocator == NULL) {
        return NULL;
    }

    return chunk->vtbl->allocator(chunk);
}

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_CHUNK_VIEW_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
