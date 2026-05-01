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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_CHUNK_FACTORY_H
#define LIBSIXEL_CHUNK_FACTORY_H

#include "chunk-view.h"
#include "factory.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal chunk construction goes through the class factory so callers depend
 * on the image/chunk contract rather than the concrete storage constructor.
 */
static inline SIXELSTATUS
sixel_chunk_create_from_factory(sixel_chunk_t **chunk_out,
                                sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_factory_t *factory;
    void *object;
    void *service;

    status = SIXEL_FALSE;
    factory = NULL;
    object = NULL;
    service = NULL;

    if (chunk_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *chunk_out = NULL;

    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    factory = (sixel_factory_t *)service;
    if (factory == NULL || factory->vtbl == NULL ||
        factory->vtbl->create == NULL || factory->vtbl->unref == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = factory->vtbl->create(factory,
                                   "image/chunk",
                                   allocator,
                                   &object);
    factory->vtbl->unref(factory);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    *chunk_out = (sixel_chunk_t *)object;
    return SIXEL_OK;
}

static inline SIXELSTATUS
sixel_chunk_create_from_source(sixel_chunk_t **chunk_out,
                               char const *filename,
                               int finsecure,
                               int const *cancel_flag,
                               sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_chunk_t *chunk;
    sixel_chunk_source_request_t request;

    status = SIXEL_FALSE;
    chunk = NULL;

    if (chunk_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *chunk_out = NULL;

    status = sixel_chunk_create_from_factory(&chunk, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    request.filename = filename;
    request.finsecure = finsecure;
    request.cancel_flag = cancel_flag;
    status = chunk->vtbl->init_source(chunk, &request);
    if (SIXEL_FAILED(status)) {
        chunk->vtbl->unref(chunk);
        return status;
    }

    *chunk_out = chunk;
    return SIXEL_OK;
}

static inline SIXELSTATUS
sixel_chunk_create_from_memory(sixel_chunk_t **chunk_out,
                               unsigned char const *bytes,
                               size_t size,
                               char const *source_path,
                               sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_chunk_t *chunk;
    sixel_chunk_memory_request_t request;

    status = SIXEL_FALSE;
    chunk = NULL;

    if (chunk_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *chunk_out = NULL;

    status = sixel_chunk_create_from_factory(&chunk, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    request.bytes = bytes;
    request.size = size;
    request.source_path = source_path;
    status = chunk->vtbl->init_memory(chunk, &request);
    if (SIXEL_FAILED(status)) {
        chunk->vtbl->unref(chunk);
        return status;
    }

    *chunk_out = chunk;
    return SIXEL_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_CHUNK_FACTORY_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
