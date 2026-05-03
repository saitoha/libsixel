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

#ifndef LIBSIXEL_OUTPUT_FACTORY_H
#define LIBSIXEL_OUTPUT_FACTORY_H

#include <6cells.h>

#include "factory.h"
#include "output.h"
#include "sixel-emitter.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal emitter construction goes through the class factory so src/ callers
 * depend on the terminal/sixel-emitter contract rather than the public
 * constructor. Public boundary code should keep using sixel_output_new().
 */
static inline SIXELSTATUS
sixel_emitter_create_interface_from_factory(
    sixel_allocator_t *allocator,
    sixel_emitter_t **emitter_out)
{
    SIXELSTATUS status;
    sixel_factory_t *factory;
    void *object;
    void *service;

    status = SIXEL_FALSE;
    factory = NULL;
    object = NULL;
    service = NULL;

    if (emitter_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *emitter_out = NULL;
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
                                   "terminal/sixel-emitter",
                                   allocator,
                                   &object);
    factory->vtbl->unref(factory);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    *emitter_out = (sixel_emitter_t *)object;
    return SIXEL_OK;
}

static inline SIXELSTATUS
sixel_emitter_create_output_from_factory(sixel_output_t **output_out,
                                         sixel_write_function fn_write,
                                         void *priv,
                                         sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_emitter_t *emitter;
    sixel_emitter_writer_request_t request;

    status = SIXEL_FALSE;
    emitter = NULL;

    if (output_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *output_out = NULL;

    status = sixel_emitter_create_interface_from_factory(allocator,
                                                         &emitter);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (emitter == NULL || emitter->vtbl == NULL ||
        emitter->vtbl->init_writer == NULL ||
        emitter->vtbl->unref == NULL) {
        if (emitter != NULL && emitter->vtbl != NULL &&
            emitter->vtbl->unref != NULL) {
            emitter->vtbl->unref(emitter);
        }
        return SIXEL_LOGIC_ERROR;
    }

    request.fn_write = fn_write;
    request.priv = priv;
    status = emitter->vtbl->init_writer(emitter, &request);
    if (SIXEL_FAILED(status)) {
        emitter->vtbl->unref(emitter);
        return status;
    }

    *output_out = (sixel_output_t *)emitter;
    return SIXEL_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_OUTPUT_FACTORY_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
