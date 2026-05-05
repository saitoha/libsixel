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
#include "encoder-core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal encoder-core construction goes through the class factory so src/
 * callers depend on the codec/encoder-core contract rather than the public
 * constructor. Public boundary code should keep using sixel_output_new().
 */
static inline SIXELSTATUS
sixel_encoder_core_create_interface_from_factory(
    sixel_allocator_t *allocator,
    sixel_encoder_core_t **core_out)
{
    SIXELSTATUS status;
    sixel_factory_t *factory;
    void *object;
    void *service;

    status = SIXEL_FALSE;
    factory = NULL;
    object = NULL;
    service = NULL;

    if (core_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *core_out = NULL;
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
                                   "codec/encoder-core",
                                   allocator,
                                   &object);
    factory->vtbl->unref(factory);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (object == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    *core_out = (sixel_encoder_core_t *)object;
    return SIXEL_OK;
}

static inline SIXELSTATUS
sixel_encoder_core_create_output_from_factory(sixel_output_t **output_out,
                                              sixel_write_function fn_write,
                                              void *priv,
                                              sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_encoder_core_t *core;

    status = SIXEL_FALSE;
    core = NULL;

    if (output_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *output_out = NULL;

    status = sixel_encoder_core_create_interface_from_factory(allocator,
                                                              &core);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    if (core == NULL || core->vtbl == NULL ||
        core->vtbl->unref == NULL) {
        if (core != NULL && core->vtbl != NULL &&
            core->vtbl->unref != NULL) {
            core->vtbl->unref(core);
        }
        return SIXEL_LOGIC_ERROR;
    }

    status = sixel_output_init_writer((sixel_output_t *)core,
                                      fn_write,
                                      priv);
    if (SIXEL_FAILED(status)) {
        core->vtbl->unref(core);
        return status;
    }

    *output_out = (sixel_output_t *)core;
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
